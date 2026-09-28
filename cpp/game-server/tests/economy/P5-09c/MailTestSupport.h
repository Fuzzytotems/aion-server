#pragma once

// Shared fixture of the M5c stage-1 mail tests (m5c-plan.md M-03, tests/economy/P5-09c): MailService, SystemMailService and the optional R-02
// StarterKitService.onLevelUp, driven against two real Players.
//
// - The players are tests/cm_ak's (ItemPacketTestSupport.h, included by relative path as tests/economy/P5-09b/TradeTestSupport.h does): the sender
//   A is ItemPacketTest's level-1 warrior "Holder" (710101) with a real AionConnection; the recipient B ("Partner", 710202) has a connection of
//   his own. Both have a Mailbox (SM_MAIL_SERVICE's writeImpl reads the receiving connection's mailbox). B is offline until a case calls
//   goOnline(): then he is in the World (PlayerService.getOrLoadPlayerCommonData and validateRecipient find him there) and his common data is
//   online (PlayerCommonData.getPlayer answers him).
// - ITEM_DATA holds ItemPacketTestSupport.h's verbatim rows plus the rows below (item_templates.xml, verbatim, line beside each).
// - Prices come from the shipped prices.properties (100/100/100) with no siege location, so Influence is 0 for both races: the global prices are
//   125 % and the taxes 113 % (m5c-plan.md §2.10). Every expected commission and total below is `oracle.py m5c-economy --no-profile --set
//   gameserver.siege.enable=false --mail ITEM:COUNT:KINAH[:express]` (tools/oracle/m5c/economy.py mail_block: Java's float arithmetic).
// - A case that reaches a DAO write either uses the economy test database of EconomyTestSupport.h (MAIL_REQUIRE_DATABASE: DatabaseFactory on
//   `aion_gs_test_economy`, skipped without AION_TEST_GS_DATABASE_URL) or runs without any database on purpose: without DatabaseFactory every DAO
//   write returns false (DB.insertUpdate and InventoryDAO.store catch the SQLException), which is how the "stored before added" cases stop the
//   Java flow at its store. ctest runs every case in its own process, so a case without MAIL_REQUIRE_DATABASE never has a database.
// - Do not mix this fixture with tests/world/WorldTestSupport.h in one executable run (m5c-plan.md §18.7).

#include "../../cm_ak/ItemPacketTestSupport.h"
#include "EconomyTestSupport.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <spdlog/details/null_mutex.h>
#include <spdlog/sinks/base_sink.h>

#include "aion/commons/database/Connection.h"
#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/configs/main/PricesConfig.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Letter.h"
#include "aion/gameserver/model/gameobjects/LetterType.h"
#include "aion/gameserver/model/gameobjects/player/Mailbox.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/items/storage/StorageTypeInfo.h"
#include "aion/gameserver/services/ExchangeService.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::economy::test::mail {

namespace cp = network::aion::clientpackets::testing;
namespace itemtest = cp::items;

using model::gameobjects::Item;
using model::gameobjects::Letter;
using model::gameobjects::LetterType;
using model::gameobjects::player::Mailbox;
using model::gameobjects::player::Player;
using model::items::storage::StorageType;
using network::test::PacketReader;
using network::test::PacketWriter;

// ServerPacketsOpcodes.java:42-179
inline constexpr int32_t SM_MESSAGE_OPCODE = 24;
inline constexpr int32_t SM_SYSTEM_MESSAGE_OPCODE = itemtest::SM_SYSTEM_MESSAGE_OPCODE;
inline constexpr int32_t SM_DELETE_ITEM_OPCODE = itemtest::SM_DELETE_ITEM_OPCODE;
inline constexpr int32_t SM_MAIL_SERVICE_OPCODE = 161;

// MailMessage.java: the ids SM_MAIL_SERVICE(1, message) writes
inline constexpr int32_t MAIL_SEND_SUCCESS = 0;
inline constexpr int32_t NO_SUCH_CHARACTER_NAME = 1;
inline constexpr int32_t RECIPIENT_MAILBOX_FULL = 2;
inline constexpr int32_t MAIL_IS_ONE_RACE_ONLY = 3;
inline constexpr int32_t YOU_ARE_IN_RECIPIENT_IGNORE_LIST = 4;

// the item ids of the rows (ItemPacketTestSupport.h's and the ones below), with their quality and price
inline constexpr int32_t KINAH = itemtest::KINAH;
inline constexpr int32_t MINOR_LIFE_POTION = itemtest::MINOR_LIFE_POTION;       // COMMON, 250, stacks to 1000, tradeable (mask 12414)
inline constexpr int32_t MANASTONE_HP_20 = itemtest::MANASTONE_HP_20;           // COMMON, 10, stacks to 10000
inline constexpr int32_t SPARKIE_CARAPACE_FRAGMENT = itemtest::SPARKIE_CARAPACE_FRAGMENT; // JUNK, 300
inline constexpr int32_t FRUIT_JUICE = itemtest::MERCENARYS_FRUIT_JUICE;        // not tradeable (mask 12360), no disposition
inline constexpr int32_t SURE_STRIKE_STIGMA = itemtest::SURE_STRIKE_STIGMA;     // LEGEND, 3970, does not stack
inline constexpr int32_t TRAINING_SWORD = itemtest::TRAINING_SWORD;             // COMMON, 5, equippable
inline constexpr int32_t ROSE_QUARTZ_ORE = 152000301;                          // RARE, 26
inline constexpr int32_t PURE_VOLUSPAR_ORE = 152000120;                        // LEGEND, 9045
inline constexpr int32_t COURIER_PASS_ABYSS_FABLED = 188950008;                // UNIQUE, 5: the shield's disposition item
inline constexpr int32_t COURIER_PASS_ETERNAL = 188950005;                     // EPIC, 5
inline constexpr int32_t COURIER_PASS_MYTHIC = 188950018;                      // MYTHIC, 5
inline constexpr int32_t SQUAD_LEADERS_SHIELD = 115000892;                     // UNIQUE, 61450, not tradeable (mask 69720), disposition 188950008 x 4

/** item_templates.xml, verbatim: the mail cases' rows that ItemPacketTestSupport.h lacks */
inline constexpr std::string_view MAIL_ITEM_ROWS = R"xml(
	<!-- :612582 -->
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
	<!-- :743673 -->
	<item_template id="152000120" name="Pure Voluspar Ore" level="50" cName="noblemetal_n_l_50b" mask="4222" max_stack_count="10000" item_group="GATHERABLE" quality="LEGEND" price="9045" desc="751141"/>
	<!-- :743779 -->
	<item_template id="152000301" name="Rose Quartz Ore" level="10" cName="jewelry_n_c_10a" mask="4222" max_stack_count="10000" item_group="GATHERABLE" quality="RARE" price="26" desc="702011"/>
	<!-- :930725 -->
	<item_template id="188950005" name="Special Courier Pass (Eternal/Lv. 50 and lower)" level="1" cName="World_cash_coin_post_E_50" mask="12410" max_stack_count="10000" quality="EPIC" price="5" desc="801828"/>
	<!-- :930728 -->
	<item_template id="188950008" name="Special Courier Pass (Abyss Fabled/Lv. 50 and lower)" level="1" cName="World_cash_coin_post_abyss_U_50" mask="12410" max_stack_count="10000" quality="UNIQUE" price="5" desc="801831"/>
	<!-- :930738 -->
	<item_template id="188950018" name="Special Courier Pass (Mythic/Lv. 56-60)" level="1" cName="World_cash_coin_post_M_60" mask="12410" max_stack_count="10000" quality="MYTHIC" price="5" desc="846955"/>
)xml";

/** item_templates.xml, verbatim: StarterKitService's level-1 and level-25 items (StarterKitService.java:36, :44-47) */
inline constexpr std::string_view STARTER_KIT_ITEM_ROWS = R"xml(
	<!-- :856965 -->
	<item_template id="169610056" name="[Title Card] Novice of Atreia – 30-day pass" level="1" cName="world_cash_add_title_188_30" mask="4168" quality="COMMON" price="5" desc="793159" activate_count="1">
		<actions>
			<titleadd titleid="188" minutes="43201"/>
		</actions>
	</item_template>
	<!-- :932713 -->
	<item_template id="190100032" name="Pagati Ironhide" level="1" cName="WORLD_CASH_ride_PAGATI_002" casting_delay="3000" mask="4101" quality="EPIC" price="5" restrict="10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10" desc="797484" activate_target="STANDALONE" activate_count="1000">
		<actions>
			<ride npc_id="2000021"/>
		</actions>
	</item_template>
	<!-- :836053 -->
	<item_template id="164002272" name="[Event] Enduring Greater Raging Wind Scroll" level="30" cName="world_event_scroll_speed_fly_50a" mask="12352" max_stack_count="1000" quality="RARE" price="5" desc="832338" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="3" skillid="10884"/>
		</actions>
		<uselimits usedelay="1000" usedelayid="35"/>
	</item_template>
	<!-- :830953 -->
	<item_template id="162000039" name="Divine Wind Serum" level="40" cName="potion_a_flytime_02a" mask="12360" max_stack_count="1000" item_type="ABYSS" quality="RARE" price="5" desc="717216" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="2" skillid="10201"/>
		</actions>
		<acquisition type="AP" ap="150"/>
		<uselimits usedelay="67500" usedelayid="15" ride_usable="true"/>
	</item_template>
	<!-- :832260 -->
	<item_template id="162002018" name="[Event] Wormwood Dish" level="30" cName="event_potion_cure_hp_50a" mask="12360" max_stack_count="1000" quality="COMMON" price="5" desc="795089" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="2" skillid="9914"/>
		</actions>
		<uselimits usedelay="30000" usedelayid="12"/>
	</item_template>
)xml";

/** ItemPacketTestSupport.h's rows and the rows above in one <item_templates> document */
inline std::string mailItemTemplatesXml() {
	std::string xml(itemtest::ITEM_TEMPLATES_XML);
	xml.erase(xml.rfind("</item_templates>"));
	xml += MAIL_ITEM_ROWS;
	xml += STARTER_KIT_ITEM_ROWS;
	xml += "</item_templates>";
	return xml;
}

/** A saved atomic configuration value, restored at the end of the scope */
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

/** Keeps the messages one logger writes (its level is set to debug for the scope), in order */
class LogCapture {
public:
	explicit LogCapture(std::string loggerName) : name(std::move(loggerName)) {
		sink = std::make_shared<Sink>();
		commons::logging::LoggerFactory::configure(name, {.level = spdlog::level::debug, .sinks = {sink}, .additive = true});
	}
	~LogCapture() { commons::logging::LoggerFactory::removeConfig(name); }
	LogCapture(const LogCapture&) = delete;
	LogCapture& operator=(const LogCapture&) = delete;

	std::vector<std::string> lines() const {
		std::lock_guard lock(sink->guard);
		return sink->lines;
	}

	/** @return true if a line contains the text */
	bool contains(std::string_view text) const {
		for (const std::string& line : lines())
			if (line.find(text) != std::string::npos)
				return true;
		return false;
	}

private:
	struct Sink : spdlog::sinks::base_sink<spdlog::details::null_mutex> {
		mutable std::mutex guard;
		std::vector<std::string> lines;

	protected:
		void sink_it_(const spdlog::details::log_msg& msg) override {
			std::lock_guard lock(guard);
			lines.emplace_back(msg.payload.data(), msg.payload.size());
		}
		void flush_() override {}
	};

	std::string name;
	std::shared_ptr<Sink> sink;
};

/** The first column of the first row of a query as text, std::nullopt for NULL or no row (EconomyTestSupport.h's queryLong for strings) */
inline std::optional<std::string> queryString(std::string_view sql) {
	auto con = commons::database::DatabaseFactory::getConnection();
	auto rs = con->prepareStatement(sql)->executeQuery();
	if (!rs->next())
		return std::nullopt;
	return rs->getObject<std::string>(1);
}

/** One entry of SM_MAIL_SERVICE(2)'s letter list (SM_MAIL_SERVICE.java writeLettersList) */
struct ListedLetter {
	int32_t letterId;
	std::string senderName;
	std::string title;
	bool isRead;
	int32_t attachedItemObjId;
	int32_t attachedItemId;
	int64_t attachedKinah;
	int32_t letterType;
};

/** SM_MAIL_SERVICE(2) decoded: C(2), D(player), C(0), H(±count: negative for the last packet), the letters */
struct LetterList {
	int32_t playerObjId = 0;
	int32_t signedCount = 0;
	std::vector<ListedLetter> letters;
};

/** The service id of a captured SM_MAIL_SERVICE (its first body byte) */
inline int32_t mailServiceIdOf(const std::vector<uint8_t>& packet) {
	std::vector<uint8_t> body = cp::bodyOf(packet);
	return body.empty() ? -1 : body[0];
}

inline LetterList decodeLetterList(const std::vector<uint8_t>& packet) {
	PacketReader reader(cp::bodyOf(packet));
	LetterList list;
	EXPECT_EQ(reader.C(), 2);
	list.playerObjId = reader.D();
	EXPECT_EQ(reader.C(), 0);
	list.signedCount = reader.H();
	int32_t count = list.signedCount < 0 ? -list.signedCount : list.signedCount;
	for (int32_t i = 0; i < count; i++) {
		ListedLetter letter{};
		letter.letterId = reader.D();
		letter.senderName = reader.S();
		letter.title = reader.S();
		letter.isRead = reader.C() == 1;
		letter.attachedItemObjId = reader.D();
		letter.attachedItemId = reader.D();
		letter.attachedKinah = reader.Q();
		letter.letterType = reader.C();
		list.letters.push_back(letter);
	}
	EXPECT_EQ(reader.remaining(), 0u);
	return list;
}

/** SM_MAIL_SERVICE(1, message) (SM_MAIL_SERVICE.java writeMailMessage): C(1), C(message id) */
inline std::vector<uint8_t> mailMessage(int32_t messageId) {
	return itemtest::javaPacket(SM_MAIL_SERVICE_OPCODE, PacketWriter().C(1).C(messageId));
}

/** SM_MAIL_SERVICE(0) (SM_MAIL_SERVICE.java writeMailboxState): C(0), H(total), H(unread), H(unread express), H(unread black cloud) */
inline std::vector<uint8_t> mailboxState(int32_t total, int32_t unread, int32_t express, int32_t blackCloud) {
	return itemtest::javaPacket(SM_MAIL_SERVICE_OPCODE, PacketWriter().C(0).H(total).H(unread).H(express).H(blackCloud));
}

/** SM_MAIL_SERVICE(5) (SM_MAIL_SERVICE.java writeLetterState): C(5), D(letterId), C(attachmentType), C(1) */
inline std::vector<uint8_t> letterState(int32_t letterId, int32_t attachmentType) {
	return itemtest::javaPacket(SM_MAIL_SERVICE_OPCODE, PacketWriter().C(5).D(letterId).C(attachmentType).C(1));
}

/** SM_DELETE_ITEM(objId) of the default delete type (SM_DELETE_ITEM.java: ItemDeleteType.DEFAULT, mask 0) */
inline std::vector<uint8_t> deleteItem(int32_t objId) {
	return itemtest::deleteItem(objId, 0);
}

/** A Letter as MailDAO.loadPlayerMailbox builds one: state UPDATED, received at `millis` */
inline runtime::Ref<Letter> loadedLetter(int32_t letterId, int32_t recipientId, runtime::Ptr<Item> item, int64_t kinah, std::string_view title,
	std::string_view sender, int64_t millis, bool unread, LetterType type) {
	runtime::Ref<Letter> letter = Letter::create(letterId, recipientId, item, kinah, title, "", sender,
		commons::database::Timestamp(std::chrono::milliseconds(millis)), unread, type);
	letter->setPersistentState(model::gameobjects::Persistable_PersistentState::UPDATED);
	return letter;
}

inline constexpr int32_t A_ID = 710101;
inline constexpr int32_t A_ACCOUNT = 9901;
inline constexpr int32_t B_ID = 710202;
inline constexpr int32_t B_ACCOUNT = 9902;
inline constexpr std::string_view A_NAME = "Holder";
inline constexpr std::string_view B_NAME = "Partner";

/**
 * ItemPacketTest with the recipient B beside the sender A, both with a Mailbox, the mail rows published and the shipped price and logging
 * configuration. Cases give items to either player the way the DAO loads them (no packet).
 */
class MailTest : public itemtest::ItemPacketTest {
protected:
	void SetUp() override {
		ItemPacketTest::SetUp();
		dataholders::DataManager::ITEM_DATA.resetForTests(); // the base rows, republished below with the mail rows
		xml::LoadContext context;
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(context, mailItemTemplatesXml()));
		player().setMailbox(std::make_unique<Mailbox>(player()));

		b = cp::makePlayer(B_ID, B_ACCOUNT, B_NAME);
		b.player->setPosition(world::WorldPosition::create(210010000, 102.0f, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(102.0f, 100.0f, 50.0f)));
		b.player->setMailbox(std::make_unique<Mailbox>(*b.player));
		clientB = std::make_unique<cp::TestClient>();
		clientB->enterWorld(b);
		clearSent();
		clearSentB();
	}

	void TearDown() override {
		if (f.player && services::ExchangeService::getInstance().isPlayerInExchange(*f.player))
			services::ExchangeService::getInstance().cancelExchange(*f.player);
		for (const runtime::Ref<Player>& online : inWorld)
			world::World::getInstance().removeObject(*online);
		inWorld.clear();
		if (b.player) {
			b.commonData->setOnline(false);
			b.player->setClientConnection(nullptr);
		}
		clientB.reset();
		given.clear();
		b = {};
		ItemPacketTest::TearDown();
	}

	Player& a() { return player(); }
	Player& partner() { return *b.player; }

	/** Java PlayerEnterWorldService for the mail's purposes: the player is in the World and his common data is online */
	void goOnline(Player& online) {
		world::World::getInstance().storeObject(online);
		online.getCommonData()->setOnline(true);
		inWorld.emplace_back(online);
	}

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

	int64_t countOf(Player& owner, int32_t itemId) { return owner.getInventory().getItemCountByItemId(itemId); }

	std::vector<std::vector<uint8_t>> sentB() { return (*clientB)->sentBytes(); }
	void clearSentB() { (*clientB)->clearSent(); }

	std::vector<uint8_t> serializedForB(network::aion::AionServerPacket&& packet) { return cp::serialized(std::move(packet), clientB->con()); }

	/** The captured packets of one opcode, in order */
	static std::vector<std::vector<uint8_t>> ofOpcode(const std::vector<std::vector<uint8_t>>& packets, int32_t opcode) {
		return itemtest::packetsOf(packets, opcode);
	}

	/** @return false (and the case skips) without the test database; creates it once per process and inserts the players rows of A and B */
	bool requireDatabase() {
		if (!isDatabaseEnabled())
			return false;
		setUpDatabaseOnce();
		insertPlayer(A_ID, A_NAME, A_ACCOUNT);
		insertPlayer(B_ID, B_NAME, B_ACCOUNT);
		return true;
	}

	cp::PlayerFixture b;
	std::unique_ptr<cp::TestClient> clientB;
	std::vector<runtime::Ref<Item>> given;
	std::vector<runtime::Ref<Player>> inWorld;
	// the shipped prices.properties, logging.properties and admin.properties values the services read (the unit tests load no properties)
	ConfigScope<int32_t> defaultPrices{configs::main::PricesConfig::DEFAULT_PRICES, 100};
	ConfigScope<int32_t> defaultModifier{configs::main::PricesConfig::DEFAULT_MODIFIER, 100};
	ConfigScope<int32_t> defaultTaxes{configs::main::PricesConfig::DEFAULT_TAXES, 100};
	ConfigScope<bool> logAudit{configs::main::LoggingConfig::LOG_AUDIT, true};
	ConfigScope<bool> logMail{configs::main::LoggingConfig::LOG_MAIL, false};
	ConfigScope<bool> logSysMail{configs::main::LoggingConfig::LOG_SYSMAIL, false};
	ConfigScope<int8_t> unrestrictedItemTrade{configs::administration::AdminConfig::UNRESTRICTED_ITEMTRADE, int8_t{1}};
};

/** Skips the case without the test database (use at the start of a TEST_F body of MailTest) */
#define MAIL_REQUIRE_DATABASE()                                                                                                                       \
	if (!requireDatabase())                                                                                                                           \
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL to run the database tests";

} // namespace aion::gameserver::economy::test::mail
