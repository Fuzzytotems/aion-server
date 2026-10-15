#include "aion/gameserver/model/gameobjects/JukeBoxObject.h"

#include "aion/gameserver/model/house/HouseRegistry.h"
#include "aion/gameserver/model/templates/housing/HousingJukeBox.h"

namespace aion::gameserver::model::gameobjects {

JukeBoxObject::JukeBoxObject(CreateKey key, house::HouseRegistry& registry, int32_t objId, int32_t templateId)
	: HouseObject(key, registry, objId, templateId) {
}

JukeBoxObject::~JukeBoxObject() = default;

const templates::housing::HousingJukeBox* JukeBoxObject::getObjectTemplate() const {
	return static_cast<const templates::housing::HousingJukeBox*>(HouseObject::getObjectTemplate());
}

} // namespace aion::gameserver::model::gameobjects
