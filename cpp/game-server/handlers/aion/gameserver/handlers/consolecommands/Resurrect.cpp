#include "aion/gameserver/handlers/consolecommands/Resurrect.h"

#include "aion/gameserver/network/aion/serverpackets/SM_RESURRECT.h"

namespace aion::gameserver::handlers::consolecommands {

AION_CONSOLE_COMMAND(Resurrect);

// Java Resurrect.java:13-15
Resurrect::Resurrect() : ConsoleCommand("resurrect", "Resurrects a player.") {
}

// Java Resurrect.java:18-31
void Resurrect::execute(Player& admin, std::span<const std::string> /*params*/) {
	runtime::Ptr<Player> player = admin.isDead() ? runtime::Ptr<Player>(&admin) : runtime::as<Player>(admin.getTarget()); // parity= if (!((admin.isDead() ? admin : admin.getTarget()) instanceof Player player)) {
	if (player == nullptr) { // parity: (the same)
		sendInfo(admin, "Please select a player.");
		return;
	}

	if (!player->isDead()) {
		sendInfo(admin, player->equals(admin) ? "You're already alive." : player->getName() + " is already alive."); // parity= sendInfo(admin, player.equals(admin) ? "You're already alive." : player.getName() + " is already alive.");
		return;
	}

	player->setPlayerResActivate(true);
	PacketSendUtility::sendPacket(*player, SM_RESURRECT(admin)); // parity= PacketSendUtility.sendPacket(player, new SM_RESURRECT(admin));
}

} // namespace aion::gameserver::handlers::consolecommands
