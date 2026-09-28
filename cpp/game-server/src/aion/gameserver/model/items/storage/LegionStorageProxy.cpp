#include "aion/gameserver/model/items/storage/LegionStorageProxy.h"

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/legion/LegionWarehouse.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::items::storage {

using ItemAddType = services::item::ItemPacketService_ItemAddType;
using ItemDeleteType = services::item::ItemPacketService_ItemDeleteType;
using ItemUpdateType = services::item::ItemPacketService_ItemUpdateType;

LegionStorageProxy::LegionStorageProxy(team::legion::LegionWarehouse& storageValue, gameobjects::player::Player& actorValue)
	: Storage(storageValue.getStorageType(), false), actor(actorValue), storage(static_cast<Storage&>(storageValue)) {
	bindOwner(actorValue);
}

LegionStorageProxy::~LegionStorageProxy() = default;

void LegionStorageProxy::increaseKinah(int64_t amount) {
	storage->increaseKinah(amount, runtime::Ptr<gameobjects::player::Player>(actor));
}

void LegionStorageProxy::increaseKinah(int64_t amount, ItemUpdateType updateType) {
	storage->increaseKinah(amount, updateType, runtime::Ptr<gameobjects::player::Player>(actor));
}

bool LegionStorageProxy::tryDecreaseKinah(int64_t amount) {
	return storage->tryDecreaseKinah(amount, runtime::Ptr<gameobjects::player::Player>(actor));
}

bool LegionStorageProxy::tryDecreaseKinah(int64_t amount, ItemUpdateType updateType) {
	return storage->tryDecreaseKinah(amount, updateType, runtime::Ptr<gameobjects::player::Player>(actor));
}

void LegionStorageProxy::decreaseKinah(int64_t amount) {
	storage->decreaseKinah(amount, runtime::Ptr<gameobjects::player::Player>(actor));
}

void LegionStorageProxy::decreaseKinah(int64_t amount, ItemUpdateType updateType) {
	storage->decreaseKinah(amount, updateType, runtime::Ptr<gameobjects::player::Player>(actor));
}

int64_t LegionStorageProxy::increaseItemCount(gameobjects::Item& item, int64_t countValue) {
	return storage->increaseItemCount(item, countValue, runtime::Ptr<gameobjects::player::Player>(actor));
}

int64_t LegionStorageProxy::increaseItemCount(gameobjects::Item& item, int64_t countValue, ItemUpdateType updateType) {
	return storage->increaseItemCount(item, countValue, updateType, runtime::Ptr<gameobjects::player::Player>(actor));
}

int64_t LegionStorageProxy::decreaseItemCount(gameobjects::Item& item, int64_t countValue) {
	return storage->decreaseItemCount(runtime::Ptr<gameobjects::Item>(item), countValue, runtime::Ptr<gameobjects::player::Player>(actor));
}

int64_t LegionStorageProxy::decreaseItemCount(gameobjects::Item& item, int64_t countValue, ItemUpdateType updateType) {
	return storage->decreaseItemCount(runtime::Ptr<gameobjects::Item>(item), countValue, updateType, runtime::Ptr<gameobjects::player::Player>(actor));
}

int64_t LegionStorageProxy::decreaseItemCount(gameobjects::Item& /*item*/, int64_t /*countValue*/, ItemUpdateType /*updateType*/,
	questEngine::model::QuestStatus /*questStatus*/) {
	throw runtime::UnsupportedOperationException("Quests should not update LWH!");
}

runtime::Ptr<gameobjects::Item> LegionStorageProxy::add(gameobjects::Item& item) {
	return storage->add(item, runtime::Ptr<gameobjects::player::Player>(actor));
}

runtime::Ptr<gameobjects::Item> LegionStorageProxy::add(gameobjects::Item& item, ItemAddType addType) {
	return storage->add(item, addType, runtime::Ptr<gameobjects::player::Player>(actor));
}

runtime::Ptr<gameobjects::Item> LegionStorageProxy::put(gameobjects::Item& item) {
	return storage->put(item, runtime::Ptr<gameobjects::player::Player>(actor));
}

runtime::Ptr<gameobjects::Item> LegionStorageProxy::delete_(gameobjects::Item& item) {
	return storage->delete_(item, runtime::Ptr<gameobjects::player::Player>(actor));
}

runtime::Ptr<gameobjects::Item> LegionStorageProxy::delete_(gameobjects::Item& item, ItemDeleteType deleteType) {
	return storage->delete_(item, deleteType, runtime::Ptr<gameobjects::player::Player>(actor));
}

bool LegionStorageProxy::decreaseByItemId(int32_t itemId, int64_t countValue) {
	return storage->decreaseByItemId(itemId, countValue, runtime::Ptr<gameobjects::player::Player>(actor));
}

bool LegionStorageProxy::decreaseByItemId(int32_t /*itemId*/, int64_t /*countValue*/, questEngine::model::QuestStatus /*questStatus*/) {
	throw runtime::UnsupportedOperationException("Quests should not update LWH!");
}

bool LegionStorageProxy::decreaseByObjectId(int32_t itemObjId, int64_t countValue) {
	return storage->decreaseByObjectId(itemObjId, countValue, runtime::Ptr<gameobjects::player::Player>(actor));
}

bool LegionStorageProxy::decreaseByObjectId(int32_t itemObjId, int64_t countValue, ItemUpdateType updateType) {
	return storage->decreaseByObjectId(itemObjId, countValue, updateType, runtime::Ptr<gameobjects::player::Player>(actor));
}

bool LegionStorageProxy::decreaseByObjectId(int32_t /*itemObjId*/, int64_t /*countValue*/, questEngine::model::QuestStatus /*questStatus*/) {
	throw runtime::UnsupportedOperationException("Quests should not update LWH!");
}

int64_t LegionStorageProxy::getKinah() {
	return storage->getKinah();
}

runtime::Ptr<gameobjects::Item> LegionStorageProxy::getKinahItem() {
	return storage->getKinahItem();
}

StorageType LegionStorageProxy::getStorageType() {
	return storage->getStorageType();
}

void LegionStorageProxy::onLoadHandler(gameobjects::Item& item) {
	storage->onLoadHandler(item);
}

runtime::Ptr<gameobjects::Item> LegionStorageProxy::remove(gameobjects::Item& item) {
	return storage->remove(item);
}

runtime::Ptr<gameobjects::Item> LegionStorageProxy::getFirstItemByItemId(int32_t itemId) {
	return storage->getFirstItemByItemId(itemId);
}

std::vector<runtime::Ptr<gameobjects::Item>> LegionStorageProxy::getItemsWithKinah() {
	return storage->getItemsWithKinah();
}

std::vector<runtime::Ptr<gameobjects::Item>> LegionStorageProxy::getItems() {
	return storage->getItems();
}

std::vector<runtime::Ptr<gameobjects::Item>> LegionStorageProxy::getItemsByItemId(int32_t itemId) {
	return storage->getItemsByItemId(itemId);
}

runtime::ConcurrentLinkedQueue<runtime::Ref<gameobjects::Item>>& LegionStorageProxy::getDeletedItems() {
	return storage->getDeletedItems();
}

runtime::Ptr<gameobjects::Item> LegionStorageProxy::getItemByObjId(int32_t itemObjId) {
	return storage->getItemByObjId(itemObjId);
}

bool LegionStorageProxy::isFull() {
	return storage->isFull();
}

int32_t LegionStorageProxy::getFreeSlots() {
	return storage->getFreeSlots();
}

void LegionStorageProxy::setLimit(int32_t value) {
	storage->setLimit(value);
}

int32_t LegionStorageProxy::getLimit() {
	return storage->getLimit();
}

int32_t LegionStorageProxy::size() {
	return storage->size();
}

void LegionStorageProxy::setOwner(runtime::Ptr<gameobjects::player::Player> /*player*/) {
	throw runtime::UnsupportedOperationException("LWH doesnt have owner");
}

} // namespace aion::gameserver::model::items::storage
