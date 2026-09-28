#include "aion/loginserver/network/gameserver/clientpackets/CM_ACCOUNT_LIST.h"

#include "aion/commons/utils/Exception.h"
#include "aion/loginserver/GameServerInfo.h"
#include "aion/loginserver/GameServerTable.h"
#include "aion/loginserver/controller/AccountController.h"
#include "aion/loginserver/network/gameserver/serverpackets/SM_HDDBAN_LIST.h"
#include "aion/loginserver/network/gameserver/serverpackets/SM_MACBAN_LIST.h"
#include "aion/loginserver/network/gameserver/serverpackets/SM_REQUEST_KICK_ACCOUNT.h"

namespace aion::loginserver::network::gameserver::clientpackets {

using namespace serverpackets;

void CM_ACCOUNT_LIST::readImpl() {
	int32_t count = readD();
	if (count < 0)
		throw commons::utils::IllegalArgumentException("Negative account count " + std::to_string(count));
	if (count > getRemainingBytes() / 4)
		throw commons::utils::IllegalArgumentException("Account count " + std::to_string(count) + " exceeds the packet size");
	accountIds.resize(static_cast<size_t>(count));
	for (int32_t& accountId : accountIds)
		accountId = readD();
}

void CM_ACCOUNT_LIST::runImpl() {
	const std::shared_ptr<GsConnection>& connection = getConnection();
	for (int32_t id : accountIds) {
		std::shared_ptr<GameServerInfo> gsi = GameServerTable::findLoggedInAccountGs(id);
		if (!gsi) // not added if the game server disconnected meanwhile (see GameServerInfo::addAccountToGameServer)
			getGameServerInfo()->addAccountToGameServer(controller::AccountController::loadAccount(id), *connection);
		else if (gsi->getId() != getGameServerInfo()->getId()) // account already plays on another gameserver
			connection->sendPacket(std::make_shared<SM_REQUEST_KICK_ACCOUNT>(id, false));
	}
	connection->sendPacket(std::make_shared<SM_MACBAN_LIST>());
	connection->sendPacket(std::make_shared<SM_HDDBAN_LIST>());
	controller::AccountController::updateServerListForAllLoggedInPlayers();
}

} // namespace aion::loginserver::network::gameserver::clientpackets
