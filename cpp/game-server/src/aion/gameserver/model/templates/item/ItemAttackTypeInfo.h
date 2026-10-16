#pragma once

#include <array>
#include <cstddef>

#include "aion/gameserver/model/SkillElement.h"
#include "aion/gameserver/model/templates/item/ItemAttackType.h"

namespace aion::gameserver::model::templates::item {

/**
 * Companion of the generated enum ItemAttackType (docs/design/static-data.md §2.5, pattern StorageTypeInfo.h): Java's constructor data and
 * methods as free functions found by ADL (`isMagical(type)` for Java `type.isMagical()`).
 *
 * @author ATracer
 */

namespace detail {
/** Java constructor arguments (magic, elem) in ordinal order */
struct ItemAttackTypeData {
	bool magic;
	SkillElement elem;
};

inline constexpr std::array<ItemAttackTypeData, 5> ITEM_ATTACK_TYPE_DATA{{
	{false, SkillElement::NONE}, // PHYSICAL
	{true, SkillElement::EARTH}, // MAGICAL_EARTH
	{true, SkillElement::WATER}, // MAGICAL_WATER
	{true, SkillElement::WIND},  // MAGICAL_WIND
	{true, SkillElement::FIRE},  // MAGICAL_FIRE
}};
static_assert(static_cast<size_t>(ItemAttackType::MAGICAL_FIRE) + 1 == ITEM_ATTACK_TYPE_DATA.size(), "one entry per ItemAttackType constant");
} // namespace detail

constexpr bool isMagical(ItemAttackType type) noexcept {
	return detail::ITEM_ATTACK_TYPE_DATA[static_cast<size_t>(type)].magic;
}

constexpr SkillElement getMagicalElement(ItemAttackType type) noexcept {
	return detail::ITEM_ATTACK_TYPE_DATA[static_cast<size_t>(type)].elem;
}

} // namespace aion::gameserver::model::templates::item
