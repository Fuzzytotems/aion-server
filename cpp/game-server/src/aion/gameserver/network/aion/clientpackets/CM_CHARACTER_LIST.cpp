#include "aion/gameserver/network/aion/clientpackets/CM_CHARACTER_LIST.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ACCOUNT_PROPERTIES.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CHARACTER_LIST.h"

namespace aion::gameserver::network::aion::clientpackets {

CM_CHARACTER_LIST::CM_CHARACTER_LIST(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_CHARACTER_LIST::readImpl() {
	playOk2 = readD();
}

void CM_CHARACTER_LIST::runImpl() {
	sendPacket(serverpackets::SM_ACCOUNT_PROPERTIES());
	sendPacket(serverpackets::SM_CHARACTER_LIST(playOk2));
}

AION_CLIENT_PACKET(CM_CHARACTER_LIST);

} // namespace aion::gameserver::network::aion::clientpackets
