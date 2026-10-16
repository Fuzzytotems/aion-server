#include "aion/loginserver/network/gameserver/clientpackets/CM_GS_CHARACTER.h"

#include "aion/loginserver/GameServerInfo.h"
#include "aion/loginserver/controller/AccountController.h"

namespace aion::loginserver::network::gameserver::clientpackets {

void CM_GS_CHARACTER::readImpl() {
	accountId = readD();
	characterCount = readUC();
}

void CM_GS_CHARACTER::runImpl() {
	controller::AccountController::addGSCharacterCountFor(accountId, getGameServerInfo()->getId(), characterCount);

	if (controller::AccountController::hasAllGSCharacterCounts(accountId))
		controller::AccountController::sendServerListFor(accountId);
}

} // namespace aion::loginserver::network::gameserver::clientpackets
