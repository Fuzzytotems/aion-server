#pragma once

// Shared fixture of the broker client packets' run cases (P5-15's tests/cm_ak/BrokerPacketsTest.cpp and P5-16's tests/cm_lz, which includes this
// header by relative path): each runImpl is one call into BrokerService (P5-09b), and the cases drive the service through the packets.
//
// - BrokerPacketTest is MailPacketTest's economy fixture (EconomyPacketTest: "Holder", 710101, a level-1 ELYOS warrior in Poeta on a real
//   AionConnection) with a trade broker spawned beside him and targeted: npc_templates.xml:547154-547160 (831377, func_dialogs 33, OPEN_VENDOR).
// - The broker's offers live in the test database (tests/economy/P5-09a/EconomyTestSupport.h, aion_gs_test_economy). BrokerService is a process
//   singleton that loads the broker table once, when it is first used (BrokerService.java:60-95). So every case calls
//   BROKER_PACKET_REQUIRE_DATABASE(), writes its rows with offer() and only then touches the service: one process per case, as ctest runs them.
//   The cases carry the realdata label (game-server/CMakeLists.txt).
// - An offer is two rows: the item in the inventory table at item_location 126 (StorageType.BROKER), and the broker row naming it as
//   item_pointer (BrokerDAO.loadBroker, InventoryDAO.loadBrokerItems).
// - SM_BROKER_SERVICE is decoded field by field (SM_BROKER_SERVICE.java writeSearchedItems, writeRegisteredItems): the cases compare the
//   offers' ids, prices and counts in the order sent.

#include "MailPacketTestSupport.h"

#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/network/aion/iteminfo/EnchantInfoBlobEntry.h"
#include "aion/gameserver/services/BrokerService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::items {

// ServerPacketsOpcodes.gen.h: SM_BROKER_SERVICE (S_VENDOR)
inline constexpr int32_t SM_BROKER_SERVICE_OPCODE = 146;
// ClientPacketInfo.gen.inc:108-120 (all IN_GAME)
inline constexpr int32_t CM_BROKER_SELL_WINDOW_OPCODE = 117;
inline constexpr int32_t CM_BROKER_LIST_OPCODE = 123;
inline constexpr int32_t CM_BROKER_SEARCH_OPCODE = 124;
inline constexpr int32_t CM_BROKER_REGISTERED_OPCODE = 125;
inline constexpr int32_t CM_BUY_BROKER_ITEM_OPCODE = 126;
inline constexpr int32_t CM_REGISTER_BROKER_ITEM_OPCODE = 127;
inline constexpr int32_t CM_BROKER_CANCEL_REGISTERED_OPCODE = 128;
inline constexpr int32_t CM_BROKER_SETTLE_LIST_OPCODE = 129;
inline constexpr int32_t CM_BROKER_SETTLE_ACCOUNT_OPCODE = 130;

inline constexpr int32_t TRADE_BROKER = 831377;
inline constexpr int32_t SELLER_ID = 710301;  // an ELYOS seller with a players row
inline constexpr int32_t ASMODIAN_SELLER_ID = 710302;

// BrokerItemMask.java: the masks the cases list (a template's mask is its id / 100000)
inline constexpr int32_t MASK_CONSUMABLES = 9060;   // 1410, 1600, 1620, ...
inline constexpr int32_t MASK_FOOD = 1600;          // child of CONSUMABLES
inline constexpr int32_t MASK_POTION = 1620;        // child of CONSUMABLES
inline constexpr int32_t MASK_WEAPON = 9010;        // 1000..1021

// BrokerItem.getComparatoryByType
inline constexpr int32_t SORT_PRICE_ASC = 4;
inline constexpr int32_t SORT_PRICE_DESC = 5;

/** npc_templates.xml:547154-547160, verbatim */
inline constexpr std::string_view TRADE_BROKER_XML = R"(<npc_templates>
	<npc_template npc_id="831377" level="1" name="trade broker" name_id="463114" height="1.375" title_id="370503" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" attack_speed="2000" hpgauge="3">
		<stats maxHp="26116">
			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />
		</stats>
		<bound_radius front="0.7" side="0.444" upper="1.375" />
		<talk_info distance="5" is_dialog="true" func_dialogs="33" can_talk_invisible="false" />
	</npc_template>
</npc_templates>)";

inline const dataholders::NpcData& brokerNpcTemplates() {
	static const dataholders::NpcData* holder = [] {
		static xml::LoadContext context;
		return xml::bindString<dataholders::NpcData>(context, TRADE_BROKER_XML).release();
	}();
	return *holder;
}

/** One offer of SM_BROKER_SERVICE's lists, the fields the cases compare */
struct SentOffer {
	int32_t objectId = 0;
	int32_t itemId = 0;
	int64_t totalPrice = 0; // price * count
	int64_t averagePrice = 0;
	int64_t count = 0;
	std::string seller;

	bool operator==(const SentOffer&) const = default;
};

/** SM_BROKER_SERVICE's SEARCHED_ITEMS (writeSearchedItems): C(0), D(total), C(0), H(startPage), H(n), n x writeItemInfo */
struct SearchedItems {
	int32_t total = 0;
	int32_t startPage = 0;
	std::vector<SentOffer> offers;

	std::vector<int32_t> ids() const {
		std::vector<int32_t> result;
		for (const SentOffer& offer : offers)
			result.push_back(offer.objectId);
		return result;
	}
};

/** The body of a captured packet (after the five header bytes javaPacket writes) */
inline std::vector<uint8_t> bodyOf(const std::vector<uint8_t>& packet) {
	return std::vector<uint8_t>(packet.begin() + 5, packet.end());
}

inline SearchedItems decodeSearchedItems(const std::vector<uint8_t>& packet) {
	PacketReader reader(bodyOf(packet));
	SearchedItems result;
	EXPECT_EQ(reader.C(), 0) << "SEARCHED_ITEMS";
	result.total = reader.D();
	EXPECT_EQ(reader.C(), 0);
	result.startPage = static_cast<uint16_t>(reader.H());
	const int32_t n = static_cast<uint16_t>(reader.H());
	for (int32_t i = 0; i < n; i++) {
		SentOffer offer;
		offer.objectId = reader.D();
		offer.itemId = reader.D();
		offer.totalPrice = reader.Q();
		offer.averagePrice = reader.Q();
		offer.count = reader.Q();
		reader.B(static_cast<size_t>(iteminfo::EnchantInfoBlobEntry::SIZE));
		offer.seller = reader.S();
		reader.S(); // creator
		reader.H();
		reader.C();
		reader.D(); // polish
		reader.C(); // wrap
		reader.C(); // splitting
		result.offers.push_back(offer);
	}
	EXPECT_EQ(reader.remaining(), 0u);
	return result;
}

/** SM_BROKER_SERVICE's REGISTERED_ITEMS (writeRegisteredItems): C(1), D(0), H(n), n x writeRegisteredItemInfo; the offers' ids in order */
inline std::vector<int32_t> decodeRegisteredItems(const std::vector<uint8_t>& packet) {
	PacketReader reader(bodyOf(packet));
	EXPECT_EQ(reader.C(), 1) << "REGISTERED_ITEMS";
	EXPECT_EQ(reader.D(), 0);
	const int32_t n = static_cast<uint16_t>(reader.H());
	std::vector<int32_t> ids;
	for (int32_t i = 0; i < n; i++) {
		ids.push_back(reader.D()); // item unique id
		reader.D();                // item id
		reader.Q();                // price * count
		reader.Q();
		reader.Q();
		reader.C(); // days left
		reader.B(static_cast<size_t>(iteminfo::EnchantInfoBlobEntry::SIZE));
		reader.S(); // creator
		reader.H();
		reader.C();
		reader.D(); // polish
		reader.C(); // wrap
		reader.C(); // splitting
	}
	EXPECT_EQ(reader.remaining(), 0u);
	return ids;
}

class BrokerPacketTest : public MailPacketTest {
protected:
	void TearDown() override {
		if (f.player)
			services::BrokerService::getInstance().removePlayerCache(*f.player);
		MailPacketTest::TearDown();
	}

	/** The database (see the header comment), emptied of broker rows, with the sellers' players rows */
	bool requireBrokerDatabase() {
		if (!requireDatabase())
			return false;
		maildb::execute("DELETE FROM broker");
		maildb::execute(std::format("DELETE FROM players WHERE id IN ({}, {})", SELLER_ID, ASMODIAN_SELLER_ID));
		maildb::insertPlayer(SELLER_ID, "Seller", SELLER_ID + 1000);
		maildb::insertPlayer(ASMODIAN_SELLER_ID, "Darkseller", ASMODIAN_SELLER_ID + 1000, "ASMODIANS");
		return true;
	}

	/** An offer as BrokerDAO.loadBroker reads it: the item at item_location 126 and its broker row (not sold, not settled) */
	static void offer(int32_t objectId, int32_t itemId, int64_t count, int64_t price, int32_t sellerId, std::string_view race = "ELYOS",
		bool splitting = false, std::string_view expireTime = "2099-01-01 00:00:00") {
		maildb::execute(std::format("INSERT INTO inventory (item_unique_id, item_id, item_count, item_owner, item_location) VALUES ({}, {}, {}, {}, 126)",
			objectId, itemId, count, sellerId));
		maildb::execute(std::format("INSERT INTO broker (item_pointer, item_id, item_count, item_creator, price, broker_race, expire_time, seller_id, "
									"is_sold, is_settled, splitting_available) VALUES ({}, {}, {}, '', {}, '{}', '{}', {}, 0, 0, {})",
			objectId, itemId, count, price, race, expireTime, sellerId, splitting ? 1 : 0));
	}

	/** The trade broker spawned beside the holder and his target (the selection's own packets are cleared) */
	model::gameobjects::Npc& targetBroker(float x = 102.0f) {
		runtime::Ref<model::templates::spawns::SpawnGroup> group = model::templates::spawns::SpawnGroup::create(210010000, TRADE_BROKER, 0, nullptr);
		model::templates::spawns::SpawnTemplate& spawn = group->addSpawnTemplate(std::make_unique<EconomySpawnTemplate>(*group, x, 100.0f, 50.0f));
		runtime::Ref<model::gameobjects::Npc> npc = model::gameobjects::VisibleObject::create<model::gameobjects::Npc>(
			std::make_unique<controllers::NpcController>(), spawn, brokerNpcTemplates().getNpcTemplate(TRADE_BROKER));
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
		npc->setPosition(world::WorldPosition::create(210010000, x, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(x, 100.0f, 50.0f)));
		npc->getPosition()->setIsSpawned(true);
		f.knownList().addForTest(*npc);
		spawnGroups.push_back(group);
		npcs.push_back(npc);
		player().setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(*npc));
		clearSent();
		return *npc;
	}

	/** The SM_BROKER_SERVICE packets sent since the last clearSent */
	std::vector<std::vector<uint8_t>> brokerPackets() { return packetsOf(sent(), SM_BROKER_SERVICE_OPCODE); }

	/** The one SEARCHED_ITEMS sent since the last clearSent */
	SearchedItems searched() {
		std::vector<std::vector<uint8_t>> packets = brokerPackets();
		EXPECT_EQ(packets.size(), 1u);
		if (packets.size() != 1)
			return {};
		return decodeSearchedItems(packets.front());
	}

	/** CM_BROKER_LIST.java readImpl: D(broker), C(sortType), H(page), H(listMask) */
	static std::vector<uint8_t> listBody(int32_t broker, int32_t sortType, int32_t page, int32_t mask) {
		return PacketWriter().D(broker).C(sortType).H(page).H(mask).data;
	}

	/** CM_BROKER_SEARCH.java readImpl: D(broker), C(sortType), H(page), H(mask), H(n), n x D(itemId) */
	static std::vector<uint8_t> searchBody(int32_t broker, int32_t sortType, int32_t page, int32_t mask, const std::vector<int32_t>& itemIds) {
		PacketWriter body;
		body.D(broker).C(sortType).H(page).H(mask).H(static_cast<int32_t>(itemIds.size()));
		for (int32_t itemId : itemIds)
			body.D(itemId);
		return body.data;
	}
};

/** Skips the case without the test database (use at the start of a TEST_F body of BrokerPacketTest) */
#define BROKER_PACKET_REQUIRE_DATABASE()                                                                                                             \
	if (!requireBrokerDatabase())                                                                                                                      \
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL to run the database tests";

} // namespace aion::gameserver::network::aion::clientpackets::testing::items
