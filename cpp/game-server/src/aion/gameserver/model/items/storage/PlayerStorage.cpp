#include "aion/gameserver/model/items/storage/PlayerStorage.h"

#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::model::items::storage {

using ItemAddType = services::item::ItemPacketService_ItemAddType;
using ItemDeleteType = services::item::ItemPacketService_ItemDeleteType;
using ItemUpdateType = services::item::ItemPacketService_ItemUpdateType;

PlayerStorage::PlayerStorage(gameobjects::player::Player& owner, StorageType storageTypeValue) : Storage(storageTypeValue) {
	bindOwner(owner);
	actor.set(owner);
}

PlayerStorage::PlayerStorage(account::Account& account, StorageType storageTypeValue) : Storage(storageTypeValue) {
	bindOwner(account);
}

PlayerStorage::~PlayerStorage() = default;

void PlayerStorage::setOwner(runtime::Ptr<gameobjects::player::Player> value) {
	actor.set(value);
}

void PlayerStorage::onLoadHandler(gameobjects::Item& item) {
	if (item.isEquipped())
		actor->getEquipment().onLoadHandler(item);
	else {
		Storage::onLoadHandler(item);
	}
}

void PlayerStorage::increaseKinah(int64_t amount) {
	increaseKinah(amount, actor.get());
}

void PlayerStorage::increaseKinah(int64_t amount, ItemUpdateType updateType) {
	increaseKinah(amount, updateType, actor.get());
}

bool PlayerStorage::tryDecreaseKinah(int64_t amount) {
	return tryDecreaseKinah(amount, actor.get());
}

bool PlayerStorage::tryDecreaseKinah(int64_t amount, ItemUpdateType updateType) {
	return tryDecreaseKinah(amount, updateType, actor.get());
}

void PlayerStorage::decreaseKinah(int64_t amount) {
	decreaseKinah(amount, actor.get());
}

void PlayerStorage::decreaseKinah(int64_t amount, ItemUpdateType updateType) {
	decreaseKinah(amount, updateType, actor.get());
}

int64_t PlayerStorage::increaseItemCount(gameobjects::Item& item, int64_t countValue) {
	return increaseItemCount(item, countValue, actor.get());
}

int64_t PlayerStorage::increaseItemCount(gameobjects::Item& item, int64_t countValue, ItemUpdateType updateType) {
	return increaseItemCount(item, countValue, updateType, actor.get());
}

int64_t PlayerStorage::decreaseItemCount(gameobjects::Item& item, int64_t countValue) {
	return decreaseItemCount(runtime::Ptr<gameobjects::Item>(item), countValue, actor.get());
}

int64_t PlayerStorage::decreaseItemCount(gameobjects::Item& item, int64_t countValue, ItemUpdateType updateType) {
	return decreaseItemCount(runtime::Ptr<gameobjects::Item>(item), countValue, updateType, actor.get());
}

int64_t PlayerStorage::decreaseItemCount(gameobjects::Item& item, int64_t countValue, ItemUpdateType updateType,
	questEngine::model::QuestStatus questStatus) {
	return decreaseItemCount(runtime::Ptr<gameobjects::Item>(item), countValue, updateType, std::optional(questStatus), actor.get());
}

runtime::Ptr<gameobjects::Item> PlayerStorage::add(gameobjects::Item& item) {
	return add(item, actor.get());
}

runtime::Ptr<gameobjects::Item> PlayerStorage::add(gameobjects::Item& item, ItemAddType addType) {
	return add(item, addType, actor.get());
}

runtime::Ptr<gameobjects::Item> PlayerStorage::put(gameobjects::Item& item) {
	return put(item, actor.get());
}

runtime::Ptr<gameobjects::Item> PlayerStorage::delete_(gameobjects::Item& item) {
	return delete_(item, actor.get());
}

runtime::Ptr<gameobjects::Item> PlayerStorage::delete_(gameobjects::Item& item, ItemDeleteType deleteType) {
	return delete_(item, deleteType, actor.get());
}

bool PlayerStorage::decreaseByItemId(int32_t itemId, int64_t countValue) {
	return decreaseByItemId(itemId, countValue, actor.get());
}

bool PlayerStorage::decreaseByItemId(int32_t itemId, int64_t countValue, questEngine::model::QuestStatus questStatus) {
	return decreaseByItemId(itemId, countValue, std::optional(questStatus), actor.get());
}

bool PlayerStorage::decreaseByObjectId(int32_t itemObjId, int64_t countValue) {
	return decreaseByObjectId(itemObjId, countValue, actor.get());
}

bool PlayerStorage::decreaseByObjectId(int32_t itemObjId, int64_t countValue, questEngine::model::QuestStatus questStatus) {
	return decreaseByObjectId(itemObjId, countValue, questStatus, actor.get());
}

bool PlayerStorage::decreaseByObjectId(int32_t itemObjId, int64_t countValue, ItemUpdateType updateType) {
	return decreaseByObjectId(itemObjId, countValue, updateType, actor.get());
}

} // namespace aion::gameserver::model::items::storage
