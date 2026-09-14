#include "aion/gameserver/services/item/ItemFactory.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::services::item {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.item.ItemFactory");

runtime::Ref<model::gameobjects::Item> ItemFactory::newItem(int32_t itemId) {
	AION_UNPORTED();
}

runtime::Ref<model::gameobjects::Item> ItemFactory::newItem(int32_t itemId, int64_t count) {
	AION_UNPORTED();
}

int64_t ItemFactory::calculateCount(const model::templates::item::ItemTemplate* itemTemplate, int64_t count) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::item
