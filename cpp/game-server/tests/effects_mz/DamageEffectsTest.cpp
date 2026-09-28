// P5-04, M5b-2 stage 1 part 3 (m5b2-plan.md F-03, F-05): the damage effect classes of the lane - SkillAttackInstantEffect (2864 Ferocious
// Strike's <skillatk>; its only own code is isNoResist and the rnddmg/cannotmiss attributes AttackUtil reads), SpellAttackInstantEffect (1282
// Flame Bolt's <spellatkinstant>, data-only: DamageEffect's bodies) and SpellAttackEffect (1447 Erosion's periodic <spellatk>).
//
// The golden damage is derived by hand from the Java arithmetic of the magical path, which draws no random number for a player effector:
// AttackUtil.calculateSkillResult (AttackUtil.java:223-348) and calculateMagicalOverTimeSkillResult (:427-456) through
// StatFunctions.calculateMagicalSkillDamage (StatFunctions.java:381-415) and adjustDamageByPvpOrPveModifiers (:472-521). The inputs are
// asserted first: a level 1 MAGE has KNOWLEDGE 115 (PlayerClass.java:19) and no magic boost; the level 1 test monster has mdef 100 and no
// elemental defense, so after the knowledge factor the damage loses 100 / 10f = 10 and keeps the rest (1 - 0 / 1300f); the PvE multiplier is
// 1 (no ratio stats, no level difference). Where a critical could be rolled, the monster's MAGICAL_CRITICAL_RESIST makes it impossible.

#include "EffectsMzTestSupport.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOGInfo.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPEInfo.h"
#include "aion/gameserver/skillengine/effect/SkillAttackInstantEffect.h"
#include "aion/gameserver/skillengine/effect/SpellAttackInstantEffect.h"
#include "aion/gameserver/skillengine/model/EffectReserved.h"
#include "aion/gameserver/skillengine/model/EffectResult.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::skillengine::effect::mztest {
namespace {

using network::aion::serverpackets::SM_ATTACK_STATUS;
using network::aion::serverpackets::SM_ATTACK_STATUS_LOG;
using network::aion::serverpackets::SM_ATTACK_STATUS_TYPE;

/** The critical marker of SM_ATTACK_STATUS (SM_ATTACK_STATUS.java:12, private there and in the C++ class) */
constexpr int32_t CRITICAL_DISPLAY_CODE = 12;

/** SM_ATTACK_STATUS as SM_ATTACK_STATUS.java writes it: creature, the signed value, TYPE.getValue(), hp percentage, skill, LOG.getValue(), critical */
struct AttackStatusFields {
	int32_t objectId = 0;
	int32_t value = 0;
	int32_t type = 0;
	int32_t percentage = 0;
	int32_t skillId = 0;
	int32_t log = 0;
	int32_t critical = 0;
};

AttackStatusFields decodeAttackStatus(const std::vector<uint8_t>& bytes) {
	network::test::PacketReader reader(cp::bodyOf(bytes));
	AttackStatusFields f;
	f.objectId = reader.D();
	f.value = reader.D();
	f.type = reader.C();
	f.percentage = reader.C();
	f.skillId = static_cast<uint16_t>(reader.H());
	f.log = reader.C();
	f.critical = reader.C();
	EXPECT_EQ(reader.remaining(), 0u) << "SM_ATTACK_STATUS consumed exactly";
	return f;
}

/** Records the ATTACKED notifications of a creature's ObserveController (the event RootEffect's observer breaks a root on) */
struct AttackedRecorder final : controllers::observer::ActionObserver {
	AION_MAKE_REF_FRIEND

	static Ref<AttackedRecorder> create() { return runtime::makeRef<AttackedRecorder>(); }

	void attacked(Creature& /*creature*/, int32_t skillId) override { skillIds.push_back(skillId); }

	std::vector<int32_t> skillIds;

protected:
	AttackedRecorder() : ActionObserver(controllers::observer::ObserverType::ATTACKED) {}
	~AttackedRecorder() override = default;
};

class DamageEffectsTest : public EffectsMzTest {
protected:
	void SetUp() override {
		EffectsMzTest::SetUp();
		EFFECT_TEST_SCOPE;
		mage = player(8001);
		npc = monster();
		pair(*npc, *mage);
		skillengine::test::addStat(*npc, StatEnum::MAGICAL_CRITICAL_RESIST, 1000); // no magical critical can be rolled against it
	}

	void TearDown() override {
		mage = nullptr;
		npc = nullptr;
		EffectsMzTest::TearDown();
	}

	/** The inputs of the golden damage (see the file comment) */
	void assertInputs() {
		ASSERT_EQ(mage->getGameStats()->getKnowledge()->getCurrent(), 115);
		ASSERT_EQ(mage->getGameStats()->getMBoost()->getCurrent(), 0);
		ASSERT_EQ(npc->getGameStats()->getMDef()->getCurrent(), 100);
		ASSERT_EQ(npc->getGameStats()->getElementalDefenseFor(gameserver::model::SkillElement::FIRE), 0);
		ASSERT_EQ(npc->getGameStats()->getElementalDefenseFor(gameserver::model::SkillElement::EARTH), 0);
		ASSERT_EQ(npc->getLifeStats()->getCurrentHp(), 1000);
	}

	int32_t reserved(Effect& effect, int32_t position = 1) { return effect.getReserveds(position)->getValue(); }

	Ref<Player> mage;
	Ref<Npc> npc;
};

// ---- SkillAttackInstantEffect (SkillAttackInstantEffect.java:15-35) -------------------------------------------------------------------------

/**
 * isNoResist is `cannotmiss || super.isNoResist()`: 64016 (cannotmiss) and 64017 (without) are the same magical <skillatk>, neither noresist.
 * Against a monster with 100,000 MAGICAL_RESIST the resist rate is capped at 900 (StatCapUtil: MAGICAL_RESIST diffLimit 900), so a first
 * Rnd.get(1, 1000) of at most 900 resists 64017 - EffectTemplate.checkDodgeOrResistRate's roll is the first draw of the calculation, since the
 * magical critical is not rolled for these templates (apply_magical_critical false) - while 64016 is never rolled for.
 */
TEST_F(DamageEffectsTest, CannotmissMakesASkillAttackUnresistable) {
	EFFECT_TEST_SCOPE;
	skillengine::test::addStat(*npc, StatEnum::MAGICAL_RESIST, 100000);
	uint64_t seed = 1;
	for (; seed < 1000; ++seed) {
		Rnd::seedCurrentThreadForTests(seed);
		if (Rnd::get(1, 1000) <= 900)
			break;
	}

	Rnd::seedCurrentThreadForTests(seed);
	Ref<Effect> resisted = calculated(64017, *mage, *npc);
	EXPECT_FALSE(resisted->isInSuccessEffects(1));
	EXPECT_EQ(resisted->getEffectResult(), model::EffectResult::RESIST);

	Rnd::seedCurrentThreadForTests(seed);
	Ref<Effect> landed = calculated(64016, *mage, *npc);
	EXPECT_TRUE(landed->isInSuccessEffects(1)) << "cannotmiss: no resist roll";
	const auto* skillatk = dynamic_cast<const SkillAttackInstantEffect*>(landed->getEffectTemplates()[0]);
	ASSERT_NE(skillatk, nullptr) << "<skillatk> binds SkillAttackInstantEffect (Effects.java:46)";
	EXPECT_TRUE(skillatk->isNoResist());
	EXPECT_TRUE(skillatk->isCannotmiss());
}

/**
 * rnddmg="3" (64018): AttackUtil.randomizeDamage draws Rnd.get(0, 19), the first draw of the calculation, and multiplies by 0.9f (0..6), 1.0f
 * (7..12) or 1.1f (13..19). The damage before it: (int) (100 * (0 / 1000f + 115 / 100f)) = 115 (BOOST_SPELL_ATTACK takes the int), minus 10 =
 * 105; so 105 * 0.9f = 94.5f -> 94, 105, and 105 * 1.1f = 115.5f -> 115. applyEffect deals the reserved damage to the monster.
 */
TEST_F(DamageEffectsTest, TheRandomDamageTypeScalesTheSkillAttack) {
	EFFECT_TEST_SCOPE;
	ASSERT_NO_FATAL_FAILURE(assertInputs());
	struct Expectation {
		int32_t lowRoll;
		int32_t highRoll;
		int32_t damage;
	};
	for (const Expectation& expected : {Expectation{0, 6, 94}, Expectation{7, 12, 105}, Expectation{13, 19, 115}}) {
		uint64_t seed = 1;
		for (; seed < 1000; ++seed) {
			Rnd::seedCurrentThreadForTests(seed);
			int32_t roll = Rnd::get(0, 19);
			if (roll >= expected.lowRoll && roll <= expected.highRoll)
				break;
		}
		Rnd::seedCurrentThreadForTests(seed);
		Ref<Npc> target = monster(505, 505, 100);
		Ref<Effect> effect = calculated(64018, *mage, *target);
		ASSERT_TRUE(effect->isInSuccessEffects(1));
		EXPECT_EQ(reserved(*effect), expected.damage) << "rolls " << expected.lowRoll << ".." << expected.highRoll;
		effect->applyEffect();
		EXPECT_EQ(target->getLifeStats()->getCurrentHp(), 1000 - expected.damage) << "DamageEffect.applyEffect -> onAttack";
	}
}

/**
 * 2864 Ferocious Strike, the gate Warrior's skill, as the data has it: a physical <skillatk> whose damage depends on random rolls, so the case
 * checks the invariant (D8 of m5b2-plan.md): a hit that lands reserves a positive damage and applyEffect takes exactly that from the monster.
 */
TEST_F(DamageEffectsTest, FerociousStrikeDealsItsReservedDamage) {
	EFFECT_TEST_SCOPE;
	Ref<Player> warrior = player(8011, gameserver::model::PlayerClass::WARRIOR, 1, 503, 500, 100);
	for (uint64_t seed = 1; seed < 50; ++seed) {
		Rnd::seedCurrentThreadForTests(seed);
		Ref<Npc> target = monster(505, 500, 100);
		Ref<Effect> effect = calculated(2864, *warrior, *target);
		if (!effect->isInSuccessEffects(1) || reserved(*effect) == 0)
			continue;
		EXPECT_GT(reserved(*effect), 0);
		effect->applyEffect();
		EXPECT_EQ(target->getLifeStats()->getCurrentHp(), 1000 - reserved(*effect));
		return;
	}
	FAIL() << "no seed landed a hit";
}

// ---- SpellAttackInstantEffect (data-only) ---------------------------------------------------------------------------------------------------

/**
 * 1282 Flame Bolt, the gate Mage's skill, as the data has it: <spellatkinstant value="141" element="FIRE"> binds SpellAttackInstantEffect, a
 * DamageEffect with nothing of its own. With the magic boost on (apply_magical_skill_boost_bonus) but 0 of it: (int) (141 * 1.15f) = 162
 * (141 * 1.15f = 162.15f), minus 10 = 152. The accuracy (MAGICAL_ACCURACY 14 of a level 1 player against a resist of 0) leaves no resist
 * rate, so no roll can resist it.
 */
TEST_F(DamageEffectsTest, FlameBoltReservesItsGoldenDamage) {
	EFFECT_TEST_SCOPE;
	ASSERT_NO_FATAL_FAILURE(assertInputs());
	Ref<Effect> effect = calculated(1282, *mage, *npc);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	ASSERT_NE(dynamic_cast<const SpellAttackInstantEffect*>(effect->getEffectTemplates()[0]), nullptr);
	EXPECT_FALSE(effect->isMagicalCritical(1));
	EXPECT_EQ(reserved(*effect), 152);
	effect->applyEffect();
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), 848);
}

// ---- SpellAttackEffect (SpellAttackEffect.java:20-42) ---------------------------------------------------------------------------------------

/**
 * 64015 (1447 Erosion's <spellatk> with value 81 + 4 per level) at skill level 1: startEffect reserves
 * calculateMagicalOverTimeSkillResult(81 + 4 * 1): knowledge is not used (100 / 100f), the magic boost is off for this template, so 85 - 10 = 75.
 * The reserve is an over-time one (setReserveds(..., true)), so the effect's effectedHp stays -1: no HP percentage is announced in advance.
 * The periodic task (AbstractOverTimeEffect: first after 300 + checktime 3000 ms, then every 3000 ms) deals 75 each time, five times within the
 * duration of duration2 + 1000 = 16,000 ms (at 3300, 6300, 9300, 12300 and 15300): 1000 - 5 * 75 = 625. Each tick is
 * onAttack(effect, TYPE.DAMAGE, 75, false, LOG.SPELLATK, ...): the players who see the monster read SM_ATTACK_STATUS -75 of type DAMAGE with the
 * SPELLATK log, and notifyAttack false means no ATTACKED notification (a 1328 root's observer would break the root on it) and no cast-cancel
 * roll (the monster's cast of 243 Return, cancel_rate 100000, would end at any hit that asks).
 */
TEST_F(DamageEffectsTest, ErosionDealsItsReservedDamageEveryThreeSeconds) {
	EFFECT_TEST_SCOPE;
	ASSERT_NO_FATAL_FAILURE(assertInputs());
	Ref<AttackedRecorder> attacked = AttackedRecorder::create();
	npc->getObserveController()->addObserver(*attacked);
	npc->setCasting(model::Skill::create(skillTemplate(243), *npc, 1, Ptr<Creature>(mage), nullptr));
	Ref<Effect> effect = applied(64015, *mage, *npc);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_FALSE(effect->isMagicalCritical(1));
	EXPECT_EQ(reserved(*effect), 75);
	EXPECT_EQ(effect->getEffectedHp(), -1) << "setReserveds(reserved, true): an over-time reserve";
	EXPECT_EQ(effect->getDuration(), 16000);
	clearSent(*mage);

	advance(3299);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), 1000);
	advance(1);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), 925);
	std::vector<std::vector<uint8_t>> statuses = sentTo<SM_ATTACK_STATUS>(*mage);
	ASSERT_EQ(statuses.size(), 1u);
	AttackStatusFields tick = decodeAttackStatus(statuses[0]);
	EXPECT_EQ(tick.objectId, npc->getObjectId());
	EXPECT_EQ(tick.value, -75);
	EXPECT_EQ(tick.type, network::aion::serverpackets::getValue(SM_ATTACK_STATUS_TYPE::DAMAGE));
	EXPECT_EQ(tick.skillId, 64015);
	EXPECT_EQ(tick.log, network::aion::serverpackets::getValue(SM_ATTACK_STATUS_LOG::SPELLATK));
	EXPECT_EQ(tick.critical, 0);
	advance(12000);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), 625);
	advance(700);
	EXPECT_TRUE(effect->isEndedByTime());
	advance(6000);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), 625) << "the periodic task ended with the effect";
	EXPECT_TRUE(attacked->skillIds.empty()) << "notifyAttack false: the ticks notify no ATTACKED observer";
	EXPECT_TRUE(npc->getCastingSkill()) << "notifyAttack false: the ticks roll no cast cancel";
	npc->setCasting(nullptr); // the cast holds its caster (Skill.effector)

	Ref<Npc> second = monster(505, 505, 100);
	skillengine::test::addStat(*second, StatEnum::MAGICAL_CRITICAL_RESIST, 1000);
	EXPECT_EQ(reserved(*applied(64015, *mage, *second, 2)), 79) << "skill level 2: 81 + 8 - 10";
}

/**
 * startEffect passes the skill's apply_magical_skill_boost_bonus to calculateMagicalOverTimeSkillResult, and 64015's is false: a mage with 200
 * magic boost still reserves 75, where the boost would give 85 * (200 / 1000f + 100 / 100f) = 102, minus 10 = 92.
 */
TEST_F(DamageEffectsTest, ErosionIgnoresTheMagicBoostItsSkillTurnsOff) {
	EFFECT_TEST_SCOPE;
	skillengine::test::addStat(*mage, StatEnum::BOOST_MAGICAL_SKILL, 200);
	ASSERT_EQ(mage->getGameStats()->getMBoost()->getCurrent(), 200);
	Ref<Effect> effect = applied(64015, *mage, *npc);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(reserved(*effect), 75);
}

/**
 * SpellAttackEffect.resolveMagicalCritical rolls the critical although 64015's skill says apply_magical_critical="false" ("periodic damage
 * ignores the apply_magical_critical flag"): against a monster without critical resist the chance is the Mage's MAGICAL_CRITICAL 50 per mille
 * (StatFunctions.calculateMagicalCriticalRate), and the roll is the first draw of the calculation. A critical reserves 75 * 1.5f = 112.5f ->
 * 112 (AttackUtil.calculateWeaponCritical, magical: coefficient 1.5), and each tick passes effect.isMagicalCritical(position) on: its
 * SM_ATTACK_STATUS carries the critical marker (CRITICAL_DISPLAY_CODE), the other effect's does not.
 */
TEST_F(DamageEffectsTest, ErosionRollsItsMagicalCriticalDespiteTheSkillsFlag) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> target = monster(505, 505, 100);
	pair(*target, *mage);
	ASSERT_EQ(mage->getGameStats()->getMCritical()->getCurrent() - target->getGameStats()->getMCR()->getCurrent(), 50);
	uint64_t critical = 1;
	for (; critical < 100000; ++critical) {
		Rnd::seedCurrentThreadForTests(critical);
		if (Rnd::nextInt(1000) < 50)
			break;
	}
	Rnd::seedCurrentThreadForTests(critical);
	Ref<Effect> crit = calculated(64015, *mage, *target);
	EXPECT_TRUE(crit->isMagicalCritical(1));
	crit->applyEffect();
	EXPECT_EQ(reserved(*crit), 112);

	Ref<Npc> other = monster(505, 495, 100);
	pair(*other, *mage);
	uint64_t normal = 1;
	for (; normal < 1000; ++normal) {
		Rnd::seedCurrentThreadForTests(normal);
		if (Rnd::nextInt(1000) >= 50)
			break;
	}
	Rnd::seedCurrentThreadForTests(normal);
	Ref<Effect> plain = applied(64015, *mage, *other);
	EXPECT_FALSE(plain->isMagicalCritical(1));
	EXPECT_EQ(reserved(*plain), 75);

	clearSent(*mage);
	advance(3300); // the first tick of both
	std::optional<AttackStatusFields> critTick;
	std::optional<AttackStatusFields> plainTick;
	for (const std::vector<uint8_t>& bytes : sentTo<SM_ATTACK_STATUS>(*mage)) {
		AttackStatusFields f = decodeAttackStatus(bytes);
		if (f.objectId == target->getObjectId())
			critTick = f;
		else if (f.objectId == other->getObjectId())
			plainTick = f;
	}
	ASSERT_TRUE(critTick.has_value());
	ASSERT_TRUE(plainTick.has_value());
	EXPECT_EQ(critTick->value, -112);
	EXPECT_EQ(critTick->critical, CRITICAL_DISPLAY_CODE);
	EXPECT_EQ(plainTick->value, -75);
	EXPECT_EQ(plainTick->critical, 0);
}

/**
 * Each tick notifies the effected's DOT_ATTACKED observers with the effector (SpellAttackEffect.java:40): an effect that ends on damage (the
 * cancel-on-damage observers of Effect.addCancelOnDmgObserver) ends at the first tick, although the tick's onAttack does not notify the
 * ATTACKED observers (notifyAttack false).
 */
TEST_F(DamageEffectsTest, ErosionsTicksNotifyTheDotAttackedObservers) {
	EFFECT_TEST_SCOPE;
	Ref<Effect> fragile = calculated(64003, *mage, *npc); // a 10 s snare
	fragile->setCancelOnDmg(true);
	fragile->applyEffect();
	ASSERT_TRUE(npc->getEffectController()->hasAbnormalEffect(64003));

	applied(64015, *mage, *npc);
	advance(3299);
	EXPECT_TRUE(npc->getEffectController()->hasAbnormalEffect(64003));
	advance(1);
	EXPECT_FALSE(npc->getEffectController()->hasAbnormalEffect(64003)) << "the DOT_ATTACKED observer ended it";
}

} // namespace
} // namespace aion::gameserver::skillengine::effect::mztest
