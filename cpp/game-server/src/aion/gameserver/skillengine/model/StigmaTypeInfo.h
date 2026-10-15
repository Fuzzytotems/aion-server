#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/skillengine/model/StigmaType.h"

namespace aion::gameserver::skillengine::model {

/** Companion of the generated enum StigmaType (docs/design/static-data.md §2.5): Java's constructor data (ADL). */

namespace detail {
/** Java: the id constructor argument, in ordinal order (StigmaType.java:7-11) */
inline constexpr std::array<int32_t, 3> STIGMA_TYPE_IDS{{
	0, // NONE
	1, // BASIC
	2, // ADVANCED
}};
static_assert(static_cast<size_t>(StigmaType::ADVANCED) + 1 == STIGMA_TYPE_IDS.size(), "one entry per StigmaType constant");
} // namespace detail

/** Java: StigmaType.getId() */
constexpr int32_t getId(StigmaType stigmaType) noexcept {
	return detail::STIGMA_TYPE_IDS[static_cast<size_t>(stigmaType)];
}

} // namespace aion::gameserver::skillengine::model
