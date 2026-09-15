#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/animations/ObjectDeleteAnimation.h"

namespace aion::gameserver::model::animations {

/** Companion of the generated enum ObjectDeleteAnimation (static-data.md §2.5): Java's constructor data as constexpr free functions (ADL). */

namespace detail {
/** ObjectDeleteAnimation ids (for SM_DELETE and SM_PET) in ordinal order */
inline constexpr std::array<int8_t, 5> OBJECT_DELETE_ANIMATION_IDS{{
	0,   // NONE
	1,   // FADE_OUT
	2,   // FADE_OUT_BEAM
	11,  // JUMP_IN: players and humanoids only
	19,  // DELAYED: deletes also flags from map
}};
static_assert(static_cast<size_t>(ObjectDeleteAnimation::DELAYED) + 1 == OBJECT_DELETE_ANIMATION_IDS.size(),
	"one entry per ObjectDeleteAnimation constant");
} // namespace detail

/** Java: ObjectDeleteAnimation.getId() */
constexpr int8_t getId(ObjectDeleteAnimation animation) noexcept {
	return detail::OBJECT_DELETE_ANIMATION_IDS[static_cast<size_t>(animation)];
}

} // namespace aion::gameserver::model::animations
