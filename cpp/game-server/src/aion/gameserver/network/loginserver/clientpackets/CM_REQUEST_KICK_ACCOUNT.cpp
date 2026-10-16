#include "aion/gameserver/network/loginserver/clientpackets/CM_REQUEST_KICK_ACCOUNT.h"

#include "aion/gameserver/network/loginserver/LoginServer.h"

namespace aion::gameserver::network::loginserver::clientpackets {

CM_REQUEST_KICK_ACCOUNT::CM_REQUEST_KICK_ACCOUNT(int32_t opCode) : LsClientPacket(opCode) {
}

void CM_REQUEST_KICK_ACCOUNT::readImpl() {
	accountId = readD();
	notifyDoubleLogin = readC() == 1;
}

void CM_REQUEST_KICK_ACCOUNT::runImpl() {
	LoginServer::getInstance().kickAccount(accountId, notifyDoubleLogin);
}

} // namespace aion::gameserver::network::loginserver::clientpackets
