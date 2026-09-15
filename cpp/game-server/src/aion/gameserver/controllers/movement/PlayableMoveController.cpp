#include "aion/gameserver/controllers/movement/PlayableMoveController.h"

#include <algorithm>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/ControllerStandIns.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/movement/MovementMask.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MOVE.h"
#include "aion/gameserver/taskmanager/tasks/PlayerMoveTaskManager.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::controllers::movement {

using model::gameobjects::Creature;
using utils::PositionUtil;

PlayableMoveController::PlayableMoveController(model::gameobjects::Creature& ownerValue) : CreatureMoveController(ownerValue) {
}

PlayableMoveController::~PlayableMoveController() = default;

void PlayableMoveController::startMovingToDestination() {
	Creature& creature = static_cast<Creature&>(owner);
	updateLastMove();
	if (creature.canPerformMove()) {
		if (isControlled() && started->compareAndSet(false, true)) {
			this->movementMask = MovementMask::NPC_STARTMOVE;
			sendForcedMovePacket();
			taskmanager::tasks::PlayerMoveTaskManager::getInstance().addPlayer(creature);
		}
	}
}

bool PlayableMoveController::isControlled() {
	Creature& creature = static_cast<Creature&>(owner);
	return creature.getEffectController()->isUnderFear() || creature.getEffectController()->isConfused();
}

void PlayableMoveController::sendForcedMovePacket() {
	Creature& creature = static_cast<Creature&>(owner);
	utils::PacketSendUtility::broadcastPacketAndReceive(creature, network::aion::serverpackets::SM_MOVE(creature));
	sendMovePacket = false;
}

void PlayableMoveController::moveToDestination() {
	Creature& creature = static_cast<Creature&>(owner);
	if (!creature.canPerformMove()) {
		if (started->compareAndSet(true, false)) {
			setAndSendStopMove(creature);
			updateLastMove();
		}
		return;
	}

	if (sendMovePacket.get() && isControlled()) {
		sendForcedMovePacket();
	}

	float x = creature.getX();
	float y = creature.getY();
	float z = creature.getZ();

	float dist = static_cast<float>(PositionUtil::getDistance(x, y, z, targetDestX.get(), targetDestY.get(), targetDestZ.get()));
	if (dist < 0.01f)
		return;

	float currentSpeed = standins::statFunctionsAdjustSpeedByMovementModifier(creature, creature.getGameStats()->getMovementSpeedFloat());
	int64_t msElapsed = commons::utils::currentTimeMillis() - lastMoveUpdate.get();
	float futureXYDistPassed = std::min(currentSpeed * static_cast<float>(msElapsed) / 1000.0f, dist);
	float futureZDistPassed = isJumping() ? std::min(2 * static_cast<float>(msElapsed) / 1000.0f, dist) : futureXYDistPassed;

	float distXYFraction = futureXYDistPassed / dist;
	float distZFraction = isJumping() ? futureZDistPassed / dist : distXYFraction;
	float newX = (targetDestX.get() - x) * distXYFraction + x;
	float newY = (targetDestY.get() - y) * distXYFraction + y;
	float newZ = (targetDestZ.get() - z) * distZFraction + z;

	/*
	 * if ((movementMask & MovementMask.MOUSE) == 0) { targetDestX = newX + vectorX; targetDestY = newY + vectorY; targetDestZ = newZ + vectorZ; }
	 */

	world::World::getInstance().updatePosition(creature, newX, newY, newZ, heading.get(), false);
	updateLastMove();
}

void PlayableMoveController::abortMove() {
	Creature& creature = static_cast<Creature&>(owner);
	started->set(false);
	taskmanager::tasks::PlayerMoveTaskManager::getInstance().removePlayer(creature);
	targetDestX = 0;
	targetDestY = 0;
	targetDestZ = 0;
	setAndSendStopMove(creature);
}

void PlayableMoveController::setNewDirection(float x, float y, float z) {
	if (targetDestX.get() != x || targetDestY.get() != y || targetDestZ.get() != z) {
		sendMovePacket = true;
	}
	CreatureMoveController::setNewDirection(x, y, z);

	float relativeMovementAngle = PositionUtil::calculateAngleTowards(owner.getX(), owner.getY(), heading.get(), targetDestX.get(), targetDestY.get());
	if (relativeMovementAngle >= -67.5 && relativeMovementAngle <= 67.5)
		movementModifierDirection = MovementModifierDirection::FORWARD;
	else if (relativeMovementAngle <= -112.5 || relativeMovementAngle >= 112.5)
		movementModifierDirection = MovementModifierDirection::BACKWARD;
	else
		movementModifierDirection = MovementModifierDirection::SIDEWAYS;
}

PlayableMoveController::MovementModifierDirection PlayableMoveController::getMovementDirection() {
	if (!isInMove() && commons::utils::currentTimeMillis() - lastMoveUpdate.get() > 1000)
		return MovementModifierDirection::NONE;
	return movementModifierDirection.get();
}

} // namespace aion::gameserver::controllers::movement
