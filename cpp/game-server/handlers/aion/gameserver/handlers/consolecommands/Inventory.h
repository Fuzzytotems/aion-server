#pragma once

#include "aion/gameserver/handlers/consolecommands/ConsoleCommandsPrelude.h"

namespace aion::gameserver::handlers::consolecommands {

/**
 * ///inventory: the GM console's inventory view; Java's body returns at once (the rest is commented out).
 */
class Inventory : public ConsoleCommand {
public:
	Inventory();

	void execute(Player& player, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::consolecommands
