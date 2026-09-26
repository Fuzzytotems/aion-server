#include "aion/gameserver/network/loginserver/clientpackets/CM_ACCOUNT_RECONNECT_KEY.h"

#include "aion/gameserver/network/loginserver/LoginServer.h"

namespace aion::gameserver::network::loginserver::clientpackets {

CM_ACCOUNT_RECONNECT_KEY::CM_ACCOUNT_RECONNECT_KEY(int32_t opCode) : LsClientPacket(opCode) {
}

void CM_ACCOUNT_RECONNECT_KEY::readImpl() {
	accountId = readD();
	reconnectKey = readD();
}

void CM_ACCOUNT_RECONNECT_KEY::runImpl() {
	LoginServer::getInstance().authReconnectionResponse(accountId, reconnectKey);
}

} // namespace aion::gameserver::network::loginserver::clientpackets
