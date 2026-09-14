#include "aion/gameserver/model/items/storage/PlayerStorage.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::model::items::storage {

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
	AION_UNPORTED();
}

void PlayerStorage::increaseKinah(int64_t amount) {
	AION_UNPORTED();
}

void PlayerStorage::increaseKinah(int64_t amount, services::item::ItemPacketService_ItemUpdateType updateType) {
	AION_UNPORTED();
}

bool PlayerStorage::tryDecreaseKinah(int64_t amount) {
	AION_UNPORTED();
}

bool PlayerStorage::tryDecreaseKinah(int64_t amount, services::item::ItemPacketService_ItemUpdateType updateType) {
	AION_UNPORTED();
}

void PlayerStorage::decreaseKinah(int64_t amount) {
	AION_UNPORTED();
}

void PlayerStorage::decreaseKinah(int64_t amount, services::item::ItemPacketService_ItemUpdateType updateType) {
	AION_UNPORTED();
}

int64_t PlayerStorage::increaseItemCount(gameobjects::Item& item, int64_t countValue) {
	AION_UNPORTED();
}

int64_t PlayerStorage::increaseItemCount(gameobjects::Item& item, int64_t countValue, services::item::ItemPacketService_ItemUpdateType updateType) {
	AION_UNPORTED();
}

int64_t PlayerStorage::decreaseItemCount(gameobjects::Item& item, int64_t countValue) {
	AION_UNPORTED();
}

int64_t PlayerStorage::decreaseItemCount(gameobjects::Item& item, int64_t countValue, services::item::ItemPacketService_ItemUpdateType updateType) {
	AION_UNPORTED();
}

int64_t PlayerStorage::decreaseItemCount(gameobjects::Item& item, int64_t countValue, services::item::ItemPacketService_ItemUpdateType updateType,
	questEngine::model::QuestStatus questStatus) {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Item> PlayerStorage::add(gameobjects::Item& item) {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Item> PlayerStorage::add(gameobjects::Item& item, services::item::ItemPacketService_ItemAddType addType) {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Item> PlayerStorage::put(gameobjects::Item& item) {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Item> PlayerStorage::delete_(gameobjects::Item& item) {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Item> PlayerStorage::delete_(gameobjects::Item& item, services::item::ItemPacketService_ItemDeleteType deleteType) {
	AION_UNPORTED();
}

bool PlayerStorage::decreaseByItemId(int32_t itemId, int64_t countValue) {
	AION_UNPORTED();
}

bool PlayerStorage::decreaseByItemId(int32_t itemId, int64_t countValue, questEngine::model::QuestStatus questStatus) {
	AION_UNPORTED();
}

bool PlayerStorage::decreaseByObjectId(int32_t itemObjId, int64_t countValue) {
	AION_UNPORTED();
}

bool PlayerStorage::decreaseByObjectId(int32_t itemObjId, int64_t countValue, questEngine::model::QuestStatus questStatus) {
	AION_UNPORTED();
}

bool PlayerStorage::decreaseByObjectId(int32_t itemObjId, int64_t countValue, services::item::ItemPacketService_ItemUpdateType updateType) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::items::storage
