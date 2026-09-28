#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/AttendType.h"

namespace aion::gameserver::model {

/** Companion of the generated enum AttendType (docs/design/static-data.md §2.5): Java's constructor data as constexpr free functions (ADL). */

namespace detail {
inline constexpr std::array<int32_t, 3> ATTEND_TYPE_IDS{{0, 1, 2}}; // DAILY, ANNIVERSARY, CUMULATIVE
static_assert(static_cast<size_t>(AttendType::CUMULATIVE) + 1 == ATTEND_TYPE_IDS.size(), "one entry per AttendType constant");
} // namespace detail

/** Java: AttendType.getId() */
constexpr int32_t getId(AttendType type) noexcept {
	return detail::ATTEND_TYPE_IDS[static_cast<size_t>(type)];
}

} // namespace aion::gameserver::model
