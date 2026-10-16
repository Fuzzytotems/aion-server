#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/templates/housing/BuildingType.h"

namespace aion::gameserver::model::templates::housing {

/**
 * Companion of the generated enum BuildingType (docs/design/static-data.md §2.5, pattern StorageTypeInfo.h): Java's constructor data and methods
 * as free functions found by ADL (`getId(type)` for Java `type.getId()`).
 *
 * @author Rolandas
 */

namespace detail {
/** Java constructor argument `id` in ordinal order */
inline constexpr std::array<int32_t, 2> BUILDING_TYPE_IDS{
	2, // PERSONAL_FIELD
	1, // PERSONAL_INS
};
static_assert(static_cast<size_t>(BuildingType::PERSONAL_INS) + 1 == BUILDING_TYPE_IDS.size(), "one entry per BuildingType constant");
} // namespace detail

constexpr int32_t getId(BuildingType type) noexcept {
	return detail::BUILDING_TYPE_IDS[static_cast<size_t>(type)];
}

} // namespace aion::gameserver::model::templates::housing
