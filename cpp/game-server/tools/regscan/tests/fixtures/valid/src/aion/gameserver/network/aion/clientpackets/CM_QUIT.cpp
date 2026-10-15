#include "aion/gameserver/network/aion/clientpackets/CM_QUIT.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"

namespace aion::gameserver::network::aion::clientpackets {

CM_QUIT::CM_QUIT(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

AION_CLIENT_PACKET(CM_QUIT);

} // namespace aion::gameserver::network::aion::clientpackets
