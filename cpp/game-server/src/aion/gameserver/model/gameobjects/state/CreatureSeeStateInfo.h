#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/gameobjects/state/CreatureSeeState.h"

namespace aion::gameserver::model::gameobjects::state {

/**
 * Companion of the generated enum CreatureSeeState (docs/design/static-data.md §2.5, pattern StorageTypeInfo.h): Java's constructor data and methods as
 * free functions found by ADL (`getId(value)` for Java `value.getId()`).
 *
 * @author Sweetkr
 */

namespace detail {
/** Java constructor argument `id` in ordinal order */
inline constexpr std::array<int32_t, 6> CREATURESEESTATE_IDS{
	0, // NORMAL
	1, // SEARCH1
	2, // SEARCH2
	5, // SEARCH5
	10, // SEARCH10
	20, // SEARCH20
};
static_assert(static_cast<size_t>(CreatureSeeState::SEARCH20) + 1 == CREATURESEESTATE_IDS.size(), "one entry per CreatureSeeState constant");
} // namespace detail

constexpr int32_t getId(CreatureSeeState value) noexcept {
	return detail::CREATURESEESTATE_IDS[static_cast<size_t>(value)];
}

} // namespace aion::gameserver::model::gameobjects::state
