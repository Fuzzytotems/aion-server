#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/animations/ActionAnimation.h"

namespace aion::gameserver::model::animations {

/** Companion of the generated enum ActionAnimation (static-data.md §2.5): Java's constructor data as constexpr free functions (ADL). */

namespace detail {
/** ActionAnimation ids (for SM_ACTION_ANIMATION) in ordinal order */
inline constexpr std::array<int32_t, 6> ACTION_ANIMATION_IDS{{
	0,  // LEVEL_UP
	1,  // UNK
	2,  // BIND_KISK
	3,  // REPAIR_GATE
	4,  // CRAFT_LEVEL_UP
	4,  // CLASS_CHANGE
}};
static_assert(static_cast<size_t>(ActionAnimation::CLASS_CHANGE) + 1 == ACTION_ANIMATION_IDS.size(), "one entry per ActionAnimation constant");
} // namespace detail

/** Java: ActionAnimation.getId() */
constexpr int32_t getId(ActionAnimation animation) noexcept {
	return detail::ACTION_ANIMATION_IDS[static_cast<size_t>(animation)];
}

} // namespace aion::gameserver::model::animations
