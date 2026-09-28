#include "aion/gameserver/network/aion/clientpackets/CM_SHOW_BLOCKLIST.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/network/aion/serverpackets/SM_BLOCK_LIST.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_SHOW_BLOCKLIST::CM_SHOW_BLOCKLIST(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_SHOW_BLOCKLIST::readImpl() {
}

void CM_SHOW_BLOCKLIST::runImpl() {
	sendPacket(serverpackets::SM_BLOCK_LIST());
}

AION_CLIENT_PACKET(CM_SHOW_BLOCKLIST);

} // namespace aion::gameserver::network::aion::clientpackets
