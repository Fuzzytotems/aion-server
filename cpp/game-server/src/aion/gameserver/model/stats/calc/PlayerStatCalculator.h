#pragma once

#include <cstdint>

#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/stats/calc/fwd.h"

namespace aion::gameserver::model::stats::calc {

/**
 * The level and class dependent base values of player stats (PlayerClass.createStatsTemplate).
 * <p>
 * C++: a static-only class of pure arithmetic; Java's float arithmetic and saturating (int) casts are kept.
 *
 * @author Yeats
 */
class PlayerStatCalculator {
public:
	PlayerStatCalculator() = delete;

	static int32_t calculateMaxHp(PlayerClass playerClass, int32_t level);

	static int32_t calculateMaxMp(PlayerClass playerClass, int32_t level);

	static int32_t calculateBlockEvasionOrParry(int32_t level);

	static int32_t calculateMagicalAccuracy(int32_t level);

	static int32_t calculatePhysicalAccuracy(int32_t level);

	static int32_t calculateStrikeResist(int32_t level);
};

} // namespace aion::gameserver::model::stats::calc
