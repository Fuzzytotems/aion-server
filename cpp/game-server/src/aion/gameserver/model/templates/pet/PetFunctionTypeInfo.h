#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/templates/pet/PetFunctionType.h"

namespace aion::gameserver::model::templates::pet {

/**
 * Companion of the generated enum PetFunctionType (docs/design/static-data.md §2.5, pattern StorageTypeInfo.h): Java's constructor data and
 * methods as free functions found by ADL (`getId(type)` for Java `type.getId()`).
 *
 * @author IlBuono, Rolandas
 */

namespace detail {
/** Java constructor arguments (id as `(byte) id`, isPlayerFunc; the one-argument constructor passes true) in ordinal order */
struct PetFunctionTypeData {
	int8_t id;
	bool isPlayerFunc;
};

inline constexpr std::array<PetFunctionTypeData, 10> PET_FUNCTION_TYPE_DATA{{
	{0, true},   // WAREHOUSE
	{1, true},   // FOOD
	{2, true},   // DOPING
	{3, true},   // LOOT
	{4, true},   // BUFF
	{5, true},   // MERCHANT
	{6, true},   // NONE
	{1, false},  // APPEARANCE
	{-1, false}, // BAG (non writable to packets)
	{-2, false}, // WING (non writable to packets)
}};
static_assert(static_cast<size_t>(PetFunctionType::WING) + 1 == PET_FUNCTION_TYPE_DATA.size(), "one entry per PetFunctionType constant");
} // namespace detail

constexpr int8_t getId(PetFunctionType type) noexcept {
	return detail::PET_FUNCTION_TYPE_DATA[static_cast<size_t>(type)].id;
}

constexpr bool isPlayerFunction(PetFunctionType type) noexcept {
	return detail::PET_FUNCTION_TYPE_DATA[static_cast<size_t>(type)].isPlayerFunc;
}

} // namespace aion::gameserver::model::templates::pet
