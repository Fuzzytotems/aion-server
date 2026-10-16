#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/stats/DropRewardEnum.h"

namespace aion::gameserver::utils::stats {

/**
 * Companion of the generated enum DropRewardEnum (docs/design/hub-headers.md §13, the XPRewardEnumInfo.h pattern): Java's constructor data and
 * its methods as free functions (`rewardPercent(reward)` for Java `reward.rewardPercent()`; `dropRewardFrom(diff)` for the static
 * `DropRewardEnum.dropRewardFrom(diff)`). Written by the M5b-3 loot lane under the I-03 lease (m5b3-plan.md L-06); its one caller is
 * DropRegistrationService::getReductionDropRate (DropRegistrationService.java:198-201).
 */

namespace detail {
/** Java constructor argument `levelDifference` in ordinal order (DropRewardEnum.java:7-12) */
inline constexpr std::array<int32_t, 6> DROP_REWARD_LEVEL_DIFFERENCES{{-10, -9, -8, -7, -6, -5}};
static_assert(
	static_cast<size_t>(DropRewardEnum::MINUS_5) + 1 == DROP_REWARD_LEVEL_DIFFERENCES.size(), "one entry per DropRewardEnum constant");

/** Java constructor argument `dropRewardPercent` in ordinal order (DropRewardEnum.java:7-12) */
inline constexpr std::array<int32_t, 6> DROP_REWARD_PERCENTS{{0, 40, 60, 70, 80, 100}};
static_assert(static_cast<size_t>(DropRewardEnum::MINUS_5) + 1 == DROP_REWARD_PERCENTS.size(), "one entry per DropRewardEnum constant");
} // namespace detail

/** Java: DropRewardEnum.rewardPercent() */
constexpr int32_t rewardPercent(DropRewardEnum reward) noexcept {
	return detail::DROP_REWARD_PERCENTS[static_cast<size_t>(reward)];
}

/** Java: DropRewardEnum.levelDifference (private field, read by dropRewardFrom) */
constexpr int32_t levelDifference(DropRewardEnum reward) noexcept {
	return detail::DROP_REWARD_LEVEL_DIFFERENCES[static_cast<size_t>(reward)];
}

/**
 * Java: DropRewardEnum.dropRewardFrom(levelDifference) - the percentage of the constant with that level difference, MINUS_10's at or below -10
 * and MINUS_5's at or above -5 (both bounds inclusive, unlike XPRewardEnum.xpRewardFrom's). The linear search over values() and its
 * NoSuchElementException are kept: the table is contiguous today, so the throw is unreachable, but a data change that leaves a gap must fail
 * the same way it does in Java.
 *
 * @param levelDiff between two objects
 * @return Drop reward percentage
 */
inline int32_t dropRewardFrom(int32_t levelDiff) {
	if (levelDiff <= levelDifference(DropRewardEnum::MINUS_10))
		return rewardPercent(DropRewardEnum::MINUS_10);
	else if (levelDiff >= levelDifference(DropRewardEnum::MINUS_5))
		return rewardPercent(DropRewardEnum::MINUS_5);

	for (size_t i = 0; i < detail::DROP_REWARD_LEVEL_DIFFERENCES.size(); i++) {
		if (detail::DROP_REWARD_LEVEL_DIFFERENCES[i] == levelDiff) {
			return detail::DROP_REWARD_PERCENTS[i];
		}
	}

	throw runtime::NoSuchElementException("Drop reward for such level difference was not found");
}

} // namespace aion::gameserver::utils::stats
