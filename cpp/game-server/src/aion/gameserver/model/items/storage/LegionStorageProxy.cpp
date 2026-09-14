#include "aion/gameserver/model/items/storage/LegionStorageProxy.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/legion/LegionWarehouse.h"

namespace aion::gameserver::model::items::storage {

LegionStorageProxy::LegionStorageProxy(team::legion::LegionWarehouse& storageValue, gameobjects::player::Player& actorValue)
	: Storage(storageValue.getStorageType(), false), actor(actorValue), storage(static_cast<Storage&>(storageValue)) {
	bindOwner(actorValue);
}

LegionStorageProxy::~LegionStorageProxy() = default;

void LegionStorageProxy::increaseKinah(int64_t amount) {
	AION_UNPORTED();
}

void LegionStorageProxy::increaseKinah(int64_t amount, services::item::ItemPacketService_ItemUpdateType updateType) {
	AION_UNPORTED();
}

bool LegionStorageProxy::tryDecreaseKinah(int64_t amount) {
	AION_UNPORTED();
}

bool LegionStorageProxy::tryDecreaseKinah(int64_t amount, services::item::ItemPacketService_ItemUpdateType updateType) {
	AION_UNPORTED();
}

void LegionStorageProxy::decreaseKinah(int64_t amount) {
	AION_UNPORTED();
}

void LegionStorageProxy::decreaseKinah(int64_t amount, services::item::ItemPacketService_ItemUpdateType updateType) {
	AION_UNPORTED();
}

int64_t LegionStorageProxy::increaseItemCount(gameobjects::Item& item, int64_t countValue) {
	AION_UNPORTED();
}

int64_t LegionStorageProxy::increaseItemCount(gameobjects::Item& item, int64_t countValue,
	services::item::ItemPacketService_ItemUpdateType updateType) {
	AION_UNPORTED();
}

int64_t LegionStorageProxy::decreaseItemCount(gameobjects::Item& item, int64_t countValue) {
	AION_UNPORTED();
}

int64_t LegionStorageProxy::decreaseItemCount(gameobjects::Item& item, int64_t countValue,
	services::item::ItemPacketService_ItemUpdateType updateType) {
	AION_UNPORTED();
}

int64_t LegionStorageProxy::decreaseItemCount(gameobjects::Item& item, int64_t countValue,
	services::item::ItemPacketService_ItemUpdateType updateType, questEngine::model::QuestStatus questStatus) {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Item> LegionStorageProxy::add(gameobjects::Item& item) {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Item> LegionStorageProxy::add(gameobjects::Item& item, services::item::ItemPacketService_ItemAddType addType) {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Item> LegionStorageProxy::put(gameobjects::Item& item) {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Item> LegionStorageProxy::delete_(gameobjects::Item& item) {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Item> LegionStorageProxy::delete_(gameobjects::Item& item, services::item::ItemPacketService_ItemDeleteType deleteType) {
	AION_UNPORTED();
}

bool LegionStorageProxy::decreaseByItemId(int32_t itemId, int64_t countValue) {
	AION_UNPORTED();
}

bool LegionStorageProxy::decreaseByItemId(int32_t itemId, int64_t countValue, questEngine::model::QuestStatus questStatus) {
	AION_UNPORTED();
}

bool LegionStorageProxy::decreaseByObjectId(int32_t itemObjId, int64_t countValue) {
	AION_UNPORTED();
}

bool LegionStorageProxy::decreaseByObjectId(int32_t itemObjId, int64_t countValue, questEngine::model::QuestStatus questStatus) {
	AION_UNPORTED();
}

bool LegionStorageProxy::decreaseByObjectId(int32_t itemObjId, int64_t countValue, services::item::ItemPacketService_ItemUpdateType updateType) {
	AION_UNPORTED();
}

int64_t LegionStorageProxy::getKinah() {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Item> LegionStorageProxy::getKinahItem() {
	AION_UNPORTED();
}

StorageType LegionStorageProxy::getStorageType() {
	AION_UNPORTED();
}

void LegionStorageProxy::onLoadHandler(gameobjects::Item& item) {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Item> LegionStorageProxy::remove(gameobjects::Item& item) {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Item> LegionStorageProxy::getFirstItemByItemId(int32_t itemId) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<gameobjects::Item>> LegionStorageProxy::getItemsWithKinah() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<gameobjects::Item>> LegionStorageProxy::getItems() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<gameobjects::Item>> LegionStorageProxy::getItemsByItemId(int32_t itemId) {
	AION_UNPORTED();
}

runtime::ConcurrentLinkedQueue<runtime::Ref<gameobjects::Item>>& LegionStorageProxy::getDeletedItems() {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Item> LegionStorageProxy::getItemByObjId(int32_t itemObjId) {
	AION_UNPORTED();
}

bool LegionStorageProxy::isFull() {
	AION_UNPORTED();
}

int32_t LegionStorageProxy::getFreeSlots() {
	AION_UNPORTED();
}

void LegionStorageProxy::setLimit(int32_t limit) {
	AION_UNPORTED();
}

int32_t LegionStorageProxy::getLimit() {
	AION_UNPORTED();
}

int32_t LegionStorageProxy::size() {
	AION_UNPORTED();
}

void LegionStorageProxy::setOwner(runtime::Ptr<gameobjects::player::Player> player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::items::storage
