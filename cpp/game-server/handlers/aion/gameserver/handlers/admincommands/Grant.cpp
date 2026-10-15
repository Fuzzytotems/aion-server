#include "aion/gameserver/handlers/admincommands/Grant.h"

#include <string>

#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/loginserver/LoginServer.h"
#include "aion/gameserver/utils/Util.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Grant);

Grant::Grant()
	: AdminCommand("grant", "Grants/revokes account permissions.",
		  "a <level> [name] - Grants the specified access level (default: target's account, optional: specified character's account). 0 will remove "
		  "the account's access level.\n"
		  "m <level> [name] - Grants the specified membership level (default: target's account, optional: specified character's account). 0 will "
		  "remove the account's membership level.\n") {
}

// Java Grant.java:26-67
void Grant::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() < 2) {
		sendInfo(admin);
		return;
	}
	int32_t type;
	if (commons::utils::StringUtils::equalsIgnoreCase("a", params[0])) {
		type = 1;
	} else if (commons::utils::StringUtils::equalsIgnoreCase("m", params[0])) {
		type = 2;
	} else {
		sendInfo(admin);
		return;
	}
	int32_t level = commons::utils::parseInt(params[1]);
	if (level < 0) {
		sendInfo(admin, "Level must not be negative.");
		return;
	}
	runtime::Ptr<Player> player;
	if (params.size() >= 3) {
		std::string playerName = Util::convertName(params[2]);
		player = World::getInstance().getPlayer(playerName);
		if (player == nullptr) {
			PacketSendUtility::sendPacket(admin, SM_SYSTEM_MESSAGE::STR_NO_SUCH_USER(playerName));
			return;
		}
	} else if (runtime::Ptr<Player> target = runtime::as<Player>(admin.getTarget())) { // parity= } else if (admin.getTarget() instanceof Player target) {
		player = target;
	} else {
		PacketSendUtility::sendPacket(admin, SM_SYSTEM_MESSAGE::STR_INVALID_TARGET());
		return;
	}
	if (type == 1) {
		if (!player->equals(admin) && player->getAccount()->getAccessLevel() >= admin.getAccount()->getAccessLevel()) {
			sendInfo(admin, "You are not allowed change the access level of players with the same or higher access level than your own.");
			return;
		}
	}
	LoginServer::getInstance().sendLsControlPacket(type, level, *player, admin);
}

} // namespace aion::gameserver::handlers::admincommands
