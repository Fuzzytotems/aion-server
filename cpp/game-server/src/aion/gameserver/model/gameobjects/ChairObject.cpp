#include "aion/gameserver/model/gameobjects/ChairObject.h"

#include "aion/gameserver/model/house/HouseRegistry.h"
#include "aion/gameserver/model/templates/housing/HousingChair.h"

namespace aion::gameserver::model::gameobjects {

ChairObject::ChairObject(CreateKey key, house::HouseRegistry& registry, int32_t objId, int32_t templateId)
	: HouseObject(key, registry, objId, templateId) {
}

ChairObject::~ChairObject() = default;

const templates::housing::HousingChair* ChairObject::getObjectTemplate() const {
	return static_cast<const templates::housing::HousingChair*>(HouseObject::getObjectTemplate());
}

void ChairObject::onUse(player::Player& player) {
}

} // namespace aion::gameserver::model::gameobjects
