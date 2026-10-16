#pragma once

// Shared fixture of the M5c stage-1 trade tests (m5c-plan.md T-04, tests/economy/P5-09b): TradeService (T-01), ExchangeService (T-02) and
// PrivateStoreService (T-03) driven against two real Players.
//
// - The players are tests/cm_ak's (ItemPacketTestSupport.h, included by relative path as tests/economy/P5-09a/DropTestSupport.h includes
//   InWorldPacketRunSupport.h): A is ItemPacketTest's spawned level-1 warrior "Holder" in a Poeta map instance, B ("Partner") stands beside
//   him in the same instance; each has a real AionConnection whose send queue the cases read. A knows B, so a broadcast of A reaches B.
// - ITEM_DATA holds ItemPacketTestSupport.h's verbatim rows plus the vendor goods below (item_templates.xml, verbatim, line beside each);
//   the merchant is minalinerk 798007 with its shipped trade list and goods lists (the rows tests/cm_ak/DialogSelectPacketsTest.cpp copies),
//   beside the ABYSS vendor adetes 203386 and the REWARD vendor gintarunerk 801517 with the rows of theirs the cases buy. Prices come from
//   PricesConfig's shipped defaults (prices.properties: 100/100/100, vendor buy 100, sell 20) with no siege location, so Influence is 0 for
//   both races and the global prices are 125 % and the taxes 113 % (m5c-plan.md §2.10; `oracle.py m5c-trade --npc 798007 --set
//   gameserver.siege.enable=false` gives the same numbers).
// - A trade that completes writes both inventories with InventoryDAO.store (ExchangeService.java:259-260): those cases need the economy test
//   database of EconomyTestSupport.h (DatabaseFactory on `aion_gs_test_economy`) and skip without AION_TEST_GS_DATABASE_URL.
// - Do not mix this fixture with tests/world/WorldTestSupport.h in one executable run: ItemPacketTestSupport.h's publishPoetaWorldDataOnce
//   and WorldTestSupport's refusal of a second publisher conflict (m5c-plan.md §18.7). ctest runs every case in its own process.

#include "../../cm_ak/ItemPacketTestSupport.h"
#include "EconomyTestSupport.h"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <spdlog/details/null_mutex.h>
#include <spdlog/sinks/base_sink.h>

#include "aion/commons/configuration/ConfigValue.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/configs/main/PricesConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/GoodsListData.bind.h"
#include "aion/gameserver/dataholders/GoodsListData.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/TradeListData.bind.h"
#include "aion/gameserver/dataholders/TradeListData.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PrivateStore.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/trade/Exchange.h"
#include "aion/gameserver/model/trade/ExchangeItem.h"
#include "aion/gameserver/services/ExchangeService.h"
#include "aion/gameserver/services/PrivateStoreService.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::economy::test::trade {

namespace cp = network::aion::clientpackets::testing;
namespace itemtest = cp::items;

using model::gameobjects::Item;
using model::gameobjects::Npc;
using model::gameobjects::player::Player;
using model::items::storage::StorageType;
using network::test::PacketReader;
using network::test::PacketWriter;

// the item ids of the rows below
inline constexpr int32_t KINAH = itemtest::KINAH;
inline constexpr int32_t TRAINING_SWORD = itemtest::TRAINING_SWORD;          // mask 138366: tradeable and sellable, not stackable, price 5
inline constexpr int32_t SLOT_TEST_SWORD = itemtest::FABLED_TEST_SWORD;      // 100000379, mask 138366, m_slots 4: takes a manastone
inline constexpr int32_t FRUIT_JUICE = itemtest::MERCENARYS_FRUIT_JUICE;     // mask 12360: neither tradeable nor sellable
inline constexpr int32_t MINOR_LIFE_POTION = itemtest::MINOR_LIFE_POTION;    // mask 12414, price 250, stacks to 1000
inline constexpr int32_t MANASTONE_HP_20 = itemtest::MANASTONE_HP_20;
inline constexpr int32_t MINOR_LIFE_ELIXIR = 162000052; // price 250, minalinerk's goods list 720
inline constexpr int32_t EXTRACTION_TOOLS = 165000001;  // price 1000, minalinerk's goods list 132

inline constexpr int32_t MINALINERK = 798007;

// ServerPacketsOpcodes.java:42-163
inline constexpr int32_t SM_EXCHANGE_REQUEST_OPCODE = 74;
inline constexpr int32_t SM_EXCHANGE_ADD_ITEM_OPCODE = 75;
inline constexpr int32_t SM_EXCHANGE_ADD_KINAH_OPCODE = 77;
inline constexpr int32_t SM_EXCHANGE_CONFIRMATION_OPCODE = 78;
inline constexpr int32_t SM_PRIVATE_STORE_NAME_OPCODE = 145;

// ItemPacketService.ItemDeleteType masks (ItemPacketService.java)
inline constexpr int32_t DELETE_DEFAULT = 0;
inline constexpr int32_t DELETE_PUT_TO_EXCHANGE = 0x26;

/** item_templates.xml, verbatim: the merchant goods that ItemPacketTestSupport.h's rows lack */
inline constexpr std::string_view TRADE_GOODS_ITEM_ROWS = R"xml(
	<!-- :831033 -->
	<item_template id="162000052" name="Minor Life Elixir" level="10" cName="shop_remedy_hp_10a" mask="12414" max_stack_count="1000" quality="COMMON" price="250" desc="741694" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="1" skillid="10202"/>
		</actions>
		<uselimits usedelay="60000" usedelayid="11"/>
	</item_template>
	<!-- :836407 -->
	<item_template id="165000001" name="Extraction Tools" level="1" cName="matter_extraction_01" mask="12414" max_stack_count="100" quality="COMMON" price="1000" desc="701680" activate_count="1">
		<actions>
			<extract/>
		</actions>
	</item_template>
	<!-- :612582, adetes' goods list 535 -->
	<item_template id="115000892" name="Elite Guardian Squad Leader's Shield" level="30" cName="shield_a_u2_30b" mask="69720" item_group="SHIELD" item_type="ABYSS" quality="UNIQUE" price="61450" restrict="30 30 30 30 30 30 30 30 30 30 30 30 30 30 30 30 30" desc="748166" exceed_enchant_skill="RANK1_SET3_PHYSICAL_WEAPON" can_exceed_enchant="true" max_enchant="15" m_slots="6">
		<modifiers>
			<add name="BLOCK" value="570"/>
			<rate name="DAMAGE_REDUCE" value="45"/>
			<add name="BLOCK" value="61" bonus="true"/>
			<add name="MAXHP" value="226" bonus="true"/>
			<add name="PVP_DEFEND_RATIO" value="40" bonus="true"/>
		</modifiers>
		<weapon_stats reduce_max="450"/>
		<acquisition type="ABYSS" item="186000031" count="6" ap="70350"/>
		<disposition id="188950008" count="4"/>
	</item_template>
	<!-- :896295 -->
	<item_template id="186000031" name="Silver Medal" level="40" cName="medal_02" mask="12414" max_stack_count="1000" item_group="MEDALS" quality="RARE" price="5000" desc="739362">
		<inventory id="1"/>
	</item_template>
	<!-- :833923, gintarunerk's goods list 1770 -->
	<item_template id="164000268" name="Pandarunerk's Delve Scroll" level="1" cName="scroll_return_ldf5b_sugo2" mask="12414" max_stack_count="1000" quality="COMMON" price="500000" race="ASMODIANS" restrict="60 60 60 60 60 60 60 60 60 60 60 60 60 60 60 60 60" desc="822620">
		<acquisition type="REWARD" item="186000237" count="2"/>
	</item_template>
	<!-- :896781 -->
	<item_template id="186000237" name="Ancient Coin" level="65" cName="coin_ancient_01" mask="12414" max_stack_count="10000" item_group="COINS" quality="RARE" price="100" desc="812755">
		<inventory id="1"/>
	</item_template>
)xml";

/** ItemPacketTestSupport.h's rows and the goods above in one <item_templates> document */
inline std::string tradeItemTemplatesXml() {
	std::string xml(itemtest::ITEM_TEMPLATES_XML);
	xml.erase(xml.rfind("</item_templates>"));
	xml += TRADE_GOODS_ITEM_ROWS;
	xml += "</item_templates>";
	return xml;
}

inline constexpr int32_t ADETES = 203386;             // an ABYSS vendor (Sanctum), m5c-plan.md §2.2 row 5
inline constexpr int32_t SQUAD_LEADERS_SHIELD = 115000892; // acquisition ABYSS: 6 x Silver Medal and 70,350 AP
inline constexpr int32_t SILVER_MEDAL = 186000031;

inline constexpr int32_t GINTARUNERK = 801517;  // a REWARD vendor: his goods cost tokens, no AP and no kinah
inline constexpr int32_t DELVE_SCROLL = 164000268; // acquisition REWARD: 2 x Ancient Coin, no AP
inline constexpr int32_t ANCIENT_COIN = 186000237;

/**
 * npc_templates.xml:461604-461610 (BUY and SELL), :6364-6377 (BUY, without <equipment>; the equipment is what the npc wears) and
 * :497718-497724 (BUY), verbatim
 */
inline constexpr std::string_view MERCHANT_NPC_TEMPLATES_XML = R"xml(<npc_templates>
	<npc_template npc_id="798007" level="9" name="minalinerk" name_id="351126" height="1.16875" title_id="350377" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="240" attack_speed="2100" hpgauge="3">
		<stats maxHp="2568">
			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />
		</stats>
		<bound_radius front="0.595" side="0.3774" upper="1.16875" />
		<talk_info distance="5" is_dialog="true" func_dialogs="2 3" can_talk_invisible="false" />
	</npc_template>
	<npc_template npc_id="203386" level="45" name="adetes" name_id="351432" height="2" title_id="370109" group_drop="LIGHT" rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="300" arange="2" attack_speed="2000" hpgauge="3">
		<stats maxHp="11878">
			<speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" />
		</stats>
		<bound_radius front="0.25" side="0.35" upper="2" />
		<talk_info distance="5" is_dialog="true" func_dialogs="2" can_talk_invisible="false" />
	</npc_template>
	<npc_template npc_id="801517" level="65" name="gintarunerk" name_id="373136" height="1.16875" title_id="463204" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="USEALL" type="GENERAL" ai="general" srange="20" sangle="240" attack_speed="2000" hpgauge="3">
		<stats maxHp="26116">
			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />
		</stats>
		<bound_radius front="0.595" side="0.3774" upper="1.16875" />
		<talk_info distance="5" is_dialog="true" func_dialogs="2" can_talk_invisible="false" />
	</npc_template>
</npc_templates>)xml";

/** npc_trade_list.xml:2186-2189, :338-341 and :5024-5026, verbatim */
inline constexpr std::string_view MERCHANT_TRADE_LIST_XML = R"xml(<npc_trade_list>
	<tradelist_template npc_id="798007" buy_price_rate="200">
		<tradelist id="132" />
		<tradelist id="720" />
	</tradelist_template>
	<tradelist_template npc_id="203386" npc_type="ABYSS">
		<tradelist id="534" />
		<tradelist id="535" />
	</tradelist_template>
	<tradelist_template npc_id="801517" npc_type="REWARD">
		<tradelist id="1770" />
	</tradelist_template>
</npc_trade_list>)xml";

/**
 * goodslists/goodslists.xml:13484-13488, :27984-27987, :22532-22536 and :51374-51377, verbatim (list 534 of adetes is not copied: a missing
 * list is skipped)
 */
inline constexpr std::string_view MERCHANT_GOODSLISTS_XML = R"xml(<goodslists>
    <list id="132">
        <item id="169000003"/>
        <item id="165000001"/>
        <item id="169300002"/>
    </list>
    <list id="720">
        <item id="162000052"/>
        <item id="162000057"/>
    </list>
    <list id="535">
        <item id="115000892"/>
        <item id="115000893"/>
        <item id="115000886"/>
    </list>
    <list id="1770">
        <item id="164000265"/>
        <item id="164000268"/>
    </list>
</goodslists>)xml";

/** The npc templates, kept for the process as DataManager keeps its own (an npc points into its template) */
inline const dataholders::NpcData& merchantTemplates() {
	static const dataholders::NpcData* holder = [] {
		static xml::LoadContext context;
		return xml::bindString<dataholders::NpcData>(context, MERCHANT_NPC_TEMPLATES_XML).release();
	}();
	return *holder;
}

class MerchantSpawnTemplate final : public model::templates::spawns::SpawnTemplate {
public:
	MerchantSpawnTemplate(model::templates::spawns::SpawnGroup& group, float x, float y, float z)
		: SpawnTemplate(group, x, y, z, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

/**
 * A sink on one logger that runs a callback once, on the logging thread, when a line contains a text. It is the interleaving point of the
 * double-confirm case (m5c-plan.md D7): the callback runs while the logging call waits, so the code logged from is suspended exactly there.
 * The logger's own distributing sink holds its lock while the callback runs, so the callback must not log to the same logger.
 */
class LogHook {
public:
	LogHook(std::string loggerName, std::string text, std::function<void()> hook) : name(std::move(loggerName)) {
		sink = std::make_shared<Sink>();
		sink->text = std::move(text);
		sink->hook = std::move(hook);
		commons::logging::LoggerFactory::configure(name, {.level = spdlog::level::debug, .sinks = {sink}, .additive = true});
	}
	~LogHook() { commons::logging::LoggerFactory::removeConfig(name); }
	LogHook(const LogHook&) = delete;
	LogHook& operator=(const LogHook&) = delete;

	bool fired() const { return sink->fired; }

private:
	struct Sink : spdlog::sinks::base_sink<spdlog::details::null_mutex> {
		std::string text;
		std::function<void()> hook;
		bool fired = false;

	protected:
		void sink_it_(const spdlog::details::log_msg& msg) override {
			if (fired)
				return;
			std::string_view payload(msg.payload.data(), msg.payload.size());
			if (payload.find(text) == std::string_view::npos)
				return;
			fired = true;
			hook();
		}
		void flush_() override {}
	};

	std::string name;
	std::shared_ptr<Sink> sink;
};

/** A saved atomic configuration value, restored by the fixture */
template <class T>
class ConfigScope {
public:
	ConfigScope(std::atomic<T>& configValue, T value) : config(configValue), previous(configValue.exchange(value)) {}
	~ConfigScope() { config.store(previous); }
	ConfigScope(const ConfigScope&) = delete;
	ConfigScope& operator=(const ConfigScope&) = delete;

private:
	std::atomic<T>& config;
	const T previous;
};

/** A saved reloadable configuration value (ConfigValue), restored by the scope */
template <class T>
class ConfigValueScope {
public:
	ConfigValueScope(commons::configuration::ConfigValue<T>& configValue, T value) : config(configValue), previous(configValue.get()) {
		config.set(std::move(value));
	}
	~ConfigValueScope() { config.set(previous ? *previous : T{}); }
	ConfigValueScope(const ConfigValueScope&) = delete;
	ConfigValueScope& operator=(const ConfigValueScope&) = delete;

private:
	commons::configuration::ConfigValue<T>& config;
	const std::shared_ptr<const T> previous;
};

/**
 * ItemPacketTest with a second player B beside A, the merchant data published and the price configuration of the shipped profile. Cases add
 * items to either player's storages the way the DAO loads them (no packet).
 */
class TradeTest : public itemtest::ItemPacketTest {
protected:
	void SetUp() override {
		ItemPacketTest::SetUp();
		// the merchant's row names ai="general", whose handler lives in the handler library this executable does not link: AIEngine then
		// substitutes a DummyNpcAI (AIEngine.cpp:158-168; the trade services never ask the AI)
		savedMissingAiHandlers = configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn");
		dataholders::DataManager::ITEM_DATA.resetForTests(); // the base rows, republished below with the merchant goods
		xml::LoadContext context;
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(context, tradeItemTemplatesXml()));
		dataholders::DataManager::TRADE_LIST_DATA.publish(xml::bindString<dataholders::TradeListData>(context, MERCHANT_TRADE_LIST_XML));
		dataholders::DataManager::GOODSLIST_DATA.publish(xml::bindString<dataholders::GoodsListData>(context, MERCHANT_GOODSLISTS_XML));

		b = cp::makePlayer(710202, 9902, "Partner");
		b.player->setPosition(world::WorldPosition::create(210010000, 102.0f, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(102.0f, 100.0f, 50.0f)));
		b.player->getPosition()->setIsSpawned(true);
		clientB = std::make_unique<cp::TestClient>();
		clientB->enterWorld(b);
		f.knownList().addForTest(*b.player); // A's broadcasts reach B (the see notification is counted by knownSeeNotifiesFailed)
		clearSent();
		clearSentB();
	}

	void TearDown() override {
		// no exchange or store outlives its players: the ExchangeService singleton holds Refs to both
		if (f.player && services::ExchangeService::getInstance().isPlayerInExchange(*f.player))
			services::ExchangeService::getInstance().cancelExchange(*f.player);
		if (b.player && services::ExchangeService::getInstance().isPlayerInExchange(*b.player))
			services::ExchangeService::getInstance().cancelExchange(*b.player);
		if (f.player)
			f.player->setStore(nullptr);
		if (b.player) {
			b.player->setStore(nullptr);
			b.player->setTarget(nullptr);
			b.player->setClientConnection(nullptr);
		}
		clientB.reset();
		given.clear();
		b = {};
		npcs.clear(); // before the map instance their positions name
		spawnGroups.clear();
		ItemPacketTest::TearDown();
		dataholders::DataManager::GOODSLIST_DATA.resetForTests();
		dataholders::DataManager::TRADE_LIST_DATA.resetForTests();
		if (savedMissingAiHandlers)
			configs::main::AIConfig::MISSING_AI_HANDLERS.set(*savedMissingAiHandlers);
	}

	Player& a() { return player(); }
	Player& partner() { return *b.player; }

	/** An item loaded into a player's storage the way the DAO does (onLoadHandler: no packet) */
	Item& give(Player& owner, int32_t objId, int32_t itemId, int64_t count, StorageType location = StorageType::CUBE) {
		runtime::Ref<Item> item = itemtest::loadedItem(objId, itemId, count, location);
		owner.getStorage(model::items::storage::getId(location))->onLoadHandler(*item);
		given.push_back(item);
		return *item;
	}

	/** The player's kinah item, loaded like give() */
	void setKinah(Player& owner, int32_t objId, int64_t count) { give(owner, objId, KINAH, count); }

	int64_t kinah(Player& owner) { return owner.getInventory().getKinah(); }

	/** The count of one item id over the player's cube (Storage.getItemCountByItemId) */
	int64_t countOf(Player& owner, int32_t itemId) { return owner.getInventory().getItemCountByItemId(itemId); }

	std::vector<std::vector<uint8_t>> sentB() { return (*clientB)->sentBytes(); }
	void clearSentB() { (*clientB)->clearSent(); }

	std::vector<uint8_t> serializedForB(network::aion::AionServerPacket&& packet) { return cp::serialized(std::move(packet), clientB->con()); }

	/** minalinerk spawned beside A (the trade services read only its template: no AI, no known list entry) */
	Npc& merchant() { return npcOf(MINALINERK); }

	/** The npc of a MERCHANT_NPC_TEMPLATES_XML row, spawned once 1 m beside A */
	Npc& npcOf(int32_t npcId) {
		for (const runtime::Ref<Npc>& npc : npcs)
			if (npc->getNpcId() == npcId)
				return *npc;
		runtime::Ref<model::templates::spawns::SpawnGroup> group = model::templates::spawns::SpawnGroup::create(210010000, npcId, 0, nullptr);
		model::templates::spawns::SpawnTemplate& spawn = group->addSpawnTemplate(std::make_unique<MerchantSpawnTemplate>(*group, 101.0f, 100.0f, 50.0f));
		runtime::Ref<Npc> npc = model::gameobjects::VisibleObject::create<Npc>(std::make_unique<controllers::NpcController>(), spawn,
			merchantTemplates().getNpcTemplate(npcId));
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
		npc->setPosition(world::WorldPosition::create(210010000, 101.0f, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(101.0f, 100.0f, 50.0f)));
		npc->getPosition()->setIsSpawned(true);
		spawnGroups.push_back(group);
		npcs.push_back(npc);
		return *npc;
	}

	std::shared_ptr<const std::string> savedMissingAiHandlers;
	cp::PlayerFixture b;
	std::unique_ptr<cp::TestClient> clientB;
	std::vector<runtime::Ref<Item>> given;
	std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>> spawnGroups;
	std::vector<runtime::Ref<Npc>> npcs;
	// the shipped prices.properties and custom.properties values the services read (PricesConfig.java, CustomConfig.java defaults), and the
	// gate profile's sell limits off (m5c.properties.example: gameserver.limits.enable = false)
	ConfigScope<int32_t> defaultPrices{configs::main::PricesConfig::DEFAULT_PRICES, 100};
	ConfigScope<int32_t> defaultModifier{configs::main::PricesConfig::DEFAULT_MODIFIER, 100};
	ConfigScope<int32_t> defaultTaxes{configs::main::PricesConfig::DEFAULT_TAXES, 100};
	ConfigScope<int32_t> vendorBuyModifier{configs::main::PricesConfig::VENDOR_BUY_MODIFIER, 100};
	ConfigScope<int32_t> vendorSellModifier{configs::main::PricesConfig::VENDOR_SELL_MODIFIER, 20};
	ConfigScope<bool> limits{configs::main::CustomConfig::LIMITS_ENABLED, false};
	ConfigScope<bool> logAudit{configs::main::LoggingConfig::LOG_AUDIT, true};
};

/** The captured packets of one opcode, in order */
inline std::vector<std::vector<uint8_t>> ofOpcode(const std::vector<std::vector<uint8_t>>& packets, int32_t opcode) {
	return itemtest::packetsOf(packets, opcode);
}

/** SM_EXCHANGE_CONFIRMATION (SM_EXCHANGE_CONFIRMATION.java writeImpl): C(action) */
inline std::vector<uint8_t> exchangeConfirmation(int32_t action) {
	return itemtest::javaPacket(SM_EXCHANGE_CONFIRMATION_OPCODE, PacketWriter().C(action));
}

/** SM_EXCHANGE_ADD_KINAH (SM_EXCHANGE_ADD_KINAH.java writeImpl): C(action: 0 self, 1 other), Q(kinahCount) */
inline std::vector<uint8_t> exchangeAddKinah(int64_t count, int32_t action) {
	return itemtest::javaPacket(SM_EXCHANGE_ADD_KINAH_OPCODE, PacketWriter().C(action).Q(count));
}

/** SM_EXCHANGE_REQUEST (SM_EXCHANGE_REQUEST.java writeImpl): S(receiver) */
inline std::vector<uint8_t> exchangeRequest(std::string_view name) {
	return itemtest::javaPacket(SM_EXCHANGE_REQUEST_OPCODE, PacketWriter().S(name));
}

/** SM_DELETE_ITEM (SM_DELETE_ITEM.java writeImpl): D(itemObjectId), C(deleteType mask) */
inline std::vector<uint8_t> deleteItem(int32_t objId, int32_t mask) {
	return itemtest::deleteItem(objId, mask);
}

} // namespace aion::gameserver::economy::test::trade
