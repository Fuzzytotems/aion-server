#include "aion/gameserver/network/aion/clientpackets/CM_INSTANCE_LEAVE.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::player::Player;

CM_INSTANCE_LEAVE::CM_INSTANCE_LEAVE(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_INSTANCE_LEAVE.java:19-21: nothing to read
void CM_INSTANCE_LEAVE::readImpl() {
}

// Java CM_INSTANCE_LEAVE.java:24-29
void CM_INSTANCE_LEAVE::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	if (player->isInInstance())
		player->getPosition()->getWorldMapInstance()->getInstanceHandler()->leaveInstance(*player);
}

AION_CLIENT_PACKET(CM_INSTANCE_LEAVE);

} // namespace aion::gameserver::network::aion::clientpackets
