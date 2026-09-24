#include "aion/gameserver/services/item/ItemSplitService.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/IStorage.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/items/storage/StorageTypeInfo.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CUBE_UPDATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/LegionService.h"
#include "aion/gameserver/services/item/ItemFactory.h"
#include "aion/gameserver/services/item/ItemPacketService.h"
#include "aion/gameserver/services/item/ItemRestrictionService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"

namespace aion::gameserver::services::item {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.item.ItemSplitService");

namespace {

using model::gameobjects::Item;
using model::items::storage::Storage;
using model::items::storage::StorageType;
using network::aion::serverpackets::SM_CUBE_UPDATE;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ptr;
using runtime::Ref;
using utils::PacketSendUtility;
using ItemUpdateType = ItemPacketService::ItemUpdateType;

/** Java long addition (two's complement wrap-around) */
constexpr int64_t javaLongAdd(int64_t a, int64_t b) noexcept {
	return static_cast<int64_t>(static_cast<uint64_t>(a) + static_cast<uint64_t>(b));
}

/** Java long subtraction (two's complement wrap-around) */
constexpr int64_t javaLongSub(int64_t a, int64_t b) noexcept {
	return static_cast<int64_t>(static_cast<uint64_t>(a) - static_cast<uint64_t>(b));
}

} // namespace

void ItemSplitService::splitItem(model::gameobjects::player::Player& player, int32_t itemObjId, int32_t destinationObjId, int64_t splitAmount, int16_t slotNum, int8_t sourceStorageType, int8_t destinationStorageType) {
	if (splitAmount <= 0) {
		return;
	}
	if (player.isTrading()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_INVENTORY_SPLIT_DURING_TRADE());
		return;
	}

	Ptr<Storage> sourceStorage = player.getStorage(sourceStorageType);
	Ptr<Storage> destStorage = player.getStorage(destinationStorageType);
	if (!sourceStorage || !destStorage) {
		// Java: String.format("storage null playerName sourceStorage destStorage %s %d %d", player.getName(), sourceStorageType, destinationStorageType)
		log.warn("storage null playerName sourceStorage destStorage " + player.getName() + " " + std::to_string(sourceStorageType) + " "
			+ std::to_string(destinationStorageType));
		return;
	}
	Ptr<Item> sourceItem = sourceStorage->getItemByObjId(itemObjId);
	Ptr<Item> targetItem = destStorage->getItemByObjId(destinationObjId);

	if (!sourceItem) {
		sourceItem = sourceStorage->getKinahItem();
		if (!sourceItem || sourceItem->getObjectId() != itemObjId) {
			// Java: String.format("CHECKPOINT: attempt to split null item %d %d %d", itemObjId, splitAmount, slotNum)
			log.warn("CHECKPOINT: attempt to split null item " + std::to_string(itemObjId) + " " + std::to_string(splitAmount) + " "
				+ std::to_string(slotNum));
			return;
		}
	}

	if (sourceStorageType != destinationStorageType
		&& (ItemRestrictionService::isItemRestrictedTo(player, *sourceItem, destStorage->getStorageType())
			|| ItemRestrictionService::isItemRestrictedFrom(player, *sourceItem, sourceStorage->getStorageType()))) {
		ItemPacketService::sendStorageUpdatePacket(player, sourceStorage->getStorageType(), *sourceItem);
		return;
	}

	// To move kinah from inventory to warehouse and vice versa client using split item packet
	if (sourceItem->getItemTemplate()->isKinah()) {
		moveKinah(player, *sourceStorage, splitAmount);
		return;
	}

	if (!targetItem) {
		if (destStorage->isFull()) {
			PacketSendUtility::sendPacket(player, destStorage->getStorageIsFullMessage());
			return;
		}
		int64_t oldItemCount = javaLongSub(sourceItem->getItemCount(), splitAmount);
		if (sourceItem->getItemCount() < splitAmount || oldItemCount == 0) {
			return;
		}
		if (sourceStorageType != destinationStorageType) {
			LegionService::getInstance().addWHItemHistory(player, sourceItem->getItemId(), splitAmount, *sourceStorage, *destStorage);
		}
		Ref<Item> newItem = ItemFactory::newItem(sourceItem->getItemTemplate()->getTemplateId(), splitAmount);
		if (sourceStorageType == destinationStorageType)
			newItem->setEquipmentSlot(slotNum);
		sourceStorage->decreaseItemCount(*sourceItem, splitAmount,
			sourceStorageType == destinationStorageType ? ItemUpdateType::DEC_ITEM_SPLIT : ItemUpdateType::DEC_ITEM_SPLIT_MOVE);
		PacketSendUtility::sendPacket(player, SM_CUBE_UPDATE::cubeSize(sourceStorage->getStorageType(), player));
		if (!destStorage->add(*newItem)) {
			// if item was not added - we can release its id
			utils::idfactory::IDFactory::getInstance().releaseId(newItem->getObjectId());
		}
	} else if (targetItem->getItemId() == sourceItem->getItemId()) {
		if (sourceStorageType != destinationStorageType) {
			LegionService::getInstance().addWHItemHistory(player, sourceItem->getItemId(), splitAmount, *sourceStorage, *destStorage);
		}
		mergeStacks(*sourceStorage, *destStorage, *sourceItem, *targetItem, splitAmount);
	}
}

void ItemSplitService::mergeStacks(model::items::storage::IStorage& sourceStorage, model::items::storage::IStorage& destStorage, model::gameobjects::Item& sourceItem, model::gameobjects::Item& targetItem, int64_t count) {
	if (sourceItem.getItemCount() >= count) {
		int64_t freeCount = targetItem.getFreeCount();
		count = count > freeCount ? freeCount : count;
		int64_t leftCount = destStorage.increaseItemCount(targetItem, count,
			sourceStorage.getStorageType() == destStorage.getStorageType() ? ItemUpdateType::INC_ITEM_MERGE : ItemUpdateType::INC_ITEM_COLLECT);
		sourceStorage.decreaseItemCount(sourceItem, javaLongSub(count, leftCount),
			sourceStorage.getStorageType() == destStorage.getStorageType() ? ItemUpdateType::DEC_ITEM_SPLIT : ItemUpdateType::DEC_ITEM_SPLIT_MOVE);
	}

}

void ItemSplitService::moveKinah(model::gameobjects::player::Player& player, model::items::storage::IStorage& source, int64_t splitAmount) {
	if (source.getKinah() < splitAmount)
		return;
	switch (source.getStorageType()) {
		case StorageType::CUBE: {
			// Java dereferences the storage without a check (a NullPointerException for a missing one: the Ptr dereference)
			Ptr<Storage> destination = player.getStorage(model::items::storage::getId(StorageType::ACCOUNT_WAREHOUSE));
			int64_t chksum = javaLongAdd(javaLongSub(source.getKinah(), splitAmount), javaLongAdd(destination->getKinah(), splitAmount));

			if (chksum != javaLongAdd(source.getKinah(), destination->getKinah()))
				return;

			updateKinahCount(source, splitAmount, *destination);
			break;
		}

		case StorageType::ACCOUNT_WAREHOUSE: {
			Ptr<Storage> destination = player.getStorage(model::items::storage::getId(StorageType::CUBE));
			int64_t chksum = javaLongAdd(javaLongSub(source.getKinah(), splitAmount), javaLongAdd(destination->getKinah(), splitAmount));

			if (chksum != javaLongAdd(source.getKinah(), destination->getKinah()))
				return;

			updateKinahCount(source, splitAmount, *destination);
			break;
		}
		default:
			break;
	}
}

void ItemSplitService::updateKinahCount(model::items::storage::IStorage& source, int64_t splitAmount, model::items::storage::IStorage& destination) {
	source.decreaseKinah(splitAmount, ItemUpdateType::DEC_ITEM_SPLIT);
	destination.increaseKinah(splitAmount, ItemUpdateType::INC_KINAH_MERGE);
}

} // namespace aion::gameserver::services::item
