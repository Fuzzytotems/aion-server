#include "aion/gameserver/network/aion/clientpackets/CM_MOVE.h"

#include <memory>
#include <string>

#include "aion/gameserver/controllers/FlyController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/controllers/movement/GlideFlag.h"
#include "aion/gameserver/controllers/movement/MovementMask.h"
#include "aion/gameserver/controllers/movement/PlayerMoveController.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/CustomPlayerState.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FORCED_MOVE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MOVE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_POSITION.h"
#include "aion/gameserver/services/antihack/AntiHackService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using controllers::movement::GlideFlag;
using controllers::movement::MovementMask;
using model::gameobjects::player::CustomPlayerState;
using model::gameobjects::player::Player;

CM_MOVE::CM_MOVE(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_MOVE::readImpl() {
	x = readF();
	y = readF();
	z = readF();
	heading = readC();
	type = readC();
	if ((type & MovementMask::POSITION) == MovementMask::POSITION && (type & MovementMask::MANUAL) == MovementMask::MANUAL) {
		if ((type & MovementMask::ABSOLUTE) == MovementMask::ABSOLUTE) {
			x2 = readF();
			y2 = readF();
			z2 = readF();
		} else {
			vectorX = readF();
			vectorY = readF();
			vectorZ = readF();
			x2 = vectorX + x;
			y2 = vectorY + y;
			z2 = vectorZ + z;
		}
	}
	if ((type & MovementMask::GLIDE) == MovementMask::GLIDE) {
		glideFlag = readC();
		if (glideFlag == GlideFlag::GEYSER)
			geyserLocationId = readUC(); // locationId from windstreams.xml
	}
	if ((type & MovementMask::VEHICLE) == MovementMask::VEHICLE) {
		unk1 = readD();
		unk2 = readD();
		vehicleX = readF();
		vehicleY = readF();
		vehicleZ = readF();
	}
}

void CM_MOVE::runImpl() {
	runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	if (player->isDead() || player->getEffectController()->isUnderFear() || player->getEffectController()->isConfused()) // just in case of bad timing
		return;
	if (handleBogusPacket(*player))
		return;
	runtime::Ptr<controllers::movement::PlayerMoveController> m = player->getMoveController();
	bool jumping = false;
	int8_t oldMask = m->movementMask.get();
	m->movementMask.set(type);
	if (type == MovementMask::IMMEDIATE) { // stopping or turning
		m->setNewDirection(x, y, z, heading);
	} else {
		jumping = !player->isFlying() && (type & MovementMask::POSITION) == MovementMask::POSITION && (type & MovementMask::MANUAL) == MovementMask::MANUAL &&
			(type & MovementMask::ABSOLUTE) != MovementMask::ABSOLUTE && (type & MovementMask::GLIDE) != MovementMask::GLIDE &&
			(type & MovementMask::VEHICLE) != MovementMask::VEHICLE && z2 > z;
		if ((type & MovementMask::GLIDE) == MovementMask::GLIDE) {
			m->glideFlag.set(glideFlag);
			m->geyserLocationId.set(geyserLocationId);
			player->getFlyController().switchToGliding();
		}
		if ((type & MovementMask::POSITION) == MovementMask::POSITION && (type & MovementMask::MANUAL) == MovementMask::MANUAL) { // start move or change direction
			m->setNewDirection(x2, y2, z2, heading);
			if ((type & MovementMask::ABSOLUTE) == MovementMask::ABSOLUTE) {
				if (player->isInCustomState(CustomPlayerState::TELEPORTATION_MODE)) {
					player->getMoveController()->setIsJumping(false);
					world::World::getInstance().updatePosition(*player, x2, y2, z2, heading);
					m->onMoveFromClient();
					utils::PacketSendUtility::broadcastToSightedPlayers(*player, serverpackets::SM_POSITION(*player), true);
					return;
				}
			} else {
				m->vectorX.set(vectorX);
				m->vectorY.set(vectorY);
				m->vectorZ.set(vectorZ);
			}
		} else {
			if ((type & MovementMask::ABSOLUTE) == 0) {
				// The movement vector from the client already accounts for movement speed, so multiplying it by speed again overestimates the distance several times.
				m->setNewDirection(x + m->vectorX.get(), y + m->vectorY.get(), z + m->vectorZ.get(), heading);
			} else if (heading != player->getHeading())
				m->setNewDirection(m->getTargetX2(), m->getTargetY2(), m->getTargetZ2(), heading);
		}
		if ((type & MovementMask::VEHICLE) == MovementMask::VEHICLE) {
			m->unk1.set(unk1);
			m->unk2.set(unk2);
			m->vehicleX.set(vehicleX);
			m->vehicleY.set(vehicleY);
			m->vehicleZ.set(vehicleZ);
		}
	}
	if (!services::antihack::AntiHackService::canMove(*player, x, y, z, type)) {
		player->getMoveController()->setIsJumping(false);
		return;
	}
	if (!player->isSpawned()) // should be checked as late as possible, to prevent false warnings from World.updatePosition
		return;
	if (player->isProtectionActive() && (player->getX() != x || player->getY() != y || player->getZ() > z + 0.5f))
		player->getController().stopProtectionActiveTask();
	player->getMoveController()->setIsJumping(jumping);
	world::World::getInstance().updatePosition(*player, x, y, z, heading);
	m->onMoveFromClient();
	notifyControllers(*player, oldMask);
	if (((type & MovementMask::POSITION) == MovementMask::POSITION && (type & MovementMask::MANUAL) == MovementMask::MANUAL) ||
		type == MovementMask::IMMEDIATE)
		utils::PacketSendUtility::broadcastToSightedPlayers(*player, serverpackets::SM_MOVE(*player));
	if ((type & MovementMask::FALL) == MovementMask::FALL) {
		player->getFlyController().onStopGliding();
		m->updateFalling(z);
	} else {
		m->stopFalling(z);
	}
}

bool CM_MOVE::handleBogusPacket(Player& player) {
	if (player.isInCustomState(CustomPlayerState::WATCHING_CUTSCENE)) // client sends crap during cutscenes in transformed state
		return true;
	runtime::Ptr<model::gameobjects::VisibleObject> target = player.getTarget();
	if (target && player.getMoveController()->hasMovedByRandomMoveLocEffect() && utils::PositionUtil::isInRange(*target, x, y, z, 2) &&
		!utils::PositionUtil::isInRange(player, x, y, z, 3)) {
		/*
		 * The game client often sends incorrect coordinates and tries to move you to your target's position when using any RandomMoveLocEffect
		 * (Emergency Teleport I, Power: Emergency Teleport I, Blind Leap, Feint, etc.) while:
		 * 1) running or jumping around the corner of an obstacle
		 * 2) jumping on an obstacle
		 * 3) jumping over an obstacle (harder to reproduce with skills that have no animation time)
		 * 4) running up/down the upper end of stairs: only works for skills with animation time, animation must either start or end at the top flat level
		 * 5) Additionally, teleporting across any type of crest blocking line of sight between the start and end position causes a similar condition.
		 * It seems like this happens if the game thinks you have passed through an obstacle while using teleportation skills. Server side positions
		 * are not considered for this, it is all evaluated by the client based on local coordinates.
		 * Most often incorrect coordinates are contained in the first move packet after SM_CASTSPELL_RESULT, but sometimes it's the second one or,
		 * in case of teleportation skills with animation time, sometimes even both. That's when we also see type == 0. Other times, type often has
		 * MovementType.FALL but not always (especially if a directional teleport was involved).
		 * Sending a move packet with the current server-side position works around this client bug and the client will not move you to your target's
		 * position.
		 */
		bool moveForcefully =
			type == 0 || ((type & MovementMask::MANUAL) == MovementMask::MANUAL && (type & MovementMask::POSITION) == MovementMask::POSITION);
		if (moveForcefully)
			sendPacket(serverpackets::SM_FORCED_MOVE(player, player));
		else
			sendPacket(serverpackets::SM_MOVE(player));
		return true;
	}
	return false;
}

void CM_MOVE::notifyControllers(Player& player, int8_t oldMovementMask) {
	if (player.getMoveController()->getMovementMask() == MovementMask::IMMEDIATE) { // stopping or turning
		if (oldMovementMask == MovementMask::IMMEDIATE) // turning
			player.getController().onMove();
		// notify arrived
		player.getController().onStopMove();
		player.getFlyController().onStopGliding();
	} else if ((type & MovementMask::POSITION) == MovementMask::POSITION && (type & MovementMask::MANUAL) == MovementMask::MANUAL &&
		!player.getMoveController()->isInMove()) { // start move or change direction
		player.getController().onStartMove();
	} else {
		player.getController().onMove();
	}
}

std::string CM_MOVE::toString() const {
	using geoEngine::math::JavaFloat;
	return "CM_MOVE [type=" + std::to_string(type & 0xFF) + ", heading=" + std::to_string(heading) + ", x=" + JavaFloat::toString(x) +
		", y=" + JavaFloat::toString(y) + ", z=" + JavaFloat::toString(z) + ", x2=" + JavaFloat::toString(x2) + ", y2=" + JavaFloat::toString(y2) +
		", z2=" + JavaFloat::toString(z2) + ", vehicleX=" + JavaFloat::toString(vehicleX) + ", vehicleY=" + JavaFloat::toString(vehicleY) +
		", vehicleZ=" + JavaFloat::toString(vehicleZ) + ", vectorX=" + JavaFloat::toString(vectorX) + ", vectorY=" + JavaFloat::toString(vectorY) +
		", vectorZ=" + JavaFloat::toString(vectorZ) + ", glideFlag=" + std::to_string(glideFlag) + ", unk1=" + std::to_string(unk1) +
		", unk2=" + std::to_string(unk2) + "]";
}

AION_CLIENT_PACKET(CM_MOVE);

} // namespace aion::gameserver::network::aion::clientpackets
