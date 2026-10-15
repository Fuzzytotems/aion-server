#include "aion/gameserver/handlers/admincommands/UnBan.h"

#include <string>

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dao/PlayerDAO.h"
#include "aion/gameserver/network/loginserver/LoginServer.h"
#include "aion/gameserver/utils/Util.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(UnBan);

UnBan::UnBan() : AdminCommand("unban") {
}

// Java UnBan.java:23-57
void UnBan::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() < 1) { // parity= if (params == null || params.length < 1) {
		PacketSendUtility::sendMessage(admin, "Syntax: //unban <player> [account|ip|full]");
		return;
	}

	// Banned player must be offline, so get his account ID from database
	std::string name = Util::convertName(params[0]);
	int32_t accountId = dao::PlayerDAO::getAccountIdByName(name);
	if (accountId == 0) {
		PacketSendUtility::sendMessage(admin, "Player " + name + " was not found!");
		PacketSendUtility::sendMessage(admin, "Syntax: //unban <player> [account|ip|full]");
		return;
	}

	int8_t type = 3; // Default: full
	if (params.size() > 1) {
		// Smart Matching
		std::string stype = commons::utils::StringUtils::toLowerCase(params[1]);
		if (std::string_view("account").starts_with(stype)) // parity= if (("account").startsWith(stype))
			type = 1;
		else if (std::string_view("ip").starts_with(stype)) // parity= else if (("ip").startsWith(stype))
			type = 2;
		else if (std::string_view("full").starts_with(stype)) // parity= else if (("full").startsWith(stype))
			type = 3;
		else {
			PacketSendUtility::sendMessage(admin, "Syntax: //unban <player> [account|ip|full]");
			return;
		}
	}

	// Sends time -1 to unban
	LoginServer::getInstance().sendBanPacket(type, accountId, "", -1, admin.getObjectId());
}

// Java UnBan.java:59-62
void UnBan::info(Player& player, std::optional<std::string_view> /*message*/) {
	PacketSendUtility::sendMessage(player, "Syntax: //unban <player> [account|ip|full]");
}

} // namespace aion::gameserver::handlers::admincommands
