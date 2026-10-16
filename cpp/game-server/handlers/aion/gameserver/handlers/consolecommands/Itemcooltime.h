#pragma once

#include "aion/gameserver/handlers/consolecommands/ConsoleCommandsPrelude.h"

namespace aion::gameserver::handlers::consolecommands {

/**
 * ///itemcooltime: removes the cooldowns of all items (//removecd's item part).
 */
class Itemcooltime : public ConsoleCommand {
public:
	Itemcooltime();

	void execute(Player& player, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::consolecommands
