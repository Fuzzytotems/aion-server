#include "aion/gameserver/model/items/storage/Storage.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/ItemId.h"
#include "aion/gameserver/model/items/storage/ItemStorage.h"
#include "aion/gameserver/model/items/storage/StorageTypeInfo.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/item/ItemFactory.h"
#include "aion/gameserver/services/item/ItemPacketService.h"
#include "aion/gameserver/services/item/ItemService.h"

namespace aion::gameserver::model::items::storage {

static const auto log = commons::logging::LoggerFactory::getLogger("ITEM_LOG");

namespace {

using services::item::ItemPacketService;
using ItemAddType = services::item::ItemPacketService_ItemAddType;
using ItemDeleteType = services::item::ItemPacketService_ItemDeleteType;
using ItemUpdateType = services::item::ItemPacketService_ItemUpdateType;
using PersistentState = gameobjects::Persistable::PersistentState;

/** Java ItemDeleteType.fromUpdateType(updateType); the enum companion of ItemPacketService belongs to the item services chunk */
ItemDeleteType deleteTypeFromUpdateType(ItemUpdateType updateType) noexcept {
	switch (updateType) {
		case ItemUpdateType::DEC_ITEM_SPLIT:
			return ItemDeleteType::SPLIT;
		case ItemUpdateType::DEC_ITEM_USE:
			return ItemDeleteType::USE;
		case ItemUpdateType::DEC_ITEM_SPLIT_MOVE:
			return ItemDeleteType::MOVE;
		default:
			return ItemDeleteType::DEFAULT;
	}
}

/** Java ItemDeleteType.fromQuestStatus(questStatus) */
ItemDeleteType deleteTypeFromQuestStatus(questEngine::model::QuestStatus questStatus) noexcept {
	switch (questStatus) {
		case questEngine::model::QuestStatus::START:
			return ItemDeleteType::QUEST_START;
		case questEngine::model::QuestStatus::COMPLETE:
			return ItemDeleteType::QUEST_COMPLETE;
		default:
			return ItemDeleteType::DEFAULT;
	}
}

/** Java `StorageType.getStorageTypeById(id)` passed to ItemPacketService, whose Objects.requireNonNull throws for an unknown id */
StorageType requireStorageTypeById(int32_t id) {
	std::optional<StorageType> storageType = getStorageTypeById(id);
	if (!storageType)
		throw runtime::NullPointerException("storageType");
	return *storageType;
}

/** Java string concatenation of a nullable Player */
std::string toJavaString(runtime::Ptr<gameobjects::player::Player> player) {
	return player ? player->toString() : "null";
}

} // namespace

Storage::Storage(StorageType storageTypeValue) : Storage(storageTypeValue, true) {
}

Storage::Storage(StorageType storageTypeValue, bool withDeletedItems)
	: itemStorage(ItemStorage::create(storageTypeValue)), storageType(storageTypeValue) {
	static_cast<void>(withDeletedItems); // Java: deletedItems = withDeletedItems ? new ConcurrentLinkedQueue<>() : null (the C++ queue always exists)
}

Storage::~Storage() = default;

int64_t Storage::getKinah() {
	return !kinahItem.get() ? 0 : kinahItem->getItemCount();
}

void Storage::increaseKinah(int64_t amount, runtime::Ptr<gameobjects::player::Player> actor) {
	increaseKinah(amount, ItemUpdateType::INC_KINAH_COLLECT, actor);
}

void Storage::increaseKinah(int64_t amount, ItemUpdateType updateType, runtime::Ptr<gameobjects::player::Player> actor) {
	if (!kinahItem.get()) {
		add(*services::item::ItemFactory::newItem(ItemId::KINAH, 0), actor);
	}
	if (amount > 0) {
		increaseItemCount(*kinahItem.get(), amount, updateType, actor);
	}
}

bool Storage::tryDecreaseKinah(int64_t amount, runtime::Ptr<gameobjects::player::Player> actor) {
	if (getKinah() >= amount) {
		decreaseKinah(amount, actor);
		return true;
	}
	return false;
}

bool Storage::tryDecreaseKinah(int64_t amount, ItemUpdateType updateType, runtime::Ptr<gameobjects::player::Player> actor) {
	if (getKinah() >= amount) {
		decreaseKinah(amount, updateType, actor);
		return true;
	}
	return false;
}

void Storage::decreaseKinah(int64_t amount, runtime::Ptr<gameobjects::player::Player> actor) {
	decreaseKinah(amount, ItemUpdateType::DEC_KINAH_BUY, actor);
}

void Storage::decreaseKinah(int64_t amount, ItemUpdateType updateType, runtime::Ptr<gameobjects::player::Player> actor) {
	if (amount > 0) {
		decreaseItemCount(kinahItem.get(), amount, updateType, actor);
	}
}

int64_t Storage::increaseItemCount(gameobjects::Item& item, int64_t countValue, runtime::Ptr<gameobjects::player::Player> actor) {
	return increaseItemCount(item, countValue, ItemUpdateType::DEC_ITEM_USE, actor);
}

int64_t Storage::increaseItemCount(gameobjects::Item& item, int64_t countValue, ItemUpdateType updateType,
	runtime::Ptr<gameobjects::player::Player> actor) {
	int64_t leftCount = item.increaseItemCount(countValue);
	if (actor)
		ItemPacketService::sendItemPacket(*actor, storageType, item, updateType);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
	return leftCount;
}

int64_t Storage::decreaseItemCount(runtime::Ptr<gameobjects::Item> item, int64_t countValue, runtime::Ptr<gameobjects::player::Player> actor) {
	return decreaseItemCount(item, countValue, ItemUpdateType::DEC_ITEM_USE, actor);
}

int64_t Storage::decreaseItemCount(runtime::Ptr<gameobjects::Item> item, int64_t countValue, ItemUpdateType updateType,
	runtime::Ptr<gameobjects::player::Player> actor) {
	return decreaseItemCount(item, countValue, updateType, std::nullopt, actor);
}

int64_t Storage::decreaseItemCount(runtime::Ptr<gameobjects::Item> item, int64_t countValue, ItemUpdateType updateType,
	std::optional<questEngine::model::QuestStatus> questStatus, runtime::Ptr<gameobjects::player::Player> actor) {
	if (!item)
		return 0;

	ItemDeleteType deleteType = questStatus ? deleteTypeFromQuestStatus(*questStatus) : deleteTypeFromUpdateType(updateType);
	int64_t leftCount = item->decreaseItemCount(countValue);
	bool isKinah = item->getItemTemplate()->isKinah();
	if (item->getItemCount() <= 0 && !isKinah)
		delete_(*item, deleteType, actor);
	else
		ItemPacketService::sendItemPacket(*actor, storageType, *item, updateType);

	setPersistentState(PersistentState::UPDATE_REQUIRED);
	return leftCount;
}

void Storage::onLoadHandler(gameobjects::Item& item) {
	if (item.getItemTemplate()->isKinah())
		kinahItem.set(runtime::Ref<gameobjects::Item>(item));
	else
		itemStorage->putItem(item);
}

runtime::Ptr<gameobjects::Item> Storage::add(gameobjects::Item& item, runtime::Ptr<gameobjects::player::Player> actor) {
	return add(item, services::item::ItemService::DEFAULT_UPDATE_PREDICATE->getAddType(), actor);
}

runtime::Ptr<gameobjects::Item> Storage::add(gameobjects::Item& item, ItemAddType addType, runtime::Ptr<gameobjects::player::Player> actor) {
	if (item.getItemTemplate()->isKinah()) {
		this->kinahItem.set(runtime::Ref<gameobjects::Item>(item));
	} else if (!itemStorage->putItem(item)) {
		return nullptr;
	}
	item.setItemLocation(getId(storageType));
	setPersistentState(PersistentState::UPDATE_REQUIRED);
	if (actor) {
		ItemPacketService::sendStorageUpdatePacket(*actor, storageType, item, addType);
		if (storageType == StorageType::CUBE)
			questEngine::QuestEngine::getInstance().onItemGet(*actor, item.getItemId());
	}
	return item;
}

runtime::Ptr<gameobjects::Item> Storage::add_CharacterTransfer(gameobjects::Item& item) {
	if (item.getItemTemplate()->isKinah()) {
		this->kinahItem.set(runtime::Ref<gameobjects::Item>(item));
	} else if (!itemStorage->putItem(item)) {
		return nullptr;
	}
	item.setItemLocation(getId(storageType));
	setPersistentState(PersistentState::UPDATE_REQUIRED);
	return item;
}

runtime::Ptr<gameobjects::Item> Storage::put(gameobjects::Item& item, runtime::Ptr<gameobjects::player::Player> actor) {
	if (!itemStorage->putItem(item)) {
		return nullptr;
	}
	item.setItemLocation(getId(storageType));
	setPersistentState(PersistentState::UPDATE_REQUIRED);
	ItemPacketService::sendItemUpdatePacket(*actor, storageType, item, ItemUpdateType::EQUIP_UNEQUIP);
	return item;
}

runtime::Ptr<gameobjects::Item> Storage::remove(gameobjects::Item& item) {
	return itemStorage->removeItem(item.getObjectId());
}

runtime::Ptr<gameobjects::Item> Storage::delete_(gameobjects::Item& item, runtime::Ptr<gameobjects::player::Player> actor) {
	return delete_(item, ItemDeleteType::DEFAULT, actor);
}

runtime::Ptr<gameobjects::Item> Storage::delete_(gameobjects::Item& item, ItemDeleteType deleteType,
	runtime::Ptr<gameobjects::player::Player> actor) {
	if (remove(item)) {
		item.setPersistentState(PersistentState::DELETED);
		deletedItems.add(runtime::Ref<gameobjects::Item>(item));
		setPersistentState(PersistentState::UPDATE_REQUIRED);
		gameobjects::player::Player& player = *actor; // Java: PacketSendUtility dereferences a null actor (NullPointerException)
		ItemPacketService::sendItemDeletePacket(player, requireStorageTypeById(item.getItemLocation()), item, deleteType);
		if (configs::main::LoggingConfig::LOG_ITEM.load() && !item.getItemTemplate()->isKinah() && item.getItemCount() > 0) {
			std::string name = (item.getEnchantLevel() > 0 ? "+" + std::to_string(item.getEnchantLevel()) + " " : "") + item.getItemName();
			log.info("Deleted " + std::to_string(item.getItemId()) + " " + name + " from " + toJavaString(actor) + " (count: "
				+ std::to_string(item.getItemCount()) + ") (deletion type: " + std::string(xml::enumName(deleteType)) + ")");
		}
		questEngine::QuestEngine::getInstance().onItemRemoved(player, item.getItemId());
		return item;
	}
	return nullptr;
}

bool Storage::decreaseByItemId(int32_t itemId, int64_t countValue, runtime::Ptr<gameobjects::player::Player> actor) {
	return decreaseByItemId(itemId, countValue, std::nullopt, actor);
}

bool Storage::decreaseByItemId(int32_t itemId, int64_t countValue, std::optional<questEngine::model::QuestStatus> questStatus,
	runtime::Ptr<gameobjects::player::Player> actor) {
	std::vector<runtime::Ptr<gameobjects::Item>> items = itemStorage->getItemsById(itemId);
	if (items.size() == 0)
		return false;

	for (runtime::Ptr<gameobjects::Item> item : items) {
		if (countValue == 0) {
			break;
		}
		countValue = decreaseItemCount(item, countValue, ItemUpdateType::DEC_ITEM_USE, questStatus, actor);
	}

	return countValue == 0;
}

bool Storage::decreaseByObjectId(int32_t itemObjId, int64_t countValue, runtime::Ptr<gameobjects::player::Player> actor) {
	return decreaseByObjectId(itemObjId, countValue, ItemUpdateType::DEC_ITEM_USE, actor);
}

bool Storage::decreaseByObjectId(int32_t itemObjId, int64_t countValue, questEngine::model::QuestStatus questStatus,
	runtime::Ptr<gameobjects::player::Player> actor) {
	runtime::Ptr<gameobjects::Item> item = itemStorage->getItemByObjId(itemObjId);
	if (!item || item->getItemCount() < countValue)
		return false;

	return decreaseItemCount(item, countValue, ItemUpdateType::DEC_ITEM_USE, questStatus, actor) == 0;
}

bool Storage::decreaseByObjectId(int32_t itemObjId, int64_t countValue, ItemUpdateType updateType,
	runtime::Ptr<gameobjects::player::Player> actor) {
	runtime::Ptr<gameobjects::Item> item = itemStorage->getItemByObjId(itemObjId);
	if (!item || item->getItemCount() < countValue)
		return false;

	return decreaseItemCount(item, countValue, updateType, actor) == 0;
}

runtime::Ptr<gameobjects::Item> Storage::getFirstItemByItemId(int32_t itemId) {
	return this->itemStorage->getFirstItemById(itemId);
}

std::vector<runtime::Ptr<gameobjects::Item>> Storage::getItemsWithKinah() {
	std::vector<runtime::Ptr<gameobjects::Item>> items = this->itemStorage->getItems();
	if (this->kinahItem.get()) {
		items.push_back(this->kinahItem.get());
	}
	return items;
}

std::vector<runtime::Ptr<gameobjects::Item>> Storage::getItems() {
	return this->itemStorage->getItems();
}

std::vector<runtime::Ptr<gameobjects::Item>> Storage::getItemsByItemId(int32_t itemId) {
	return this->itemStorage->getItemsById(itemId);
}

runtime::Ptr<gameobjects::Item> Storage::getItemByObjId(int32_t itemObjId) {
	return this->itemStorage->getItemByObjId(itemObjId);
}

int64_t Storage::getItemCountByItemId(int32_t itemId) {
	std::vector<runtime::Ptr<gameobjects::Item>> temp = this->itemStorage->getItemsById(itemId);
	if (temp.size() == 0)
		return 0;

	int64_t cnt = 0;
	for (runtime::Ptr<gameobjects::Item> item : temp)
		cnt += item->getItemCount();

	return cnt;
}

bool Storage::isFull() {
	return this->itemStorage->isFull();
}

bool Storage::isFullSpecialCube() {
	return this->itemStorage->isFullSpecialCube();
}

bool Storage::isFull(int32_t inventory) {
	if (inventory > 0) {
		return isFullSpecialCube();
	}
	return isFull();
}

int32_t Storage::getFreeSlots(int32_t inventory) {
	if (inventory > 0) {
		return getSpecialCubeFreeSlots();
	}
	return getFreeSlots();
}

int32_t Storage::getSpecialCubeFreeSlots() {
	return this->itemStorage->getSpecialCubeFreeSlots();
}

int32_t Storage::getFreeSlots() {
	return this->itemStorage->getFreeSlots();
}

void Storage::setLimit(int32_t value) {
	itemStorage->setLimit(value);
}

int32_t Storage::getLimit() {
	return this->itemStorage->getLimit();
}

int32_t Storage::getRowLength() {
	return this->itemStorage->getRowLength();
}

int32_t Storage::size() {
	return itemStorage->size();
}

} // namespace aion::gameserver::model::items::storage
