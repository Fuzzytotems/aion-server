#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/skillengine/task/AbstractCraftTask_CraftType.h"

namespace aion::gameserver::skillengine::task {

/** Companion of the generated nested enum AbstractCraftTask.CraftType (docs/design/static-data.md §2.5): Java's constructor data (ADL). */

namespace detail {
/** Java: the progressId constructor argument, in ordinal order */
inline constexpr std::array<int32_t, 3> CRAFT_TYPE_PROGRESS_IDS{{
	1, // NORMAL
	2, // CRIT_BLUE
	3, // CRIT_PURPLE
}};
static_assert(static_cast<size_t>(AbstractCraftTask_CraftType::CRIT_PURPLE) + 1 == CRAFT_TYPE_PROGRESS_IDS.size(),
	"one entry per CraftType constant");
} // namespace detail

/** Java: CraftType.getProgressId() */
constexpr int32_t getProgressId(AbstractCraftTask_CraftType craftType) noexcept {
	return detail::CRAFT_TYPE_PROGRESS_IDS[static_cast<size_t>(craftType)];
}

} // namespace aion::gameserver::skillengine::task
