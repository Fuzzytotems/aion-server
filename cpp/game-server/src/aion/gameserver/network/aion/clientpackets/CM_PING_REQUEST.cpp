#include "aion/gameserver/network/aion/clientpackets/CM_PING_REQUEST.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PING_RESPONSE.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_PING_REQUEST::CM_PING_REQUEST(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_PING_REQUEST::readImpl() {
	// empty
}

void CM_PING_REQUEST::runImpl() {
	sendPacket(serverpackets::SM_PING_RESPONSE());
}

AION_CLIENT_PACKET(CM_PING_REQUEST);

} // namespace aion::gameserver::network::aion::clientpackets
