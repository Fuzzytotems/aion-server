#include "aion/gameserver/network/aion/clientpackets/CM_L2AUTH_LOGIN_CHECK.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/network/loginserver/LoginServer.h"

namespace aion::gameserver::network::aion::clientpackets {

CM_L2AUTH_LOGIN_CHECK::CM_L2AUTH_LOGIN_CHECK(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_L2AUTH_LOGIN_CHECK::readImpl() {
	playOk2 = readD();
	playOk1 = readD();
	accountId = readD();
	loginOk = readD();
	unk1 = readD();
	unk2 = readD();
}

void CM_L2AUTH_LOGIN_CHECK::runImpl() {
	loginserver::LoginServer::getInstance().registerLoginRequest(accountId, getConnection().get(), loginOk, playOk1, playOk2);
}

AION_CLIENT_PACKET(CM_L2AUTH_LOGIN_CHECK);

} // namespace aion::gameserver::network::aion::clientpackets
