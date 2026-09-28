#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/templates/item/RandomType.h"

namespace aion::gameserver::model::templates::item {

/**
 * Companion of the generated enum RandomType (docs/design/static-data.md §2.5, pattern StorageTypeInfo.h): Java's constructor data and methods as
 * free functions found by ADL (`getLevel(type)` for Java `type.getLevel()`).
 *
 * @author vlog
 */

namespace detail {
/** Java constructor argument `level` in ordinal order (0 for the constants without one) */
inline constexpr std::array<int32_t, 42> RANDOM_TYPE_LEVELS{
	0,  // ENCHANTMENT
	0,  // MANASTONE
	10, // MANASTONE_COMMON_GRADE_10
	20, // MANASTONE_COMMON_GRADE_20
	30, // MANASTONE_COMMON_GRADE_30
	40, // MANASTONE_COMMON_GRADE_40
	50, // MANASTONE_COMMON_GRADE_50
	60, // MANASTONE_COMMON_GRADE_60
	70, // MANASTONE_COMMON_GRADE_70
	10, // MANASTONE_RARE_GRADE_10
	20, // MANASTONE_RARE_GRADE_20
	30, // MANASTONE_RARE_GRADE_30
	40, // MANASTONE_RARE_GRADE_40
	50, // MANASTONE_RARE_GRADE_50
	60, // MANASTONE_RARE_GRADE_60
	70, // MANASTONE_RARE_GRADE_70
	10, // MANASTONE_LEGEND_GRADE_10
	20, // MANASTONE_LEGEND_GRADE_20
	30, // MANASTONE_LEGEND_GRADE_30
	40, // MANASTONE_LEGEND_GRADE_40
	50, // MANASTONE_LEGEND_GRADE_50
	60, // MANASTONE_LEGEND_GRADE_60
	70, // MANASTONE_LEGEND_GRADE_70
	70, // SPECIAL_MANASTONE_RARE_GRADE
	70, // SPECIAL_MANASTONE_LEGEND_GRADE
	70, // SPECIAL_MANASTONE_UNIQUE_GRADE
	70, // SPECIAL_MANASTONE_EPIC_GRADE
	0,  // ANCIENTITEMS
	0,  // ANCIENT_CROWN
	0,  // ANCIENT_GOBLET
	0,  // ANCIENT_SEAL
	0,  // ANCIENT_ICON
	0,  // CHUNK_EARTH
	0,  // CHUNK_ROCK
	0,  // CHUNK_SAND
	0,  // CHUNK_GEMSTONE
	0,  // SCROLLS
	0,  // POTION
	0,  // LESSER_POTIONS
	0,  // POTION_50
	0,  // ILLUSION_GODSTONE
	0,  // PREMIUM_OPHIDAN_RECIPE
};
static_assert(static_cast<size_t>(RandomType::PREMIUM_OPHIDAN_RECIPE) + 1 == RANDOM_TYPE_LEVELS.size(), "one entry per RandomType constant");
} // namespace detail

constexpr int32_t getLevel(RandomType type) noexcept {
	return detail::RANDOM_TYPE_LEVELS[static_cast<size_t>(type)];
}

} // namespace aion::gameserver::model::templates::item
