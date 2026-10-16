#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/gameobjects/state/CreatureVisualState.h"

namespace aion::gameserver::model::gameobjects::state {

/**
 * Companion of the generated enum CreatureVisualState (docs/design/static-data.md §2.5, pattern StorageTypeInfo.h): Java's constructor data and methods as
 * free functions found by ADL (`getId(value)` for Java `value.getId()`).
 *
 * @author Sweetkr
 */

namespace detail {
/** Java constructor argument `id` in ordinal order */
inline constexpr std::array<int32_t, 9> CREATUREVISUALSTATE_IDS{
	0, // VISIBLE
	1, // HIDE1
	2, // HIDE2
	3, // HIDE3
	5, // HIDE5
	10, // HIDE10
	13, // HIDE13
	20, // HIDE20
	64, // BLINKING
};
static_assert(static_cast<size_t>(CreatureVisualState::BLINKING) + 1 == CREATUREVISUALSTATE_IDS.size(), "one entry per CreatureVisualState constant");
} // namespace detail

constexpr int32_t getId(CreatureVisualState value) noexcept {
	return detail::CREATUREVISUALSTATE_IDS[static_cast<size_t>(value)];
}

} // namespace aion::gameserver::model::gameobjects::state
