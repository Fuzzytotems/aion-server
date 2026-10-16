#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/templates/item/ItemQuality.h"

namespace aion::gameserver::model::templates::item {

/**
 * Companion of the generated enum ItemQuality (docs/design/static-data.md §2.5, pattern StorageTypeInfo.h): Java's constructor data and methods
 * as free functions found by ADL (`getQualityId(quality)` for Java `quality.getQualityId()`).
 *
 * @author ATracer
 */

namespace detail {
/** Java constructor argument `qualityId` in ordinal order */
inline constexpr std::array<int32_t, 7> ITEM_QUALITY_IDS{
	0, // JUNK - Gray
	1, // COMMON - White
	2, // RARE - Superior - Green
	3, // LEGEND - Heroic - Blue
	4, // UNIQUE - Fabled - Yellow
	5, // EPIC - Eternal - Orange
	6, // MYTHIC - Purple
};
static_assert(static_cast<size_t>(ItemQuality::MYTHIC) + 1 == ITEM_QUALITY_IDS.size(), "one entry per ItemQuality constant");
} // namespace detail

constexpr int32_t getQualityId(ItemQuality quality) noexcept {
	return detail::ITEM_QUALITY_IDS[static_cast<size_t>(quality)];
}

} // namespace aion::gameserver::model::templates::item
