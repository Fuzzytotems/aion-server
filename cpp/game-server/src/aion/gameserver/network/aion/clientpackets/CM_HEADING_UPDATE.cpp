#include "aion/gameserver/network/aion/clientpackets/CM_HEADING_UPDATE.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_HEADING_UPDATE::CM_HEADING_UPDATE(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_HEADING_UPDATE::readImpl() {
	readC(); // heading
}

void CM_HEADING_UPDATE::runImpl() {
}

AION_CLIENT_PACKET(CM_HEADING_UPDATE);

} // namespace aion::gameserver::network::aion::clientpackets
