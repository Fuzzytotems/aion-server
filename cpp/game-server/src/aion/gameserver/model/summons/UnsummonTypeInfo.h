#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/summons/UnsummonType.h"

namespace aion::gameserver::model::summons {

/** Companion of the generated enum UnsummonType (docs/design/static-data.md §2.5): Java's constructor data and methods (ADL). */

namespace detail {
/** Java: the delayMillis constructor argument, in ordinal order (UnsummonType.java:9-18) */
inline constexpr std::array<int32_t, 8> UNSUMMON_DELAY_MILLIS{{
	0,    // LOGOUT
	0,    // DISTANCE
	3000, // COMMAND
	0,    // SUMMON_DEATH
	0,    // MASTER_DEATH
	0,    // UNSPECIFIED
	3000, // SKILL_ORDER
	0,    // PET_ORDER_UNSUMMON_EFFECT
}};
/** Java: the cancelableByMaster constructor argument, in ordinal order (UnsummonType.java:9-18) */
inline constexpr std::array<bool, 8> UNSUMMON_CANCELABLE_BY_MASTER{{
	false, // LOGOUT
	false, // DISTANCE
	true,  // COMMAND
	false, // SUMMON_DEATH
	false, // MASTER_DEATH
	false, // UNSPECIFIED
	false, // SKILL_ORDER
	false, // PET_ORDER_UNSUMMON_EFFECT
}};
static_assert(static_cast<size_t>(UnsummonType::PET_ORDER_UNSUMMON_EFFECT) + 1 == UNSUMMON_DELAY_MILLIS.size(), "one entry per UnsummonType constant");
static_assert(UNSUMMON_DELAY_MILLIS.size() == UNSUMMON_CANCELABLE_BY_MASTER.size());
} // namespace detail

/** Java: UnsummonType.getDelayMillis() */
constexpr int32_t getDelayMillis(UnsummonType type) noexcept {
	return detail::UNSUMMON_DELAY_MILLIS[static_cast<size_t>(type)];
}

/** Java: UnsummonType.isInstant(): delayMillis == 0 */
constexpr bool isInstant(UnsummonType type) noexcept {
	return getDelayMillis(type) == 0;
}

/** Java: UnsummonType.isCancelableByMaster() */
constexpr bool isCancelableByMaster(UnsummonType type) noexcept {
	return detail::UNSUMMON_CANCELABLE_BY_MASTER[static_cast<size_t>(type)];
}

} // namespace aion::gameserver::model::summons
