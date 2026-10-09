#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/team/legion/LegionEmblemType.h"

namespace aion::gameserver::model::team::legion {

/** Companion of the generated enum LegionEmblemType (docs/design/static-data.md §2.5): Java's constructor data and methods (ADL). */

namespace detail {
/** Java: the value constructor argument, stored as a byte, in ordinal order (LegionEmblemType.java:7-8): CUSTOM's 0x80 is the byte -128 */
inline constexpr std::array<int8_t, 2> LEGION_EMBLEM_TYPE_VALUES{{
	static_cast<int8_t>(0x00), // DEFAULT
	static_cast<int8_t>(0x80), // CUSTOM
}};
static_assert(static_cast<size_t>(LegionEmblemType::CUSTOM) + 1 == LEGION_EMBLEM_TYPE_VALUES.size(), "one entry per LegionEmblemType constant");
} // namespace detail

/** Java: LegionEmblemType.getValue() */
constexpr int8_t getValue(LegionEmblemType type) noexcept {
	return detail::LEGION_EMBLEM_TYPE_VALUES[static_cast<size_t>(type)];
}

} // namespace aion::gameserver::model::team::legion
