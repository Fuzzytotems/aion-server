#include "aion/gameserver/model/team/legion/LegionWarehouse.h"

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::model::team::legion {

namespace {
/** Java: throw new UnsupportedOperationException("LWH should be used behind proxy") */
[[noreturn]] void behindProxy() {
	throw runtime::UnsupportedOperationException("LWH should be used behind proxy");
}
} // namespace

LegionWarehouse::LegionWarehouse(Legion& legion) : Storage(items::storage::StorageType::LEGION_WAREHOUSE) {
	bindOwner(legion);
	updateLimit(legion.getWarehouseExpansions());
}

LegionWarehouse::~LegionWarehouse() = default;

void LegionWarehouse::increaseKinah(int64_t amount) {
	int32_t currentWhUser = getCurrentUser();
	runtime::Ptr<gameobjects::player::Player> player = currentWhUser == 0 ? nullptr : world::World::getInstance().getPlayer(currentWhUser);
	// Java: new LegionStorageProxy(this, player).increaseKinah(amount), which is storage.increaseKinah(amount, actor). The C++ proxy is a part
	// of its (non-null) acting player, so the call is made directly with the same actor, null when nobody uses the warehouse
	Storage::increaseKinah(amount, player);
}

void LegionWarehouse::increaseKinah(int64_t /*amount*/, services::item::ItemPacketService_ItemUpdateType /*updateType*/) {
	behindProxy();
}

bool LegionWarehouse::tryDecreaseKinah(int64_t /*amount*/) {
	behindProxy();
}

bool LegionWarehouse::tryDecreaseKinah(int64_t /*amount*/, services::item::ItemPacketService_ItemUpdateType /*updateType*/) {
	behindProxy();
}

void LegionWarehouse::decreaseKinah(int64_t /*amount*/) {
	behindProxy();
}

void LegionWarehouse::decreaseKinah(int64_t /*amount*/, services::item::ItemPacketService_ItemUpdateType /*updateType*/) {
	behindProxy();
}

int64_t LegionWarehouse::increaseItemCount(gameobjects::Item& /*item*/, int64_t /*countValue*/) {
	behindProxy();
}

int64_t LegionWarehouse::increaseItemCount(gameobjects::Item& /*item*/, int64_t /*countValue*/,
	services::item::ItemPacketService_ItemUpdateType /*updateType*/) {
	behindProxy();
}

int64_t LegionWarehouse::decreaseItemCount(gameobjects::Item& /*item*/, int64_t /*countValue*/) {
	behindProxy();
}

int64_t LegionWarehouse::decreaseItemCount(gameobjects::Item& /*item*/, int64_t /*countValue*/,
	services::item::ItemPacketService_ItemUpdateType /*updateType*/) {
	behindProxy();
}

int64_t LegionWarehouse::decreaseItemCount(gameobjects::Item& /*item*/, int64_t /*countValue*/,
	services::item::ItemPacketService_ItemUpdateType /*updateType*/, questEngine::model::QuestStatus /*questStatus*/) {
	behindProxy();
}

runtime::Ptr<gameobjects::Item> LegionWarehouse::add(gameobjects::Item& /*item*/) {
	behindProxy();
}

runtime::Ptr<gameobjects::Item> LegionWarehouse::add(gameobjects::Item& /*item*/, services::item::ItemPacketService_ItemAddType /*addType*/) {
	behindProxy();
}

runtime::Ptr<gameobjects::Item> LegionWarehouse::put(gameobjects::Item& /*item*/) {
	behindProxy();
}

runtime::Ptr<gameobjects::Item> LegionWarehouse::delete_(gameobjects::Item& /*item*/) {
	behindProxy();
}

runtime::Ptr<gameobjects::Item> LegionWarehouse::delete_(gameobjects::Item& /*item*/,
	services::item::ItemPacketService_ItemDeleteType /*deleteType*/) {
	behindProxy();
}

bool LegionWarehouse::decreaseByItemId(int32_t /*itemId*/, int64_t /*countValue*/) {
	behindProxy();
}

bool LegionWarehouse::decreaseByItemId(int32_t /*itemId*/, int64_t /*countValue*/, questEngine::model::QuestStatus /*questStatus*/) {
	behindProxy();
}

bool LegionWarehouse::decreaseByObjectId(int32_t /*itemObjId*/, int64_t /*countValue*/) {
	behindProxy();
}

bool LegionWarehouse::decreaseByObjectId(int32_t /*itemObjId*/, int64_t /*countValue*/, questEngine::model::QuestStatus /*questStatus*/) {
	behindProxy();
}

bool LegionWarehouse::decreaseByObjectId(int32_t /*itemObjId*/, int64_t /*countValue*/,
	services::item::ItemPacketService_ItemUpdateType /*updateType*/) {
	behindProxy();
}

void LegionWarehouse::setOwner(runtime::Ptr<gameobjects::player::Player> /*player*/) {
	throw runtime::UnsupportedOperationException("LWH doesnt have owner");
}

bool LegionWarehouse::unsetInUse(int32_t playerObjId) {
	return currentUser.compareAndSet(playerObjId, 0);
}

bool LegionWarehouse::setInUse(int32_t playerObjId) {
	return currentUser.compareAndSet(0, playerObjId);
}

int32_t LegionWarehouse::getCurrentUser() {
	return currentUser.get();
}

void LegionWarehouse::setLimit(int32_t /*limit*/) {
	throw runtime::UnsupportedOperationException("Slot limit is controlled by the expansion level, use updateLimit() instead");
}

void LegionWarehouse::updateLimit(int32_t warehouseExpansions) {
	if (warehouseExpansions < 0)
		throw runtime::IllegalArgumentException("");
	int32_t rows = DEFAULT_ROWS + warehouseExpansions;
	Storage::setLimit(rows * SLOTS_PER_ROW);
}

} // namespace aion::gameserver::model::team::legion
