#include "aion/gameserver/network/aion/clientpackets/CM_SUMMON_MOVE.h"

#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/movement/CreatureMoveController.h"
#include "aion/gameserver/controllers/movement/MovementMask.h"
#include "aion/gameserver/controllers/movement/SummonMoveController.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MOVE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using controllers::movement::MovementMask;

CM_SUMMON_MOVE::CM_SUMMON_MOVE(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_SUMMON_MOVE.java:30-56
void CM_SUMMON_MOVE::readImpl() {
	objectId = readD();
	x = readF();
	y = readF();
	z = readF();
	heading = readC();
	type = readC();
	if ((type & MovementMask::POSITION) == MovementMask::POSITION && (type & MovementMask::MANUAL) == MovementMask::MANUAL) {
		if ((type & MovementMask::ABSOLUTE) == 0) {
			// this type is sent when the summon is in move and it receives or resists movement restricting effects, like stun, stagger, etc.
			// summon's x/y/z is expected to be immediately updated to the sent x/y/z values and no vector or x2/y2/z2 coords are sent
		} else {
			x2 = readF();
			y2 = readF();
			z2 = readF();
		}
	}
	if ((type & MovementMask::GLIDE) == MovementMask::GLIDE) {
		glideFlag = readC();
	}
	if ((type & MovementMask::VEHICLE) == MovementMask::VEHICLE) {
		unk1 = readD();
		unk2 = readD();
		vehicleX = readF();
		vehicleY = readF();
		vehicleZ = readF();
	}
}

// Java CM_SUMMON_MOVE.java:58-97
void CM_SUMMON_MOVE::runImpl() {
	const runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	const runtime::Ptr<model::gameobjects::Creature> summonOrMercenary = player->getSummonOrMercenary(objectId);
	if (!summonOrMercenary || !summonOrMercenary->isSpawned())
		return;
	const runtime::Ptr<controllers::effect::EffectController> effectController = summonOrMercenary->getEffectController();
	if (effectController->isInAnyAbnormalState(skillengine::effect::AbnormalState::CANT_MOVE_STATE) || effectController->isUnderFear() ||
		effectController->isConfused())
		return;
	const runtime::Ptr<controllers::movement::CreatureMoveController> m = summonOrMercenary->getMoveController();
	m->movementMask.set(type);

	const runtime::Ptr<controllers::movement::SummonMoveController> smc = runtime::as<controllers::movement::SummonMoveController>(m);
	if (smc && (type & MovementMask::GLIDE) == MovementMask::GLIDE) {
		smc->glideFlag.set(glideFlag);
	}

	if (type == MovementMask::IMMEDIATE) {
		summonOrMercenary->getController().onStopMove();
	} else if ((type & MovementMask::POSITION) == MovementMask::POSITION && (type & MovementMask::MANUAL) == MovementMask::MANUAL) {
		if ((type & MovementMask::ABSOLUTE) == 0) // skip position update since the server has already set the correct position for stun or resist
			return;
		summonOrMercenary->getMoveController()->setNewDirection(x2, y2, z2, heading);
		summonOrMercenary->getController().onStartMove();
	} else
		summonOrMercenary->getController().onMove();

	if (smc && (type & MovementMask::VEHICLE) == MovementMask::VEHICLE) {
		smc->unk1.set(unk1);
		smc->unk2.set(unk2);
		smc->vehicleX.set(vehicleX);
		smc->vehicleY.set(vehicleY);
		smc->vehicleZ.set(vehicleZ);
	}
	world::World::getInstance().updatePosition(*summonOrMercenary, x, y, z, heading);
	m->updateLastMove();

	if ((type & MovementMask::POSITION) == MovementMask::POSITION || type == MovementMask::IMMEDIATE)
		utils::PacketSendUtility::broadcastToSightedPlayers(*summonOrMercenary, serverpackets::SM_MOVE(*summonOrMercenary));
}

AION_CLIENT_PACKET(CM_SUMMON_MOVE);

} // namespace aion::gameserver::network::aion::clientpackets
