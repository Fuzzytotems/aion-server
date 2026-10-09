#include "aion/gameserver/handlers/admincommands/AddCube.h"

#include "aion/gameserver/services/CubeExpandService.h"
#include "aion/gameserver/utils/Util.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(AddCube);

AddCube::AddCube() : AdminCommand("addcube") {
}

// Java AddCube.java:20-43
void AddCube::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() != 1) { // parity= if (params.length != 1) {
		PacketSendUtility::sendMessage(admin, "Syntax: //addcube <player name>");
		return;
	}

	runtime::Ptr<Player> receiver = nullptr;

	receiver = World::getInstance().getPlayer(Util::convertName(params[0]));
	if (receiver == nullptr) {
		PacketSendUtility::sendMessage(admin, "The player " + Util::convertName(params[0]) + " is not online.");
		return;
	}

	if (CubeExpandService::canExpand(*receiver)) {
		CubeExpandService::npcExpand(*receiver);
		PacketSendUtility::sendMessage(admin, "9 cube slots successfully added to player " + receiver->getName() + "!");
		PacketSendUtility::sendMessage(*receiver, "Admin " + admin.getName() + " gave you a cube expansion!");
	} else {
		PacketSendUtility::sendMessage(admin, "Cube expansion cannot be added to " + receiver->getName()
			+ "!\nReason: player cube already fully expanded.");
		return;
	}
}

// Java AddCube.java:45-48
void AddCube::info(Player& admin, std::optional<std::string_view> /*message*/) {
	PacketSendUtility::sendMessage(admin, "Syntax: //addcube <player name>");
}

} // namespace aion::gameserver::handlers::admincommands
