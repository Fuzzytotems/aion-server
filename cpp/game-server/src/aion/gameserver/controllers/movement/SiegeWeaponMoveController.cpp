#include "aion/gameserver/controllers/movement/SiegeWeaponMoveController.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/controllers/ControllerStandIns.h"
#include "aion/gameserver/controllers/movement/MovementMask.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/stats/container/SummonGameStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MOVE.h"
#include "aion/gameserver/taskmanager/tasks/MoveTaskManager.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::controllers::movement {

using model::gameobjects::Summon;
using utils::PositionUtil;

SiegeWeaponMoveController::SiegeWeaponMoveController(model::gameobjects::Summon& ownerValue) : SummonMoveController(ownerValue) {
}

SiegeWeaponMoveController::~SiegeWeaponMoveController() = default;

void SiegeWeaponMoveController::moveToDestination() {
	Summon& summon = static_cast<Summon&>(owner);
	if (!summon.canPerformMove() || (summon.getAi().getSubState() == ai::AISubState::CAST)) {
		if (started->compareAndSet(true, false)) {
			setAndSendStopMove(summon);
			updateLastMove();
		}
		return;
	} else if (started->compareAndSet(false, true)) {
		updateLastMove();
		setAndSendStartMove(summon);
	}

	runtime::Ptr<model::gameobjects::VisibleObject> target = summon.getTarget();
	if (target) { // update target position, in case target moved
		pointX = target->getX();
		pointY = target->getY();
		pointZ = target->getZ();
	}
	moveToLocation(pointX.get(), pointY.get(), pointZ.get());
	updateLastMove();
}

void SiegeWeaponMoveController::moveToTargetObject() {
	updateLastMove();
	taskmanager::tasks::MoveTaskManager::getInstance().addCreature(static_cast<Summon&>(owner));
}

void SiegeWeaponMoveController::abortMove() {
	SummonMoveController::abortMove();
	taskmanager::tasks::MoveTaskManager::getInstance().removeCreature(static_cast<Summon&>(owner));
}

void SiegeWeaponMoveController::moveToLocation(float targetX, float targetY, float targetZ) {
	Summon& summon = static_cast<Summon&>(owner);
	bool destinationChanged = targetX != targetDestX.get() || targetY != targetDestY.get() || targetZ != targetDestZ.get();
	float ownerX = summon.getX();
	float ownerY = summon.getY();
	float ownerZ = summon.getZ();

	if (targetX != targetDestX.get() || targetY != targetDestY.get()) {
		heading = PositionUtil::getHeadingTowards(ownerX, ownerY, targetX, targetY);
	}

	targetDestX = targetX;
	targetDestY = targetY;
	targetDestZ = targetZ;

	float currentSpeed = summon.getGameStats()->getMovementSpeedFloat();
	float futureDistPassed = currentSpeed * static_cast<float>(commons::utils::currentTimeMillis() - lastMoveUpdate.get()) / 1000.0f;

	float dist = static_cast<float>(PositionUtil::getDistance(ownerX, ownerY, ownerZ, targetX, targetY, targetZ));

	if (dist == 0) {
		return;
	}

	if (futureDistPassed > dist) {
		futureDistPassed = dist;
	}

	float distFraction = futureDistPassed / dist;
	float newX = (targetDestX.get() - ownerX) * distFraction + ownerX;
	float newY = (targetDestY.get() - ownerY) * distFraction + ownerY;
	float newZ = (targetDestZ.get() - ownerZ) * distFraction + ownerZ;
	world::World::getInstance().updatePosition(summon, newX, newY, newZ, heading.get(), true);
	if (destinationChanged) {
		movementMask = MovementMask::NPC_STARTMOVE;
		utils::PacketSendUtility::broadcastPacket(summon, network::aion::serverpackets::SM_MOVE(summon));
	}
}

} // namespace aion::gameserver::controllers::movement
