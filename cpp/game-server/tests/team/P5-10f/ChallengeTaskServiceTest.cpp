// P5-10f (M5h F2b): ChallengeTaskService against ChallengeTaskService.java - the legion task list, a legion task's quests finished up to the
// completion (scores, the complete count stored, the offline member stored) and canRaiseLegionLevel - over the test database
// (ChallengeTestDatabase.h: run under gate_lock.py) and real Players on recording connections (tests/team/P5-10b/TeamTestSupport.h).

#include "../P5-10b/TeamTestSupport.h"
#include "ChallengeTestDatabase.h"

#include <string>

#include <gtest/gtest.h>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/dataholders/ChallengeData.bind.h"
#include "aion/gameserver/dataholders/ChallengeData.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/HouseData.bind.h"
#include "aion/gameserver/dataholders/HouseData.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/TownSpawnsData.bind.h"
#include "aion/gameserver/dataholders/TownSpawnsData.h"
#include "aion/gameserver/dataholders/WorldMapsData.bind.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/model/templates/challenge/ChallengeType.h"
#include "aion/gameserver/network/aion/clientpackets/CM_CHALLENGE_LIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CHALLENGE_LIST.h"
#include "aion/gameserver/services/ChallengeTaskService.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/LegionService.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/geo/GeoService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

/** The friend CM_CHALLENGE_LIST.h declares: the fields readImpl decoded */
struct CM_CHALLENGE_LISTTestAccess {
	static int32_t action(const CM_CHALLENGE_LIST& p) { return p.action; }
	static int32_t taskOwner(const CM_CHALLENGE_LIST& p) { return p.taskOwner; }
	static int32_t ownerType(const CM_CHALLENGE_LIST& p) { return p.ownerType; }
	static int32_t playerId(const CM_CHALLENGE_LIST& p) { return p.playerId; }
	static int32_t dateSince(const CM_CHALLENGE_LIST& p) { return p.dateSince; }
};

} // namespace aion::gameserver::network::aion::clientpackets

namespace aion::gameserver::network::aion::clientpackets::testing::team {
namespace {

namespace db = ::aion::gameserver::team::challenge::test;
using model::team::legion::Legion;
using model::team::legion::LegionMember;
using model::templates::challenge::ChallengeType;
using network::test::PacketWriter;
using services::ChallengeTaskService;

constexpr int32_t LEGION_ID = 500;
constexpr int32_t OFFLINE_MEMBER = 900001;

// tasks 300 and 301 of quest_data/challenge_tasks.xml (the repeat counts lowered), an Asmodian task the Elyos legion never sees, a non-repeatable
// task of level 6
constexpr const char* CHALLENGE_XML = R"(<challenge_tasks>
    <task id="300" type="LEGION" race="ELYOS" min_level="5" max_level="6" name_id="1199506" repeat="true" legion_level_task="true">
        <quest id="17000" repeat_count="2" score="81"/>
        <quest id="17001" repeat_count="1" score="150"/>
        <contrib rank="1" number="2" reward_id="186000199" item_count="46"/>
        <reward type="NONE" msg_id="904485"/>
    </task>
    <task id="301" type="LEGION" race="ELYOS" prev_task="300" min_level="6" max_level="7" name_id="1199508" repeat="true" legion_level_task="true">
        <quest id="17003" repeat_count="12" score="111"/>
        <reward type="NONE" msg_id="904485"/>
    </task>
    <task id="302" type="LEGION" race="ASMODIANS" min_level="5" max_level="6" name_id="1199507" repeat="true" legion_level_task="true">
        <quest id="27000" repeat_count="6" score="81"/>
        <reward type="NONE" msg_id="904485"/>
    </task>
    <task id="303" type="LEGION" race="ELYOS" min_level="6" max_level="6" name_id="1199509" repeat="false">
        <quest id="17010" repeat_count="1" score="5"/>
        <reward type="NONE" msg_id="904485"/>
    </task>
</challenge_tasks>)";

class ChallengeTaskServiceTest : public TeamTest {
protected:
	void SetUp() override {
		TeamTest::SetUp();
		dataholders::DataManager::CHALLENGE_DATA.publish(xml::bindString<dataholders::ChallengeData>(context, CHALLENGE_XML));
	}

	void TearDown() override {
		for (Member* m : stored) {
			m->player().setLegionMember(nullptr);
			world::World::getInstance().removeObject(m->player());
		}
		stored.clear();
		dataholders::DataManager::CHALLENGE_DATA.resetForTests();
		TeamTest::TearDown();
	}

	/** @return false without the test database (the test is then skipped) */
	bool requireDatabase() {
		if (!db::isDatabaseEnabled())
			return false;
		db::setUpDatabaseOnce();
		return true;
	}

	static void insertPlayer(int32_t id, std::string_view name) {
		db::execute("INSERT INTO players (id, name, account_id, account_name, x, y, z, heading, world_id, gender, race, player_class, exp) VALUES (" +
			std::to_string(id) + ", '" + std::string(name) + "', " + std::to_string(id) + ", 'account" + std::to_string(id) +
			"', 1, 2, 3, 4, 210010000, 'MALE', 'ELYOS', 'WARRIOR', 0)");
	}

	/** A legion of `level` in the database and LegionService's cache, with the online member A (brigade general) and an offline member */
	Member& legionWithMembers(int32_t level) {
		Member& a = addMember("Challenger");
		world::World::getInstance().storeObject(a.player());
		stored.push_back(&a);
		insertPlayer(a.player().getObjectId(), "Challenger");
		insertPlayer(OFFLINE_MEMBER, "Offliner");
		db::execute("INSERT INTO legions (id, name, level) VALUES (" + std::to_string(LEGION_ID) + ", 'Challengers', " + std::to_string(level) + ")");
		db::execute("INSERT INTO legion_members (legion_id, player_id, `rank`, challenge_score) VALUES (" + std::to_string(LEGION_ID) + ", " +
			std::to_string(a.player().getObjectId()) + ", 'BRIGADE_GENERAL', 0), (" + std::to_string(LEGION_ID) + ", " + std::to_string(OFFLINE_MEMBER) +
			", 'VOLUNTEER', 40)");
		legion = services::LegionService::getInstance().getLegion(LEGION_ID); // loaded by LegionDAO, cached by the service
		runtime::Ptr<LegionMember> member = services::LegionService::getInstance().getLegionMember(a.player().getCommonData()->getPlayerObjId());
		a.player().setLegionMember(member);
		a.clearSent();
		return a;
	}

	int32_t challengeLists(const Member& m) const { return m.count(opcodeOf<serverpackets::SM_CHALLENGE_LIST>); }

	static int64_t rows(std::string_view where) { return *db::queryLong("SELECT COUNT(*) FROM challenge_tasks WHERE " + std::string(where)); }

	xml::LoadContext context;
	runtime::Ptr<Legion> legion;
	std::vector<Member*> stored;
	ConfigScope<bool> enabled{configs::main::CustomConfig::CHALLENGE_TASKS_ENABLED, true};
};

#define CHALLENGE_REQUIRE_DATABASE()                                                                                                                  \
	if (!requireDatabase())                                                                                                                           \
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL to run the database tests";

TEST_F(ChallengeTaskServiceTest, TheLegionListOffersTheFirstTasksOfTheRaceAndLevelAndStoresThemOnce) {
	CHALLENGE_REQUIRE_DATABASE();
	Member& a = legionWithMembers(6);
	ChallengeTaskService::getInstance().showTaskList(a.player(), ChallengeType::LEGION, LEGION_ID);
	// 300 (no previous task) and 303 (level 6) are new; 301 waits for 300, 302 is Asmodian: SM_CHALLENGE_LIST(2) and one (7) per task
	EXPECT_EQ(challengeLists(a), 3);
	EXPECT_EQ(rows("owner_id = 500 AND owner_type = 'LEGION' AND task_id = 300"), 2) << "one row per quest";
	EXPECT_EQ(rows("owner_id = 500 AND task_id = 303"), 1);
	EXPECT_EQ(rows("owner_id = 500 AND task_id IN (301, 302)"), 0);
	a.clearSent();
	db::execute("DELETE FROM challenge_tasks");
	ChallengeTaskService::getInstance().showTaskList(a.player(), ChallengeType::LEGION, LEGION_ID);
	EXPECT_EQ(challengeLists(a), 3) << "the same two tasks, from memory";
	EXPECT_EQ(rows("owner_id = 500"), 0) << "a known task is not stored again";
}

TEST_F(ChallengeTaskServiceTest, ADisabledServiceSendsNothing) {
	CHALLENGE_REQUIRE_DATABASE();
	Member& a = legionWithMembers(5);
	ConfigScope<bool> disabled(configs::main::CustomConfig::CHALLENGE_TASKS_ENABLED, false);
	ChallengeTaskService::getInstance().showTaskList(a.player(), ChallengeType::LEGION, LEGION_ID);
	EXPECT_EQ(challengeLists(a), 0);
	EXPECT_EQ(rows("owner_id = 500"), 0);
}

TEST_F(ChallengeTaskServiceTest, FinishingTheQuestsCompletesTheTaskResetsTheScoresAndUnlocksTheNextTask) {
	CHALLENGE_REQUIRE_DATABASE();
	Member& a = legionWithMembers(6);
	ChallengeTaskService& service = ChallengeTaskService::getInstance();
	EXPECT_FALSE(service.canRaiseLegionLevel(*legion, a.player())) << "no level-6 legion task loaded or completed";
	service.showTaskList(a.player(), ChallengeType::LEGION, LEGION_ID);
	a.clearSent();
	service.onChallengeQuestFinish(a.player(), 17000);
	EXPECT_EQ(a.player().getLegionMember()->getChallengeScore(), 81);
	EXPECT_EQ(*db::queryLong("SELECT complete_count FROM challenge_tasks WHERE task_id = 300 AND quest_id = 17000 AND owner_id = 500"), 1);
	EXPECT_EQ(challengeLists(a), 3) << "the online members are shown the list again";
	service.onChallengeQuestFinish(a.player(), 17000);
	service.onChallengeQuestFinish(a.player(), 17000);
	EXPECT_EQ(a.player().getLegionMember()->getChallengeScore(), 162) << "a quest at its repeat count adds no score";
	EXPECT_EQ(*db::queryLong("SELECT complete_count FROM challenge_tasks WHERE task_id = 300 AND quest_id = 17000 AND owner_id = 500"), 2);
	a.clearSent();
	service.onChallengeQuestFinish(a.player(), 17001); // the task completes: the scores are reset, the offline member is stored
	EXPECT_EQ(a.player().getLegionMember()->getChallengeScore(), 0);
	EXPECT_EQ(*db::queryLong("SELECT challenge_score FROM legion_members WHERE player_id = " + std::to_string(OFFLINE_MEMBER)), 0)
		<< "the offline member's 40 points are reset and stored";
	EXPECT_EQ(*db::queryLong("SELECT complete_count FROM challenge_tasks WHERE task_id = 300 AND quest_id = 17001 AND owner_id = 500"), 1);
	EXPECT_EQ(challengeLists(a), 4) << "301 is offered now: (2), 300 (repeatable), 301, 303";
	EXPECT_EQ(rows("owner_id = 500 AND task_id = 301"), 1);
	service.onChallengeQuestFinish(a.player(), 17001);
	EXPECT_EQ(a.player().getLegionMember()->getChallengeScore(), 0) << "a completed task's quest adds nothing";
}

TEST_F(ChallengeTaskServiceTest, TheLegionLevelNeedsTheCompletedLevelTasks) {
	CHALLENGE_REQUIRE_DATABASE();
	Member& a = legionWithMembers(5);
	ChallengeTaskService& service = ChallengeTaskService::getInstance();
	db::execute("INSERT INTO challenge_tasks (task_id, quest_id, owner_id, owner_type, complete_count) VALUES (300, 17000, 500, 'LEGION', 2), "
		"(300, 17001, 500, 'LEGION', 0)");
	EXPECT_FALSE(service.canRaiseLegionLevel(*legion, a.player())) << "17001 is at 0 of 1";
	service.onChallengeQuestFinish(a.player(), 17001);
	EXPECT_TRUE(service.canRaiseLegionLevel(*legion, a.player()));
	legion->setLegionLevel(6);
	EXPECT_FALSE(service.canRaiseLegionLevel(*legion, a.player())) << "no loaded legion level task has min level 6";
}

TEST_F(ChallengeTaskServiceTest, AQuestOfALegionTaskWithoutALegionOrAnUnknownLegionIsIgnored) {
	CHALLENGE_REQUIRE_DATABASE();
	Member& b = addMember("Loner");
	ChallengeTaskService::getInstance().onChallengeQuestFinish(b.player(), 17000);
	EXPECT_EQ(challengeLists(b), 0);
	Member& a = legionWithMembers(6);
	ChallengeTaskService::getInstance().onChallengeQuestFinish(a.player(), 17000);
	EXPECT_EQ(a.player().getLegionMember()->getChallengeScore(), 0) << "the legion's tasks were never loaded";
	EXPECT_EQ(challengeLists(a), 0);
}


// CM_CHALLENGE_LIST (P5-15, CM_CHALLENGE_LIST.java:28-48): C_CHALLENGE_TASK, ClientPacketInfo.gen.inc:192 (opcode 232, IN_GAME). Run without the
// test database: ChallengeTasksDAO.load logs its SQLException and returns no task, so a legion of level 1 (no template of the fixture has
// min_level 1) gets the empty list. The town arm reaches TownService.getTownById: TownService's first use builds town 1001 (level 1) from the
// fixture's house land when the database has none (TownService.java:32-48; the Town import constructor needs the town spawns and Oriel's
// geo map, set up as TownLevelUpTest does), so 1001 gets its empty list and an unknown town is getLevel() on null, Java's
// NullPointerException. Each ctest case runs in its own process, so the singleton is built from this fixture.
constexpr int32_t CM_CHALLENGE_LIST_OPCODE = 232;

TEST(ChallengeListReadTest, TheBodyIsActionOwnerOwnerTypePlayerAndDate) {
	std::vector<uint8_t> body = PacketWriter().C(0xF2).D(0x01020304).C(0xE1).D(0x11121314).D(0x21222324).data;
	CM_CHALLENGE_LIST packet(CM_CHALLENGE_LIST_OPCODE, StateSet{AionConnection_State::IN_GAME});
	packet.setBuffer(commons::utils::ByteBuffer::wrap(body));
	ASSERT_TRUE(packet.read());
	using Access = CM_CHALLENGE_LISTTestAccess;
	EXPECT_EQ(Access::action(packet), 0xF2) << "readUC";
	EXPECT_EQ(Access::taskOwner(packet), 0x01020304);
	EXPECT_EQ(Access::ownerType(packet), 0xE1) << "readUC";
	EXPECT_EQ(Access::playerId(packet), 0x11121314);
	EXPECT_EQ(Access::dateSince(packet), 0x21222324);
	EXPECT_EQ(packet.getRemainingBytes(), 0);
}

class ChallengeListPacketTest : public ChallengeTaskServiceTest {
protected:
	void SetUp() override {
		// before TeamTest publishes Poeta alone: Poeta and the two housing worlds, whose geo maps the Town constructor updates
		dataholders::DataManager::WORLD_MAPS_DATA.publish(xml::bindString<dataholders::WorldMapsData>(context,
			R"(<world_maps><map id="210010000" cName="LF1" name="Poeta" name_id="1" water_level="16" death_level="0")"
			R"( world_type="ELYSEA" world_size="1024" flags="FLY GLIDE RECALL"/>)"
			R"(<map id="700010000" cName="Oriel" death_level="0" water_level="0" flags="BIND"/>)"
			R"(<map id="710010000" cName="Pernon" death_level="0" water_level="0" flags="BIND"/></world_maps>)"));
		ChallengeTaskServiceTest::SetUp();
		dataholders::DataManager::TOWN_SPAWNS_DATA.publish(xml::bindString<dataholders::TownSpawnsData>(context,
			R"(<town_spawns_data><spawn_map map_id="700010000"><town_spawn town_id="1001"><town_level level="1"/></town_spawn></spawn_map>)"
			R"(</town_spawns_data>)"));
		world::geo::GeoService::getInstance().init();
		// houses.xml's Oriel land 325001 with one address of town 1001; its manager npc's tribe GENERAL makes the town Elyos
		dataholders::DataManager::HOUSE_DATA.publish(xml::bindString<dataholders::HouseData>(context, R"(<house_lands>)"
			R"(<land id="325001" teleport_npc="810003" manager_npc="810017" sign_home="810007" sign_waiting="810006" sign_sale="810005" sign_nosale="810004">)"
			R"(<addresses><address id="10001" map="700010000" town="1001" x="696.159973" y="1999.969971" z="174.42577"/></addresses>)"
			R"(<buildings><building id="350000" default="true" type="PERSONAL_FIELD" size="HOUSE"/></buildings>)"
			R"(<sale level="50" gold_price="1000000" point_price="0"/><fee>20000000</fee><caps room="false" floor="false" emblemId="2" addon="true"/>)"
			R"(</land></house_lands>)"));
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(context,
			R"(<npc_templates><npc_template npc_id="810017" level="1" name="palace butler" name_id="461896" rank="DISCIPLINED" rating="NORMAL")"
			R"( tribe="GENERAL" type="HOUSING" ai="butler"><stats maxHp="2256"/></npc_template></npc_templates>)"));
	}

	void TearDown() override {
		if (member)
			member->player().setLegionMember(nullptr);
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::HOUSE_DATA.resetForTests();
		dataholders::DataManager::TOWN_SPAWNS_DATA.resetForTests();
		ChallengeTaskServiceTest::TearDown();
		dataholders::DataManager::WORLD_MAPS_DATA.resetForTests(); // after the map instance of TeamTest that reads the Poeta template
	}

	void run(Member& m, int32_t taskOwner, int32_t ownerType) {
		Driver<CM_CHALLENGE_LIST> packet(CM_CHALLENGE_LIST_OPCODE);
		packet.readAndRun(PacketWriter().C(0).D(taskOwner).C(ownerType).D(m.player().getObjectId()).D(0).data, m.client->get());
	}

	/** a legion of level 1 that only this process knows (no row), with the member in it */
	void joinLegion(Member& m, int32_t legionId) {
		ownLegion = Legion::create(legionId, "Listers");
		ownLegion->setLegionLevel(1);
		ownMember = LegionMember::create(m.player().getObjectId(), *ownLegion);
		m.player().setLegionMember(ownMember);
		member = &m;
	}

	runtime::Ref<Legion> ownLegion;
	runtime::Ref<LegionMember> ownMember;
	Member* member = nullptr;
};

TEST_F(ChallengeListPacketTest, ALegionListWithoutALegionIsAuditedAndNotSent) {
	Member& a = addMember("Nolegion");
	ConfigScope<bool> audit(configs::main::LoggingConfig::LOG_AUDIT, true);
	network::test::LogCapture capture({"AUDIT_LOG"});
	run(a, 701, 1);
	EXPECT_EQ(challengeLists(a), 0);
	EXPECT_TRUE(capture.contains("tried to receive legion challenge task without legion")) << capture.dump();
}

TEST_F(ChallengeListPacketTest, OwnerTypeOneListsTheLegionTasksOfThePacketsOwner) {
	Member& a = addMember("Lister");
	joinLegion(a, 702);
	network::test::LogCapture capture({"AUDIT_LOG"});
	run(a, 703, 1);
	// showTaskList(player, LEGION, taskOwner): SM_CHALLENGE_LIST(2, 703, LEGION, no task) - the owner id is the packet's, not the player's legion
	EXPECT_EQ(a.count(serverpackets::SM_CHALLENGE_LIST(2, 703, ChallengeType::LEGION, {})), 1);
	EXPECT_EQ(challengeLists(a), 1);
	EXPECT_FALSE(capture.contains("without legion"));
}

TEST_F(ChallengeListPacketTest, AnyOtherOwnerTypeListsTheTownTasksOfThePacketsOwner) {
	Member& a = addMember("Townlister");
	joinLegion(a, 704); // a legion too: the town arm must not take the legion path
	run(a, 1001, 0);
	// showTaskList(player, TOWN, taskOwner): town 1001 is level 1, so SM_CHALLENGE_LIST(2, 1001, TOWN, no task)
	EXPECT_EQ(a.count(serverpackets::SM_CHALLENGE_LIST(2, 1001, ChallengeType::TOWN, {})), 1);
	run(a, 1001, 2);
	EXPECT_EQ(a.count(serverpackets::SM_CHALLENGE_LIST(2, 1001, ChallengeType::TOWN, {})), 2) << "only 1 is the legion";
	EXPECT_EQ(challengeLists(a), 2);
	EXPECT_THROW(run(a, 705, 0), runtime::NullPointerException) << "TownService.getTownById(705) is null: getLevel() throws";
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::team
