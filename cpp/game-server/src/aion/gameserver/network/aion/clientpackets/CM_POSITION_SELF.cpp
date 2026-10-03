#include "aion/gameserver/network/aion/clientpackets/CM_POSITION_SELF.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_POSITION_SELF::CM_POSITION_SELF(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_POSITION_SELF.java:18-20: the packet has no body
void CM_POSITION_SELF::readImpl() {
}

// Java CM_POSITION_SELF.java:22-24: the client's answer to SM_POSITION_SELF needs nothing
void CM_POSITION_SELF::runImpl() {
}

AION_CLIENT_PACKET(CM_POSITION_SELF);

} // namespace aion::gameserver::network::aion::clientpackets
