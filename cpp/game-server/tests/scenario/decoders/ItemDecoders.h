#pragma once

// The item and loot packets the M5b-3 gate reads (m5b3-plan.md §2.7, G-02; the §10.3 rows Y2-Y12, Y16): SM_LOOT_ITEMLIST,
// SM_INVENTORY_ADD_ITEM, SM_INVENTORY_UPDATE_ITEM, SM_DELETE_ITEM, SM_CUBE_UPDATE, SM_WAREHOUSE_ADD_ITEM, SM_DELETE_WAREHOUSE_ITEM,
// SM_UPDATE_PLAYER_APPEARANCE and SM_ITEM_USAGE_ANIMATION - the nine of §2.7 - plus SM_WAREHOUSE_UPDATE_ITEM, the merge arm of a move into the
// warehouse (ItemPacketService.java:202), which L8 can reach. SM_LOOT_STATUS is decodeLootStatus in CombatDecoders.h.
//
// **m5a-plan.md D9:** every layout is written from the Java `writeImpl` under game-server/src/com/aionemu/gameserver/network/aion/serverpackets/
// and the helpers it calls (iteminfo/ItemInfoBlob and its entries, AbstractPlayerInfoPacket.writeEquippedItems), and the enum values from
// services/item/ItemPacketService.java (ItemUpdateType, ItemAddType, ItemDeleteType) and model/items/storage/StorageType.java. Nothing here
// includes, calls or mirrors a C++ serverpackets header. The item info blob is PacketDecoders.cpp's reader (readItemInfoBlob), the one
// SM_INVENTORY_INFO and SM_WAREHOUSE_INFO already use; a socketed godstone travels in its ENCHANT_INFO entry (godStoneId,
// EnchantInfoBlobEntry.java:51), so m5b3-plan.md §13 question 2 is answered by that reader as it stands.
//
// Every decode function consumes the body exactly and throws DecodeError otherwise; the Java constants that carry no data are verified.

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "decoders/PacketDecoders.h" // BodyReader, DecodeError, InventoryItem, EquippedItems, readItemInfoBlob

namespace aion::gameserver::scenario::decoders {

// ---- the enums the packets carry --------------------------------------------------------------------------------------------------------

/** ItemPacketService.ItemAddType.getMask() (ItemPacketService.java:83-101), the writeH of SM_INVENTORY_ADD_ITEM / SM_WAREHOUSE_ADD_ITEM */
constexpr uint16_t ITEM_ADD_SERVER_GENERATED = 0x00;
constexpr uint16_t ITEM_ADD_PARTIAL_WITH_SLOT = 0x07;
constexpr uint16_t ITEM_ADD_ALL_SLOT = 0x13;
constexpr uint16_t ITEM_ADD_ITEM_COLLECT = 0x19;
constexpr uint16_t ITEM_ADD_BUY = 0x1C;

/**
 * ItemPacketService.ItemUpdateType.getMask() (ItemPacketService.java:27-53), the trailing writeH of SM_INVENTORY_UPDATE_ITEM and
 * SM_WAREHOUSE_UPDATE_ITEM - written only when the type isSendable(): EQUIP_UNEQUIP (-1), CHARGE (-2) and POLISH_CHARGE (-3) are "internal
 * usage only" and leave the packet without it
 */
constexpr uint16_t ITEM_UPDATE_STATS_CHANGE = 0x00;
constexpr uint16_t ITEM_UPDATE_INC_ITEM_MERGE = 0x01;
constexpr uint16_t ITEM_UPDATE_INC_KINAH_MERGE = 0x05;
constexpr uint16_t ITEM_UPDATE_DEC_ITEM_SPLIT = 0x06;
constexpr uint16_t ITEM_UPDATE_DEC_ITEM_SPLIT_MOVE = 0x0A;
constexpr uint16_t ITEM_UPDATE_PUT = 0x13;
constexpr uint16_t ITEM_UPDATE_DEC_ITEM_USE = 0x16;
constexpr uint16_t ITEM_UPDATE_INC_ITEM_COLLECT = 0x19;
constexpr uint16_t ITEM_UPDATE_INC_KINAH_COLLECT = 0x1A;

/** ItemPacketService.ItemDeleteType.getMask() (ItemPacketService.java:114-125), the writeC of SM_DELETE_ITEM / SM_DELETE_WAREHOUSE_ITEM */
constexpr uint8_t ITEM_DELETE_DEFAULT = 0x00;
constexpr uint8_t ITEM_DELETE_SPLIT = 0x04;
constexpr uint8_t ITEM_DELETE_MOVE = 0x14;
constexpr uint8_t ITEM_DELETE_DISCARD = 0x15;
constexpr uint8_t ITEM_DELETE_USE = 0x17;

/**
 * StorageType (StorageType.java:7-10): getId() is what SM_WAREHOUSE_ADD_ITEM / _UPDATE_ITEM / SM_DELETE_WAREHOUSE_ITEM write
 * (ItemPacketService.java:182, 202, 225), ordinal() what SM_CUBE_UPDATE.cubeSize writes (SM_CUBE_UPDATE.java:53); the two agree for these four
 */
constexpr uint8_t STORAGE_CUBE = 0;
constexpr uint8_t STORAGE_REGULAR_WAREHOUSE = 1;
constexpr uint8_t STORAGE_ACCOUNT_WAREHOUSE = 2;
constexpr uint8_t STORAGE_LEGION_WAREHOUSE = 3;

// ---- SM_LOOT_ITEMLIST -------------------------------------------------------------------------------------------------------------------

/** one entry of SM_LOOT_ITEMLIST (SM_LOOT_ITEMLIST.java:45-56) */
struct LootItem {
	/** DropItem.getIndex(): registerDrop's running index from 1 (the client keys entries by it; the list order is a HashSet's, plan D10) */
	uint8_t index = 0;
	int32_t itemId = 0;
	/** (int) DropItem.getCount() */
	int32_t count = 0;
	/** DropItem.getOptionalSocket(): -1 for a template with option_slot_bonus, else 0 (DropItem.java:28-33) */
	int8_t optionalSocket = 0;
	bool showLootConfirmation = false;

	bool operator==(const LootItem&) const = default;
};

struct LootItemList {
	/** the corpse's object id */
	int32_t targetObjectId = 0;
	std::vector<LootItem> items;
};

LootItemList decodeLootItemList(std::span<const uint8_t> body);

// ---- SM_INVENTORY_ADD_ITEM, SM_INVENTORY_UPDATE_ITEM, SM_DELETE_ITEM ------------------------------------------------------------------------

/** SM_INVENTORY_ADD_ITEM (SM_INVENTORY_ADD_ITEM.java:30-55); each item like one of SM_INVENTORY_INFO (object id, template id, l10n, blob, slot, cloth) */
struct InventoryAddItem {
	/** ItemAddType.getMask(), PARTIAL_WITH_SLOT instead of ITEM_COLLECT for a single item with a slot (:33-37) */
	uint16_t addTypeMask = 0;
	std::vector<InventoryItem> items;
};

InventoryAddItem decodeInventoryAddItem(std::span<const uint8_t> body);

/**
 * SM_INVENTORY_UPDATE_ITEM (SM_INVENTORY_UPDATE_ITEM.java:32-60): no template id, no slot and no cloth byte. `item` carries the object id, the
 * l10n and the blob fields; its templateId, equipmentSlot and cloth stay at their defaults because the packet does not write them. The blob is
 * EQUIPPED_SLOT alone for EQUIP_UNEQUIP (:40-43), CONDITIONING_INFO / POLISH_INFO for the two charges, the full blob otherwise.
 */
struct InventoryUpdateItem {
	InventoryItem item;
	/** ItemUpdateType.getMask() when the type isSendable() (:58-59), nothing for the three internal types */
	std::optional<uint16_t> updateTypeMask;
};

InventoryUpdateItem decodeInventoryUpdateItem(std::span<const uint8_t> body);

/** SM_DELETE_ITEM (SM_DELETE_ITEM.java:24-27) */
struct DeleteItem {
	int32_t objectId = 0;
	/** ItemDeleteType.getMask() */
	uint8_t deleteTypeMask = 0;
};

DeleteItem decodeDeleteItem(std::span<const uint8_t> body);

// ---- SM_CUBE_UPDATE ---------------------------------------------------------------------------------------------------------------------

/** SM_CUBE_UPDATE (SM_CUBE_UPDATE.java:69-80): action 0 is cubeSize (:29-53), action 6 stigmaSlots (:25-27), which writes nothing more */
struct CubeUpdate {
	uint8_t action = 0;
	/** action 0: the StorageType ordinal; action 6: the advanced stigma count */
	uint8_t actionValue = 0;
	/**
	 * action 0 only (0 for the others): Storage.size(), i.e. ItemStorage.size() - the stacks the storage holds, the kinah NOT among them
	 * (Storage.add keeps the kinah item in a field of its own, Storage.java:172-173)
	 */
	int32_t itemsCount = 0;
	uint8_t npcExpands = 0, questExpands = 0, itemExpands = 0;
};

CubeUpdate decodeCubeUpdate(std::span<const uint8_t> body);

// ---- SM_WAREHOUSE_ADD_ITEM, SM_WAREHOUSE_UPDATE_ITEM, SM_DELETE_WAREHOUSE_ITEM ----------------------------------------------------------

/**
 * SM_WAREHOUSE_ADD_ITEM (SM_WAREHOUSE_ADD_ITEM.java:32-52); each item: object id, template id, a literal writeC(0) (verified), l10n, the full
 * blob and the slot - no cloth byte, unlike the cube's
 */
struct WarehouseAddItem {
	uint8_t warehouseType = 0;
	uint16_t addTypeMask = 0;
	std::vector<InventoryItem> items;
};

WarehouseAddItem decodeWarehouseAddItem(std::span<const uint8_t> body);

/** SM_WAREHOUSE_UPDATE_ITEM (SM_WAREHOUSE_UPDATE_ITEM.java:30-43): object id, warehouse type, l10n, a GENERAL_INFO-only blob, the mask */
struct WarehouseUpdateItem {
	uint8_t warehouseType = 0;
	/** object id, l10n and the blob fields, as InventoryUpdateItem::item */
	InventoryItem item;
	std::optional<uint16_t> updateTypeMask;
};

WarehouseUpdateItem decodeWarehouseUpdateItem(std::span<const uint8_t> body);

/** SM_DELETE_WAREHOUSE_ITEM (SM_DELETE_WAREHOUSE_ITEM.java:23-27) */
struct DeleteWarehouseItem {
	uint8_t warehouseType = 0;
	int32_t objectId = 0;
	uint8_t deleteTypeMask = 0;
};

DeleteWarehouseItem decodeDeleteWarehouseItem(std::span<const uint8_t> body);

// ---- SM_UPDATE_PLAYER_APPEARANCE --------------------------------------------------------------------------------------------------------

/** SM_UPDATE_PLAYER_APPEARANCE (SM_UPDATE_PLAYER_APPEARANCE.java:22-25): the player id and AbstractPlayerInfoPacket.writeEquippedItems */
struct UpdatePlayerAppearance {
	int32_t playerObjectId = 0;
	EquippedItems equipment;
};

UpdatePlayerAppearance decodeUpdatePlayerAppearance(std::span<const uint8_t> body);

// ---- SM_ITEM_USAGE_ANIMATION ------------------------------------------------------------------------------------------------------------

/** SM_ITEM_USAGE_ANIMATION (SM_ITEM_USAGE_ANIMATION.java:75-88), 28 bytes; field names are the Java ones */
struct ItemUsageAnimation {
	int32_t playerObjectId = 0;
	int32_t targetObjectId = 0;
	int32_t itemObjectId = 0;
	int32_t itemId = 0;
	/** the use time in ms (a socketing's 2000, ItemSocketService.java:191), 0 for its closing packet (end 1, :197) */
	int32_t time = 0;
	/** 0 for the opening animation of a timed use, 1 when it ends, 3 when it is aborted (ItemSocketService.java:184) */
	uint8_t end = 0;
	uint8_t unk = 0;
	uint8_t unk1 = 0;
	/** 1 unless the ten-argument constructor set it (the field initializer, :19) */
	uint8_t unk2 = 0;
	int32_t unk3 = 0;

	bool operator==(const ItemUsageAnimation&) const = default;
};

ItemUsageAnimation decodeItemUsageAnimation(std::span<const uint8_t> body);

} // namespace aion::gameserver::scenario::decoders
