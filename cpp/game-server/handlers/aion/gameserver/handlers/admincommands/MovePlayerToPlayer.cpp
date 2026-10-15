#include "aion/gameserver/handlers/admincommands/MovePlayerToPlayer.h"

#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/utils/Util.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(MovePlayerToPlayer);

MovePlayerToPlayer::MovePlayerToPlayer() : AdminCommand("moveplayertoplayer") {
}

// Java MovePlayerToPlayer.java:21-51
void MovePlayerToPlayer::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() < 2) { // parity= if (params == null || params.length < 2) {
		PacketSendUtility::sendMessage(admin, "syntax //moveplayertoplayer <characterNameToMove> <characterNameDestination>");
		return;
	}

	runtime::Ptr<Player> playerToMove = World::getInstance().getPlayer(Util::convertName(params[0]));
	if (playerToMove == nullptr) {
		PacketSendUtility::sendMessage(admin, "The specified player is not online.");
		return;
	}

	runtime::Ptr<Player> playerDestination = World::getInstance().getPlayer(Util::convertName(params[1]));
	if (playerDestination == nullptr) {
		PacketSendUtility::sendMessage(admin, "The destination player is not online.");
		return;
	}

	if (playerToMove->equals(*playerDestination)) {
		PacketSendUtility::sendMessage(admin, "Cannot move the specified player to their own position.");
		return;
	}

	TeleportService::teleportTo(*playerToMove, playerDestination->getWorldId(), playerDestination->getInstanceId(), playerDestination->getX(),
		playerDestination->getY(), playerDestination->getZ(), playerDestination->getHeading());

	PacketSendUtility::sendMessage(admin,
		"Teleported player " + playerToMove->getName() + " to the location of player " + playerDestination->getName() + ".");
	PacketSendUtility::sendMessage(*playerToMove, "You have been teleported by an administrator.");
}

// Java MovePlayerToPlayer.java:53-56
void MovePlayerToPlayer::info(Player& player, std::optional<std::string_view> /*message*/) {
	PacketSendUtility::sendMessage(player, "syntax //moveplayertoplayer <characterNameToMove> <characterNameDestination>");
}

} // namespace aion::gameserver::handlers::admincommands
