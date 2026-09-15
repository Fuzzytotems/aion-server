#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/animations/ArrivalAnimation.h"
#include "aion/gameserver/model/animations/ObjectDeleteAnimation.h"
#include "aion/gameserver/model/animations/TeleportAnimation.h"

namespace aion::gameserver::model::animations {

/** Companion of the generated enum TeleportAnimation (static-data.md §2.5): Java's constructor data and methods as constexpr free functions (ADL). */

namespace detail {
/** TeleportAnimation ids (for SM_TELEPORT_LOC) in ordinal order */
inline constexpr std::array<int8_t, 7> TELEPORT_ANIMATION_IDS{{
	0,  // NONE
	1,  // FADE_OUT_BEAM
	2,  // FADE_OUT
	3,  // JUMP_IN
	4,  // JUMP_IN_STATUE
	8,  // JUMP_IN_GATE
	0,  // BATTLEGROUND: for custom battlegrounds/pvp-maps only
}};
static_assert(static_cast<size_t>(TeleportAnimation::BATTLEGROUND) + 1 == TELEPORT_ANIMATION_IDS.size(), "one entry per TeleportAnimation constant");
} // namespace detail

/** Java: TeleportAnimation.getId() */
constexpr int8_t getId(TeleportAnimation animation) noexcept {
	return detail::TELEPORT_ANIMATION_IDS[static_cast<size_t>(animation)];
}

/** Java: TeleportAnimation.getDefaultArrivalAnimation() */
constexpr ArrivalAnimation getDefaultArrivalAnimation(TeleportAnimation animation) noexcept {
	switch (animation) {
		case TeleportAnimation::FADE_OUT_BEAM:
			return ArrivalAnimation::FADE_IN_BEAM;
		case TeleportAnimation::JUMP_IN_STATUE:
			return ArrivalAnimation::JUMP_OUT_CAMERA_FRONT;
		case TeleportAnimation::JUMP_IN:
		case TeleportAnimation::JUMP_IN_GATE:
			return ArrivalAnimation::JUMP_OUT_CAMERA_BEHIND;
		case TeleportAnimation::BATTLEGROUND:
			return ArrivalAnimation::LANDING_GLOW;
		default:
			return ArrivalAnimation::LANDING;
	}
}

/** Java: TeleportAnimation.getDefaultObjectDeleteAnimation() */
constexpr ObjectDeleteAnimation getDefaultObjectDeleteAnimation(TeleportAnimation animation) noexcept {
	switch (animation) {
		case TeleportAnimation::FADE_OUT_BEAM:
			return ObjectDeleteAnimation::FADE_OUT_BEAM;
		case TeleportAnimation::JUMP_IN:
		case TeleportAnimation::JUMP_IN_GATE:
		case TeleportAnimation::JUMP_IN_STATUE:
			return ObjectDeleteAnimation::JUMP_IN;
		default:
			return ObjectDeleteAnimation::FADE_OUT;
	}
}

} // namespace aion::gameserver::model::animations
