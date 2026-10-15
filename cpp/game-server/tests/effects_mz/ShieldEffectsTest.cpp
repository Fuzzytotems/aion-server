// P5-04, M5b-2 stage 1 part 3 (m5b2-plan.md F-03, F-05): the effect classes that watch the attacks on their effected - ShieldEffect and
// ReflectorEffect (an AttackShieldObserver built from the template's values, ShieldEffect.java:36-43, ReflectorEffect.java:22-30) and
// ProvokerEffect (an ActionObserver that applies another skill, ProvokerEffect.java:40-88).
//
// The shield arithmetic is AttackShieldObserver.checkShield's (AttackShieldObserver.java:62-107) with the hit and total the effect computes:
// ShieldEffect passes hitvalue + hitdelta * skill level and calculateBaseValue (value + delta * skill level); ReflectorEffect the same hit but
// the template's plain `value`, never its delta. The attacks are AttackResults handed to the effected's ObserveController.checkShieldStatus,
// as AttackUtil does after a hit is calculated.

#include "EffectsMzTestSupport.h"

#include <cstdint>
#include <vector>

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/attack/AttackResult.h"
#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/model/HitType.h"

namespace aion::gameserver::skillengine::effect::mztest {
namespace {

using controllers::attack::AttackResult;
using controllers::attack::AttackStatus;
using network::aion::serverpackets::SM_ABNORMAL_STATE;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

/** Java ShieldType ids (ShieldType.java:9-16) */
constexpr int32_t REFLECTOR_ID = 1;
constexpr int32_t NORMAL_SHIELD_ID = 2;

class ShieldEffectsTest : public EffectsMzTest {
protected:
	/** One hit of `damage` on the effected, through its shield observers (AttackUtil.calculateEffectResult / calculateAttackResult) */
	static Ref<AttackResult> hit(Creature& effected, Creature& attacker, int32_t damage, model::HitType hitType = model::HitType::PHHIT) {
		Ref<AttackResult> result = AttackResult::create(static_cast<float>(damage), AttackStatus::NORMALHIT, hitType);
		effected.getObserveController()->checkShieldStatus(std::vector<Ptr<AttackResult>>{Ptr<AttackResult>(result)}, nullptr, attacker);
		return result;
	}
};

// ---- ShieldEffect (ShieldEffect.java:17-49) -------------------------------------------------------------------------------------------------

/**
 * 64006 at skill level 3: hit 50 + 10 * 3 = 80 %, total 100 + 20 * 3 = 160. A hit of 100 is absorbed by 100 * 80 / 100 = 80 (20 get through,
 * 80 of the total left); a hit of 150 would be absorbed by 120, but only the 80 left are (70 get through), and the empty shield ends its effect.
 */
TEST_F(ShieldEffectsTest, APercentShieldAbsorbsItsShareUntilItsTotalIsSpent) {
	EFFECT_TEST_SCOPE;
	Ref<Player> self = player(7101);
	Ref<Npc> npc = monster();
	clearSent(*self);
	Ref<Effect> effect = applied(64006, *self, *self, 3);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(effect->getDuration(), 900000);
	EXPECT_EQ(sentTo<SM_ABNORMAL_STATE>(*self).size(), 1u);
	EXPECT_TRUE(self->getObserveController()->hasObservers()) << "the AttackShieldObserver";

	Ref<AttackResult> first = hit(*self, *npc, 100);
	EXPECT_EQ(first->getDamage(), 20);
	EXPECT_EQ(first->getShieldType(), NORMAL_SHIELD_ID);
	EXPECT_TRUE(self->getEffectController()->hasAbnormalEffect(64006));
	Ref<AttackResult> second = hit(*self, *npc, 150);
	EXPECT_EQ(second->getDamage(), 70);
	EXPECT_FALSE(self->getEffectController()->hasAbnormalEffect(64006)) << "totalHit <= 0 ends the effect";
	EXPECT_FALSE(self->getObserveController()->hasObservers()) << "and its observer (Effect.endEffect -> removeObservers)";
	Ref<AttackResult> third = hit(*self, *npc, 150);
	EXPECT_EQ(third->getDamage(), 150);
}

/**
 * 64007: a flat shield, at most 30 of each hit (Math.min(damage, hit)) and 50 in all. A hit of 20 is absorbed whole, which also cancels the
 * hit's sub effect (absorbedDamage >= damage); a hit of 100 then loses min(100, 30) = 30 (70 get through), a total of 50 - 20 - 25 = 5 is
 * left after a hit of 25, and the next hit of 100 loses only those 5 and ends the shield.
 */
TEST_F(ShieldEffectsTest, AFlatShieldAbsorbsAtMostItsHitValue) {
	EFFECT_TEST_SCOPE;
	Ref<Player> self = player(7111);
	Ref<Npc> npc = monster();
	applied(64007, *self, *self);

	Ref<AttackResult> whole = hit(*self, *npc, 20);
	EXPECT_EQ(whole->getDamage(), 0);
	EXPECT_FALSE(whole->isLaunchSubEffect());
	Ref<AttackResult> alsoWhole = hit(*self, *npc, 25);
	EXPECT_EQ(alsoWhole->getDamage(), 0);
	Ref<AttackResult> rest = hit(*self, *npc, 100);
	EXPECT_EQ(rest->getDamage(), 95) << "the 5 left of the total";
	EXPECT_TRUE(rest->isLaunchSubEffect());
	EXPECT_FALSE(self->getEffectController()->hasAbnormalEffect(64007));

	Ref<Player> fresh = player(7112);
	applied(64007, *fresh, *fresh);
	EXPECT_EQ(hit(*fresh, *npc, 100)->getDamage(), 70) << "at most the hit value of 30";
}

/**
 * ShieldEffect.startEffect hands the template's hittype and hittypeprob2 to the AttackShieldObserver (AttackShieldObserver.java:74-87): 64030
 * (hittype PHHIT) halves a physical hit and lets a magical one through whole; 64031 (EVERYHIT, hittypeprob2 50) absorbs only when the first
 * Rnd.chance() of the hit is below 50.
 */
TEST_F(ShieldEffectsTest, AShieldAbsorbsOnlyItsHitTypeAndWithItsProbability) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = monster();
	Ref<Player> physical = player(7131);
	applied(64030, *physical, *physical);
	EXPECT_EQ(hit(*physical, *npc, 100, model::HitType::MAHIT)->getDamage(), 100) << "a magical hit is not a PHHIT";
	EXPECT_EQ(hit(*physical, *npc, 100, model::HitType::PHHIT)->getDamage(), 50);

	Ref<Player> lucky = player(7132);
	applied(64031, *lucky, *lucky);
	seedWhereFirstChance([](float chance) { return chance >= 50.0f; });
	EXPECT_EQ(hit(*lucky, *npc, 100)->getDamage(), 100) << "a chance of 50 and more: not absorbed";
	seedWhereFirstChance([](float chance) { return chance < 50.0f; });
	EXPECT_EQ(hit(*lucky, *npc, 100)->getDamage(), 50);
}

/** 264 Trajanus' Blessing, as the data has it: 50 % of each hit, 10,000 in all, 900,000 ms */
TEST_F(ShieldEffectsTest, TrajanusBlessingHalvesEachHit) {
	EFFECT_TEST_SCOPE;
	Ref<Player> self = player(7121);
	Ref<Npc> npc = monster();
	Ref<Effect> effect = applied(264, *self, *self);
	EXPECT_EQ(effect->getDuration(), 900000);
	EXPECT_EQ(hit(*self, *npc, 401)->getDamage(), 201) << "401 * 50 / 100 = 200 absorbed (int division)";
	effect->endEffect();
	EXPECT_EQ(hit(*self, *npc, 401)->getDamage(), 401) << "the observer ended with the effect";
}

// ---- ReflectorEffect (ReflectorEffect.java:17-40) -------------------------------------------------------------------------------------------

/**
 * 64008 at skill level 2: hit 70 + 5 * 2 = 80, totalHit the plain value 30 (not value + delta * level: ReflectorEffect.java:26 passes `value`).
 * A hit of 500 from an Asmodian warrior 5 m away is reflected by max(500 * 30 / 100, 80) = 150, a hit of 100 by max(30, 80) = 80: the attacker
 * loses both (244 - 150 - 80 = 14); the hits themselves are not reduced. An attacker 40 m away is outside the radius of 30. (The attackers are
 * players: the reflected damage reaches the attacker's controller, and a test monster's DummyAI is no NpcAI for NpcController.onAttack.)
 */
TEST_F(ShieldEffectsTest, AReflectorReturnsItsShareOrAtLeastItsHitWithinItsRadius) {
	EFFECT_TEST_SCOPE;
	Ref<Player> self = player(7201);
	Ref<Player> near = player(7202, gameserver::model::PlayerClass::WARRIOR, 1, 505, 500, 100, gameserver::model::Race::ASMODIANS);
	Ref<Player> far = player(7203, gameserver::model::PlayerClass::WARRIOR, 1, 540, 500, 100, gameserver::model::Race::ASMODIANS);
	Ref<Effect> effect = applied(64008, *self, *self, 2);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(effect->getDuration(), 15000);
	ASSERT_EQ(near->getLifeStats()->getCurrentHp(), 244);

	Ref<AttackResult> big = hit(*self, *near, 500);
	EXPECT_EQ(big->getReflectedDamage(), 150);
	EXPECT_EQ(big->getShieldType(), REFLECTOR_ID);
	EXPECT_EQ(big->getDamage(), 500);
	EXPECT_EQ(near->getLifeStats()->getCurrentHp(), 94);
	Ref<AttackResult> small = hit(*self, *near, 100);
	EXPECT_EQ(small->getReflectedDamage(), 80);
	EXPECT_EQ(near->getLifeStats()->getCurrentHp(), 14);

	Ref<AttackResult> outOfRange = hit(*self, *far, 500);
	EXPECT_EQ(outOfRange->getReflectedDamage(), 0);
	EXPECT_EQ(far->getLifeStats()->getCurrentHp(), 244);

	effect->endEffect();
	EXPECT_EQ(hit(*self, *near, 500)->getReflectedDamage(), 0) << "ReflectorEffect.endEffect is empty; Effect.endEffect removed the observer";
}

/**
 * ReflectorEffect.startEffect hands the template's minradius and hittypeprob2 to the AttackShieldObserver (AttackShieldObserver.java:86,
 * 123-127): 64032 reflects at least 10 of a hit from an attacker beyond 3 m (minradius) and within 30 m, with probability 50. An attacker 2 m
 * away is not reflected even when the chance is in favour; one 5 m away is, but only when the first Rnd.chance() of the hit is below 50.
 */
TEST_F(ShieldEffectsTest, AReflectorSparesTheAttackersWithinItsMinimumRadiusAndReflectsWithItsProbability) {
	EFFECT_TEST_SCOPE;
	Ref<Player> self = player(7221);
	Ref<Player> close = player(7222, gameserver::model::PlayerClass::WARRIOR, 1, 502, 500, 100, gameserver::model::Race::ASMODIANS);
	Ref<Player> near = player(7223, gameserver::model::PlayerClass::WARRIOR, 1, 505, 500, 100, gameserver::model::Race::ASMODIANS);
	applied(64032, *self, *self);

	seedWhereFirstChance([](float chance) { return chance < 50.0f; });
	EXPECT_EQ(hit(*self, *close, 100)->getReflectedDamage(), 0) << "2 m: within minradius 3";
	seedWhereFirstChance([](float chance) { return chance >= 50.0f; });
	EXPECT_EQ(hit(*self, *near, 100)->getReflectedDamage(), 0) << "a chance of 50 and more: not reflected";
	seedWhereFirstChance([](float chance) { return chance < 50.0f; });
	EXPECT_EQ(hit(*self, *near, 100)->getReflectedDamage(), 10) << "max(100 * 0 / 100, 10)";
	EXPECT_EQ(near->getLifeStats()->getCurrentHp(), 234);
	EXPECT_EQ(close->getLifeStats()->getCurrentHp(), 244);
}

/** 620 Armor of Attrition, as the data has it: hitvalue 70 and no value, so every hit in 30 m reflects 70 */
TEST_F(ShieldEffectsTest, ArmorOfAttritionReflectsSeventy) {
	EFFECT_TEST_SCOPE;
	Ref<Player> self = player(7211, gameserver::model::PlayerClass::WARRIOR);
	Ref<Player> attacker = player(7212, gameserver::model::PlayerClass::WARRIOR, 1, 505, 500, 100, gameserver::model::Race::ASMODIANS);
	Ref<Effect> effect = applied(620, *self, *self);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(hit(*self, *attacker, 1000)->getReflectedDamage(), 70);
	EXPECT_EQ(hit(*self, *attacker, 5)->getReflectedDamage(), 70);
	EXPECT_EQ(attacker->getLifeStats()->getCurrentHp(), 244 - 140);
}

// ---- ProvokerEffect (ProvokerEffect.java:28-93) ---------------------------------------------------------------------------------------------

/**
 * 868 Tactical Retreat: hittype EVERYHIT, so the observer listens to ATTACKED; an attack on the effected applies 8502 (+30 % SPEED for 5 s) to
 * the effector (provoke_target ME, getProvokeTarget) and tells a player effector STR_SKILL_PROC_EFFECT_OCCURRED with 8502's name. After the
 * provoker ended (endEffect is empty: Effect.endEffect removes the observer), attacks apply nothing.
 */
TEST_F(ShieldEffectsTest, TacticalRetreatSpeedsTheScoutUpWhenItIsAttacked) {
	EFFECT_TEST_SCOPE;
	Ref<Player> scout = player(7301, gameserver::model::PlayerClass::SCOUT);
	Ref<Npc> npc = monster();
	Ref<Effect> provoker = applied(868, *scout, *scout);
	ASSERT_TRUE(provoker->isInSuccessEffects(1));
	EXPECT_EQ(provoker->getDuration(), 30000);
	ASSERT_EQ(scout->getGameStats()->getMovementSpeed()->getCurrent(), 6000);
	clearSent(*scout);

	scout->getObserveController()->notifyAttackedObservers(*npc, 0);
	EXPECT_TRUE(scout->getEffectController()->hasAbnormalEffect(8502)) << "applyEffectDirectly(8502, effector, effector)";
	EXPECT_EQ(scout->getGameStats()->getMovementSpeed()->getCurrent(), 7800) << "6000 + 6000 * 30 / 100f";
	std::vector<std::vector<uint8_t>> messages = sentTo<SM_SYSTEM_MESSAGE>(*scout);
	ASSERT_EQ(messages.size(), 1u);
	EXPECT_EQ(messages[0], cp::serialized(SM_SYSTEM_MESSAGE::STR_SKILL_PROC_EFFECT_OCCURRED(skillTemplate(8502)->getL10n()), &connection(*scout)));
	scout->getObserveController()->notifyAttackObservers(*npc, 0);
	EXPECT_EQ(sentTo<SM_SYSTEM_MESSAGE>(*scout).size(), 1u) << "an EVERYHIT provoker does not listen to the effected's own attacks";

	provoker->endEffect();
	scout->getEffectController()->removeEffect(8502);
	clearSent(*scout);
	scout->getObserveController()->notifyAttackedObservers(*npc, 0);
	EXPECT_FALSE(scout->getEffectController()->hasAbnormalEffect(8502)) << "the provoker's observer is gone";
	EXPECT_TRUE(sentTo<SM_SYSTEM_MESSAGE>(*scout).empty());
}

/**
 * 64009, provoke_target OPPONENT: the provoked skill (64010, -10 PHYSICAL_ATTACK) lands on the attacker; an OPPONENT provoker does not fire
 * when the attacker is its own effector (shouldApply's first check, ProvokerEffect.java:69-70).
 */
TEST_F(ShieldEffectsTest, AnOpponentProvokerHitsTheAttackerButNeverItsEffector) {
	EFFECT_TEST_SCOPE;
	Ref<Player> self = player(7311);
	Ref<Npc> npc = monster();
	applied(64009, *self, *self);

	self->getObserveController()->notifyAttackedObservers(*self, 0);
	EXPECT_FALSE(self->getEffectController()->hasAbnormalEffect(64010)) << "target == effector";
	EXPECT_FALSE(npc->getEffectController()->hasAbnormalEffect(64010));
	self->getObserveController()->notifyAttackedObservers(*npc, 0);
	EXPECT_TRUE(npc->getEffectController()->hasAbnormalEffect(64010));
	EXPECT_EQ(npc->getGameStats()->getStat(StatEnum::PHYSICAL_ATTACK, 100)->getCurrent(), 90);
	EXPECT_FALSE(self->getEffectController()->hasAbnormalEffect(64010));
}

/**
 * 64011, hittype NMLATK: the observer listens to ATTACK (the effected's own attacks, ProvokerEffect.java:43) and fires when Rnd.chance() is
 * below hittypeprob2 50 (`Rnd.chance() >= hitTypeProb` refuses).
 */
TEST_F(ShieldEffectsTest, ANormalAttackProvokerListensToTheEffectedsAttacksWithItsProbability) {
	EFFECT_TEST_SCOPE;
	Ref<Player> self = player(7321);
	Ref<Npc> npc = monster();
	applied(64011, *self, *self);

	self->getObserveController()->notifyAttackedObservers(*npc, 0);
	EXPECT_FALSE(npc->getEffectController()->hasAbnormalEffect(64010)) << "ATTACK observer: an attack on the effected does not count";
	seedWhereFirstChance([](float chance) { return chance >= 50.0f; });
	self->getObserveController()->notifyAttackObservers(*npc, 0);
	EXPECT_FALSE(npc->getEffectController()->hasAbnormalEffect(64010)) << "a chance of 50 and more refuses";
	seedWhereFirstChance([](float chance) { return chance < 50.0f; });
	self->getObserveController()->notifyAttackObservers(*npc, 0);
	EXPECT_TRUE(npc->getEffectController()->hasAbnormalEffect(64010));
}

/**
 * 64012, hittype PHHIT within radius 10: an auto attack (skill id 0) or a PHYSICAL skill fires it, a MAGICAL skill does not, nor anything from
 * beyond 10 m; 64013, hittype MAHIT: only a MAGICAL skill (ProvokerEffect.java:75-78).
 */
TEST_F(ShieldEffectsTest, PhysicalAndMagicalProvokersAskTheAttackingSkillsType) {
	EFFECT_TEST_SCOPE;
	Ref<Player> self = player(7331);
	Ref<Npc> near = monster(505, 500, 100);
	Ref<Npc> far = monster(520, 500, 100);
	Ref<Effect> physical = applied(64012, *self, *self);
	auto attackedBy = [&](Npc& attacker, int32_t skillId) {
		self->getEffectController()->removeEffect(64010);
		self->getObserveController()->notifyAttackedObservers(attacker, skillId);
		return self->getEffectController()->hasAbnormalEffect(64010);
	};
	EXPECT_TRUE(attackedBy(*near, 0)) << "an auto attack";
	EXPECT_TRUE(attackedBy(*near, 2864)) << "2864 is PHYSICAL";
	EXPECT_FALSE(attackedBy(*near, 1282)) << "1282 is MAGICAL";
	EXPECT_FALSE(attackedBy(*far, 0)) << "15 m: outside radius 10";
	physical->endEffect();

	applied(64013, *self, *self);
	EXPECT_FALSE(attackedBy(*near, 0)) << "MAHIT needs a skill";
	EXPECT_FALSE(attackedBy(*near, 2864));
	EXPECT_TRUE(attackedBy(*near, 1282));
	EXPECT_TRUE(attackedBy(*far, 1282)) << "no radius";
}

/**
 * 64014, hittype BACKATK: the observer listens to ATTACK and fires only when the effector is behind the attacked creature
 * (PositionUtil.isBehind(effector, target)). The monster at (505, 500) faces heading 0, i.e. +x: the player at (500, 500) is behind it, the
 * player at (510, 500) in front.
 */
TEST_F(ShieldEffectsTest, ABackAttackProvokerFiresOnlyFromBehind) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = monster(505, 500, 100);
	Ref<Player> behind = player(7341, gameserver::model::PlayerClass::SCOUT, 1, 500, 500, 100);
	Ref<Player> inFront = player(7342, gameserver::model::PlayerClass::SCOUT, 1, 510, 500, 100);
	applied(64014, *inFront, *inFront);
	inFront->getObserveController()->notifyAttackObservers(*npc, 0);
	EXPECT_FALSE(npc->getEffectController()->hasAbnormalEffect(64010));
	applied(64014, *behind, *behind);
	behind->getObserveController()->notifyAttackObservers(*npc, 0);
	EXPECT_TRUE(npc->getEffectController()->hasAbnormalEffect(64010));
}

} // namespace
} // namespace aion::gameserver::skillengine::effect::mztest
