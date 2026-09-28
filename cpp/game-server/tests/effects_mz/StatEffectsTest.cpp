// P5-04, M5b-2 stage 1 part 3 (m5b2-plan.md F-03, F-05): the effect classes that change stats - StatupEffect, StatdownEffect (8291 Soul
// Sickness), StatboostEffect (the Warrior passive 140, data-only), WeaponMasteryEffect (the Warrior passives 37 and 39, the two-handed 51),
// ShieldMasteryEffect (43, 50) and WeaponDualEffect (55, 70).
//
// The golden stat values are derived by hand from the Java arithmetic, with Java's float and int semantics:
// - the base values are PlayerStatCalculator's (PlayerStatCalculator.java:10-22) for the PlayerClass multipliers (PlayerClass.java:13-29): a
//   level 1 MAGE has healthMultiplier 260 and willMultiplier 600, so maxHp = (int) (130 + 1 * (0.1075f * 260) + 1 * (0.002875f * 260)) = 158
//   and maxMp = (int) (600 * 0.35f + 1 * 210f / 2f + 600 * 0.125f / 10000) = 315; the test players carry no predefined stat functions
//   (PlayerStatFunctions is added by the enter-world path, not by the fixture), so getStat(stat, base) starts from the given base;
// - a PERCENT change is a bonus StatRateFunction: bonus += (int) base * value / 100f, current = (int) (base + bonus) (StatRateFunction.java:22-31,
//   Stat2.java:64-70); an ADD change a bonus StatAddFunction: bonus += value;
// - a weapon mastery function sets the fixed bonus rate value / 100f when the weapon matches (StatWeaponMasteryFunction.java:23-54), so
//   current = (int) (base + base * rate); a shield mastery function is the rate function, applied only while a shield is equipped
//   (StatShieldMasteryFunction.java:19-24).

#include "EffectsMzTestSupport.h"

#include <cstdint>
#include <memory>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_STATS_INFO.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/effect/WeaponDualEffect.h"
#include "aion/gameserver/skillengine/model/SkillTargetSlot.h"
#include "aion/gameserver/utils/stats/CalculationType.h"

namespace aion::gameserver::skillengine::effect::mztest {
namespace {

using gameserver::model::PlayerClass;
using gameserver::model::gameobjects::player::detail::MAIN_HAND;
using gameserver::model::gameobjects::player::detail::SUB_HAND;
using gameserver::model::stats::calc::Stat2;
using gameserver::model::stats::container::CreatureGameStats;
using gameserver::model::templates::item::enums::ItemGroup;
using network::aion::serverpackets::SM_ABNORMAL_STATE;
using network::aion::serverpackets::SM_STATS_INFO;
using utils::stats::CalculationType;

/**
 * CreatureGameStats.getStat(statEnum, base, calculationTypes) is protected; the explicit-instantiation rule hands it out (EffectTestSupport.h's
 * PrivateAccess, repeated here because the friend it defines lives in the namespace of the template)
 */
template <class Tag, typename Tag::Type Member>
struct StatAccess {
	friend typename Tag::Type privateMember(Tag) { return Member; }
};

struct GetStatWithTypesTag {
	using Type = std::unique_ptr<Stat2> (CreatureGameStats::*)(StatEnum, float, const std::unordered_set<CalculationType>&);
	friend Type privateMember(GetStatWithTypesTag);
};
template struct StatAccess<GetStatWithTypesTag, &CreatureGameStats::getStat>;

class StatEffectsTest : public EffectsMzTest {
protected:
	/** getStat(stat, base) of the creature (the stat functions of the effects applied, and nothing else: see the file comment) */
	static int32_t stat(Creature& creature, StatEnum statEnum, float base) { return creature.getGameStats()->getStat(statEnum, base)->getCurrent(); }
};

// ---- StatupEffect (StatupEffect.java:14-22) -------------------------------------------------------------------------------------------------

/**
 * 64004 at skill level 2: MAXHP +50 % (158 * 50 / 100f = 79, so 237; the current HP follows the maximum, CreatureGameStats.checkMaxHPChanged)
 * and PHYSICAL_ATTACK + (25 + 5 * 2) = 35, for duration2 120,000 ms in the BUFF slot; the end removes both functions.
 */
TEST_F(StatEffectsTest, AStatupAddsItsPercentAndItsDeltaAndTakesThemBackAtTheEnd) {
	EFFECT_TEST_SCOPE;
	Ref<Player> mage = player(6101);
	ASSERT_EQ(mage->getGameStats()->getMaxHp()->getCurrent(), 158) << "PlayerStatCalculator.calculateMaxHp(MAGE, 1)";
	ASSERT_EQ(mage->getLifeStats()->getCurrentHp(), 158);
	ASSERT_EQ(stat(*mage, StatEnum::PHYSICAL_ATTACK, 100), 100);
	clearSent(*mage);

	Ref<Effect> effect = applied(64004, *mage, *mage, 2);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(mage->getGameStats()->getMaxHp()->getCurrent(), 237);
	EXPECT_EQ(mage->getLifeStats()->getCurrentHp(), 237);
	EXPECT_EQ(stat(*mage, StatEnum::PHYSICAL_ATTACK, 100), 135);
	EXPECT_EQ(effect->getDuration(), 120000);
	std::vector<std::vector<uint8_t>> icons = sentTo<SM_ABNORMAL_STATE>(*mage);
	ASSERT_EQ(icons.size(), 1u);
	AbnormalStateFields state = decodeAbnormalState(icons[0]);
	EXPECT_EQ(state.slot, BUFF_SLOT_ID);
	ASSERT_EQ(state.effects.size(), 1u);
	EXPECT_EQ(state.effects[0].skillId, 64004);
	EXPECT_EQ(state.effects[0].level, 2);
	EXPECT_EQ(state.effects[0].targetSlotOrdinal, BUFF_ORDINAL);
	EXPECT_EQ(state.effects[0].remainingTime, 120000);

	effect->endEffect();
	EXPECT_EQ(mage->getGameStats()->getMaxHp()->getCurrent(), 158);
	EXPECT_EQ(mage->getLifeStats()->getCurrentHp(), 158);
	EXPECT_EQ(stat(*mage, StatEnum::PHYSICAL_ATTACK, 100), 100);
}

/**
 * StatupEffect.endEffect ends with lifeStats.updateCurrentStats() (StatupEffect.java:19): for a player that also starts the fly time restore
 * when the player neither flies nor sprints (PlayerLifeStats.java:82-93), which nothing else of the effect's end does - so a player whose fly
 * time is 10 short gains 3 (PlayerLifeStats.restoreFp) when the restore task first runs, 3,000 ms later (LifeStatsRestoreService).
 */
TEST_F(StatEffectsTest, TheEndOfAStatupUpdatesTheCurrentStats) {
	EFFECT_TEST_SCOPE;
	skillengine::test::ConfigOverride<int32_t> flyTime(configs::main::CustomConfig::BASE_FLYTIME, 60); // gameserver.base.flytime's default
	Ref<Player> mage = player(6111);
	Ref<Effect> effect = applied(64004, *mage, *mage, 1);
	const int32_t maxFp = mage->getLifeStats()->getMaxFp();
	ASSERT_EQ(maxFp, 60);
	mage->getLifeStats()->setCurrentFp(maxFp - 10);
	advance(3000);
	ASSERT_EQ(mage->getLifeStats()->getCurrentFp(), maxFp - 10) << "no fly time restore runs before the statup ends";

	effect->endEffect();
	advance(3000);
	EXPECT_EQ(mage->getLifeStats()->getCurrentFp(), maxFp - 7) << "updateCurrentStats started the fly time restore";
}

// ---- StatdownEffect (StatdownEffect.java:14-23): 8291 Soul Sickness --------------------------------------------------------------------

/**
 * 8291 at skill level 2 (the death count, PlayerController.updateSoulSickness): MAXHP -30 % (158 * -30 / 100f = -47.4f, (int) 110.6f = 110),
 * MAXMP -30 % (315 * -30 / 100f = -94.5f, (int) 220.5f = 220), SPEED and FLY_SPEED -50 %, for duration2 40,000 + duration1 20,000 x 2 =
 * 80,000 ms in the SPEC2 slot; the current HP and MP follow the maxima down. At the end every function is gone again.
 */
TEST_F(StatEffectsTest, SoulSicknessTakesThirtyPercentOfHpAndMpAndHalfTheSpeed) {
	EFFECT_TEST_SCOPE;
	Ref<Player> mage = player(6201);
	ASSERT_EQ(mage->getGameStats()->getMaxMp()->getCurrent(), 315) << "PlayerStatCalculator.calculateMaxMp(MAGE, 1)";
	const int32_t speed = mage->getGameStats()->getMovementSpeed()->getCurrent();
	ASSERT_EQ(speed, 6000);
	clearSent(*mage);

	Ref<Effect> effect = applied(8291, *mage, *mage, 2);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	ASSERT_TRUE(effect->isInSuccessEffects(2));
	ASSERT_TRUE(effect->isInSuccessEffects(3));
	EXPECT_EQ(mage->getGameStats()->getMaxHp()->getCurrent(), 110);
	EXPECT_EQ(mage->getGameStats()->getMaxMp()->getCurrent(), 220);
	EXPECT_EQ(mage->getLifeStats()->getCurrentHp(), 110);
	EXPECT_EQ(mage->getLifeStats()->getCurrentMp(), 220);
	EXPECT_EQ(mage->getGameStats()->getMovementSpeed()->getCurrent(), 3000);
	EXPECT_EQ(stat(*mage, StatEnum::FLY_SPEED, 9000), 4500);
	EXPECT_EQ(effect->getDuration(), 80000);
	EXPECT_EQ(effect->getTargetSlot(), model::SkillTargetSlot::SPEC2);
	std::vector<std::vector<uint8_t>> icons = sentTo<SM_ABNORMAL_STATE>(*mage);
	ASSERT_EQ(icons.size(), 1u);
	AbnormalStateFields state = decodeAbnormalState(icons[0]);
	ASSERT_EQ(state.effects.size(), 1u);
	EXPECT_EQ(state.effects[0].skillId, 8291);
	EXPECT_EQ(state.effects[0].level, 2);
	EXPECT_EQ(state.effects[0].targetSlotOrdinal, SPEC2_ORDINAL);
	EXPECT_EQ(state.effects[0].remainingTime, 80000);

	advance(80000);
	EXPECT_TRUE(effect->isEndedByTime());
	EXPECT_EQ(mage->getGameStats()->getMaxHp()->getCurrent(), 158);
	EXPECT_EQ(mage->getGameStats()->getMaxMp()->getCurrent(), 315);
	EXPECT_EQ(mage->getGameStats()->getMovementSpeed()->getCurrent(), 6000);
}

/**
 * StatdownEffect.startEffect ends with lifeStats.updateCurrentStats() (StatdownEffect.java:19): the fly time restore starts with it, so the fly
 * time 10 short of the maximum is 3 higher 3,000 ms after the soul sickness began.
 */
TEST_F(StatEffectsTest, TheStartOfAStatdownUpdatesTheCurrentStats) {
	EFFECT_TEST_SCOPE;
	skillengine::test::ConfigOverride<int32_t> flyTime(configs::main::CustomConfig::BASE_FLYTIME, 60); // gameserver.base.flytime's default
	Ref<Player> mage = player(6211);
	const int32_t maxFp = mage->getLifeStats()->getMaxFp();
	ASSERT_EQ(maxFp, 60);
	mage->getLifeStats()->setCurrentFp(maxFp - 10);
	advance(3000);
	ASSERT_EQ(mage->getLifeStats()->getCurrentFp(), maxFp - 10) << "nothing restores the fly time yet";

	applied(8291, *mage, *mage, 1);
	advance(3000);
	EXPECT_EQ(mage->getLifeStats()->getCurrentFp(), maxFp - 7);
}

// ---- StatboostEffect (data-only: BufEffect's bodies) ----------------------------------------------------------------------------------------

/** 140 Boost Physical Attack I, a Warrior's enter-world passive: +7 PHYSICAL_ATTACK (StatAddFunction), permanent, no icon */
TEST_F(StatEffectsTest, TheWarriorPassive140AddsSevenPhysicalAttackWithoutAnIcon) {
	EFFECT_TEST_SCOPE;
	Ref<Player> warrior = player(6301, PlayerClass::WARRIOR);
	clearSent(*warrior);

	Ref<Effect> effect = applied(140, *warrior, *warrior);
	ASSERT_TRUE(effect->isInSuccessEffects(1)) << "a passive skill's effect always lands (EffectTemplate.calculate)";
	EXPECT_TRUE(effect->isPassive());
	EXPECT_EQ(stat(*warrior, StatEnum::PHYSICAL_ATTACK, 100), 107);
	EXPECT_EQ(effect->getDuration(), 0) << "permanent: no end task";
	EXPECT_TRUE(sentTo<SM_ABNORMAL_STATE>(*warrior).empty()) << "a passive effect shows no icon";
	advance(3600000);
	EXPECT_EQ(stat(*warrior, StatEnum::PHYSICAL_ATTACK, 100), 107);
	effect->endEffect();
	EXPECT_EQ(stat(*warrior, StatEnum::PHYSICAL_ATTACK, 100), 100);
}

// ---- WeaponMasteryEffect (WeaponMasteryEffect.java:23-46) -----------------------------------------------------------------------------------

/**
 * 37 Basic Sword Training with the gate Warrior's sword in the main hand: a one-handed weapon, so the PHYSICAL_ATTACK change becomes two
 * mastery functions, MAIN_HAND_POWER and OFF_HAND_POWER, each +16 % as a fixed bonus rate: 1000 + 1000 * 0.16f = 1160 for the main hand; the
 * off hand holds no sword (getOffHandWeaponType is null), and PHYSICAL_ATTACK itself is left alone. 39 (MACE, +20 %) adds nothing to a sword.
 */
TEST_F(StatEffectsTest, SwordTrainingAddsSixteenPercentToTheSwordHand) {
	EFFECT_TEST_SCOPE;
	Ref<Player> warrior = player(6401, PlayerClass::WARRIOR);
	equip(*warrior, ItemGroup::SWORD, 100000094, 64001, MAIN_HAND);
	ASSERT_EQ(warrior->getEquipment().getMainHandWeaponType(), ItemGroup::SWORD);

	Ref<Effect> sword = applied(37, *warrior, *warrior);
	ASSERT_TRUE(sword->isInSuccessEffects(1));
	EXPECT_EQ(stat(*warrior, StatEnum::MAIN_HAND_POWER, 1000), 1160);
	EXPECT_EQ(stat(*warrior, StatEnum::OFF_HAND_POWER, 1000), 1000);
	EXPECT_EQ(stat(*warrior, StatEnum::PHYSICAL_ATTACK, 1000), 1000);

	Ref<Effect> mace = applied(39, *warrior, *warrior);
	ASSERT_TRUE(mace->isInSuccessEffects(1));
	EXPECT_EQ(stat(*warrior, StatEnum::MAIN_HAND_POWER, 1000), 1160) << "the mace training's functions do not apply to a sword";

	sword->endEffect();
	EXPECT_EQ(stat(*warrior, StatEnum::MAIN_HAND_POWER, 1000), 1000);
	mace->endEffect();
}

/** 39 Basic Mace Training with a mace: +20 %, 1000 + 1000 * 0.2f = 1200 */
TEST_F(StatEffectsTest, MaceTrainingAddsTwentyPercentToTheMaceHand) {
	EFFECT_TEST_SCOPE;
	Ref<Player> warrior = player(6411, PlayerClass::WARRIOR);
	equip(*warrior, ItemGroup::MACE, 100100001, 64101, MAIN_HAND);
	applied(39, *warrior, *warrior);
	EXPECT_EQ(stat(*warrior, StatEnum::MAIN_HAND_POWER, 1000), 1200);
	EXPECT_EQ(stat(*warrior, StatEnum::OFF_HAND_POWER, 1000), 1000);
}

/**
 * 51 Advanced Greatsword Training: a two-handed group, so the change keeps its own stat (PHYSICAL_ATTACK +4 %: 1000 + 1000 * 0.04f = 1040) and
 * applies while the main hand holds a greatsword (the default arm of StatWeaponMasteryFunction.apply); with a sword it adds nothing.
 */
TEST_F(StatEffectsTest, GreatswordTrainingKeepsItsStatForATwoHandedWeapon) {
	EFFECT_TEST_SCOPE;
	Ref<Player> greatsword = player(6421, PlayerClass::WARRIOR);
	equip(*greatsword, ItemGroup::GREATSWORD, 100900001, 64201, MAIN_HAND);
	applied(51, *greatsword, *greatsword);
	EXPECT_EQ(stat(*greatsword, StatEnum::PHYSICAL_ATTACK, 1000), 1040);
	EXPECT_EQ(stat(*greatsword, StatEnum::MAIN_HAND_POWER, 1000), 1000);

	Ref<Player> sword = player(6422, PlayerClass::WARRIOR);
	equip(*sword, ItemGroup::SWORD, 100000094, 64202, MAIN_HAND);
	applied(51, *sword, *sword);
	EXPECT_EQ(stat(*sword, StatEnum::PHYSICAL_ATTACK, 1000), 1000);
}

/**
 * A skill hit with two weapons (CalculationType SKILL and DUAL_WIELD) draws the rate: bonusRate = Rnd.get(0, value)
 * (StatWeaponMasteryFunction.java:47-50). The draw comes from a seeded stream whose first Rnd.get(0, 16) is below 16, then from one whose first
 * draw is 0 (the lower bound).
 */
TEST_F(StatEffectsTest, ADualWieldedSkillHitDrawsTheMasteryRate) {
	EFFECT_TEST_SCOPE;
	Ref<Player> warrior = player(6431, PlayerClass::WARRIOR);
	equip(*warrior, ItemGroup::SWORD, 100000094, 64301, MAIN_HAND);
	applied(37, *warrior, *warrior);
	auto getStat = privateMember(GetStatWithTypesTag{});
	CreatureGameStats& stats = *warrior->getGameStats();

	uint64_t seed = 1;
	int32_t roll = 16;
	for (; seed < 1000 && roll == 16; ++seed) {
		Rnd::seedCurrentThreadForTests(seed);
		roll = Rnd::get(0, 16);
	}
	--seed;
	ASSERT_LT(roll, 16);
	Rnd::seedCurrentThreadForTests(seed);
	std::unique_ptr<Stat2> drawn = (stats.*getStat)(StatEnum::MAIN_HAND_POWER, 1000, {CalculationType::SKILL, CalculationType::DUAL_WIELD});
	EXPECT_EQ(drawn->getCurrent(), 1000 + roll * 10) << "fixed bonus rate roll / 100f, roll " << roll;
	std::unique_ptr<Stat2> skillOnly = (stats.*getStat)(StatEnum::MAIN_HAND_POWER, 1000, {CalculationType::SKILL});
	EXPECT_EQ(skillOnly->getCurrent(), 1160) << "one weapon: the whole rate";

	// the draw starts at 0: a roll of 0 adds nothing
	uint64_t zeroSeed = 1;
	for (; zeroSeed < 10000; ++zeroSeed) {
		Rnd::seedCurrentThreadForTests(zeroSeed);
		if (Rnd::get(0, 16) == 0)
			break;
	}
	Rnd::seedCurrentThreadForTests(zeroSeed);
	ASSERT_EQ(Rnd::get(0, 16), 0);
	Rnd::seedCurrentThreadForTests(zeroSeed);
	std::unique_ptr<Stat2> nothing = (stats.*getStat)(StatEnum::MAIN_HAND_POWER, 1000, {CalculationType::SKILL, CalculationType::DUAL_WIELD});
	EXPECT_EQ(nothing->getCurrent(), 1000) << "Rnd.get(0, value) drew 0";
}

/**
 * The two early outs: a <wpnmastery> without <change> starts nothing (`change == null`, WeaponMasteryEffect.java:30-31: not even an empty
 * CreatureGameStats.addEffect, which would send SM_STATS_INFO); one without a weapon attribute has a null itemGroup, and the first change
 * dereferences it (`itemGroup.getItemSubType()`): NullPointerException.
 */
TEST_F(StatEffectsTest, AMasteryWithoutChangesStartsNothingAndOneWithoutAWeaponThrows) {
	EFFECT_TEST_SCOPE;
	Ref<Player> warrior = player(6441, PlayerClass::WARRIOR);
	clearSent(*warrior);
	Ref<Effect> noChange = applied(64025, *warrior, *warrior);
	ASSERT_TRUE(noChange->isInSuccessEffects(1));
	EXPECT_TRUE(sentTo<SM_STATS_INFO>(*warrior).empty()) << "no addEffect, so no onStatsChange";

	Ref<Effect> sword = applied(37, *warrior, *warrior);
	EXPECT_FALSE(sentTo<SM_STATS_INFO>(*warrior).empty()) << "the same passive with a change reaches onStatsChange";

	Ref<Effect> noWeapon = calculated(64026, *warrior, *warrior);
	ASSERT_TRUE(noWeapon->isInSuccessEffects(1));
	EXPECT_THROW(noWeapon->getEffectTemplates()[0]->startEffect(*noWeapon), runtime::NullPointerException);
}

// ---- ShieldMasteryEffect (ShieldMasteryEffect.java:19-33) -----------------------------------------------------------------------------------

/**
 * 50 Advanced Shield Training I: DAMAGE_REDUCE +5 % while a shield is in the off hand (1000 * 5 / 100f = 50, so 1050), nothing without one.
 */
TEST_F(StatEffectsTest, ShieldTrainingAppliesOnlyWithAShield) {
	EFFECT_TEST_SCOPE;
	Ref<Player> shielded = player(6501, PlayerClass::WARRIOR);
	equip(*shielded, ItemGroup::SWORD, 100000094, 65001, MAIN_HAND);
	equip(*shielded, ItemGroup::SHIELD, 115000001, 65002, SUB_HAND);
	ASSERT_TRUE(shielded->getEquipment().isShieldEquipped());
	Ref<Effect> effect = applied(50, *shielded, *shielded);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(stat(*shielded, StatEnum::DAMAGE_REDUCE, 1000), 1050);
	effect->endEffect();
	EXPECT_EQ(stat(*shielded, StatEnum::DAMAGE_REDUCE, 1000), 1000);

	Ref<Player> unshielded = player(6502, PlayerClass::WARRIOR);
	applied(50, *unshielded, *unshielded);
	EXPECT_EQ(stat(*unshielded, StatEnum::DAMAGE_REDUCE, 1000), 1000);
}

/**
 * 43 Basic Shield Training (the gate Warrior's): DAMAGE_REDUCE +0 % - no value changes, but the function is added (SM_STATS_INFO from
 * onStatsChange); a <shieldmastery> without <change> has no modifier, and `masteryModifiers.size() > 0` keeps it from calling addEffect.
 */
TEST_F(StatEffectsTest, ShieldTrainingAddsItsFunctionsOnlyWhenItHasSome) {
	EFFECT_TEST_SCOPE;
	Ref<Player> warrior = player(6511, PlayerClass::WARRIOR);
	equip(*warrior, ItemGroup::SHIELD, 115000001, 65101, SUB_HAND);
	clearSent(*warrior);
	applied(64027, *warrior, *warrior);
	EXPECT_TRUE(sentTo<SM_STATS_INFO>(*warrior).empty());
	applied(43, *warrior, *warrior);
	EXPECT_FALSE(sentTo<SM_STATS_INFO>(*warrior).empty());
	EXPECT_EQ(stat(*warrior, StatEnum::DAMAGE_REDUCE, 1000), 1000) << "+0 %";
}

// ---- WeaponDualEffect (WeaponDualEffect.java:13-46) -----------------------------------------------------------------------------------------

/**
 * 55 Advanced Dual-Wielding I: skill efficiency 40 / 100f, max damage chance 400 + level * 0, min damage ratio (70 + level * 0) / 100f; 70 at
 * skill level 3: 50 / 100f, 20 + 3 * 80 = 260, (63 + 3 * 2) / 100f. Each start and end sends the stats (updateStatsVisually); the end zeroes all
 * three.
 */
TEST_F(StatEffectsTest, DualWieldingSetsTheThreeDualWieldStats) {
	EFFECT_TEST_SCOPE;
	Ref<Player> scout = player(6601, PlayerClass::SCOUT);
	clearSent(*scout);
	Ref<Effect> effect = applied(55, *scout, *scout);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(scout->getGameStats()->getSkillEfficiency(), 40 / 100.0f);
	EXPECT_EQ(scout->getGameStats()->getMaxDamageChance(), 400);
	EXPECT_EQ(scout->getGameStats()->getMinDamageRatio(), 70 / 100.0f);
	EXPECT_EQ(sentTo<SM_STATS_INFO>(*scout).size(), 1u);
	clearSent(*scout);
	effect->endEffect();
	EXPECT_EQ(scout->getGameStats()->getSkillEfficiency(), 0.0f);
	EXPECT_EQ(scout->getGameStats()->getMaxDamageChance(), 0);
	EXPECT_EQ(scout->getGameStats()->getMinDamageRatio(), 0.0f);
	EXPECT_EQ(sentTo<SM_STATS_INFO>(*scout).size(), 1u);

	applied(70, *scout, *scout, 3);
	EXPECT_EQ(scout->getGameStats()->getSkillEfficiency(), 50 / 100.0f);
	EXPECT_EQ(scout->getGameStats()->getMaxDamageChance(), 260);
	EXPECT_EQ(scout->getGameStats()->getMinDamageRatio(), 69 / 100.0f);
}

/** An npc effected: WeaponDualEffect only changes a player's stats (the instanceof arms of startEffect and endEffect) */
TEST_F(StatEffectsTest, DualWieldingOnAnNpcChangesNothing) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = monster();
	Ref<Effect> effect = applied(55, *npc, *npc);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_NO_THROW(effect->endEffect());
}

/**
 * WeaponDualEffect.hasDualWieldEffect (WeaponDualEffect.java:36-45): a player not yet spawned (enter-world) answers from its skill list - any
 * skill with a WEAPONDUAL effect; a spawned one from its skill efficiency, i.e. from a started dual-wield effect.
 */
TEST_F(StatEffectsTest, HasDualWieldEffectAsksTheSkillListBeforeTheSpawn) {
	EFFECT_TEST_SCOPE;
	namespace m = gameserver::model;
	Ref<Player> scout = player(6611, PlayerClass::SCOUT);
	std::vector<Ptr<m::skill::PlayerSkillEntry>> entries;
	for (int32_t skillId : {37, 55}) {
		Ref<m::skill::PlayerSkillEntry> entry = m::skill::PlayerSkillEntry::create(skillId, 1, 0, m::gameobjects::Persistable_PersistentState::NOACTION);
		entries.push_back(Ptr<m::skill::PlayerSkillEntry>(entry));
		learned.push_back(std::move(entry));
	}
	scout->setSkillList(m::skill::PlayerSkillList::create(entries));

	scout->getPosition()->setIsSpawned(false);
	EXPECT_TRUE(WeaponDualEffect::hasDualWieldEffect(*scout)) << "55 has a WEAPONDUAL effect";
	scout->getPosition()->setIsSpawned(true);
	EXPECT_FALSE(WeaponDualEffect::hasDualWieldEffect(*scout)) << "spawned: the skill efficiency, still 0";
	Ref<Effect> effect = applied(55, *scout, *scout);
	EXPECT_TRUE(WeaponDualEffect::hasDualWieldEffect(*scout));
	effect->endEffect();

	Ref<Player> warrior = player(6612, PlayerClass::WARRIOR);
	Ref<m::skill::PlayerSkillEntry> sword = m::skill::PlayerSkillEntry::create(37, 1, 0, m::gameobjects::Persistable_PersistentState::NOACTION);
	warrior->setSkillList(m::skill::PlayerSkillList::create({Ptr<m::skill::PlayerSkillEntry>(sword)}));
	learned.push_back(std::move(sword));
	warrior->getPosition()->setIsSpawned(false);
	EXPECT_FALSE(WeaponDualEffect::hasDualWieldEffect(*warrior)) << "no WEAPONDUAL skill";
	warrior->getPosition()->setIsSpawned(true);
}

} // namespace
} // namespace aion::gameserver::skillengine::effect::mztest
