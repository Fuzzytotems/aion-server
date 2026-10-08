#include "aion/gameserver/handlers/admincommands/Res.h"

#include <string_view>

#include "aion/gameserver/network/aion/serverpackets/SM_RESURRECT.h"
#include "aion/gameserver/services/player/PlayerReviveService.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Res);

Res::Res() : AdminCommand("res") {
}

// Java Res.java:20-51
void Res::execute(Player& admin, std::span<const std::string> params) {
	const runtime::Ptr<VisibleObject> target = admin.getTarget();
	if (target == nullptr) {
		PacketSendUtility::sendMessage(admin, "No target selected.");
		return;
	}

	if (runtime::as<Player>(target) == nullptr) { // parity= if (!(target instanceof Player)) {
		PacketSendUtility::sendMessage(admin, "You can only resurrect other players.");
		return;
	}

	const runtime::Ptr<Player> player = runtime::cast<Player>(target); // parity= final Player player = (Player) target;
	if (!player->isDead()) {
		PacketSendUtility::sendMessage(admin, "That player is already alive.");
		return;
	}

	// Default action is to prompt for resurrect.
	if (params.empty() || std::string_view("prompt").starts_with(params[0])) { // parity= if (params == null || params.length == 0 || ("prompt").startsWith(params[0])) {
		player->setPlayerResActivate(true);
		PacketSendUtility::sendPacket(*player, SM_RESURRECT(admin));
		return;
	}

	if (std::string_view("instant").starts_with(params[0])) { // parity= if (("instant").startsWith(params[0])) {
		PlayerReviveService::skillRevive(*player);
		return;
	}

	PacketSendUtility::sendMessage(admin, "[Resurrect] Usage: target player and use //res <instant|prompt>");
}

// Java Res.java:53-56
void Res::info([[maybe_unused]] Player& player, [[maybe_unused]] std::optional<std::string_view> message) {
	// TODO Auto-generated method stub
}

} // namespace aion::gameserver::handlers::admincommands
