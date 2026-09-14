#include "aion/gameserver/controllers/movement/PlayableMoveController.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Creature.h"

namespace aion::gameserver::controllers::movement {

PlayableMoveController::PlayableMoveController(model::gameobjects::Creature& ownerValue) : CreatureMoveController(ownerValue) {
}

PlayableMoveController::~PlayableMoveController() = default;

void PlayableMoveController::startMovingToDestination() {
	AION_UNPORTED();
}

bool PlayableMoveController::isControlled() {
	AION_UNPORTED();
}

void PlayableMoveController::sendForcedMovePacket() {
	AION_UNPORTED();
}

void PlayableMoveController::moveToDestination() {
	AION_UNPORTED();
}

void PlayableMoveController::abortMove() {
	AION_UNPORTED();
}

void PlayableMoveController::setNewDirection(float x, float y, float z) {
	AION_UNPORTED();
}

PlayableMoveController::MovementModifierDirection PlayableMoveController::getMovementDirection() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::controllers::movement
