#include "aion/gameserver/controllers/movement/CreatureMoveController.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"

namespace aion::gameserver::controllers::movement {

CreatureMoveController::CreatureMoveController(model::gameobjects::VisibleObject& ownerValue)
	: OwnedPart(ownerValue), owner(ownerValue), lastMoveUpdate(commons::utils::currentTimeMillis()),
	  started(runtime::Rc<runtime::AtomicBoolean>::create(false)) {
}

CreatureMoveController::~CreatureMoveController() = default;

void CreatureMoveController::setNewDirection(float x, float y, float z, int8_t value) {
	AION_UNPORTED();
}

void CreatureMoveController::setNewDirection(float x, float y, float z) {
	AION_UNPORTED();
}

void CreatureMoveController::setAndSendStartMove(model::gameobjects::Creature& value) {
	AION_UNPORTED();
}

void CreatureMoveController::setAndSendStopMove(model::gameobjects::Creature& value) {
	AION_UNPORTED();
}

void CreatureMoveController::updateLastMove() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::controllers::movement
