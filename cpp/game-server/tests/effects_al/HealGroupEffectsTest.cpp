// The heal effects of P5-03 / P5-04 (M5b-2's heals group, ported 2026-10-02 on the owner's request): CaseHealEffect (with its HP_CHANGED
// observer), HealCastorOnAttackedEffect (with its ATTACKED observer), HealCastorOnTargetDeadEffect, DPTransferEffect, ConvertHealEffect (a
// CONVERT AttackShieldObserver), DPHealEffect, DPHealInstantEffect, ProcDPHealInstantEffect and AbsoluteEXPPointHealInstantEffect (an empty Java
// body). Each case binds the data template that uses the effect (skill_templates.xml, cut to its effect) and runs it on the DaevaEffectTest
// fixture: a case that reaches an AION_UNPORTED site fails in TearDown, so a green case ran through ported code only. The group arms of the two
// HealCastor effects are not run: a group member's HP change reaches PlayerLifeStats.sendGroupPacketUpdate, whose TeamStatUpdater (P5-10) is
// AION_UNPORTED.

#include "DaevaEffectsTestSupport.h"

#include <cstdint>
#include <string>
#include <vector>

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/attack/AttackResult.h"
#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/skillengine/model/HitType.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::skillengine::effecttest {
namespace {

using controllers::attack::AttackResult;
using controllers::attack::AttackStatus;
using gameserver::model::PlayerClass;
using model::Effect;

std::string skillXml(int32_t skillId, std::string_view name, std::string_view subtype, std::string_view effectXml, std::string_view extra = "") {
	return templateXml(R"(skill_id=")" + std::to_string(skillId) + R"(" name=")" + std::string(name) + R"(" nameId="1" stack="TEST_HEAL_)" +
			std::to_string(skillId) + R"(" lvl="1" skilltype="MAGICAL" skillsubtype=")" + std::string(subtype) + R"(" tslot=")" +
			(subtype == "DEBUFF" ? "DEBUFF" : subtype == "NONE" ? "NONE" : "BUFF") + R"(" activation="ACTIVE" cooldown="0" duration="0")" +
			std::string(extra),
		effectXml);
}

// ---- the effects of the data (skill_templates.xml), each on its own template -----------------------------------------------------------------

/** 3924 Saving Grace (CaseHealEffect): heals once the HP fall to 50 % */
const std::string CASE_HEAL_XML = skillXml(3924, "Saving Grace", "HEAL",
	R"(<caseheal type="HP" cond_value="50" value="1713" duration2="60000" effectid="155" e="1" noresist="true" />)");
/** 4631 Healing Conduit (HealCastorOnAttackedEffect, element healcastoronatk) */
const std::string HEAL_ON_ATTACKED_XML = skillXml(4631, "Healing Conduit", "DEBUFF",
	R"(<healcastoronatk type="HP" range="15.0" value="23" duration2="10000" effectid="210" e="1" noresist="true" element="FIRE" />)",
	R"( hostile_type="DIRECT")");
/** 19573 Offering (HealCastorOnTargetDeadEffect) */
const std::string HEAL_ON_DEAD_XML = skillXml(19573, "Offering", "DEBUFF",
	R"(<healcastorontargetdead type="HP" range="100.0" value="100" duration2="45000" effectid="211" e="1" noresist="true" element="EARTH" />)",
	R"( hostile_type="DIRECT")");
/** 248 DP Transfer (DPTransferEffect) */
const std::string DP_TRANSFER_XML = skillXml(248, "DP Transfer", "NONE", R"(<dptransfer e="1" noresist="true" />)", R"( hostile_type="INDIRECT")");
/**
 * 1216 Absolute Zero's convert heal (ConvertHealEffect), with a hit value of 50 % (the data's templates heal by none). Java passes the template's
 * `percent` as the observer's hitPercent (the heal is a percent of the hit) and `hitpercent` as its totalHitPercent (the total is a percent of
 * the Health stat): this one has a percent heal and a flat total 150.
 */
const std::string CONVERT_HEAL_XML = skillXml(1216, "Absolute Zero", "BUFF",
	R"(<convertheal type="HP" value="150" hitvalue="50" percent="true" duration2="8000" e="1" noresist="true" hittype="EVERYHIT" />)");
/** 8740 Recover (DPHealEffect) */
const std::string DP_HEAL_XML = skillXml(8740, "Recover", "BUFF", R"(<dpheal checktime="1000" value="100" duration2="6000" effectid="187402" e="1" />)");
/** 10164 DP Recovery (DPHealInstantEffect) */
const std::string DP_HEAL_INSTANT_XML = skillXml(10164, "DP Recovery", "NONE", R"(<dphealinstant delta="1000" e="1" noresist="true" element="FIRE" />)");
/** 8387 Daevic Efflux (ProcDPHealInstantEffect) */
const std::string PROC_DP_HEAL_INSTANT_XML = skillXml(8387, "Daevic Efflux", "BUFF", R"(<procdphealinstant value="1000" e="1" noresist="true" />)");
/** 10954 Practitioner's Gift (AbsoluteEXPPointHealInstantEffect, element absexppointhealinstant) */
const std::string ABS_EXP_XML = skillXml(10954, "Practitioner's Gift", "NONE", R"(<absexppointhealinstant value="10000000" e="1" noresist="true" />)");

class HealGroupEffectsTest : public DaevaEffectTest {
protected:
	/** One hit of `damage` on the effected, through its shield observers (as ShieldEffectsTest) */
	static Ref<AttackResult> hit(Creature& effected, Creature& attacker, int32_t damage) {
		Ref<AttackResult> result = AttackResult::create(static_cast<float>(damage), AttackStatus::NORMALHIT, model::HitType::PHHIT);
		effected.getObserveController()->checkShieldStatus(std::vector<Ptr<AttackResult>>{Ptr<AttackResult>(result)}, nullptr, attacker);
		return result;
	}
};

// ---- CaseHealEffect -------------------------------------------------------------------------------------------------------------------------

TEST_F(HealGroupEffectsTest, ACaseHealWaitsUntilTheHpFallToItsPercentThenHealsOnceAndEnds) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* skill = bindSkill(CASE_HEAL_XML);
	ASSERT_EQ(effectOf(*skill, 0).javaClassName(), "CaseHealEffect");
	Ref<Player> cleric = daeva(8901, PlayerClass::CLERIC);
	const int32_t maxHp = cleric->getLifeStats()->getMaxHp();

	// startEffect: tryHeal does not heal at full HP, so the effect stays with an HP_CHANGED observer
	Ref<Effect> effect = cast(*cleric, *cleric, skill, 1);
	ASSERT_TRUE(cleric->getEffectController()->hasAbnormalEffect(3924));
	cleric->getLifeStats()->setCurrentHp(maxHp / 2 + 1);
	EXPECT_EQ(cleric->getLifeStats()->getCurrentHp(), maxHp / 2 + 1) << "above 50 %: no heal";
	EXPECT_TRUE(cleric->getEffectController()->hasAbnormalEffect(3924));

	// the HP change to 50 % or below heals and ends the effect
	cleric->getLifeStats()->setCurrentHp(maxHp / 4);
	EXPECT_GT(cleric->getLifeStats()->getCurrentHp(), maxHp / 4) << "healed by the observer";
	EXPECT_FALSE(cleric->getEffectController()->hasAbnormalEffect(3924)) << "effect.endEffect()";
	cleric->getLifeStats()->setCurrentHp(maxHp / 4);
	EXPECT_EQ(cleric->getLifeStats()->getCurrentHp(), maxHp / 4) << "the observer went with the effect";
}

TEST_F(HealGroupEffectsTest, ACaseHealAtLowHpHealsAtOnce) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* skill = bindSkill(CASE_HEAL_XML);
	Ref<Player> cleric = daeva(8902, PlayerClass::CLERIC);
	const int32_t maxHp = cleric->getLifeStats()->getMaxHp();
	cleric->getLifeStats()->setCurrentHp(maxHp / 2);

	cast(*cleric, *cleric, skill, 1);
	EXPECT_GT(cleric->getLifeStats()->getCurrentHp(), maxHp / 2) << "exactly 50 % heals (currentValue <= max * cond / 100f)";
	EXPECT_FALSE(cleric->getEffectController()->hasAbnormalEffect(3924));
}

// ---- HealCastorOnAttackedEffect / HealCastorOnTargetDeadEffect -------------------------------------------------------------------------------

TEST_F(HealGroupEffectsTest, EachHitOnTheConduitHealsItsCasterInRange) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* skill = bindSkill(HEAL_ON_ATTACKED_XML);
	ASSERT_EQ(effectOf(*skill, 0).javaClassName(), "HealCastorOnAttackedEffect");
	Ref<Player> chanter = daeva(8903, PlayerClass::CHANTER);
	Ref<Npc> monster = makeMonster(701903, 505, 500);
	const int32_t maxHp = chanter->getLifeStats()->getMaxHp();
	chanter->getLifeStats()->setCurrentHp(maxHp / 2);

	Ref<Effect> effect = forced(*chanter, *monster, skill, 1);
	ASSERT_TRUE(monster->getEffectController()->hasAbnormalEffect(4631));
	monster->getObserveController()->notifyAttackedObservers(*chanter, 0);
	EXPECT_EQ(chanter->getLifeStats()->getCurrentHp(), maxHp / 2 + 23) << "calculateBaseValue: value 23 at level 1, no group";
	monster->getObserveController()->notifyAttackedObservers(*chanter, 0);
	EXPECT_EQ(chanter->getLifeStats()->getCurrentHp(), maxHp / 2 + 46) << "each hit";

	// out of the range 15 of the effected: no heal
	world::World::getInstance().updatePosition(*chanter, 530, 500, 100, 0);
	monster->getObserveController()->notifyAttackedObservers(*chanter, 0);
	EXPECT_EQ(chanter->getLifeStats()->getCurrentHp(), maxHp / 2 + 46);

	// endEffect removes the observer
	world::World::getInstance().updatePosition(*chanter, 500, 500, 100, 0);
	effect->endEffect();
	monster->getObserveController()->notifyAttackedObservers(*chanter, 0);
	EXPECT_EQ(chanter->getLifeStats()->getCurrentHp(), maxHp / 2 + 46);
}

TEST_F(HealGroupEffectsTest, AnOfferingHealsItsCasterOnlyWhenTheTargetDiedUnderIt) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* skill = bindSkill(HEAL_ON_DEAD_XML);
	ASSERT_EQ(effectOf(*skill, 0).javaClassName(), "HealCastorOnTargetDeadEffect");
	Ref<Player> caster = daeva(8904, PlayerClass::SORCERER);
	ASSERT_GT(caster->getLifeStats()->getMaxHp(), 110);
	caster->getLifeStats()->setCurrentHp(10);

	// ends on a living target: no heal
	Ref<Npc> living = makeMonster(701904, 505, 500);
	forced(*caster, *living, skill, 1)->endEffect();
	EXPECT_EQ(caster->getLifeStats()->getCurrentHp(), 10);

	// ends on a dead target in range 100: the caster gets calculateBaseValue
	Ref<Npc> dying = makeMonster(701905, 505, 500);
	Ref<Effect> effect = forced(*caster, *dying, skill, 1);
	dying->getLifeStats()->setCurrentHp(0);
	ASSERT_TRUE(dying->isDead());
	effect->endEffect();
	EXPECT_EQ(caster->getLifeStats()->getCurrentHp(), 110);
}

// ---- DPTransferEffect -----------------------------------------------------------------------------------------------------------------------

TEST_F(HealGroupEffectsTest, DpTransferGivesAllOfTheCastersDpToTheTarget) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* skill = bindSkill(DP_TRANSFER_XML);
	ASSERT_EQ(effectOf(*skill, 0).javaClassName(), "DPTransferEffect");
	Ref<Player> giver = daeva(8905, PlayerClass::CHANTER);
	Ref<Player> taker = daeva(8906, PlayerClass::GLADIATOR, 502);
	giver->getCommonData()->setDp(1500);
	taker->getCommonData()->setDp(200);

	cast(*giver, *taker, skill, 1);
	EXPECT_EQ(taker->getCommonData()->getDp(), 1700) << "calculate reserves the effector's DP, applyEffect adds it to the effected";
	EXPECT_EQ(giver->getCommonData()->getDp(), 0) << "and takes it from the effector";
}

// ---- ConvertHealEffect ----------------------------------------------------------------------------------------------------------------------

TEST_F(HealGroupEffectsTest, AConvertHealAbsorbsTheHitAndHealsByItsPercent) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* skill = bindSkill(CONVERT_HEAL_XML);
	ASSERT_EQ(effectOf(*skill, 0).javaClassName(), "ConvertHealEffect");
	Ref<Player> mage = daeva(8907, PlayerClass::SORCERER);
	Ref<Npc> monster = makeMonster(701907, 505, 500);
	ASSERT_GT(mage->getLifeStats()->getMaxHp(), 110);
	mage->getLifeStats()->setCurrentHp(10);

	cast(*mage, *mage, skill, 1);
	// hit 100: the total 150 absorbs it all, and heals 100 * 50 / 100
	EXPECT_EQ(hit(*mage, *monster, 100)->getDamage(), 0);
	EXPECT_EQ(mage->getLifeStats()->getCurrentHp(), 60);
	EXPECT_TRUE(mage->getEffectController()->hasAbnormalEffect(1216));
	// hit 100 again: 50 left to absorb, 50 get through; the heal is of the whole hit; the empty total ends the effect
	EXPECT_EQ(hit(*mage, *monster, 100)->getDamage(), 50);
	EXPECT_EQ(mage->getLifeStats()->getCurrentHp(), 110);
	EXPECT_FALSE(mage->getEffectController()->hasAbnormalEffect(1216));
}

// ---- the DP heals ---------------------------------------------------------------------------------------------------------------------------

TEST_F(HealGroupEffectsTest, TheInstantDpHealsAddTheirValueAndTheDpHealOverTimeTicks) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* instant = bindSkill(DP_HEAL_INSTANT_XML);
	const model::SkillTemplate* proc = bindSkill(PROC_DP_HEAL_INSTANT_XML);
	const model::SkillTemplate* overTime = bindSkill(DP_HEAL_XML);
	ASSERT_EQ(effectOf(*instant, 0).javaClassName(), "DPHealInstantEffect");
	ASSERT_EQ(effectOf(*proc, 0).javaClassName(), "ProcDPHealInstantEffect");
	ASSERT_EQ(effectOf(*overTime, 0).javaClassName(), "DPHealEffect");
	Ref<Player> player = daeva(8908, PlayerClass::GLADIATOR);
	ASSERT_GE(player->getGameStats()->getMaxDp()->getCurrent(), 2200);
	player->getCommonData()->setDp(0);

	cast(*player, *player, instant, 1);
	EXPECT_EQ(player->getCommonData()->getDp(), 1000) << "DPHealInstantEffect: delta 1000 at level 1";
	cast(*player, *player, proc, 1);
	EXPECT_EQ(player->getCommonData()->getDp(), 2000) << "ProcDPHealInstantEffect: value 1000";

	cast(*player, *player, overTime, 1);
	EXPECT_EQ(player->getCommonData()->getDp(), 2000) << "DPHealEffect: nothing before the first tick";
	advance(1299);
	EXPECT_EQ(player->getCommonData()->getDp(), 2000) << "AbstractOverTimeEffect: the first tick after 300 + checktime";
	advance(1);
	EXPECT_EQ(player->getCommonData()->getDp(), 2100) << "value 100 every checktime";
	advance(1000);
	EXPECT_EQ(player->getCommonData()->getDp(), 2200);
}

TEST_F(HealGroupEffectsTest, TheDpHealsCapAtTheMaxDp) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* instant = bindSkill(DP_HEAL_INSTANT_XML);
	Ref<Player> player = daeva(8909, PlayerClass::GLADIATOR);
	const int32_t maxDp = player->getGameStats()->getMaxDp()->getCurrent();
	player->getCommonData()->setDp(maxDp - 10);
	cast(*player, *player, instant, 1);
	EXPECT_EQ(player->getCommonData()->getDp(), maxDp) << "AbstractHealEffect.calculate: the heal is limited to max - current";
}

// ---- AbsoluteEXPPointHealInstantEffect ------------------------------------------------------------------------------------------------------

TEST_F(HealGroupEffectsTest, ThePractitionersGiftDoesNothingYet) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* skill = bindSkill(ABS_EXP_XML);
	ASSERT_EQ(effectOf(*skill, 0).javaClassName(), "AbsoluteEXPPointHealInstantEffect");
	Ref<Player> player = daeva(8910, PlayerClass::CLERIC);
	const int64_t exp = player->getCommonData()->getExp();
	cast(*player, *player, skill, 1);
	EXPECT_EQ(player->getCommonData()->getExp(), exp) << "AbsoluteEXPPointHealInstantEffect.applyEffect is an empty Java body";
}

} // namespace
} // namespace aion::gameserver::skillengine::effecttest
