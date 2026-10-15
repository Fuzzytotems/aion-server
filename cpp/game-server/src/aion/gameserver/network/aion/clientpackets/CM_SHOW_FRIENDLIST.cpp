#include "aion/gameserver/network/aion/clientpackets/CM_SHOW_FRIENDLIST.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FRIEND_LIST.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_SHOW_FRIENDLIST::CM_SHOW_FRIENDLIST(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_SHOW_FRIENDLIST::readImpl() {
}

void CM_SHOW_FRIENDLIST::runImpl() {
	sendPacket(serverpackets::SM_FRIEND_LIST());
}

AION_CLIENT_PACKET(CM_SHOW_FRIENDLIST);

} // namespace aion::gameserver::network::aion::clientpackets
