#include "aion/loginserver/network/aion/clientpackets/CM_SERVER_LIST.h"

#include "aion/loginserver/GameServerTable.h"
#include "aion/loginserver/controller/AccountController.h"
#include "aion/loginserver/network/aion/serverpackets/SM_LOGIN_FAIL.h"

namespace aion::loginserver::network::aion::clientpackets {

void CM_SERVER_LIST::readFields() {
	accountId = readD();
	loginOk = readD();
	readC(); // always 7 in the 4.8 client
}

void CM_SERVER_LIST::runImpl() {
	const std::shared_ptr<LoginConnection>& con = getConnection();
	if (con->getSessionKey().value().checkLogin(accountId, loginOk)) {
		if (GameServerTable::size() == 0)
			con->close(serverpackets::SM_LOGIN_FAIL(AionAuthResponse::STR_L2AUTH_S_NO_SERVER_LIST));
		else
			controller::AccountController::loadGSCharactersCount(accountId);
	} else {
		con->close(serverpackets::SM_LOGIN_FAIL(AionAuthResponse::STR_L2AUTH_S_SYSTEM_ERROR));
	}
}

} // namespace aion::loginserver::network::aion::clientpackets
