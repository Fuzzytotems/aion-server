#include "aion/gameserver/network/aion/clientpackets/CM_BIND_POINT_TELEPORT.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/teleport/BindPointTeleportService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::player::Player;
using services::teleport::BindPointTeleportService;

CM_BIND_POINT_TELEPORT::CM_BIND_POINT_TELEPORT(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_BIND_POINT_TELEPORT.java:23-30
void CM_BIND_POINT_TELEPORT::readImpl() {
	action = readC(); // 1 casting, 2 cancel, 3 done
	if (action == 1) {
		locId = readD();
		kinah = readQ(); // kinah
	}
}

// Java CM_BIND_POINT_TELEPORT.java:32-46. A cancel (2) carries no loc id: cancelTeleport broadcasts loc id 0, as in Java.
void CM_BIND_POINT_TELEPORT::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	if (player->isDead())
		return;

	switch (action) {
		case 1:
			BindPointTeleportService::teleport(*player, locId, kinah);
			break;
		case 2:
			BindPointTeleportService::cancelTeleport(*player, locId);
			break;
	}
}

AION_CLIENT_PACKET(CM_BIND_POINT_TELEPORT);

} // namespace aion::gameserver::network::aion::clientpackets
