#pragma once

#include "aion/gameserver/handlers/consolecommands/ConsoleCommandsPrelude.h"

namespace aion::gameserver::handlers::consolecommands {

/**
 * ///teleportto: the GM console's "Transport" button, functionally //goto.
 *
 * @author Neon
 */
class Teleportto : public ConsoleCommand {
public:
	Teleportto();

	void execute(Player& player, std::span<const std::string> params) override;

	std::string getSyntaxInfo() override;
};

} // namespace aion::gameserver::handlers::consolecommands
