#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/team/legion/LegionPermissionsMask.h"

namespace aion::gameserver::model::team::legion {

/** Companion of the generated enum LegionPermissionsMask (docs/design/static-data.md §2.5): Java's constructor data and methods (ADL). */

namespace detail {
/** Java: the rank constructor argument (the permission bit), in ordinal order (LegionPermissionsMask.java:8-14) */
inline constexpr std::array<int32_t, 7> LEGION_PERMISSIONS_MASK_RANKS{{
	0x200,  // EDIT
	0x8,    // INVITE
	0x10,   // KICK
	0x4,    // WH_WITHDRAWAL
	0x1000, // WH_DEPOSIT
	0x400,  // ARTIFACT
	0x800,  // GUARDIAN_STONE
}};
static_assert(static_cast<size_t>(LegionPermissionsMask::GUARDIAN_STONE) + 1 == LEGION_PERMISSIONS_MASK_RANKS.size(),
	"one entry per LegionPermissionsMask constant");
} // namespace detail

/** Java: LegionPermissionsMask.can(int permission) - (rank & permission) != 0 */
constexpr bool can(LegionPermissionsMask mask, int32_t permission) noexcept {
	return (detail::LEGION_PERMISSIONS_MASK_RANKS[static_cast<size_t>(mask)] & permission) != 0;
}

} // namespace aion::gameserver::model::team::legion
