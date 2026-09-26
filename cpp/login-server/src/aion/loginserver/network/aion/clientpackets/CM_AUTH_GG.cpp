#include "aion/loginserver/network/aion/clientpackets/CM_AUTH_GG.h"

#include "aion/loginserver/network/aion/serverpackets/SM_AUTH_GG.h"
#include "aion/loginserver/network/aion/serverpackets/SM_LOGIN_FAIL.h"

namespace aion::loginserver::network::aion::clientpackets {

void CM_AUTH_GG::readImpl() {
	sessionId = readD();
	readD();
	readD();
	readD();
	readD();
	readB(0x0B);
}

void CM_AUTH_GG::runImpl() {
	const std::shared_ptr<LoginConnection>& con = getConnection();
	if (con->getSessionId() == sessionId) {
		con->setState(LoginConnection::State::AUTHED_GG);
		con->sendPacket(std::make_shared<serverpackets::SM_AUTH_GG>(sessionId));
	} else {
		// Session id is not ok, notify and disconnect client
		con->close(std::make_shared<serverpackets::SM_LOGIN_FAIL>(AionAuthResponse::STR_L2AUTH_S_SYSTEM_ERROR));
	}
}

} // namespace aion::loginserver::network::aion::clientpackets
