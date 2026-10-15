#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/templates/tradelist/TradeNpcType.h"

namespace aion::gameserver::model::templates::tradelist {

/**
 * Companion of the generated enum TradeNpcType (docs/design/static-data.md §2.5, pattern StorageTypeInfo.h): Java's constructor data and methods
 * as free functions found by ADL (`index(type)` for Java `type.index()`).
 *
 * @author namedrisk
 */

namespace detail {
/** Java constructor argument `index` in ordinal order */
inline constexpr std::array<int32_t, 5> TRADE_NPC_TYPE_INDEXES{
	1, // NORMAL
	2, // ABYSS
	3, // LEGION_COIN
	4, // REWARD
	5, // ABYSS_KINAH (General Shop)
};
static_assert(static_cast<size_t>(TradeNpcType::ABYSS_KINAH) + 1 == TRADE_NPC_TYPE_INDEXES.size(), "one entry per TradeNpcType constant");
} // namespace detail

constexpr int32_t index(TradeNpcType type) noexcept {
	return detail::TRADE_NPC_TYPE_INDEXES[static_cast<size_t>(type)];
}

} // namespace aion::gameserver::model::templates::tradelist
