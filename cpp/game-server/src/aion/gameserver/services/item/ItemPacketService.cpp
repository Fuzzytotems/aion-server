#include "aion/gameserver/services/item/ItemPacketService.h"

#include <optional>
#include <vector>

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/items/storage/StorageTypeInfo.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CUBE_UPDATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DELETE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DELETE_WAREHOUSE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_ADD_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_EDIT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_WAREHOUSE_ADD_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_WAREHOUSE_UPDATE_ITEM.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemDeleteTypeInfo.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::services::item {

namespace {

using model::gameobjects::Item;
using model::gameobjects::player::Player;
using model::items::storage::StorageType;
using network::aion::serverpackets::SM_CUBE_UPDATE;
using network::aion::serverpackets::SM_DELETE_ITEM;
using network::aion::serverpackets::SM_DELETE_WAREHOUSE_ITEM;
using network::aion::serverpackets::SM_INVENTORY_ADD_ITEM;
using network::aion::serverpackets::SM_INVENTORY_UPDATE_ITEM;
using network::aion::serverpackets::SM_LEGION_EDIT;
using network::aion::serverpackets::SM_WAREHOUSE_ADD_ITEM;
using network::aion::serverpackets::SM_WAREHOUSE_UPDATE_ITEM;
using utils::PacketSendUtility;

/**
 * Java `PacketSendUtility.sendPacket(player, new SM_LEGION_EDIT(0x04, player.getLegion()))`.
 * <p>
 * Deviation (docs/deviations/P5-07.md): for a player without a legion Java queues the packet with a null legion and the caller goes on; its
 * writeImpl then throws a NullPointerException on the write thread (`legion.getLegionWarehouse()`) after the length, the opcode header and the
 * 0x04 byte went into the connection's write buffer, and the stale rest of that buffer is sent next, corrupting the client's stream (a Java
 * defect). C++ serializes in the caller, so the packet is not built for a null legion rather than throwing into the caller (the precedent of
 * MotionList, P4-12.md): the client receives nothing.
 */
void sendLegionWarehouseKinah(Player& player) {
	if (runtime::Ptr<model::team::legion::Legion> legion = player.getLegion())
		PacketSendUtility::sendPacket(player, SM_LEGION_EDIT(0x04, *legion));
}

} // namespace

void ItemPacketService::updateItemAfterInfoChange(Player& player, Item& item) {
	PacketSendUtility::sendPacket(player, SM_INVENTORY_UPDATE_ITEM(player, item));
}

void ItemPacketService::updateItemAfterInfoChange(Player& player, Item& item, ItemPacketService::ItemUpdateType updateType) {
	PacketSendUtility::sendPacket(player, SM_INVENTORY_UPDATE_ITEM(player, item, updateType));
}

void ItemPacketService::updateItemAfterEquip(Player& player, Item& item) {
	PacketSendUtility::sendPacket(player, SM_INVENTORY_UPDATE_ITEM(player, item, ItemUpdateType::EQUIP_UNEQUIP));
}

void ItemPacketService::sendItemPacket(Player& player, StorageType storageType, Item& item, ItemPacketService::ItemUpdateType updateType) {
	if (item.getItemCount() <= 0 && !item.getItemTemplate()->isKinah()) {
		sendItemDeletePacket(player, storageType, item, fromUpdateType(updateType));
	} else {
		sendItemUpdatePacket(player, storageType, item, updateType);
	}
}

void ItemPacketService::sendItemDeletePacket(Player& player, StorageType storageType, Item& item, ItemPacketService::ItemDeleteType deleteType) {
	// Java: Objects.requireNonNull(storageType) == StorageType.CUBE - the C++ StorageType is a value; a caller holding an unknown storage id
	// throws the NullPointerException before the call (Storage.cpp requireStorageTypeById)
	if (storageType == StorageType::CUBE) {
		PacketSendUtility::sendPacket(player, SM_DELETE_ITEM(item.getObjectId(), deleteType));
	} else {
		PacketSendUtility::sendPacket(player, SM_DELETE_WAREHOUSE_ITEM(model::items::storage::getId(storageType), item.getObjectId(), deleteType));
	}
	PacketSendUtility::sendPacket(player, SM_CUBE_UPDATE::cubeSize(storageType, player));
}

void ItemPacketService::sendItemUpdatePacket(Player& player, StorageType storageType, Item& item, ItemPacketService::ItemUpdateType updateType) {
	switch (storageType) {
		case StorageType::CUBE:
			PacketSendUtility::sendPacket(player, SM_INVENTORY_UPDATE_ITEM(player, item, updateType));
			break;
		case StorageType::LEGION_WAREHOUSE:
			if (item.getItemTemplate()->isKinah()) {
				sendLegionWarehouseKinah(player);
				break;
			}
			[[fallthrough]];
		default:
			PacketSendUtility::sendPacket(player, SM_WAREHOUSE_UPDATE_ITEM(player, item, model::items::storage::getId(storageType), updateType));
	}
}

void ItemPacketService::sendStorageUpdatePacket(Player& player, StorageType storageType, Item& item) {
	sendStorageUpdatePacket(player, storageType, item, ItemAddType::ITEM_COLLECT);
}

void ItemPacketService::sendStorageUpdatePacket(Player& player, StorageType storageType, Item& item, ItemPacketService::ItemAddType addType) {
	switch (storageType) {
		case StorageType::CUBE:
			// Java: Collections.singletonList(item)
			PacketSendUtility::sendPacket(player, SM_INVENTORY_ADD_ITEM(std::vector<runtime::Ptr<Item>>{runtime::Ptr<Item>(item)}, player, addType));
			break;
		case StorageType::LEGION_WAREHOUSE:
			if (item.getItemTemplate()->isKinah()) {
				sendLegionWarehouseKinah(player);
				break;
			}
			[[fallthrough]];
		default:
			PacketSendUtility::sendPacket(player, SM_WAREHOUSE_ADD_ITEM(item, model::items::storage::getId(storageType), player, addType));
	}
	PacketSendUtility::sendPacket(player, SM_CUBE_UPDATE::cubeSize(storageType, player));
}

void ItemPacketService::sendItemUnlockPacket(Player& player, Item& item) {
	std::optional<StorageType> storageType = model::items::storage::getStorageTypeById(item.getItemLocation());
	if (storageType)
		sendStorageUpdatePacket(player, *storageType, item, ItemAddType::ALL_SLOT);
}

} // namespace aion::gameserver::services::item
