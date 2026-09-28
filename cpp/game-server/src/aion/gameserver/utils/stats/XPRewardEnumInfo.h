#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/stats/XPRewardEnum.h"

namespace aion::gameserver::utils::stats {

/**
 * Companion of the generated enum XPRewardEnum (docs/design/hub-headers.md §13, pattern NpcRatingInfo.h): Java's constructor data and its one
 * static method as free functions (`rewardPercent(reward)` for Java `reward.rewardPercent()`; `xpRewardFrom(diff)` for the static
 * `XPRewardEnum.xpRewardFrom(diff)`).
 *
 * @author ATracer
 */

namespace detail {
/** Java constructor argument `levelDifference` in ordinal order (XPRewardEnum.java:9-24) */
inline constexpr std::array<int32_t, 16> XP_REWARD_LEVEL_DIFFERENCES{{-11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4}};
static_assert(
	static_cast<size_t>(XPRewardEnum::PLUS_4) + 1 == XP_REWARD_LEVEL_DIFFERENCES.size(), "one entry per XPRewardEnum constant");

/** Java constructor argument `xpRewardPercent` in ordinal order */
inline constexpr std::array<int32_t, 16> XP_REWARD_PERCENTS{{0, 1, 10, 20, 30, 40, 50, 60, 90, 100, 100, 100, 105, 110, 115, 120}};
static_assert(static_cast<size_t>(XPRewardEnum::PLUS_4) + 1 == XP_REWARD_PERCENTS.size(), "one entry per XPRewardEnum constant");
} // namespace detail

/** Java: XPRewardEnum.rewardPercent() */
constexpr int32_t rewardPercent(XPRewardEnum reward) noexcept {
	return detail::XP_REWARD_PERCENTS[static_cast<size_t>(reward)];
}

/** Java: XPRewardEnum.levelDifference (private field, read by xpRewardFrom) */
constexpr int32_t levelDifference(XPRewardEnum reward) noexcept {
	return detail::XP_REWARD_LEVEL_DIFFERENCES[static_cast<size_t>(reward)];
}

/**
 * Java: XPRewardEnum.xpRewardFrom(levelDifference) - the percentage of the constant with that level difference, clamped to MINUS_11 below and
 * PLUS_4 above. The linear search over values() and its NoSuchElementException are kept: the table is contiguous today, so the throw is
 * unreachable, but a data change that leaves a gap must fail the same way it does in Java.
 *
 * @return XP reward percentage
 */
inline int32_t xpRewardFrom(int32_t levelDiff) {
	if (levelDiff < levelDifference(XPRewardEnum::MINUS_11)) {
		return rewardPercent(XPRewardEnum::MINUS_11);
	}
	if (levelDiff > levelDifference(XPRewardEnum::PLUS_4)) {
		return rewardPercent(XPRewardEnum::PLUS_4);
	}

	for (size_t i = 0; i < detail::XP_REWARD_LEVEL_DIFFERENCES.size(); i++) {
		if (detail::XP_REWARD_LEVEL_DIFFERENCES[i] == levelDiff) {
			return detail::XP_REWARD_PERCENTS[i];
		}
	}

	throw runtime::NoSuchElementException("XP reward for such level difference was not found");
}

} // namespace aion::gameserver::utils::stats
