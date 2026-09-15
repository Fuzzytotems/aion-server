#include "aion/gameserver/controllers/StaticObjectController.h"

#include "aion/gameserver/model/gameobjects/StaticObject.h"

namespace aion::gameserver::controllers {

StaticObjectController::StaticObjectController() = default;

StaticObjectController::~StaticObjectController() = default;

model::gameobjects::StaticObject& StaticObjectController::getOwner() const {
	return static_cast<model::gameobjects::StaticObject&>(VisibleObjectController::getOwner());
}

} // namespace aion::gameserver::controllers
