#include "aion/gameserver/network/aion/clientpackets/CM_ENTER_WORLD.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/services/player/PlayerEnterWorldService.h"

namespace aion::gameserver::network::aion::clientpackets {

CM_ENTER_WORLD::CM_ENTER_WORLD(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_ENTER_WORLD::readImpl() {
	objectId = readD();
}

void CM_ENTER_WORLD::runImpl() {
	services::player::PlayerEnterWorldService::enterWorld(getConnection().get(), objectId);
}

AION_CLIENT_PACKET(CM_ENTER_WORLD);

} // namespace aion::gameserver::network::aion::clientpackets
