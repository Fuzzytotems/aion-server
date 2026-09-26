#include "aion/gameserver/model/stats/calc/PlayerStatCalculator.h"

#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/templates/detail/JavaCasts.h"

namespace aion::gameserver::model::stats::calc {

using templates::detail::floatToInt;

namespace {

/** Java int multiplication (wraps on overflow) */
constexpr int32_t multiplyInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}

} // namespace

int32_t PlayerStatCalculator::calculateMaxHp(PlayerClass playerClass, int32_t level) {
	int32_t base = getHealthMultiplier(playerClass) / 2;
	float mod1 = 0.1075f * getHealthMultiplier(playerClass);
	float mod2 = 0.002875f * getHealthMultiplier(playerClass);
	// Java: base + level * mod1 + level * level * mod2, evaluated left to right in float (level * level is an int product)
	return floatToInt(base + level * mod1 + multiplyInt(level, level) * mod2);
}

int32_t PlayerStatCalculator::calculateMaxMp(PlayerClass playerClass, int32_t level) {
	float base = getWillMultiplier(playerClass) * 0.35f;
	float mod1 = level * base / 2.0f;
	// Java: level * level * willMultiplier is an int product, then * 0.125f / 10000
	float mod2 = multiplyInt(multiplyInt(level, level), getWillMultiplier(playerClass)) * 0.125f / 10000;
	return floatToInt(base + mod1 + mod2);
}

int32_t PlayerStatCalculator::calculateBlockEvasionOrParry(int32_t level) {
	return floatToInt(62 + 12.4f * level);
}

int32_t PlayerStatCalculator::calculateMagicalAccuracy(int32_t level) {
	return floatToInt(14.26f * level);
}

int32_t PlayerStatCalculator::calculatePhysicalAccuracy(int32_t level) {
	return 190 + 8 * level;
}

int32_t PlayerStatCalculator::calculateStrikeResist(int32_t level) {
	return level > 50 ? 6 * (level - 50) : 0;
}

} // namespace aion::gameserver::model::stats::calc
