#include "aion/gameserver/network/aion/clientpackets/CM_MOVE_IN_AIR.h"

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/movement/PlayerMoveController.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/templates/flypath/FlightPath.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_MOVE_IN_AIR::CM_MOVE_IN_AIR(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_MOVE_IN_AIR.java:34-42
void CM_MOVE_IN_AIR::readImpl() {
	worldId = readD();
	x = readF();
	y = readF();
	z = readF();
	heading = readC();
	distance = readD();
}

// Java CM_MOVE_IN_AIR.java:44-60
void CM_MOVE_IN_AIR::runImpl() {
	const runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	if (!player->isSpawned())
		return;
	if (!player->isInState(model::gameobjects::state::CreatureState::FLYING))
		return;

	if (runtime::Ptr<model::templates::flypath::FlightPath> flightPath = player->getFlightPath())
		flightPath->setDistance(distance);

	if (player->isProtectionActive())
		player->getController().stopProtectionActiveTask();

	world::World::getInstance().updatePosition(*player, x, y, z, heading);
	player->getMoveController()->onMoveFromClient();
	player->getController().onMove();
}

AION_CLIENT_PACKET(CM_MOVE_IN_AIR);

} // namespace aion::gameserver::network::aion::clientpackets
