#include "aion/gameserver/model/items/storage/ItemStorage.h"

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/items/storage/StorageTypeInfo.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"

namespace aion::gameserver::model::items::storage {

ItemStorage::ItemStorage(StorageType storageTypeValue) : storageType(storageTypeValue), limit(storage::getLimit(storageTypeValue)) {
}

ItemStorage::~ItemStorage() = default;

runtime::Ref<ItemStorage> ItemStorage::create(StorageType storageTypeValue) {
	return runtime::makeRef<ItemStorage>(storageTypeValue);
}

std::vector<runtime::Ptr<gameobjects::Item>> ItemStorage::getItems() {
	return items.values().toVector();
}

int32_t ItemStorage::getRowLength() {
	return storage::getLength(storageType);
}

runtime::Ptr<gameobjects::Item> ItemStorage::getFirstItemById(int32_t itemId) {
	for (runtime::Ptr<gameobjects::Item> item : items.values()) {
		if (item->getItemTemplate()->getTemplateId() == itemId) {
			return item;
		}
	}
	return nullptr;
}

std::vector<runtime::Ptr<gameobjects::Item>> ItemStorage::getItemsById(int32_t itemId) {
	std::vector<runtime::Ptr<gameobjects::Item>> temp;
	for (runtime::Ptr<gameobjects::Item> item : items.values()) {
		if (item->getItemTemplate()->getTemplateId() == itemId) {
			temp.push_back(item);
		}
	}
	return temp;
}

runtime::Ptr<gameobjects::Item> ItemStorage::getItemByObjId(int32_t itemObjId) {
	return items.get(itemObjId);
}

int64_t ItemStorage::getSlotIdByItemId(int32_t itemId) {
	for (runtime::Ptr<gameobjects::Item> item : items.values()) {
		if (item->getItemTemplate()->getTemplateId() == itemId) {
			return item->getEquipmentSlot();
		}
	}
	return -1;
}

runtime::Ptr<gameobjects::Item> ItemStorage::getItemBySlotId(int16_t slotId) {
	for (runtime::Ptr<gameobjects::Item> item : getCubeItems()) {
		if (item->getEquipmentSlot() == slotId) {
			return item;
		}
	}
	return nullptr;
}

runtime::Ptr<gameobjects::Item> ItemStorage::getSpecialItemBySlotId(int16_t slotId) {
	for (runtime::Ptr<gameobjects::Item> item : getSpecialCubeItems()) {
		if (item->getEquipmentSlot() == slotId) {
			return item;
		}
	}
	return nullptr;
}

int64_t ItemStorage::getSlotIdByObjId(int32_t objId) {
	runtime::Ptr<gameobjects::Item> item = getItemByObjId(objId);
	if (item)
		return item->getEquipmentSlot();
	else
		return -1;
}

bool ItemStorage::putItem(gameobjects::Item& item) {
	return !items.putIfAbsent(item.getObjectId(), runtime::Ref<gameobjects::Item>(item));
}

runtime::Ptr<gameobjects::Item> ItemStorage::removeItem(int32_t objId) {
	return items.remove(objId);
}

bool ItemStorage::isFull() {
	return static_cast<int32_t>(getCubeItems().size()) >= limit.get();
}

bool ItemStorage::isFullSpecialCube() {
	return static_cast<int32_t>(getSpecialCubeItems().size()) >= storage::getSpecialLimit(storageType);
}

std::vector<runtime::Ptr<gameobjects::Item>> ItemStorage::getSpecialCubeItems() {
	std::vector<runtime::Ptr<gameobjects::Item>> specialCubeItems;
	for (runtime::Ptr<gameobjects::Item> i : items.values()) {
		if (i->getItemTemplate()->getExtraInventoryId() > 0)
			specialCubeItems.push_back(i);
	}
	return specialCubeItems;
}

std::vector<runtime::Ptr<gameobjects::Item>> ItemStorage::getCubeItems() {
	std::vector<runtime::Ptr<gameobjects::Item>> cubeItems;
	for (runtime::Ptr<gameobjects::Item> i : items.values()) {
		if (i->getItemTemplate()->getExtraInventoryId() < 1)
			cubeItems.push_back(i);
	}
	return cubeItems;
}

int32_t ItemStorage::getFreeSlots() {
	return limit.get() - static_cast<int32_t>(getCubeItems().size());
}

int32_t ItemStorage::getSpecialCubeFreeSlots() {
	return storage::getSpecialLimit(storageType) - static_cast<int32_t>(getSpecialCubeItems().size());
}

int32_t ItemStorage::size() {
	return items.size();
}

} // namespace aion::gameserver::model::items::storage
