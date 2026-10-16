#include "aion/gameserver/controllers/movement/CreatureMoveController.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/movement/MovementMask.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MOVE.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::controllers::movement {

CreatureMoveController::CreatureMoveController(model::gameobjects::VisibleObject& ownerValue)
	: OwnedPart(ownerValue), owner(ownerValue), lastMoveUpdate(commons::utils::currentTimeMillis()),
	  started(runtime::Rc<runtime::AtomicBoolean>::create(false)) {
}

CreatureMoveController::~CreatureMoveController() = default;

void CreatureMoveController::setNewDirection(float x, float y, float z, int8_t value) {
	this->heading = value;
	setNewDirection(x, y, z);
}

void CreatureMoveController::setNewDirection(float x, float y, float z) {
	this->targetDestX = x;
	this->targetDestY = y;
	this->targetDestZ = z;
}

void CreatureMoveController::setAndSendStartMove(model::gameobjects::Creature& value) {
	setInMove(true);
	movementMask = MovementMask::NPC_STARTMOVE;
	utils::PacketSendUtility::broadcastToSightedPlayers(value, network::aion::serverpackets::SM_MOVE(value));
}

void CreatureMoveController::setAndSendStopMove(model::gameobjects::Creature& value) {
	setInMove(false);
	movementMask = MovementMask::IMMEDIATE;
	utils::PacketSendUtility::broadcastToSightedPlayers(value, network::aion::serverpackets::SM_MOVE(value));
}

void CreatureMoveController::updateLastMove() {
	lastMoveUpdate = commons::utils::currentTimeMillis();
}

} // namespace aion::gameserver::controllers::movement
