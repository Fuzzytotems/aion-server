#include "aion/gameserver/handlers/consolecommands/Inventory.h"

namespace aion::gameserver::handlers::consolecommands {

AION_CONSOLE_COMMAND(Inventory);

// Java Inventory.java:11-12
Inventory::Inventory() : ConsoleCommand("inventory") {
}

// Java Inventory.java:15-40 (only `return;`: the rest of the body is a comment)
void Inventory::execute(Player& player, std::span<const std::string> /*params*/) {
	return;
}

} // namespace aion::gameserver::handlers::consolecommands
