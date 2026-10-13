#include "aion/gameserver/handlers/consolecommands/Itemcooltime.h"

#include "aion/gameserver/handlers/admincommands/RemoveCd.h"

namespace aion::gameserver::handlers::consolecommands {

AION_CONSOLE_COMMAND(Itemcooltime);

// Java Itemcooltime.java:13-15
Itemcooltime::Itemcooltime() : ConsoleCommand("itemcooltime", "Removes cooldowns of all items.") {
}

// Java Itemcooltime.java:18-20
void Itemcooltime::execute(Player& player, std::span<const std::string> /*params*/) {
	admincommands::RemoveCd::removeItemCooldowns(player);
}

} // namespace aion::gameserver::handlers::consolecommands
