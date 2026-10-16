#include "aion/gameserver/network/loginserver/clientpackets/CM_LS_CONTROL_RESPONSE.h"

#include <memory>
#include <string>

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/loginserver/LoginServer.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::loginserver::clientpackets {

using model::gameobjects::player::Player;

CM_LS_CONTROL_RESPONSE::CM_LS_CONTROL_RESPONSE(int32_t opCode) : LsClientPacket(opCode) {
}

void CM_LS_CONTROL_RESPONSE::readImpl() {
	type = readC();
	param = readC();
	accountId = readD();
	adminId = readD();
	result = readC() == 1;
}

void CM_LS_CONTROL_RESPONSE::runImpl() {
	runtime::Ptr<Player> admin = world::World::getInstance().getPlayer(adminId);
	if (!result) {
		if (admin)
			utils::PacketSendUtility::sendMessage(*admin, "The operation failed.");
		return;
	}
	std::shared_ptr<network::aion::AionConnection> playerConnection = LoginServer::getInstance().accountUpdate(accountId, type, param);
	runtime::Ptr<Player> player = !playerConnection ? nullptr : playerConnection->getActivePlayer();
	std::string targetAccount = !player ? "Account " + std::to_string(accountId) : "Account of " + player->getName();
	switch (type) {
		case 1:
			notifyAboutNewPermissions(admin, player, targetAccount, "access level");
			break;
		case 2:
			notifyAboutNewPermissions(admin, player, targetAccount, "membership level");
			break;
		default:
			sendMessage(admin, targetAccount + " has been successfully updated.");
			break;
	}
}

void CM_LS_CONTROL_RESPONSE::notifyAboutNewPermissions(runtime::Ptr<Player> admin, runtime::Ptr<Player> player, std::string_view targetAccount,
	std::string_view permissionType) {
	const std::string paramText = std::to_string(param); // Java %s of a Byte
	sendMessage(admin, std::string(targetAccount) + " has been granted " + std::string(permissionType) + " " + paramText + ".");
	if (!admin)
		sendMessage(player, "You have been granted " + std::string(permissionType) + " " + paramText + ".");
	else
		sendMessage(player, "You have been granted " + std::string(permissionType) + " " + paramText + " by " + admin->getName(true) + ".");
}

void CM_LS_CONTROL_RESPONSE::sendMessage(runtime::Ptr<Player> player, std::string_view message) {
	if (player)
		utils::PacketSendUtility::sendMessage(*player, message);
}

} // namespace aion::gameserver::network::loginserver::clientpackets
