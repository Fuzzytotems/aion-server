#include "aion/loginserver/network/gameserver/clientpackets/CM_ACCOUNT_RECONNECT_KEY.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/loginserver/GameServerInfo.h"
#include "aion/loginserver/controller/AccountController.h"
#include "aion/loginserver/model/ReconnectingAccount.h"
#include "aion/loginserver/network/gameserver/serverpackets/SM_ACCOUNT_RECONNECT_KEY.h"

namespace aion::loginserver::network::gameserver::clientpackets {

void CM_ACCOUNT_RECONNECT_KEY::readImpl() {
	accountId = readD();
}

void CM_ACCOUNT_RECONNECT_KEY::runImpl() {
	int32_t reconectKey = commons::utils::Rnd::nextInt();
	std::shared_ptr<model::Account> acc = getGameServerInfo()->removeAccountFromGameServer(accountId);
	if (!acc)
		commons::logging::LoggerFactory::getLogger("com.aionemu.loginserver.network.gameserver.clientpackets.CM_ACCOUNT_RECONNECT_KEY")
			.warn(getConnection()->toString() + " requested reconnection for account " + std::to_string(accountId) + ", but account is not registered on game server");
	else
		controller::AccountController::addReconnectingAccount(model::ReconnectingAccount(std::move(acc), reconectKey));
	sendPacket(std::make_shared<serverpackets::SM_ACCOUNT_RECONNECT_KEY>(accountId, reconectKey));
}

} // namespace aion::loginserver::network::gameserver::clientpackets
