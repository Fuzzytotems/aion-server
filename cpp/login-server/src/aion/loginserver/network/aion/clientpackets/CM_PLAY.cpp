#include "aion/loginserver/network/aion/clientpackets/CM_PLAY.h"

#include "aion/commons/utils/Exception.h"
#include "aion/loginserver/GameServerInfo.h"
#include "aion/loginserver/GameServerTable.h"
#include "aion/loginserver/model/Account.h"
#include "aion/loginserver/network/aion/serverpackets/SM_LOGIN_FAIL.h"
#include "aion/loginserver/network/aion/serverpackets/SM_PLAY_FAIL.h"
#include "aion/loginserver/network/aion/serverpackets/SM_PLAY_OK.h"

namespace aion::loginserver::network::aion::clientpackets {

using namespace serverpackets;

void CM_PLAY::readImpl() {
	accountId = readD();
	loginOk = readD();
	servId = readC();
	readB(6); // CE 15 F9 75 78 30 or all zero
	readQ(); // random
}

void CM_PLAY::runImpl() {
	const std::shared_ptr<LoginConnection>& con = getConnection();
	SessionKey key = con->getSessionKey().value();
	if (key.checkLogin(accountId, loginOk)) {
		std::shared_ptr<GameServerInfo> gsi = GameServerTable::getGameServerInfo(servId);
		std::shared_ptr<model::Account> account = con->getAccount();
		if (!gsi || !gsi->isOnline())
			con->sendPacket(std::make_shared<SM_PLAY_FAIL>(AionAuthResponse::STR_L2AUTH_S_SERVER_DOWN));
		else if (!account)
			throw commons::utils::IllegalStateException(con->toString() + " has no account");
		else if (gsi->getMinAccessLevel() > account->getAccessLevel())
			con->sendPacket(std::make_shared<SM_PLAY_FAIL>(AionAuthResponse::STR_L2AUTH_S_SEVER_CHECK));
		else if (gsi->isFull())
			con->sendPacket(std::make_shared<SM_PLAY_FAIL>(AionAuthResponse::STR_L2AUTH_S_LIMIT_EXCEED));
		else {
			con->setJoinedGs();
			sendPacket(std::make_shared<SM_PLAY_OK>(key, servId));
		}
	} else
		con->close(std::make_shared<SM_LOGIN_FAIL>(AionAuthResponse::STR_L2AUTH_S_SYSTEM_ERROR));
}

} // namespace aion::loginserver::network::aion::clientpackets
