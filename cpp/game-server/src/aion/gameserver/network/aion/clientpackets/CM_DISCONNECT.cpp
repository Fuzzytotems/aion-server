#include "aion/gameserver/network/aion/clientpackets/CM_DISCONNECT.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"

namespace aion::gameserver::network::aion::clientpackets {

CM_DISCONNECT::CM_DISCONNECT(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_DISCONNECT::readImpl() {
	readC(); // 0 when client auto closes the connection for inactivity, maybe there are other flags in different cases
}

void CM_DISCONNECT::runImpl() {
	// no need to do something here, since the character will leave world shortly after the connection is closed
}

AION_CLIENT_PACKET(CM_DISCONNECT);

} // namespace aion::gameserver::network::aion::clientpackets
