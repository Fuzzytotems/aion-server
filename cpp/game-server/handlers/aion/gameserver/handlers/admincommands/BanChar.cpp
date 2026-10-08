#include "aion/gameserver/handlers/admincommands/BanChar.h"

#include <string>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/dao/PlayerDAO.h"
#include "aion/gameserver/services/PunishmentService.h"
#include "aion/gameserver/utils/Util.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(BanChar);

BanChar::BanChar() : AdminCommand("banchar") {
}

// Java BanChar.java:24-71
void BanChar::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() < 3) { // parity= if (params == null || params.length < 3) {
		sendInfo(admin, true);
		return;
	}

	int32_t playerId = 0;
	std::string playerName = Util::convertName(params[0]);

	// First, try to find player in the World
	runtime::Ptr<Player> player = World::getInstance().getPlayer(playerName);
	if (player != nullptr)
		playerId = player->getObjectId();

	// Second, try to get player Id from offline player from database
	if (playerId == 0)
		playerId = dao::PlayerDAO::getPlayerIdByName(playerName);

	// Third, fail
	if (playerId == 0) {
		PacketSendUtility::sendMessage(admin, "Player " + playerName + " was not found!");
		sendInfo(admin, true);
		return;
	}

	int32_t dayCount = -1;
	try {
		dayCount = commons::utils::parseInt(params[1]);
	} catch (const commons::utils::NumberFormatException&) { // parity= } catch (NumberFormatException e) {
		PacketSendUtility::sendMessage(admin, "Second parameter is not an int");
		sendInfo(admin, true);
		return;
	}

	if (dayCount < 0) {
		PacketSendUtility::sendMessage(admin, "Second parameter has to be a positive daycount or 0 for infinity");
		sendInfo(admin, true);
		return;
	}

	std::string reason = Util::convertName(params[2]);
	for (size_t itr = 3; itr < params.size(); itr++)
		reason += " " + params[itr];

	PacketSendUtility::sendMessage(admin, "Char " + playerName + " is now banned for the next " + std::to_string(dayCount) + " days!");
	services::PunishmentService::banChar(playerId, dayCount, reason);
}

// Java BanChar.java:73-76
void BanChar::info(Player& player, std::optional<std::string_view> /*message*/) {
	sendInfo(player, false);
}

// Java BanChar.java:78-82
void BanChar::sendInfo(Player& player, bool withNote) {
	PacketSendUtility::sendMessage(player, "Syntax: //banChar <playername> <days>/0 (for permanent) <reason>");
	if (withNote)
		PacketSendUtility::sendMessage(player, "Note: the current day is defined as a whole day even if it has just a few hours left!");
}

} // namespace aion::gameserver::handlers::admincommands
