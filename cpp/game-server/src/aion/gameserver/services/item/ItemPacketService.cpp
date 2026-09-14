#include "aion/gameserver/services/item/ItemPacketService.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services::item {

void ItemPacketService::updateItemAfterInfoChange(model::gameobjects::player::Player& player, model::gameobjects::Item& item) {
	AION_UNPORTED();
}

void ItemPacketService::updateItemAfterInfoChange(model::gameobjects::player::Player& player, model::gameobjects::Item& item, ItemPacketService::ItemUpdateType updateType) {
	AION_UNPORTED();
}

void ItemPacketService::updateItemAfterEquip(model::gameobjects::player::Player& player, model::gameobjects::Item& item) {
	AION_UNPORTED();
}

void ItemPacketService::sendItemPacket(model::gameobjects::player::Player& player, model::items::storage::StorageType storageType, model::gameobjects::Item& item, ItemPacketService::ItemUpdateType updateType) {
	AION_UNPORTED();
}

void ItemPacketService::sendItemDeletePacket(model::gameobjects::player::Player& player, model::items::storage::StorageType storageType, model::gameobjects::Item& item, ItemPacketService::ItemDeleteType deleteType) {
	AION_UNPORTED();
}

void ItemPacketService::sendItemUpdatePacket(model::gameobjects::player::Player& player, model::items::storage::StorageType storageType, model::gameobjects::Item& item, ItemPacketService::ItemUpdateType updateType) {
	AION_UNPORTED();
}

void ItemPacketService::sendStorageUpdatePacket(model::gameobjects::player::Player& player, model::items::storage::StorageType storageType, model::gameobjects::Item& item) {
	AION_UNPORTED();
}

void ItemPacketService::sendStorageUpdatePacket(model::gameobjects::player::Player& player, model::items::storage::StorageType storageType, model::gameobjects::Item& item, ItemPacketService::ItemAddType addType) {
	AION_UNPORTED();
}

void ItemPacketService::sendItemUnlockPacket(model::gameobjects::player::Player& player, model::gameobjects::Item& item) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::item
