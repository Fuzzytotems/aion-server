#include "aion/gameserver/controllers/PlaceableObjectController.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"

namespace aion::gameserver::controllers {

void PlaceableObjectController::onDespawn() {
	AION_UNPORTED();
}

void PlaceableObjectController::onDialogRequest(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlaceableObjectController::notKnow(model::gameobjects::VisibleObject& object) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::controllers
