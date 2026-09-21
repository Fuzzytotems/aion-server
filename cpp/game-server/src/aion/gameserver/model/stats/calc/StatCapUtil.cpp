#include "aion/gameserver/model/stats/calc/StatCapUtil.h"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CombatMode.h"
#include "aion/gameserver/model/stats/container/RatioType.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/detail/JavaCasts.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"

namespace aion::gameserver::model::stats::calc {

using container::StatEnum;
using templates::detail::floatToInt;

namespace {

constexpr int32_t INTEGER_MAX_VALUE = std::numeric_limits<int32_t>::max();
constexpr int32_t INTEGER_MIN_VALUE = std::numeric_limits<int32_t>::min();

/** Java: CapFunction.UNLIMITED_LOWER */
int32_t unlimitedLower(gameobjects::Creature&) {
	return INTEGER_MIN_VALUE;
}

/** Java: CapFunction.UNLIMITED_UPPER */
int32_t unlimitedUpper(gameobjects::Creature&) {
	return INTEGER_MAX_VALUE;
}

/** Java: Math.clamp(long value, int min, int max) */
int32_t clamp(int64_t value, int32_t min, int32_t max) {
	if (min > max)
		throw runtime::IllegalArgumentException(std::to_string(min) + " > " + std::to_string(max));
	return static_cast<int32_t>(std::min(std::max(value, static_cast<int64_t>(min)), static_cast<int64_t>(max)));
}

std::string nameOf(StatEnum stat) {
	return std::string(xml::EnumTraits<StatEnum>::names[static_cast<size_t>(stat)]);
}

} // namespace

const StatCapUtil::StatCapRule StatCapUtil::StatCapRule::UNLIMITED{&unlimitedLower, &unlimitedUpper, INTEGER_MAX_VALUE};

const std::map<StatEnum, StatCapUtil::StatCapRule> StatCapUtil::limits = registerDefaults();

std::map<StatEnum, StatCapUtil::StatCapRule> StatCapUtil::registerDefaults() {
	std::map<StatEnum, StatCapRule> rules;
	register_(
		rules, StatEnum::MAXHP, [](gameobjects::Creature& creature) { return runtime::as<gameobjects::player::Player>(creature) ? 100 : 1; },
		&unlimitedUpper);
	register_(
		rules, StatEnum::MAXMP, [](gameobjects::Creature& creature) { return runtime::as<gameobjects::player::Player>(creature) ? 1 : 0; },
		&unlimitedUpper);
	register_(rules, StatEnum::SPEED, 0, [](gameobjects::Creature& creature) {
		runtime::Ptr<gameobjects::player::Player> p = runtime::as<gameobjects::player::Player>(creature);
		return p && !p->isStaff() ? 12000 : INTEGER_MAX_VALUE;
	});
	register_(rules, StatEnum::FLY_SPEED, 0, [](gameobjects::Creature& creature) {
		runtime::Ptr<gameobjects::player::Player> p = runtime::as<gameobjects::player::Player>(creature);
		return p && !p->isStaff() ? 16000 : INTEGER_MAX_VALUE;
	});
	register_(rules, StatEnum::ATTACK_SPEED, 500, 10000);
	register_(rules, StatEnum::HEAL_BOOST, -1000, 1000);
	register_(rules, StatEnum::EVASION, 0, &unlimitedUpper, 300);
	register_(rules, StatEnum::PARRY, 0, &unlimitedUpper, 400);
	register_(rules, StatEnum::BLOCK, 0, &unlimitedUpper, 500);
	register_(rules, StatEnum::PHYSICAL_CRITICAL, 0, &unlimitedUpper, 500);
	register_(rules, StatEnum::MAGICAL_CRITICAL, 0, &unlimitedUpper, 500);
	register_(rules, StatEnum::MAGICAL_RESIST, 0, &unlimitedUpper, 900); // diffLimit in PvP: 500 (see StatFunctions#calculateMagicalResistRate)
	register_(rules, StatEnum::BOOST_MAGICAL_SKILL, 0, &unlimitedUpper, 2900);
	for (StatEnum stat : {StatEnum::PHYSICAL_CRITICAL_RESIST, StatEnum::MAGICAL_CRITICAL_RESIST, StatEnum::PHYSICAL_CRITICAL_DAMAGE_REDUCE,
			 StatEnum::MAGICAL_CRITICAL_DAMAGE_REDUCE})
		register_(rules, stat, 0, 700);
	for (StatEnum stat : {StatEnum::POWER, StatEnum::AGILITY, StatEnum::ACCURACY, StatEnum::HEALTH, StatEnum::KNOWLEDGE, StatEnum::WILL})
		register_(rules, stat, 80, 999);
	for (StatEnum stat : {StatEnum::PHYSICAL_ATTACK, StatEnum::MAGICAL_ATTACK, StatEnum::PHYSICAL_DEFENSE, StatEnum::MAGICAL_DEFEND,
			 StatEnum::PHYSICAL_ACCURACY, StatEnum::MAGICAL_ACCURACY})
		register_(rules, stat, 0, &unlimitedUpper);
	for (StatEnum stat : {StatEnum::WATER_RESISTANCE, StatEnum::FIRE_RESISTANCE, StatEnum::EARTH_RESISTANCE, StatEnum::WIND_RESISTANCE,
			 StatEnum::DARK_RESISTANCE, StatEnum::LIGHT_RESISTANCE})
		register_(
			rules, stat, [](gameobjects::Creature& creature) { return -getElementalDefenseCapForCreature(creature); },
			&StatCapUtil::getElementalDefenseCapForCreature);
	return rules;
}

int32_t StatCapUtil::getElementalDefenseBaseValue() {
	return 1300;
}

void StatCapUtil::calculateBaseValue(Stat2& stat, gameobjects::Creature& creature) {
	int32_t lowerCap = getLowerCap(stat.getStat(), creature);
	int32_t upperCap = getUpperCap(stat.getStat(), creature);

	cap(stat, lowerCap, upperCap);
	if (stat.getStat() == StatEnum::ATTACK_SPEED) // attack delay is first capped to [500, 10000] ms and then to [base * 0.5, base * 2]
		cap(stat, floatToInt(stat.getBase() * 0.5f), static_cast<int32_t>(static_cast<uint32_t>(stat.getBase()) * 2u));
}

int32_t StatCapUtil::getLowerCap(StatEnum stat, gameobjects::Creature& creature) {
	return getRule(stat).lowerCap(creature);
}

int32_t StatCapUtil::getUpperCap(StatEnum stat, gameobjects::Creature& creature) {
	return getRule(stat).upperCap(creature);
}

int32_t StatCapUtil::getElementalDefenseCapForCreature(gameobjects::Creature& creature) {
	if (runtime::as<gameobjects::player::Player>(creature)) {
		return 1000 + std::max(0, creature.getLevel() - 50) * 10;
	}
	return getElementalDefenseBaseValue();
}

int32_t StatCapUtil::getDifferenceLimit(StatEnum stat) {
	return getRule(stat).diffLimit;
}

int32_t StatCapUtil::clampStatValue(StatEnum stat, gameobjects::Creature& creature, int32_t value) {
	int32_t lower = getLowerCap(stat, creature);
	int32_t upper = getUpperCap(stat, creature);
	return clamp(value, lower, upper);
}

int32_t StatCapUtil::limitValueForPvpOrPveStat(container::CombatMode mode, container::RatioType type, int32_t value) {
	// Note: PvP/PvE ratio caps are symmetric:
	// - attack min is fixed, defense max is fixed
	// - upper/lower bounds depend on combat mode
	Cap cap{};
	switch (mode) {
		case container::CombatMode::PVP:
			cap = type == container::RatioType::ATTACK ? Cap{-900, 1000} : Cap{-1000, 900};
			break;
		case container::CombatMode::PVE:
			cap = type == container::RatioType::ATTACK ? Cap{-900, 5000} : Cap{-5000, 900};
			break;
	}

	return clamp(value, cap.min, cap.max);
}

void StatCapUtil::cap(Stat2& stat2, int32_t lowerCap, int32_t upperCap) {
	float exactCurrent = stat2.getExactCurrent();
	if (exactCurrent > upperCap) {
		stat2.setFinalRate(1.0f);
		stat2.setBonusRate(1.0f);
		stat2.setBonus(upperCap - stat2.getExactCurrentWithoutBonus());
	} else if (exactCurrent < lowerCap) {
		stat2.setFinalRate(1.0f);
		stat2.setBonusRate(1.0f);
		stat2.setBonus(lowerCap - stat2.getExactCurrentWithoutBonus());
	}
}

void StatCapUtil::register_(std::map<StatEnum, StatCapRule>& rules, StatEnum stat, int32_t lowerCap, int32_t upperCap) {
	register_(
		rules, stat, [lowerCap](gameobjects::Creature&) { return lowerCap; }, [upperCap](gameobjects::Creature&) { return upperCap; },
		INTEGER_MAX_VALUE);
}

void StatCapUtil::register_(std::map<StatEnum, StatCapRule>& rules, StatEnum stat, CapFunction lowerCap, CapFunction upperCap) {
	register_(rules, stat, std::move(lowerCap), std::move(upperCap), INTEGER_MAX_VALUE);
}

void StatCapUtil::register_(std::map<StatEnum, StatCapRule>& rules, StatEnum stat, int32_t lowerCap, CapFunction upperCap) {
	register_(
		rules, stat, [lowerCap](gameobjects::Creature&) { return lowerCap; }, std::move(upperCap), INTEGER_MAX_VALUE);
}

void StatCapUtil::register_(std::map<StatEnum, StatCapRule>& rules, StatEnum stat, int32_t lowerCap, CapFunction upperCap, int32_t diffLimit) {
	register_(
		rules, stat, [lowerCap](gameobjects::Creature&) { return lowerCap; }, std::move(upperCap), diffLimit);
}

void StatCapUtil::register_(std::map<StatEnum, StatCapRule>& rules, StatEnum stat, CapFunction lowerCap, CapFunction upperCap, int32_t diffLimit) {
	if (!rules.try_emplace(stat, StatCapRule{std::move(lowerCap), std::move(upperCap), diffLimit}).second)
		throw runtime::IllegalArgumentException("A limit for " + nameOf(stat) + " is already registered");
}

const StatCapUtil::StatCapRule& StatCapUtil::getRule(StatEnum stat) {
	auto it = limits.find(stat);
	return it == limits.end() ? StatCapRule::UNLIMITED : it->second;
}

} // namespace aion::gameserver::model::stats::calc
