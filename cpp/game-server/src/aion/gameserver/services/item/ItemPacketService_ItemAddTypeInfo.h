#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/services/item/ItemPacketService_ItemAddType.h"

namespace aion::gameserver::services::item {

/**
 * Companion of the generated nested enum ItemPacketService.ItemAddType (docs/design/static-data.md §2.5, m5b3-plan.md T-02): Java's
 * constructor data as a free function found by ADL (`getMask(type)` for Java `type.getMask()`). The server packets keep their own stand-in
 * (network/aion/serverpackets/detail/PacketSupport.h, `itemAddTypeMask`, P4-17) until that chunk adopts this header.
 */

namespace detail {
/** Java constructor argument `mask` in ordinal order (ItemPacketService.java:84-101); MAIL shares QUEST_WORK_ITEM2's 0x36 */
inline constexpr std::array<int32_t, 18> ITEM_ADD_TYPE_MASKS{
	{0x00, 0x07, 0x13, 0x19, 0x1C, 0x21, 0x23, 0x2B, 0x2D, 0x2E, 0x2F, 0x30, 0x35, 0x36, 0x36, 0x40, 0x50, 0x51}};
static_assert(static_cast<size_t>(ItemPacketService_ItemAddType::REPURCHASE) + 1 == ITEM_ADD_TYPE_MASKS.size(),
	"one entry per ItemAddType constant");
} // namespace detail

/** Java: ItemAddType.getMask() */
constexpr int32_t getMask(ItemPacketService_ItemAddType type) noexcept {
	return detail::ITEM_ADD_TYPE_MASKS[static_cast<size_t>(type)];
}

} // namespace aion::gameserver::services::item
