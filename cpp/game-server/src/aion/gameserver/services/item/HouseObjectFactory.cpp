#include "aion/gameserver/services/item/HouseObjectFactory.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services::item {

runtime::Ref<model::gameobjects::HouseObject> HouseObjectFactory::createNew(model::house::HouseRegistry& registry, int32_t objectId, int32_t objectTemplateId) {
	AION_UNPORTED();
}

runtime::Ref<model::gameobjects::HouseObject> HouseObjectFactory::createNew(model::house::House& house, const model::templates::item::ItemTemplate* itemTemplate) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::item
