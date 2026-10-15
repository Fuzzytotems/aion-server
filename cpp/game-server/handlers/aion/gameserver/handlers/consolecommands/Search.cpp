#include "aion/gameserver/handlers/consolecommands/Search.h"

#include "aion/gameserver/network/aion/serverpackets/SM_GM_SEARCH.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::handlers::consolecommands {

AION_CONSOLE_COMMAND(Search);

// Java Search.java:14-16
Search::Search() : ConsoleCommand("search") {
}

// Java Search.java:19-25
void Search::execute(Player& admin, std::span<const std::string> params) {
	if (!params.empty()) {
		runtime::Ptr<Player> p = World::getInstance().getPlayer(params[0]); // parity= Player p = World.getInstance().getPlayer(params[0]);
		if (p != nullptr)
			PacketSendUtility::sendPacket(admin, SM_GM_SEARCH(*p)); // parity= PacketSendUtility.sendPacket(admin, new SM_GM_SEARCH(p));
	}
}

} // namespace aion::gameserver::handlers::consolecommands
