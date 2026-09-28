#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemDeleteType.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemUpdateType.h"

namespace aion::gameserver::services::item {

/**
 * Companion of the generated nested enum ItemPacketService.ItemDeleteType (docs/design/static-data.md §2.5, m5b3-plan.md T-02): Java's
 * constructor data and its two static methods as free functions found by ADL (`getMask(type)` for Java `type.getMask()`,
 * `fromUpdateType(updateType)` and `fromQuestStatus(questStatus)` for the static `ItemDeleteType.fromUpdateType` / `fromQuestStatus`).
 * <p>
 * Stand-ins that predate this header and belong to other chunks: `itemDeleteTypeMask` of network/aion/serverpackets/detail/PacketSupport.h
 * (P4-17) and the file-local `deleteTypeFromUpdateType` / `deleteTypeFromQuestStatus` of model/items/storage/Storage.cpp (P4-13).
 */

namespace detail {
/** Java constructor argument `mask` in ordinal order (ItemPacketService.java:115-125) */
inline constexpr std::array<int32_t, 11> ITEM_DELETE_TYPE_MASKS{{0x00, 0x04, 0x14, 0x15, 0x17, 0x1F, 0x31, 0x34, 0x66, 0x78, 0x26}};
static_assert(static_cast<size_t>(ItemPacketService_ItemDeleteType::PUT_TO_EXCHANGE) + 1 == ITEM_DELETE_TYPE_MASKS.size(),
	"one entry per ItemDeleteType constant");
} // namespace detail

/** Java: ItemDeleteType.getMask() */
constexpr int32_t getMask(ItemPacketService_ItemDeleteType type) noexcept {
	return detail::ITEM_DELETE_TYPE_MASKS[static_cast<size_t>(type)];
}

/** Java: ItemDeleteType.fromUpdateType(updateType) (ItemPacketService.java:137-144): DEC_ITEM_SPLIT -> SPLIT, DEC_ITEM_USE -> USE,
 *  DEC_ITEM_SPLIT_MOVE -> MOVE, anything else DEFAULT */
constexpr ItemPacketService_ItemDeleteType fromUpdateType(ItemPacketService_ItemUpdateType updateType) noexcept {
	switch (updateType) {
		case ItemPacketService_ItemUpdateType::DEC_ITEM_SPLIT:
			return ItemPacketService_ItemDeleteType::SPLIT;
		case ItemPacketService_ItemUpdateType::DEC_ITEM_USE:
			return ItemPacketService_ItemDeleteType::USE;
		case ItemPacketService_ItemUpdateType::DEC_ITEM_SPLIT_MOVE:
			return ItemPacketService_ItemDeleteType::MOVE;
		default:
			return ItemPacketService_ItemDeleteType::DEFAULT;
	}
}

/** Java: ItemDeleteType.fromQuestStatus(questStatus) (ItemPacketService.java:146-152): START -> QUEST_START, COMPLETE -> QUEST_COMPLETE,
 *  anything else DEFAULT */
constexpr ItemPacketService_ItemDeleteType fromQuestStatus(questEngine::model::QuestStatus questStatus) noexcept {
	switch (questStatus) {
		case questEngine::model::QuestStatus::START:
			return ItemPacketService_ItemDeleteType::QUEST_START;
		case questEngine::model::QuestStatus::COMPLETE:
			return ItemPacketService_ItemDeleteType::QUEST_COMPLETE;
		default:
			return ItemPacketService_ItemDeleteType::DEFAULT;
	}
}

} // namespace aion::gameserver::services::item
