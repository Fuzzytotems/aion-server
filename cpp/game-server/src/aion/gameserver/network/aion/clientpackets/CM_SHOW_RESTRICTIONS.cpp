#include "aion/gameserver/network/aion/clientpackets/CM_SHOW_RESTRICTIONS.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_SHOW_RESTRICTIONS::CM_SHOW_RESTRICTIONS(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_SHOW_RESTRICTIONS.java:21-23: the packet has no body
void CM_SHOW_RESTRICTIONS::readImpl() {
}

// Java CM_SHOW_RESTRICTIONS.java:25-28
void CM_SHOW_RESTRICTIONS::runImpl() {
	sendPacket(serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_ACCUSE_INFO_NORMAL()); // can be STR_MSG_ACCUSE_INFO_1_LEVEL to STR_MSG_ACCUSE_INFO_4_LEVEL
}

AION_CLIENT_PACKET(CM_SHOW_RESTRICTIONS);

} // namespace aion::gameserver::network::aion::clientpackets
