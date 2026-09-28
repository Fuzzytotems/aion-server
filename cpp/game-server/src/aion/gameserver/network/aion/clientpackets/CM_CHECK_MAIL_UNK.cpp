#include "aion/gameserver/network/aion/clientpackets/CM_CHECK_MAIL_UNK.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_CHECK_MAIL_UNK::CM_CHECK_MAIL_UNK(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_CHECK_MAIL_UNK::readImpl() {
}

void CM_CHECK_MAIL_UNK::runImpl() {
	// TODO???
}

AION_CLIENT_PACKET(CM_CHECK_MAIL_UNK);

} // namespace aion::gameserver::network::aion::clientpackets
