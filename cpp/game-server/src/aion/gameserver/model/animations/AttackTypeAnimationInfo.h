#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/animations/AttackTypeAnimation.h"

namespace aion::gameserver::model::animations {

/** Companion of the generated enum AttackTypeAnimation (static-data.md §2.5): Java's constructor data as constexpr free functions (ADL). */

namespace detail {
/** AttackTypeAnimation ids in ordinal order */
inline constexpr std::array<int8_t, 2> ATTACK_TYPE_ANIMATION_IDS{{
	0,  // MELEE
	1,  // RANGED
}};
static_assert(static_cast<size_t>(AttackTypeAnimation::RANGED) + 1 == ATTACK_TYPE_ANIMATION_IDS.size(), "one entry per AttackTypeAnimation constant");
} // namespace detail

/** Java: AttackTypeAnimation.getId() */
constexpr int8_t getId(AttackTypeAnimation animation) noexcept {
	return detail::ATTACK_TYPE_ANIMATION_IDS[static_cast<size_t>(animation)];
}

} // namespace aion::gameserver::model::animations
