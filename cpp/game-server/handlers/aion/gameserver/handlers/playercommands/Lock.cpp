#include "aion/gameserver/handlers/playercommands/Lock.h"

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/loginserver/LoginServer.h"
#include "aion/gameserver/network/loginserver/serverpackets/SM_CHANGE_ALLOWED_HDD_SERIAL.h"

namespace aion::gameserver::handlers::playercommands {

AION_PLAYER_COMMAND(Lock);

// Java Lock.java:13-18
Lock::Lock()
	: PlayerCommand("lock", "Enables/disables blocking logins from other computers.",
		  "enable - Allows login from only this computer.\n"
		  "disable - Allows login from any computer.\n") {
}

// Java Lock.java:20-43
void Lock::execute(Player& player, std::span<const std::string> params) {
	if (params.empty()) { // parity= if (params.length == 0) {
		sendInfo(player);
		return;
	}

	if (commons::utils::StringUtils::equalsIgnoreCase("enable", params[0])) { // parity= if ("enable".equalsIgnoreCase(params[0])) {
		std::string hddSerial = player.getClientConnection()->getHddSerial();
		if (hddSerial.empty()) { // parity= if (hddSerial == null || hddSerial.isEmpty()) {
			sendInfo(player, "Couldn't lock your account. Please re-log and try again.");
			return;
		}
		player.getAccount()->setAllowedHddSerial(hddSerial);
		network::loginserver::LoginServer::getInstance().sendPacket(
			network::loginserver::serverpackets::SM_CHANGE_ALLOWED_HDD_SERIAL(*player.getAccount()));
		sendInfo(player, "Your account is now locked. You will not be able to login from any other computer from now on.");
	} else if (commons::utils::StringUtils::equalsIgnoreCase("disable", params[0])) { // parity= } else if ("disable".equalsIgnoreCase(params[0])) {
		player.getAccount()->setAllowedHddSerial(std::nullopt);
		network::loginserver::LoginServer::getInstance().sendPacket(
			network::loginserver::serverpackets::SM_CHANGE_ALLOWED_HDD_SERIAL(*player.getAccount()));
		sendInfo(player, "Your account is unlocked. You can login from any computer again.");
	} else {
		sendInfo(player, "Invalid parameter.");
	}
}

} // namespace aion::gameserver::handlers::playercommands
