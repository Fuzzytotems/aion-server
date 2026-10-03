#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/utils/stats/XPLossEnum.h"

namespace aion::gameserver::utils::stats {

/**
 * Companion of the generated enum XPLossEnum (docs/design/hub-headers.md §13, pattern XPRewardEnumInfo.h): Java's constructor data and its
 * accessors as free functions (`level(loss)` for Java `loss.getLevel()`, `param(loss)` for `loss.getParam()`). The static getExpLoss keeps
 * its stand-in in model/gameobjects/player/detail/PlayerMath.h, which the tests pin to the same table.
 *
 * @author ATracer, Jangan
 */

namespace detail {
/** Java constructor argument `level` in ordinal order (XPLossEnum.java:8-14) */
inline constexpr std::array<int32_t, 7> XP_LOSS_LEVELS{{6, 30, 40, 50, 55, 60, 65}};
static_assert(static_cast<size_t>(XPLossEnum::LEVEL_65) + 1 == XP_LOSS_LEVELS.size(), "one entry per XPLossEnum constant");

/** Java constructor argument `param` in ordinal order */
inline constexpr std::array<double, 7> XP_LOSS_PARAMS{{1.0, 1.0, 0.35, 0.25, 0.25, 0.25, 0.25}};
static_assert(static_cast<size_t>(XPLossEnum::LEVEL_65) + 1 == XP_LOSS_PARAMS.size(), "one entry per XPLossEnum constant");
} // namespace detail

/** Java: XPLossEnum.getLevel() */
constexpr int32_t level(XPLossEnum loss) noexcept {
	return detail::XP_LOSS_LEVELS[static_cast<size_t>(loss)];
}

/** Java: XPLossEnum.getParam() */
constexpr double param(XPLossEnum loss) noexcept {
	return detail::XP_LOSS_PARAMS[static_cast<size_t>(loss)];
}

} // namespace aion::gameserver::utils::stats
