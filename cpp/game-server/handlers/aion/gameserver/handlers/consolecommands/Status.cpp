#include "aion/gameserver/handlers/consolecommands/Status.h"

#include "aion/gameserver/network/aion/serverpackets/SM_GM_SHOW_PLAYER_STATUS.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::handlers::consolecommands {

AION_CONSOLE_COMMAND(Status);

// Java Status.java:14-16
Status::Status() : ConsoleCommand("status") {
}

// Java Status.java:19-30
void Status::execute(Player& admin, std::span<const std::string> params) {
	runtime::Ptr<Player> target; // parity= Player target = null;
	if (!params.empty()) {
		target = World::getInstance().getPlayer(params[0]);
	}
	if (target == nullptr && runtime::as<Player>(admin.getTarget()) != nullptr) { // parity= if (target == null && admin.getTarget() instanceof Player) {
		target = runtime::as<Player>(admin.getTarget()); // parity= target = (Player) admin.getTarget();
	}
	if (target != nullptr) {
		PacketSendUtility::sendPacket(admin, SM_GM_SHOW_PLAYER_STATUS(*target)); // parity= PacketSendUtility.sendPacket(admin, new SM_GM_SHOW_PLAYER_STATUS(target));
	}
}

} // namespace aion::gameserver::handlers::consolecommands
