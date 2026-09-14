#include "aion/gameserver/services/item/ItemRestrictionService.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services::item {

bool ItemRestrictionService::isItemRestrictedFrom(model::gameobjects::player::Player& player, model::gameobjects::Item& item, model::items::storage::StorageType storageType) {
	AION_UNPORTED();
}

bool ItemRestrictionService::isItemRestrictedTo(model::gameobjects::player::Player& player, model::gameobjects::Item& item, model::items::storage::StorageType storageType) {
	AION_UNPORTED();
}

bool ItemRestrictionService::canRemoveItem(model::gameobjects::player::Player& player, model::gameobjects::Item& item) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::item
