// A-06 (m5b-plan.md §4, P5-05 / aion_gs_handlers_ai_core): the three root AI handlers, the first files of the whole handler tree.
//
// They are the wiring between AbstractAI's dispatch and the ai/handler statics, so the cases below drive real events into a real GeneralNpcAI,
// AggressiveNpcAI and NoActionAI over a real Npc and assert what the handler package did:
// - the AION_AI markers: aion_gs_regscan turns each into a `Class##_aiFactory` function of the registry's type, and AIEngine::newAI's owner-type
//   check is that factory returning null (HandlerRegistry.h createAI). The test declares the three factories and calls them, which is the only
//   way to reach them from an executable that links the *empty* registry table (AionChunks.cmake: a handler target's tests do).
// - GeneralNpcAI::canHandleEvent's CREATURE_NEEDS_SUPPORT arm and chooseAttackIntention's FINISH_ATTACK and SIMPLE_ATTACK answers (the
//   SKILL_ATTACK answer, closed in M5b-2, is NpcSkillAttackTest's).
// - the aggro chain of an AggressiveNpcAI end to end: handleCreatureSee -> CreatureEventHandler -> CREATURE_AGGRO -> AggroEventHandler's 500 ms
//   AggroNotifier -> AggroList::addHate -> NpcController::onAddHate -> the ATTACK event -> AttackEventHandler -> AIState::FIGHT and a scheduled
//   attack. The scheduled attack is asserted as *scheduled*: running it reaches CreatureController::attackTarget, whose
//   standins::attackUtilCalculatePhysAttackResult is unported until item E-01b of stage 2.
// - NoActionAI's training-dummy arm.

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/AttackIntention.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/handlers/ai/NoActionAI.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"

#include "../ai/AiWorldTestSupport.h"

// The factory functions the AION_AI markers define, declared exactly as Registry.ai.gen.cpp declares them (handlers::AIFactory).
namespace aion::gameserver::handlers::ai {
::aion::gameserver::handlers::AIFactory AggressiveNpcAI_aiFactory;
::aion::gameserver::handlers::AIFactory GeneralNpcAI_aiFactory;
::aion::gameserver::handlers::AIFactory NoActionAI_aiFactory;
} // namespace aion::gameserver::handlers::ai

namespace aion::gameserver::ai::testing {
namespace {

namespace roots = gameserver::handlers::ai;

using event::AIEventType;
using model::gameobjects::Creature;
using model::gameobjects::Npc;

class RootAiHandlersTest : public AiWorldTest {
protected:
	void SetUp() override {
		AiWorldTest::SetUp();
		AI_TEST_SCOPE;
		regionActivator = activateRegionAt(500, 500, 100);
		executor->runReady(); // MapRegion::activate posts the ACTIVATE notification of its creatures; drain it before the cases count tasks
	}

	/** Replaces the npc's warn-mode substitute AI with the real root handler (this executable links the empty registry table). */
	template <class AI>
	AI& installRootAi(Npc& npc) {
		auto handlerAi = std::make_unique<AI>(npc);
		AI& result = *handlerAi;
		npc.replaceAi(std::move(handlerAi));
		return result;
	}

	runtime::Ref<model::gameobjects::player::Player> regionActivator;
};

// ---- the AION_AI markers ----------------------------------------------------------------------------------------------------------------------

TEST_F(RootAiHandlersTest, EachMarkerDefinesAFactoryThatBuildsItsAiForAnNpcOwner) {
	AI_TEST_SCOPE;
	runtime::Ref<Npc> npc = makeWorldNpc(SPARKIE_NPC_ID, 500, 500, 100);

	std::unique_ptr<AbstractAI> general = roots::GeneralNpcAI_aiFactory(*npc);
	ASSERT_TRUE(general);
	EXPECT_TRUE(dynamic_cast<roots::GeneralNpcAI*>(general.get()));
	EXPECT_FALSE(dynamic_cast<roots::AggressiveNpcAI*>(general.get()));

	std::unique_ptr<AbstractAI> aggressive = roots::AggressiveNpcAI_aiFactory(*npc);
	ASSERT_TRUE(aggressive);
	EXPECT_TRUE(dynamic_cast<roots::AggressiveNpcAI*>(aggressive.get())) << "AggressiveNpcAI extends GeneralNpcAI";
	EXPECT_TRUE(dynamic_cast<roots::GeneralNpcAI*>(aggressive.get()));

	std::unique_ptr<AbstractAI> noAction = roots::NoActionAI_aiFactory(*npc);
	ASSERT_TRUE(noAction);
	EXPECT_TRUE(dynamic_cast<roots::NoActionAI*>(noAction.get()));
	EXPECT_FALSE(dynamic_cast<roots::GeneralNpcAI*>(noAction.get())) << "NoActionAI extends NpcAI directly";

	// Java: AIEngine.newAI's findConstructor(aiClass, owner.getClass()) returns null for the wrong owner type, and newAI then throws
	// "<class> cannot be instantiated with <owner> as the owner". The C++ factory answers with a null unique_ptr (HandlerRegistry.h createAI).
	EXPECT_EQ(roots::GeneralNpcAI_aiFactory(*regionActivator), nullptr) << "a Player is no Npc";
}

// ---- GeneralNpcAI -----------------------------------------------------------------------------------------------------------------------------

TEST_F(RootAiHandlersTest, GeneralNpcAiOnlyTakesSupportCallsWhileIdleOrWalking) {
	AI_TEST_SCOPE;
	runtime::Ref<Npc> npc = makeWorldNpc(SPARKIE_NPC_ID, 500, 500, 100);
	runtime::Ref<Npc> friendly = makeWorldNpc(SPARKIE_NPC_ID, 502, 500, 100); // same tribe: TribeRelationService::canHelpCreature
	runtime::Ref<Npc> enemy = makeWorldNpc(GUARD_NPC_ID, 503, 500, 100);
	know(*npc, *friendly);
	know(*npc, *enemy);
	friendly->setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(*enemy)); // the attacker the friend asks for help against
	roots::GeneralNpcAI& ai = installRootAi<roots::GeneralNpcAI>(*npc);

	// the override of canHandleEvent (GeneralNpcAI.java:98-105): `getState() == IDLE || getState() == WALKING`, one row per side of the `||`
	// and one row per state the guard turns away. A run that only tries FIGHT, RETURNING and IDLE cannot tell the two-term condition from
	// `getState() == IDLE`, which is the whole decision the case is named after.
	struct Row {
		AIState state;
		bool helps;
		const char* why;
	};
	const Row rows[] = {
		{AIState::FIGHT, false, "already fighting: the support call is dropped"},
		{AIState::RETURNING, false, "walking home: the support call is dropped"},
		{AIState::WALKING, true, "a walking npc leaves its route and helps"},
		{AIState::IDLE, true, "an idle npc helps"},
	};
	for (const Row& row : rows) {
		// AggroEventHandler::onCreatureNeedsSupport answers false for an attacker the owner already hates, so each row starts from an empty list
		npc->getAggroList().clear();
		ASSERT_FALSE(npc->getAggroList().isHating(*enemy)) << row.why;
		ai.setStateIfNot(row.state);
		ASSERT_TRUE(ai.isInState(row.state)) << row.why;

		ai.onCreatureEvent(AIEventType::CREATURE_NEEDS_SUPPORT, *friendly);
		EXPECT_FALSE(npc->getAggroList().isHating(*enemy)) << "the AggroNotifier runs 500 ms later: " << row.why;
		executor->advance(std::chrono::milliseconds(600));
		EXPECT_EQ(npc->getAggroList().isHating(*enemy), row.helps) << row.why;
	}
}

TEST_F(RootAiHandlersTest, ChooseAttackIntentionFinishesWithoutAHatedTarget) {
	AI_TEST_SCOPE;
	runtime::Ref<Npc> npc = makeWorldNpc(SPARKIE_NPC_ID, 500, 500, 100);
	roots::GeneralNpcAI& ai = installRootAi<roots::GeneralNpcAI>(*npc);
	ai.setStateIfNot(AIState::FIGHT);

	// no target and an empty aggro list: getTarget(MOST_HATED) is null, so the fight ends
	EXPECT_EQ(ai.chooseAttackIntention(), AttackIntention::FINISH_ATTACK);
}

TEST_F(RootAiHandlersTest, ChooseAttackIntentionAnswersSimpleAttackForAnNpcWithoutSkills) {
	AI_TEST_SCOPE;
	runtime::Ref<Npc> npc = makeWorldNpc(SPARKIE_NPC_ID, 500, 500, 100);
	runtime::Ref<Npc> target = makeWorldNpc(GUARD_NPC_ID, 503, 500, 100);
	know(*npc, *target);
	roots::GeneralNpcAI& ai = installRootAi<roots::GeneralNpcAI>(*npc);
	ai.setStateIfNot(AIState::FIGHT);

	npc->getAggroList().addHate(*target, 10);
	npc->setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(*target));
	ASSERT_TRUE(npc->getAggroList().isHating(*target));

	// 210663 owns no npc_skills row (m5b2-plan.md §2.4 (b)), so SkillAttackManager::chooseNextSkill finds an empty skill list and the attack is
	// a simple melee attack; the npcs that own skills are NpcSkillAttackTest's
	EXPECT_EQ(ai.chooseAttackIntention(), AttackIntention::SIMPLE_ATTACK);
}

/** Life stats constructed with 0 current HP, so isDead() is true from the start (CreatureLifeStats.cpp:40-41, 54-56) without a death path. */
class DeadLifeStats final : public model::stats::container::CreatureLifeStats {
public:
	explicit DeadLifeStats(Creature& owner) : CreatureLifeStats(owner, 0, 0) {}
};

/**
 * ThinkEventHandler::onThink's two guards, made observable through ThinkEventHandler::thinkIdle: an idle npc that stands on its spawn point
 * with a heading other than its spawn heading schedules a 500 ms task that turns it back (ThinkEventHandler.java:92-99). A dropped think
 * schedules nothing.
 * <p>
 * The dead row installs life stats with 0 HP rather than setting the AI state to DIED, because `AbstractAI::isDead()` reads the **owner's**
 * life stats (AbstractAI.java:176-178) and not the AI state: a DIED state alone would leave the guard untouched and the row would pass for
 * the wrong reason (the switch of onThink has no DIED arm either).
 */
TEST_F(RootAiHandlersTest, ThinkResetsTheHeadingUnlessTheAiIsDeadOrAlreadyThinking) {
	AI_TEST_SCOPE;
	runtime::Ref<Npc> npc = makeWorldNpc(SPARKIE_NPC_ID, 500, 500, 100);
	roots::GeneralNpcAI& ai = installRootAi<roots::GeneralNpcAI>(*npc);
	npc->getPosition()->setH(int8_t{42}); // the spawn heading is 0
	ai.setStateIfNot(AIState::IDLE);
	ASSERT_EQ(executor->pendingTasksCount(), 0u);

	ASSERT_TRUE(ai.setThinking());
	ai.think();
	EXPECT_EQ(executor->pendingTasksCount(), 0u) << "the thinking flag drops the think (m5b-plan.md §8 risk 3)";
	ai.unsetThinking();

	ai.think();
	EXPECT_EQ(executor->pendingTasksCount(), 1u) << "thinkIdle scheduled the heading reset";
	EXPECT_EQ(npc->getPosition()->getHeading(), int8_t{42}) << "it runs 500 ms later";
	executor->advance(std::chrono::milliseconds(500));
	EXPECT_EQ(npc->getPosition()->getHeading(), npc->getSpawn()->getHeading());
	EXPECT_TRUE(ai.isInState(AIState::IDLE));

	npc->getPosition()->setH(int8_t{42});
	npc->setLifeStats(std::make_unique<DeadLifeStats>(*npc));
	ASSERT_TRUE(ai.isDead());
	ai.think();
	EXPECT_EQ(executor->pendingTasksCount(), 0u) << "onThink returns for a dead AI";
	EXPECT_EQ(npc->getPosition()->getHeading(), int8_t{42});
}

// ---- AggressiveNpcAI --------------------------------------------------------------------------------------------------------------------------

TEST_F(RootAiHandlersTest, AnAggressiveNpcSeesAggroesAndStartsFighting) {
	AI_TEST_SCOPE;
	runtime::Ref<Npc> npc = makeWorldNpc(SPARKIE_NPC_ID, 500, 500, 100);
	// 1 m apart: inside srange 8 and inside arange 2, so NpcGameStats::getNextAttackInterval takes its 750 ms opening-swing arm and the attack
	// is scheduled rather than carried out on the spot
	runtime::Ref<Npc> enemy = makeWorldNpc(GUARD_NPC_ID, 501, 500, 100);
	know(*npc, *enemy); // paired before the real AI is installed, so the knownlist notifications reach the no-op substitute
	roots::AggressiveNpcAI& ai = installRootAi<roots::AggressiveNpcAI>(*npc);
	ai.setStateIfNot(AIState::IDLE);
	ASSERT_TRUE(npc->getPosition()->isMapRegionActive());

	// CREATURE_SEE -> CreatureEventHandler::onCreatureSee -> checkAggro -> CREATURE_AGGRO -> AggroEventHandler::onAggro schedules the notifier
	ai.onCreatureEvent(AIEventType::CREATURE_SEE, *enemy);
	EXPECT_FALSE(npc->getAggroList().isHating(*enemy)) << "the AggroNotifier runs 500 ms later (AggroEventHandler.java:23)";
	EXPECT_GT(executor->pendingTasksCount(), 0u);

	executor->advance(std::chrono::milliseconds(500));
	EXPECT_TRUE(npc->getAggroList().isHating(*enemy)) << "addHate(target, 1)";
	EXPECT_EQ(npc->getAggroList().getHate(*enemy), 1);

	// onAddHate -> the ATTACK creature event -> AttackEventHandler::onAttack
	EXPECT_TRUE(ai.isInState(AIState::FIGHT));
	EXPECT_TRUE(ai.isInSubState(AISubState::NONE));
	EXPECT_TRUE(npc->isTargeting(enemy->getObjectId()));
	EXPECT_GT(npc->getGameStats()->getFightStartingTime(), 0) << "AttackManager::startAttacking";
	// AttackEventHandler::onAttack sets the target *before* startAttacking, and EmoteManager::emoteStartAttacking only draws the weapon when
	// there is one (AttackManager.java:25-27). Without that setTarget the npc still ends up targeting through TARGET_CHANGED, but unarmed.
	EXPECT_TRUE(npc->isInState(model::gameobjects::state::CreatureState::WEAPON_EQUIPPED)) << "the npc drew its weapon when the fight began";

	// SimpleAttackManager::performAttack scheduled the swing at the template's attack interval; running it would reach the unported
	// attack arithmetic of stage 2 (standins::attackUtilCalculatePhysAttackResult), so the assertion is that it is pending.
	EXPECT_TRUE(npc->getGameStats()->isNextAttackScheduled());
	EXPECT_GT(executor->pendingTasksCount(), 0u);
}

TEST_F(RootAiHandlersTest, AGeneralNpcDoesNotAggroWhatItSees) {
	AI_TEST_SCOPE;
	runtime::Ref<Npc> npc = makeWorldNpc(SPARKIE_NPC_ID, 500, 500, 100);
	runtime::Ref<Npc> enemy = makeWorldNpc(GUARD_NPC_ID, 503, 500, 100);
	know(*npc, *enemy); // the same setup as the aggressive case, so only the AI class differs
	roots::GeneralNpcAI& ai = installRootAi<roots::GeneralNpcAI>(*npc);
	ai.setStateIfNot(AIState::IDLE);

	// GeneralNpcAI does not override handleCreatureSee, so AITemplate's empty body runs: the whole aggro chain is AggressiveNpcAI's
	ai.onCreatureEvent(AIEventType::CREATURE_SEE, *enemy);
	executor->advance(std::chrono::milliseconds(1000));
	EXPECT_FALSE(npc->getAggroList().isHating(*enemy));
	EXPECT_TRUE(ai.isInState(AIState::IDLE));
}

// ---- NoActionAI -------------------------------------------------------------------------------------------------------------------------------

TEST_F(RootAiHandlersTest, ANoActionTrainingDummyDropsItsAggroAgain) {
	AI_TEST_SCOPE;
	runtime::Ref<Npc> dummy = makeWorldNpc(DUMMY_NPC_ID, 500, 500, 100);
	runtime::Ref<Npc> attacker = makeWorldNpc(GUARD_NPC_ID, 503, 500, 100);
	know(*dummy, *attacker);
	roots::NoActionAI& ai = installRootAi<roots::NoActionAI>(*dummy);
	ai.setStateIfNot(AIState::IDLE);

	dummy->setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(*attacker));

	// AggroList::addDamageAndHate ends in NpcController::onAddHate, which fires the ATTACK creature event; tribe DUMMY falls through the switch
	// to loseAggro(true), which clears the target and the aggro list again - the hate never survives the call that added it.
	dummy->getAggroList().addHate(*attacker, 10);
	EXPECT_FALSE(dummy->getAggroList().isHating(*attacker));
	EXPECT_FALSE(dummy->getTarget());
	EXPECT_TRUE(ai.isInState(AIState::IDLE)) << "a noaction npc never enters FIGHT";
}

TEST_F(RootAiHandlersTest, ANoActionNpcOfAnotherTribeKeepsItsAggro) {
	AI_TEST_SCOPE;
	runtime::Ref<Npc> statue = makeWorldNpc(NO_ACTION_NPC_ID, 500, 500, 100);
	runtime::Ref<Npc> attacker = makeWorldNpc(GUARD_NPC_ID, 503, 500, 100);
	know(*statue, *attacker);
	roots::NoActionAI& ai = installRootAi<roots::NoActionAI>(*statue);
	ai.setStateIfNot(AIState::IDLE);

	// tribe GENERAL: no case of the switch matches, so handleAttack does nothing and the hate the same call added stays
	statue->getAggroList().addHate(*attacker, 10);
	EXPECT_TRUE(statue->getAggroList().isHating(*attacker)) << "only the training dummies drop their aggro";
	EXPECT_EQ(statue->getAggroList().getHate(*attacker), 10);
	EXPECT_TRUE(ai.isInState(AIState::IDLE));
}

} // namespace
} // namespace aion::gameserver::ai::testing
