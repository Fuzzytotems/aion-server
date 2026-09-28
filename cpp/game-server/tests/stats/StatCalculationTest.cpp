// Stat calculation of P5-01 (wave 5a, work items B-01..B-03): Stat2, AdditionStat, ReverseStat, StatCapUtil, PlayerStatCalculator, the player stats
// template, CreatureGameStats/PlayerGameStats with the stat functions and PlayerStatFunctions, and the life stats of a fresh player.
// Expectations are derived by hand from the Java sources (float arithmetic and (int) truncation noted at each value).

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <unordered_set>
#include <vector>

#include "StatsTestSupport.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/FallDamageConfig.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/attack/AttackUtil.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/SkillElement.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/stats/calc/AdditionStat.h"
#include "aion/gameserver/model/stats/calc/PlayerStatCalculator.h"
#include "aion/gameserver/model/stats/calc/ReverseStat.h"
#include "aion/gameserver/model/stats/calc/StatCapUtil.h"
#include "aion/gameserver/model/stats/calc/functions/IStatFunction.h"
#include "aion/gameserver/model/stats/calc/functions/PlayerStatFunctions.h"
#include "aion/gameserver/model/stats/calc/functions/StatAbsFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatAddFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatArmorMasteryFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunctionProxy.h"
#include "aion/gameserver/model/stats/calc/functions/StatRateFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatSetFunction.h"
#include "aion/gameserver/model/stats/container/CombatMode.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/stats/container/RatioType.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/item/enums/ItemSubType.h"
#include "aion/gameserver/model/templates/stats/StatsTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/utils/stats/CalculationType.h"
#include "aion/gameserver/utils/stats/StatFunctions.h"

namespace aion::gameserver::model::stats::test {
namespace {

using calc::AdditionStat;
using calc::PlayerStatCalculator;
using calc::ReverseStat;
using calc::StatCapUtil;
using calc::functions::IStatFunction;
using calc::functions::RcStatFunction;
using container::StatEnum;
using runtime::Ptr;
using runtime::Ref;

constexpr int32_t INT_MAX_VALUE = std::numeric_limits<int32_t>::max();

std::vector<Ptr<IStatFunction>> functionsOf(std::initializer_list<Ref<IStatFunction>> functions) {
	return std::vector<Ptr<IStatFunction>>(functions.begin(), functions.end());
}

TEST(PlayerStatCalculatorTest, JavaFloatFormulas) {
	// maxHp = (int) (hm / 2 + level * 0.1075f * hm + level * level * 0.002875f * hm)
	EXPECT_EQ(PlayerStatCalculator::calculateMaxHp(PlayerClass::WARRIOR, 0), 200);
	EXPECT_EQ(PlayerStatCalculator::calculateMaxHp(PlayerClass::WARRIOR, 1), 244);  // 200 + 43 + 1.15
	EXPECT_EQ(PlayerStatCalculator::calculateMaxHp(PlayerClass::WARRIOR, 10), 745); // 200 + 430 + 115
	EXPECT_EQ(PlayerStatCalculator::calculateMaxHp(PlayerClass::MAGE, 1), 158);     // 130 + 27.95 + 0.7475
	EXPECT_EQ(PlayerStatCalculator::calculateMaxHp(PlayerClass::GLADIATOR, 65), 8639); // 220 + 65 * 47.3f + 4225 * 1.265f = 8639.125 (in float)
	// maxMp = (int) (wm * 0.35f + level * base / 2 + level * level * wm * 0.125f / 10000)
	EXPECT_EQ(PlayerStatCalculator::calculateMaxMp(PlayerClass::WARRIOR, 0), 140);
	EXPECT_EQ(PlayerStatCalculator::calculateMaxMp(PlayerClass::WARRIOR, 1), 210); // 140 + 70 + 0.005
	EXPECT_EQ(PlayerStatCalculator::calculateMaxMp(PlayerClass::MAGE, 1), 315);    // 210 + 105 + 0.0075
	EXPECT_EQ(PlayerStatCalculator::calculateBlockEvasionOrParry(1), 74);         // 62 + 12.4
	EXPECT_EQ(PlayerStatCalculator::calculateBlockEvasionOrParry(10), 186);       // 62 + 124
	EXPECT_EQ(PlayerStatCalculator::calculateMagicalAccuracy(1), 14);             // 14.26
	EXPECT_EQ(PlayerStatCalculator::calculatePhysicalAccuracy(1), 198);
	EXPECT_EQ(PlayerStatCalculator::calculateStrikeResist(50), 0);
	EXPECT_EQ(PlayerStatCalculator::calculateStrikeResist(55), 30);
}

TEST(PlayerStatsTemplateTest, CreatedPerClassAndLevelWithTheClassAttributes) {
	const templates::stats::StatsTemplate* warrior = createStatsTemplate(PlayerClass::WARRIOR, 1);
	ASSERT_NE(warrior, nullptr);
	EXPECT_EQ(warrior->getMaxHp(), 244);
	EXPECT_EQ(warrior->getMaxMp(), 210);
	EXPECT_EQ(warrior->getBlock(), 74);
	EXPECT_EQ(warrior->getParry(), 74);
	EXPECT_EQ(warrior->getEvasion(), 74);
	EXPECT_EQ(warrior->getAccuracy(), 198);
	EXPECT_EQ(warrior->getMacc(), 14);
	EXPECT_EQ(warrior->getAttack(), 18);
	EXPECT_EQ(warrior->getPcrit(), 2);
	EXPECT_EQ(warrior->getMcrit(), 50);
	EXPECT_EQ(warrior->getStrikeResist(), 0);
	EXPECT_EQ(warrior->getSpellResist(), 0);
	EXPECT_EQ(warrior->getMagicalAttack(), 0) << "not set by createStatsTemplate";
	EXPECT_EQ(warrior->getPower(), 110);
	EXPECT_EQ(warrior->getHealth(), 110);
	EXPECT_EQ(warrior->getAgility(), 100);
	EXPECT_EQ(warrior->getBaseAccuracy(), 100);
	EXPECT_EQ(warrior->getKnowledge(), 90);
	EXPECT_EQ(warrior->getWill(), 90);
	EXPECT_FLOAT_EQ(warrior->getWalkSpeed(), 1.5f);
	EXPECT_FLOAT_EQ(warrior->getRunSpeed(), 6.0f);
	EXPECT_FLOAT_EQ(warrior->getFlySpeed(), 9.0f);
	EXPECT_FLOAT_EQ(warrior->getRunSpeedFight(), 0.0f) << "no speeds element";
	EXPECT_EQ(createStatsTemplate(PlayerClass::WARRIOR, 1), warrior) << "C++ interns one template per class and level";
	EXPECT_NE(createStatsTemplate(PlayerClass::WARRIOR, 2), warrior);
	const templates::stats::StatsTemplate* sorcerer = createStatsTemplate(PlayerClass::SORCERER, 55);
	EXPECT_EQ(sorcerer->getSpellResist(), 50);
	EXPECT_EQ(sorcerer->getStrikeResist(), 30);
	EXPECT_EQ(sorcerer->getKnowledge(), 120);
}

class StatCalculationTest : public StatsPlayerTest {};

TEST_F(StatCalculationTest, Stat2ArithmeticTruncatesAndSaturatesLikeJava) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture f = makePlayer(1001, PlayerClass::WARRIOR);
	AdditionStat stat(StatEnum::POWER, 10.9f, *f.player);
	EXPECT_EQ(stat.getBase(), 10);
	EXPECT_EQ(stat.getBaseWithoutBaseRate(), 10);
	stat.addToBase(5.0f);
	stat.addToBonus(-2.5f);
	EXPECT_FLOAT_EQ(stat.getExactCurrent(), 13.4f);
	EXPECT_EQ(stat.getCurrent(), 13);
	EXPECT_EQ(stat.getBonus(), -2) << "(int) truncates towards zero";
	stat.setBaseRate(2.0f);
	stat.setBonusRate(0.5f);
	stat.setFixedBonusRate(0.1f);
	stat.setFinalRate(0.5f);
	// (base * baseRate + bonus * bonusRate + base * fixedBonusRate) * finalRate = (31.8 - 1.25 + 1.59) * 0.5
	EXPECT_NEAR(stat.getExactCurrent(), 16.07f, 0.001f);
	EXPECT_NEAR(stat.getExactCurrentWithoutBonus(), 16.695f, 0.001f);
	EXPECT_NEAR(stat.getExactCurrentWithoutFixedBonus(), 15.275f, 0.001f);
	EXPECT_FLOAT_EQ(stat.calculatePercent(20), 1.2f);

	AdditionStat huge(StatEnum::MAXHP, 3.0e9f, *f.player);
	EXPECT_EQ(huge.getCurrent(), INT_MAX_VALUE) << "Java (int) saturates";
	AdditionStat negativeHuge(StatEnum::MAXHP, -3.0e9f, *f.player);
	EXPECT_EQ(negativeHuge.getCurrent(), std::numeric_limits<int32_t>::min());

	ReverseStat reverse(StatEnum::BOOST_CASTING_TIME, 100.0f, *f.player);
	reverse.addToBase(30.0f);
	EXPECT_EQ(reverse.getBase(), 70);
	reverse.addToBase(500.0f);
	EXPECT_EQ(reverse.getBase(), 0) << "a reverse base never drops below 0";
	reverse.addToBonus(10.0f);
	EXPECT_EQ(reverse.getBonus(), -10);
	EXPECT_FLOAT_EQ(reverse.calculatePercent(20), 0.8f);
	EXPECT_FLOAT_EQ(reverse.calculatePercent(150), 0.0f);
}

TEST_F(StatCalculationTest, StatCapRules) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture f = makePlayer(1002, PlayerClass::WARRIOR);
	gameobjects::player::Player& player = *f.player;

	AdditionStat maxHp(StatEnum::MAXHP, 5.0f, player);
	StatCapUtil::calculateBaseValue(maxHp, player);
	EXPECT_EQ(maxHp.getCurrent(), 100) << "players have at least 100 max HP (the bonus fills the gap)";
	EXPECT_EQ(maxHp.getBase(), 5);

	AdditionStat speed(StatEnum::SPEED, 20000.0f, player);
	speed.setFinalRate(0.5f);
	StatCapUtil::calculateBaseValue(speed, player);
	EXPECT_EQ(speed.getCurrent(), 10000) << "the cap is computed with the final rate, which stays";
	AdditionStat fastSpeed(StatEnum::SPEED, 20000.0f, player);
	StatCapUtil::calculateBaseValue(fastSpeed, player);
	EXPECT_EQ(fastSpeed.getCurrent(), 12000) << "non-staff players run at most 12000";

	AdditionStat attackSpeed(StatEnum::ATTACK_SPEED, 400.0f, player);
	StatCapUtil::calculateBaseValue(attackSpeed, player);
	EXPECT_EQ(attackSpeed.getCurrent(), 500) << "[500, 10000], then [base * 0.5, base * 2] = [200, 800]";
	AdditionStat slowAttack(StatEnum::ATTACK_SPEED, 1500.0f, player);
	slowAttack.addToBonus(2000.0f);
	StatCapUtil::calculateBaseValue(slowAttack, player);
	EXPECT_EQ(slowAttack.getCurrent(), 3000) << "capped to base * 2";

	AdditionStat fire(StatEnum::FIRE_RESISTANCE, 2000.0f, player);
	StatCapUtil::calculateBaseValue(fire, player);
	EXPECT_EQ(fire.getCurrent(), 1000) << "player elemental defense cap: 1000 + max(0, level - 50) * 10";
	AdditionStat water(StatEnum::WATER_RESISTANCE, -2000.0f, player);
	StatCapUtil::calculateBaseValue(water, player);
	EXPECT_EQ(water.getCurrent(), -1000);

	AdditionStat unlimited(StatEnum::CONCENTRATION, -50.0f, player);
	StatCapUtil::calculateBaseValue(unlimited, player);
	EXPECT_EQ(unlimited.getCurrent(), -50) << "no rule: unlimited";

	EXPECT_EQ(StatCapUtil::getLowerCap(StatEnum::POWER, player), 80);
	EXPECT_EQ(StatCapUtil::getUpperCap(StatEnum::POWER, player), 999);
	EXPECT_EQ(StatCapUtil::getLowerCap(StatEnum::MAXMP, player), 1);
	EXPECT_EQ(StatCapUtil::getUpperCap(StatEnum::MAXMP, player), INT_MAX_VALUE);
	EXPECT_EQ(StatCapUtil::getUpperCap(StatEnum::FLY_SPEED, player), 16000);
	EXPECT_EQ(StatCapUtil::getLowerCap(StatEnum::CONCENTRATION, player), std::numeric_limits<int32_t>::min());
	EXPECT_EQ(StatCapUtil::getDifferenceLimit(StatEnum::MAGICAL_RESIST), 900);
	EXPECT_EQ(StatCapUtil::getDifferenceLimit(StatEnum::BOOST_MAGICAL_SKILL), 2900);
	EXPECT_EQ(StatCapUtil::getDifferenceLimit(StatEnum::POWER), INT_MAX_VALUE);
	EXPECT_EQ(StatCapUtil::getElementalDefenseBaseValue(), 1300);
	EXPECT_EQ(StatCapUtil::clampStatValue(StatEnum::HEAL_BOOST, player, 5000), 1000);
	EXPECT_EQ(StatCapUtil::clampStatValue(StatEnum::HEAL_BOOST, player, -5000), -1000);
	EXPECT_EQ(StatCapUtil::limitValueForPvpOrPveStat(container::CombatMode::PVP, container::RatioType::ATTACK, 2000), 1000);
	EXPECT_EQ(StatCapUtil::limitValueForPvpOrPveStat(container::CombatMode::PVP, container::RatioType::DEFENSE, -2000), -1000);
	EXPECT_EQ(StatCapUtil::limitValueForPvpOrPveStat(container::CombatMode::PVE, container::RatioType::DEFENSE, -9000), -5000);
	EXPECT_EQ(StatCapUtil::limitValueForPvpOrPveStat(container::CombatMode::PVE, container::RatioType::ATTACK, 4000), 4000);
	EXPECT_FLOAT_EQ(utils::stats::StatFunctions::limit(StatEnum::EVASION, 500.0f), 300.0f);
	EXPECT_FLOAT_EQ(utils::stats::StatFunctions::limit(StatEnum::EVASION, 120.5f), 120.5f);
}

TEST_F(StatCalculationTest, FreshWarriorStatsWithThePredefinedPlayerFunctions) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	AtomicConfigScope<int32_t> flyTime(configs::main::CustomConfig::BASE_FLYTIME, 60);
	PlayerFixture f = makePlayer(1003, PlayerClass::WARRIOR);
	Ptr<container::PlayerGameStats> stats = f.player->getGameStats();
	Ptr<container::PlayerLifeStats> life = f.player->getLifeStats();
	ASSERT_TRUE(stats);
	ASSERT_TRUE(life);
	// level 0 (no experience table): the template of PlayerClass.createStatsTemplate(0)
	EXPECT_EQ(stats->getStatsTemplate(), createStatsTemplate(PlayerClass::WARRIOR, 0));
	EXPECT_EQ(stats->getMaxHp()->getCurrent(), 200);
	EXPECT_EQ(stats->getMaxMp()->getCurrent(), 140);
	// PlayerLifeStats(Player) reads the stats of the moment
	EXPECT_EQ(life->getCurrentHp(), 200);
	EXPECT_EQ(life->getCurrentMp(), 140);
	EXPECT_EQ(life->getCurrentFp(), 60);

	// PlayerService.getPlayer: PlayerStatFunctions.addPredefinedStatFunctions
	calc::functions::PlayerStatFunctions::addPredefinedStatFunctions(*f.player);
	EXPECT_EQ(calc::functions::PlayerStatFunctions::getFunctions().size(), 15u);
	// MAXHP: 200 + (int) ((110 - 100) / 100f * 400) = 240; MAXMP: 140 + (int) ((90 - 100) / 100f * 400) = 100
	EXPECT_EQ(stats->getMaxHp()->getCurrent(), 240);
	EXPECT_EQ(stats->getMaxHp()->getBase(), 240);
	EXPECT_EQ(stats->getMaxMp()->getCurrent(), 100);
	EXPECT_EQ(stats->getHealthDependentAdditionalHp(), 40);
	EXPECT_EQ(stats->getWillDependentAdditionalMp(), -40);
	EXPECT_EQ(stats->getPower()->getCurrent(), 110);
	EXPECT_EQ(stats->getKnowledge()->getCurrent(), 90);
	EXPECT_EQ(stats->getBlock()->getCurrent(), 62) << "agility 100 adds nothing";
	EXPECT_EQ(stats->getEvasion()->getCurrent(), 62);
	EXPECT_EQ(stats->getParry()->getCurrent(), 62) << "no main hand weapon";
	EXPECT_EQ(stats->getMainHandPAccuracy()->getCurrent(), 190);
	EXPECT_EQ(stats->getMainHandPCritical()->getCurrent(), 2);
	EXPECT_EQ(stats->getMCritical()->getCurrent(), 50);
	// PhysicalAttackFunction without a weapon: baseRate = 1 + (110 - 100) * 70 / 10000f = 1.07, 18 * 1.07 = 19.26
	EXPECT_EQ(stats->getMainHandPAttack({utils::stats::CalculationType::DISPLAY})->getCurrent(), 19);
	EXPECT_EQ(stats->getMainHandPAttack({utils::stats::CalculationType::DISPLAY})->getBase(), 19);
	EXPECT_EQ(stats->getOffHandPAttack({utils::stats::CalculationType::DISPLAY})->getCurrent(), 0) << "no off hand weapon";
	EXPECT_EQ(stats->getMainHandMAttack({utils::stats::CalculationType::DISPLAY})->getCurrent(), 0);
	EXPECT_EQ(stats->getOffHandPCritical()->getCurrent(), 0);
	EXPECT_EQ(stats->getOffHandPAccuracy()->getCurrent(), 0);
	EXPECT_EQ(stats->getMBoost()->getCurrent(), 0);
	EXPECT_EQ(stats->getMAccuracy()->getCurrent(), 0);
	EXPECT_EQ(stats->getMaxDp()->getCurrent(), 4000);
	EXPECT_EQ(stats->getFlyTime()->getCurrent(), 60);
	EXPECT_EQ(stats->getBaseAttackSpeed(), 1500);
	EXPECT_EQ(stats->getAttackSpeed()->getCurrent(), 1500);
	EXPECT_FLOAT_EQ(stats->getAttackSpeedRate(), 1.0f);
	EXPECT_EQ(stats->getAttackRange()->getCurrent(), 1500);
	EXPECT_EQ(stats->getMovementSpeed()->getCurrent(), 6000);
	EXPECT_FLOAT_EQ(stats->getMovementSpeedFloat(), 6.0f);
	EXPECT_EQ(stats->getHpRegenRate()->getCurrent(), 3) << "(int) (3 * 1.1f)";
	EXPECT_EQ(stats->getMpRegenRate()->getCurrent(), 7) << "(int) (8 * 0.9f)";
	EXPECT_EQ(stats->getReverseStat(StatEnum::BOOST_CASTING_TIME, 1000.0f)->getCurrent(), 1000);
	EXPECT_EQ(stats->getPositiveReverseStat(StatEnum::BOOST_CASTING_TIME, 1000), 1000);
	EXPECT_EQ(stats->getElementalDefenseFor(SkillElement::FIRE), 0);
	EXPECT_FLOAT_EQ(stats->getOffHandDamageRatio(), 0.0f);

	// Equipment.onLoadApplyEquipmentStats ends with lifeStats.synchronizeWithMaxStats (not spawned: no packets)
	life->synchronizeWithMaxStats();
	EXPECT_EQ(life->getCurrentHp(), 240);
	EXPECT_EQ(life->getCurrentMp(), 100);
	EXPECT_EQ(life->getMaxFp(), 60);
	EXPECT_TRUE(life->isFullyRestoredHpMp());
	EXPECT_TRUE(life->isFlyTimeFullyRestored());
	EXPECT_EQ(life->getHpPercentage(), 100);
	EXPECT_EQ(life->getMpPercentage(), 100);
	EXPECT_EQ(life->getFpPercentage(), 100);
}

TEST_F(StatCalculationTest, MageBaseValues) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture f = makePlayer(1004, PlayerClass::MAGE);
	calc::functions::PlayerStatFunctions::addPredefinedStatFunctions(*f.player);
	Ptr<container::PlayerGameStats> stats = f.player->getGameStats();
	// MAXHP: 130 + (int) ((90 - 100) / 100f * 260) = 104; MAXMP: 210 + (int) ((115 - 100) / 100f * 600) = 300
	EXPECT_EQ(stats->getMaxHp()->getCurrent(), 104);
	EXPECT_EQ(stats->getMaxMp()->getCurrent(), 300);
	// agility 95: (int) ((95 - 100) / 100f * 310) = -15
	EXPECT_EQ(stats->getAgilityDependentAdditionalBaseBlock(), -15);
	EXPECT_EQ(stats->getBlock()->getCurrent(), 47);
	// accuracy 95: physical accuracy (int) (-0.05f * 200) = -10, physical critical (int) (-0.05f * 10) = 0
	EXPECT_EQ(stats->getMainHandPAccuracy()->getCurrent(), 180);
	EXPECT_EQ(stats->getMainHandPCritical()->getCurrent(), 2);
	// MagicalAttackFunction: baseRate knowledge 115 * 0.01f, matk base 0
	EXPECT_EQ(stats->getMainHandMAttack(std::unordered_set<utils::stats::CalculationType>{})->getCurrent(), 0);
}

TEST_F(StatCalculationTest, StatFunctionsInPriorityOrderAndEndEffectByOwner) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture f = makePlayer(1005, PlayerClass::WARRIOR);
	Ptr<container::PlayerGameStats> stats = f.player->getGameStats();
	Ref<TestStatOwner> owner = TestStatOwner::create();
	Ref<TestStatOwner> otherOwner = TestStatOwner::create();

	// added in reverse priority order: the rate bonus (50) must run after the base addition (30)
	stats->addEffectOnly(Ptr<calc::StatOwner>(*owner),
		functionsOf({RcStatFunction<calc::functions::StatRateFunction>::create(StatEnum::MAXHP, 10, true),
			RcStatFunction<calc::functions::StatAddFunction>::create(StatEnum::MAXHP, 100, false)}));
	// base 200 + 100 = 300, bonus 300 * 10 / 100f = 30
	EXPECT_EQ(stats->getMaxHp()->getCurrent(), 330);
	std::vector<Ptr<IStatFunction>> sorted = stats->getStatsSorted(StatEnum::MAXHP);
	ASSERT_EQ(sorted.size(), 2u);
	EXPECT_EQ(sorted[0]->getPriority(), 30);
	EXPECT_EQ(sorted[1]->getPriority(), 50);
	// the functions have no owner, so addEffectOnly wrapped them into proxies of the owner
	for (const Ptr<IStatFunction>& function : sorted) {
		EXPECT_NE(runtime::as<calc::functions::StatFunctionProxy>(function), nullptr);
		EXPECT_EQ(function->getOwner().get(), static_cast<calc::StatOwner*>(owner.get()));
		EXPECT_EQ(function->getName(), StatEnum::MAXHP);
	}

	// physical defense: add 50, rate bonus 20 (+10), then the absolute function (80) replaces base and bonus
	stats->addEffectOnly(Ptr<calc::StatOwner>(*otherOwner),
		functionsOf({RcStatFunction<calc::functions::StatAddFunction>::create(StatEnum::PHYSICAL_DEFENSE, 50, false),
			RcStatFunction<calc::functions::StatRateFunction>::create(StatEnum::PHYSICAL_DEFENSE, 20, true)}));
	EXPECT_EQ(stats->getPDef()->getCurrent(), 60);
	stats->addEffectOnly(Ptr<calc::StatOwner>(*otherOwner), functionsOf({RcStatFunction<calc::functions::StatAbsFunction>::create(StatEnum::PHYSICAL_DEFENSE, 5, false)}));
	EXPECT_EQ(stats->getPDef()->getCurrent(), 5);
	stats->addEffectOnly(Ptr<calc::StatOwner>(*otherOwner), functionsOf({RcStatFunction<calc::functions::StatSetFunction>::create(StatEnum::MAGICAL_DEFEND, 77)}));
	EXPECT_EQ(stats->getMDef()->getCurrent(), 77);
	// a non-rate function of a negative rate: StatRateFunction base (not bonus) multiplies the base by calculatePercent
	stats->addEffectOnly(Ptr<calc::StatOwner>(*otherOwner), functionsOf({RcStatFunction<calc::functions::StatRateFunction>::create(StatEnum::EVASION, -50, false)}));
	EXPECT_EQ(stats->getEvasion()->getCurrent(), 31) << "62 * (100 - 50) / 100f";

	// endEffect removes only the functions of that owner (the stats changed: onStatsChange adjusts HP, sends SM_STATS_INFO to the offline player)
	stats->endEffect(*owner);
	EXPECT_EQ(stats->getMaxHp()->getCurrent(), 200);
	EXPECT_EQ(stats->getPDef()->getCurrent(), 5);
	stats->endEffect(*otherOwner);
	EXPECT_EQ(stats->getPDef()->getCurrent(), 0);
	EXPECT_TRUE(stats->getStatsSorted(StatEnum::PHYSICAL_DEFENSE).empty());
	EXPECT_EQ(f.player->getLifeStats()->getCurrentHp(), 200);
}

TEST_F(StatCalculationTest, OnStatsChangeScalesTheCurrentHpToTheNewMaximum) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture f = makePlayer(1006, PlayerClass::WARRIOR);
	Ptr<container::PlayerGameStats> stats = f.player->getGameStats();
	Ref<TestStatOwner> owner = TestStatOwner::create();
	f.player->getLifeStats()->setCurrentHp(100);
	// addEffect: max HP 200 -> 300, current HP Math.round(100 * 1.5f) = 150
	stats->addEffect(Ptr<calc::StatOwner>(*owner), functionsOf({RcStatFunction<calc::functions::StatAddFunction>::create(StatEnum::MAXHP, 100, false)}));
	EXPECT_EQ(f.player->getLifeStats()->getCurrentHp(), 150);
	stats->endEffect(*owner);
	EXPECT_EQ(f.player->getLifeStats()->getCurrentHp(), 100) << "Math.round(150 * (200 / 300f))";
}

TEST_F(StatCalculationTest, LifeStatsChangesOfAnOfflinePlayer) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	AtomicConfigScope<int32_t> flyTime(configs::main::CustomConfig::BASE_FLYTIME, 60);
	PlayerFixture f = makePlayer(1007, PlayerClass::WARRIOR);
	Ptr<container::PlayerLifeStats> life = f.player->getLifeStats();
	using TYPE = network::aion::serverpackets::SM_ATTACK_STATUS_TYPE;
	using LOG = network::aion::serverpackets::SM_ATTACK_STATUS_LOG;

	EXPECT_FALSE(life->isDead());
	EXPECT_FALSE(life->isAboutToDie());
	life->setCurrentHp(50);
	EXPECT_EQ(life->getCurrentHp(), 50);
	EXPECT_EQ(life->getHpPercentage(), 25);
	life->setCurrentHp(1);
	EXPECT_EQ(life->getHpPercentage(), 1) << "at least 1% while alive";
	life->setCurrentHp(5000);
	EXPECT_EQ(life->getCurrentHp(), 200) << "clamped to max HP";
	life->setCurrentHpPercent(30);
	EXPECT_EQ(life->getCurrentHp(), 60);
	life->setCurrentMp(500);
	EXPECT_EQ(life->getCurrentMp(), 140);
	life->setCurrentMpPercent(50);
	EXPECT_EQ(life->getCurrentMp(), 70);
	EXPECT_EQ(life->getMpPercentage(), 50);

	// a skill cost (USED_HP) never kills
	EXPECT_EQ(life->reduceHp(TYPE::USED_HP, 1000, 0, LOG::REGULAR, *f.player), 1);
	EXPECT_FALSE(life->isDead());
	EXPECT_EQ(life->reduceMp(TYPE::USED_MP, 30, 0, LOG::REGULAR), 40);
	EXPECT_EQ(life->reduceMp(TYPE::USED_MP, 300, 0, LOG::REGULAR), 0);
	EXPECT_EQ(life->increaseMp(1000), 140);
	EXPECT_EQ(life->increaseHp(TYPE::HP, 20), 21);

	// flight points
	EXPECT_EQ(life->getCurrentFp(), 60);
	EXPECT_EQ(life->reduceFp(TYPE::FP, 25, 0, LOG::REGULAR), 35);
	EXPECT_EQ(life->increaseFp(TYPE::NATURAL_FP, 100, 0, LOG::REGULAR), 60) << "capped to the max FP";
	EXPECT_EQ(life->setCurrentFp(-5), 0);
	life->restoreFp();
	EXPECT_EQ(life->getCurrentFp(), 3);

	// no task was scheduled: the cancels do nothing
	EXPECT_NO_THROW(life->cancelAllTasks());
	EXPECT_NO_THROW(life->cancelRestoreTask());
	EXPECT_EQ(life->getFlightReducePeriod(), 2);
	EXPECT_EQ(life->getFlightReduceValue(), 1);
}

TEST_F(StatCalculationTest, AggroListAndTargetHelpersWithoutAggro) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture f = makePlayer(1008, PlayerClass::WARRIOR);
	PlayerFixture other = makePlayer(1009, PlayerClass::MAGE);
	controllers::attack::AggroList& aggroList = f.player->getAggroList();
	EXPECT_FALSE(aggroList.isHating(*other.player));
	EXPECT_EQ(aggroList.getHate(*other.player), 0);
	EXPECT_NO_THROW(aggroList.remove(*other.player));
	EXPECT_NO_THROW(aggroList.remove(*other.player, false));
	EXPECT_NO_THROW(aggroList.stopHating(*other.player));
	EXPECT_NO_THROW(aggroList.clear());

	// nobody knows the player: nothing to cancel or untarget
	EXPECT_NO_THROW(controllers::attack::AttackUtil::cancelCastOn(*f.player));
	EXPECT_NO_THROW(controllers::attack::AttackUtil::removeTargetFrom(*f.player));
	EXPECT_NO_THROW(controllers::attack::AttackUtil::removeTargetFrom(*f.player, true));
}

TEST_F(StatCalculationTest, ArmorMasteryFunctionWithoutArmor) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture f = makePlayer(1010, PlayerClass::WARRIOR);
	Ref<calc::functions::StatArmorMasteryFunction> mastery = calc::functions::StatArmorMasteryFunction::create(
		templates::item::enums::ItemSubType::CHAIN, StatEnum::PHYSICAL_DEFENSE, 20, true, 30, f.player->getEquipment().getEquippedItems());
	EXPECT_EQ(mastery->getValue(), 0) << "no equipped chain armor: equipment factor 0";
	EXPECT_EQ(mastery->getPriority(), 50);
	Ref<TestStatOwner> owner = TestStatOwner::create();
	f.player->getGameStats()->addEffectOnly(Ptr<calc::StatOwner>(*owner), functionsOf({mastery}));
	EXPECT_NO_THROW(f.player->getGameStats()->updateArmorMasteryStats(f.player->getEquipment().getEquippedItems()));
	EXPECT_EQ(f.player->getGameStats()->getPDef()->getCurrent(), 0);
}

TEST_F(StatCalculationTest, MovementModifierAndFallDamage) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture f = makePlayer(1011, PlayerClass::WARRIOR);
	// a player standing still: no movement modifier; a null stat is returned unchanged
	EXPECT_FLOAT_EQ(utils::stats::StatFunctions::adjustStatByMovementModifier(*f.player, StatEnum::SPEED, 6.0f), 6.0f);
	EXPECT_FLOAT_EQ(utils::stats::StatFunctions::adjustStatByMovementModifier(*f.player, std::nullopt, 6.0f), 6.0f);
	AtomicConfigScope<int32_t> minimum(configs::main::FallDamageConfig::MINIMUM_DISTANCE_DAMAGE, 10);
	AtomicConfigScope<int32_t> maximum(configs::main::FallDamageConfig::MAXIMUM_DISTANCE_DAMAGE, 50);
	AtomicConfigScope<float> percentage(configs::main::FallDamageConfig::FALL_DAMAGE_PERCENTAGE, 1.5f);
	EXPECT_EQ(utils::stats::StatFunctions::calculateFallDamage(*f.player, 5.0f), 0);
	EXPECT_EQ(utils::stats::StatFunctions::calculateFallDamage(*f.player, 12.5f), 37) << "(int) (12.5 * 200 * 1.5 / 100f)";
	EXPECT_EQ(utils::stats::StatFunctions::calculateFallDamage(*f.player, 60.0f), 200) << "the current HP";
}

} // namespace
} // namespace aion::gameserver::model::stats::test
