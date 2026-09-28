#include "aion/gameserver/services/item/ItemMoveService.h"

#include <string>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/GameServer.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/IStorage.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/items/storage/StorageTypeInfo.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/LegionService.h"
#include "aion/gameserver/services/item/ItemPacketService.h"
#include "aion/gameserver/services/item/ItemRestrictionService.h"
#include "aion/gameserver/services/item/ItemSplitService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::services::item {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.item.ItemMoveService");

namespace {

using model::gameobjects::Item;
using model::items::storage::Storage;
using model::items::storage::StorageType;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ptr;
using utils::PacketSendUtility;
using PersistentState = model::gameobjects::Persistable::PersistentState;

} // namespace

void ItemMoveService::moveItem(model::gameobjects::player::Player& player, int32_t itemObjId, int8_t sourceStorageType, int8_t destinationStorageType, int16_t slot) {
	Ptr<Storage> sourceStorage = player.getStorage(sourceStorageType);
	if (!sourceStorage) {
		log.error(player.toString() + " tried to move itemObjId " + std::to_string(itemObjId) + " from unknown sourceStorageType: "
			+ std::to_string(sourceStorageType));
		return;
	}
	Ptr<Item> item = sourceStorage->getItemByObjId(itemObjId);
	if (!item)
		return;

	Ptr<Storage> targetStorage = player.getStorage(destinationStorageType);
	if (!targetStorage) {
		log.error(player.toString() + " tried to move itemObjId " + std::to_string(itemObjId) + " to unknown destinationStorageType: "
			+ std::to_string(destinationStorageType));
		return;
	}

	if (sourceStorageType == destinationStorageType) {
		if (item->getEquipmentSlot() != slot)
			moveInSameStorage(*sourceStorage, *item, slot);
		return;
	}
	if (ItemRestrictionService::isItemRestrictedTo(player, *item, targetStorage->getStorageType())
		|| ItemRestrictionService::isItemRestrictedFrom(player, *item, sourceStorage->getStorageType()) || player.isTrading()
		|| GameServer::isShuttingDownSoon()) {
		ItemPacketService::sendItemUnlockPacket(player, *item);
		if (GameServer::isShuttingDownSoon())
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_DISABLE("Shutdown Progress"));
		return;
	}

	const int32_t legionWarehouseId = model::items::storage::getId(StorageType::LEGION_WAREHOUSE);
	if (sourceStorageType == legionWarehouseId || destinationStorageType == legionWarehouseId) {
		LegionService::getInstance().addWHItemHistory(player, item->getItemId(), item->getItemCount(), *sourceStorage, *targetStorage);
	}
	if (slot == -1) {
		if (item->getItemTemplate()->isStackable()) {
			for (const Ptr<Item>& targetStack : targetStorage->getItemsByItemId(item->getItemId())) {
				ItemSplitService::mergeStacks(*sourceStorage, *targetStorage, *item, *targetStack, item->getItemCount());
				if (item->getItemCount() == 0) {
					return;
				}
			}
		}
	}
	if (targetStorage->isFull()) {
		PacketSendUtility::sendPacket(player, targetStorage->getStorageIsFullMessage());
		ItemPacketService::sendItemUnlockPacket(player, *item);
		return;
	}
	sourceStorage->remove(*item);
	ItemPacketService::sendItemDeletePacket(player, sourceStorage->getStorageType(), *item, ItemPacketService::ItemDeleteType::MOVE);
	item->setEquipmentSlot(slot);
	targetStorage->add(*item);
}

void ItemMoveService::moveInSameStorage(model::items::storage::IStorage& storage, model::gameobjects::Item& item, int16_t slot) {
	storage.setPersistentState(PersistentState::UPDATE_REQUIRED);
	item.setEquipmentSlot(slot);
	item.setPersistentState(PersistentState::UPDATE_REQUIRED);
}

void ItemMoveService::switchItemsInStorages(model::gameobjects::player::Player& player, int8_t sourceStorageType, int32_t sourceItemObjId, int8_t replaceStorageType, int32_t replaceItemObjId) {
	// Java dereferences both storages without a check: an unknown storage type is a NullPointerException (the Ptr dereferences below)
	Ptr<Storage> sourceStorage = player.getStorage(sourceStorageType);
	Ptr<Storage> replaceStorage = player.getStorage(replaceStorageType);

	Ptr<Item> sourceItem = sourceStorage->getItemByObjId(sourceItemObjId);
	if (!sourceItem)
		return;

	Ptr<Item> replaceItem = replaceStorage->getItemByObjId(replaceItemObjId);
	if (!replaceItem)
		return;

	// restrictions checks
	if (ItemRestrictionService::isItemRestrictedFrom(player, *sourceItem, sourceStorage->getStorageType())
		|| ItemRestrictionService::isItemRestrictedFrom(player, *replaceItem, replaceStorage->getStorageType())
		|| ItemRestrictionService::isItemRestrictedTo(player, *sourceItem, replaceStorage->getStorageType())
		|| ItemRestrictionService::isItemRestrictedTo(player, *replaceItem, sourceStorage->getStorageType()) || player.isTrading()
		|| GameServer::isShuttingDownSoon()) {
		ItemPacketService::sendItemUnlockPacket(player, *sourceItem);
		ItemPacketService::sendItemUnlockPacket(player, *replaceItem);
		if (GameServer::isShuttingDownSoon())
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_DISABLE("Shutdown Progress"));
		return;
	}

	int64_t sourceSlot = sourceItem->getEquipmentSlot();
	int64_t replaceSlot = replaceItem->getEquipmentSlot();

	sourceItem->setEquipmentSlot(replaceSlot);
	replaceItem->setEquipmentSlot(sourceSlot);

	sourceStorage->remove(*sourceItem);
	replaceStorage->remove(*replaceItem);

	// correct UI update order is 1)delete items 2) add items
	ItemPacketService::sendItemDeletePacket(player, sourceStorage->getStorageType(), *sourceItem, ItemPacketService::ItemDeleteType::MOVE);
	ItemPacketService::sendItemDeletePacket(player, replaceStorage->getStorageType(), *replaceItem, ItemPacketService::ItemDeleteType::MOVE);
	sourceStorage->add(*replaceItem);
	replaceStorage->add(*sourceItem);
}

} // namespace aion::gameserver::services::item
