#include "aion/gameserver/handlers/admincommands/PasskeyReset.h"

#include <string>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dao/PlayerDAO.h"
#include "aion/gameserver/dao/PlayerPasskeyDAO.h"
#include "aion/gameserver/network/loginserver/LoginServer.h"
#include "aion/gameserver/utils/Util.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(PasskeyReset);

PasskeyReset::PasskeyReset() : AdminCommand("passkeyreset") {
}

// Java PasskeyReset.java:22-50. `newPasskey.length()` counts UTF-16 units
void PasskeyReset::execute(Player& player, std::span<const std::string> params) {
	if (params.size() < 2) {
		PacketSendUtility::sendMessage(player, "syntax: //passkeyreset <player> <passkey>");
		return;
	}

	std::string name = Util::convertName(params[0]);
	int32_t accountId = dao::PlayerDAO::getAccountIdByName(name);
	if (accountId == 0) {
		PacketSendUtility::sendMessage(player, "player " + name + " can't find!");
		PacketSendUtility::sendMessage(player, "syntax: //passkeyreset <player> <passkey>");
		return;
	}

	try {
		commons::utils::parseInt(params[1]);
	} catch (const commons::utils::NumberFormatException&) { // parity= } catch (NumberFormatException e) {
		PacketSendUtility::sendMessage(player, "parameters should be number!");
		return;
	}

	std::string newPasskey = params[1];
	const size_t length = commons::utils::StringUtils::toUtf16(newPasskey).size(); // parity: Java's String.length()
	if (!(length > 5 && length < 9)) { // parity= if (!(newPasskey.length() > 5 && newPasskey.length() < 9)) {
		PacketSendUtility::sendMessage(player, "passkey is 6~8 digits!");
		return;
	}

	dao::PlayerPasskeyDAO::updateForcePlayerPasskey(accountId, newPasskey);
	LoginServer::getInstance().sendBanPacket(static_cast<int8_t>(2), accountId, "", -1, player.getObjectId());
}

// Java PasskeyReset.java:52-55
void PasskeyReset::info(Player& player, std::optional<std::string_view> /*message*/) {
	PacketSendUtility::sendMessage(player, "syntax: //passkeyreset <player> <passkey>");
}

} // namespace aion::gameserver::handlers::admincommands
