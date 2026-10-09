#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/team/legion/LegionRank.h"

namespace aion::gameserver::model::team::legion {

/** Companion of the generated enum LegionRank (docs/design/static-data.md §2.5): Java's constructor data and methods (ADL). */

namespace detail {
/** Java: the rank constructor argument, stored as a byte, in ordinal order (LegionRank.java:9-13) */
inline constexpr std::array<int8_t, 5> LEGION_RANK_IDS{{
	0, // BRIGADE_GENERAL
	1, // DEPUTY
	2, // CENTURION
	3, // LEGIONARY
	4, // VOLUNTEER
}};
static_assert(static_cast<size_t>(LegionRank::VOLUNTEER) + 1 == LEGION_RANK_IDS.size(), "one entry per LegionRank constant");
} // namespace detail

/** Java: LegionRank.getRankId() - the client-side id */
constexpr int8_t getRankId(LegionRank rank) noexcept {
	return detail::LEGION_RANK_IDS[static_cast<size_t>(rank)];
}

} // namespace aion::gameserver::model::team::legion
