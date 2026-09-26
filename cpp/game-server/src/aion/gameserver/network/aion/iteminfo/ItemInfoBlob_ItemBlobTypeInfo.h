#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/network/aion/iteminfo/ItemInfoBlob_ItemBlobType.h"

namespace aion::gameserver::network::aion::iteminfo {

/**
 * Companion of the generated enum ItemInfoBlob_ItemBlobType (docs/design/static-data.md §2.5): Java's constructor data as free functions found
 * by ADL (`getEntryId(type)` for Java `type.getEntryId()`). The constant-specific `newBlobEntry()` bodies are declared in ItemInfoBlob.h.
 */

/** Java: ItemBlobType.getEntryId() (constructor arguments in ordinal order) */
constexpr int32_t getEntryId(ItemInfoBlob_ItemBlobType type) noexcept {
	static constexpr std::array<int32_t, 18> ENTRY_IDS{
		0x00, // GENERAL_INFO
		0x01, // SLOTS_WEAPON
		0x02, // SLOTS_ARMOR
		0x03, // SLOTS_SHIELD
		0x04, // SLOTS_ACCESSORY
		0x05, // SLOTS_ARROW
		0x06, // EQUIPPED_SLOT
		0x07, // STIGMA_INFO
		0x08, // STIGMA_SHARD
		0x10, // PREMIUM_OPTION
		0x11, // POLISH_INFO
		0x12, // WRAP_INFO
		0x13, // PLUME_INFO
		0x0A, // STAT_BONUSES
		0x0B, // ENCHANT_INFO
		0x0D, // SLOTS_WING
		0x0E, // COMPOSITE_ITEM
		0x0F, // CONDITIONING_INFO
	};
	return ENTRY_IDS[static_cast<size_t>(type)];
}

} // namespace aion::gameserver::network::aion::iteminfo
