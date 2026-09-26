#pragma once

#include <array>
#include <cstddef>
#include <optional>

#include "aion/gameserver/model/templates/item/enums/ArmorType.h"
#include "aion/gameserver/model/templates/item/enums/EquipType.h"
#include "aion/gameserver/model/templates/item/enums/ItemSubType.h"

namespace aion::gameserver::model::templates::item::enums {

/**
 * Companion of the generated enum ItemSubType (docs/design/static-data.md §2.5, pattern StorageTypeInfo.h): Java's constructor data and methods
 * as free functions found by ADL (`getArmorType(subType)` for Java `subType.getArmorType()`).
 *
 * @author xTz
 */

namespace detail {
/** Java constructor data: `ItemSubType(ArmorType)` sets equipType ARMOR, `ItemSubType(EquipType)` leaves armorType null */
struct ItemSubTypeData {
	std::optional<ArmorType> armorType;
	EquipType equipType;
};

inline constexpr std::array<ItemSubTypeData, 14> ITEM_SUB_TYPE_DATA{{
	{ArmorType::GENERAL, EquipType::ARMOR}, // ALL_ARMOR
	{std::nullopt, EquipType::NONE},        // NONE
	{ArmorType::GENERAL, EquipType::ARMOR}, // CHAIN
	{ArmorType::GENERAL, EquipType::ARMOR}, // CLOTHES
	{ArmorType::GENERAL, EquipType::ARMOR}, // LEATHER
	{ArmorType::GENERAL, EquipType::ARMOR}, // PLATE
	{ArmorType::GENERAL, EquipType::ARMOR}, // ROBE
	{ArmorType::GENERAL, EquipType::ARMOR}, // SHIELD
	{std::nullopt, EquipType::NONE},        // ARROW
	{ArmorType::GENERAL, EquipType::ARMOR}, // WING
	{std::nullopt, EquipType::WEAPON},      // ONE_HAND
	{std::nullopt, EquipType::WEAPON},      // TWO_HAND
	{std::nullopt, EquipType::STIGMA},      // STIGMA
	{std::nullopt, EquipType::PLUME},       // PLUME
}};
static_assert(static_cast<size_t>(ItemSubType::PLUME) + 1 == ITEM_SUB_TYPE_DATA.size(), "one entry per ItemSubType constant");
} // namespace detail

/** @return the armor type, std::nullopt (Java null) for the subtypes created with an EquipType */
constexpr std::optional<ArmorType> getArmorType(ItemSubType subType) noexcept {
	return detail::ITEM_SUB_TYPE_DATA[static_cast<size_t>(subType)].armorType;
}

/** Java protected getEquipType() (used by ItemGroup.getEquipType) */
constexpr EquipType getEquipType(ItemSubType subType) noexcept {
	return detail::ITEM_SUB_TYPE_DATA[static_cast<size_t>(subType)].equipType;
}

} // namespace aion::gameserver::model::templates::item::enums
