#include "aion/gameserver/handlers/admincommands/UnBanMac.h"

#include <string>

#include "aion/gameserver/network/BannedMacManager.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(UnBanMac);

UnBanMac::UnBanMac() : AdminCommand("unbanmac") {
}

// Java UnBanMac.java:20-33
void UnBanMac::execute(Player& player, std::span<const std::string> params) {
	if (params.size() < 1) { // parity= if (params == null || params.length < 1) {
		info(player, std::nullopt);
		return;
	}

	std::string address = params[0];
	bool result = BannedMacManager::getInstance().unbanAddress(address,
		"uban;mac=" + address + ", " + std::to_string(player.getObjectId()) + "; admin=" + player.getName());
	if (result)
		PacketSendUtility::sendMessage(player, "mac " + address + " has unbanned");
	else
		PacketSendUtility::sendMessage(player, "mac " + address + " is not banned");
}

// Java UnBanMac.java:35-38
void UnBanMac::info(Player& player, std::optional<std::string_view> /*message*/) {
	PacketSendUtility::sendMessage(player, "Syntax: //unbanmac <mac>");
}

} // namespace aion::gameserver::handlers::admincommands
