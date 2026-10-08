#include "aion/gameserver/handlers/admincommands/UnBanIp.h"

#include <string>

#include "aion/gameserver/network/loginserver/LoginServer.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(UnBanIp);

UnBanIp::UnBanIp() : AdminCommand("unbanip") {
}

// Java UnBanIp.java:20-26
void UnBanIp::execute(Player& player, std::span<const std::string> params) {
	if (params.size() < 1) { // parity= if (params == null || params.length < 1) {
		PacketSendUtility::sendMessage(player, "Syntax: //unbanip <mask>");
		return;
	}
	LoginServer::getInstance().sendBanPacket(static_cast<int8_t>(2), 0, params[0], -1, player.getObjectId());
}

// Java UnBanIp.java:28-31
void UnBanIp::info(Player& player, std::optional<std::string_view> /*message*/) {
	PacketSendUtility::sendMessage(player, "Syntax: //unbanip <mask>");
}

} // namespace aion::gameserver::handlers::admincommands
