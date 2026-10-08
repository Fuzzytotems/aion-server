#include "aion/gameserver/handlers/admincommands/Ban.h"

#include <string>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dao/PlayerDAO.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/loginserver/LoginServer.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/Util.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Ban);

Ban::Ban() : AdminCommand("ban") {
}

// Java Ban.java:27-87
void Ban::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() < 1) { // parity= if (params == null || params.length < 1) {
		PacketSendUtility::sendMessage(admin, "Syntax: //ban <player> [account|ip|full] [time in minutes]");
		return;
	}

	// We need to get player's account ID
	std::string name = Util::convertName(params[0]);
	int32_t accountId = 0;
	std::string accountIp = "";

	// First, try to find player in the World
	runtime::Ptr<Player> player = World::getInstance().getPlayer(name);
	if (player != nullptr) {
		std::shared_ptr<AionConnection> connection = player->getClientConnection(); // parity: the connection of the two Java calls below
		if (connection == nullptr) // parity: Java's NullPointerException of player.getClientConnection().getAccount(), explicit
			throw runtime::NullPointerException("Player.getClientConnection()"); // parity: (the same)
		accountId = connection->getAccount()->getId(); // parity= accountId = player.getClientConnection().getAccount().getId();
		accountIp = connection->getIP(); // parity= accountIp = player.getClientConnection().getIP();
	}

	// Second, try to get account ID of offline player from database
	if (accountId == 0)
		accountId = dao::PlayerDAO::getAccountIdByName(name);

	// Third, fail
	if (accountId == 0) {
		PacketSendUtility::sendMessage(admin, "Player " + name + " was not found!");
		PacketSendUtility::sendMessage(admin, "Syntax: //ban <player> [account|ip|full] [time in minutes]");
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
			PacketSendUtility::sendMessage(admin, "Syntax: //ban <player> [account|ip|full] [time in minutes]");
			return;
		}
	}

	int32_t time = 0; // Default: infinity
	if (params.size() > 2) {
		try {
			time = commons::utils::parseInt(params[2]);
		} catch (const commons::utils::NumberFormatException&) { // parity= } catch (NumberFormatException e) {
			PacketSendUtility::sendMessage(admin, "Syntax: //ban <player> [account|ip|full] [time in minutes]");
			return;
		}
	}

	if (time == 0) {
		time = 60 * 24 * 365 * 10; // pseudo infinity. TODO: rework
	}

	LoginServer::getInstance().sendBanPacket(type, accountId, accountIp, time, admin.getObjectId());
}

// Java Ban.java:89-92
void Ban::info(Player& player, std::optional<std::string_view> /*message*/) {
	PacketSendUtility::sendMessage(player, "Syntax: //ban <player> [account|ip|full] [time in minutes]");
}

} // namespace aion::gameserver::handlers::admincommands
