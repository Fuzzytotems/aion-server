#include "aion/gameserver/model/gameobjects/PassiveObject.h"

#include "aion/gameserver/model/house/HouseRegistry.h"
#include "aion/gameserver/model/templates/housing/HousingPassiveItem.h"

namespace aion::gameserver::model::gameobjects {

PassiveObject::PassiveObject(CreateKey key, house::HouseRegistry& registry, int32_t objId, int32_t templateId)
	: HouseObject(key, registry, objId, templateId) {
}

PassiveObject::~PassiveObject() = default;

const templates::housing::HousingPassiveItem* PassiveObject::getObjectTemplate() const {
	return static_cast<const templates::housing::HousingPassiveItem*>(HouseObject::getObjectTemplate());
}

} // namespace aion::gameserver::model::gameobjects
