#include "aion/gameserver/handlers/admincommands/MoveToMe.h"

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/utils/Util.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(MoveToMe);

// Java MoveToMe.java:17-22
MoveToMe::MoveToMe()
	: AdminCommand("movetome", "Teleports a player (optional his team) to the user.",
		  "<name> - Teleports only the player.\n"
		  "<name> <(g)rp|(a)lli> - Teleports either the players group or his alliance including him.\n") {
}

// Java MoveToMe.java:24-61
void MoveToMe::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() < 1) {
		sendInfo(admin);
		return;
	}
	std::string playerName = Util::convertName(params[0]);
	runtime::Ptr<Player> playerToMove = World::getInstance().getPlayer(playerName);
	if (playerToMove == nullptr) {
		PacketSendUtility::sendPacket(admin, SM_SYSTEM_MESSAGE::STR_NO_SUCH_USER(playerName));
		return;
	}
	if (params.size() >= 2) {
		runtime::Ptr<TemporaryPlayerTeam> teamToMove;
		std::string type = commons::utils::StringUtils::toLowerCase(params[1]); // parity= switch (params[1].toLowerCase()) {
		if (type == "g" || type == "grp" || type == "group") { // parity= case "g": case "grp": case "group":
			teamToMove = playerToMove->getCurrentGroup();
		} else if (type == "a" || type == "alli" || type == "alliance") { // parity= case "a": case "alli": case "alliance":
			teamToMove = playerToMove->getCurrentTeam();
		} else {
			sendInfo(admin);
			return;
		}
		if (teamToMove == nullptr) {
			sendInfo(admin, name(*playerToMove) + " currently has no team.");
			return;
		}
		for (const runtime::Ptr<Player>& p : teamToMove->getOnlineMembers()) // parity= teamToMove.getOnlineMembers().forEach(p -> teleportPlayer(p, admin));
			teleportPlayer(*p, admin); // parity: (the forEach above)
	} else {
		teleportPlayer(*playerToMove, admin);
	}
}

// Java MoveToMe.java:63-67
void MoveToMe::teleportPlayer(Player& playerToMove, Player& admin) {
	TeleportService::teleportTo(playerToMove, *admin.getPosition());
	sendInfo(admin, "Teleported " + name(playerToMove) + " to your location.");
	sendInfo(playerToMove, "You have been teleported by " + name(admin) + ".");
}

} // namespace aion::gameserver::handlers::admincommands
