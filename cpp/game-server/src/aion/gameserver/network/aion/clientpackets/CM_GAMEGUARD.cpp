#include "aion/gameserver/network/aion/clientpackets/CM_GAMEGUARD.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/services/antihack/AntiHackService.h"

namespace aion::gameserver::network::aion::clientpackets {

CM_GAMEGUARD::CM_GAMEGUARD(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_GAMEGUARD::readImpl() {
	size = readD();
	readB(size);
}

void CM_GAMEGUARD::runImpl() {
	services::antihack::AntiHackService::checkAionBin(size, getConnection().get());
}

AION_CLIENT_PACKET(CM_GAMEGUARD);

} // namespace aion::gameserver::network::aion::clientpackets
