#include "aion/gameserver/services/item/ItemMoveService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::services::item {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.item.ItemMoveService");

void ItemMoveService::moveItem(model::gameobjects::player::Player& player, int32_t itemObjId, int8_t sourceStorageType, int8_t destinationStorageType, int16_t slot) {
	AION_UNPORTED();
}

void ItemMoveService::moveInSameStorage(model::items::storage::IStorage& storage, model::gameobjects::Item& item, int16_t slot) {
	AION_UNPORTED();
}

void ItemMoveService::switchItemsInStorages(model::gameobjects::player::Player& player, int8_t sourceStorageType, int32_t sourceItemObjId, int8_t replaceStorageType, int32_t replaceItemObjId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::item
