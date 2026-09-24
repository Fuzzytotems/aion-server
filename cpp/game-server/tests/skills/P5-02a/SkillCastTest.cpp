// P5-02a, M5b-2 stage 1 part 2 (m5b2-plan.md S-01, S-02, S-03, S-04, S-06, S-08): the cast machine end to end - Skill.useSkill ->
// canUseSkill -> Properties.validate -> startCast now, endCast after the cast duration -> payCastCosts -> the chain -> SM_CASTSPELL_RESULT - on a
// real Player in a real map instance (CastTestSupport.h). Expectations follow skillengine/model/Skill.java:143-702, Properties.java:70-118 and
// the condition classes.
//
// What these cases prove: the two-phase cast (the bar starts now, the costs are paid when it ends), that a cancelled cast pays nothing, the MP
// and HP end conditions, the cooldown, the chain window, the first-target / range / relation rules of an enemy skill, SkillEngine.getSkillFor's
// PROVOKED bypass and the real Effect of applyEffectDirectly's skill-id overloads. The template overload that enter-world's passive skills call
// is ported but held back behind the M5a O-09 AION_PARTIAL until part 3 ports the effect leaves it reaches (m5b2-plan.md F-02/F-03; its case
// pins the held-back answer). What they cannot: any effect's behaviour - the effect leaf classes are the part-3 lanes' (the templates here carry no
// <effects>, except EFFECT_SKILL, whose case shows where the cast first meets an unported body).

#include "CastTestSupport.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CASTSPELL.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CASTSPELL_RESULT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/LiveInstanceCounters.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/ChainSkill.h"
#include "aion/gameserver/skillengine/model/ChainSkills.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/properties/Properties_CastState.h"

namespace aion::gameserver::skillengine::test {
namespace {

using namespace std::chrono_literals;
using gameserver::model::gameobjects::Creature;
using network::aion::serverpackets::SM_CASTSPELL;
using network::aion::serverpackets::SM_CASTSPELL_RESULT;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using properties::Properties_CastState;
using runtime::Ptr;
using runtime::Ref;

class SkillCastTest : public CastTest {
protected:
	std::vector<uint8_t> message(SM_SYSTEM_MESSAGE&& packet) { return cp::serialized(std::move(packet), client->con()); }

	bool sentMessage(SM_SYSTEM_MESSAGE&& packet) {
		std::vector<uint8_t> expected = message(std::move(packet));
		for (const std::vector<uint8_t>& bytes : sent())
			if (bytes == expected)
				return true;
		return false;
	}

	/** Sets the caster's MP and drops the SM_STATUPDATE_MP that setCurrentMp queues */
	void setMp(int32_t value) {
		caster.player->getLifeStats()->setCurrentMp(value);
		ASSERT_EQ(currentMp(), value);
		(*client)->clearSent();
	}
};

TEST_F(SkillCastTest, ATimedCastStartsTheBarNowAndPaysItsMpWhenItEnds) {
	setMp(100);
	Ref<model::Skill> cast = skill(TIMED_SKILL);

	// Skill.java:274-319: canUseSkill(CAST_START), the durations, setCasting, startCast, then schedule(this::endCast, castDuration)
	ASSERT_TRUE(cast->useSkill());
	std::vector<std::vector<uint8_t>> started = packetsOf<SM_CASTSPELL>(sent());
	ASSERT_EQ(started.size(), 1u) << "startCast broadcasts SM_CASTSPELL to the caster too (broadcastPacketAndReceive)";
	CastSpellFields bar = decodeCastSpell(started[0]);
	EXPECT_EQ(bar.effectorId, caster.player->getObjectId());
	EXPECT_EQ(bar.spellId, TIMED_SKILL);
	EXPECT_EQ(bar.level, 1);
	EXPECT_EQ(bar.targetType, 0);
	EXPECT_EQ(bar.targetObjectId, caster.player->getObjectId()) << "first_target=ME: FirstTargetProperty sets the caster (FirstTargetProperty.java:22-25)";
	EXPECT_EQ(bar.castDuration, 2000) << "no cast speed modifier: calculateMagicalCastDuration answers the template's duration";
	EXPECT_TRUE(caster.player->isCasting());
	EXPECT_EQ(caster.player->getCastingSkill(), cast);
	EXPECT_EQ(currentMp(), 100) << "the MP is an end condition: payCastCosts runs in endCast, not in useSkill (Skill.java:570)";
	EXPECT_TRUE(packetsOf<SM_CASTSPELL_RESULT>(sent()).empty());

	advance(1999ms);
	EXPECT_TRUE(caster.player->isCasting()) << "the end task is due at the cast duration";
	EXPECT_TRUE(packetsOf<SM_CASTSPELL_RESULT>(sent()).empty());
	EXPECT_EQ(currentMp(), 100);

	advance(1ms);
	EXPECT_FALSE(caster.player->isCasting()) << "endCast: effector.setCasting(null) (Skill.java:574)";
	EXPECT_EQ(currentMp(), 81) << "MpCondition.validate: reduceMp(USED_MP, 19) (MpCondition.java:31-35)";
	std::vector<std::vector<uint8_t>> results = packetsOf<SM_CASTSPELL_RESULT>(sent());
	ASSERT_EQ(results.size(), 1u);
	CastSpellResultFields result = decodeCastSpellResult(results[0]);
	EXPECT_EQ(result.skillId, TIMED_SKILL);
	EXPECT_EQ(result.targetObjectId, caster.player->getObjectId());
	EXPECT_EQ(result.status, 16) << "no effect was created (the template has no <effects>): SM_CASTSPELL_RESULT writes 16";
	EXPECT_EQ(result.effectCount, 0);
	EXPECT_EQ(runtime::unportedHitCount(), 0u) << "no body of the cast machine is unported";
}

TEST_F(SkillCastTest, ACancelledCastEndsWithoutPayingAndWithoutAResult) {
	setMp(100);
	Ref<model::Skill> cast = skill(TIMED_SKILL);
	ASSERT_TRUE(cast->useSkill());
	advance(500ms);

	// Skill.java:541-546: cancelCast marks the skill; the end task still runs and returns at `if (!effector.isCasting() || isCancelled)`
	cast->cancelCast();
	EXPECT_TRUE(caster.player->isCasting()) << "cancelCast alone does not clear the casting skill (PlayerController.cancelCurrentSkill does)";
	(*client)->clearSent();
	advance(2000ms);
	EXPECT_EQ(currentMp(), 100) << "a cancelled cast pays nothing";
	EXPECT_TRUE(packetsOf<SM_CASTSPELL_RESULT>(sent()).empty()) << "and ends without SM_CASTSPELL_RESULT";
}

TEST_F(SkillCastTest, CancellingThroughThePlayerControllerClearsTheCastAndItsEndTaskDoesNothing) {
	setMp(100);
	Ref<model::Skill> cast = skill(TIMED_SKILL);
	ASSERT_TRUE(cast->useSkill());
	advance(300ms);

	// PlayerController.cancelCurrentSkill -> castingSkill.cancelCast(), setCasting(null), SM_SKILL_CANCEL (PlayerController.java:514-546)
	caster.player->getController().cancelCurrentSkill(nullptr);
	EXPECT_FALSE(caster.player->isCasting());
	(*client)->clearSent();
	advance(2000ms);
	EXPECT_EQ(currentMp(), 100);
	EXPECT_TRUE(packetsOf<SM_CASTSPELL_RESULT>(sent()).empty());
}

TEST_F(SkillCastTest, AnInstantSkillEndsInsideUseSkillAndStartsItsCooldown) {
	int64_t before = commons::utils::currentTimeMillis();
	Ref<model::Skill> cast = skill(INSTANT_SKILL);
	ASSERT_TRUE(cast->useSkill());
	int64_t after = commons::utils::currentTimeMillis();

	// duration="0": useSkill calls endCast() itself (Skill.java:315-316), so both packets are queued before it returns
	std::vector<std::vector<uint8_t>> started = packetsOf<SM_CASTSPELL>(sent());
	ASSERT_EQ(started.size(), 1u);
	EXPECT_EQ(decodeCastSpell(started[0]).castDuration, 0);
	std::vector<std::vector<uint8_t>> results = packetsOf<SM_CASTSPELL_RESULT>(sent());
	ASSERT_EQ(results.size(), 1u);
	EXPECT_EQ(decodeCastSpellResult(results[0]).cooldown, 100) << "Skill.getCooldown: the template's cooldown (tenths of a second)";
	EXPECT_FALSE(caster.player->isCasting());

	// setCooldowns: setSkillCoolDown(cooldownId, cooldown * 100 + now) (Skill.java:321-328)
	int64_t cooldownEnd = caster.player->getSkillCoolDown(502);
	EXPECT_GE(cooldownEnd, before + 10000);
	EXPECT_LE(cooldownEnd, after + 10000);
	EXPECT_TRUE(caster.player->isSkillDisabled(skillTemplate(INSTANT_SKILL)));
}

TEST_F(SkillCastTest, NotEnoughMpRefusesTheCastBeforeItStarts) {
	setMp(18);
	Ref<model::Skill> cast = skill(TIMED_SKILL);

	// canUseSkill(CAST_START) -> canPayCastCosts -> MpCondition.canValidate: currentMp < 19 (Skill.java:175, MpCondition.java:38-45)
	EXPECT_FALSE(cast->useSkill());
	EXPECT_TRUE(sentMessage(SM_SYSTEM_MESSAGE::STR_SKILL_NOT_ENOUGH_MP()));
	EXPECT_TRUE(packetsOf<SM_CASTSPELL>(sent()).empty()) << "a cast that cannot be paid never starts";
	EXPECT_FALSE(caster.player->isCasting());
	EXPECT_EQ(currentMp(), 18);

	setMp(19);
	EXPECT_TRUE(skill(TIMED_SKILL)->useSkill()) << "exactly the cost is enough: the check is `currentMp < cost`";
}

TEST_F(SkillCastTest, AnHpCostIsPaidFromTheCastersHpAndMayNeverBeLethal) {
	// <hp value="10" delta="2"/>: getCost = value + delta * skillLevel = 12 at level 1 (HpCondition.java:49-54)
	caster.player->getLifeStats()->setCurrentHp(12);
	(*client)->clearSent();
	EXPECT_FALSE(skill(HP_COST_SKILL)->useSkill()) << "HpCondition.canValidate: currentHp <= cost refuses (the cast may never be lethal)";
	EXPECT_TRUE(sentMessage(SM_SYSTEM_MESSAGE::STR_SKILL_NOT_ENOUGH_HP()));

	caster.player->getLifeStats()->setCurrentHp(50);
	(*client)->clearSent();
	ASSERT_TRUE(skill(HP_COST_SKILL)->useSkill());
	EXPECT_EQ(caster.player->getLifeStats()->getCurrentHp(), 50) << "not before the cast ends";
	advance(1000ms);
	EXPECT_EQ(caster.player->getLifeStats()->getCurrentHp(), 38) << "reduceHp(USED_HP, 12)";
}

TEST_F(SkillCastTest, NothingIsPaidUnlessEveryCostCanBePaid) {
	// Skill.canPayCastCosts (Skill.java:187-198) asks every end condition and every action before anything is paid; DpUseAction.canAct refuses
	// a player with no DP, so the MP end condition and the MP action must not be paid either
	setMp(100);
	// a starting class holds no DP (PlayerCommonData.setDp returns for WARRIOR/SCOUT/MAGE/PRIEST), so the caster becomes a Gladiator here
	caster.commonData->setPlayerClass(gameserver::model::PlayerClass::GLADIATOR);
	caster.player->getCommonData()->setDp(0);
	(*client)->clearSent();
	EXPECT_FALSE(skill(ACTIONS_SKILL)->useSkill());
	EXPECT_TRUE(sentMessage(SM_SYSTEM_MESSAGE::STR_SKILL_NOT_ENOUGH_DP()));
	EXPECT_EQ(currentMp(), 100);
	EXPECT_TRUE(packetsOf<SM_CASTSPELL>(sent()).empty());

	// with the DP: payCastCosts pays the end conditions, then the actions in order (Skill.java:784-812) - MpCondition 10, MpUseAction
	// value + delta * level = 8 (MpUseAction.java getCost), DpUseAction 50
	caster.player->getCommonData()->setDp(60);
	ASSERT_TRUE(skill(ACTIONS_SKILL)->useSkill());
	EXPECT_EQ(currentMp(), 82);
	EXPECT_EQ(caster.player->getCommonData()->getDp(), 10);
}

TEST_F(SkillCastTest, AChainSkillOpensItsChainAndTheFollowUpContinuesIt) {
	Ptr<model::ChainSkills> chain = caster.player->getChainSkills();
	ASSERT_TRUE(chain->getCurrentChainSkill()->getCategory().empty());

	// without the first skill, the follow-up's precategory matches neither the current nor the previous chain skill (ChainCondition.java:36-43)
	EXPECT_FALSE(skill(CHAIN_FOLLOWUP_SKILL)->useSkill());
	EXPECT_TRUE(chain->getCurrentChainSkill()->getCategory().empty());

	// ChainCondition sets the skill's chain category; endCast rolls chain_skill_prob="100" and updates the chain (Skill.java:627-638)
	ASSERT_TRUE(skill(CHAIN_SKILL)->useSkill());
	EXPECT_EQ(chain->getCurrentChainSkill()->getCategory(), "T_CHAINA_1TH_1");
	EXPECT_EQ(chain->getCurrentChainSkill()->getUseCount(), 1);
	EXPECT_EQ(chain->getCurrentChainCount("T_CHAINA_1TH_1"), 1);

	ASSERT_TRUE(skill(CHAIN_FOLLOWUP_SKILL)->useSkill()) << "the precategory skill was activated once: precount 1 is met";
	EXPECT_EQ(chain->getCurrentChainSkill()->getCategory(), "T_CHAINA_2TH_1");
	EXPECT_EQ(chain->getPreviousChainSkill()->getCategory(), "T_CHAINA_1TH_1") << "ChainSkills.updateChain keeps the previous skill";
	EXPECT_FALSE(chain->isChainExpired()) << "the follow-up's time=\"5000\" window has just opened";
}

TEST_F(SkillCastTest, AnActiveNonChainSkillResetsTheChain) {
	ASSERT_TRUE(skill(CHAIN_SKILL)->useSkill());
	Ptr<model::ChainSkills> chain = caster.player->getChainSkills();
	ASSERT_EQ(chain->getCurrentChainSkill()->getCategory(), "T_CHAINA_1TH_1");

	// Skill.canUseSkill: a CAST skill without a chain category resets the chain (Skill.java:158-159)
	ASSERT_TRUE(skill(INSTANT_SKILL)->useSkill());
	EXPECT_TRUE(chain->getCurrentChainSkill()->getCategory().empty());
	EXPECT_EQ(chain->getCurrentChainSkill()->getUseCount(), 0);
}

TEST_F(SkillCastTest, AnEnemySkillNeedsAnEnemyTargetInRange) {
	Ref<CastTestNpc> near = spawnMonster(110.0f, 100.0f, 50.0f); // 10 m
	Ref<CastTestNpc> far = spawnMonster(140.0f, 100.0f, 50.0f);  // 40 m, beyond first_target_range="25"

	{
		SCOPED_TRACE("no target");
		Ref<model::Skill> cast = skill(ENEMY_SKILL);
		EXPECT_FALSE(cast->canUseSkill(Properties_CastState::CAST_START));
		EXPECT_TRUE(sentMessage(SM_SYSTEM_MESSAGE::STR_SKILL_TARGET_IS_NOT_VALID())) << "FirstTargetProperty.java:72-82";
		(*client)->clearSent();
	}
	{
		SCOPED_TRACE("the caster itself");
		Ref<model::Skill> cast = skill(ENEMY_SKILL, caster.player);
		EXPECT_FALSE(cast->canUseSkill(Properties_CastState::CAST_START));
		EXPECT_TRUE(sentMessage(SM_SYSTEM_MESSAGE::STR_SKILL_TARGET_IS_NOT_VALID()));
		(*client)->clearSent();
	}
	{
		SCOPED_TRACE("out of range");
		Ref<model::Skill> cast = skill(ENEMY_SKILL, far);
		EXPECT_FALSE(cast->canUseSkill(Properties_CastState::CAST_START));
		EXPECT_TRUE(sentMessage(SM_SYSTEM_MESSAGE::STR_SKILL_NOT_ENOUGH_DISTANCE())) << "FirstTargetRangeProperty.java:59-64";
		(*client)->clearSent();
	}
	{
		SCOPED_TRACE("in range");
		Ref<model::Skill> cast = skill(ENEMY_SKILL, near);
		EXPECT_TRUE(cast->canUseSkill(Properties_CastState::CAST_START));
		std::vector<Ptr<Creature>> effected = cast->getEffectedList().snapshot();
		ASSERT_EQ(effected.size(), 1u) << "target_type ONLYONE: the first target alone";
		EXPECT_EQ(effected[0], Ptr<Creature>(near));
		EXPECT_EQ(cast->getFirstTarget(), Ptr<Creature>(near));
	}
}

TEST_F(SkillCastTest, AnEnemySkillDropsATargetThatIsNoEnemy) {
	// a second Elyos player: same race, not dueling, so Player.isEnemy is false; target_relation ENEMY filters it out (TargetRelationProperty.java:21-24)
	cp::PlayerFixture friendly = cp::makePlayer(410002, 9402, "Friend");
	place(*friendly.player, 105.0f, 100.0f, 50.0f);
	Ref<model::Skill> cast = skill(ENEMY_SKILL, friendly.player);
	EXPECT_FALSE(cast->canUseSkill(Properties_CastState::CAST_START));
	EXPECT_TRUE(cast->getEffectedList().isEmpty());
	EXPECT_TRUE(sentMessage(SM_SYSTEM_MESSAGE::STR_SKILL_TARGET_IS_NOT_VALID()))
		<< "Skill.validateEffectedList: a selected target that will not be hit (Skill.java:208-214)";
}

TEST_F(SkillCastTest, AnEnemySkillHitsItsTargetAndPaysAtTheEnd) {
	Ref<CastTestNpc> near = spawnMonster(110.0f, 100.0f, 50.0f);
	setMp(100);
	Ref<model::Skill> cast = skill(ENEMY_SKILL, near);
	ASSERT_TRUE(cast->useSkill());
	EXPECT_EQ(currentMp(), 62) << "38 MP";
	std::vector<std::vector<uint8_t>> results = packetsOf<SM_CASTSPELL_RESULT>(sent());
	ASSERT_EQ(results.size(), 1u);
	EXPECT_EQ(decodeCastSpellResult(results[0]).targetObjectId, near->getObjectId());
	EXPECT_TRUE(caster.player->getController().isInCombat()) << "a DEBUFF is hostile: enterCombat(true) (Skill.java:650-651)";
}

TEST_F(SkillCastTest, ATimedCastIsCancelledWhenItsTargetDies) {
	Ref<CastTestNpc> npc = spawnMonster(110.0f, 100.0f, 50.0f);
	Ref<model::Skill> cast = skill(TIMED_ENEMY_SKILL, npc);
	ASSERT_TRUE(cast->useSkill());
	ASSERT_TRUE(caster.player->isCasting());
	(*client)->clearSent();

	// Skill.startCast attached a DeathObserver to the first target (Skill.java:528-537): its death cancels the cast with STR_SKILL_TARGET_LOST
	npc->getObserveController()->notifyDeathObservers(*caster.player);
	EXPECT_FALSE(caster.player->isCasting());
	EXPECT_TRUE(sentMessage(SM_SYSTEM_MESSAGE::STR_SKILL_TARGET_LOST()));
	advance(2000ms);
	EXPECT_TRUE(packetsOf<SM_CASTSPELL_RESULT>(sent()).empty()) << "the end task finds the cast cancelled";
}

TEST_F(SkillCastTest, AnEndedCastDetachesItsDeathObserverAndIsReclaimed) {
	Ref<CastTestNpc> npc = spawnMonster(110.0f, 100.0f, 50.0f);
	ASSERT_FALSE(npc->getObserveController()->hasObservers()) << "a fresh npc observes nothing";
	int64_t skillsBefore = runtime::liveCountOf(typeid(model::Skill));
	{
		Ref<model::Skill> cast = skill(TIMED_ENEMY_SKILL, npc);
		ASSERT_TRUE(cast->useSkill());
		ASSERT_TRUE(npc->getObserveController()->hasObservers()) << "startCast attached the DeathObserver (Skill.java:535-536)";
		advance(2000ms);
		ASSERT_FALSE(caster.player->isCasting());
		ASSERT_EQ(packetsOf<SM_CASTSPELL_RESULT>(sent()).size(), 1u);
	}
	// Skill.endCast -> removeObservers (Skill.java:557, :702-706) detaches the DeathObserver from the target: checked in every build, directly on
	// the target's observer list (the observer is one-shot, so notifying the death first would detach it and hide a missing removal)
	EXPECT_FALSE(npc->getObserveController()->hasObservers()) << "the target's observer list no longer holds the DeathObserver";

	// removeObservers also clears Skill.firstTargetDieObserver, the C++ breaker of cycles.toml's `Skill.firstTargetDieObserver` row (Java keeps
	// the field; its GC does not care). Either one left undone keeps the ended Skill alive: the target's observer list pins it through the
	// observer's callback, or the Skill and its observer hold each other. Checked builds count live instances.
	if (runtime::LIVE_COUNTS_ENABLED) {
		scope.reset(); // a scope pins what it borrowed; the Reclaimer frees the retired Skill once no scope can see it
		runtime::Reclaimer::getInstance().drain();
		scope = std::make_unique<runtime::TaskScope>(AION_TASK_INFO(runtime::TaskKind::TEST));
		EXPECT_EQ(runtime::liveCountOf(typeid(model::Skill)), skillsBefore) << "the ended cast was reclaimed";
	}

	// the old target's death no longer reaches the caster: a left-over observer would run getEffector().getController().cancelCurrentSkill(null,
	// STR_SKILL_TARGET_LOST) (Skill.java:535) and cancel whatever the caster casts next - with nothing being cast, that call returns at once
	// (PlayerController.java:520-521), so the check needs a second cast in progress
	setMp(100);
	ASSERT_TRUE(skill(TIMED_SKILL)->useSkill());
	(*client)->clearSent();
	npc->getObserveController()->notifyDeathObservers(*caster.player);
	EXPECT_TRUE(caster.player->isCasting()) << "the self cast goes on";
	EXPECT_FALSE(sentMessage(SM_SYSTEM_MESSAGE::STR_SKILL_TARGET_LOST()));
}

TEST_F(SkillCastTest, TheEffectsAreCreatedAfterTheCastEnded) {
	// Skill.endCast (Skill.java:556-618): the costs are paid and setCasting(null) runs BEFORE the first Effect is created and initialized. The
	// template's <skillatk> is SkillAttackInstantEffect, whose behaviour is the part-3 effect lanes' work: while any body under
	// Effect.initialize is unported, the cast stops there - and the caster must already be free and have paid.
	setMp(100);
	Ref<model::Skill> cast = skill(EFFECT_SKILL);
	std::string stoppedAt;
	try {
		cast->useSkill();
	} catch (const runtime::UnportedException& e) {
		stoppedAt = e.what();
	}
	if (!stoppedAt.empty())
		RecordProperty("first_unported_body", stoppedAt);
	EXPECT_EQ(currentMp(), 93) << "payCastCosts ran first";
	EXPECT_FALSE(caster.player->isCasting()) << "setCasting(null) ran before the effects were created" << (stoppedAt.empty() ? "" : " - " + stoppedAt);
}

TEST_F(SkillCastTest, GetSkillForAsksTheSkillListExceptForAProvokedSkill) {
	SkillEngine& engine = SkillEngine::getInstance();
	// SkillEngine.java:56-68: an ACTIVE skill must be in the skill list, else null
	EXPECT_TRUE(engine.getSkillFor(*caster.player, TIMED_SKILL, nullptr));
	EXPECT_FALSE(engine.getSkillFor(*caster.player, 99999, nullptr)) << "no template";
	learn({INSTANT_SKILL});
	EXPECT_FALSE(engine.getSkillFor(*caster.player, TIMED_SKILL, nullptr)) << "not learned: null, before any Skill is built";

	// A PROVOKED skill skips isSkillPresent and goes straight to `new Skill(template, player, target)`, whose constructor reads the level from
	// the skill list: `effector.getSkillList().getSkillLevel(id)` dereferences the missing entry (Skill.java:107, PlayerSkillList.java). So for
	// an unlearned provoked skill Java answers a NullPointerException where an active one answers null - which is why soul sickness (8291)
	// goes through SkillEngine.getSkill(player, 8291, deathCount, player) and not through getSkillFor (PlayerController.java:723-733).
	EXPECT_THROW(engine.getSkillFor(*caster.player, PROVOKED_SKILL, nullptr), runtime::NullPointerException)
		<< "PROVOKED: isSkillPresent is not asked, the Skill constructor is";

	// learned, a provoked skill is built with the first target kept only when it is a Creature, and the PROVOKED skill method
	learn({INSTANT_SKILL, PROVOKED_SKILL});
	Ref<CastTestNpc> npc = spawnMonster(110.0f, 100.0f, 50.0f);
	Ref<model::Skill> targeted = engine.getSkillFor(*caster.player, PROVOKED_SKILL, npc);
	ASSERT_TRUE(targeted);
	EXPECT_EQ(targeted->getFirstTarget(), Ptr<Creature>(npc));
	EXPECT_EQ(targeted->getSkillMethod(), model::Skill::SkillMethod::PROVOKED);
}

TEST_F(SkillCastTest, GetSkillBuildsAnUnlearnedSkillAtTheGivenLevel) {
	// SkillEngine.java:86-100: getSkill is the path of item skills, npc skills and soul sickness - no skill list, the level is the argument
	Ref<model::Skill> provoked = SkillEngine::getInstance().getSkill(*caster.player, PROVOKED_SKILL, 3, caster.player);
	ASSERT_TRUE(provoked);
	EXPECT_EQ(provoked->getSkillLevel(), 3);
	EXPECT_EQ(provoked->getFirstTarget(), Ptr<Creature>(caster.player));
	EXPECT_FALSE(SkillEngine::getInstance().getSkill(*caster.player, 99999, 1, nullptr));
}

TEST_F(SkillCastTest, ApplyEffectDirectlyOfATemplateIsHeldBackAndTheIdOverloadsCreateARealEffect) {
	// SkillEngine.applyEffectDirectly(template, level, effector, effected) is applyEffect(..., null, ForceType.DEFAULT), i.e. new Effect(...),
	// initialize(), applyEffect() (SkillEngine.java:145-147, 174-179) - the enter-world passive skills. Part 2 ported the path, but every
	// enter-world passive reaches effect leaves that part 3 ports, so the overload stays behind the M5a O-09 partial until then.
	// part 3 (m5b2-plan.md F-02/F-03) removes the holdback; restore: a real Effect of PASSIVE_SKILL, ForceType DEFAULT, no partial hit
	uint64_t partialsBefore = runtime::partialHitCount();
	Ref<model::Effect> effect = SkillEngine::getInstance().applyEffectDirectly(skillTemplate(PASSIVE_SKILL), 1, *caster.player, *caster.player);
	EXPECT_FALSE(effect) << "the held-back overload answers null";
	EXPECT_EQ(runtime::partialHitCount(), partialsBefore + 1) << "through the O-09 AION_PARTIAL, once";
	bool o09Hit = false;
	for (const runtime::PartialHit& hit : runtime::partialHits())
		if (hit.reason == "passive skill effects are not applied yet (M5a O-09)" && hit.hits > 0)
			o09Hit = true;
	EXPECT_TRUE(o09Hit) << "the partial reached is SkillEngine.cpp's O-09 site";
	EXPECT_EQ(runtime::unportedHitCount(), 0u);

	// the id overloads are not held back: they look the template up first and answer null for an unknown id (checkAndGetSkillTemplate,
	// SkillEngine.java:181-188), else a real Effect at the template's lvl with ForceType.DEFAULT (SkillEngine.java:121-124)
	EXPECT_FALSE(SkillEngine::getInstance().applyEffectDirectly(99999, *caster.player, *caster.player));
	Ref<model::Effect> byId = SkillEngine::getInstance().applyEffectDirectly(PASSIVE_SKILL, *caster.player, *caster.player);
	ASSERT_TRUE(byId);
	EXPECT_EQ(byId->getSkillId(), PASSIVE_SKILL);
	EXPECT_EQ(byId->getSkillLevel(), 1) << "the template's lvl";
	EXPECT_EQ(byId->getForceType(), model::Effect_ForceType::DEFAULT);
	EXPECT_EQ(runtime::partialHitCount(), partialsBefore + 1) << "no partial on the id path";
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

} // namespace
} // namespace aion::gameserver::skillengine::test
