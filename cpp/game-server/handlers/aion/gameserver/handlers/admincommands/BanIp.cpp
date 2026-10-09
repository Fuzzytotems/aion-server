#include "aion/gameserver/handlers/admincommands/BanIp.h"

#include <string>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/network/loginserver/LoginServer.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(BanIp);

BanIp::BanIp() : AdminCommand("banip") {
}

// Java BanIp.java:20-41
void BanIp::execute(Player& player, std::span<const std::string> params) {
	if (params.size() < 1) { // parity= if (params == null || params.length < 1) {
		PacketSendUtility::sendMessage(player, "Syntax: //banip <mask> [time in minutes]");
		return;
	}

	std::string mask = params[0];

	int32_t time = 0; // Default: infinity
	if (params.size() > 1) {
		try {
			time = commons::utils::parseInt(params[1]);
		} catch (const commons::utils::NumberFormatException& e) { // parity= } catch (NumberFormatException e) {
			info(player, std::string_view(e.what())); // parity= info(player, e.getMessage());
			return;
		}
	}

	if (time == 0) {
		time = 60 * 24 * 365 * 10; // pseudo infinity
	}

	LoginServer::getInstance().sendBanPacket(static_cast<int8_t>(2), 0, mask, time, player.getObjectId());
}

// Java BanIp.java:43-46
void BanIp::info(Player& player, std::optional<std::string_view> /*message*/) {
	PacketSendUtility::sendMessage(player, "Syntax: //banip <mask> [time in minutes]");
}

} // namespace aion::gameserver::handlers::admincommands
