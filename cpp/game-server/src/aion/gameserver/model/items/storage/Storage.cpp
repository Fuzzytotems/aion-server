#include "aion/gameserver/model/items/storage/Storage.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/items/storage/ItemStorage.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::items::storage {

[[maybe_unused]] static const auto log = commons::logging::LoggerFactory::getLogger("ITEM_LOG");

Storage::Storage(StorageType storageTypeValue) : Storage(storageTypeValue, true) {
}

Storage::Storage(StorageType storageTypeValue, bool withDeletedItems)
	: itemStorage(ItemStorage::create(storageTypeValue)), storageType(storageTypeValue) {
	static_cast<void>(withDeletedItems); // Java: deletedItems = withDeletedItems ? new ConcurrentLinkedQueue<>() : null (the C++ queue always exists)
}

Storage::~Storage() = default;

int64_t Storage::getKinah() {
	AION_UNPORTED();
}

void Storage::increaseKinah(int64_t amount, runtime::Ptr<gameobjects::player::Player> actor) {
	AION_UNPORTED();
}

void Storage::increaseKinah(int64_t amount, services::item::ItemPacketService_ItemUpdateType updateType,
	runtime::Ptr<gameobjects::player::Player> actor) {
	AION_UNPORTED();
}

bool Storage::tryDecreaseKinah(int64_t amount, runtime::Ptr<gameobjects::player::Player> actor) {
	AION_UNPORTED();
}

bool Storage::tryDecreaseKinah(int64_t amount, services::item::ItemPacketService_ItemUpdateType updateType,
	runtime::Ptr<gameobjects::player::Player> actor) {
	AION_UNPORTED();
}

void Storage::decreaseKinah(int64_t amount, runtime::Ptr<gameobjects::player::Player> actor) {
	AION_UNPORTED();
}

void Storage::decreaseKinah(int64_t amount, services::item::ItemPacketService_ItemUpdateType updateType,
	runtime::Ptr<gameobjects::player::Player> actor) {
	AION_UNPORTED();
}

int64_t Storage::increaseItemCount(gameobjects::Item& item, int64_t countValue, runtime::Ptr<gameobjects::player::Player> actor) {
	AION_UNPORTED();
}

int64_t Storage::increaseItemCount(gameobjects::Item& item, int64_t countValue, services::item::ItemPacketService_ItemUpdateType updateType,
	runtime::Ptr<gameobjects::player::Player> actor) {
	AION_UNPORTED();
}

int64_t Storage::decreaseItemCount(runtime::Ptr<gameobjects::Item> item, int64_t countValue, runtime::Ptr<gameobjects::player::Player> actor) {
	AION_UNPORTED();
}

int64_t Storage::decreaseItemCount(runtime::Ptr<gameobjects::Item> item, int64_t countValue,
	services::item::ItemPacketService_ItemUpdateType updateType, runtime::Ptr<gameobjects::player::Player> actor) {
	AION_UNPORTED();
}

int64_t Storage::decreaseItemCount(runtime::Ptr<gameobjects::Item> item, int64_t countValue,
	services::item::ItemPacketService_ItemUpdateType updateType, std::optional<questEngine::model::QuestStatus> questStatus,
	runtime::Ptr<gameobjects::player::Player> actor) {
	AION_UNPORTED();
}

void Storage::onLoadHandler(gameobjects::Item& item) {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Item> Storage::add(gameobjects::Item& item, runtime::Ptr<gameobjects::player::Player> actor) {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Item> Storage::add(gameobjects::Item& item, services::item::ItemPacketService_ItemAddType addType,
	runtime::Ptr<gameobjects::player::Player> actor) {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Item> Storage::add_CharacterTransfer(gameobjects::Item& item) {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Item> Storage::put(gameobjects::Item& item, runtime::Ptr<gameobjects::player::Player> actor) {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Item> Storage::remove(gameobjects::Item& item) {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Item> Storage::delete_(gameobjects::Item& item, runtime::Ptr<gameobjects::player::Player> actor) {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Item> Storage::delete_(gameobjects::Item& item, services::item::ItemPacketService_ItemDeleteType deleteType,
	runtime::Ptr<gameobjects::player::Player> actor) {
	AION_UNPORTED();
}

bool Storage::decreaseByItemId(int32_t itemId, int64_t countValue, runtime::Ptr<gameobjects::player::Player> actor) {
	AION_UNPORTED();
}

bool Storage::decreaseByItemId(int32_t itemId, int64_t countValue, std::optional<questEngine::model::QuestStatus> questStatus,
	runtime::Ptr<gameobjects::player::Player> actor) {
	AION_UNPORTED();
}

bool Storage::decreaseByObjectId(int32_t itemObjId, int64_t countValue, runtime::Ptr<gameobjects::player::Player> actor) {
	AION_UNPORTED();
}

bool Storage::decreaseByObjectId(int32_t itemObjId, int64_t countValue, questEngine::model::QuestStatus questStatus,
	runtime::Ptr<gameobjects::player::Player> actor) {
	AION_UNPORTED();
}

bool Storage::decreaseByObjectId(int32_t itemObjId, int64_t countValue, services::item::ItemPacketService_ItemUpdateType updateType,
	runtime::Ptr<gameobjects::player::Player> actor) {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Item> Storage::getFirstItemByItemId(int32_t itemId) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<gameobjects::Item>> Storage::getItemsWithKinah() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<gameobjects::Item>> Storage::getItems() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<gameobjects::Item>> Storage::getItemsByItemId(int32_t itemId) {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Item> Storage::getItemByObjId(int32_t itemObjId) {
	AION_UNPORTED();
}

int64_t Storage::getItemCountByItemId(int32_t itemId) {
	AION_UNPORTED();
}

bool Storage::isFull() {
	AION_UNPORTED();
}

bool Storage::isFullSpecialCube() {
	AION_UNPORTED();
}

bool Storage::isFull(int32_t inventory) {
	AION_UNPORTED();
}

int32_t Storage::getFreeSlots(int32_t inventory) {
	AION_UNPORTED();
}

int32_t Storage::getSpecialCubeFreeSlots() {
	AION_UNPORTED();
}

int32_t Storage::getFreeSlots() {
	AION_UNPORTED();
}

void Storage::setLimit(int32_t limit) {
	AION_UNPORTED();
}

int32_t Storage::getLimit() {
	AION_UNPORTED();
}

int32_t Storage::getRowLength() {
	AION_UNPORTED();
}

int32_t Storage::size() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::items::storage
