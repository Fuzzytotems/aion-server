#include "aion/gameserver/model/gameobjects/MoveableObject.h"

#include "aion/gameserver/model/house/HouseRegistry.h"
#include "aion/gameserver/model/templates/housing/HousingMoveableItem.h"

namespace aion::gameserver::model::gameobjects {

MoveableObject::MoveableObject(CreateKey key, house::HouseRegistry& registry, int32_t objId, int32_t templateId)
	: HouseObject(key, registry, objId, templateId) {
}

MoveableObject::~MoveableObject() = default;

const templates::housing::HousingMoveableItem* MoveableObject::getObjectTemplate() const {
	return static_cast<const templates::housing::HousingMoveableItem*>(HouseObject::getObjectTemplate());
}

void MoveableObject::onUse(player::Player& player) {
}

} // namespace aion::gameserver::model::gameobjects
