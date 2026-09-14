#include "aion/gameserver/controllers/movement/PlayerMoveController.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::controllers::movement {

PlayerMoveController::PlayerMoveController(model::gameobjects::player::Player& ownerValue) : PlayableMoveController(ownerValue) {
}

PlayerMoveController::~PlayerMoveController() = default;

void PlayerMoveController::abortMove() {
	AION_UNPORTED();
}

void PlayerMoveController::resetLastPositionFromClient() {
	lastPositionFromClient.set(nullptr); // Java: lastPositionFromClient = null
}

void PlayerMoveController::onMoveFromClient() {
	AION_UNPORTED();
}

void PlayerMoveController::resetToLastPositionFromClient() {
	AION_UNPORTED();
}

void PlayerMoveController::updateFalling(float newZ) {
	AION_UNPORTED();
}

void PlayerMoveController::stopFalling(float newZ) {
	AION_UNPORTED();
}

void PlayerMoveController::setHasMovedByRandomMoveLocEffect(skillengine::model::Skill& skill) {
	AION_UNPORTED();
}

bool PlayerMoveController::hasMovedByRandomMoveLocEffect() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::controllers::movement
