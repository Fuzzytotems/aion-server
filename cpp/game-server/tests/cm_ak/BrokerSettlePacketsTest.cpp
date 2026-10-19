// The broker's settlement packets (P5-15 with P5-09b's BrokerService, the broker lane of 2026-10-18): CM_BROKER_SETTLE_LIST and
// CM_BROKER_SETTLE_ACCOUNT, read and run against the settled rows of the test database (BrokerPacketTestSupport.h).
//
// Java: CM_BROKER_SETTLE_LIST.java, CM_BROKER_SETTLE_ACCOUNT.java and BrokerService.java:487-582. Expectations are derived by hand from those
// bodies; the page split is SM_BROKER_SERVICE.java's SETTLED_ITEMS_STATIC_BODY_SIZE and its per-item calculator.

#include "BrokerPacketTestSupport.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/network/aion/clientpackets/CM_BROKER_SETTLE_ACCOUNT.h"
#include "aion/gameserver/network/aion/clientpackets/CM_BROKER_SETTLE_LIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_BROKER_SERVICE.h"
#include "aion/gameserver/utils/collections/DynamicServerPacketBodySplitList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

/** The friends the packet headers declare: the fields readImpl decoded */
struct CM_BROKER_SETTLE_LISTTestAccess {
	static int32_t brokerObjId(const CM_BROKER_SETTLE_LIST& p) { return p.brokerObjId; }
	static int32_t startPageIndex(const CM_BROKER_SETTLE_LIST& p) { return p.startPageIndex; }
};

struct CM_BROKER_SETTLE_ACCOUNTTestAccess {
	static int32_t brokerObjId(const CM_BROKER_SETTLE_ACCOUNT& p) { return p.brokerObjId; }
};

namespace testing::items {
namespace {

using serverpackets::SM_BROKER_SERVICE;

/** SM_BROKER_SERVICE's SETTLED_ITEMS (writeShowSettledItems): C(5), Q(kinah), D(total), H(page), C(0), H(n), n x (D(itemId), Q, Q, Q, D, blob, S) */
struct SettledItems {
	int64_t kinah = 0;
	int32_t total = 0;
	int32_t page = 0;
	std::vector<int32_t> itemIds;
	std::vector<int64_t> earned; // the second field: price * count of a sold offer, 0 otherwise
};

SettledItems decodeSettledItems(const std::vector<uint8_t>& packet) {
	PacketReader reader(bodyOf(packet));
	SettledItems result;
	EXPECT_EQ(reader.C(), 5) << "SETTLED_ITEMS";
	result.kinah = reader.Q();
	result.total = reader.D();
	result.page = static_cast<uint16_t>(reader.H());
	EXPECT_EQ(reader.C(), 0);
	const int32_t n = static_cast<uint16_t>(reader.H());
	for (int32_t i = 0; i < n; i++) {
		result.itemIds.push_back(reader.D());
		result.earned.push_back(reader.Q());
		reader.Q();
		reader.Q();
		reader.D(); // settle time in minutes
		reader.B(static_cast<size_t>(iteminfo::EnchantInfoBlobEntry::SIZE));
		reader.S(); // creator
	}
	EXPECT_EQ(reader.remaining(), 0u);
	return result;
}

class BrokerSettleTest : public BrokerPacketTest {
protected:
	/** a settled row of the holder: sold (no item: it went to the buyer) or expired (the item still at the broker, if `withItem`) */
	static void settled(int32_t objectId, int32_t itemId, int64_t count, int64_t price, bool sold, bool withItem = true, int32_t seller = HOLDER_ID) {
		if (!sold && withItem)
			maildb::execute(std::format("INSERT INTO inventory (item_unique_id, item_id, item_count, item_owner, item_location) VALUES ({}, {}, {}, {}, 126)",
				objectId, itemId, count, seller));
		maildb::execute(std::format("INSERT INTO broker (item_pointer, item_id, item_count, item_creator, price, broker_race, expire_time, settle_time, "
									"seller_id, is_sold, is_settled, splitting_available) VALUES ({}, {}, {}, '', {}, 'ELYOS', '2020-01-01 00:00:00', "
									"'2020-01-02 00:00:00', {}, {}, 1, 0)",
			objectId, itemId, count, price, seller, sold ? 1 : 0));
	}

	SettledItems settledList(int32_t page) {
		clearSent();
		readAndRun<CM_BROKER_SETTLE_LIST>(CM_BROKER_SETTLE_LIST_OPCODE, PacketWriter().D(broker->getObjectId()).H(page).data);
		std::vector<std::vector<uint8_t>> packets = brokerPackets();
		EXPECT_EQ(packets.size(), 1u);
		return packets.empty() ? SettledItems{} : decodeSettledItems(packets.front());
	}

	void settleAccount(int32_t brokerId = 0) {
		readAndRun<CM_BROKER_SETTLE_ACCOUNT>(CM_BROKER_SETTLE_ACCOUNT_OPCODE, PacketWriter().D(brokerId != 0 ? brokerId : broker->getObjectId()).data);
	}
};

TEST(BrokerSettleReadTest, TheBodiesAreReadAsJavaReadsThem) {
	EconomyDriver<CM_BROKER_SETTLE_LIST> list(CM_BROKER_SETTLE_LIST_OPCODE);
	ASSERT_TRUE(list.readOn(PacketWriter().D(0x01020304).H(0xFFFE).data, nullptr));
	EXPECT_EQ(CM_BROKER_SETTLE_LISTTestAccess::brokerObjId(list), 0x01020304);
	EXPECT_EQ(CM_BROKER_SETTLE_LISTTestAccess::startPageIndex(list), 0xFFFE) << "readUH";
	EconomyDriver<CM_BROKER_SETTLE_ACCOUNT> account(CM_BROKER_SETTLE_ACCOUNT_OPCODE);
	ASSERT_TRUE(account.readOn(PacketWriter().D(-3).data, nullptr));
	EXPECT_EQ(CM_BROKER_SETTLE_ACCOUNTTestAccess::brokerObjId(account), -3);
}

TEST_F(BrokerSettleTest, TheListShowsTheSellersSoldAndExpiredOffersWithTheEarnings) {
	BROKER_PACKET_REQUIRE_DATABASE();
	settled(9701, MINOR_LIFE_POTION, 3, 10, true);         // sold: 30
	settled(9702, MERCENARYS_FRUIT_JUICE, 2, 5, true);     // sold: 10
	settled(9703, TRAINING_SWORD, 1, 900, false);          // expired
	settled(9704, MINOR_LIFE_POTION, 1, 50, true, true, SELLER_ID); // another seller's
	targetBroker();

	SettledItems list = settledList(0);
	EXPECT_EQ(list.kinah, 40) << "extractEarnedKinahForSoldItems: the sold ones (BrokerService.java:506-510)";
	EXPECT_EQ(list.total, 3);
	EXPECT_EQ(list.page, 0);
	std::vector<int32_t> settledIds = list.itemIds;
	std::sort(settledIds.begin(), settledIds.end());
	EXPECT_EQ(settledIds, (std::vector<int32_t>{TRAINING_SWORD, MERCENARYS_FRUIT_JUICE, MINOR_LIFE_POTION}));
	std::vector<int64_t> earned = list.earned;
	std::sort(earned.begin(), earned.end());
	EXPECT_EQ(earned, (std::vector<int64_t>{0, 10, 30}));
}

TEST_F(BrokerSettleTest, APageStartsNineOffersLaterAndAPageBeyondTheEndThrows) {
	BROKER_PACKET_REQUIRE_DATABASE();
	for (int32_t i = 0; i < 12; i++)
		settled(9710 + i, MINOR_LIFE_POTION, 1, 10, true);
	targetBroker();

	SettledItems second = settledList(1);
	EXPECT_EQ(second.total, 12);
	EXPECT_EQ(second.page, 1);
	EXPECT_EQ(second.itemIds.size(), 3u) << "subList(9, 12), the last part: all of it";
	EXPECT_EQ(second.kinah, 120) << "the earnings of all twelve";

	EXPECT_THROW(settledList(2), runtime::IllegalArgumentException) << "subList(18, 12): fromIndex > toIndex";
}

TEST_F(BrokerSettleTest, MoreOffersThanOnePacketHoldsAreCutToFullPages) {
	BROKER_PACKET_REQUIRE_DATABASE();
	// each offer takes 32 + 138 + 2 bytes (an empty creator); one packet holds what fits after the 18 static bytes
	const int32_t perItem = 32 + iteminfo::EnchantInfoBlobEntry::SIZE + 2;
	const int32_t fit = (utils::collections::detail::maxUsablePacketBodySize() - SM_BROKER_SERVICE::SETTLED_ITEMS_STATIC_BODY_SIZE) / perItem;
	ASSERT_GT(fit % 9, 0) << "the case needs a partial page to cut";
	for (int32_t i = 0; i < fit + 5; i++)
		settled(9800 + i, MINOR_LIFE_POTION, 1, 10, true);
	targetBroker();

	SettledItems first = settledList(0);
	EXPECT_EQ(first.total, fit + 5);
	EXPECT_EQ(static_cast<int32_t>(first.itemIds.size()), fit - fit % 9) << "the first packet's offers, trimmed to full pages (:494-495)";
}

TEST_F(BrokerSettleTest, SettlingCollectsTheKinahAndTheExpiredItemsAndClearsTheIcon) {
	BROKER_PACKET_REQUIRE_DATABASE();
	settled(9701, MINOR_LIFE_POTION, 3, 10, true);
	settled(9702, MERCENARYS_FRUIT_JUICE, 2, 5, true);
	settled(9703, TRAINING_SWORD, 1, 900, false);
	settled(9704, MINOR_LIFE_POTION, 1, 50, true, true, SELLER_ID);
	owned(9799, KINAH, 100);
	targetBroker();

	settleAccount();

	// BrokerService.java:528-581: 30 + 10 kinah, the sword back in the cube, then the (now empty) list and the icon removed
	EXPECT_EQ(kinahOf(player()), 140);
	EXPECT_TRUE(storage(StorageType::CUBE).getItemByObjId(9703));
	std::vector<std::vector<uint8_t>> packets = brokerPackets();
	ASSERT_EQ(packets.size(), 2u);
	SettledItems list = decodeSettledItems(packets[0]);
	EXPECT_EQ(list.total, 0);
	EXPECT_EQ(list.kinah, 0);
	EXPECT_EQ(packets[1], serializedFor(SM_BROKER_SERVICE(false, int64_t{0})));

	saved();
	EXPECT_EQ(maildb::queryLong("SELECT COUNT(*) FROM broker WHERE seller_id = " + std::to_string(HOLDER_ID)), 0) << "DELETED";
	EXPECT_EQ(maildb::queryLong("SELECT COUNT(*) FROM broker WHERE seller_id = " + std::to_string(SELLER_ID)), 1);
	EXPECT_EQ(maildb::queryLong("SELECT item_count FROM inventory WHERE item_unique_id = 9799"), 100)
		<< "settleAccount stores no kinah item (Java has no save task for it here)";
}

TEST_F(BrokerSettleTest, AnItemTheCubeRefusesStaysSettledAndKeepsTheIcon) {
	BROKER_PACKET_REQUIRE_DATABASE();
	settled(9705, TRAINING_SWORD, 1, 900, false);
	settled(9706, MINOR_LIFE_POTION, 2, 10, true);
	owned(9799, KINAH, 100);
	stored(9705, TRAINING_SWORD, 1); // an item of the same object id in the cube: ItemStorage.putItem's putIfAbsent refuses the settled one
	targetBroker();

	settleAccount();

	// Storage.add returned null (ItemStorage.java:101-103), so itemsLeft (BrokerService.java:567-568): no SM_BROKER_SERVICE(false, 0) (:579-580)
	EXPECT_EQ(kinahOf(player()), 120) << "the sold offer is collected all the same";
	std::vector<std::vector<uint8_t>> packets = brokerPackets();
	ASSERT_EQ(packets.size(), 1u);
	SettledItems list = decodeSettledItems(packets[0]);
	EXPECT_EQ(list.itemIds, (std::vector<int32_t>{TRAINING_SWORD})) << "the sword is still settled";
}

TEST_F(BrokerSettleTest, AnExpiredOfferWithoutItsItemIsLoggedAndKept) {
	BROKER_PACKET_REQUIRE_DATABASE();
	settled(9707, TRAINING_SWORD, 1, 900, false, false); // no inventory row: BrokerDAO loads it without item
	targetBroker();
	network::test::LogCapture capture({"EXCHANGE_LOG"});

	settleAccount();

	EXPECT_TRUE(capture.contains("Broker settled item missed. ObjID: 9707")) << capture.dump();
	std::vector<std::vector<uint8_t>> packets = brokerPackets();
	ASSERT_EQ(packets.size(), 2u) << "itemsLeft stays false: the icon is removed (:570-571)";
	EXPECT_EQ(decodeSettledItems(packets[0]).total, 1);
}

TEST_F(BrokerSettleTest, WithoutTheBrokerNothingIsListedOrSettled) {
	BROKER_PACKET_REQUIRE_DATABASE();
	settled(9701, MINOR_LIFE_POTION, 3, 10, true);
	owned(9799, KINAH, 100);
	targetBroker();
	network::test::LogCapture capture({"AUDIT_LOG"});
	readAndRun<CM_BROKER_SETTLE_LIST>(CM_BROKER_SETTLE_LIST_OPCODE, PacketWriter().D(broker->getObjectId() + 1).H(0).data);
	settleAccount(broker->getObjectId() + 1);
	EXPECT_TRUE(brokerPackets().empty());
	EXPECT_EQ(kinahOf(player()), 100);
	EXPECT_TRUE(capture.contains("tried to open the broker sold item list without targeting a broker")) << capture.dump();
	EXPECT_TRUE(capture.contains("tried to get Kinah and unsold items from the broker without targeting a broker")) << capture.dump();
}

} // namespace
} // namespace testing::items
} // namespace aion::gameserver::network::aion::clientpackets
