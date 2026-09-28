#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "aion/gameserver/model/templates/housing/HouseType.h"

namespace aion::gameserver::model::templates::housing {

/**
 * Companion of the generated enum HouseType (docs/design/static-data.md §2.5, pattern StorageTypeInfo.h): Java's constructor data and methods as
 * free functions found by ADL (`getId(type)` for Java `type.getId()`).
 *
 * @author Rolandas
 */

namespace detail {
/** Java constructor arguments (index, id, abbrev) in ordinal order */
struct HouseTypeData {
	int32_t limitTypeIndex;
	int32_t id;
	std::string_view abbrev; // building parts end with this letter (like CP_S for palace)
};

inline constexpr std::array<HouseTypeData, 5> HOUSE_TYPE_DATA{{
	{0, 3, "a"}, // ESTATE
	{1, 2, "b"}, // MANSION
	{2, 1, "c"}, // HOUSE
	{3, 0, "d"}, // STUDIO
	{4, 4, "s"}, // PALACE
}};
static_assert(static_cast<size_t>(HouseType::PALACE) + 1 == HOUSE_TYPE_DATA.size(), "one entry per HouseType constant");
} // namespace detail

constexpr int32_t getLimitTypeIndex(HouseType type) noexcept {
	return detail::HOUSE_TYPE_DATA[static_cast<size_t>(type)].limitTypeIndex;
}

constexpr int32_t getId(HouseType type) noexcept {
	return detail::HOUSE_TYPE_DATA[static_cast<size_t>(type)].id;
}

constexpr std::string_view getAbbreviation(HouseType type) noexcept {
	return detail::HOUSE_TYPE_DATA[static_cast<size_t>(type)].abbrev;
}

} // namespace aion::gameserver::model::templates::housing
