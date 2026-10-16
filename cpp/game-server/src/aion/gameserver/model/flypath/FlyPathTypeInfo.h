#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/flypath/FlyPathType.h"

namespace aion::gameserver::model::flypath {

/**
 * Companion of the generated enum FlyPathType (static-data.md §2.5, InstanceProgressionTypeInfo.h's pattern): the constructor data and
 * getId() as a free function found by ADL.
 *
 * @author xTz
 */

namespace detail {
/** Java constructor argument `id` in ordinal order: GEYSER(0), ONE_WAY(1), TWO_WAY(2) */
inline constexpr std::array<int32_t, 3> FLYPATHTYPE_IDS{0, 1, 2};
static_assert(static_cast<size_t>(FlyPathType::TWO_WAY) + 1 == FLYPATHTYPE_IDS.size(), "one entry per constant");
} // namespace detail

/** Java: getId() */
constexpr int32_t getId(FlyPathType value) noexcept {
	return detail::FLYPATHTYPE_IDS[static_cast<size_t>(value)];
}

} // namespace aion::gameserver::model::flypath
