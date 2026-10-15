// P5-10f (M5h F2b): ChallengeTaskService against ChallengeTaskService.java - the legion task list, a legion task's quests finished up to the
// completion (scores, the complete count stored, the offline member stored) and canRaiseLegionLevel - over the test database
// (ChallengeTestDatabase.h: run under gate_lock.py) and real Players on recording connections (tests/team/P5-10b/TeamTestSupport.h).

#include "../P5-10b/TeamTestSupport.h"
#include "ChallengeTestDatabase.h"

#include <string>

#include <gtest/gtest.h>

#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/dataholders/ChallengeData.bind.h"
#include "aion/gameserver/dataholders/ChallengeData.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/model/templates/challenge/ChallengeType.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CHALLENGE_LIST.h"
#include "aion/gameserver/services/ChallengeTaskService.h"
#include "aion/gameserver/services/LegionService.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::team {
namespace {

namespace db = ::aion::gameserver::team::challenge::test;
using model::team::legion::Legion;
using model::team::legion::LegionMember;
using model::templates::challenge::ChallengeType;
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

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::team
