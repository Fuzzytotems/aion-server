#include "aion/loginserver/network/gameserver/clientpackets/CM_ACCOUNT_DISCONNECTED.h"

#include "aion/loginserver/GameServerInfo.h"
#include "aion/loginserver/controller/AccountTimeController.h"
#include "aion/loginserver/model/Account.h"

namespace aion::loginserver::network::gameserver::clientpackets {

void CM_ACCOUNT_DISCONNECTED::readImpl() {
	accountId = readD();
}

void CM_ACCOUNT_DISCONNECTED::runImpl() {
	std::shared_ptr<model::Account> account = getGameServerInfo()->removeAccountFromGameServer(accountId);

	// account can be null if a player logged out from gs (see CM_ACCOUNT_RECONNECT_KEY)
	if (account) {
		controller::AccountTimeController::updateOnLogout(*account);
	}
}

} // namespace aion::loginserver::network::gameserver::clientpackets
