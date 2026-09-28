// The ascension lane (m5d-plan.md A-01 and A-04, P5-05 / aion_gs_handlers_ai_core): AbyssGuardSimpleAI, the AI of the 859 "simple_abyssguard"
// npc templates. Without it AIEngine gives them a DummyNpcAI (in the profiles' warn mode), so Jucleas (203752) of quest 1007 and the six
// Pandaemonium guards of quest 2009 neither talk nor fight.
//
// Java: data/handlers/ai/AbyssGuardSimpleAI.java:23-76. The cases drive real events into the handler over real npcs built from shipped rows
// (QuestNpcAiTestSupport.h) and assert what Java decides:
// - the AION_AI marker (the factory the registry references) and the superclass chain;
// - canHandleEvent: CREATURE_MOVED in every state but FIGHT, everything else as AggressiveNpcAI / GeneralNpcAI / AbstractAI decide;
// - the npc-vs-npc aggro rule of the private checkAggro, row by row, and that it is a different rule from CreatureEventHandler.checkAggro (no
//   aggro angle, center-to-center range, a level floor of 2, npcs under attack ignored, no handleCreatureDetected);
// - characters still going through AggressiveNpcAI's rule;
// - handleCreatureNeedsSupportByGuard answering false where AggressiveNpcAI's would join the fight;
// - the whole chain once, from CREATURE_SEE to a hated enemy and AIState::FIGHT.
// The talk (the reason quest 1007 needs this AI) is AbyssGuardDialogTest.cpp's; the last conjunct of the rule, GeoService.canSee, needs geo
// data on and is AbyssGuardGeoTest.cpp's (here the fixture keeps geo off, so canSee answers true).

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/handlers/ai/AbyssGuardSimpleAI.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/state/CreatureVisualState.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "QuestNpcAiTestSupport.h"

// The factory functions the AION_AI markers define, declared exactly as Registry.ai.gen.cpp declares them (handlers::AIFactory)
namespace aion::gameserver::handlers::ai {
::aion::gameserver::handlers::AIFactory AbyssGuardSimpleAI_aiFactory;
::aion::gameserver::handlers::AIFactory AggressiveNpcAI_aiFactory;
} // namespace aion::gameserver::handlers::ai

namespace aion::gameserver::ai::testing {
namespace {

namespace roots = gameserver::handlers::ai;

using event::AIEventType;
using model::gameobjects::Creature;
using model::gameobjects::Npc;
using model::gameobjects::VisibleObject;
using model::gameobjects::player::Player;
using model::gameobjects::state::CreatureVisualState;

/**
 * The shipped AbyssGuardSimpleAI with two hooks recorded instead of run: handleCreatureDetected (only CreatureEventHandler.checkAggro calls it)
 * and handleCreatureAggro (what both aggro rules end in). Everything the handler itself decides is the shipped code; the recording only says
 * which rule ran and whether it aggroed, without the 500 ms notifier and the fight after it. canHandleEvent and
 * handleCreatureNeedsSupportByGuard are made callable for their tables.
 */
class RecordingAbyssGuard final : public roots::AbyssGuardSimpleAI {
public:
	using AbyssGuardSimpleAI::AbyssGuardSimpleAI;
	using AbyssGuardSimpleAI::canHandleEvent;
	using AbyssGuardSimpleAI::handleCreatureNeedsSupportByGuard;

	std::vector<std::string> calls;

	void handleCreatureDetected(Creature& creature) override {
		static_cast<void>(creature);
		calls.push_back("detected");
	}

protected:
	void handleCreatureAggro(Creature& creature) override {
		static_cast<void>(creature);
		calls.push_back("aggro");
	}
};

/** AggressiveNpcAI with its guard-support hook callable: the superclass answer AbyssGuardSimpleAI overrides */
class ProbedAggressiveNpcAI final : public roots::AggressiveNpcAI {
public:
	using AggressiveNpcAI::AggressiveNpcAI;
	using AggressiveNpcAI::handleCreatureNeedsSupportByGuard;
};

/** Life stats constructed with 0 current HP, so isDead() is true from the start (CreatureLifeStats.cpp:40-41, 54-56) without a death path */
class DeadLifeStats final : public model::stats::container::CreatureLifeStats {
public:
	explicit DeadLifeStats(Creature& owner) : CreatureLifeStats(owner, 0, 0) {}
};

class AbyssGuardSimpleAiTest : public QuestNpcAiWorldTest {
protected:
	/** the calls one event made, cleared before it */
	static std::vector<std::string> callsOf(RecordingAbyssGuard& ai, AIEventType event, Creature& creature) {
		ai.calls.clear();
		ai.onCreatureEvent(event, creature);
		return ai.calls;
	}

	const std::vector<std::string> none;
	const std::vector<std::string> aggro{"aggro"};
};

// ---- the AION_AI marker -----------------------------------------------------------------------------------------------------------------------

TEST_F(AbyssGuardSimpleAiTest, TheMarkerDefinesAFactoryThatBuildsAnAbyssGuardForAnNpcOwner) {
	AI_TEST_SCOPE;
	runtime::Ref<Npc> jucleas = makeWorldNpc(JUCLEAS, 500, 500, 100);

	// the factory's own answers for an npc and for a player owner are HandlerRegistry.h's createAI, which RootAiHandlersTest pins
	std::unique_ptr<AbstractAI> ai = roots::AbyssGuardSimpleAI_aiFactory(*jucleas);
	ASSERT_TRUE(ai);
	ASSERT_TRUE(dynamic_cast<roots::AbyssGuardSimpleAI*>(ai.get()));
	EXPECT_TRUE(dynamic_cast<roots::AggressiveNpcAI*>(ai.get()))
		<< "AbyssGuardSimpleAI extends AggressiveNpcAI (and so talks, fights back and walks home like every GeneralNpcAI)";
}

// ---- canHandleEvent ---------------------------------------------------------------------------------------------------------------------------

TEST_F(AbyssGuardSimpleAiTest, MovesAreHandledInEveryStateButFightAndTheRestAsTheSuperclassesDecide) {
	AI_TEST_SCOPE;
	runtime::Ref<Npc> jucleas = makeWorldNpc(JUCLEAS, 500, 500, 100);
	RecordingAbyssGuard& ai = installLeaf<RecordingAbyssGuard>(*jucleas);

	// AbyssGuardSimpleAI.java:23-30: `case CREATURE_MOVED: return getState() != AIState.FIGHT;` and nothing else, so the FOLLOWING and
	// RETURNING rows are where it differs from AbstractAI's own CREATURE_MOVED rule (IDLE or WALKING only, AbstractAI.java:76-84)
	struct Row {
		AIState state;
		AIEventType event;
		bool handled;
		const char* why;
	};
	const Row rows[] = {
		{AIState::IDLE, AIEventType::CREATURE_MOVED, true, "idle"},
		{AIState::WALKING, AIEventType::CREATURE_MOVED, true, "walking"},
		{AIState::FOLLOWING, AIEventType::CREATURE_MOVED, true, "following: AbstractAI alone would drop the move"},
		{AIState::RETURNING, AIEventType::CREATURE_MOVED, true, "returning: AbstractAI alone would drop the move"},
		{AIState::FEAR, AIEventType::CREATURE_MOVED, true, "feared"},
		{AIState::FIGHT, AIEventType::CREATURE_MOVED, false, "fighting: the override's one false answer"},
		// `!= FIGHT` also answers true where the state's own event set (AIStateInfo.h canHandle, Java AIState) drops the move anyway
		{AIState::DIED, AIEventType::CREATURE_MOVED, true, "dead: the override answers true, the state drops it"},
		{AIState::CREATED, AIEventType::CREATURE_MOVED, true, "created: the override answers true, the state drops it"},
		{AIState::DESPAWNED, AIEventType::CREATURE_MOVED, true, "despawned: the override answers true, the state drops it"},
		{AIState::RETURNING, AIEventType::DIALOG_START, false, "every other event falls through: AbstractAI's IDLE-or-WALKING dialog rule"},
		{AIState::FIGHT, AIEventType::CREATURE_NEEDS_SUPPORT, false, "every other event falls through: GeneralNpcAI's support rule"},
		{AIState::IDLE, AIEventType::CREATURE_NEEDS_SUPPORT, true, "GeneralNpcAI's support rule, the idle side"},
		{AIState::FIGHT, AIEventType::CREATURE_SEE, true, "every other event falls through: AbstractAI's default"},
	};
	for (const Row& row : rows) {
		ai.setStateIfNot(row.state);
		ASSERT_TRUE(ai.isInState(row.state)) << row.why;
		EXPECT_EQ(ai.canHandleEvent(row.event), row.handled) << row.why;
	}
}

// ---- the npc-vs-npc aggro rule ----------------------------------------------------------------------------------------------------------------

TEST_F(AbyssGuardSimpleAiTest, AnEnemyNpcSeenOrMovingInTheAggroRangeIsAggroedWithoutTheCharacterRule) {
	AI_TEST_SCOPE;
	runtime::Ref<Npc> jucleas = makeWorldNpc(JUCLEAS, 500, 500, 100); // heading 0: facing +x
	RecordingAbyssGuard& ai = installLeaf<RecordingAbyssGuard>(*jucleas);
	runtime::Ref<Npc> inFront = makeWorldNpc(BALDER, 505, 500, 100);
	runtime::Ref<Npc> behind = makeWorldNpc(BALDER, 495, 500, 100);
	ASSERT_TRUE(jucleas->isEnemy(*inFront)) << "TribeRelationService: GUARD is aggressive to GUARD_DARK (TribeRelationService.java:37-44)";

	// only CREATURE_AGGRO: the npc arm never enters CreatureEventHandler, whose checkAggro would call handleCreatureDetected first
	EXPECT_EQ(callsOf(ai, AIEventType::CREATURE_SEE, *inFront), aggro) << "seen, 5 m in front";
	EXPECT_EQ(callsOf(ai, AIEventType::CREATURE_MOVED, *inFront), aggro) << "moved, 5 m in front (AbyssGuardSimpleAI.java:40-46)";

	// no aggro angle: sangle 300 leaves a 60 degree blind cone behind the guard for CreatureEventHandler.isInSeeRange, and 5 m is outside its
	// short aggro range (7 / 2 = 3 m plus the bound radii), so the character rule would not aggro here
	EXPECT_EQ(callsOf(ai, AIEventType::CREATURE_SEE, *behind), aggro) << "5 m behind the guard";
	EXPECT_EQ(callsOf(ai, AIEventType::CREATURE_MOVED, *behind), aggro);
}

TEST_F(AbyssGuardSimpleAiTest, TheRangeIsTheAggroRangeCenterToCenter) {
	AI_TEST_SCOPE;
	runtime::Ref<Npc> jucleas = makeWorldNpc(JUCLEAS, 500, 500, 100);
	RecordingAbyssGuard& ai = installLeaf<RecordingAbyssGuard>(*jucleas);
	runtime::Ref<Npc> balder = makeWorldNpc(BALDER, 506.9f, 500, 100);

	// PositionUtil.isInRange(owner, npc, owner.getAggroRange()) is the three-argument overload, centerToCenter = true (PositionUtil.java:235-237):
	// srange 7 without the two bound radii of 0.35 m that CreatureEventHandler.isInSeeRange adds
	EXPECT_EQ(callsOf(ai, AIEventType::CREATURE_SEE, *balder), aggro) << "6.9 m";
	// the boundary: PositionUtil.isInRange compares the squared distance with `<` (PositionUtil.java:257-262 = PositionUtil.cpp:204-209)
	balder->getPosition()->setXYZH(506.95f, std::nullopt, std::nullopt, std::nullopt);
	EXPECT_EQ(callsOf(ai, AIEventType::CREATURE_SEE, *balder), aggro) << "6.95 m, just inside";
	balder->getPosition()->setXYZH(507.0f, std::nullopt, std::nullopt, std::nullopt);
	EXPECT_EQ(callsOf(ai, AIEventType::CREATURE_SEE, *balder), none) << "exactly 7 m is out";
	balder->getPosition()->setXYZH(507.5f, std::nullopt, std::nullopt, std::nullopt);
	EXPECT_EQ(callsOf(ai, AIEventType::CREATURE_SEE, *balder), none) << "7.5 m: inside 7 + 0.35 + 0.35, outside 7";
	balder->getPosition()->setXYZH(520.0f, std::nullopt, std::nullopt, std::nullopt);
	EXPECT_EQ(callsOf(ai, AIEventType::CREATURE_MOVED, *balder), none) << "20 m";
}

/**
 * The early returns of checkAggro that look at the other npc (AbyssGuardSimpleAI.java:60-68), each on an npc that differs from an aggroed
 * Balder in that one respect: dead, not visible to the guard, no enemy, below level 2, already fighting someone.
 */
TEST_F(AbyssGuardSimpleAiTest, DeadHiddenFriendlyLowLevelAndEngagedNpcsAreLeftAlone) {
	AI_TEST_SCOPE;
	runtime::Ref<Npc> jucleas = makeWorldNpc(JUCLEAS, 500, 500, 100);
	RecordingAbyssGuard& ai = installLeaf<RecordingAbyssGuard>(*jucleas);

	runtime::Ref<Npc> dead = makeWorldNpc(BALDER, 505, 500, 100);
	dead->setLifeStats(std::make_unique<DeadLifeStats>(*dead));
	ASSERT_TRUE(dead->isDead());
	EXPECT_EQ(callsOf(ai, AIEventType::CREATURE_SEE, *dead), none) << "npc.isDead()";

	runtime::Ref<Npc> hidden = makeWorldNpc(BALDER, 505, 501, 100);
	hidden->setVisualState(CreatureVisualState::HIDE1);
	ASSERT_FALSE(jucleas->canSee(runtime::Ptr<VisibleObject>(*hidden)));
	EXPECT_EQ(callsOf(ai, AIEventType::CREATURE_SEE, *hidden), none) << "!owner.canSee(npc)";
	hidden->unsetVisualState(CreatureVisualState::HIDE1);
	EXPECT_EQ(callsOf(ai, AIEventType::CREATURE_SEE, *hidden), aggro) << "visible again";

	runtime::Ref<Npc> leah = makeWorldNpc(LEAH, 505, 499, 100);
	ASSERT_FALSE(jucleas->isEnemy(*leah));
	EXPECT_EQ(callsOf(ai, AIEventType::CREATURE_SEE, *leah), none) << "!owner.isEnemy(npc): GUARD and GENERAL";

	// the same tribe as Balder, so the level is the only difference; CreatureEventHandler.validateAggro would let an ABYSS_GUARD aggro any level
	runtime::Ref<Npc> shadowCourt = makeWorldNpc(SHADOW_COURT, 505, 502, 100);
	ASSERT_TRUE(jucleas->isEnemy(*shadowCourt));
	ASSERT_EQ(shadowCourt->getLevel(), 1);
	EXPECT_EQ(callsOf(ai, AIEventType::CREATURE_SEE, *shadowCourt), none) << "npc.getLevel() < 2";
	EXPECT_EQ(callsOf(ai, AIEventType::CREATURE_MOVED, *shadowCourt), none);
	// the floor itself: `npc.getLevel() < 2` lets a level-2 enemy through (another tribe, GUARD_DRAGON, the only shipped level-2 enemy of GUARD)
	runtime::Ref<Npc> balaur = makeWorldNpc(BALAUR_LEVEL_2, 505, 504, 100);
	ASSERT_TRUE(jucleas->isEnemy(*balaur)) << "TribeRelationService: GUARD is aggressive to GUARD_DRAGON (TribeRelationService.java:37-44)";
	ASSERT_EQ(balaur->getLevel(), 2);
	EXPECT_EQ(callsOf(ai, AIEventType::CREATURE_SEE, *balaur), aggro) << "level 2 is not below the floor";

	// "ignore npcs which are under attack"
	runtime::Ref<Npc> engaged = makeWorldNpc(BALDER, 505, 503, 100);
	EXPECT_EQ(callsOf(ai, AIEventType::CREATURE_SEE, *engaged), aggro) << "the row's control: no target yet";
	engaged->setTarget(runtime::Ptr<VisibleObject>(*leah));
	EXPECT_EQ(callsOf(ai, AIEventType::CREATURE_SEE, *engaged), none) << "npc.getTarget() != null";
}

/** The early returns that look at the guard itself (AbyssGuardSimpleAI.java:54-58 and :70-71) */
TEST_F(AbyssGuardSimpleAiTest, AGuardThatFightsReturnsOrStandsInASleepingRegionChecksNothing) {
	AI_TEST_SCOPE;
	runtime::Ref<Npc> jucleas = makeWorldNpc(JUCLEAS, 500, 500, 100);
	RecordingAbyssGuard& ai = installLeaf<RecordingAbyssGuard>(*jucleas);
	runtime::Ref<Npc> balder = makeWorldNpc(BALDER, 505, 500, 100);

	ai.setStateIfNot(AIState::FIGHT);
	EXPECT_EQ(callsOf(ai, AIEventType::CREATURE_SEE, *balder), none) << "isInState(FIGHT)";
	ai.setStateIfNot(AIState::RETURNING);
	EXPECT_EQ(callsOf(ai, AIEventType::CREATURE_SEE, *balder), none) << "isInState(RETURNING)";
	// canHandleEvent lets a move through while returning, so it is checkAggro's own RETURNING line that stops this one
	EXPECT_EQ(callsOf(ai, AIEventType::CREATURE_MOVED, *balder), none) << "a move while returning";
	ai.setStateIfNot(AIState::IDLE);
	EXPECT_EQ(callsOf(ai, AIEventType::CREATURE_SEE, *balder), aggro) << "the state was the only reason";

	place(*jucleas, 900, 900, 100); // a region no player entered (MapRegion.java:89-91)
	ASSERT_FALSE(jucleas->getPosition()->isMapRegionActive());
	runtime::Ref<Npc> nearInactive = makeWorldNpc(BALDER, 905, 900, 100);
	EXPECT_EQ(callsOf(ai, AIEventType::CREATURE_SEE, *nearInactive), none) << "!owner.getPosition().isMapRegionActive()";
	place(*jucleas, 500, 500, 100);
	EXPECT_EQ(callsOf(ai, AIEventType::CREATURE_SEE, *balder), aggro) << "every early return was undone";
}

/**
 * `owner.getPosition().isMapRegionActive()` (AbyssGuardSimpleAI.java:70-71) asks the guard's region, not the other npc's. A region is
 * activated with its neighbours (MapRegion.java:64-66 and :93-101), so two npcs 4 m apart can stand in regions of different states: with regions of
 * 128 m the fixture's activator at (500, 500) wakes the regions of x 256..639, so x 638 is awake and x 642 asleep.
 */
TEST_F(AbyssGuardSimpleAiTest, OnlyTheGuardsOwnRegionMustBeAwake) {
	AI_TEST_SCOPE;
	runtime::Ref<Npc> jucleas = makeWorldNpc(JUCLEAS, 638, 500, 100);
	RecordingAbyssGuard& ai = installLeaf<RecordingAbyssGuard>(*jucleas);
	runtime::Ref<Npc> balder = makeWorldNpc(BALDER, 642, 500, 100);
	ASSERT_TRUE(jucleas->getPosition()->isMapRegionActive());
	ASSERT_FALSE(balder->getPosition()->isMapRegionActive());
	EXPECT_EQ(callsOf(ai, AIEventType::CREATURE_SEE, *balder), aggro) << "the guard's region is awake, the enemy's asleep";

	place(*jucleas, 642, 500, 100);
	place(*balder, 638, 500, 100);
	ASSERT_FALSE(jucleas->getPosition()->isMapRegionActive());
	ASSERT_TRUE(balder->getPosition()->isMapRegionActive());
	EXPECT_EQ(callsOf(ai, AIEventType::CREATURE_SEE, *balder), none) << "the guard's region is asleep, the enemy's awake";
}

// ---- characters -------------------------------------------------------------------------------------------------------------------------------

TEST_F(AbyssGuardSimpleAiTest, CharactersGoThroughTheAggressiveNpcRule) {
	AI_TEST_SCOPE;
	runtime::Ref<Npc> jucleas = makeWorldNpc(JUCLEAS, 500, 500, 100);
	RecordingAbyssGuard& ai = installLeaf<RecordingAbyssGuard>(*jucleas);
	runtime::Ref<Player> asmodianInFront = makeAsmodian(700101, 505, 500, 100);
	runtime::Ref<Player> asmodianBehind = makeAsmodian(700102, 495, 500, 100);
	runtime::Ref<Player> elyos = makeWorldPlayer(700103, 505, 501, 100);

	// super.handleCreatureSee / super.handleCreatureMoved -> CreatureEventHandler: handleCreatureDetected first, then the tribe decision
	// (GUARD is aggressive to PC_DARK, TribeRelationService.java:37-44; validateAggro's ABYSS_GUARD arm takes any level)
	const std::vector<std::string> detectedAndAggro{"detected", "aggro"};
	EXPECT_EQ(callsOf(ai, AIEventType::CREATURE_SEE, *asmodianInFront), detectedAndAggro) << "an Asmodian 5 m in front";
	EXPECT_EQ(callsOf(ai, AIEventType::CREATURE_MOVED, *asmodianInFront), detectedAndAggro) << "and when he moves";
	// the character rule's angle, which the npc rule does not have (the same 5 m behind aggroes Balder above)
	EXPECT_EQ(callsOf(ai, AIEventType::CREATURE_SEE, *asmodianBehind), none) << "an Asmodian 5 m behind, in the blind cone";
	EXPECT_EQ(callsOf(ai, AIEventType::CREATURE_SEE, *elyos), (std::vector<std::string>{"detected"}))
		<< "an Elyos is detected and not aggroed: GUARD is not aggressive to PC";
}

// ---- handleCreatureNeedsSupportByGuard --------------------------------------------------------------------------------------------------------

TEST_F(AbyssGuardSimpleAiTest, AnAbyssGuardNeverAnswersAGuardCallWhereAnAggressiveNpcWould) {
	AI_TEST_SCOPE;
	// Leah is attacked by an Asmodian and asks around for support. canHelpCreature(guard, Leah) is false (the GUARD row has no <support>), so
	// AbstractAI's CREATURE_NEEDS_SUPPORT arm goes on to handleCreatureNeedsSupportByGuard (AbstractAI.java:339-343), which is where the two
	// classes differ (AbyssGuardSimpleAI.java:48-51, AggressiveNpcAI.java:30-33 -> AggroEventHandler.java:38-49).
	runtime::Ref<Npc> leah = makeWorldNpc(LEAH, 503, 500, 100);
	runtime::Ref<Player> attacker = makeAsmodian(700201, 504, 500, 100);
	leah->setTarget(runtime::Ptr<VisibleObject>(*attacker));

	runtime::Ref<Npc> abyssGuard = makeWorldNpc(JUCLEAS, 500, 500, 100);
	runtime::Ref<Npc> aggressiveGuard = makeWorldNpc(JUCLEAS, 500, 503, 100); // the same template with the superclass as its AI
	for (Npc* guard : {abyssGuard.get(), aggressiveGuard.get()}) {
		know(*guard, *leah);
		know(*guard, *attacker); // AggroList.isAware: hate needs a known creature
	}
	RecordingAbyssGuard& recording = installLeaf<RecordingAbyssGuard>(*abyssGuard);
	const auto pendingBefore = executor->pendingTasksCount();
	EXPECT_FALSE(recording.handleCreatureNeedsSupportByGuard(*leah));
	EXPECT_EQ(executor->pendingTasksCount(), pendingBefore) << "and schedules no AggroNotifier";

	// the same call through the event, with the shipped AI from the marker
	roots::AbyssGuardSimpleAI& ai = installFrom<roots::AbyssGuardSimpleAI>(roots::AbyssGuardSimpleAI_aiFactory, *abyssGuard);
	ai.onCreatureEvent(AIEventType::CREATURE_NEEDS_SUPPORT, *leah);
	executor->advance(std::chrono::milliseconds(1000));
	EXPECT_FALSE(abyssGuard->getAggroList().isHating(*attacker));
	EXPECT_TRUE(ai.isInState(AIState::IDLE));

	// the superclass answer, for the same template, caller and attacker; the notifier's hate starts a fight, so the clock stops right there
	ProbedAggressiveNpcAI& probe = installLeaf<ProbedAggressiveNpcAI>(*aggressiveGuard);
	EXPECT_TRUE(probe.handleCreatureNeedsSupportByGuard(*leah));
	executor->advance(std::chrono::milliseconds(500));
	EXPECT_TRUE(aggressiveGuard->getAggroList().isHating(*attacker)) << "its AggroNotifier ran";
}

// ---- end to end -------------------------------------------------------------------------------------------------------------------------------

TEST_F(AbyssGuardSimpleAiTest, AGuardThatSeesAnEnemyGuardHatesItAndFights) {
	AI_TEST_SCOPE;
	runtime::Ref<Npc> jucleas = makeWorldNpc(JUCLEAS, 500, 500, 100);
	runtime::Ref<Npc> balder = makeWorldNpc(BALDER, 505, 500, 100);
	know(*jucleas, *balder); // paired before the real AI is installed, so the knownlist notifications reach the no-op substitute
	roots::AbyssGuardSimpleAI& ai = installFrom<roots::AbyssGuardSimpleAI>(roots::AbyssGuardSimpleAI_aiFactory, *jucleas);

	// CREATURE_SEE -> checkAggro -> CREATURE_AGGRO -> AggressiveNpcAI.handleCreatureAggro -> AggroEventHandler's 500 ms AggroNotifier
	// -> AggroList.addHate(balder, 1) -> NpcController.onAddHate -> ATTACK -> AttackEventHandler.onAttack -> FIGHT
	ai.onCreatureEvent(AIEventType::CREATURE_SEE, *balder);
	EXPECT_FALSE(jucleas->getAggroList().isHating(*balder)) << "the AggroNotifier runs 500 ms later (AggroEventHandler.java:23)";
	executor->advance(std::chrono::milliseconds(500));
	EXPECT_TRUE(jucleas->getAggroList().isHating(*balder));
	EXPECT_EQ(jucleas->getAggroList().getHate(*balder), 1);
	EXPECT_TRUE(ai.isInState(AIState::FIGHT));
	EXPECT_TRUE(jucleas->isTargeting(balder->getObjectId()));

	// while it fights, moves are not handled at all, and a second enemy seen is stopped by checkAggro's FIGHT line: no second AggroNotifier.
	// (The clock is not advanced again: the fight's own tasks - the chase, the first swing - would run against an npc with a substitute AI.)
	runtime::Ref<Npc> second = makeWorldNpc(BALDER, 504, 502, 100);
	know(*jucleas, *second);
	const auto pendingBefore = executor->pendingTasksCount();
	ai.onCreatureEvent(AIEventType::CREATURE_MOVED, *second);
	ai.onCreatureEvent(AIEventType::CREATURE_SEE, *second);
	EXPECT_EQ(executor->pendingTasksCount(), pendingBefore);
	EXPECT_FALSE(jucleas->getAggroList().isHating(*second));
}

} // namespace
} // namespace aion::gameserver::ai::testing
