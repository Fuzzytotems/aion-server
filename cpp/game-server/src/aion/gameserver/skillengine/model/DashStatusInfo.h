#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/skillengine/model/DashStatus.h"

namespace aion::gameserver::skillengine::model {

/** Companion of the generated enum DashStatus (docs/design/static-data.md §2.5): Java's constructor data (ADL). */

namespace detail {
/** Java: the id constructor argument, in ordinal order (DashStatus.java:6-12) */
inline constexpr std::array<int32_t, 6> DASH_STATUS_IDS{{
	0, // NONE
	1, // RANDOMMOVELOC
	2, // DASH
	3, // BACKDASH
	4, // MOVEBEHIND
	6, // RANDOMMOVELOC_NEW
}};
static_assert(static_cast<size_t>(DashStatus::RANDOMMOVELOC_NEW) + 1 == DASH_STATUS_IDS.size(), "one entry per DashStatus constant");
} // namespace detail

/** Java: DashStatus.getId() */
constexpr int32_t getId(DashStatus dashStatus) noexcept {
	return detail::DASH_STATUS_IDS[static_cast<size_t>(dashStatus)];
}

} // namespace aion::gameserver::skillengine::model
