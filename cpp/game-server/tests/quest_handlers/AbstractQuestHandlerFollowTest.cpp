// P5-06b, M5d E-07 (m5d-plan.md §7): the follow family of AbstractQuestHandler - defaultStartFollowEvent x2 and, through the escort's check
// task, defaultFollowEndEvent x2 (AbstractQuestHandler.java:806-851) - on the escort fixture (QuestFollowTestSupport.h).
//
// defaultStartFollowEvent answers false unless the env's object is an Npc; else the npc overload makes the follower PEACE, the point overload
// sends the player SM_NPC_INFO of the follower instead; both send the follower's AI FOLLOW_ME from the player and add the check
// (QuestTasks.newFollowingToTargetCheckTask) as the player's QUEST_FOLLOW task - replacing, and so cancelling, one already there - and then
// answer true for step 0 -> 0 or defaultCloseDialog(env, step, nextStep) otherwise (the step, DIALOG_FINISH to the npc's AI, the closed
// window). The escort cases run a fabricated handler the way the Java escorts do (_1149MissingPoppy.java: start 0 -> 1, reach: 1 -> REWARD with
// movie 12, lost: 1 -> 0) on quest 1111, a QUEST of the fixture; the check task itself is tests/quest/QuestTasksTest.cpp.
//
// Packets: the follower stands spawned 5 m from the quester, whose TestKnownList sees no npc (QuestHandlerTestSupport.h), so what the npc
// broadcasts reaches no client here, and the cases' exact packet lists hold only for what defaultStartFollowEvent and the quest steps send to the
// quester himself. In Java a quester who sees the follower also gets its broadcasts: SM_CUSTOM_SETTINGS with PEACE from Npc.overrideNpcType
// (Npc.java:272-276), the two SM_EMOTION of EmoteManager.emoteStartFollowing (EmoteManager.java:50-54) and, when the task stops, its despawn.
// SM_NPC_INFO is serialized when it is sent, before FOLLOW_ME, where Java writes it later (it reads the npc's target and state at write time,
// SM_NPC_INFO.java:67, 126): the project's eager serialization (runtime-architecture.md §8.1), not a deviation of this family.

#include "QuestFollowTestSupport.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "aion/gameserver/model/CreatureType.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/network/aion/serverpackets/SM_NPC_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::questEngine::handlers::test {
namespace {

using ai::event::AIEventType;
using gameserver::model::CreatureType;
using gameserver::model::gameobjects::Npc;
using gameserver::model::gameobjects::VisibleObject;
using network::aion::serverpackets::SM_NPC_INFO;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

constexpr int32_t ESCORT_QUEST = 1111;

class AbstractQuestHandlerFollowTest : public QuestFollowTest {
protected:
	void SetUp() override {
		QuestFollowTest::SetUp();
		handler = &escort(ESCORT_QUEST, true);
		follower = &spawnNpc(STRIPED_KERUB, 105.0f, 100.0f, 50.0f);
		ai = &followingAi(*follower);
		env = talkTo(*follower);
	}

	void TearDown() override {
		env = nullptr;
		ai = nullptr;
		follower = nullptr;
		handler = nullptr;
		QuestFollowTest::TearDown();
	}

	/** The env of a dialog with the npc (SETPRO1: the escort's start, _1149MissingPoppy.java:63-64) */
	Ref<QuestEnv> talkTo(VisibleObject& object) {
		return envOf(*me, ESCORT_QUEST, gameserver::model::DialogAction::SETPRO1, Ptr<VisibleObject>(object));
	}

	int32_t var() { return player().getQuestStateList()->getQuestState(ESCORT_QUEST)->getQuestVars()->getQuestVars(); }

	void expectFollowing() {
		ASSERT_EQ(ai->events.size(), 1u);
		EXPECT_EQ(ai->events[0].type, AIEventType::FOLLOW_ME);
		EXPECT_EQ(ai->events[0].creatureObjectId, player().getObjectId());
		EXPECT_EQ(ai->getState(), ai::AIState::FOLLOWING) << "FollowEventHandler.follow";
		EXPECT_EQ(follower->getTarget().get(), static_cast<VisibleObject*>(&player()));
		EXPECT_TRUE(hasFollowTask());
	}

	EscortHandler* handler = nullptr;
	Npc* follower = nullptr;
	FollowingProbeAI* ai = nullptr;
	Ref<QuestEnv> env;
};

// defaultStartFollowEvent(env, follower, targetNpcId, 0, 0) (:807-816): the follower turns PEACE, follows the player and the check runs as his
// QUEST_FOLLOW task; step 0 -> 0 answers true before any quest state is read, so defaultStartFollowEvent itself sends the quester nothing (the
// npc's broadcasts do not reach him in this fixture, see the header)
TEST_F(AbstractQuestHandlerFollowTest, StartFollowToAnNpcMakesTheFollowerPeacefulAndFollowThePlayer) {
	publishFollowSpawns();
	me->clearSent();
	EXPECT_TRUE(handler->defaultStartFollowEvent(*env, *follower, MIRES, 0, 0)) << "no quest state";
	EXPECT_EQ(follower->getType(player()), CreatureType::PEACE) << "a monster made peaceful";
	expectFollowing();
	EXPECT_TRUE(me->sent().empty());
}

// :815 with a step: defaultCloseDialog(env, step, nextStep) - the step moves (SM_QUEST_ACTION) and the window closes (SM_DIALOG_WINDOW page 0),
// after the npc follows; a step the quest is not at answers false, but the npc follows and the check runs all the same
TEST_F(AbstractQuestHandlerFollowTest, StartFollowWithAStepMovesTheStepAndClosesTheWindow) {
	publishFollowSpawns();
	hold(*me, ESCORT_QUEST, QuestStatus::START);
	me->clearSent();
	EXPECT_TRUE(handler->defaultStartFollowEvent(*env, *follower, MIRES, 0, 1));
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(ESCORT_QUEST, START, 1), dialogWindow(follower->getObjectId(), 0, 0)}));
	EXPECT_EQ(var(), 1);
	expectFollowing();

	Npc& other = spawnNpc(STRIPED_KERUB, 95.0f, 100.0f, 50.0f);
	FollowingProbeAI& otherAi = followingAi(other);
	me->clearSent();
	EXPECT_FALSE(handler->defaultStartFollowEvent(*talkTo(other), other, MIRES, 0, 1)) << "the quest is at step 1";
	EXPECT_TRUE(me->sent().empty());
	EXPECT_EQ(otherAi.getState(), ai::AIState::FOLLOWING) << "the follow starts before the step is checked";
	EXPECT_EQ(other.getType(player()), CreatureType::PEACE);
	EXPECT_TRUE(hasFollowTask());
}

// :809-811, :821-823: only a talk with an npc starts an escort - no env object, or the player himself, answers false and changes nothing
TEST_F(AbstractQuestHandlerFollowTest, StartFollowNeedsAnNpcAsTheEnvsObject) {
	publishFollowSpawns();
	hold(*me, ESCORT_QUEST, QuestStatus::START);
	me->clearSent();
	Ref<QuestEnv> noObject = envOf(*me, ESCORT_QUEST, gameserver::model::DialogAction::SETPRO1);
	EXPECT_FALSE(handler->defaultStartFollowEvent(*noObject, *follower, MIRES, 0, 1));
	EXPECT_FALSE(handler->defaultStartFollowEvent(*noObject, *follower, 130.0f, 100.0f, 50.0f, 0, 1));
	Ref<QuestEnv> self = talkTo(player());
	EXPECT_FALSE(handler->defaultStartFollowEvent(*self, *follower, MIRES, 0, 0));
	EXPECT_FALSE(handler->defaultStartFollowEvent(*self, *follower, 130.0f, 100.0f, 50.0f, 0, 0));
	EXPECT_TRUE(ai->events.empty());
	EXPECT_NE(follower->getType(player()), CreatureType::PEACE);
	EXPECT_FALSE(hasFollowTask());
	EXPECT_TRUE(me->sent().empty());
	EXPECT_EQ(var(), 0);
}

// defaultStartFollowEvent(env, follower, x, y, z, step, nextStep) (:819-832): SM_NPC_INFO of the follower to the player first, and no type
// change; then as the npc overload (0 -> 0: nothing more to the quester; with a step: the step and the closed window)
TEST_F(AbstractQuestHandlerFollowTest, StartFollowToAPointShowsTheFollowerAndKeepsItsType) {
	std::vector<uint8_t> npcInfo = me->serializedFor(SM_NPC_INFO(*follower, player()));
	me->clearSent();
	EXPECT_TRUE(handler->defaultStartFollowEvent(*env, *follower, 130.0f, 100.0f, 50.0f, 0, 0));
	EXPECT_EQ(me->sent(), cp::exactly({npcInfo}));
	EXPECT_NE(follower->getType(player()), CreatureType::PEACE) << "only the npc overload overrides the type";
	expectFollowing();

	hold(*me, ESCORT_QUEST, QuestStatus::START);
	Npc& other = spawnNpc(STRIPED_KERUB, 95.0f, 100.0f, 50.0f);
	followingAi(other);
	std::vector<uint8_t> otherInfo = me->serializedFor(SM_NPC_INFO(other, player()));
	me->clearSent();
	EXPECT_TRUE(handler->defaultStartFollowEvent(*talkTo(other), other, 130.0f, 100.0f, 50.0f, 0, 1));
	EXPECT_EQ(me->sent(), cp::exactly({otherInfo, questUpdate(ESCORT_QUEST, START, 1), dialogWindow(other.getObjectId(), 0, 0)}));
}

// :815, :827-831 with a later step: 7 of the 16 Java calls start at 1 -> 2 (heiron/_1562, _1614, _1693, altgard/_2284, theobomos/_3050) or
// 3 -> 4 (reshanta/_14042, _24042), and defaultCloseDialog(env, step, nextStep) moves that step and closes the window, through either overload
TEST_F(AbstractQuestHandlerFollowTest, StartFollowFromALaterStepMovesThatStep) {
	publishFollowSpawns();
	Ref<QuestState> qs = hold(*me, ESCORT_QUEST, QuestStatus::START, 1);
	me->clearSent();
	EXPECT_TRUE(handler->defaultStartFollowEvent(*env, *follower, MIRES, 1, 2));
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(ESCORT_QUEST, START, 2), dialogWindow(follower->getObjectId(), 0, 0)}));
	EXPECT_EQ(var(), 2);

	qs->setQuestVar(1);
	std::vector<uint8_t> npcInfo = me->serializedFor(SM_NPC_INFO(*follower, player()));
	me->clearSent();
	EXPECT_TRUE(handler->defaultStartFollowEvent(*env, *follower, 130.0f, 100.0f, 50.0f, 1, 2));
	EXPECT_EQ(me->sent(), cp::exactly({npcInfo, questUpdate(ESCORT_QUEST, START, 2), dialogWindow(follower->getObjectId(), 0, 0)}));
	EXPECT_EQ(var(), 2);
}

// :815, :827-831: only 0 -> 0 skips defaultCloseDialog. 1 -> 1 keeps the step and closes the window; 1 -> 0 takes the step back, with the give-up
// message changeQuestStep sends for a rollback in START (:310-313), and closes it. No Java escort starts so, but the helper is public
TEST_F(AbstractQuestHandlerFollowTest, StartFollowToTheSameOrAnEarlierStepStillClosesTheWindow) {
	publishFollowSpawns();
	Ref<QuestState> qs = hold(*me, ESCORT_QUEST, QuestStatus::START, 1);
	std::vector<uint8_t> window = dialogWindow(follower->getObjectId(), 0, 0);
	std::vector<uint8_t> giveUp = me->serializedFor(
		SM_SYSTEM_MESSAGE::STR_QUEST_SYSTEMMSG_GIVEUP(dataholders::DataManager::QUEST_DATA->getQuestById(ESCORT_QUEST)->getL10n()));
	me->clearSent();
	EXPECT_TRUE(handler->defaultStartFollowEvent(*env, *follower, MIRES, 1, 1));
	EXPECT_EQ(me->sent(), cp::exactly({window})) << "1 -> 1, the npc overload";

	std::vector<uint8_t> npcInfo = me->serializedFor(SM_NPC_INFO(*follower, player()));
	me->clearSent();
	EXPECT_TRUE(handler->defaultStartFollowEvent(*env, *follower, 130.0f, 100.0f, 50.0f, 1, 1));
	EXPECT_EQ(me->sent(), cp::exactly({npcInfo, window})) << "1 -> 1, the point overload";

	me->clearSent();
	EXPECT_TRUE(handler->defaultStartFollowEvent(*env, *follower, MIRES, 1, 0));
	EXPECT_EQ(me->sent(), cp::exactly({giveUp, questUpdate(ESCORT_QUEST, START, 0), window})) << "1 -> 0, the npc overload";
	EXPECT_EQ(var(), 0);

	qs->setQuestVar(1);
	npcInfo = me->serializedFor(SM_NPC_INFO(*follower, player()));
	me->clearSent();
	EXPECT_TRUE(handler->defaultStartFollowEvent(*env, *follower, 130.0f, 100.0f, 50.0f, 1, 0));
	EXPECT_EQ(me->sent(), cp::exactly({npcInfo, giveUp, questUpdate(ESCORT_QUEST, START, 0), window})) << "1 -> 0, the point overload";
	EXPECT_EQ(var(), 0);
}

// :826: the point overload hands the point on with its height: a point 25 m above the follower is not reached, and it is once the follower
// stands 19 m below it
TEST_F(AbstractQuestHandlerFollowTest, StartFollowToAPointKeepsItsHeight) {
	handler->defaultStartFollowEvent(*env, *follower, 105.0f, 100.0f, 75.0f, 0, 0);
	advance(1000);
	EXPECT_TRUE(targetEvents.empty()) << "25 m below the point";
	moveTo(*follower, 105.0f, 100.0f, 56.0f);
	advance(1000);
	EXPECT_TRUE(targetEvents.size() == 1 && targetEvents[0].reached) << "19 m below it: arrived";
}

// QuestTasks.java:43-45 through :814: an npc id without a spawn throws IllegalArgumentException out of defaultStartFollowEvent, after the npc
// turned PEACE and started to follow - no QUEST_FOLLOW task, no step
TEST_F(AbstractQuestHandlerFollowTest, StartFollowToAnNpcWithoutASpawnThrowsOnceTheNpcFollows) {
	publishFollowSpawns();
	hold(*me, ESCORT_QUEST, QuestStatus::START);
	me->clearSent();
	EXPECT_THROW(static_cast<void>(handler->defaultStartFollowEvent(*env, *follower, 299999, 0, 1)), runtime::IllegalArgumentException);
	EXPECT_EQ(follower->getType(player()), CreatureType::PEACE);
	EXPECT_EQ(ai->getState(), ai::AIState::FOLLOWING);
	EXPECT_FALSE(hasFollowTask());
	EXPECT_EQ(var(), 0);
	EXPECT_TRUE(me->sent().empty());
}

// The escort, start to end (_1149MissingPoppy.java:64, 84-87): started at step 0 -> 1, the follower arriving below 20 m from the point ends it
// at the next check - onNpcReachTarget -> defaultFollowEndEvent(env, 1, 1, true, 12): REWARD, the nearby quests, movie 12 on the follower; the
// follower got STOP_FOLLOW_ME, and FollowingNpcAI's stop deleted it, before the handler's event (FollowingNpcCheckTask.java:49-52)
TEST_F(AbstractQuestHandlerFollowTest, TheEscortEndsTheStepWhenTheFollowerArrives) {
	std::vector<std::pair<size_t, bool>> followerAtEvent; // the follower AI's events so far, and whether the follower was spawned
	atEvent = [&] { followerAtEvent.emplace_back(ai->events.size(), follower->isSpawned()); };
	hold(*me, ESCORT_QUEST, QuestStatus::START);
	EXPECT_TRUE(handler->defaultStartFollowEvent(*env, *follower, 130.0f, 100.0f, 50.0f, 0, 1));
	advance(1000);
	EXPECT_TRUE(targetEvents.empty()) << "25 m to go";
	moveTo(*follower, 115.0f, 100.0f, 50.0f);
	me->clearSent();
	advance(1000);
	ASSERT_EQ(targetEvents.size(), 1u);
	EXPECT_TRUE(targetEvents[0].reached);
	EXPECT_EQ(targetEvents[0].env, env.get());
	EXPECT_FALSE(targetEvents[0].followTask) << "the task was cancelled before the event";
	EXPECT_EQ(followerAtEvent, (std::vector<std::pair<size_t, bool>>{{2, false}})) << "STOP_FOLLOW_ME and the delete came before the event";
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(ESCORT_QUEST, REWARD, 1), noNearbyQuests(),
		playMovie(false, follower->getObjectId(), ESCORT_QUEST, 12)}));
	EXPECT_EQ(player().getQuestStateList()->getQuestState(ESCORT_QUEST)->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(ai->getState(), ai::AIState::DESPAWNED) << "stopFollow: IDLE, then the owner's delete despawns it";
	EXPECT_FALSE(follower->isSpawned()) << "FollowEventHandler.stopFollow deletes the owner";
	EXPECT_FALSE(hasFollowTask());
	ASSERT_EQ(ai->events.size(), 2u);
	EXPECT_EQ(ai->events[1].type, AIEventType::STOP_FOLLOW_ME);
	advance(5000);
	EXPECT_EQ(targetEvents.size(), 1u);
}

// A lost escort (_1149MissingPoppy.java:89-92): the player 50 m from the follower fails it at the next check - onNpcLostTarget ->
// defaultFollowEndEvent(env, 1, 0, false): the step goes back to 0, with the give-up message changeQuestStep sends for a rollback in START; the
// stop came first here too (FollowingNpcCheckTask.java:54-57)
TEST_F(AbstractQuestHandlerFollowTest, ALostEscortTakesTheStepBack) {
	std::vector<std::pair<size_t, bool>> followerAtEvent;
	atEvent = [&] { followerAtEvent.emplace_back(ai->events.size(), follower->isSpawned()); };
	hold(*me, ESCORT_QUEST, QuestStatus::START);
	EXPECT_TRUE(handler->defaultStartFollowEvent(*env, *follower, 130.0f, 100.0f, 50.0f, 0, 1));
	moveTo(player(), 55.0f, 100.0f, 50.0f);
	std::vector<uint8_t> giveUp = me->serializedFor(
		SM_SYSTEM_MESSAGE::STR_QUEST_SYSTEMMSG_GIVEUP(dataholders::DataManager::QUEST_DATA->getQuestById(ESCORT_QUEST)->getL10n()));
	me->clearSent();
	advance(1000);
	ASSERT_EQ(targetEvents.size(), 1u);
	EXPECT_FALSE(targetEvents[0].reached);
	EXPECT_FALSE(targetEvents[0].followTask) << "the task was cancelled before the event";
	EXPECT_EQ(followerAtEvent, (std::vector<std::pair<size_t, bool>>{{2, false}})) << "STOP_FOLLOW_ME and the delete came before the event";
	EXPECT_EQ(me->sent(), cp::exactly({giveUp, questUpdate(ESCORT_QUEST, START, 0)}));
	EXPECT_EQ(var(), 0);
	EXPECT_FALSE(follower->isSpawned());
	EXPECT_FALSE(hasFollowTask());
	advance(5000);
	EXPECT_EQ(targetEvents.size(), 1u);
}

// :814, :826 add the check under QUEST_FOLLOW, and CreatureController.addTask cancels the task it replaces: a second escort's start stops the
// first one's check, which never runs again
TEST_F(AbstractQuestHandlerFollowTest, ASecondStartReplacesTheFirstCheck) {
	EXPECT_TRUE(handler->defaultStartFollowEvent(*env, *follower, 105.0f, 110.0f, 50.0f, 0, 0)); // 10 m: would arrive at its first check
	Npc& second = spawnNpc(STRIPED_KERUB, 95.0f, 100.0f, 50.0f);
	FollowingProbeAI& secondAi = followingAi(second);
	EXPECT_TRUE(handler->defaultStartFollowEvent(*talkTo(second), second, 200.0f, 100.0f, 50.0f, 0, 0));
	EXPECT_TRUE(hasFollowTask());
	advance(1000);
	EXPECT_TRUE(targetEvents.empty()) << "the first check was cancelled; the second one's npc is 105 m from its point";
	EXPECT_TRUE(follower->isSpawned());
	EXPECT_EQ(ai->events.size(), 1u) << "no STOP_FOLLOW_ME: the replaced check never ran";
	moveTo(second, 190.0f, 100.0f, 50.0f);
	moveTo(player(), 185.0f, 100.0f, 50.0f);
	advance(1000);
	ASSERT_EQ(targetEvents.size(), 1u);
	EXPECT_TRUE(targetEvents[0].reached);
	EXPECT_EQ(secondAi.events.size(), 2u);
	EXPECT_FALSE(second.isSpawned());
}

} // namespace
} // namespace aion::gameserver::questEngine::handlers::test
