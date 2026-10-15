#pragma once

#include "aion/gameserver/handlers/consolecommands/ConsoleCommandsPrelude.h"

namespace aion::gameserver::handlers::consolecommands {

/**
 * ///classup: promotes a player's class.
 *
 * @author ginho1, Neon
 */
class Classup : public ConsoleCommand {
public:
	Classup();

	void execute(Player& admin, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::consolecommands
