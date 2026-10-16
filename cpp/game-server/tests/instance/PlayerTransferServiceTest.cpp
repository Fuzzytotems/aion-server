// P5-13 PlayerTransferService, PlayerTransfer.getDB and CMT_CHARACTER_INFORMATION (m5j-plan.md §18.1 stage 1 CP3, item S-08; D-04: a
// login-server player transfer) on the DAO test schema, with the login slice's fixture (tests/login_slice/SliceDbTest.h, by relative path):
// the source server's refusals and its transfer task (onOk deletes the character, onError forgets the task), the target server's clone of a
// character from the six data arrays (the transfer options, a taken name, the character limit).
//
// The login server link is not up in this process: SM_PTRANSFER_CONTROL goes nowhere (LoginServer.sendPacket answers false), so the cases
// read the logs and the rows. Expectations are derived by hand from PlayerTransferService.java:52-195, PlayerTransfer.java:85-97 and
// CMT_CHARACTER_INFORMATION.java:63-396 (the byte layout below is its read order).

#include "../login_slice/SliceDbTest.h"
#include "../support/NetworkTestSupport.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/configs/main/PlayerTransferConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/TitleData.bind.h"
#include "aion/gameserver/dataholders/TitleData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/Gender.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/storage/PlayerStorage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/services/player/PlayerService.h"
#include "aion/gameserver/services/transfers/PlayerTransfer.h"
#include "aion/gameserver/services/transfers/PlayerTransferService.h"

namespace aion::gameserver::loginslice::test {
namespace {

using network::test::LogCapture;
using network::test::PacketWriter;
using runtime::Ptr;
using runtime::Ref;
using services::transfers::PlayerTransfer;
using services::transfers::PlayerTransferService;

constexpr std::string_view TRANSFER_LOG = "com.aionemu.gameserver.services.transfers.PlayerTransferService";
constexpr std::string_view TEXT_LOG = "PLAYERTRANSFER";

// item_templates.xml:859307 and the kinah row of tests/cm_ak, verbatim
constexpr std::string_view ITEM_TEMPLATES = R"xml(<item_templates>
	<item_template id="169670001" name="[Event] Name Change Ticket" level="1" cName="event_change_character_name_01" mask="4168" quality="COMMON" price="10000" desc="745179"/>
	<item_template id="182400001" name="Kinah" level="1" cName="gold" mask="12350" quality="COMMON" price="0" desc="701677"/>
</item_templates>)xml";

template <class T>
class ConfigScope {
public:
	ConfigScope(std::atomic<T>& config, T value) : config_(config), previous_(config.load()) { config.store(value); }
	~ConfigScope() { config_.store(previous_); }

private:
	std::atomic<T>& config_;
	const T previous_;
};

Ref<runtime::Array<int8_t>> array(const PacketWriter& w) {
	Ref<runtime::Array<int8_t>> a = runtime::Array<int8_t>::make(static_cast<int32_t>(w.data.size()));
	for (size_t i = 0; i < w.data.size(); ++i)
		(*a)[static_cast<int32_t>(i)] = static_cast<int8_t>(w.data[i]);
	return a;
}

/** One inventory row of the items array (CMT_CHARACTER_INFORMATION.java:113-149) */
void item(PacketWriter& w, int32_t oldId, int32_t itemId, int64_t count, int32_t location) {
	w.D(oldId).D(itemId).Q(count).D(-1).S("").D(0).D(0).C(0).C(0).Q(0).D(location).D(0).D(0).D(0).D(0).D(0).D(0).D(0).C(0).C(0);
	w.D(0).D(0).D(0).D(0).D(0).D(0).D(0).C(0).H(0);
}

/**
 * A level-2 Asmodian female warrior (exp 400) at (571, 2787, 299) of Pandaemonium: a 50 kinah stack in the cube and a stack in the warehouse,
 * an item without template; the emotion 64, the macro 2, the title 7; the unknown skill 999; the completed quest 1001.
 */
Ref<PlayerTransfer> transfer(int32_t taskId, int32_t targetAccount, std::string_view name) {
	Ref<PlayerTransfer> t = PlayerTransfer::create(taskId, targetAccount, "account" + std::to_string(targetAccount), name);
	PacketWriter common;
	common.D(0).Q(400).D(1).D(1).D(0).D(100).D(0).D(0).D(0).D(0); // class WARRIOR, exp, ASMODIANS, FEMALE, title, dp, the four expands
	common.D(0x111111).D(0x222222).D(0x333333).D(0x444444);   // skin, hair, eye, lip RGB
	for (int i = 0; i < 48; ++i)
		common.C(i == 0 ? 3 : 0); // face 3, the other 47 shape values 0
	common.F(1.25f);              // height
	common.F(571.0f).F(2787.0f).F(299.0f).C(32).D(220010000);
	PacketWriter items;
	items.D(3);
	item(items, 501, 182400001, 50, 0);   // StorageType.CUBE
	item(items, 502, 182400001, 70, 1);   // StorageType.REGULAR_WAREHOUSE
	item(items, 503, 100000001, 1, 0);    // no template
	PacketWriter data;
	data.D(1).D(64).D(0);                   // emotions
	data.D(0);                              // motions
	data.D(1).D(2).S("<macro>two</macro>"); // macros
	data.D(0);                              // npc factions
	data.D(0);                              // pets
	data.D(1).D(7).D(0);                    // titles
	data.D(0).D(0).D(3).D(0);               // ui and shortcut lengths, deny, penalty
	PacketWriter skills;
	skills.D(1).D(999).D(1);
	PacketWriter recipes;
	recipes.D(0);
	PacketWriter quests;
	quests.D(1).D(1001).S("COMPLETE").D(0).D(1).D(-1).Q(1'700'000'000'000).Q(1'700'086'400'000).D(0); // complete, next repeat
	t->setCommonData(array(common));
	t->setItemsData(array(items));
	t->setData(array(data));
	t->setSkillData(array(skills));
	t->setRecipeData(array(recipes));
	t->setQuestData(array(quests));
	return t;
}

class PlayerTransferServiceTest : public SliceDbTest {
protected:
	void SetUp() override {
		if (!dataholders::DataManager::ITEM_DATA) // before the slice's empty table: published once per process
			dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(itemContext(), std::string(ITEM_TEMPLATES)));
		if (!dataholders::DataManager::TITLE_DATA) // TitleList.addEntry reads the title
			dataholders::DataManager::TITLE_DATA.publish(xml::bindString<dataholders::TitleData>(itemContext(),
				R"(<player_titles><title id="7" nameId="1101606" desc="test title" race="PC_ALL"/></player_titles>)"));
		SliceDbTest::SetUp();
	}

	static xml::LoadContext& itemContext() {
		static xml::LoadContext context;
		return context;
	}

	/** a stored character of account 21 (PlayerServiceTest's newCharacter, storeNewPlayer) */
	void sourceCharacter(int32_t objectId, std::string_view name) {
		Ref<model::account::Account> account = model::account::Account::create(21);
		account->setName("account21");
		account->setAccountWarehouse(
			std::make_unique<model::items::storage::PlayerStorage>(*account, model::items::storage::StorageType::ACCOUNT_WAREHOUSE));
		Ref<model::gameobjects::player::PlayerCommonData> commonData = model::gameobjects::player::PlayerCommonData::create(objectId);
		commonData->setName(name);
		commonData->setGender(model::Gender::MALE);
		commonData->setRace(model::Race::ELYOS);
		commonData->setPlayerClass(model::PlayerClass::WARRIOR);
		commonData->setLevel(1);
		Ref<model::gameobjects::player::PlayerAppearance> appearance = model::gameobjects::player::PlayerAppearance::create();
		auto part = std::make_unique<model::account::PlayerAccountData>(*account, *commonData, *appearance);
		Ptr<model::account::PlayerAccountData> data(*part);
		account->addPlayerAccountData(std::move(part));
		Ref<model::gameobjects::player::Player> player;
		SLICE_SKIP_IF_UNPORTED(player = services::player::PlayerService::newPlayer(*data, *account));
		ASSERT_TRUE(services::player::PlayerService::storeNewPlayer(*player, account->getName(), account->getId()));
	}

	static int64_t count(std::string_view sql) { return queryLong(sql).value_or(-1); }
};

/** PlayerTransfer.java:85-97: the arrays in order; a missing one is Java's NPE on its length */
TEST_F(PlayerTransferServiceTest, TheDatabaseBufferConcatenatesTheArrays) {
	Ref<PlayerTransfer> t = PlayerTransfer::create(1, 2, "a", "n");
	t->setCommonData(runtime::Array<int8_t>::of({1}));
	t->setItemsData(runtime::Array<int8_t>::of({2, 3}));
	t->setData(runtime::Array<int8_t>::of({4}));
	t->setSkillData(runtime::Array<int8_t>::of({}));
	t->setRecipeData(runtime::Array<int8_t>::of({5}));
	EXPECT_THROW(t->getDB(), runtime::NullPointerException) << "no quest data";
	t->setQuestData(runtime::Array<int8_t>::of({-1}));
	EXPECT_EQ(t->getDB(), (std::vector<uint8_t>{1, 2, 3, 4, 5, 0xFF}));
}

/**
 * PlayerTransferService.java:130-170, CMT_CHARACTER_INFORMATION.java:63-396: the clone is a new character of the target account with the
 * data the options allow (inventory, macros, titles, quests here; not the warehouse or the emotions) and the bind point of its race; the
 * stored task is gone. Java's last statement, PlayerService.storePlayer, ends in AccountPassportsDAO.storePassport(player.getAccount()) of an
 * account whose passports were never loaded (getPassportsList() is null: PlayerService.java:97): the NullPointerException leaves the clone
 * stored without the transfer time, the OK packet or the log line (kept; proposed correction J-CP3-1)
 */
TEST_F(PlayerTransferServiceTest, TheCloneIsANewCharacterOfTheTargetAccount) {
	using configs::main::PlayerTransferConfig;
	ConfigScope<bool> inv(PlayerTransferConfig::ALLOW_INV, true);
	ConfigScope<bool> warehouse(PlayerTransferConfig::ALLOW_WAREHOUSE, false);
	ConfigScope<bool> emotions(PlayerTransferConfig::ALLOW_EMOTIONS, false);
	ConfigScope<bool> macros(PlayerTransferConfig::ALLOW_MACRO, true);
	ConfigScope<bool> titles(PlayerTransferConfig::ALLOW_TITLES, true);
	ConfigScope<bool> questsAllowed(PlayerTransferConfig::ALLOW_QUESTS, true);
	PlayerTransferService& service = PlayerTransferService::getInstance();
	Ref<PlayerTransfer> t = transfer(5, 31, "Traveler");
	service.putTransfer(5, *t);
	EXPECT_EQ(service.getTransfer(5), t);
	LogCapture capture({TEXT_LOG, TRANSFER_LOG});

	EXPECT_THROW(service.cloneCharacter(5, *t), runtime::NullPointerException) << "storePassport";
	EXPECT_EQ(service.getTransfer(5), nullptr);
	const std::optional<int64_t> id = queryLong("SELECT id FROM players WHERE name = 'Traveler'");
	ASSERT_TRUE(id.has_value()) << capture.dump();
	const std::string where = " WHERE player_id = " + std::to_string(*id);
	EXPECT_EQ(count("SELECT account_id FROM players WHERE id = " + std::to_string(*id)), 31);
	EXPECT_EQ(queryString("SELECT race FROM players WHERE id = " + std::to_string(*id)), std::optional<std::string>("ASMODIANS"));
	EXPECT_EQ(queryString("SELECT gender FROM players WHERE id = " + std::to_string(*id)), std::optional<std::string>("FEMALE"));
	EXPECT_EQ(count("SELECT exp FROM players WHERE id = " + std::to_string(*id)), 400);
	EXPECT_EQ(count("SELECT world_id FROM players WHERE id = " + std::to_string(*id)), 220010000);
	EXPECT_EQ(count("SELECT last_transfer_time FROM players WHERE id = " + std::to_string(*id)), 0) << "after the NullPointerException";
	EXPECT_EQ(count("SELECT face FROM player_appearance" + where), 3);
	EXPECT_EQ(count("SELECT COUNT(*) FROM inventory WHERE item_owner = " + std::to_string(*id)), 1) << "the cube stack only";
	EXPECT_EQ(count("SELECT item_count FROM inventory WHERE item_owner = " + std::to_string(*id)), 50);
	EXPECT_NE(count("SELECT item_unique_id FROM inventory WHERE item_owner = " + std::to_string(*id)), 501) << "a new object id";
	EXPECT_EQ(queryString("SELECT macro FROM player_macrosses" + where), std::optional<std::string>("<macro>two</macro>"));
	EXPECT_EQ(count("SELECT title_id FROM player_titles" + where), 7);
	EXPECT_EQ(queryString("SELECT status FROM player_quests" + where + " AND quest_id = 1001"), std::optional<std::string>("COMPLETE"));
	EXPECT_EQ(count("SELECT COUNT(*) FROM player_emotions" + where), 0) << "emotions not allowed";
	EXPECT_EQ(count("SELECT map_id FROM player_bind_point" + where), 220010000) << "ptransfer.bindpoint.asmo";
	EXPECT_EQ(capture.count("item with id 100000001 was not found in templates"), 1) << capture.dump();
	EXPECT_EQ(capture.count("null skillid:999 name:Traveler"), 1);
	EXPECT_EQ(capture.count("taskId:5; [CloneCharacter:Done]"), 0);
}

/**
 * PlayerTransferService.java:136-155: a taken name gets "_1"; with BLOCK_SAMENAME no clone. The name change ticket (:163-164) follows readInfo,
 * whose storePassport NullPointerException comes first (see above): no ticket
 */
TEST_F(PlayerTransferServiceTest, ATakenNameIsNumberedOrBlocked) {
	using configs::main::PlayerTransferConfig;
	ASSERT_NO_FATAL_FAILURE(sourceCharacter(2101, "Traveler"));
	PlayerTransferService& service = PlayerTransferService::getInstance();
	{
		ConfigScope<bool> block(PlayerTransferConfig::BLOCK_SAMENAME, true);
		service.cloneCharacter(6, *transfer(6, 31, "Traveler"));
		EXPECT_EQ(count("SELECT COUNT(*) FROM players WHERE account_id = 31"), 0);
	}
	ConfigScope<bool> allow(PlayerTransferConfig::BLOCK_SAMENAME, false);
	EXPECT_THROW(service.cloneCharacter(7, *transfer(7, 31, "Traveler")), runtime::NullPointerException);
	const std::optional<int64_t> id = queryLong("SELECT id FROM players WHERE name = 'Traveler_1'");
	ASSERT_TRUE(id.has_value());
	EXPECT_EQ(count("SELECT COUNT(*) FROM inventory WHERE item_id = 169670001 AND item_owner = " + std::to_string(*id)), 0) << "no ticket";
}

/** PlayerTransferService.java:156-159: a target account without a free slot gets no clone */
TEST_F(PlayerTransferServiceTest, AFullTargetAccountGetsNoClone) {
	ConfigScope<int32_t> limit(configs::main::GSConfig::CHARACTER_LIMIT_COUNT, 1);
	ASSERT_NO_FATAL_FAILURE(sourceCharacter(2101, "Resident")); // account 21: one character
	PlayerTransferService::getInstance().cloneCharacter(8, *transfer(8, 21, "Traveler"));
	EXPECT_EQ(count("SELECT COUNT(*) FROM players WHERE name = 'Traveler'"), 0);
}

/**
 * PlayerTransferService.java:52-125, :175-187: the source server refuses a character of another account, a legion member, an online one and
 * one transferred within the reuse hours; a transfer it starts is a task that onOk completes by deleting the character and onError forgets
 */
TEST_F(PlayerTransferServiceTest, TheSourceServerChecksAndCompletesTheTask) {
	using configs::main::PlayerTransferConfig;
	ASSERT_NO_FATAL_FAILURE(sourceCharacter(2101, "Mover"));
	PlayerTransferService& service = PlayerTransferService::getInstance();
	LogCapture capture({TEXT_LOG, TRANSFER_LOG});

	service.startTransfer(77, 31, 2101, 2, 1);
	EXPECT_EQ(capture.count("transfer #1 player 2101 is not present on account 77."), 1) << capture.dump();

	execute("INSERT INTO legions (id, name) VALUES (50, 'Fifty')");
	execute("INSERT INTO legion_members (legion_id, player_id) VALUES (50, 2101)");
	service.startTransfer(21, 31, 2101, 2, 2);
	EXPECT_EQ(capture.count("cannot transfer #2 player with existing legion 2101."), 1);
	execute("DELETE FROM legion_members");

	execute("UPDATE players SET online = 1 WHERE id = 2101");
	service.startTransfer(21, 31, 2101, 2, 3);
	EXPECT_EQ(capture.count("cannot transfer #3 online players 2101."), 1);
	execute("UPDATE players SET online = 0 WHERE id = 2101");

	{
		ConfigScope<int32_t> reuse(PlayerTransferConfig::REUSE_HOURS, 24);
		execute("UPDATE players SET last_transfer_time = " + std::to_string(commons::utils::currentTimeMillis() - 3'600'000) + " WHERE id = 2101");
		service.startTransfer(21, 31, 2101, 2, 4);
		EXPECT_EQ(capture.count("cannot transfer #4 that player so often 2101."), 1);
	}

	service.startTransfer(21, 31, 2101, 2, 9);
	EXPECT_EQ(capture.count("taskId:9; [StartTransfer]"), 1) << capture.dump();
	service.startTransfer(21, 31, 2101, 2, 10);
	service.onError(10, "target down");
	EXPECT_EQ(capture.count("taskId:10; [Error. Transfer failed] target down"), 1);
	EXPECT_THROW(service.onOk(10), runtime::NullPointerException) << "the task is forgotten";
	EXPECT_EQ(count("SELECT COUNT(*) FROM players WHERE id = 2101"), 1);

	service.onOk(9);
	EXPECT_EQ(count("SELECT COUNT(*) FROM players WHERE id = 2101"), 0) << "the transferred character is deleted";
	EXPECT_EQ(capture.count("taskId:9; [TransferComplete]"), 1);
}

} // namespace
} // namespace aion::gameserver::loginslice::test
