#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/gameobjects/state/FlyState.h"

namespace aion::gameserver::model::gameobjects::state {

/**
 * Companion of the generated enum FlyState (docs/design/static-data.md §2.5, pattern StorageTypeInfo.h): Java's constructor data and methods as
 * free functions found by ADL (`getId(value)` for Java `value.getId()`).
 *
 * @author kecimis
 */

namespace detail {
/** Java constructor argument `id` in ordinal order */
inline constexpr std::array<int32_t, 3> FLYSTATE_IDS{
	0, // NONE
	1, // FLYING
	1 << 1, // GLIDING
};
static_assert(static_cast<size_t>(FlyState::GLIDING) + 1 == FLYSTATE_IDS.size(), "one entry per FlyState constant");
} // namespace detail

constexpr int32_t getId(FlyState value) noexcept {
	return detail::FLYSTATE_IDS[static_cast<size_t>(value)];
}

} // namespace aion::gameserver::model::gameobjects::state
