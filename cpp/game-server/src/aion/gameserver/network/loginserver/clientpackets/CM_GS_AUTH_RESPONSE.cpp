#include "aion/gameserver/network/loginserver/clientpackets/CM_GS_AUTH_RESPONSE.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/network/loginserver/LoginServer.h"
#include "aion/gameserver/network/loginserver/LoginServerConnection.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::loginserver::clientpackets {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.loginserver.clientpackets.CM_GS_AUTH_RESPONSE");

CM_GS_AUTH_RESPONSE::CM_GS_AUTH_RESPONSE(int32_t opCode) : LsClientPacket(opCode) {
}

void CM_GS_AUTH_RESPONSE::readImpl() {
	response = readUC();
	if (response == 0)
		serverCount = readUC();
}

void CM_GS_AUTH_RESPONSE::runImpl() {
	switch (response) {
		case 0: // Authed
			getConnection()->setState(LoginServerConnection::State::AUTHED);
			LoginServer::getInstance().setGameServerCount(serverCount);
			LoginServer::getInstance().sendLoggedInAccounts();
			break;
		case 1: // Not authed
			log.error("GameServer is not authenticated at LoginServer side!");
			getConnection()->close();
			break;
		case 2: // Already registered
			log.info("GameServer is already registered at LoginServer side!");
			getConnection()->close();
			break;
	}
}

} // namespace aion::gameserver::network::loginserver::clientpackets
