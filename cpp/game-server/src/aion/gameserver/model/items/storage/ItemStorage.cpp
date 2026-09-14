#include "aion/gameserver/model/items/storage/ItemStorage.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/items/storage/StorageTypeInfo.h"

namespace aion::gameserver::model::items::storage {

ItemStorage::ItemStorage(StorageType storageTypeValue) : storageType(storageTypeValue), limit(storage::getLimit(storageTypeValue)) {
}

ItemStorage::~ItemStorage() = default;

runtime::Ref<ItemStorage> ItemStorage::create(StorageType storageTypeValue) {
	return runtime::makeRef<ItemStorage>(storageTypeValue);
}

std::vector<runtime::Ptr<gameobjects::Item>> ItemStorage::getItems() {
	AION_UNPORTED();
}

int32_t ItemStorage::getRowLength() {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Item> ItemStorage::getFirstItemById(int32_t itemId) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<gameobjects::Item>> ItemStorage::getItemsById(int32_t itemId) {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Item> ItemStorage::getItemByObjId(int32_t itemObjId) {
	AION_UNPORTED();
}

int64_t ItemStorage::getSlotIdByItemId(int32_t itemId) {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Item> ItemStorage::getItemBySlotId(int16_t slotId) {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Item> ItemStorage::getSpecialItemBySlotId(int16_t slotId) {
	AION_UNPORTED();
}

int64_t ItemStorage::getSlotIdByObjId(int32_t objId) {
	AION_UNPORTED();
}

bool ItemStorage::putItem(gameobjects::Item& item) {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Item> ItemStorage::removeItem(int32_t objId) {
	AION_UNPORTED();
}

bool ItemStorage::isFull() {
	AION_UNPORTED();
}

bool ItemStorage::isFullSpecialCube() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<gameobjects::Item>> ItemStorage::getSpecialCubeItems() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<gameobjects::Item>> ItemStorage::getCubeItems() {
	AION_UNPORTED();
}

int32_t ItemStorage::getFreeSlots() {
	AION_UNPORTED();
}

int32_t ItemStorage::getSpecialCubeFreeSlots() {
	AION_UNPORTED();
}

int32_t ItemStorage::size() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::items::storage
