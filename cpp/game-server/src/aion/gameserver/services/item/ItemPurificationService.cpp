#include "aion/gameserver/services/item/ItemPurificationService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::services::item {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.item.ItemPurificationService");

bool ItemPurificationService::isPurificationAllowed(model::gameobjects::player::Player& player, model::gameobjects::Item& baseItem, int32_t resultItemId) {
	AION_UNPORTED();
}

bool ItemPurificationService::decreaseMaterials(model::gameobjects::player::Player& player, model::gameobjects::Item& baseItem, int32_t resultItemId) {
	AION_UNPORTED();
}

void ItemPurificationService::upgradeItem(model::gameobjects::player::Player& player, model::gameobjects::Item& sourceItem, int32_t targetItemId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::item
