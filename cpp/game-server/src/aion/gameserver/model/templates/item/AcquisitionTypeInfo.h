#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/templates/item/AcquisitionType.h"

namespace aion::gameserver::model::templates::item {

/**
 * Companion of the generated enum AcquisitionType (docs/design/static-data.md §2.5, pattern StorageTypeInfo.h): Java's constructor data and
 * methods as free functions found by ADL (`getId(type)` for Java `type.getId()`).
 *
 * @author Rolandas
 */

namespace detail {
/** Java constructor argument `id` in ordinal order */
inline constexpr std::array<int32_t, 4> ACQUISITION_TYPE_IDS{
	0, // AP
	1, // ABYSS
	2, // REWARD (the same as COUPON now)
	2, // COUPON
};
static_assert(static_cast<size_t>(AcquisitionType::COUPON) + 1 == ACQUISITION_TYPE_IDS.size(), "one entry per AcquisitionType constant");
} // namespace detail

constexpr int32_t getId(AcquisitionType type) noexcept {
	return detail::ACQUISITION_TYPE_IDS[static_cast<size_t>(type)];
}

} // namespace aion::gameserver::model::templates::item
