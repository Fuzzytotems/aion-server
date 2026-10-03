// The combat effects of P5-03 / P5-04 (M5b-2's combat group, ported 2026-10-03 on the owner's request): MagicCounterAtkEffect,
// SpellAtkDrainEffect, SpellAtkDrainInstantEffect, NoReduceSpellATKInstantEffect, OneTimeBoostSkillCriticalEffect, ProtectEffect,
// SwitchHostileEffect, SwitchHpMpEffect, ChangeHateOnAttackedEffect, MpAttackEffect, BoostSkillCostEffect, MoveBehindEffect, RebirthEffect,
// ResurrectPositionalEffect, and the three empty Java bodies AlwaysHitEffect, AlwaysNoResistEffect and LimitedReduceDamageEffect. Each case binds
// the data template that uses the effect (skill_templates.xml, cut to its effect; no data skill uses <changehateonattacked>, so its case builds
// one) and runs it on the DaevaEffectTest fixture: a case that reaches an AION_UNPORTED site fails in TearDown, so a green case ran through
// ported code only.

#include "DaevaEffectsTestSupport.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/SummonController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/observer/AttackerCriticalStatus.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/templates/detail/JavaCasts.h"
#include "aion/gameserver/skillengine/model/DashStatus.h"
#include "aion/gameserver/skillengine/model/EffectReserved.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/utils/stats/StatFunctions.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::skillengine::effecttest {
namespace {

using controllers::attack::AttackStatus;
using gameserver::model::PlayerClass;
using gameserver::model::gameobjects::Summon;
using gameserver::model::gameobjects::VisibleObject;
using model::Effect;

std::string skillXml(int32_t skillId, std::string_view name, std::string_view type, std::string_view subtype, std::string_view effectXml,
	std::string_view extra = "") {
	return templateXml(R"(skill_id=")" + std::to_string(skillId) + R"(" name=")" + std::string(name) + R"(" nameId="1" stack="TEST_COMBAT_)" +
			std::to_string(skillId) + R"(" lvl="1" skilltype=")" + std::string(type) + R"(" skillsubtype=")" + std::string(subtype) + R"(" tslot=")" +
			(subtype == "DEBUFF" ? "DEBUFF" : subtype == "BUFF" ? "BUFF" : "NONE") + R"(" activation="ACTIVE" cooldown="0" duration="0")" +
			std::string(extra),
		effectXml);
}

constexpr std::string_view DIRECT = R"( hostile_type="DIRECT")";

// ---- the effects of the data (skill_templates.xml), each on its own template -----------------------------------------------------------------

/** 1329 Curse of Weakness (MagicCounterAtkEffect), without its hop */
const std::string MAGIC_COUNTER_XML = skillXml(1329, "Curse of Weakness", "MAGICAL", "DEBUFF",
	R"(<magiccounteratk maxdmg="605" value="12" duration2="60000" effectid="203" e="1" noresist="true" element="FIRE" />)", DIRECT);
/** a magical and a physical skill that the cursed monster casts */
const std::string MONSTER_SPELL_XML = skillXml(64301, "monster spell", "MAGICAL", "ATTACK", R"(<dummy e="1" />)");
const std::string MONSTER_BLOW_XML = skillXml(64302, "monster blow", "PHYSICAL", "ATTACK", R"(<dummy e="1" />)");
/** 3849 Blood Funnel (SpellAtkDrainEffect) */
const std::string SPELL_ATK_DRAIN_XML = skillXml(3849, "Blood Funnel", "MAGICAL", "DEBUFF",
	R"(<spellatkdrain hp_percent="100" checktime="3000" value="80" duration2="30000" effectid="112781" e="1" noresist="true" element="FIRE" hoptype="DAMAGE" />)",
	DIRECT);
/** 1250 Refracting Shard (SpellAtkDrainInstantEffect) */
const std::string SPELL_ATK_DRAIN_INSTANT_XML = skillXml(1250, "Refracting Shard", "MAGICAL", "ATTACK",
	R"(<spellatkdraininstant mp_percent="50" value="279" e="1" noresist="true" element="WATER" hoptype="DAMAGE" />)", DIRECT);
/** 324 Shredding Blow (NoReduceSpellATKInstantEffect), and two percent variants of it */
const std::string NO_REDUCE_XML = skillXml(324, "Shredding Blow", "PHYSICAL", "ATTACK",
	R"(<noreducespellatk value="600" e="1" noresist="true" element="FIRE" hoptype="SKILLLV" />)", DIRECT);
const std::string NO_REDUCE_PERCENT_XML = skillXml(64303, "Shredding Blow, percent", "PHYSICAL", "ATTACK",
	R"(<noreducespellatk percent="true" value="10" e="1" noresist="true" element="FIRE" />)", DIRECT);
const std::string NO_REDUCE_CAPPED_XML = skillXml(64304, "Shredding Blow, capped", "PHYSICAL", "ATTACK",
	R"(<noreducespellatk percent="true" value="50" max_damage="20" e="1" noresist="true" element="FIRE" />)", DIRECT);
/** 888 Hunter's Might (OneTimeBoostSkillCriticalEffect) */
const std::string ONE_TIME_CRITICAL_XML = skillXml(888, "Hunter's Might", "MAGICAL", "BUFF",
	R"(<onetimeboostskillcritical count="3" value="1000" duration2="20000" effectid="176" e="1" noresist="true" />)");
/** 2975 Bodyguard (ProtectEffect) */
const std::string PROTECT_XML = skillXml(2975, "Bodyguard", "MAGICAL", "BUFF",
	R"(<protect percent="true" hitvalue="100" radius="25" value="100" duration2="30000" effectid="151" e="1" noresist="true" hittype="EVERYHIT" />)");
/** 3739 Emnity Swap (SwitchHostileEffect) */
const std::string SWITCH_HOSTILE_XML = skillXml(3739, "Emnity Swap", "MAGICAL", "NONE", R"(<switchhostile e="1" noresist="true" />)", DIRECT);
/** 1327 Exchange Vitality (SwitchHpMpEffect) */
const std::string SWITCH_HP_MP_XML = skillXml(1327, "Exchange Vitality", "MAGICAL", "NONE", R"(<switchhpmp e="1" noresist="true" />)");
/** no data skill uses <changehateonattacked> (ChangeHateOnAttackedEffect): a template with value1 100 and value2 50 */
const std::string CHANGE_HATE_XML = skillXml(64305, "change hate on attacked", "MAGICAL", "BUFF",
	R"(<changehateonattacked value1="100" value2="50" duration2="10000" e="1" noresist="true" />)");
/** 12125 Fissure Rend's MP attack (MpAttackEffect), and a flat variant */
const std::string MP_ATTACK_XML = skillXml(12125, "Fissure Rend", "MAGICAL", "DEBUFF",
	R"(<mpattack percent="true" checktime="1000" value="10" duration2="3000" effectid="121253" e="1" noresist="true" element="EARTH" />)", DIRECT);
const std::string MP_ATTACK_FLAT_XML = skillXml(64306, "Fissure Rend, flat", "MAGICAL", "DEBUFF",
	R"(<mpattack checktime="1000" value="7" duration2="3000" e="1" noresist="true" element="EARTH" />)", DIRECT);
/** 1126 Sharpen Arrows (BoostSkillCostEffect) */
const std::string BOOST_SKILL_COST_XML = skillXml(1126, "Sharpen Arrows", "MAGICAL", "BUFF",
	R"(<boostskillcost value="-10" duration2="30000" effectid="173" e="1" noresist="true" />)");
/** 3239 Fangdrop Stab (MoveBehindEffect) */
const std::string MOVE_BEHIND_XML =
	skillXml(3239, "Fangdrop Stab", "MAGICAL", "ATTACK", R"(<movebehind value="300" e="1" noresist="true" hoptype="DAMAGE" />)", DIRECT);
/** 3923 Brilliant Protection's rebirth (RebirthEffect) */
const std::string REBIRTH_XML = skillXml(3923, "Brilliant Protection", "MAGICAL", "BUFF",
	R"(<rebirth resurrect_percent="30" duration2="60000" effectid="160" e="1" noresist="true" />)");
/** 4004 Resurrection Loci (ResurrectPositionalEffect) */
const std::string RESURRECT_POSITIONAL_XML =
	skillXml(4004, "Resurrection Loci", "MAGICAL", "NONE", R"(<resurrectpos skill_id="8295" e="1" noresist="true" hoptype="SKILLLV" />)");
/** 13194 Blessing: Successful Attack I (AlwaysHitEffect, AlwaysNoResistEffect) and 8887 Evasion Rate Increase V (LimitedReduceDamageEffect) */
const std::string ALWAYS_HIT_XML = skillXml(13194, "Blessing: Successful Attack I", "MAGICAL", "NONE",
	R"(<alwayshit duration2="3000" effectid="141" e="1" noresist="true" /><alwaysnoresist duration2="3000" effectid="205" e="2" noresist="true" />)");
const std::string LIMITED_REDUCE_XML = skillXml(8887, "Evasion Rate Increase V", "MAGICAL", "NONE", R"(<limitedreduceDamage e="1" />)");

using CombatGroupEffectsTest = DaevaEffectTest;

// ---- MagicCounterAtkEffect ------------------------------------------------------------------------------------------------------------------

TEST_F(CombatGroupEffectsTest, ACursedMonsterIsHitByEachMagicalSkillItFinishes) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* curse = bindSkill(MAGIC_COUNTER_XML);
	const model::SkillTemplate* spell = bindSkill(MONSTER_SPELL_XML);
	const model::SkillTemplate* blow = bindSkill(MONSTER_BLOW_XML);
	ASSERT_EQ(effectOf(*curse, 0).javaClassName(), "MagicCounterAtkEffect");
	Ref<Player> sorcerer = daeva(9001, PlayerClass::SORCERER);
	Ref<Npc> monster = makeMonster(702001, 505, 500);
	forced(*sorcerer, *monster, curse, 1);
	ASSERT_TRUE(monster->getEffectController()->hasAbnormalEffect(1329));

	// a physical skill: nothing
	const int32_t hp = monster->getLifeStats()->getCurrentHp();
	monster->getObserveController()->notifyEndSkillCastObservers(*model::Skill::create(blow, *monster, 1, Ptr<Creature>(sorcerer), nullptr));
	EXPECT_EQ(monster->getLifeStats()->getCurrentHp(), hp);

	// a magical one: maxHp base * 12 / 100f, through the PvE modifiers, capped by maxdmg 605
	const float maxHpDamage = static_cast<float>(monster->getGameStats()->getMaxHp()->getBase() * 12) / 100.0f;
	const int32_t expected = gameserver::model::templates::detail::floatToInt(std::min(605.0f,
		utils::stats::StatFunctions::adjustDamageByPvpOrPveModifiers(*sorcerer, *monster, maxHpDamage, curse->getPvpDamage(), false,
			gameserver::model::SkillElement::FIRE)));
	ASSERT_GT(expected, 0);
	monster->getObserveController()->notifyEndSkillCastObservers(*model::Skill::create(spell, *monster, 1, Ptr<Creature>(sorcerer), nullptr));
	EXPECT_EQ(monster->getLifeStats()->getCurrentHp(), std::max(0, hp - expected));
}

// ---- SpellAtkDrainEffect / SpellAtkDrainInstantEffect --------------------------------------------------------------------------------------

TEST_F(CombatGroupEffectsTest, BloodFunnelDrainsItsDamageIntoTheCastersHpEveryTick) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* skill = bindSkill(SPELL_ATK_DRAIN_XML);
	ASSERT_EQ(effectOf(*skill, 0).javaClassName(), "SpellAtkDrainEffect");
	Ref<Player> caster = daeva(9002, PlayerClass::SORCERER);
	Ref<Npc> monster = makeMonster(702002, 505, 500);
	caster->getLifeStats()->setCurrentHp(1);
	const int32_t monsterHp = monster->getLifeStats()->getCurrentHp();

	forced(*caster, *monster, skill, 1);
	advance(3299);
	EXPECT_EQ(monster->getLifeStats()->getCurrentHp(), monsterHp) << "AbstractOverTimeEffect: the first tick after 300 + checktime";
	advance(1);
	const int32_t damage = monsterHp - monster->getLifeStats()->getCurrentHp();
	ASSERT_GT(damage, 0);
	EXPECT_EQ(caster->getLifeStats()->getCurrentHp(), std::min(caster->getLifeStats()->getMaxHp(), 1 + damage)) << "hp_percent 100";
}

TEST_F(CombatGroupEffectsTest, RefractingShardGivesHalfItsDamageAsMpASecondLater) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* skill = bindSkill(SPELL_ATK_DRAIN_INSTANT_XML);
	ASSERT_EQ(effectOf(*skill, 0).javaClassName(), "SpellAtkDrainInstantEffect");
	Ref<Player> caster = daeva(9003, PlayerClass::SORCERER);
	Ref<Npc> monster = makeMonster(702003, 505, 500);
	caster->getLifeStats()->setCurrentMp(0);

	Ref<Effect> effect = forced(*caster, *monster, skill, 1);
	const int32_t damage = effect->getReserveds(1)->getValue();
	ASSERT_GT(damage, 0);
	advance(999);
	EXPECT_EQ(caster->getLifeStats()->getCurrentMp(), 0);
	advance(1);
	EXPECT_EQ(caster->getLifeStats()->getCurrentMp(), std::min(caster->getLifeStats()->getMaxMp(), damage * 50 / 100)) << "mp_percent 50";
}

// ---- NoReduceSpellATKInstantEffect ----------------------------------------------------------------------------------------------------------

TEST_F(CombatGroupEffectsTest, ShreddingBlowDealsItsTemplateDamageOrAPercentOfTheMaxHp) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* flat = bindSkill(NO_REDUCE_XML);
	const model::SkillTemplate* percent = bindSkill(NO_REDUCE_PERCENT_XML);
	const model::SkillTemplate* capped = bindSkill(NO_REDUCE_CAPPED_XML);
	ASSERT_EQ(effectOf(*flat, 0).javaClassName(), "NoReduceSpellATKInstantEffect");
	Ref<Player> templar = daeva(9004, PlayerClass::TEMPLAR);
	Ref<Npc> monster = makeMonster(702004, 505, 500);
	const int32_t maxHp = monster->getLifeStats()->getMaxHp();

	EXPECT_EQ(calculated(*templar, *monster, flat, 1)->getReserveds(1)->getValue(), 600) << "useTemplateDmg: no reduction";
	EXPECT_EQ(calculated(*templar, *monster, percent, 1)->getReserveds(1)->getValue(),
		gameserver::model::templates::detail::floatToInt(static_cast<float>(maxHp) * (10 / 100.0f)))
		<< "(int) (maxHp * value / 100f)";
	EXPECT_EQ(calculated(*templar, *monster, capped, 1)->getReserveds(1)->getValue(), 20) << "max_damage";
}

// ---- OneTimeBoostSkillCriticalEffect --------------------------------------------------------------------------------------------------------

TEST_F(CombatGroupEffectsTest, HuntersMightMakesTheNextThreeSkillsCriticalThenEnds) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* skill = bindSkill(ONE_TIME_CRITICAL_XML);
	ASSERT_EQ(effectOf(*skill, 0).javaClassName(), "OneTimeBoostSkillCriticalEffect");
	Ref<Player> ranger = daeva(9005, PlayerClass::RANGER);
	forced(*ranger, *ranger, skill, 1);
	ASSERT_TRUE(ranger->getEffectController()->hasAbnormalEffect(888));

	controllers::ObserveController& observers = *ranger->getObserveController();
	EXPECT_FALSE(observers.checkAttackerCriticalStatus(AttackStatus::CRITICAL, false)->isResult()) << "an auto attack is no skill";
	EXPECT_TRUE(observers.checkAttackerCriticalStatus(AttackStatus::CRITICAL, true)->isResult());
	EXPECT_TRUE(observers.checkAttackerCriticalStatus(AttackStatus::CRITICAL, true)->isResult());
	EXPECT_TRUE(ranger->getEffectController()->hasAbnormalEffect(888)) << "one left";
	EXPECT_TRUE(observers.checkAttackerCriticalStatus(AttackStatus::CRITICAL, true)->isResult());
	EXPECT_FALSE(ranger->getEffectController()->hasAbnormalEffect(888)) << "the third ends the effect";
}

// ---- ProtectEffect --------------------------------------------------------------------------------------------------------------------------

TEST_F(CombatGroupEffectsTest, ABodyguardsProtectionEndsWhenTheBodyguardDies) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* skill = bindSkill(PROTECT_XML);
	ASSERT_EQ(effectOf(*skill, 0).javaClassName(), "ProtectEffect");
	Ref<Player> templar = daeva(9006, PlayerClass::TEMPLAR);
	Ref<Player> cleric = daeva(9007, PlayerClass::CLERIC, 502);
	Ref<Npc> monster = makeMonster(702007, 505, 500);
	forced(*templar, *cleric, skill, 1);
	ASSERT_TRUE(cleric->getEffectController()->hasAbnormalEffect(2975));

	// the DeathObserver on the effector (not a summon)
	monster->getObserveController()->notifyDeathObservers(*templar);
	EXPECT_TRUE(cleric->getEffectController()->hasAbnormalEffect(2975)) << "another creature's death";
	templar->getObserveController()->notifyDeathObservers(*monster);
	EXPECT_FALSE(cleric->getEffectController()->hasAbnormalEffect(2975)) << "the bodyguard's death ends it";
}

// ---- SwitchHostileEffect / SwitchHpMpEffect / ChangeHateOnAttackedEffect -------------------------------------------------------------------

TEST_F(CombatGroupEffectsTest, EmnitySwapSwapsTheHateOfTheSpiritmasterAndTheSpirit) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* skill = bindSkill(SWITCH_HOSTILE_XML);
	ASSERT_EQ(effectOf(*skill, 0).javaClassName(), "SwitchHostileEffect");
	Ref<Player> master = daeva(9008, PlayerClass::SPIRIT_MASTER);
	Ref<Npc> monster = makeMonster(702008, 505, 500);
	ASSERT_TRUE(KnownListPairing::pair(*monster, *master));
	controllers::attack::AggroList& aggro = monster->getAggroList();
	aggro.addHate(*master, 100);

	// no summon: nothing
	forced(*master, *monster, skill, 1);
	EXPECT_EQ(aggro.getHate(*master), 100);

	Ref<Summon> spirit = VisibleObject::create<Summon>(utils::idfactory::IDFactory::getInstance().nextId(),
		std::make_unique<controllers::SummonController>(), *monster->getSpawn(), monster->getObjectTemplate(), *master, 0);
	spirit->setKnownlist(std::make_unique<world::knownlist::KnownList>(*spirit));
	place(*spirit, 503, 500, 100);
	master->setSummon(Ptr<Summon>(spirit));
	ASSERT_TRUE(KnownListPairing::pair(*monster, *spirit));
	aggro.addHate(*spirit, 30);
	forced(*master, *monster, skill, 1);
	EXPECT_EQ(aggro.getHate(*master), 30);
	EXPECT_EQ(aggro.getHate(*spirit), 100);
	master->setSummon(nullptr);
}

TEST_F(CombatGroupEffectsTest, ExchangeVitalitySwapsTheHpAndTheMp) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* skill = bindSkill(SWITCH_HP_MP_XML);
	ASSERT_EQ(effectOf(*skill, 0).javaClassName(), "SwitchHpMpEffect");
	Ref<Player> cleric = daeva(9009, PlayerClass::CLERIC);
	cleric->getLifeStats()->setCurrentHp(50);
	cleric->getLifeStats()->setCurrentMp(80);
	forced(*cleric, *cleric, skill, 1);
	EXPECT_EQ(cleric->getLifeStats()->getCurrentHp(), std::min(80, cleric->getLifeStats()->getMaxHp()));
	EXPECT_EQ(cleric->getLifeStats()->getCurrentMp(), std::min(50, cleric->getLifeStats()->getMaxMp()));
}

TEST_F(CombatGroupEffectsTest, AnNpcThatAttacksTheEffectedHatesItMore) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* skill = bindSkill(CHANGE_HATE_XML);
	ASSERT_EQ(effectOf(*skill, 0).javaClassName(), "ChangeHateOnAttackedEffect");
	Ref<Player> templar = daeva(9010, PlayerClass::TEMPLAR);
	Ref<Npc> monster = makeMonster(702010, 505, 500);
	ASSERT_TRUE(KnownListPairing::pair(*monster, *templar));
	Ref<Effect> effect = forced(*templar, *templar, skill, 1);

	templar->getObserveController()->notifyAttackedObservers(*monster, 0);
	EXPECT_EQ(monster->getAggroList().getHate(*templar), 150) << "value1 + value2";
	effect->endEffect();
	templar->getObserveController()->notifyAttackedObservers(*monster, 0);
	EXPECT_EQ(monster->getAggroList().getHate(*templar), 150) << "the observer went with the effect";
}

// ---- MpAttackEffect / BoostSkillCostEffect --------------------------------------------------------------------------------------------------

TEST_F(CombatGroupEffectsTest, FissureRendBurnsMpEveryTick) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* percent = bindSkill(MP_ATTACK_XML);
	const model::SkillTemplate* flat = bindSkill(MP_ATTACK_FLAT_XML);
	ASSERT_EQ(effectOf(*percent, 0).javaClassName(), "MpAttackEffect");
	Ref<Player> caster = daeva(9012, PlayerClass::SORCERER);
	Ref<Player> victim = daeva(9013, PlayerClass::CLERIC, 502);
	const int32_t maxMp = victim->getLifeStats()->getMaxMp();
	ASSERT_GE(maxMp, 40);

	forced(*caster, *victim, percent, 1);
	advance(1299);
	EXPECT_EQ(victim->getLifeStats()->getCurrentMp(), maxMp);
	advance(1);
	EXPECT_EQ(victim->getLifeStats()->getCurrentMp(), maxMp - maxMp * 10 / 100) << "percent: maxMp * value / 100";
	advance(5000);
	const int32_t afterPercent = victim->getLifeStats()->getCurrentMp();

	forced(*caster, *victim, flat, 1);
	advance(1300);
	EXPECT_EQ(victim->getLifeStats()->getCurrentMp(), afterPercent - 7) << "value 7";
}

TEST_F(CombatGroupEffectsTest, SharpenArrowsBoostsTheCostOfEachSkill) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* skill = bindSkill(BOOST_SKILL_COST_XML);
	const model::SkillTemplate* spell = bindSkill(MONSTER_SPELL_XML);
	ASSERT_EQ(effectOf(*skill, 0).javaClassName(), "BoostSkillCostEffect");
	Ref<Player> ranger = daeva(9014, PlayerClass::RANGER);
	Ref<Effect> effect = forced(*ranger, *ranger, skill, 1);

	Ref<model::Skill> next = model::Skill::create(spell, *ranger, 1, Ptr<Creature>(ranger), nullptr);
	ranger->getObserveController()->notifyBoostSkillCostObservers(*next);
	EXPECT_EQ(next->getBoostSkillCost(), -10);
	effect->endEffect();
	Ref<model::Skill> after = model::Skill::create(spell, *ranger, 1, Ptr<Creature>(ranger), nullptr);
	ranger->getObserveController()->notifyBoostSkillCostObservers(*after);
	EXPECT_EQ(after->getBoostSkillCost(), 0);
}

// ---- MoveBehindEffect -----------------------------------------------------------------------------------------------------------------------

/**
 * 3239 Fangdrop Stab from (500, 500, 100) on a monster at (510, 500, 100) that faces west (heading 60, 180 degrees), derived by hand: the point
 * behind it is (510 + (float) cos(PI + PI) * 1.25, 500 + (float) sin(PI + PI) * 1.25) = (511.25, 500) - the Assassin's bound radius 0.25 + the
 * monster's 0 + 1, as DaevaEffectsTest's dash - and the heading taken before the move is the one towards the monster, 0.
 */
TEST_F(CombatGroupEffectsTest, FangdropStabMovesTheAssassinBehindTheTarget) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* stab = bindSkill(MOVE_BEHIND_XML);
	ASSERT_EQ(effectOf(*stab, 0).javaClassName(), "MoveBehindEffect");
	Ref<Player> assassin = daeva(9015, PlayerClass::ASSASSIN);
	Ref<Npc> monster = makeMonster(702015, 510, 500);
	monster->getPosition()->setH(int8_t{60});
	Ref<model::Skill> skill = model::Skill::create(stab, *assassin, Ptr<Creature>(monster), 1);

	Ref<Effect> effect = Effect::create(*skill, Ptr<Creature>(monster));
	effect->initialize();
	EXPECT_EQ(effect->getDashStatus(), model::DashStatus::MOVEBEHIND);
	EXPECT_EQ(assassin->getX(), 511.25f);
	EXPECT_EQ(assassin->getY(), 500.0f);
	EXPECT_EQ(assassin->getHeading(), 0);
	EXPECT_EQ(skill->getX(), 511.25f) << "the target position for SM_CASTSPELL_RESULT";
	EXPECT_TRUE(effect->isInSuccessEffects(1)) << "super.calculate(effect)";
}

// ---- RebirthEffect / ResurrectPositionalEffect ----------------------------------------------------------------------------------------------

TEST_F(CombatGroupEffectsTest, BrilliantProtectionStaysOnTheEffected) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* skill = bindSkill(REBIRTH_XML);
	ASSERT_EQ(effectOf(*skill, 0).javaClassName(), "RebirthEffect");
	Ref<Player> cleric = daeva(9016, PlayerClass::CLERIC);
	forced(*cleric, *cleric, skill, 1);
	EXPECT_TRUE(cleric->getEffectController()->hasAbnormalEffect(3923)) << "effect.addToEffectedController()";
}

TEST_F(CombatGroupEffectsTest, ResurrectionLociResurrectsADeadPlayerAtTheCastersPosition) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* skill = bindSkill(RESURRECT_POSITIONAL_XML);
	ASSERT_EQ(effectOf(*skill, 0).javaClassName(), "ResurrectPositionalEffect");
	Ref<Player> cleric = daeva(9017, PlayerClass::CLERIC, 503, 507, 100);
	Ref<Player> fallen = daeva(9018, PlayerClass::GLADIATOR, 502);

	// a living player: calculate admits only the dead
	EXPECT_FALSE(calculated(*cleric, *fallen, skill, 1)->isInSuccessEffects(1));

	// a player at 0 HP without PlayerController.onDie's services (RecoveryEffectsTest's)
	fallen->setLifeStats(std::make_unique<cp::DeadPlayerLifeStats>(*fallen));
	ASSERT_TRUE(fallen->isDead());
	forced(*cleric, *fallen, skill, 1);
	EXPECT_TRUE(fallen->isInResPostState());
	EXPECT_EQ(fallen->getResurrectionSkill(), 8295);
	EXPECT_EQ(fallen->getResPosX(), 503.0f);
	EXPECT_EQ(fallen->getResPosY(), 507.0f);
	EXPECT_EQ(fallen->getResPosZ(), 100.0f);
}

// ---- the empty Java bodies ------------------------------------------------------------------------------------------------------------------

TEST_F(CombatGroupEffectsTest, TheEmptyEffectsDoNothing) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* alwaysHit = bindSkill(ALWAYS_HIT_XML);
	const model::SkillTemplate* limited = bindSkill(LIMITED_REDUCE_XML);
	ASSERT_EQ(effectOf(*alwaysHit, 0).javaClassName(), "AlwaysHitEffect");
	ASSERT_EQ(effectOf(*alwaysHit, 1).javaClassName(), "AlwaysNoResistEffect");
	ASSERT_EQ(effectOf(*limited, 0).javaClassName(), "LimitedReduceDamageEffect");
	Ref<Player> player = daeva(9019, PlayerClass::GLADIATOR);
	forced(*player, *player, alwaysHit, 1);
	forced(*player, *player, limited, 1);
	EXPECT_FALSE(player->getEffectController()->hasAbnormalEffect(13194)) << "their applyEffect is empty: no addToEffectedController";
	EXPECT_FALSE(player->getEffectController()->hasAbnormalEffect(8887));
}

} // namespace
} // namespace aion::gameserver::skillengine::effecttest
