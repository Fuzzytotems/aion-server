#include "aion/gameserver/handlers/admincommands/UnBanChar.h"

#include <string>

#include "aion/gameserver/dao/PlayerDAO.h"
#include "aion/gameserver/services/PunishmentService.h"
#include "aion/gameserver/utils/Util.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(UnBanChar);

UnBanChar::UnBanChar() : AdminCommand("unbanchar") {
}

// Java UnBanChar.java:23-42
void UnBanChar::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() < 1) { // parity= if (params == null || params.length < 1) {
		PacketSendUtility::sendMessage(admin, "Syntax: //unbanchar <player>");
		return;
	}

	// Banned player must be offline
	std::string name = Util::convertName(params[0]);
	int32_t playerId = dao::PlayerDAO::getPlayerIdByName(name);
	if (playerId == 0) {
		PacketSendUtility::sendMessage(admin, "Player " + name + " was not found!");
		PacketSendUtility::sendMessage(admin, "Syntax: //unbanchar <player>");
		return;
	}

	PacketSendUtility::sendMessage(admin, "Character " + name + " is not longer banned!");

	services::PunishmentService::unbanChar(playerId);
}

// Java UnBanChar.java:44-47
void UnBanChar::info(Player& player, std::optional<std::string_view> /*message*/) {
	PacketSendUtility::sendMessage(player, "Syntax: //unban <player> [account|ip|full]");
}

} // namespace aion::gameserver::handlers::admincommands
