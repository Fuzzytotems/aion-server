#include "aion/gameserver/handlers/admincommands/Kick.h"

#include <memory>

#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/Util.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Kick);

Kick::Kick()
	: AdminCommand("kick", "Disconnects players from the server.",
		  "<name> - Disconnects the player with the specified name.\n"
		  "ALL - Disconnects everyone (parameter must be typed in uppercase, for safety).\n") {
}

// Java Kick.java:24-49
void Kick::execute(Player& admin, std::span<const std::string> params) {
	if (params.empty()) {
		sendInfo(admin);
		return;
	}

	if (params[0] == "ALL") { // Java: "ALL".equals(params[0])
		if (World::getInstance().getAllPlayers().size() == 1) {
			sendInfo(admin, "There is nobody online to kick.");
			return;
		}
		World::getInstance().forEachPlayer([&admin](Player& player) {
			if (!player.equals(admin)) {
				std::shared_ptr<AionConnection> connection = player.getClientConnection();
				if (connection == nullptr) // Java: player.getClientConnection().close(...) on null
					throw runtime::NullPointerException("Player.getClientConnection()");
				connection->close(SM_SYSTEM_MESSAGE::STR_KICK_CHARACTER());
				PacketSendUtility::sendPacket(admin, SM_SYSTEM_MESSAGE::STR_USER_KICKED(player.getName()));
			}
		});
	} else {
		runtime::Ptr<Player> player = World::getInstance().getPlayer(Util::convertName(params[0]));
		if (player == nullptr) {
			PacketSendUtility::sendPacket(admin, SM_SYSTEM_MESSAGE::STR_BUDDYLIST_NO_OFFLINE_CHARACTER());
			return;
		}
		std::shared_ptr<AionConnection> connection = player->getClientConnection();
		if (connection == nullptr) // Java: player.getClientConnection().close(...) on null
			throw runtime::NullPointerException("Player.getClientConnection()");
		connection->close(SM_SYSTEM_MESSAGE::STR_KICK_CHARACTER());
		PacketSendUtility::sendPacket(admin, SM_SYSTEM_MESSAGE::STR_USER_KICKED(player->getName()));
	}
}

} // namespace aion::gameserver::handlers::admincommands
