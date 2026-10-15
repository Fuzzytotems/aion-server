#include "aion/gameserver/handlers/admincommands/SecurityToken.h"

#include <string>

#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/services/player/SecurityTokenService.h"
#include "aion/gameserver/utils/Util.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(SecurityToken);

SecurityToken::SecurityToken() : AdminCommand("stoken") {
}

// Java SecurityToken.java:22-49
void SecurityToken::execute(Player& player, std::span<const std::string> params) {
	if (params.size() < 1 || (params[0] == "show" && params.size() < 2)) { // parity= if (params.length < 1 || (params[0].equals("show") && params.length < 2)) {
		PacketSendUtility::sendMessage(player, "Syntax: //stoken <playername> || //stoken show <playername>");
		return;
	}

	runtime::Ptr<Player> receiver = nullptr;

	if (params[0] == "show") { // parity= if (params[0].equals("show")) {
		receiver = World::getInstance().getPlayer(Util::convertName(params[1]));
		if (receiver == nullptr) {
			PacketSendUtility::sendMessage(player, "Can't find this player, maybe he's not online");
			return;
		}

		if ("" != receiver->getAccount()->getSecurityToken()) { // parity= if (!"".equals(receiver.getAccount().getSecurityToken())) {
			PacketSendUtility::sendMessage(player, "The Security Token of this player is: " + receiver->getAccount()->getSecurityToken());
		} else {
			PacketSendUtility::sendMessage(player, "This player haven't an Security Token!");
		}
	} else {
		receiver = World::getInstance().getPlayer(Util::convertName(params[0]));
		if (receiver == nullptr) {
			PacketSendUtility::sendMessage(player, "Can't find this player, maybe he's not online");
			return;
		}
		services::player::SecurityTokenService::generateToken(*receiver->getAccount());
	}
}

// Java SecurityToken.java:51-54
void SecurityToken::info(Player& admin, std::optional<std::string_view> /*message*/) {
	PacketSendUtility::sendMessage(admin, "Syntax: //stoken <playername> || //stoken show <playername>");
}

} // namespace aion::gameserver::handlers::admincommands
