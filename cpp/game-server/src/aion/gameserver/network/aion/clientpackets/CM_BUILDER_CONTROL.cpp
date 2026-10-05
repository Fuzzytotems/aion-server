#include "aion/gameserver/network/aion/clientpackets/CM_BUILDER_CONTROL.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_BUILDER_CONTROL::CM_BUILDER_CONTROL(int32_t opcode, const StateSet& validStates) : AbstractGmCommandPacket(opcode, validStates) {
}

AION_CLIENT_PACKET(CM_BUILDER_CONTROL);

} // namespace aion::gameserver::network::aion::clientpackets
