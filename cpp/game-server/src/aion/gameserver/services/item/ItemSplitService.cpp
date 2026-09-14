#include "aion/gameserver/services/item/ItemSplitService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::services::item {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.item.ItemSplitService");

void ItemSplitService::splitItem(model::gameobjects::player::Player& player, int32_t itemObjId, int32_t destinationObjId, int64_t splitAmount, int16_t slotNum, int8_t sourceStorageType, int8_t destinationStorageType) {
	AION_UNPORTED();
}

void ItemSplitService::mergeStacks(model::items::storage::IStorage& sourceStorage, model::items::storage::IStorage& destStorage, model::gameobjects::Item& sourceItem, model::gameobjects::Item& targetItem, int64_t count) {
	AION_UNPORTED();
}

void ItemSplitService::moveKinah(model::gameobjects::player::Player& player, model::items::storage::IStorage& source, int64_t splitAmount) {
	AION_UNPORTED();
}

void ItemSplitService::updateKinahCount(model::items::storage::IStorage& source, int64_t splitAmount, model::items::storage::IStorage& destination) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::item
