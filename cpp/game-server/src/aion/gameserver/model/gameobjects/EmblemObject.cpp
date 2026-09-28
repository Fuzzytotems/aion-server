#include "aion/gameserver/model/gameobjects/EmblemObject.h"

#include "aion/gameserver/model/house/HouseRegistry.h"
#include "aion/gameserver/model/templates/housing/HousingEmblem.h"

namespace aion::gameserver::model::gameobjects {

EmblemObject::EmblemObject(CreateKey key, house::HouseRegistry& registry, int32_t objId, int32_t templateId)
	: HouseObject(key, registry, objId, templateId) {
}

EmblemObject::~EmblemObject() = default;

const templates::housing::HousingEmblem* EmblemObject::getObjectTemplate() const {
	return static_cast<const templates::housing::HousingEmblem*>(HouseObject::getObjectTemplate());
}

bool EmblemObject::canExpireNow() {
	return false;
}

} // namespace aion::gameserver::model::gameobjects
