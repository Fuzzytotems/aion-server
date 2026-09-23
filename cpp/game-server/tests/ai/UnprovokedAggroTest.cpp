// M5b-1 stage 2 (P5-05, m5b-plan.md §6.3 A5 / §6.7 G-1): an aggressive NPC starting a fight with a character that has not touched it.
//
// This is the one path no earlier test and no real-client session could see, because every one of them started the fight with a player attack,
// and being hit puts the attacker on the hate list through AggroList::addDamage - a different path. What this file drives is the other one, end
// to end and through the real delivery machinery:
//
//   the character stops moving -> CreatureController::onStopMove -> MovementNotifyTask (a 500 ms FIFO periodic task manager)
//   -> CREATURE_MOVED on every npc of the character's known list -> AbstractAI::canHandleEvent (IDLE or WALKING only)
//   -> NpcAI::handleCreatureMoved -> CreatureEventHandler::onCreatureMoved -> checkAggro -> CREATURE_AGGRO
//   -> AggroEventHandler::onAggro -> a 500 ms AggroNotifier -> AggroList::addHate(target, 1) -> NpcController::onAddHate
//   -> the ATTACK creature event -> AttackEventHandler::onAttack -> AIState::FIGHT + AttackManager::startAttacking
//
// **Which npc starts a fight is a tribe decision, not an `ai="aggressive"` decision**, and that is the finding the gate's A5 turned up.
// `ai="aggressive"` only picks the AI class that checks what it sees (AggressiveNpcAI overrides handleCreatureSee); whether it aggroes is
// `TribeRelationService.isAggressive(npc, creature)` (CreatureEventHandler.java:87), which for a player ends in
// `TRIBE_RELATIONS_DATA.isAggressiveRelation(npcTribe, PC)`. Tribe MONSTER has no `<aggro>` list at all in the real tribe_relations.xml
// (:2170-2173), so a MONSTER npc never starts a fight with a character - in Java exactly as in this port - while tribes like KRALL (:1592) and
// TOWERMAN (:2579) do. Of Poeta's 72 `ai="aggressive"` npc ids only 27 have a tribe that is aggressive to PC; npc 210663, the gate's monster,
// is not one of them. Both sides are asserted here, so neither can be broken without a red test.

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/ai/handler/AggroEventHandler.h"
#include "aion/gameserver/ai/handler/AttackEventHandler.h"
#include "aion/gameserver/ai/handler/CreatureEventHandler.h"
#include "aion/gameserver/ai/handler/TargetEventHandler.h"
#include "aion/gameserver/ai/handler/ThinkEventHandler.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/services/TribeRelationService.h"
#include "aion/gameserver/taskmanager/tasks/MovementNotifyTask.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "AiWorldTestSupport.h"

namespace aion::gameserver::ai::testing {
namespace {

using event::AIEventType;
using model::gameobjects::Creature;
using model::gameobjects::Npc;
using model::gameobjects::player::Player;

/**
 * The hooks AggressiveNpcAI and GeneralNpcAI wire for this chain, wired the same way (AggressiveNpcAI.cpp:10-22, GeneralNpcAI.cpp:21-80).
 * The handler chunk is not linked into this executable, so the leaf is the test's; every body it calls is the shipped one. `calls` records the
 * two hooks checkAggro reaches plus the ATTACK event, so a failure says *where* the chain stopped instead of only that it stopped.
 */
class AggressiveTestAI final : public NpcAI {
public:
	explicit AggressiveTestAI(Npc& owner) : NpcAI(owner) {}

	void think() override { handler::ThinkEventHandler::onThink(*this); }

	std::vector<std::string> calls;

	void handleCreatureDetected(Creature& creature) override { calls.push_back("detected"); }

protected:
	void handleCreatureSee(Creature& creature) override {
		calls.push_back("see");
		handler::CreatureEventHandler::onCreatureSee(*this, creature);
	}

	void handleCreatureMoved(Creature& creature) override {
		calls.push_back("moved");
		NpcAI::handleCreatureMoved(creature);
	}

	void handleCreatureAggro(Creature& creature) override {
		calls.push_back("aggro");
		if (canThink())
			handler::AggroEventHandler::onAggro(*this, creature);
	}

	void handleAttack(runtime::Ptr<Creature> creature) override {
		calls.push_back("attack");
		handler::AttackEventHandler::onAttack(*this, creature);
	}

	void handleTargetChanged(Creature& creature) override {
		NpcAI::handleTargetChanged(creature);
		handler::TargetEventHandler::onTargetChange(*this, creature);
	}
};

class UnprovokedAggroTest : public AiWorldTest {
protected:
	void SetUp() override {
		AiWorldTest::SetUp();
		AI_TEST_SCOPE;
		// Java: a map region is only active while a player is in it, and checkAggro returns early otherwise
		regionActivator = activateRegionAt(500, 500, 100);
		executor->runReady(); // MapRegion::activate posts the ACTIVATE notification; drain it before the cases count tasks
	}

	AggressiveTestAI& installAi(Npc& npc) {
		auto ai = std::make_unique<AggressiveTestAI>(npc);
		AggressiveTestAI& result = *ai;
		npc.replaceAi(std::move(ai));
		return result;
	}

	/** an npc of `npcId` 2 m from a character, both in the active region and in each other's known list, the npc idle with the test AI */
	AggressiveTestAI& standCharacterNextTo(int32_t npcId, runtime::Ref<Npc>& npc, runtime::Ref<Player>& player, int32_t playerObjectId) {
		npc = makeWorldNpc(npcId, 500, 500, 100);
		player = makeWorldPlayer(playerObjectId, 502, 500, 100); // 2 m: inside srange 8 and inside the 4 m short aggro range
		know(*npc, *player);                                     // Java: the knownlist update pairs them as the character walks up
		AggressiveTestAI& ai = installAi(*npc);
		ai.setStateIfNot(AIState::IDLE);
		return ai;
	}

	/**
	 * What the server does when a character stops next to an npc: `CM_MOVE`'s stop reaches `CreatureController::onStopMove`, which queues the
	 * character on `MovementNotifyTask` (`CreatureController.cpp:191-198`, P4-11b), whose 500 ms tick then calls `notifyCreatureMoved` for
	 * every npc of the character's known list. The tick cannot drive a unit case: `MovementNotifyTask` is a process-wide singleton whose
	 * fixed-rate task binds to the executor that existed when it was first used, so whether it fires at all depends on which test of the
	 * binary ran first - which would make the sequences below order-dependent. The case therefore calls the tick's own body, the shipped one
	 * with its DIED guard and its try/catch, exactly as `callTask` calls it.
	 */
	void characterMovesNextTo(Npc& npc, Player& player) {
		taskmanager::tasks::MovementNotifyTask::getInstance().notifyCreatureMoved(npc, player);
	}

	runtime::Ref<Player> regionActivator;
};

/**
 * The assertion §6.3 A5 makes, as a unit case: a character stops next to an aggressive npc, touches nothing, the clock runs, and the npc ends
 * up hating it and swinging at it. Every step is the shipped code; only the AI leaf and the clock belong to the test.
 */
TEST_F(UnprovokedAggroTest, AnAggressiveNpcAggroesAndAttacksACharacterThatStopsNextToIt) {
	AI_TEST_SCOPE;
	runtime::Ref<Npc> npc;
	runtime::Ref<Player> player;
	AggressiveTestAI& ai = standCharacterNextTo(AGGRO_TO_PC_NPC_ID, npc, player, 700001);

	// the four terms of CreatureEventHandler.java:85-88, so a failure below names the one that changed
	ASSERT_TRUE(npc->getPosition()->isMapRegionActive());
	ASSERT_TRUE(utils::PositionUtil::isInRange(*npc, *player, static_cast<float>(npc->getShortAggroRange()), false)) << "inside the 4 m short range";
	ASSERT_TRUE(services::TribeRelationService::isAggressive(*npc, *player)) << "tribe KRALL is aggressive to PC (tribe_relations.xml:1592-1593)";
	ASSERT_FALSE(services::TribeRelationService::isFriend(*npc, *player));
	ASSERT_TRUE(player->isEnemyFrom(*npc));

	// the character stops moving and touches nothing - which is the whole of the scenario gate's K4b before it waits
	ai.calls.clear();
	characterMovesNextTo(*npc, *player);
	EXPECT_FALSE(npc->getAggroList().isHating(*player)) << "the AggroNotifier runs 500 ms later (AggroEventHandler.java:23)";

	executor->advance(std::chrono::milliseconds(600));

	EXPECT_EQ(ai.calls, (std::vector<std::string>{"moved", "detected", "aggro", "attack"}))
		<< "CREATURE_MOVED -> checkAggro -> CREATURE_AGGRO -> addHate -> ATTACK";
	EXPECT_TRUE(npc->getAggroList().isHating(*player)) << "AggroNotifier: addHate(target, 1)";
	EXPECT_EQ(npc->getAggroList().getHate(*player), 1);
	EXPECT_TRUE(ai.isInState(AIState::FIGHT)) << "AttackEventHandler::onAttack";
	EXPECT_TRUE(ai.isInSubState(AISubState::NONE));
	EXPECT_TRUE(npc->isTargeting(player->getObjectId())) << "AttackEventHandler::onAttack sets the target before startAttacking";
	EXPECT_GT(npc->getGameStats()->getFightStartingTime(), 0) << "AttackManager::startAttacking";
	EXPECT_TRUE(npc->isInState(model::gameobjects::state::CreatureState::WEAPON_EQUIPPED)) << "the npc drew its weapon when the fight began";
	// running the swing would reach standins::attackUtilCalculatePhysAttackResult (stage 2 item E-01b), so the assertion is that it is pending
	EXPECT_TRUE(npc->getGameStats()->isNextAttackScheduled());
}

/**
 * The other side of the same decision, and the explanation of m5b-plan.md §6.7 G-1: npc 210663, the gate's monster, has tribe MONSTER, and
 * MONSTER is aggressive to nothing in the real tribe_relations.xml. Java answers exactly the same, so this row is not a bug being pinned but
 * the behaviour being pinned: a MONSTER-tribe npc is *detected* (handleCreatureDetected runs, which is what feeds the instance handler's
 * onCreatureDetected) and the else branch shouts, and no CREATURE_AGGRO is ever fired.
 */
TEST_F(UnprovokedAggroTest, AMonsterTribeNpcDetectsACharacterButNeverStartsAFightWithIt) {
	AI_TEST_SCOPE;
	runtime::Ref<Npc> npc;
	runtime::Ref<Player> player;
	AggressiveTestAI& ai = standCharacterNextTo(SPARKIE_NPC_ID, npc, player, 700002);

	ASSERT_TRUE(utils::PositionUtil::isInRange(*npc, *player, static_cast<float>(npc->getShortAggroRange()), false))
		<< "the same 2 m as the case above: only the tribe differs";
	EXPECT_FALSE(services::TribeRelationService::isAggressive(*npc, *player))
		<< "tribe MONSTER has no <aggro> list in the real tribe_relations.xml (:2170-2173)";

	ai.calls.clear();
	characterMovesNextTo(*npc, *player);
	executor->advance(std::chrono::milliseconds(600));

	EXPECT_EQ(ai.calls, (std::vector<std::string>{"moved", "detected"})) << "seen and detected, but the aggro arm is not taken";
	EXPECT_FALSE(npc->getAggroList().isHating(*player));
	EXPECT_TRUE(ai.isInState(AIState::IDLE));
	EXPECT_FALSE(npc->getGameStats()->isNextAttackScheduled());

	// and it is not peaceful either - it is *hostile*, which is why it fights back the moment it is hit (TribeRelationService::isHostile's
	// hard-coded `baseTribe == MONSTER && PC` arm). That is the path every earlier test took, and it is unaffected by the row above.
	EXPECT_TRUE(player->isEnemyFrom(*npc)) << "getType(player) is ATTACKABLE, so the character may attack it and it answers";
	npc->getAggroList().addHate(*player, 10);
	EXPECT_TRUE(npc->getAggroList().isHating(*player)) << "being hit starts the fight; seeing the character does not";
	EXPECT_TRUE(ai.isInState(AIState::FIGHT));
}

/** CREATURE_SEE takes the same decision as CREATURE_MOVED: the pairing of the known lists is the other way into checkAggro. */
TEST_F(UnprovokedAggroTest, SeeingTheCharacterTakesTheSameDecisionAsSeeingItMove) {
	AI_TEST_SCOPE;
	runtime::Ref<Npc> aggressive;
	runtime::Ref<Player> nearAggressive;
	AggressiveTestAI& aggressiveAi = standCharacterNextTo(AGGRO_TO_PC_NPC_ID, aggressive, nearAggressive, 700003);
	runtime::Ref<Npc> monster;
	runtime::Ref<Player> nearMonster;
	AggressiveTestAI& monsterAi = standCharacterNextTo(SPARKIE_NPC_ID, monster, nearMonster, 700004);

	aggressiveAi.calls.clear();
	monsterAi.calls.clear();
	aggressive->getAi().onCreatureEvent(AIEventType::CREATURE_SEE, *nearAggressive);
	monster->getAi().onCreatureEvent(AIEventType::CREATURE_SEE, *nearMonster);
	executor->advance(std::chrono::milliseconds(1000));

	EXPECT_EQ(aggressiveAi.calls, (std::vector<std::string>{"see", "detected", "aggro", "attack"}));
	EXPECT_TRUE(aggressive->getAggroList().isHating(*nearAggressive));
	EXPECT_EQ(monsterAi.calls, (std::vector<std::string>{"see", "detected"}));
	EXPECT_FALSE(monster->getAggroList().isHating(*nearMonster));
}

} // namespace
} // namespace aion::gameserver::ai::testing
