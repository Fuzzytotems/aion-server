#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/animations/ArrivalAnimation.h"

namespace aion::gameserver::model::animations {

/** Companion of the generated enum ArrivalAnimation (static-data.md §2.5): Java's constructor data as constexpr free functions (ADL). */

namespace detail {
/** ArrivalAnimation ids (for SM_PLAYER_INFO) in ordinal order */
inline constexpr std::array<int8_t, 6> ARRIVAL_ANIMATION_IDS{{
	0,   // NONE
	2,   // LANDING
	4,   // FADE_IN_BEAM
	10,  // JUMP_OUT_CAMERA_BEHIND
	11,  // JUMP_OUT_CAMERA_FRONT
	18,  // LANDING_GLOW
}};
static_assert(static_cast<size_t>(ArrivalAnimation::LANDING_GLOW) + 1 == ARRIVAL_ANIMATION_IDS.size(), "one entry per ArrivalAnimation constant");
} // namespace detail

/** Java: ArrivalAnimation.getId() */
constexpr int8_t getId(ArrivalAnimation animation) noexcept {
	return detail::ARRIVAL_ANIMATION_IDS[static_cast<size_t>(animation)];
}

} // namespace aion::gameserver::model::animations
