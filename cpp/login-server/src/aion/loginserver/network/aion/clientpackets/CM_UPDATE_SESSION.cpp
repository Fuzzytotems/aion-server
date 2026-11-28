#include "aion/loginserver/network/aion/clientpackets/CM_UPDATE_SESSION.h"

#include "aion/loginserver/controller/AccountController.h"
#include "aion/loginserver/network/aion/LoginConnection.h"

namespace aion::loginserver::network::aion::clientpackets {

void CM_UPDATE_SESSION::readFields() {
	accountId = readD();
	loginOk = readD();
	reconnectKey = readD();
}

void CM_UPDATE_SESSION::runImpl() {
	LoginConnection::AccountAttachScope accountAttachScope(*getConnection()); // logs out if the client disconnects before the account was attached
	controller::AccountController::authReconnectingAccount(accountId, loginOk, reconnectKey, getConnection());
}

} // namespace aion::loginserver::network::aion::clientpackets
