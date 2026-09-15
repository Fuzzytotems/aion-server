#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "aion/gameserver/model/templates/detail/EnumValueOf.h"
#include "aion/gameserver/model/templates/housing/HouseTypeInfo.h"
#include "aion/gameserver/model/templates/housing/LimitType.h"

namespace aion::gameserver::model::templates::housing {

/**
 * Companion of the generated enum LimitType (docs/design/static-data.md §2.5, pattern StorageTypeInfo.h): Java's constructor data and methods as
 * free functions found by ADL (`getObjectPlaceLimit(type, houseType)` for Java `type.getObjectPlaceLimit(houseType)`). The static
 * `LimitType.fromValue(value)` is `housing::fromValue<LimitType>(value)`, since other enums of the package declare a fromValue too.
 *
 * @author Rolandas
 */

namespace detail {
/** Java constructor arguments (id, personal limits, trial limits); limits are in the order of house type: a, b, c, d, s */
struct LimitTypeData {
	int32_t id;
	std::array<int32_t, 5> personalLimits;
	std::array<int32_t, 5> trialLimits;
};

inline constexpr std::array<LimitTypeData, 8> LIMIT_TYPE_DATA{{
	{0, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}}, // NONE
	{1, {6, 4, 3, 8, 8}, {0, 0, 0, 4, 0}}, // OWNER_POT
	{2, {7, 5, 2, 8, 9}, {0, 0, 0, 4, 0}}, // VISITOR_POT
	{3, {6, 5, 4, 8, 7}, {0, 0, 0, 4, 0}}, // STORAGE
	{4, {6, 5, 4, 3, 7}, {6, 5, 4, 1, 7}}, // POT
	{5, {1, 1, 1, 1, 1}, {1, 1, 1, 1, 1}}, // COOKING
	{6, {1, 1, 1, 1, 1}, {1, 1, 1, 0, 1}}, // PICTURE
	{7, {1, 1, 1, 1, 1}, {1, 1, 1, 0, 1}}, // JUKEBOX
}};
static_assert(static_cast<size_t>(LimitType::JUKEBOX) + 1 == LIMIT_TYPE_DATA.size(), "one entry per LimitType constant");
} // namespace detail

/** Java value(): name() */
constexpr std::string_view value(LimitType type) noexcept {
	return xml::enumName(type);
}

constexpr int32_t getId(LimitType type) noexcept {
	return detail::LIMIT_TYPE_DATA[static_cast<size_t>(type)].id;
}

constexpr int32_t getObjectPlaceLimit(LimitType type, HouseType houseType) noexcept {
	return detail::LIMIT_TYPE_DATA[static_cast<size_t>(type)].personalLimits[static_cast<size_t>(getLimitTypeIndex(houseType))];
}

constexpr int32_t getTrialObjectPlaceLimit(LimitType type, HouseType houseType) noexcept {
	return detail::LIMIT_TYPE_DATA[static_cast<size_t>(type)].trialLimits[static_cast<size_t>(getLimitTypeIndex(houseType))];
}

/** The static fromValue(String) of the housing enums (specialized per enum). @throws IllegalArgumentException for an unknown name */
template <class E>
E fromValue(std::string_view value);

/** Java static LimitType.fromValue(value): valueOf(value) */
template <>
inline LimitType fromValue<LimitType>(std::string_view value) {
	return templates::detail::enumValueOf<LimitType>(value, "com.aionemu.gameserver.model.templates.housing.LimitType");
}

} // namespace aion::gameserver::model::templates::housing
