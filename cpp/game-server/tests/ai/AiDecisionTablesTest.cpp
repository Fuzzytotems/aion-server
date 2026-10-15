// A-01 / A-02 (m5b-plan.md §4, P5-05): the two decision tables m5b-plan.md A-08 names by line number.
//
// 1. CreatureEventHandler::checkAggro (CreatureEventHandler.java:56-110) - the predicate chain that decides whether an NPC aggroes a creature
//    it sees: the AI state, the target's state, the see range (aggro range, aggro angle, short aggro range), the tribe relation and
//    validateAggro's level difference. Its two observable effects are handleCreatureDetected and the CREATURE_AGGRO creature event, which the
//    recording AI below counts.
// 2. AttackManager::checkGiveupDistance (AttackManager.java:104-129) - when a chasing NPC gives its target up: the map's chase_target and
//    chase_home (AiInfo defaults 50 and 200, 50 and 150 for a boss) and the "no attack for long enough" arm.
//
// The gate cannot reach most of these rows: gs.scenario.m5b scripts one approach, one fight and one giveup, and a decision table needs the
// rows it does not take. Both functions are called directly, which is why checkAggro is public in the port (Java: protected, package access
// for TargetEventHandler) and checkGiveupDistance is public (Java: private; the body is unchanged).

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/ai/handler/CreatureEventHandler.h"
#include "aion/gameserver/ai/manager/AttackManager.h"
#include "aion/gameserver/ai/manager/SimpleAttackManager.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/gameobjects/state/CreatureVisualState.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/TribeRelationService.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/MapRegion.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "AiWorldTestSupport.h"

namespace aion::gameserver::ai::testing {
namespace {

using handler::CreatureEventHandler;
using manager::AttackManager;
using manager::SimpleAttackManager;
using model::gameobjects::Creature;
using model::gameobjects::Npc;
using model::gameobjects::state::CreatureVisualState;
using utils::PositionUtil;

/** An NpcAI leaf that counts the two hooks checkAggro reaches. */
class AggroRecordingAI final : public NpcAI {
public:
	explicit AggroRecordingAI(Npc& owner) : NpcAI(owner) {}

	std::vector<std::string> calls;

	void handleCreatureDetected(Creature& creature) override { calls.push_back("detected"); }

protected:
	void handleCreatureAggro(Creature& creature) override { calls.push_back("aggro"); }
};

class AiDecisionTablesTest : public AiWorldTest {
protected:
	void SetUp() override {
		AiWorldTest::SetUp();
		AI_TEST_SCOPE;
		// Java: MapRegion.activate() runs when a player enters; checkAggro returns early while the owner's region is inactive
		regionActivator = activateRegionAt(500, 500, 100);
		owner = makeWorldNpc(SPARKIE_NPC_ID, 500, 500, 100);
		ai = &installRecordingAi(*owner);
		ASSERT_TRUE(owner->getPosition()->isMapRegionActive());
		ai->setStateIfNot(AIState::IDLE);
	}

	AggroRecordingAI& installRecordingAi(Npc& npc) {
		auto recording = std::make_unique<AggroRecordingAI>(npc);
		AggroRecordingAI& result = *recording;
		npc.replaceAi(std::move(recording));
		return result;
	}

	/** the calls checkAggro made for one creature, cleared before each row */
	std::vector<std::string> aggroCalls(Creature& creature) {
		ai->calls.clear();
		CreatureEventHandler::checkAggro(*ai, creature);
		return ai->calls;
	}

	runtime::Ref<model::gameobjects::player::Player> regionActivator;
	runtime::Ref<Npc> owner;
	AggroRecordingAI* ai = nullptr;
};

// ---- CreatureEventHandler::checkAggro --------------------------------------------------------------------------------------------------------

TEST_F(AiDecisionTablesTest, CheckAggroAggroesAHostileCreatureInSeeRange) {
	AI_TEST_SCOPE;
	runtime::Ref<Npc> guard = makeWorldNpc(GUARD_NPC_ID, 505, 500, 100); // 5 m, inside the aggro range of 8

	EXPECT_EQ(aggroCalls(*guard), (std::vector<std::string>{"detected", "aggro"}));
}

/**
 * Five of checkAggro's eight early returns (CreatureEventHandler.java:57-83). The two it leaves out are `creature.isDead()`, which needs a
 * real death (setCurrentHp(0) runs the whole NpcController::onDie path, whose reward arm is another chunk's stage-2 work), and
 * `creature.isFlag()`, which is false for every npc template these tests can build; `owner.getEffectController().isAbnormalSet(SANCTUARY)`
 * needs an effect, which is M5b-2.
 */
TEST_F(AiDecisionTablesTest, CheckAggroStopsAtTheStateVisualStateSpawnAndRegionGates) {
	AI_TEST_SCOPE;
	runtime::Ref<Npc> guard = makeWorldNpc(GUARD_NPC_ID, 505, 500, 100);
	const std::vector<std::string> none;

	ai->setStateIfNot(AIState::FIGHT);
	EXPECT_EQ(aggroCalls(*guard), none) << "already fighting";
	ai->setStateIfNot(AIState::RETURNING);
	EXPECT_EQ(aggroCalls(*guard), none) << "walking home";
	ai->setStateIfNot(AIState::IDLE);
	EXPECT_EQ(aggroCalls(*guard), (std::vector<std::string>{"detected", "aggro"})) << "the state filter is the only reason so far";

	guard->setVisualState(CreatureVisualState::BLINKING);
	EXPECT_EQ(aggroCalls(*guard), none) << "a blinking creature is not aggroed";
	guard->unsetVisualState(CreatureVisualState::BLINKING);

	owner->getPosition()->setIsSpawned(false);
	EXPECT_EQ(aggroCalls(*guard), none) << "the owner is not spawned";
	owner->getPosition()->setIsSpawned(true);

	// the map region of the owner: Java's own gate against npcs thinking where no player is (CreatureEventHandler.java:82-83)
	place(*owner, 900, 900, 100); // a region no player entered
	ASSERT_FALSE(owner->getPosition()->isMapRegionActive());
	runtime::Ref<Npc> nearInactive = makeWorldNpc(GUARD_NPC_ID, 905, 900, 100);
	EXPECT_EQ(aggroCalls(*nearInactive), none) << "the owner's map region is inactive";
	place(*owner, 500, 500, 100);

	EXPECT_EQ(aggroCalls(*guard), (std::vector<std::string>{"detected", "aggro"})) << "every early return was undone";
}

TEST_F(AiDecisionTablesTest, CheckAggroMeasuresTheSeeRange) {
	AI_TEST_SCOPE;
	runtime::Ref<Npc> far = makeWorldNpc(GUARD_NPC_ID, 520, 500, 100); // 20 m: past the aggro range of 8
	EXPECT_EQ(aggroCalls(*far), (std::vector<std::string>{})) << "isInSeeRange: outside srange";

	// srange 8 with bound radii on both sides: 7 m apart is inside, 12 m is not
	far->getPosition()->setXYZH(507.0f, std::nullopt, std::nullopt, std::nullopt);
	EXPECT_EQ(aggroCalls(*far), (std::vector<std::string>{"detected", "aggro"}));
	far->getPosition()->setXYZH(512.0f, std::nullopt, std::nullopt, std::nullopt);
	EXPECT_EQ(aggroCalls(*far), (std::vector<std::string>{})) << "12 m is past srange 8 even with the bound radii";
}

TEST_F(AiDecisionTablesTest, CheckAggroSkipsTheAggroArmForAFriendAndForABigLevelDifference) {
	AI_TEST_SCOPE;
	// same tribe: MONSTER is aggressive to MONSTER in this fixture, but isFriend is true for two npcs of one tribe, so the `!isFriend` term
	// sends the pair to the non-aggressive arm (the shout) instead of to CREATURE_AGGRO
	runtime::Ref<Npc> sameTribe = makeWorldNpc(SPARKIE_NPC_ID, 505, 500, 100);
	ASSERT_TRUE(services::TribeRelationService::isAggressive(*owner, *sameTribe)) << "only then is the isFriend term the deciding one";
	EXPECT_EQ(aggroCalls(*sameTribe), (std::vector<std::string>{"detected"})) << "a friend is detected but not aggroed";

	// hostile tribe, but 38 levels above a level-2 owner whose template type is not a guard: validateAggro is false
	runtime::Ref<Npc> highLevel = makeWorldNpc(HIGH_LEVEL_GUARD_NPC_ID, 505, 500, 100);
	EXPECT_EQ(aggroCalls(*highLevel), (std::vector<std::string>{"detected"})) << "validateAggro: level difference 38 >= 10";
}

TEST_F(AiDecisionTablesTest, CheckAggroLetsAGuardAggroAnyLevel) {
	AI_TEST_SCOPE;
	// the mirror of the previous case's second row - a level-2 owner against a level-40 enemy - with a GUARD template type on the owner, which
	// is validateAggro's second arm
	runtime::Ref<Npc> guardOwner = makeWorldNpc(GUARD_NPC_ID, 500, 500, 100);
	AggroRecordingAI& guardAi = installRecordingAi(*guardOwner);
	guardAi.setStateIfNot(AIState::IDLE);
	ASSERT_TRUE(guardOwner->getPosition()->isMapRegionActive());
	ASSERT_EQ(guardOwner->getObjectTemplate()->getNpcTemplateType(), model::templates::npc::NpcTemplateType::GUARD);

	runtime::Ref<Npc> highLevel = makeWorldNpc(HIGH_LEVEL_NPC_ID, 505, 500, 100);
	CreatureEventHandler::checkAggro(guardAi, *highLevel);
	EXPECT_EQ(guardAi.calls, (std::vector<std::string>{"detected", "aggro"})) << "a GUARD aggroes whatever the level difference";
}

// ---- AttackManager::checkGiveupDistance ------------------------------------------------------------------------------------------------------

TEST_F(AiDecisionTablesTest, CheckGiveupDistanceKeepsChasingANearTargetFromHome) {
	AI_TEST_SCOPE;
	runtime::Ref<Npc> target = makeWorldNpc(GUARD_NPC_ID, 510, 500, 100);
	owner->setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(*target));
	owner->getGameStats()->renewLastAttackTime();
	owner->getGameStats()->renewLastAttackedTime();

	EXPECT_FALSE(AttackManager::checkGiveupDistance(*ai)) << "10 m away, at home, hitting and being hit";
}

TEST_F(AiDecisionTablesTest, CheckGiveupDistanceGivesUpPastTheMapsChaseTarget) {
	AI_TEST_SCOPE;
	runtime::Ref<Npc> target = makeWorldNpc(GUARD_NPC_ID, 510, 500, 100);
	owner->setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(*target));
	owner->getGameStats()->renewLastAttackTime();
	owner->getGameStats()->renewLastAttackedTime();
	ASSERT_FALSE(AttackManager::checkGiveupDistance(*ai));

	target->getPosition()->setXYZH(560.0f, std::nullopt, std::nullopt, std::nullopt); // 60 m, past AiInfo's chase_target of 50
	EXPECT_TRUE(AttackManager::checkGiveupDistance(*ai));
}

TEST_F(AiDecisionTablesTest, CheckGiveupDistanceGivesUpPastTheMapsChaseHome) {
	AI_TEST_SCOPE;
	owner->getGameStats()->renewLastAttackTime();
	owner->getGameStats()->renewLastAttackedTime();
	EXPECT_FALSE(AttackManager::checkGiveupDistance(*ai)) << "no target, at home, fresh timers";

	owner->getPosition()->setXYZH(750.0f, std::nullopt, std::nullopt, std::nullopt); // 250 m from the spawn point, past chase_home 200
	EXPECT_TRUE(AttackManager::checkGiveupDistance(*ai));
}

TEST_F(AiDecisionTablesTest, CheckGiveupDistanceGivesUpAfterTwentySecondsWithoutAnAttack) {
	AI_TEST_SCOPE;
	// the game stats start at 0, so both deltas are the seconds since the epoch: the "> 20 && > 20" arm
	EXPECT_TRUE(AttackManager::checkGiveupDistance(*ai));

	owner->getGameStats()->renewLastAttackedTime();
	EXPECT_FALSE(AttackManager::checkGiveupDistance(*ai)) << "being attacked keeps the npc in the fight";
}

TEST_F(AiDecisionTablesTest, CheckGiveupDistanceGivesUpHalfWayHomeAfterTenSecondsUnattacked) {
	AI_TEST_SCOPE;
	// only the attack timer is renewed, so the npc is hitting but has not been hit for longer than 10 s (the stats start at 0)
	owner->getGameStats()->renewLastAttackTime();
	ASSERT_EQ(owner->getGameStats()->getLastAttackTimeDelta(), 0);
	ASSERT_GT(owner->getGameStats()->getLastAttackedTimeDelta(), 20);

	owner->getPosition()->setXYZH(650.0f, std::nullopt, std::nullopt, std::nullopt); // 150 m: past chase_home / 2 = 100
	EXPECT_TRUE(AttackManager::checkGiveupDistance(*ai));

	owner->getPosition()->setXYZH(550.0f, std::nullopt, std::nullopt, std::nullopt); // 50 m: inside chase_home / 2
	EXPECT_FALSE(AttackManager::checkGiveupDistance(*ai)) << "the same stale timer does not matter close to home";

	owner->getPosition()->setXYZH(650.0f, std::nullopt, std::nullopt, std::nullopt);
	owner->getGameStats()->renewLastAttackedTime();
	EXPECT_FALSE(AttackManager::checkGiveupDistance(*ai)) << "being hit again keeps the npc chasing 150 m from home";
}

TEST_F(AiDecisionTablesTest, CheckGiveupDistanceUsesTheBossLimitsForABoss) {
	AI_TEST_SCOPE;
	runtime::Ref<Npc> boss = makeWorldNpc(BOSS_NPC_ID, 500, 500, 100);
	AggroRecordingAI& bossAi = installRecordingAi(*boss);
	bossAi.setStateIfNot(AIState::IDLE);
	ASSERT_TRUE(boss->isBoss());
	boss->getGameStats()->renewLastAttackTime();
	boss->getGameStats()->renewLastAttackedTime();
	owner->getGameStats()->renewLastAttackTime();
	owner->getGameStats()->renewLastAttackedTime();

	// 175 m from home: inside a normal npc's chase_home of 200, past a boss's hardcoded 150
	boss->getPosition()->setXYZH(675.0f, std::nullopt, std::nullopt, std::nullopt);
	owner->getPosition()->setXYZH(675.0f, std::nullopt, std::nullopt, std::nullopt);
	EXPECT_TRUE(AttackManager::checkGiveupDistance(bossAi));
	EXPECT_FALSE(AttackManager::checkGiveupDistance(*ai)) << "a normal npc may chase 200 m from home";
}

// ---- SimpleAttackManager::isTargetInAttackRange and the first-hit tolerance ------------------------------------------------------------------

TEST_F(AiDecisionTablesTest, IsTargetInAttackRangeUsesTheAttackRangeStat) {
	AI_TEST_SCOPE;
	EXPECT_FALSE(SimpleAttackManager::isTargetInAttackRange(*owner)) << "no target";

	runtime::Ref<Npc> target = makeWorldNpc(GUARD_NPC_ID, 501, 500, 100);
	owner->setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(*target));
	EXPECT_TRUE(SimpleAttackManager::isTargetInAttackRange(*owner)) << "1 m apart, arange 2";

	target->getPosition()->setXYZH(510.0f, std::nullopt, std::nullopt, std::nullopt);
	EXPECT_FALSE(SimpleAttackManager::isTargetInAttackRange(*owner)) << "10 m apart";
}

/**
 * m5b-plan.md §6.3 A1b and §6.6: the gate gave the first-hit range tolerance of PlayerController::attackTarget up, because an aggressive
 * monster is already hating the character by the time it is inside the ~0.6 m band. A-08 asks for a unit case instead.
 *
 * **What this case proves and what it cannot.** It pins the two ported ingredients of the branch
 * (`if (!target->getAggroList().isHating(attacker)) attackRange += PositionUtil::calculateMaxCoveredDistance(attacker, 100);`,
 * PlayerController.cpp:493-494): that `calculateMaxCoveredDistance(attacker, 100)` really is a positive, bounded band, and that there is a
 * stand-off distance which `isInAttackRange` rejects with the plain attack range and accepts with the widened one. It does **not** execute
 * PlayerController::attackTarget: that body's first statement is `standins::playerRestrictionsCanAttack`, which is AION_UNPORTED until item
 * E-01b of stage 2. The branch-level case (isHating forced both ways around one attackTarget call) belongs in tests/controllers and has to be
 * written by the lane that lands E-01b; this case is what stage 1 can hold.
 */
TEST_F(AiDecisionTablesTest, TheFirstHitRangeToleranceBandIsPositiveAndBounded) {
	AI_TEST_SCOPE;
	runtime::Ref<Npc> attacker = makeWorldNpc(SPARKIE_NPC_ID, 500, 500, 100);
	runtime::Ref<Npc> target = makeWorldNpc(GUARD_NPC_ID, 500, 500, 100);

	// the tolerance is `movementSpeed * 100 ms`, in meters: run speed 2.0 -> 2000 thousandths -> 0.2 m
	const float tolerance = PositionUtil::calculateMaxCoveredDistance(*attacker, 100);
	EXPECT_GT(tolerance, 0.0f);
	EXPECT_FLOAT_EQ(tolerance, attacker->getGameStats()->getMovementSpeed()->getCurrent() * 100 / 1'000'000.0f);
	EXPECT_EQ(PositionUtil::calculateMaxCoveredDistance(*attacker, 0), 0.0f) << "a non-positive duration is no tolerance";

	// a stand-off inside the band: attackRange rejects it, attackRange + tolerance accepts it. The range is PlayerController.cpp:492's
	// expression evaluated with an npc attacker, because tests/ai cannot build a Player without leasing another chunk's test support.
	const float attackRange = 1 + static_cast<float>(attacker->getGameStats()->getAttackRange()->getCurrent()) / 1000.0f;
	const float boundRadii = attacker->getObjectTemplate()->getBoundRadius()->getMaxOfFrontAndSide() +
		target->getObjectTemplate()->getBoundRadius()->getMaxOfFrontAndSide();
	const float standOff = attackRange + boundRadii + tolerance / 2;
	target->getPosition()->setXYZH(500.0f + standOff, std::nullopt, std::nullopt, std::nullopt);

	EXPECT_FALSE(PositionUtil::isInAttackRange(runtime::Ptr<Creature>(*attacker), runtime::Ptr<Creature>(*target), attackRange))
		<< "without the tolerance the shot is out of range";
	EXPECT_TRUE(PositionUtil::isInAttackRange(runtime::Ptr<Creature>(*attacker), runtime::Ptr<Creature>(*target), attackRange + tolerance))
		<< "with the tolerance it is in range";

	// and the band is bounded: the gate's 12 m stand-off (m5b-plan.md K4) is out of range either way, which is what A1a asserts
	target->getPosition()->setXYZH(512.0f, std::nullopt, std::nullopt, std::nullopt);
	EXPECT_FALSE(PositionUtil::isInAttackRange(runtime::Ptr<Creature>(*attacker), runtime::Ptr<Creature>(*target), attackRange + tolerance));

	// the gate of the branch: the aggro list decides which of the two ranges attackTarget uses
	EXPECT_FALSE(target->getAggroList().isHating(*attacker)) << "a fresh aggro list hates nobody";
}

} // namespace
} // namespace aion::gameserver::ai::testing
