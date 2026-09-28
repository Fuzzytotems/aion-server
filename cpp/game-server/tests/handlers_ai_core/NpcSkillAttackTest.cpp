// m5b2-plan.md N-02/N-04 (P5-05, aion_gs_handlers_ai_core): GeneralNpcAI::chooseSkillAttack, closed in M5b-2, and the attack loop it now feeds.
// The root handlers are the only AIs that call chooseSkillAttack and handle ATTACK_COMPLETE, so these cases run a real AggressiveNpcAI (the
// kerub's own ai) and a real GeneralNpcAI over the shipped rows of ../ai/NpcSkillTestSupport.h:
// - chooseAttackIntention answers SKILL_ATTACK when SkillAttackManager::chooseNextSkill finds a ready skill and SIMPLE_ATTACK when it does not,
//   and chooseSkillAttack stores the chosen entry as the last skill (GeneralNpcAI.java:130-138);
// - the alwaysRandomSkill arm: an npc without attack range takes NpcSkillList.getRandomSkill, which asks no gate at all;
// - the loop: AttackManager.chooseAttack -> SkillAttackManager.performAttack -> the cast -> Skill.endCast -> afterUseSkill -> ATTACK_COMPLETE
//   -> AttackEventHandler.onAttackComplete -> scheduleNextAttack, i.e. the npc goes back to its swings when the cast is over;
// - the same way back from the two other ends of a skill attack: a cast the skill engine refuses (skillAction's `!success` arm, 210162's
//   16856), and a delayed skillAction whose CAST was interrupted before it ran (the think() arm).

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/AttackIntention.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/manager/AttackManager.h"
#include "aion/gameserver/ai/manager/SkillAttackManager.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/skill/NpcSkillEntry.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

#include "../ai/NpcSkillTestSupport.h"

namespace aion::gameserver::ai::testing {
namespace {

namespace Rnd = commons::utils::Rnd;
namespace roots = gameserver::handlers::ai;

using model::gameobjects::Npc;
using model::skill::NpcSkillEntry;
using runtime::Ptr;
using runtime::Ref;

class NpcSkillAttackTest : public NpcSkillWorldTest {
protected:
	void SetUp() override {
		NpcSkillWorldTest::SetUp();
		AI_TEST_SCOPE;
		regionActivator = activateRegionAt(500, 500, 100);
		executor->runReady(); // MapRegion::activate posts the ACTIVATE notification of its creatures
	}

	/** Replaces the npc's warn-mode substitute AI with the real root handler (this executable links the empty registry table). */
	template <class AI>
	AI& installRootAi(Npc& npc) {
		auto handlerAi = std::make_unique<AI>(npc);
		AI& result = *handlerAi;
		npc.replaceAi(std::move(handlerAi));
		return result;
	}

	/** An npc in FIGHT against `target`, hating it and targeting it - the state AttackManager.chooseAttack runs in */
	template <class AI>
	AI& fighting(Npc& npc, Npc& target) {
		know(npc, target); // before the real AI is installed, so the knownlist notifications reach the no-op substitute
		AI& ai = installRootAi<AI>(npc);
		ai.setStateIfNot(AIState::FIGHT);
		npc.getAggroList().addHate(target, 10);
		npc.setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(target));
		return ai;
	}

	runtime::Ref<model::gameobjects::player::Player> regionActivator;
};

TEST_F(NpcSkillAttackTest, ChooseAttackIntentionAnswersSkillAttackOnlyForAReadySkillAndStoresIt) {
	AI_TEST_SCOPE;
	Ref<Npc> kerub = makeWorldNpc(STRIPED_KERUB_NPC_ID, 500, 500, 100);
	Ref<Npc> pagati = makeWorldNpc(TAMED_PAGATI_NPC_ID, 501.5f, 500, 100);
	roots::AggressiveNpcAI& ai = fighting<roots::AggressiveNpcAI>(*kerub, *pagati);
	Ptr<NpcSkillEntry> brandish = entry(*kerub, 0);
	ASSERT_EQ(kerub->getObjectTemplate()->getAttackRange(), 2) << "arange 2: SkillAttackManager decides, not getRandomSkill";

	Rnd::seedCurrentThreadForTests(seedWhere(chanceDraws(2100, 25), true));
	EXPECT_EQ(ai.chooseAttackIntention(), AttackIntention::SKILL_ATTACK) << "Brandish's 25 % draw passed";
	EXPECT_EQ(kerub->getGameStats()->getLastSkill().get(), brandish.get()) << "chooseSkillAttack stores the skill the attack will cast";

	kerub->getGameStats()->setLastSkill(nullptr);
	Rnd::seedCurrentThreadForTests(seedWhere(chanceDraws(2100, 25), false));
	EXPECT_EQ(ai.chooseAttackIntention(), AttackIntention::SIMPLE_ATTACK) << "Brandish's 25 % draw failed";
	EXPECT_EQ(kerub->getGameStats()->getLastSkill().get(), nullptr);
}

/**
 * 206292 has no arange, i.e. attack range 0, and a general AI: GeneralNpcAI.chooseAttackIntention passes alwaysRandomSkill = true and the
 * skill comes from NpcSkillList.getRandomSkill, which asks neither the fight's initial skill delay nor the chance nor the conditions. The fight
 * starts now, so SkillAttackManager.chooseNextSkill would still answer null for Rnd.get(2000, 6000) ms. One decision over 206292's one-skill
 * row (npc_skills.xml:1344-1346): the case shows the gates are skipped, not how the random pick spreads.
 */
TEST_F(NpcSkillAttackTest, AnNpcWithoutAttackRangeTakesItsSkillWithoutTheRotationGates) {
	AI_TEST_SCOPE;
	Ref<Npc> caster = makeWorldNpc(RANGELESS_CASTER_NPC_ID, 500, 500, 100);
	Ref<Npc> pagati = makeWorldNpc(TAMED_PAGATI_NPC_ID, 505, 500, 100);
	roots::GeneralNpcAI& ai = fighting<roots::GeneralNpcAI>(*caster, *pagati);
	ASSERT_EQ(caster->getObjectTemplate()->getAttackRange(), 0);
	caster->getGameStats()->setFightStartingTime();

	EXPECT_EQ(ai.chooseAttackIntention(), AttackIntention::SKILL_ATTACK);
	EXPECT_EQ(caster->getGameStats()->getLastSkill().get(), entry(*caster, 0).get()) << "21128 Flame Blaze, its only skill";
}

TEST_F(NpcSkillAttackTest, ASkillAttackCastsAndThenResumesTheAttackLoop) {
	AI_TEST_SCOPE;
	Ref<Npc> kerub = makeWorldNpc(STRIPED_KERUB_NPC_ID, 500, 500, 100);
	Ref<Npc> pagati = makeWorldNpc(TAMED_PAGATI_NPC_ID, 501.5f, 500, 100);
	roots::AggressiveNpcAI& ai = fighting<roots::AggressiveNpcAI>(*kerub, *pagati);

	Rnd::seedCurrentThreadForTests(seedWhere(chanceDraws(2100, 25), true));
	manager::AttackManager::chooseAttack(ai, 0);
	EXPECT_TRUE(ai.isInSubState(AISubState::CAST));
	EXPECT_TRUE(kerub->isCasting()) << "the SKILL_ATTACK cast Brandish";

	executor->advance(std::chrono::milliseconds(2600));
	EXPECT_TRUE(ai.isInSubState(AISubState::NONE));
	EXPECT_TRUE(ai.isInState(AIState::FIGHT));
	// afterUseSkill -> ATTACK_COMPLETE -> onAttackComplete -> scheduleNextAttack -> chooseAttack: next_skill_time -1 made the next skill wait
	// Rnd.get(3000, 9000) ms (Skill.useSkill), so the decision is a SIMPLE_ATTACK, scheduled at the attack interval
	EXPECT_TRUE(kerub->getGameStats()->isNextAttackScheduled()) << "the npc went back to its swings";
}

/**
 * skillAction's `!success` arm (SkillAttackManager.java:98-101), live on the start maps: 210162's 16856 Blessing of Rock (npc_skills.xml:2885)
 * has target_relation FRIEND (skill_templates.xml:124843) but the default npc_skill target MOST_HATED, so the skill engine refuses every cast
 * of it at the npc's enemy and useSkill answers false. afterUseSkill hands the npc back to its swings; without it the npc would stay in CAST,
 * where AttackManager.scheduleNextAttack refuses to attack.
 */
TEST_F(NpcSkillAttackTest, ASkillTheEngineRefusesHandsTheNpcBackToItsSwings) {
	AI_TEST_SCOPE;
	Ref<Npc> duaguru = makeWorldNpc(SUPERVISOR_DUAGURU_NPC_ID, 500, 500, 100);
	Ref<Npc> pagati = makeWorldNpc(TAMED_PAGATI_NPC_ID, 502, 500, 100);
	roots::AggressiveNpcAI& ai = fighting<roots::AggressiveNpcAI>(*duaguru, *pagati);
	Ptr<NpcSkillEntry> blessing = entryOf(*duaguru, BLESSING_OF_ROCK);
	ASSERT_TRUE(blessing);
	// the decision after this one is a swing: the last skill was used a moment ago and the next one waits 5000 ms
	duaguru->getGameStats()->renewLastSkillTime();
	duaguru->getGameStats()->setNextSkillDelay(5000);
	duaguru->getGameStats()->setLastSkill(blessing); // what chooseSkillAttack stores before the attack
	ASSERT_FALSE(duaguru->getGameStats()->isNextAttackScheduled());

	manager::SkillAttackManager::performAttack(ai, 0);
	ASSERT_FALSE(duaguru->isCasting()) << "precondition: the skill engine refused 16856 at an enemy";
	EXPECT_TRUE(ai.isInSubState(AISubState::NONE)) << "afterUseSkill left CAST";
	EXPECT_TRUE(duaguru->getGameStats()->isNextAttackScheduled()) << "ATTACK_COMPLETE -> scheduleNextAttack: the npc swings again";
}

/**
 * skillAction's interrupted-cast arm (SkillAttackManager.java:54-57): a delayed skill attack whose CAST ended before its task ran -
 * CreatureController.abortCast, as a stun or a target change calls it - finds the npc in sub-state NONE and, while it fights, thinks again:
 * ThinkEventHandler.thinkAttack -> AttackManager.scheduleNextAttack schedules the next attack.
 */
TEST_F(NpcSkillAttackTest, ADelayedSkillAttackWhoseCastWasInterruptedThinksAgain) {
	AI_TEST_SCOPE;
	Ref<Npc> kerub = makeWorldNpc(STRIPED_KERUB_NPC_ID, 500, 500, 100);
	Ref<Npc> pagati = makeWorldNpc(TAMED_PAGATI_NPC_ID, 501.5f, 500, 100);
	roots::AggressiveNpcAI& ai = fighting<roots::AggressiveNpcAI>(*kerub, *pagati);
	// the decision after the interruption is a swing: the last skill was used a moment ago and the next one waits 5000 ms
	kerub->getGameStats()->renewLastSkillTime();
	kerub->getGameStats()->setNextSkillDelay(5000);
	kerub->getGameStats()->setLastSkill(entry(*kerub, 0));

	manager::SkillAttackManager::performAttack(ai, 750);
	ASSERT_TRUE(ai.isInSubState(AISubState::CAST)) << "precondition: performAttack entered CAST and scheduled skillAction";
	kerub->getController().abortCast();
	ASSERT_TRUE(ai.isInSubState(AISubState::NONE));
	ASSERT_FALSE(kerub->getGameStats()->isNextAttackScheduled());

	executor->advance(std::chrono::milliseconds(750));
	EXPECT_TRUE(kerub->getGameStats()->isNextAttackScheduled()) << "skillAction found NONE in FIGHT and thought: the npc swings again";
}

} // namespace
} // namespace aion::gameserver::ai::testing
