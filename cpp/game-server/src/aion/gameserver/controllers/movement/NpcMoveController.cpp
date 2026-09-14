#include "aion/gameserver/controllers/movement/NpcMoveController.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Npc.h"

namespace aion::gameserver::controllers::movement {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.controllers.movement.NpcMoveController");

NpcMoveController::NpcMoveController(model::gameobjects::Npc& ownerValue) : CreatureMoveController(ownerValue) {
}

NpcMoveController::~NpcMoveController() = default;

void NpcMoveController::moveToTargetObject() {
	AION_UNPORTED();
}

bool NpcMoveController::moveToPoint(float x, float y, float z) {
	AION_UNPORTED();
}

void NpcMoveController::forcedMoveToPoint(float x, float y, float z) {
	AION_UNPORTED();
}

void NpcMoveController::moveToNextPoint() {
	AION_UNPORTED();
}

void NpcMoveController::moveToDestination() {
	AION_UNPORTED();
}

bool NpcMoveController::isOnGround(model::gameobjects::Creature& creature) {
	AION_UNPORTED();
}

bool NpcMoveController::trySetValidGeoPoint(float targetX, float targetY) {
	AION_UNPORTED();
}

void NpcMoveController::moveToLocation(float targetX, float targetY, float targetZ) {
	AION_UNPORTED();
}

int8_t NpcMoveController::getMoveMask(bool directionChanged) {
	AION_UNPORTED();
}

void NpcMoveController::abortMove() {
	AION_UNPORTED();
}

void NpcMoveController::resetMove() {
	AION_UNPORTED();
}

void NpcMoveController::setWalkerTemplate(const model::templates::walker::WalkerTemplate* value, int32_t stepIndex) {
	AION_UNPORTED();
}

void NpcMoveController::setRouteStep(const model::templates::walker::RouteStep* step) {
	AION_UNPORTED();
}

bool NpcMoveController::isReachedPoint() {
	AION_UNPORTED();
}

bool NpcMoveController::isNextRouteStepChosen() {
	AION_UNPORTED();
}

bool NpcMoveController::isChangingDirection() {
	AION_UNPORTED();
}

float NpcMoveController::getTargetX2() {
	AION_UNPORTED();
}

float NpcMoveController::getTargetY2() {
	AION_UNPORTED();
}

float NpcMoveController::getTargetZ2() {
	AION_UNPORTED();
}

void NpcMoveController::tryStoreStep(float x, float y, float z) {
	AION_UNPORTED();
}

void NpcMoveController::returnToLastStepOrSpawn() {
	AION_UNPORTED();
}

void NpcMoveController::clearBackSteps() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::controllers::movement
