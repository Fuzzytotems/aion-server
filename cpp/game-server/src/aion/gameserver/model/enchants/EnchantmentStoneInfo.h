#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "aion/gameserver/model/enchants/EnchantmentStone.h"
#include "aion/gameserver/model/templates/item/ItemQuality.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::enchants {

/**
 * Companion of the generated enum EnchantmentStone (docs/design/static-data.md §2.5): the Java constructor data and methods as free functions
 * found by ADL (`getBaseLevel(stone)` for Java `stone.getBaseLevel()`). Pure data.
 *
 * @author Neon
 */

namespace detail {
/** Java constructor arguments (baseLevel, baseQuality) in ordinal order */
struct EnchantmentStoneData {
	int32_t baseLevel;
	templates::item::ItemQuality baseQuality;
};

inline constexpr std::array<EnchantmentStoneData, 6> ENCHANTMENT_STONE_DATA{{
	{20, templates::item::ItemQuality::RARE},   // ALPHA
	{40, templates::item::ItemQuality::LEGEND}, // BETA
	{55, templates::item::ItemQuality::UNIQUE}, // GAMMA
	{60, templates::item::ItemQuality::EPIC},   // DELTA
	{65, templates::item::ItemQuality::MYTHIC}, // EPSILON
	{65, templates::item::ItemQuality::MYTHIC}, // OMEGA
}};
static_assert(static_cast<size_t>(EnchantmentStone::OMEGA) + 1 == ENCHANTMENT_STONE_DATA.size(), "one entry per EnchantmentStone constant");
} // namespace detail

constexpr int32_t getBaseLevel(EnchantmentStone stone) noexcept {
	return detail::ENCHANTMENT_STONE_DATA[static_cast<size_t>(stone)].baseLevel;
}

constexpr templates::item::ItemQuality getBaseQuality(EnchantmentStone stone) noexcept {
	return detail::ENCHANTMENT_STONE_DATA[static_cast<size_t>(stone)].baseQuality;
}

/**
 * Java EnchantmentStone.getByItemId(itemId).
 *
 * @throws IllegalArgumentException if the item is no enchantment stone
 */
inline EnchantmentStone getByItemId(int32_t itemId) {
	switch (itemId) {
		case 166000191:
			return EnchantmentStone::ALPHA;
		case 166000192:
			return EnchantmentStone::BETA;
		case 166000193:
			return EnchantmentStone::GAMMA;
		case 166000194:
			return EnchantmentStone::DELTA;
		case 166000195:
			return EnchantmentStone::EPSILON;
		case 166020000:
		case 166020001:
		case 166020002:
		case 166020003:
			return EnchantmentStone::OMEGA;
		default:
			if (itemId >= 166000001 && itemId <= 166000190) { // L1 - L190 (old stones)
				if (itemId > 166000100) {                     // 101+
					return EnchantmentStone::EPSILON;
				} else if (itemId > 166000060) { // 61-100
					return EnchantmentStone::DELTA;
				} else if (itemId > 166000050) { // 51-60
					return EnchantmentStone::GAMMA;
				} else if (itemId > 166000030) { // 31-50
					return EnchantmentStone::BETA;
				} else { // 1-30
					return EnchantmentStone::ALPHA;
				}
			}
			throw runtime::IllegalArgumentException("No matching enchantment stone found for item ID " + std::to_string(itemId));
	}
}

} // namespace aion::gameserver::model::enchants
