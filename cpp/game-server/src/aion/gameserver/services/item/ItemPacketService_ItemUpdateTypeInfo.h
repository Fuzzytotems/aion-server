#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/services/item/ItemPacketService_ItemAddType.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemUpdateType.h"

namespace aion::gameserver::services::item {

/**
 * Companion of the generated nested enum ItemPacketService.ItemUpdateType (docs/design/static-data.md §2.5, m5b3-plan.md T-02): Java's
 * constructor data and its static method as free functions found by ADL (`getMask(type)` for Java `type.getMask()`,
 * `getKinahUpdateTypeFromAddType(addType, isIncrease)` for the static `ItemUpdateType.getKinahUpdateTypeFromAddType(addType, isIncrease)`).
 * <p>
 * The server packets keep their own stand-in of the constructor data (network/aion/serverpackets/detail/PacketSupport.h,
 * `itemUpdateTypeData`, P4-17) until that chunk adopts this header.
 */

namespace detail {
/** Java constructor arguments (mask, sendable) in ordinal order (ItemPacketService.java:28-53) */
struct ItemUpdateTypeData {
	int32_t mask;
	bool sendable;
};
inline constexpr std::array<ItemUpdateTypeData, 26> ITEM_UPDATE_TYPE_DATA{{{-1, false}, {-2, false}, {-3, false}, {0, true}, {0x01, true},
	{0x05, true}, {0x06, true}, {0x0A, true}, {0x13, true}, {0x16, true}, {0x17, true}, {0x19, true}, {0x1A, true}, {0x1C, true}, {0x1D, true},
	{0x20, true}, {0x23, true}, {0x25, true}, {0x32, true}, {0x49, true}, {0x4B, true}, {0x50, true}, {0x51, true}, {0x5A, true}, {0x5E, true},
	{0x8A, true}}};
static_assert(static_cast<size_t>(ItemPacketService_ItemUpdateType::INC_PASSPORT_ADD) + 1 == ITEM_UPDATE_TYPE_DATA.size(),
	"one entry per ItemUpdateType constant");
} // namespace detail

/** Java: ItemUpdateType.getMask() */
constexpr int32_t getMask(ItemPacketService_ItemUpdateType type) noexcept {
	return detail::ITEM_UPDATE_TYPE_DATA[static_cast<size_t>(type)].mask;
}

/** Java: ItemUpdateType.isSendable() */
constexpr bool isSendable(ItemPacketService_ItemUpdateType type) noexcept {
	return detail::ITEM_UPDATE_TYPE_DATA[static_cast<size_t>(type)].sendable;
}

/**
 * Java: ItemUpdateType.getKinahUpdateTypeFromAddType(itemAddType, isIncrease) (ItemPacketService.java:71-80): DEC_KINAH_BUY for a decrease;
 * for an increase BUY -> INC_KINAH_SELL, ITEM_COLLECT -> INC_KINAH_COLLECT, QUEST_WORK_ITEM -> INC_KINAH_QUEST, anything else INC_KINAH_MERGE.
 */
constexpr ItemPacketService_ItemUpdateType getKinahUpdateTypeFromAddType(ItemPacketService_ItemAddType itemAddType, bool isIncrease) noexcept {
	if (!isIncrease)
		return ItemPacketService_ItemUpdateType::DEC_KINAH_BUY;
	switch (itemAddType) {
		case ItemPacketService_ItemAddType::BUY:
			return ItemPacketService_ItemUpdateType::INC_KINAH_SELL;
		case ItemPacketService_ItemAddType::ITEM_COLLECT:
			return ItemPacketService_ItemUpdateType::INC_KINAH_COLLECT;
		case ItemPacketService_ItemAddType::QUEST_WORK_ITEM:
			return ItemPacketService_ItemUpdateType::INC_KINAH_QUEST;
		default:
			return ItemPacketService_ItemUpdateType::INC_KINAH_MERGE;
	}
}

} // namespace aion::gameserver::services::item
