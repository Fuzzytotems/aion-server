#include "aion/gameserver/services/item/ItemFactory.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"

namespace aion::gameserver::services::item {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.item.ItemFactory");

runtime::Ref<model::gameobjects::Item> ItemFactory::newItem(int32_t itemId) {
	const model::templates::item::ItemTemplate* itemTemplate = dataholders::DataManager::ITEM_DATA->getItemTemplate(itemId);
	if (itemTemplate == nullptr) {
		log.error("Item was not populated correctly. Item template is missing for item id: " + std::to_string(itemId));
		return nullptr;
	}
	return model::gameobjects::Item::create(utils::idfactory::IDFactory::getInstance().nextId(), itemTemplate);
}

runtime::Ref<model::gameobjects::Item> ItemFactory::newItem(int32_t itemId, int64_t count) {
	runtime::Ref<model::gameobjects::Item> item = newItem(itemId);
	item->setItemCount(calculateCount(item->getItemTemplate(), count)); // Java: NullPointerException for a missing template
	return item;
}

int64_t ItemFactory::calculateCount(const model::templates::item::ItemTemplate* itemTemplate, int64_t count) {
	int64_t maxStackCount = itemTemplate->getMaxStackCount();
	if (count > maxStackCount && !itemTemplate->isKinah())
		count = maxStackCount;
	return count;
}

} // namespace aion::gameserver::services::item
