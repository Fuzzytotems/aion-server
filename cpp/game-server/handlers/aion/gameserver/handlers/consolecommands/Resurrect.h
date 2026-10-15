#pragma once

#include "aion/gameserver/handlers/consolecommands/ConsoleCommandsPrelude.h"

namespace aion::gameserver::handlers::consolecommands {

/**
 * ///resurrect: offers the resurrection to the dead GM or to the targeted dead player.
 */
class Resurrect : public ConsoleCommand {
public:
	Resurrect();

	void execute(Player& admin, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::consolecommands
