#include "decoders/ItemDecoders.h"

#include <string>
#include <string_view>
#include <utility>

namespace aion::gameserver::scenario::decoders {

namespace {

/** reads a byte and fails unless it is 0 or 1 (a Java `flag ? 1 : 0`) */
bool readFlag(BodyReader& reader, std::string_view what) {
	const uint8_t value = reader.C();
	if (value > 1)
		reader.fail(std::string(what) + ": expected 0 or 1, got " + std::to_string(value));
	return value == 1;
}

/**
 * The trailing `if (updateType.isSendable()) writeH(updateType.getMask())` of the two update packets: the blob before it declares its own size,
 * so what is left is either nothing (an internal type) or exactly the short.
 */
std::optional<uint16_t> readOptionalUpdateMask(BodyReader& reader) {
	if (reader.remaining() == 0)
		return std::nullopt;
	if (reader.remaining() != 2)
		reader.fail(std::to_string(reader.remaining()) + " bytes after the item info blob, where only the update type short or nothing may follow");
	return reader.H();
}

} // namespace

// ---- SM_LOOT_ITEMLIST -------------------------------------------------------------------------------------------------------------------

LootItemList decodeLootItemList(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_LOOT_ITEMLIST");
	LootItemList list;
	list.targetObjectId = reader.D();  // SM_LOOT_ITEMLIST.java:41
	const uint8_t count = reader.C(); // :42, dropItems.size()
	for (uint8_t i = 0; i < count; i++) {
		LootItem item;
		item.index = reader.C();        // :46
		item.itemId = reader.D();       // :47
		item.count = reader.D();        // :48, (int) getCount()
		item.optionalSocket = reader.Cs(); // :49
		reader.expectC(0, "the byte after the optional socket"); // :50
		reader.expectC(0, "the 3.5 byte");                      // :51
		item.showLootConfirmation = readFlag(reader, "showLootConfirmation"); // :56
		list.items.push_back(item);
	}
	reader.expectFullyConsumed();
	return list;
}

// ---- SM_INVENTORY_ADD_ITEM, SM_INVENTORY_UPDATE_ITEM, SM_DELETE_ITEM ------------------------------------------------------------------------

InventoryAddItem decodeInventoryAddItem(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_INVENTORY_ADD_ITEM");
	InventoryAddItem add;
	add.addTypeMask = reader.H();     // SM_INVENTORY_ADD_ITEM.java:38
	const uint16_t count = reader.H(); // :39
	for (uint16_t i = 0; i < count; i++) {
		InventoryItem item;
		item.objectId = reader.D();   // :47
		item.templateId = reader.D(); // :48
		item.l10n = reader.S();       // :49
		readItemInfoBlob(reader, item); // :51, ItemInfoBlob.getFullBlob
		item.equipmentSlot = reader.H(); // :53
		item.cloth = readFlag(reader, "the cloth flag"); // :54
		add.items.push_back(std::move(item));
	}
	reader.expectFullyConsumed();
	return add;
}

InventoryUpdateItem decodeInventoryUpdateItem(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_INVENTORY_UPDATE_ITEM");
	InventoryUpdateItem update;
	update.item.objectId = reader.D(); // SM_INVENTORY_UPDATE_ITEM.java:35
	update.item.l10n = reader.S();     // :36
	readItemInfoBlob(reader, update.item); // :38-56
	update.updateTypeMask = readOptionalUpdateMask(reader); // :58-59
	reader.expectFullyConsumed();
	return update;
}

DeleteItem decodeDeleteItem(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_DELETE_ITEM");
	DeleteItem deleted;
	deleted.objectId = reader.D();       // SM_DELETE_ITEM.java:25
	deleted.deleteTypeMask = reader.C(); // :26
	reader.expectFullyConsumed();
	return deleted;
}

// ---- SM_CUBE_UPDATE ---------------------------------------------------------------------------------------------------------------------

CubeUpdate decodeCubeUpdate(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_CUBE_UPDATE");
	CubeUpdate update;
	update.action = reader.C();      // SM_CUBE_UPDATE.java:70
	update.actionValue = reader.C(); // :71
	if (update.action == 0) {        // :72-78, the one arm that writes more
		update.itemsCount = reader.D();
		update.npcExpands = reader.C();
		update.questExpands = reader.C();
		update.itemExpands = reader.C();
	}
	reader.expectFullyConsumed();
	return update;
}

// ---- SM_WAREHOUSE_ADD_ITEM, SM_WAREHOUSE_UPDATE_ITEM, SM_DELETE_WAREHOUSE_ITEM ----------------------------------------------------------

WarehouseAddItem decodeWarehouseAddItem(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_WAREHOUSE_ADD_ITEM");
	WarehouseAddItem add;
	add.warehouseType = reader.C();   // SM_WAREHOUSE_ADD_ITEM.java:33
	add.addTypeMask = reader.H();     // :34
	const uint16_t count = reader.H(); // :35, always 1 (Collections.singletonList)
	for (uint16_t i = 0; i < count; i++) {
		InventoryItem item;
		item.objectId = reader.D();   // :44
		item.templateId = reader.D(); // :45
		reader.expectC(0, "the item info byte"); // :46, a literal writeC(0)
		item.l10n = reader.S();       // :47
		readItemInfoBlob(reader, item); // :49
		item.equipmentSlot = reader.H(); // :51
		add.items.push_back(std::move(item));
	}
	reader.expectFullyConsumed();
	return add;
}

WarehouseUpdateItem decodeWarehouseUpdateItem(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_WAREHOUSE_UPDATE_ITEM");
	WarehouseUpdateItem update;
	update.item.objectId = reader.D();  // SM_WAREHOUSE_UPDATE_ITEM.java:33
	update.warehouseType = reader.C();  // :34
	update.item.l10n = reader.S();      // :35
	readItemInfoBlob(reader, update.item); // :37-39, GENERAL_INFO alone
	update.updateTypeMask = readOptionalUpdateMask(reader); // :41-42
	reader.expectFullyConsumed();
	return update;
}

DeleteWarehouseItem decodeDeleteWarehouseItem(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_DELETE_WAREHOUSE_ITEM");
	DeleteWarehouseItem deleted;
	deleted.warehouseType = reader.C();  // SM_DELETE_WAREHOUSE_ITEM.java:24
	deleted.objectId = reader.D();       // :25
	deleted.deleteTypeMask = reader.C(); // :26
	reader.expectFullyConsumed();
	return deleted;
}

// ---- SM_UPDATE_PLAYER_APPEARANCE --------------------------------------------------------------------------------------------------------

UpdatePlayerAppearance decodeUpdatePlayerAppearance(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_UPDATE_PLAYER_APPEARANCE");
	UpdatePlayerAppearance appearance;
	appearance.playerObjectId = reader.D();         // SM_UPDATE_PLAYER_APPEARANCE.java:23
	appearance.equipment = readEquippedItems(reader); // :24, AbstractPlayerInfoPacket.writeEquippedItems
	reader.expectFullyConsumed();
	return appearance;
}

// ---- SM_ITEM_USAGE_ANIMATION ------------------------------------------------------------------------------------------------------------

ItemUsageAnimation decodeItemUsageAnimation(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_ITEM_USAGE_ANIMATION");
	ItemUsageAnimation animation;
	animation.playerObjectId = reader.D(); // SM_ITEM_USAGE_ANIMATION.java:76
	animation.targetObjectId = reader.D(); // :77
	animation.itemObjectId = reader.D();   // :79
	animation.itemId = reader.D();         // :80
	animation.time = reader.D();           // :82
	animation.end = reader.C();            // :83
	animation.unk = reader.C();            // :84
	animation.unk1 = reader.C();           // :85
	animation.unk2 = reader.C();           // :86
	animation.unk3 = reader.D();           // :87
	reader.expectFullyConsumed();
	return animation;
}

} // namespace aion::gameserver::scenario::decoders
