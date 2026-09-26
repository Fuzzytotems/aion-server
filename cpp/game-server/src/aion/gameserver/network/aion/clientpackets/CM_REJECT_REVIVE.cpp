#include "aion/gameserver/network/aion/clientpackets/CM_REJECT_REVIVE.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_REJECT_REVIVE::CM_REJECT_REVIVE(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_REJECT_REVIVE::readImpl() {
}

void CM_REJECT_REVIVE::runImpl() {
}

AION_CLIENT_PACKET(CM_REJECT_REVIVE);

} // namespace aion::gameserver::network::aion::clientpackets
