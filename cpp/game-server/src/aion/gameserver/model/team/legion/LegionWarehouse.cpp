#include "aion/gameserver/model/team/legion/LegionWarehouse.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/team/legion/Legion.h"

namespace aion::gameserver::model::team::legion {

LegionWarehouse::LegionWarehouse(Legion& legion) : Storage(items::storage::StorageType::LEGION_WAREHOUSE) {
	bindOwner(legion);
	// Java: updateLimit(legion.getWarehouseExpansions())
	AION_UNPORTED();
}

LegionWarehouse::~LegionWarehouse() = default;

void LegionWarehouse::increaseKinah(int64_t amount) {
	AION_UNPORTED();
}

void LegionWarehouse::increaseKinah(int64_t amount, services::item::ItemPacketService_ItemUpdateType updateType) {
	AION_UNPORTED();
}

bool LegionWarehouse::tryDecreaseKinah(int64_t amount) {
	AION_UNPORTED();
}

bool LegionWarehouse::tryDecreaseKinah(int64_t amount, services::item::ItemPacketService_ItemUpdateType updateType) {
	AION_UNPORTED();
}

void LegionWarehouse::decreaseKinah(int64_t amount) {
	AION_UNPORTED();
}

void LegionWarehouse::decreaseKinah(int64_t amount, services::item::ItemPacketService_ItemUpdateType updateType) {
	AION_UNPORTED();
}

int64_t LegionWarehouse::increaseItemCount(gameobjects::Item& item, int64_t countValue) {
	AION_UNPORTED();
}

int64_t LegionWarehouse::increaseItemCount(gameobjects::Item& item, int64_t countValue, services::item::ItemPacketService_ItemUpdateType updateType) {
	AION_UNPORTED();
}

int64_t LegionWarehouse::decreaseItemCount(gameobjects::Item& item, int64_t countValue) {
	AION_UNPORTED();
}

int64_t LegionWarehouse::decreaseItemCount(gameobjects::Item& item, int64_t countValue, services::item::ItemPacketService_ItemUpdateType updateType) {
	AION_UNPORTED();
}

int64_t LegionWarehouse::decreaseItemCount(gameobjects::Item& item, int64_t countValue,
	services::item::ItemPacketService_ItemUpdateType updateType, questEngine::model::QuestStatus questStatus) {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Item> LegionWarehouse::add(gameobjects::Item& item) {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Item> LegionWarehouse::add(gameobjects::Item& item, services::item::ItemPacketService_ItemAddType addType) {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Item> LegionWarehouse::put(gameobjects::Item& item) {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Item> LegionWarehouse::delete_(gameobjects::Item& item) {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Item> LegionWarehouse::delete_(gameobjects::Item& item, services::item::ItemPacketService_ItemDeleteType deleteType) {
	AION_UNPORTED();
}

bool LegionWarehouse::decreaseByItemId(int32_t itemId, int64_t countValue) {
	AION_UNPORTED();
}

bool LegionWarehouse::decreaseByItemId(int32_t itemId, int64_t countValue, questEngine::model::QuestStatus questStatus) {
	AION_UNPORTED();
}

bool LegionWarehouse::decreaseByObjectId(int32_t itemObjId, int64_t countValue) {
	AION_UNPORTED();
}

bool LegionWarehouse::decreaseByObjectId(int32_t itemObjId, int64_t countValue, questEngine::model::QuestStatus questStatus) {
	AION_UNPORTED();
}

bool LegionWarehouse::decreaseByObjectId(int32_t itemObjId, int64_t countValue, services::item::ItemPacketService_ItemUpdateType updateType) {
	AION_UNPORTED();
}

void LegionWarehouse::setOwner(runtime::Ptr<gameobjects::player::Player> player) {
	AION_UNPORTED();
}

bool LegionWarehouse::unsetInUse(int32_t playerObjId) {
	AION_UNPORTED();
}

bool LegionWarehouse::setInUse(int32_t playerObjId) {
	AION_UNPORTED();
}

int32_t LegionWarehouse::getCurrentUser() {
	AION_UNPORTED();
}

void LegionWarehouse::setLimit(int32_t limit) {
	AION_UNPORTED();
}

void LegionWarehouse::updateLimit(int32_t warehouseExpansions) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::team::legion
