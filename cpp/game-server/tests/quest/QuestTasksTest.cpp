// P5-06a, M5d E-07 (m5d-plan.md §7): questEngine/task - the check of a quest escort (FollowingNpcCheckTask.java), its four schedules
// (QuestTasks.java) and the destination checkers (task/checker/*.java), on the ManualClock of the handler-base fixture.
//
// The fixture is tests/quest_handlers/QuestFollowTestSupport.h (included by relative path, as this directory includes tests/cm_ak's support): the
// quester at (100, 100, 50) in Poeta's instance 1, the escort npc spawned 5 m from him, and an EscortHandler for quest 1111 registered for the
// reach and lost target events. A case schedules the check through QuestTasks and keeps it as the player's QUEST_FOLLOW task, which is what
// AbstractQuestHandler.defaultStartFollowEvent does after the npc starts to follow (AbstractQuestHandler.java:814, 826); the follow itself and
// the quest steps are tests/quest_handlers/AbstractQuestHandlerFollowTest.cpp.
//
// The check (FollowingNpcCheckTask.run, :30-43) fails the escort when the player or the npc is dead or they are 50 m or more apart
// (PositionUtil.isInRange: the same map and instance, and the squared 3D distance below 50 squared), and ends it when the checker says the npc
// has arrived (less than 20 m, same rule). Either way the task stops (:59-66): the player's QUEST_FOLLOW task is cancelled, the npc gets
// STOP_FOLLOW_ME and, unless its AI is "following", the task deletes it; only then does the quest event fire (onSuccess and onFail stop first,
// :49-57). The npc it watches and stops is the checker's follower, which need not be the env's object. Java has no return after onFail, so one
// run can fail twice or fail and then succeed; the java-bug cases keep that.

#include "../quest_handlers/QuestFollowTestSupport.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/dataholders/WorldMapsData.bind.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/questEngine/task/FollowingNpcCheckTask.h"
#include "aion/gameserver/questEngine/task/QuestTasks.h"
#include "aion/gameserver/questEngine/task/checker/CoordinateDestinationChecker.h"
#include "aion/gameserver/questEngine/task/checker/DestinationChecker.h"
#include "aion/gameserver/questEngine/task/checker/TargetDestinationChecker.h"
#include "aion/gameserver/questEngine/task/checker/ZoneChecker.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/world/zone/ZoneName.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::questEngine::handlers::test {
namespace {

using gameserver::model::TaskId;
using gameserver::model::gameobjects::Npc;
using gameserver::model::gameobjects::VisibleObject;
using runtime::FutureRef;
using task::QuestTasks;
using world::zone::ZoneName;

constexpr int32_t ESCORT_QUEST = 1111;

/**
 * world_maps.xml's Sanctum row (:3) before the fixture's Poeta row (ItemPacketTestSupport.h's, character for character), both reduced to the
 * attributes a map template needs: the shipped order, in which SpawnsData.getFirstSpawnByNpcId's search of the other maps meets Sanctum first
 */
constexpr const char* SANCTUM_AND_POETA_XML =
	R"(<world_maps><map id="110010000" cName="LC1" name="Sanctum" name_id="400437" water_level="16" death_level="400")"
	R"( world_type="ELYSEA" world_size="3072" flags="RECALL GLIDE RIDE PVP DUEL_SAME_RACE"/>)"
	R"(<map id="210010000" cName="LF1" name="Poeta" name_id="1" water_level="16" death_level="0")"
	R"( world_type="ELYSEA" world_size="1024" flags="FLY GLIDE RECALL"/></world_maps>)";

/** Mires on two maps: a fabricated Sanctum row (mires spawns only in Poeta) and the shipped Poeta row (QuestFollowTestSupport.h) */
constexpr std::string_view TWO_MAP_SPAWNS_XML = R"xml(<spawns>
	<spawn_map map_id="110010000">
		<spawn npc_id="203057" respawn_time="295">
			<spot x="1500" y="1500" z="570" h="0"/>
		</spawn>
	</spawn_map>
	<spawn_map map_id="210010000">
		<spawn npc_id="203057" respawn_time="295">
			<spot x="1141" y="1032" z="128.875" h="3"/>
		</spawn>
	</spawn_map>
</spawns>)xml";

class QuestTasksTest : public QuestFollowTest {
protected:
	void SetUp() override {
		QuestFollowTest::SetUp();
		escort(ESCORT_QUEST);
		npc = &spawnNpc(MIRES, 105.0f, 100.0f, 50.0f);
		// quest id 0: which quest an event is for is the engine's to say (onNpcReachTarget/onNpcLostTarget set each registered quest's id on the
		// env before its handler's event, QuestEngine.java:295-321)
		env = envOf(*me, 0, gameserver::model::DialogAction::SETPRO1, Ptr<VisibleObject>(*npc));
	}

	void TearDown() override {
		env = nullptr;
		npc = nullptr;
		if (fixtureWorldMaps != nullptr) {
			// the holder the executable published (never freed: HolderRef leaks a reset holder), published again as it was
			dataholders::DataManager::WORLD_MAPS_DATA.resetForTests();
			dataholders::DataManager::WORLD_MAPS_DATA.publish(
				std::unique_ptr<dataholders::WorldMapsData>(const_cast<dataholders::WorldMapsData*>(fixtureWorldMaps)));
			fixtureWorldMaps = nullptr;
		}
		QuestFollowTest::TearDown();
	}

	/** Sanctum and Poeta as the world maps, and mires spawned on both (the TearDown restores the executable's world maps) */
	void publishTwoMaps() {
		fixtureWorldMaps = dataholders::DataManager::WORLD_MAPS_DATA.get();
		dataholders::DataManager::WORLD_MAPS_DATA.resetForTests();
		dataholders::DataManager::WORLD_MAPS_DATA.publish(
			xml::bindString<dataholders::WorldMapsData>(contexts.emplace_back(), SANCTUM_AND_POETA_XML));
		dataholders::DataManager::SPAWNS_DATA.publish(xml::bindString<dataholders::SpawnsData>(contexts.emplace_back(), TWO_MAP_SPAWNS_XML));
		spawnsPublished = true;
	}

	/** The schedule kept as the player's QUEST_FOLLOW task, as defaultStartFollowEvent keeps it */
	FutureRef follow(FutureRef task) {
		player().getController().addTask(TaskId::QUEST_FOLLOW, task);
		return task;
	}

	/** A check that goes to (300, 100, 50): 195 m from the npc, so it never arrives unless a case moves the npc there */
	FutureRef followFar() { return follow(QuestTasks::newFollowingToTargetCheckTask(*env, *npc, 300.0f, 100.0f, 50.0f)); }

	int32_t reached() const {
		int32_t n = 0;
		for (const TargetEvent& e : targetEvents)
			n += e.reached ? 1 : 0;
		return n;
	}

	int32_t lost() const { return static_cast<int32_t>(targetEvents.size()) - reached(); }

	/** For each event: the player still had his QUEST_FOLLOW task when it came */
	std::vector<bool> followTasksAtEvents() const {
		std::vector<bool> result;
		for (const TargetEvent& e : targetEvents)
			result.push_back(e.followTask);
		return result;
	}

	/** Every event came with the env the check was scheduled with, its quest id set by QuestEngine.onNpcReachTarget/onNpcLostTarget */
	void expectEventsOfTheEscortsEnv() const {
		for (const TargetEvent& e : targetEvents) {
			EXPECT_EQ(e.envQuestId, ESCORT_QUEST);
			EXPECT_EQ(e.env, env.get()) << "the env the check was scheduled with";
		}
	}

	/** The task stopped: the QUEST_FOLLOW task is gone and cancelled, and five more seconds run no check */
	void expectStopped(const FutureRef& task) {
		EXPECT_FALSE(hasFollowTask());
		EXPECT_TRUE(task->isCancelled());
		size_t events = targetEvents.size();
		advance(5000);
		EXPECT_EQ(targetEvents.size(), events) << "no check after the stop";
	}

	Npc* npc = nullptr;
	Ref<QuestEnv> env;
	const dataholders::WorldMapsData* fixtureWorldMaps = nullptr;
};

// QuestTasks.newFollowingToTargetCheckTask (QuestTasks.java:62-65, and every overload): scheduleAtFixedRate(task, 1000, 1000) - the first check
// a second after the schedule, then one a second; an arrival ends the escort (onNpcReachTarget with the env) and stops the task, and the task
// deletes an npc whose AI is not "following" (this one keeps its template's AI, a DummyNpcAI: "noname")
TEST_F(QuestTasksTest, TheCheckRunsASecondAfterTheScheduleThenEverySecondUntilTheNpcArrives) {
	std::vector<bool> npcSpawnedAtEvent;
	atEvent = [&] { npcSpawnedAtEvent.push_back(npc->isSpawned()); };
	FutureRef task = followFar();
	advance(1000);
	EXPECT_TRUE(targetEvents.empty()) << "the first check: 195 m to go, 5 m from the player";
	moveTo(*npc, 285.0f, 100.0f, 50.0f);
	moveTo(player(), 280.0f, 100.0f, 50.0f);
	advance(999);
	EXPECT_TRUE(targetEvents.empty()) << "the second check is due at 2000 ms";
	EXPECT_TRUE(npc->isSpawned());
	advance(1);
	ASSERT_EQ(targetEvents.size(), 1u);
	EXPECT_TRUE(targetEvents[0].reached);
	expectEventsOfTheEscortsEnv();
	EXPECT_FALSE(npc->isSpawned()) << "stopFollowing deletes an npc whose AI is not \"following\"";
	// onSuccess (:49-52) stops before onNpcReachTarget: the handler finds the QUEST_FOLLOW task gone and the npc deleted
	EXPECT_EQ(followTasksAtEvents(), std::vector<bool>{false}) << "the task was cancelled before the event";
	EXPECT_EQ(npcSpawnedAtEvent, std::vector<bool>{false}) << "the npc was deleted before the event";
	expectStopped(task);

	// the first check of a new schedule comes a second after it, not at the old period's next beat
	atEvent = nullptr;
	targetEvents.clear();
	Npc& second = spawnNpc(MIRES, 285.0f, 100.0f, 50.0f);
	Ref<QuestEnv> secondEnv = envOf(*me, ESCORT_QUEST, gameserver::model::DialogAction::SETPRO1, Ptr<VisibleObject>(second));
	advance(500);
	FutureRef again = follow(QuestTasks::newFollowingToTargetCheckTask(*secondEnv, second, 300.0f, 100.0f, 50.0f));
	advance(999);
	EXPECT_TRUE(targetEvents.empty());
	advance(1);
	EXPECT_EQ(reached(), 1) << "15 m from the point: arrived at the first check";
	EXPECT_TRUE(again->isCancelled());
}

// stopFollowing (FollowingNpcCheckTask.java:59-66): the npc gets STOP_FOLLOW_ME from the player, and an npc whose AI is "following" is not
// deleted by the task (FollowingNpcAI's own stop deletes it when it was following; this one never started, so it stays)
TEST_F(QuestTasksTest, AFollowingNpcGetsStopFollowMeAndTheTaskLeavesItToItsAi) {
	FollowingProbeAI& ai = followingAi(*npc);
	std::vector<size_t> aiEventsAtEvent;
	atEvent = [&] { aiEventsAtEvent.push_back(ai.events.size()); };
	FutureRef task = follow(QuestTasks::newFollowingToTargetCheckTask(*env, *npc, 110.0f, 100.0f, 50.0f));
	advance(1000);
	EXPECT_EQ(reached(), 1);
	EXPECT_EQ(lost(), 0);
	EXPECT_EQ(aiEventsAtEvent, std::vector<size_t>{1}) << "STOP_FOLLOW_ME came before onNpcReachTarget (:49-52)";
	ASSERT_EQ(ai.events.size(), 1u);
	EXPECT_EQ(ai.events[0].type, ai::event::AIEventType::STOP_FOLLOW_ME);
	EXPECT_EQ(ai.events[0].creatureObjectId, player().getObjectId());
	EXPECT_TRUE(npc->isSpawned()) << "the task deletes only an npc whose AI is not \"following\"";
	expectStopped(task);
}

// run (:33-35): a dead npc fails the escort (onNpcLostTarget) at the next check, and the task stops and deletes it
TEST_F(QuestTasksTest, TheEscortFailsWhenTheNpcDies) {
	FutureRef task = followFar();
	advance(1000);
	EXPECT_TRUE(targetEvents.empty());
	npc->getController().die();
	advance(1000);
	EXPECT_EQ(lost(), 1);
	EXPECT_EQ(reached(), 0);
	expectEventsOfTheEscortsEnv();
	// onFail (:54-57) stops before onNpcLostTarget: the handler finds the QUEST_FOLLOW task gone
	EXPECT_EQ(followTasksAtEvents(), std::vector<bool>{false}) << "the task was cancelled before the event";
	expectStopped(task);
}

// The npc the check watches and stops is the checker's follower (:32, :61), not the env's object: four of the six Java handlers that call
// QuestTasks directly pass a follower they have just spawned, while the env's object is the npc the player talked to
// (beluslan/_24053TheMaulingoftheMau.java:132-134, beluslan/_2634TheDraupnirRedemption.java:67-70, morheim/_2333, morheim/_2394). Here the
// talked-to npc is the fixture's, 5 m from the player, and the follower walks 55 m away: the escort fails on the follower's distance, the
// follower gets STOP_FOLLOW_ME and is deleted, and the talked-to npc is left alone. Both AIs record their events and are not "following"
TEST_F(QuestTasksTest, TheCheckWatchesTheFollowerNotTheNpcThePlayerTalkedTo) {
	FollowingProbeAI& talkedToAi = followingAi(*npc, false);
	Npc& follower = spawnNpc(MIRES, 95.0f, 100.0f, 50.0f);
	FollowingProbeAI& followerAi = followingAi(follower, false);
	FutureRef task = follow(QuestTasks::newFollowingToTargetCheckTask(*env, follower, 300.0f, 100.0f, 50.0f));
	moveTo(follower, 45.0f, 100.0f, 50.0f);
	advance(1000);
	EXPECT_EQ(lost(), 1) << "the follower is 55 m from the player";
	EXPECT_FALSE(follower.isSpawned()) << "the task deleted the follower";
	EXPECT_TRUE(npc->isSpawned()) << "the npc the player talked to stays";
	EXPECT_TRUE(talkedToAi.events.empty());
	ASSERT_EQ(followerAi.events.size(), 1u);
	EXPECT_EQ(followerAi.events[0].type, ai::event::AIEventType::STOP_FOLLOW_ME);
	expectStopped(task);
}

// run (:33-35): a dead player fails the escort
TEST_F(QuestTasksTest, TheEscortFailsWhenThePlayerDies) {
	publishKillBounties();
	FutureRef task = followFar();
	player().getController().die();
	advance(1000);
	EXPECT_EQ(lost(), 1);
	EXPECT_EQ(reached(), 0);
	expectEventsOfTheEscortsEnv();
	expectStopped(task);
}

// run (:36-38): PositionUtil.isInRange(player, npc, 50) - 49.99 m apart the escort goes on, 50 m apart it fails, whichever of the two moved
TEST_F(QuestTasksTest, TheEscortFailsWhenThePlayerAndTheNpcAre50MetresApart) {
	FutureRef task = followFar();
	moveTo(*npc, 149.99f, 100.0f, 50.0f);
	advance(1000);
	EXPECT_TRUE(targetEvents.empty()) << "49.99 m";
	moveTo(*npc, 105.0f, 100.0f, 50.0f);
	moveTo(player(), 105.0f, 100.0f, 100.0f); // straight up: the distance is 3D
	advance(1000);
	EXPECT_EQ(lost(), 1) << "50 m";
	EXPECT_EQ(reached(), 0);
	expectEventsOfTheEscortsEnv();
	expectStopped(task);
}

// run (:36-38): a player who left the npc's map (or instance) is out of range whatever the coordinates (isInRange compares the world and the
// instance first)
TEST_F(QuestTasksTest, TheEscortFailsWhenThePlayerLeavesTheMap) {
	FutureRef task = followFar();
	Ref<world::WorldPosition> home(*player().getPosition());
	player().setPosition(world::WorldPosition::create(220010000, 100.0f, 100.0f, 50.0f, int8_t{0}));
	advance(1000);
	player().setPosition(home);
	EXPECT_EQ(lost(), 1);
	EXPECT_EQ(reached(), 0);
	expectEventsOfTheEscortsEnv();
	expectStopped(task);
}

// Leaving the world (CreatureController.onDelete: cancelAllTasks, which a logout's controller delete runs) cancels the check with no event: the
// escort neither fails nor ends, even with the npc at its destination, and the npc is not deleted
TEST_F(QuestTasksTest, CancellingThePlayersTasksStopsTheCheckWithoutAnEvent) {
	FutureRef task = followFar();
	advance(1000);
	player().getController().cancelAllTasks();
	moveTo(*npc, 300.0f, 100.0f, 50.0f);
	advance(5000);
	EXPECT_TRUE(targetEvents.empty());
	EXPECT_TRUE(npc->isSpawned());
}

// java-bug kept (FollowingNpcCheckTask.java:33-38): no return after onFail - a dead npc out of range fails twice in one check (onNpcLostTarget
// twice, the npc deleted once: the second delete finds it gone)
TEST_F(QuestTasksTest, JavaBugADeadNpcOutOfRangeFailsTwiceInOneCheck) {
	FutureRef task = followFar();
	npc->getController().die();
	moveTo(player(), 160.0f, 100.0f, 50.0f);
	advance(1000);
	EXPECT_EQ(lost(), 2);
	EXPECT_EQ(reached(), 0);
	expectEventsOfTheEscortsEnv();
	expectStopped(task);
}

// java-bug kept (:33-42): a failed escort whose npc stands at its destination also arrives - onNpcLostTarget, then onNpcReachTarget
TEST_F(QuestTasksTest, JavaBugADeadNpcAtItsDestinationFailsAndThenArrives) {
	FutureRef task = follow(QuestTasks::newFollowingToTargetCheckTask(*env, *npc, 110.0f, 100.0f, 50.0f));
	npc->getController().die();
	advance(1000);
	ASSERT_EQ(targetEvents.size(), 2u);
	EXPECT_FALSE(targetEvents[0].reached) << "the failure first";
	EXPECT_TRUE(targetEvents[1].reached);
	expectEventsOfTheEscortsEnv();
	expectStopped(task);
}

// newFollowingToTargetCheckTask(env, npc, target) (QuestTasks.java:29-31): TargetDestinationChecker - arrived below 20 m from the target npc
TEST_F(QuestTasksTest, TheNpcOverloadArrivesBelow20MetresFromTheTargetNpc) {
	Npc& target = spawnNpc(ANMURNERK, 130.0f, 100.0f, 50.0f);
	FutureRef task = follow(QuestTasks::newFollowingToTargetCheckTask(*env, *npc, target));
	moveTo(*npc, 110.0f, 100.0f, 50.0f);
	advance(1000);
	EXPECT_TRUE(targetEvents.empty()) << "20 m";
	moveTo(*npc, 110.01f, 100.0f, 50.0f);
	advance(1000);
	EXPECT_EQ(reached(), 1) << "19.99 m";
	EXPECT_EQ(lost(), 0);
	EXPECT_TRUE(target.isSpawned()) << "only the follower is deleted";
	expectStopped(task);
}

// The npc overload checks the target where it is at each check (TargetDestinationChecker holds the creature, not its position at the schedule):
// a target that comes within 20 m ends the escort, and a target on another map is never reached, however close its coordinates
TEST_F(QuestTasksTest, TheNpcOverloadChecksTheTargetWhereItIsAtEachCheck) {
	Npc& target = spawnNpc(ANMURNERK, 105.0f, 140.0f, 50.0f); // 40 m from the npc at the schedule
	FutureRef task = follow(QuestTasks::newFollowingToTargetCheckTask(*env, *npc, target));
	moveTo(target, 105.0f, 115.0f, 50.0f);
	advance(1000);
	EXPECT_EQ(reached(), 1) << "the target came within 15 m";

	targetEvents.clear();
	Npc& second = spawnNpc(MIRES, 95.0f, 100.0f, 50.0f);
	Ref<world::WorldPosition> targetHome(*target.getPosition());
	target.setPosition(world::WorldPosition::create(220010000, 95.0f, 110.0f, 50.0f, int8_t{0}));
	FutureRef onAnotherMap = follow(QuestTasks::newFollowingToTargetCheckTask(*env, second, target));
	advance(3000);
	target.setPosition(targetHome);
	EXPECT_TRUE(targetEvents.empty()) << "the target on another map, 10 m from the follower's coordinates";
	onAnotherMap->cancel(false);
}

// newFollowingToTargetCheckTask(env, npc, npcTargetId) (:41-50): the first spawn spot of the npc id on the npc's map
// (SpawnsData.getFirstSpawnByNpcId), checked as a point - mires's spot (1141, 1032, 128.875)
TEST_F(QuestTasksTest, TheNpcIdOverloadHeadsForTheFirstSpawnSpotOfThatNpc) {
	publishFollowSpawns();
	FutureRef task = follow(QuestTasks::newFollowingToTargetCheckTask(*env, *npc, MIRES));
	moveTo(player(), 1141.0f, 1070.0f, 128.875f);
	moveTo(*npc, 1141.0f, 1052.0f, 128.875f);
	advance(1000);
	EXPECT_TRUE(targetEvents.empty()) << "20 m from the spot";
	moveTo(*npc, 1141.0f, 1051.5f, 128.875f);
	advance(1000);
	EXPECT_EQ(reached(), 1) << "19.5 m";
	EXPECT_EQ(lost(), 0);
	expectStopped(task);
}

// :42: the spot on the follower's own map (getFirstSpawnByNpcId(npc.getWorldId(), ...)); only a map without one sends SpawnsData's search to
// the other maps, in world_maps.xml's order (SpawnsData.java:418-433). Here mires stands on Sanctum too, and Sanctum comes first: the follower
// in Poeta still heads for Poeta's spot (1141, 1032, 128.875), not Sanctum's (1500, 1500, 570)
TEST_F(QuestTasksTest, TheNpcIdOverloadPrefersTheSpotOnTheFollowersMap) {
	publishTwoMaps();
	FutureRef task = follow(QuestTasks::newFollowingToTargetCheckTask(*env, *npc, MIRES));
	moveTo(player(), 1141.0f, 1045.0f, 128.875f);
	moveTo(*npc, 1141.0f, 1040.0f, 128.875f);
	advance(1000);
	EXPECT_EQ(reached(), 1) << "8 m from Poeta's spot";
}

// :43-45: an npc id with no spawn on any map throws IllegalArgumentException, and nothing is scheduled
TEST_F(QuestTasksTest, TheNpcIdOverloadThrowsForAnNpcWithoutASpawn) {
	publishFollowSpawns();
	size_t pending = executor->pendingTaskCount();
	try {
		static_cast<void>(QuestTasks::newFollowingToTargetCheckTask(*env, *npc, 299999));
		ADD_FAILURE() << "no exception";
	} catch (const runtime::IllegalArgumentException& ex) {
		EXPECT_NE(std::string(ex.what()).find("Supplied npc doesn't exist: 299999"), std::string::npos) << ex.what();
	}
	EXPECT_EQ(executor->pendingTaskCount(), pending);
	advance(5000);
	EXPECT_TRUE(targetEvents.empty());
}

// newFollowingToTargetCheckTask(env, npc, x, y, z) (:62-65) keeps the point's height: a point 25 m above the npc is not reached, and it is once
// the npc stands 19 m below it (the escorts' points lie at other heights than their followers: _1385's z 355.61, _11040's 574.37)
TEST_F(QuestTasksTest, ThePointOverloadKeepsTheHeightOfThePoint) {
	FutureRef task = follow(QuestTasks::newFollowingToTargetCheckTask(*env, *npc, 105.0f, 100.0f, 75.0f));
	advance(1000);
	EXPECT_TRUE(targetEvents.empty()) << "25 m below the point";
	moveTo(*npc, 105.0f, 100.0f, 56.0f);
	advance(1000);
	EXPECT_EQ(reached(), 1) << "19 m below it";
}

// newFollowingToTargetCheckTask(env, npc, zoneName) (:67-69): ZoneChecker - arrived inside the zone. Every map region holds the zone of the whole
// map, named after the map id (ZoneService.getZoneInstancesByWorldId); a zone the npc's region does not hold is never reached
TEST_F(QuestTasksTest, TheZoneOverloadArrivesInsideTheZone) {
	const ZoneName* elsewhere = ZoneName::createOrGet("HALABANA_HOT_SPRINGS_220020000"); // _2394ADyingWish.java:70
	FutureRef never = QuestTasks::newFollowingToTargetCheckTask(*env, *npc, elsewhere);
	advance(3000);
	EXPECT_TRUE(targetEvents.empty());
	never->cancel(false);

	FutureRef task = follow(QuestTasks::newFollowingToTargetCheckTask(*env, *npc, ZoneName::get("210010000")));
	advance(1000);
	EXPECT_EQ(reached(), 1);
	EXPECT_EQ(lost(), 0);
	expectStopped(task);
}

// The checkers themselves (task/checker/*.java): getFollower; the point and the target compare strictly below 20 m in 3D, the target only on
// the follower's map and instance; the zone asks Creature.isInsideZone, which a despawned follower never is
TEST_F(QuestTasksTest, TheCheckersCompareBelow20MetresInThreeDimensionsAndTheZoneOfASpawnedFollower) {
	Ref<task::checker::CoordinateDestinationChecker> point = task::checker::CoordinateDestinationChecker::create(*npc, 105.0f, 100.0f, 70.0f);
	EXPECT_FALSE(point->check()) << "20 m straight up";
	moveTo(*npc, 105.0f, 100.0f, 50.01f);
	EXPECT_TRUE(point->check()) << "19.99 m";

	Npc& target = spawnNpc(ANMURNERK, 105.0f, 119.0f, 50.0f);
	Ref<task::checker::TargetDestinationChecker> toTarget = task::checker::TargetDestinationChecker::create(*npc, target);
	EXPECT_EQ(toTarget->getFollower().get(), static_cast<gameserver::model::gameobjects::Creature*>(npc)) << "the follower, not the target";
	EXPECT_TRUE(toTarget->check()) << "19 m";
	moveTo(target, 105.0f, 120.01f, 50.0f);
	EXPECT_FALSE(toTarget->check()) << "20 m";
	Ref<world::WorldPosition> targetHome(*target.getPosition());
	target.setPosition(world::WorldPosition::create(220010000, 105.0f, 100.0f, 50.0f, int8_t{0}));
	EXPECT_FALSE(toTarget->check()) << "another map";
	target.setPosition(targetHome);

	Ref<task::checker::ZoneChecker> inMap = task::checker::ZoneChecker::create(*npc, ZoneName::get("210010000"));
	EXPECT_TRUE(inMap->check());
	EXPECT_FALSE(task::checker::ZoneChecker::create(*npc, ZoneName::createOrGet("HALABANA_HOT_SPRINGS_220020000"))->check());
	npc->getController().delete_();
	EXPECT_FALSE(inMap->check()) << "despawned";
}

} // namespace
} // namespace aion::gameserver::questEngine::handlers::test
