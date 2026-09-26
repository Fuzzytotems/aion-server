#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/animations/AttackHandAnimation.h"

namespace aion::gameserver::model::animations {

/** Companion of the generated enum AttackHandAnimation (static-data.md §2.5): Java's constructor data as constexpr free functions (ADL). */

namespace detail {
/** AttackHandAnimation ids in ordinal order */
inline constexpr std::array<int8_t, 3> ATTACK_HAND_ANIMATION_IDS{{
	0,  // MAIN_HAND
	1,  // OFF_HAND: only some npcs support off hand & random animation e.g. Hyperion
	2,  // RANDOM
}};
static_assert(static_cast<size_t>(AttackHandAnimation::RANDOM) + 1 == ATTACK_HAND_ANIMATION_IDS.size(), "one entry per AttackHandAnimation constant");
} // namespace detail

/** Java: AttackHandAnimation.getId() */
constexpr int8_t getId(AttackHandAnimation animation) noexcept {
	return detail::ATTACK_HAND_ANIMATION_IDS[static_cast<size_t>(animation)];
}

} // namespace aion::gameserver::model::animations
