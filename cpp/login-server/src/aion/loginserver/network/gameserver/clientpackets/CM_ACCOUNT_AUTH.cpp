#include "aion/loginserver/network/gameserver/clientpackets/CM_ACCOUNT_AUTH.h"

#include "aion/loginserver/controller/AccountController.h"

namespace aion::loginserver::network::gameserver::clientpackets {

void CM_ACCOUNT_AUTH::readImpl() {
	int32_t accountId = readD();
	int32_t loginOk = readD();
	int32_t playOk1 = readD();
	int32_t playOk2 = readD();

	sessionKey = aion::SessionKey(accountId, loginOk, playOk1, playOk2);
}

void CM_ACCOUNT_AUTH::runImpl() {
	controller::AccountController::checkAuth(sessionKey, getConnection());
}

} // namespace aion::loginserver::network::gameserver::clientpackets
