#include "aion/gameserver/network/aion/clientpackets/CM_MAY_QUIT.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_MAY_QUIT::CM_MAY_QUIT(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_MAY_QUIT::readImpl() {
	// empty
}

void CM_MAY_QUIT::runImpl() {
	// Nothing to do
}

AION_CLIENT_PACKET(CM_MAY_QUIT);

} // namespace aion::gameserver::network::aion::clientpackets
