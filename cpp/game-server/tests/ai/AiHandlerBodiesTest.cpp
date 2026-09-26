// A-01 / A-02 / A-03 (m5b-plan.md §4, P5-05): the bodies of the ai/handler and ai/manager statics that the lane's own cases only ever reached
// as a side effect of something else, so a reviewer could empty them out and every test stayed green.
//
// Each case drives one static directly over a real Npc with a real NpcAI leaf, and asserts the state the body leaves behind:
// - AttackEventHandler::onAttack, statement by statement: the RETURNING early return, renewLastAttackedTime (which
//   AttackManager::checkGiveupDistance reads, and which therefore decides when a monster stops chasing), the FEAR/CONFUSE terms of allowFight,
//   and the setSubStateIfNot(NONE) without which AttackManager::scheduleNextAttack refuses to choose an attack at all.
// - SimpleAttackManager::attackAction's swing itself, asserted as the AION_UNPORTED stand-in it reaches: stage 1 stops at
//   standins::attackUtilCalculatePhysAttackResult, which item E-01b of stage 2 replaces.
// - CreatureEventHandler::onCreatureSee's TARGET_LOST block, TargetEventHandler::onTargetGiveup and onTargetChange's FIGHT gate,
//   WalkManager::stopWalking, ActivateEventHandler::onActivate, ReturningEventHandler::onBackHome, HpPhases::tryEnterNextPhase and
//   NpcAI::handleSpawned's second call.
//
// One of them asserts an exception on purpose, because that is what the tree does today and the assertion is how the gate lane learns it:
// NpcAI::handleSpawned reaches NpcShoutsService::mayShout (AION_UNPORTED, P5-14) once shouts are switched on; it is noted in the case that
// asserts it. ReturningEventHandler::onBackHome's EffectController::removeByDispelSlotType is ported since M5b-2 part 2, and its case puts
// ProbeEffect effects (tests/skills/P5-02b/EffectTestSupport.h) on the npc to see the buff of the fight dropped; since part 3 the same case
// gives the npc a post-spawn probe skill and sees it cast again after the dispel.

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/ai/handler/ActivateEventHandler.h"
#include "aion/gameserver/ai/handler/AttackEventHandler.h"
#include "aion/gameserver/ai/handler/CreatureEventHandler.h"
#include "aion/gameserver/ai/handler/ReturningEventHandler.h"
#include "aion/gameserver/ai/handler/TalkEventHandler.h"
#include "aion/gameserver/ai/handler/TargetEventHandler.h"
#include "aion/gameserver/ai/handler/ThinkEventHandler.h"
#include "aion/gameserver/ai/manager/SimpleAttackManager.h"
#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/attack/AggroTarget.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcSkillData.bind.h"
#include "aion/gameserver/dataholders/NpcSkillData.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/skill/NpcSkillList.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "../skills/P5-02b/EffectTestSupport.h"
#include "AiWorldTestSupport.h"

namespace aion::gameserver::ai::testing {
namespace {

using event::AIEventType;
using handler::ActivateEventHandler;
using handler::AttackEventHandler;
using handler::CreatureEventHandler;
using handler::ReturningEventHandler;
using handler::TargetEventHandler;
using manager::SimpleAttackManager;
using manager::WalkManager;
using model::gameobjects::Creature;
using model::gameobjects::Npc;
using model::gameobjects::state::CreatureState;

/**
 * A real NpcAI leaf that wires the hooks this file's cases fire to the same `ai/handler` statics GeneralNpcAI wires them to
 * (GeneralNpcAI.cpp:21-80). NpcAI itself wires none of them - AITemplate's `think()` and the handleX hooks are empty bodies, and the root
 * handlers of `game-server/handlers/` are the classes that fill them in. That chunk is not linked into this executable, so the leaf below is
 * the stand-in: the bodies under test are the shipped statics, only the dispatch is the test's.
 * <p>
 * AITemplate answers canThink() true and chooseAttackIntention() SIMPLE_ATTACK, which is what GeneralNpcAI answers for a melee npc in M5b-1.
 */
class HandlerTestAI : public NpcAI {
public:
	explicit HandlerTestAI(Npc& owner) : NpcAI(owner) {}

	using NpcAI::handleSpawned; // Java: protected, package access inside com.aionemu.gameserver.ai

	std::vector<std::string> calls;

	void think() override { handler::ThinkEventHandler::onThink(*this); }

	void handleCreatureDetected(Creature& creature) override { calls.push_back("detected"); }

protected:
	void handleCreatureAggro(Creature& creature) override { calls.push_back("aggro"); }

	void handleAttack(runtime::Ptr<Creature> creature) override { AttackEventHandler::onAttack(*this, creature); }

	void handleAttackComplete() override { AttackEventHandler::onAttackComplete(*this); }

	void handleFinishAttack() override { AttackEventHandler::onFinishAttack(*this); }

	void handleNotAtHome() override { ReturningEventHandler::onNotAtHome(*this); }

	void handleBackHome() override { ReturningEventHandler::onBackHome(*this); }

	void handleTargetTooFar() override { TargetEventHandler::onTargetTooFar(*this); }

	void handleTargetGiveup() override { TargetEventHandler::onTargetGiveup(*this); }

	void handleTargetChanged(Creature& creature) override {
		NpcAI::handleTargetChanged(creature);
		TargetEventHandler::onTargetChange(*this, creature);
	}
};

/** Java: `class X extends NpcAI implements HpPhases.PhaseHandler` - the shape HpPhases::tryEnterNextPhase casts its argument to. */
class PhasedAI final : public NpcAI, public HpPhases::PhaseHandler {
public:
	explicit PhasedAI(Npc& owner) : NpcAI(owner) {}

	void handleHpPhase(int32_t phaseHpPercent) override { phases.push_back(phaseHpPercent); }

	std::vector<int32_t> phases;
};

/** Sets AIConfig::SHOUTS_ENABLE for the scope and restores it (the M5b profile leaves it off, m5b-plan.md D1). */
class ShoutsEnabledScope {
public:
	explicit ShoutsEnabledScope(bool value) : previous(configs::main::AIConfig::SHOUTS_ENABLE.load()) {
		configs::main::AIConfig::SHOUTS_ENABLE.store(value);
	}
	~ShoutsEnabledScope() { configs::main::AIConfig::SHOUTS_ENABLE.store(previous); }
	ShoutsEnabledScope(const ShoutsEnabledScope&) = delete;
	ShoutsEnabledScope& operator=(const ShoutsEnabledScope&) = delete;

private:
	const bool previous;
};

class AiHandlerBodiesTest : public AiWorldTest {
protected:
	void SetUp() override {
		AiWorldTest::SetUp();
		AI_TEST_SCOPE;
		// Java: MapRegion.activate() runs when a player enters; ThinkEventHandler::onThink takes its inactive-region arm without it
		regionActivator = activateRegionAt(500, 500, 100);
		executor->runReady(); // MapRegion::activate posts the ACTIVATE notification of its creatures; drain it before the cases count tasks
	}

	/** Replaces the npc's warn-mode substitute AI with a real leaf (this executable links the empty registry table). */
	template <class AI>
	AI& installAi(Npc& npc) {
		auto leaf = std::make_unique<AI>(npc);
		AI& result = *leaf;
		npc.replaceAi(std::move(leaf));
		return result;
	}

	/** Keeps a probe skill template until the fixture goes: the effects made of it point into it */
	const skillengine::model::SkillTemplate* keep(std::unique_ptr<skillengine::model::SkillTemplate> skill) {
		keptSkills.push_back(std::move(skill));
		return keptSkills.back().get();
	}

	runtime::Ref<model::gameobjects::player::Player> regionActivator;
	/** What the probe effects of the BACK_HOME case record (skillengine::effecttest::ProbeEffect) */
	skillengine::effecttest::Journal journal;
	std::vector<std::unique_ptr<skillengine::model::SkillTemplate>> keptSkills;
	/** The load contexts of the BACK_HOME case's post-spawn skill data (PostSpawnSkillData) */
	xml::LoadContext postSpawnSkillContext;
	xml::LoadContext postSpawnNpcSkillContext;
};

// ---- AttackEventHandler::onAttack ------------------------------------------------------------------------------------------------------------

TEST_F(AiHandlerBodiesTest, OnAttackRenewsTheAttackedTimeAndEntersTheFight) {
	AI_TEST_SCOPE;
	// AttackEventHandler.java:24-56. renewLastAttackedTime is the timer AttackManager::checkGiveupDistance reads in three of its four arms
	// (AiDecisionTablesTest), so an onAttack that forgets it makes every monster give the character up after 20 seconds of being hit.
	runtime::Ref<Npc> npc = makeWorldNpc(SPARKIE_NPC_ID, 500, 500, 100);
	runtime::Ref<Npc> attacker = makeWorldNpc(GUARD_NPC_ID, 501, 500, 100);
	know(*npc, *attacker);
	HandlerTestAI& ai = installAi<HandlerTestAI>(*npc);
	ai.setStateIfNot(AIState::IDLE);
	ASSERT_GT(npc->getGameStats()->getLastAttackedTimeDelta(), 20) << "the game stats start at 0, so the delta is the seconds since the epoch";

	AttackEventHandler::onAttack(ai, runtime::Ptr<Creature>(*attacker));

	EXPECT_EQ(npc->getGameStats()->getLastAttackedTimeDelta(), 0) << "renewLastAttackedTime";
	EXPECT_TRUE(ai.isInState(AIState::FIGHT));
	EXPECT_TRUE(npc->isTargeting(attacker->getObjectId()));
	EXPECT_TRUE(npc->getGameStats()->isNextAttackScheduled()) << "AttackManager::startAttacking scheduled the first swing";
}

TEST_F(AiHandlerBodiesTest, OnAttackIgnoresANullOrDeadAttacker) {
	AI_TEST_SCOPE;
	runtime::Ref<Npc> npc = makeWorldNpc(SPARKIE_NPC_ID, 500, 500, 100);
	HandlerTestAI& ai = installAi<HandlerTestAI>(*npc);
	ai.setStateIfNot(AIState::IDLE);

	AttackEventHandler::onAttack(ai, nullptr);
	EXPECT_TRUE(ai.isInState(AIState::IDLE));
	EXPECT_GT(npc->getGameStats()->getLastAttackedTimeDelta(), 20) << "the guard returns before renewLastAttackedTime";
}

TEST_F(AiHandlerBodiesTest, OnAttackDropsTheEventWhileTheNpcIsWalkingHome) {
	AI_TEST_SCOPE;
	// the RETURNING arm (AttackEventHandler.java:33-38): abort the move, go IDLE and fire NOT_AT_HOME, then *return* - a monster on its way home
	// does not turn around and fight, it starts the way home again.
	runtime::Ref<Npc> npc = makeWorldNpc(SPARKIE_NPC_ID, 500, 500, 100);
	runtime::Ref<Npc> attacker = makeWorldNpc(GUARD_NPC_ID, 501, 500, 100);
	know(*npc, *attacker);
	HandlerTestAI& ai = installAi<HandlerTestAI>(*npc);
	ai.setStateIfNot(AIState::RETURNING);
	// 30 m from its spawn point, so the NOT_AT_HOME the arm fires really is a return and not the BACK_HOME of the case further down
	npc->getPosition()->setXYZH(530.0f, std::nullopt, std::nullopt, std::nullopt);

	AttackEventHandler::onAttack(ai, runtime::Ptr<Creature>(*attacker));

	EXPECT_FALSE(ai.isInState(AIState::FIGHT)) << "the arm returns before the fight";
	EXPECT_GT(npc->getGameStats()->getLastAttackedTimeDelta(), 20) << "and before renewLastAttackedTime, which comes six lines later";
	EXPECT_FALSE(npc->isTargeting(attacker->getObjectId()));
	EXPECT_FALSE(npc->getGameStats()->isNextAttackScheduled());
	EXPECT_TRUE(ai.isInState(AIState::RETURNING)) << "the NOT_AT_HOME event put it back on the way home";
}

TEST_F(AiHandlerBodiesTest, OnAttackDoesNotEnterTheFightWhileFearedOrConfused) {
	// `allowFight` (AttackEventHandler.java:45-47): a feared or confused npc is running, not fighting. The two abnormal-state terms of the same
	// expression need an Effect and belong to M5b-2; these are the two AI-state terms.
	for (AIState state : {AIState::FEAR, AIState::CONFUSE}) {
		AI_TEST_SCOPE;
		runtime::Ref<Npc> npc = makeWorldNpc(SPARKIE_NPC_ID, 500, 500, 100);
		runtime::Ref<Npc> attacker = makeWorldNpc(GUARD_NPC_ID, 501, 500, 100);
		know(*npc, *attacker);
		HandlerTestAI& ai = installAi<HandlerTestAI>(*npc);
		ai.setStateIfNot(state);

		AttackEventHandler::onAttack(ai, runtime::Ptr<Creature>(*attacker));

		EXPECT_TRUE(ai.isInState(state)) << "setStateIfNot(FIGHT) is behind allowFight";
		EXPECT_FALSE(npc->isTargeting(attacker->getObjectId()));
		EXPECT_FALSE(npc->getGameStats()->isNextAttackScheduled());
		EXPECT_EQ(npc->getGameStats()->getLastAttackedTimeDelta(), 0)
			<< "renewLastAttackedTime is before allowFight: a feared npc still counts as being attacked";
	}
}

TEST_F(AiHandlerBodiesTest, OnAttackClearsTheSubStateBeforeItStartsAttacking) {
	AI_TEST_SCOPE;
	// `npcAI.setSubStateIfNot(AISubState.NONE)` (AttackEventHandler.java:51). AttackManager::scheduleNextAttack refuses to choose an attack in
	// any other sub state (AttackManager.java:53-61), so without this line the npc enters FIGHT and then never swings.
	runtime::Ref<Npc> npc = makeWorldNpc(SPARKIE_NPC_ID, 500, 500, 100);
	runtime::Ref<Npc> attacker = makeWorldNpc(GUARD_NPC_ID, 501, 500, 100);
	know(*npc, *attacker);
	HandlerTestAI& ai = installAi<HandlerTestAI>(*npc);
	ai.setStateIfNot(AIState::IDLE);
	ASSERT_TRUE(ai.setSubStateIfNot(AISubState::TARGET_LOST));

	AttackEventHandler::onAttack(ai, runtime::Ptr<Creature>(*attacker));

	EXPECT_TRUE(ai.isInSubState(AISubState::NONE));
	EXPECT_TRUE(npc->getGameStats()->isNextAttackScheduled()) << "and the swing was chosen because the sub state was NONE by then";
}

// ---- SimpleAttackManager::attackAction -------------------------------------------------------------------------------------------------------

TEST_F(AiHandlerBodiesTest, AttackActionSwingsAtTheTargetAndTakesItsHitPoints) {
	AI_TEST_SCOPE;
	// SimpleAttackManager.java:56-90, the `else` arm: turn towards the target, then `npc.getController().attackTarget(target, 0, true)`. That one
	// statement is the whole point of the AI lane - it is what makes a monster fight back - and with item E-01b landed it runs all the way
	// through AttackUtil::calculatePhysAttackResult into CreatureController::onAttack and the target's life stats.
	runtime::Ref<Npc> npc = makeWorldNpc(SPARKIE_NPC_ID, 500, 500, 100);
	runtime::Ref<Npc> target = makeWorldNpc(GUARD_NPC_ID, 501, 500, 100); // 1 m: inside arange 2
	know(*npc, *target);
	HandlerTestAI& ai = installAi<HandlerTestAI>(*npc);
	// the target needs a real NpcAI too: NpcController::onAttack casts its own AI to NpcAI without a guard (NpcController.cpp:341, plan D15)
	installAi<HandlerTestAI>(*target);
	ai.setStateIfNot(AIState::FIGHT);
	npc->getAggroList().addHate(*target, 10);
	npc->setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(*target));
	ASSERT_TRUE(SimpleAttackManager::isTargetInAttackRange(*npc));
	ASSERT_EQ(npc->getAggroList().getTarget(controllers::attack::AggroTarget::MOST_HATED).rawPointer(), static_cast<Creature*>(target.get()));
	ASSERT_TRUE(npc->isSpawned());
	ASSERT_FALSE(npc->isDead());
	ASSERT_FALSE(npc->getLifeStats()->isAboutToDie());
	ASSERT_TRUE(npc->canAttack());

	const int32_t attacksBefore = npc->getGameStats()->getAttackCounter();
	const int32_t targetHpBefore = target->getLifeStats()->getCurrentHp();

	SimpleAttackManager::attackAction(ai);

	EXPECT_EQ(npc->getGameStats()->getAttackCounter(), attacksBefore + 1) << "CreatureController::attackTarget: increaseAttackCounter";
	EXPECT_LT(target->getLifeStats()->getCurrentHp(), targetHpBefore) << "and the target lost the damage of the swing";
	EXPECT_TRUE(target->getAggroList().isHating(*npc)) << "CreatureController::onAttack -> AggroList::addDamage on the target";
	EXPECT_EQ(npc->getPosition()->getHeading(), utils::PositionUtil::getHeadingTowards(*npc, *target))
		<< "the npc turned towards its target first (SimpleAttackManager.java:86)";

	// the guard in front of everything: outside FIGHT the body returns at once
	runtime::Ref<Npc> quiet = makeWorldNpc(SPARKIE_NPC_ID, 520, 500, 100);
	runtime::Ref<Npc> quietTarget = makeWorldNpc(GUARD_NPC_ID, 521, 500, 100);
	know(*quiet, *quietTarget);
	HandlerTestAI& quietAi = installAi<HandlerTestAI>(*quiet);
	installAi<HandlerTestAI>(*quietTarget);
	quiet->getAggroList().addHate(*quietTarget, 10); // this alone puts the npc into FIGHT through onAddHate -> AttackEventHandler::onAttack
	quiet->setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(*quietTarget));
	quietAi.setStateIfNot(AIState::IDLE); // and this takes it back out again, with everything else left in place
	const int32_t quietHpBefore = quietTarget->getLifeStats()->getCurrentHp();
	const int32_t quietAttacksBefore = quiet->getGameStats()->getAttackCounter();
	SimpleAttackManager::attackAction(quietAi);
	EXPECT_EQ(quietTarget->getLifeStats()->getCurrentHp(), quietHpBefore) << "attackAction returns at once outside FIGHT";
	EXPECT_EQ(quiet->getGameStats()->getAttackCounter(), quietAttacksBefore);
}

// ---- CreatureEventHandler::onCreatureSee -----------------------------------------------------------------------------------------------------

TEST_F(AiHandlerBodiesTest, SeeingTheLostTargetAgainResumesTheFightInsteadOfCheckingAggro) {
	AI_TEST_SCOPE;
	// CreatureEventHandler.java:36-44: the block that ends a hide. AttackManager::targetTooFar parks a chasing npc in the TARGET_LOST sub state
	// and arms a 2 s TARGET_GIVEUP; seeing the same creature again has to cancel that by clearing the sub state and scheduling the next attack.
	runtime::Ref<Npc> npc = makeWorldNpc(SPARKIE_NPC_ID, 500, 500, 100);
	runtime::Ref<Npc> target = makeWorldNpc(GUARD_NPC_ID, 501, 500, 100);
	know(*npc, *target);
	HandlerTestAI& ai = installAi<HandlerTestAI>(*npc);
	ai.setStateIfNot(AIState::FIGHT);
	npc->setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(*target));
	ASSERT_TRUE(ai.setSubStateIfNot(AISubState::TARGET_LOST));

	CreatureEventHandler::onCreatureSee(ai, *target);

	EXPECT_TRUE(ai.isInSubState(AISubState::NONE));
	EXPECT_TRUE(npc->getGameStats()->isNextAttackScheduled()) << "AttackManager::scheduleNextAttack: the fight continues";
	EXPECT_TRUE(ai.calls.empty()) << "the block returns, so checkAggro never runs";

	// a creature that is not the lost target falls through to checkAggro (the owner is in FIGHT, so checkAggro returns at its own first line)
	runtime::Ref<Npc> other = makeWorldNpc(GUARD_NPC_ID, 503, 500, 100);
	know(*npc, *other);
	ASSERT_TRUE(ai.setSubStateIfNot(AISubState::TARGET_LOST));
	CreatureEventHandler::onCreatureSee(ai, *other);
	EXPECT_TRUE(ai.isInSubState(AISubState::TARGET_LOST)) << "the sub state belongs to the target, not to anything the npc sees";
}

TEST_F(AiHandlerBodiesTest, SeeingTheLostTargetOutsideAFightOnlyClearsTheSubState) {
	AI_TEST_SCOPE;
	// the same block with the inner `if (npcAI.isInState(AIState.FIGHT))` false: the sub state is cleared and the event goes on to checkAggro
	runtime::Ref<Npc> npc = makeWorldNpc(SPARKIE_NPC_ID, 500, 500, 100);
	runtime::Ref<Npc> target = makeWorldNpc(GUARD_NPC_ID, 501, 500, 100);
	know(*npc, *target);
	HandlerTestAI& ai = installAi<HandlerTestAI>(*npc);
	ai.setStateIfNot(AIState::IDLE);
	npc->setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(*target));
	ASSERT_TRUE(ai.setSubStateIfNot(AISubState::TARGET_LOST));

	CreatureEventHandler::onCreatureSee(ai, *target);

	EXPECT_TRUE(ai.isInSubState(AISubState::NONE));
	EXPECT_FALSE(npc->getGameStats()->isNextAttackScheduled()) << "no fight to continue";
	EXPECT_EQ(ai.calls, (std::vector<std::string>{"detected", "aggro"})) << "checkAggro ran: a GUARD in see range of a MONSTER";
}

// ---- TargetEventHandler ----------------------------------------------------------------------------------------------------------------------

TEST_F(AiHandlerBodiesTest, TargetGiveupStopsHatingTheTargetAndClearsTheLostSubState) {
	AI_TEST_SCOPE;
	// TargetEventHandler.java:85-100. Giving a target up is how a fight ends without a death, and `stopHating` is what stops
	// ThinkEventHandler::thinkAttack from scheduling the next swing against the same creature for ever.
	runtime::Ref<Npc> npc = makeWorldNpc(SPARKIE_NPC_ID, 500, 500, 100);
	runtime::Ref<Npc> target = makeWorldNpc(GUARD_NPC_ID, 501, 500, 100);
	know(*npc, *target);
	HandlerTestAI& ai = installAi<HandlerTestAI>(*npc);
	ai.setStateIfNot(AIState::FIGHT);
	npc->getAggroList().addHate(*target, 10);
	npc->setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(*target));
	// 30 m from its spawn point, so the think() at the end of the body fires NOT_AT_HOME and not the BACK_HOME of the case below
	npc->getPosition()->setXYZH(530.0f, std::nullopt, std::nullopt, std::nullopt);
	ASSERT_TRUE(ai.setSubStateIfNot(AISubState::TARGET_LOST));
	ASSERT_TRUE(npc->getAggroList().isHating(*target));

	TargetEventHandler::onTargetGiveup(ai);

	EXPECT_FALSE(npc->getAggroList().isHating(*target)) << "AggroList::stopHating";
	EXPECT_TRUE(ai.isInSubState(AISubState::NONE)) << "the TARGET_LOST sub state is dropped with the target";
	EXPECT_TRUE(ai.isInState(AIState::RETURNING)) << "think() found a target it no longer hates and sent the npc home";
}

TEST_F(AiHandlerBodiesTest, TargetChangeOnlyRetargetsWhileFighting) {
	AI_TEST_SCOPE;
	// TargetEventHandler.java:102-110: the whole body is behind `if (npcAI.isInState(AIState.FIGHT))`. SimpleAttackManager::attackAction fires
	// TARGET_CHANGED for the most hated creature on every swing, so the gate walks this line once per attack.
	runtime::Ref<Npc> npc = makeWorldNpc(SPARKIE_NPC_ID, 500, 500, 100);
	runtime::Ref<Npc> first = makeWorldNpc(GUARD_NPC_ID, 501, 500, 100);
	runtime::Ref<Npc> second = makeWorldNpc(GUARD_NPC_ID, 501, 501, 100); // 1 m away too, so the swing is scheduled and not carried out on the spot
	know(*npc, *first);
	know(*npc, *second);
	HandlerTestAI& ai = installAi<HandlerTestAI>(*npc);
	ai.setStateIfNot(AIState::IDLE);
	npc->setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(*first));

	TargetEventHandler::onTargetChange(ai, *second);
	EXPECT_TRUE(npc->isTargeting(first->getObjectId())) << "outside FIGHT the event is dropped";
	EXPECT_FALSE(npc->getGameStats()->isNextAttackScheduled());

	ai.setStateIfNot(AIState::FIGHT);
	TargetEventHandler::onTargetChange(ai, *second);
	EXPECT_TRUE(npc->isTargeting(second->getObjectId()));
	EXPECT_TRUE(npc->getGameStats()->isNextAttackScheduled());
}

// ---- WalkManager::stopWalking ----------------------------------------------------------------------------------------------------------------

TEST_F(AiHandlerBodiesTest, StopWalkingPutsAWalkingNpcBackToIdleAndKeepsAFreeze) {
	AI_TEST_SCOPE;
	// WalkManager.java:213-219. It is the first thing AttackEventHandler::onAttack does to a walking npc and the first thing
	// ThinkEventHandler::thinkInInactiveRegion does when the last player leaves the region.
	runtime::Ref<Npc> npc = makeWorldNpc(SPARKIE_NPC_ID, 500, 500, 100);
	HandlerTestAI& ai = installAi<HandlerTestAI>(*npc);
	ASSERT_TRUE(ai.setStateIfNot(AIState::WALKING));
	ASSERT_TRUE(ai.setSubStateIfNot(AISubState::WALK_RANDOM));
	npc->setState(CreatureState::WALK_MODE); // Java: EmoteManager.emoteStartWalking set it when the walk began

	WalkManager::stopWalking(ai);

	EXPECT_TRUE(ai.isInState(AIState::IDLE));
	EXPECT_TRUE(ai.isInSubState(AISubState::NONE));
	EXPECT_FALSE(npc->isInState(CreatureState::WALK_MODE)) << "EmoteManager::emoteStopWalking";

	// the FREEZE sub state is the one exception (WalkManager.java:216-217)
	ASSERT_TRUE(ai.setStateIfNot(AIState::WALKING));
	ASSERT_TRUE(ai.setSubStateIfNot(AISubState::FREEZE));
	WalkManager::stopWalking(ai);
	EXPECT_TRUE(ai.isInState(AIState::IDLE));
	EXPECT_TRUE(ai.isInSubState(AISubState::FREEZE)) << "a frozen npc keeps its sub state";
}

// ---- ActivateEventHandler::onActivate --------------------------------------------------------------------------------------------------------

TEST_F(AiHandlerBodiesTest, ActivateOnlyMakesAnIdleNpcThink) {
	AI_TEST_SCOPE;
	// ActivateEventHandler.java:12-17. MapRegion::activate fires ACTIVATE for every creature of the region when the first player enters, so this
	// is the body that wakes 83,872 npcs up; `if (isInState(IDLE))` is what keeps it from disturbing a fight.
	runtime::Ref<Npc> npc = makeWorldNpc(SPARKIE_NPC_ID, 500, 500, 100);
	HandlerTestAI& ai = installAi<HandlerTestAI>(*npc);
	ai.setStateIfNot(AIState::IDLE);
	npc->getPosition()->setH(int8_t{42}); // the spawn heading is 0, so ThinkEventHandler::thinkIdle schedules the 500 ms reset
	executor->runReady();
	ASSERT_EQ(executor->pendingTasksCount(), 0u);

	ActivateEventHandler::onActivate(ai);

	EXPECT_EQ(executor->pendingTasksCount(), 1u) << "think() reached ThinkEventHandler::thinkIdle";
	executor->advance(std::chrono::milliseconds(500));
	EXPECT_EQ(npc->getPosition()->getHeading(), npc->getSpawn()->getHeading());

	// every other state drops the event
	npc->getPosition()->setH(int8_t{42});
	ai.setStateIfNot(AIState::FIGHT);
	ActivateEventHandler::onActivate(ai);
	EXPECT_EQ(executor->pendingTasksCount(), 0u) << "an npc in a fight is not made to think by an ACTIVATE";
	EXPECT_EQ(npc->getPosition()->getHeading(), int8_t{42});
}

// ---- ReturningEventHandler::onBackHome -------------------------------------------------------------------------------------------------------

/** The hits of the AION_PARTIAL sites whose function contains `function` since the last resetPartialHitsForTests() */
uint64_t partialHitsOf(std::string_view function) {
	uint64_t hits = 0;
	for (const runtime::PartialHit& hit : runtime::partialHits())
		if (hit.function.find(function) != std::string::npos)
			hits += hit.hits;
	return hits;
}

/** Java Skill.applyEffect for one effected creature: new Effect(effector, effected, template, 1), initialize(), addToEffectedController() */
runtime::Ref<skillengine::model::Effect> putOn(Npc& npc, const skillengine::model::SkillTemplate* skill) {
	runtime::Ref<skillengine::model::Effect> effect = skillengine::model::Effect::create(npc, runtime::Ptr<Creature>(npc), skill, 1);
	effect->initialize();
	effect->addToEffectedController();
	return effect;
}

/**
 * One post-spawn skill of one npc id for the life of a case: SKILL_DATA holds the probe skill `skillId` - the shape of the post-spawn 19125
 * Dragon's Blessing (skill_templates.xml: an instant BUFF with first_target TARGET, first_target_range 17, target_relation FRIEND, which
 * Properties.endCastValidate needs to put the npc on the effected list) whose one probe "home" lasts a minute and puts its Effect on the effected
 * as BufEffect.applyEffect does - and NPC_SKILL_DATA gives `npcId` that skill with is_post_spawn="true". Both are published before the npc is
 * created, because the Npc constructor builds its NpcSkillList (NpcSkillList.java:27-49).
 */
class PostSpawnSkillData {
public:
	PostSpawnSkillData(int32_t skillId, int32_t npcId, skillengine::effecttest::Journal& journal, xml::LoadContext& skillContext,
		xml::LoadContext& npcSkillContext) {
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext,
			R"(<skill_data><skill_template skill_id=")" + std::to_string(skillId) + R"(" name="home" nameId="1" stack="HOME)"
				+ std::to_string(skillId) + R"(" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF" activation="ACTIVE" duration="0">)"
				+ R"(<properties first_target="TARGET" first_target_range="17" target_relation="FRIEND" target_type="ONLYONE" target_maxcount="1"/>)"
				+ R"(</skill_template></skill_data>)"));
		std::unique_ptr<skillengine::effecttest::ProbeEffect> home = skillengine::effecttest::probe("home", &journal, 1);
		home->duration2 = 60000;
		home->onApply = [](skillengine::model::Effect& effect) { effect.addToEffectedController(); };
		skillengine::effecttest::injectProbes(dataholders::DataManager::SKILL_DATA->getSkillTemplate(skillId),
			skillengine::effecttest::probeList(std::move(home)));
		dataholders::DataManager::NPC_SKILL_DATA.resetForTests(); // the fixture published an empty one
		dataholders::DataManager::NPC_SKILL_DATA.publish(xml::bindString<dataholders::NpcSkillData>(npcSkillContext,
			R"(<npc_skill_templates><npc_skills npc_ids=")" + std::to_string(npcId) + R"("><npc_skill id=")" + std::to_string(skillId)
				+ R"(" lv="1" prob="100" prio="1" is_post_spawn="true"/></npc_skills></npc_skill_templates>)"));
	}
	~PostSpawnSkillData() { dataholders::DataManager::SKILL_DATA.resetForTests(); } // AiTest::TearDown forgets NPC_SKILL_DATA
	PostSpawnSkillData(const PostSpawnSkillData&) = delete;
	PostSpawnSkillData& operator=(const PostSpawnSkillData&) = delete;
};

/** A probe template of one skill (EffectTestSupport.h) that lasts a minute, in the target slot `tslot`, dispellable at level 1 */
std::unique_ptr<skillengine::model::SkillTemplate> fightEffect(int32_t skillId, std::string_view tslot, std::string name,
	skillengine::effecttest::Journal& journal) {
	std::unique_ptr<skillengine::effecttest::ProbeEffect> probe = skillengine::effecttest::probe(std::move(name), &journal, 1);
	probe->duration2 = 60000;
	return skillengine::effecttest::skillWithProbes(
		skillengine::effecttest::skillXml(skillId, "HOME" + std::to_string(skillId),
			R"(tslot=")" + std::string(tslot) + R"(" req_dispel_level="1" req_dispel_count="10")"),
		skillengine::effecttest::probeList(std::move(probe)));
}

TEST_F(AiHandlerBodiesTest, BackHomePutsTheNpcBackToIdleAndAsksForTheBuffsOfTheFightToBeDropped) {
	AI_TEST_SCOPE;
	// ReturningEventHandler.java:47-64. The IDLE block: setSubStateIfNot(NONE), then `getEffectController().removeByDispelSlotType(
	// DispelSlotType.BUFF)` - the M5b-1 AION_PARTIAL, ported by M5b-2 part 2 (m5b2-plan.md K-02): removeByDispelEffect(null, BUFF, 255, 100, 100)
	// (EffectController.java:427-429, 450-485) ends every effect of the BUFF slot whose req_dispel_level is at most 100 and whose
	// req_dispel_count the 100 power covers. So a buff the npc gave itself in the fight is dropped on the way home, and a debuff is not.
	// Then, after think(), the npc casts its post-spawn skills again (ReturningEventHandler.java:58-60): NpcSkillList::getPostSpawnSkills is
	// Java's filter since M5b-2 part 3 (m5b2-plan.md D7, D11), so this npc's one post-spawn skill, a probe (9703), is cast on itself.
	const PostSpawnSkillData postSpawn(9703, SPARKIE_NPC_ID, journal, postSpawnSkillContext, postSpawnNpcSkillContext);
	runtime::Ref<Npc> npc = makeWorldNpc(SPARKIE_NPC_ID, 500, 500, 100);
	ASSERT_EQ(npc->getSkillList()->getNpcSkills()->size(), 1) << "the npc's skill list is built from the NPC_SKILL_DATA published above";
	HandlerTestAI& ai = installAi<HandlerTestAI>(*npc);
	ASSERT_TRUE(ai.setStateIfNot(AIState::RETURNING));
	ASSERT_TRUE(ai.setSubStateIfNot(AISubState::WALK_PATH));
	const skillengine::model::SkillTemplate* buff = keep(fightEffect(9701, "BUFF", "buff", journal));
	const skillengine::model::SkillTemplate* debuff = keep(fightEffect(9702, "DEBUFF", "debuff", journal));
	putOn(*npc, buff);
	putOn(*npc, debuff);
	ASSERT_TRUE(npc->getEffectController()->hasAbnormalEffect(9701));
	ASSERT_TRUE(npc->getEffectController()->hasAbnormalEffect(9702));
	journal.clear();

	runtime::resetUnportedHitsForTests();
	runtime::resetPartialHitsForTests();
	EXPECT_NO_THROW(ReturningEventHandler::onBackHome(ai));

	EXPECT_TRUE(ai.isInState(AIState::IDLE)) << "setStateIfNot(IDLE) ran";
	EXPECT_TRUE(ai.isInSubState(AISubState::NONE)) << "and setSubStateIfNot(NONE) with it";
	EXPECT_EQ(journal, (skillengine::effecttest::Journal{"buff.end", "home.calculate", "home.apply", "home.start"}))
		<< "the buff of the fight is dropped, the debuff is not; then the post-spawn skill is cast, after the dispel";
	EXPECT_FALSE(npc->getEffectController()->hasAbnormalEffect(9701));
	EXPECT_TRUE(npc->getEffectController()->hasAbnormalEffect(9702)) << "another slot";
	EXPECT_TRUE(npc->getEffectController()->hasAbnormalEffect(9703)) << "the post-spawn buff, cast on the npc itself";
	EXPECT_EQ(runtime::unportedHitCount(), 0u) << "walking home may not reach an unported body any more";
	EXPECT_EQ(partialHitsOf("removeByDispelSlotType"), 0u) << "the M5b-1 partial is closed";
	for (const runtime::PartialHit& hit : runtime::partialHits())
		EXPECT_EQ(hit.hits, 0u) << "no partial on the way home, but it reached " << hit.function;

	// the whole block is behind setStateIfNot(IDLE): a second BACK_HOME does nothing at all, not even ask the effect controller
	putOn(*npc, buff);
	journal.clear();
	runtime::resetPartialHitsForTests();
	EXPECT_NO_THROW(ReturningEventHandler::onBackHome(ai));
	EXPECT_TRUE(journal.empty()) << "the second call returns at setStateIfNot(IDLE), before the dispel";
	EXPECT_TRUE(npc->getEffectController()->hasAbnormalEffect(9701));
	// partialHits() keeps one row per site for the whole process; resetPartialHitsForTests() zeroes the counts, so "not reached" is hits == 0
	for (const runtime::PartialHit& hit : runtime::partialHits())
		EXPECT_EQ(hit.hits, 0u) << "the second call returns at setStateIfNot(IDLE), but it reached " << hit.function;
	EXPECT_TRUE(ai.isInState(AIState::IDLE));
	npc->getEffectController()->removeAllEffects(true); // an effect holds its effector: the npc -> effect -> npc cycle is cut here
}

// ---- HpPhases::tryEnterNextPhase -------------------------------------------------------------------------------------------------------------

TEST_F(AiHandlerBodiesTest, HpPhasesAreEnteredOnceEachInDescendingOrder) {
	AI_TEST_SCOPE;
	// HpPhases.java:24-45. The percentages a boss changes its behaviour at; the constructor sorts them descending and drops duplicates, and
	// tryEnterNextPhase enters at most one of them per call and each of them at most once.
	runtime::Ref<Npc> npc = makeWorldNpc(SPARKIE_NPC_ID, 500, 500, 100); // maxHp 199
	PhasedAI& ai = installAi<PhasedAI>(*npc);
	ai.setStateIfNot(AIState::IDLE);
	runtime::Ref<HpPhases> phases = HpPhases::create(25, {75, 50, 25});
	EXPECT_EQ(phases->getCurrentPhase(), 0);

	phases->tryEnterNextPhase(ai);
	EXPECT_TRUE(ai.phases.empty()) << "full HP is above every phase";
	EXPECT_EQ(phases->getCurrentPhase(), 0);

	npc->getLifeStats()->setCurrentHp(149); // 74 %
	phases->tryEnterNextPhase(ai);
	EXPECT_EQ(ai.phases, (std::vector<int32_t>{75})) << "the constructor sorted 25, 75, 50, 25 into 75, 50, 25";
	EXPECT_EQ(phases->getCurrentPhase(), 1);
	phases->tryEnterNextPhase(ai);
	EXPECT_EQ(ai.phases, (std::vector<int32_t>{75})) << "50 is not reached yet, and 75 is spent";

	npc->getLifeStats()->setCurrentHp(49); // 24 %: two phases at once, but one call enters one phase
	phases->tryEnterNextPhase(ai);
	EXPECT_EQ(ai.phases, (std::vector<int32_t>{75, 50}));
	phases->tryEnterNextPhase(ai);
	EXPECT_EQ(ai.phases, (std::vector<int32_t>{75, 50, 25}));
	phases->tryEnterNextPhase(ai);
	EXPECT_EQ(ai.phases, (std::vector<int32_t>{75, 50, 25})) << "the list is exhausted";
	EXPECT_EQ(phases->getCurrentPhase(), 3);

	// reset() is what a respawn calls
	phases->reset();
	EXPECT_EQ(phases->getCurrentPhase(), 0);
	phases->tryEnterNextPhase(ai);
	EXPECT_EQ(ai.phases, (std::vector<int32_t>{75, 50, 25, 75}));

	// the three guards of the first line (HpPhases.java:32-33)
	phases->reset();
	ai.setStateIfNot(AIState::RETURNING);
	phases->tryEnterNextPhase(ai);
	EXPECT_EQ(phases->getCurrentPhase(), 0) << "a returning npc does not change phase";
	ai.setStateIfNot(AIState::IDLE);
	npc->getPosition()->setIsSpawned(false);
	phases->tryEnterNextPhase(ai);
	EXPECT_EQ(phases->getCurrentPhase(), 0) << "a despawned npc does not change phase";
	npc->getPosition()->setIsSpawned(true);
	phases->tryEnterNextPhase(ai);
	EXPECT_EQ(phases->getCurrentPhase(), 1) << "every guard was undone";
}

// ---- NpcAI::handleSpawned --------------------------------------------------------------------------------------------------------------------

TEST_F(AiHandlerBodiesTest, HandleSpawnedRunsTheSpawnHandlerAndThenTheShoutHandler) {
	AI_TEST_SCOPE;
	// NpcAI.java:122-125 is two calls, and with the M5b profile's `gameserver.npcshouts.enable=false` the second one is invisible: it asks
	// AIQuestion.CAN_SHOUT, which short-circuits on the config. Switching shouts on makes it visible, because the other half of CAN_SHOUT is
	// NpcShoutsService::mayShout, which is AION_UNPORTED (P5-14).
	runtime::Ref<Npc> npc = makeWorldNpc(SPARKIE_NPC_ID, 500, 500, 100);
	HandlerTestAI& ai = installAi<HandlerTestAI>(*npc);
	ASSERT_FALSE(configs::main::AIConfig::SHOUTS_ENABLE.load()) << "the M5b profile (m5b-plan.md D1)";

	ai.handleSpawned();
	EXPECT_TRUE(ai.isInState(AIState::IDLE)) << "SpawnEventHandler::onSpawn";

	runtime::Ref<Npc> second = makeWorldNpc(SPARKIE_NPC_ID, 505, 500, 100);
	HandlerTestAI& secondAi = installAi<HandlerTestAI>(*second);
	ShoutsEnabledScope shouts(true);
	runtime::resetUnportedHitsForTests();
	EXPECT_THROW(secondAi.handleSpawned(), runtime::UnportedException) << "ShoutEventHandler::onSpawn was called";
	EXPECT_TRUE(secondAi.isInState(AIState::IDLE)) << "and it was called after SpawnEventHandler::onSpawn, not before";
	bool reachedTheShoutService = false;
	for (const runtime::UnportedHit& hit : runtime::unportedHits()) {
		if (hit.hits > 0 && hit.function.find("mayShout") != std::string::npos)
			reachedTheShoutService = true;
	}
	EXPECT_TRUE(reachedTheShoutService);
}

// ---- TalkEventHandler::onTalk ----------------------------------------------------------------------------------------------------------------

TEST_F(AiHandlerBodiesTest, TalkPutsADialogNpcIntoTheTalkSubStateAndTargetsTheTalker) {
	AI_TEST_SCOPE;
	// TalkEventHandler.java:26-56, its first statement `onSimpleTalk(npcAI, creature)` and the `isDialogNpc()` guard inside it. The rest of
	// onTalk is the Player half - QuestEngine.onDialog, TownService and SM_DIALOG_WINDOW - which needs a client session and belongs to the
	// scenario gate, so this case pins the half a unit test can reach.
	runtime::Ref<Npc> villager = makeWorldNpc(DIALOG_NPC_ID, 500, 500, 100);
	runtime::Ref<Npc> visitor = makeWorldNpc(GUARD_NPC_ID, 501, 500, 100);
	HandlerTestAI& ai = installAi<HandlerTestAI>(*villager);
	ai.setStateIfNot(AIState::IDLE);
	ASSERT_TRUE(villager->getObjectTemplate()->isDialogNpc());

	handler::TalkEventHandler::onTalk(ai, *visitor);

	EXPECT_TRUE(ai.isInSubState(AISubState::TALK));
	EXPECT_TRUE(villager->isTargeting(visitor->getObjectId()));

	// and the guard: an npc without a talk_info is left alone
	runtime::Ref<Npc> sparkie = makeWorldNpc(SPARKIE_NPC_ID, 505, 500, 100);
	HandlerTestAI& sparkieAi = installAi<HandlerTestAI>(*sparkie);
	sparkieAi.setStateIfNot(AIState::IDLE);
	ASSERT_FALSE(sparkie->getObjectTemplate()->isDialogNpc());
	handler::TalkEventHandler::onTalk(sparkieAi, *visitor);
	EXPECT_TRUE(sparkieAi.isInSubState(AISubState::NONE));
	EXPECT_FALSE(sparkie->getTarget());

	// onFinishTalk is the other half of the pair: it drops the target it took
	handler::TalkEventHandler::onFinishTalk(ai, *visitor);
	EXPECT_FALSE(villager->getTarget());
}

// ---- AIActions::deleteOwner ------------------------------------------------------------------------------------------------------------------

TEST_F(AiHandlerBodiesTest, DeleteOwnerTakesTheNpcOutOfTheWorld) {
	AI_TEST_SCOPE;
	// AIActions.java:24-26, `ai.getOwner().getController().delete()`. It is one line, but it is the line NpcController::onDie takes when the AI
	// answers ALLOW_DECAY false, and a body that dropped it would leave a corpse in the world for ever.
	runtime::Ref<Npc> npc = makeWorldNpc(SPARKIE_NPC_ID, 500, 500, 100);
	HandlerTestAI& ai = installAi<HandlerTestAI>(*npc);
	world::World::getInstance().storeObject(*npc); // Java: VisibleObjectSpawner does this before World.spawn
	ASSERT_TRUE(npc->isSpawned());
	ASSERT_EQ(world::World::getInstance().findVisibleObject(npc->getObjectId()).get(), static_cast<model::gameobjects::VisibleObject*>(npc.get()));

	AIActions::deleteOwner(ai);

	EXPECT_FALSE(npc->isSpawned()) << "World::removeObject despawned it on the way out";
	EXPECT_FALSE(world::World::getInstance().findVisibleObject(npc->getObjectId()));
}

} // namespace
} // namespace aion::gameserver::ai::testing
