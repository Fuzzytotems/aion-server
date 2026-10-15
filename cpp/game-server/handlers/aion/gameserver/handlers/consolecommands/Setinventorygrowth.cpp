#include "aion/gameserver/handlers/consolecommands/Setinventorygrowth.h"

#include "aion/gameserver/services/CubeExpandService.h"

namespace aion::gameserver::handlers::consolecommands {

AION_CONSOLE_COMMAND(Setinventorygrowth);

Setinventorygrowth::Setinventorygrowth() : ConsoleCommand("setinventorygrowth") {
}

// Java Setinventorygrowth.java:17-35
void Setinventorygrowth::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() < 1) {
		info(admin, std::nullopt); // parity= info(admin, null);
		return;
	}

	runtime::Ptr<Player> target = runtime::as<Player>(admin.getTarget()); // parity= final Player player = admin.getTarget() instanceof Player target ? target : admin;
	Player& player = target != nullptr ? *target : admin; // parity: (the same)

	if (CubeExpandService::canExpand(player)) {
		CubeExpandService::npcExpand(player);
		PacketSendUtility::sendMessage(admin, "9 cube slots successfully added to player " + player.getName() + "!");
		PacketSendUtility::sendMessage(player, "Admin " + admin.getName() + " gave you a cube expansion!");
	} else {
		PacketSendUtility::sendMessage(admin, "Cube expansion cannot be added to " + player.getName() + "!\nReason: player cube already fully expanded.");
		return;
	}
}

// Java Setinventorygrowth.java:37-40
void Setinventorygrowth::info(Player& admin, std::optional<std::string_view> /*message*/) {
	PacketSendUtility::sendMessage(admin, "syntax ///setinventorygrowth <?>");
}

} // namespace aion::gameserver::handlers::consolecommands
