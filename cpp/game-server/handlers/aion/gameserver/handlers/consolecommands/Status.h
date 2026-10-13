#pragma once

#include "aion/gameserver/handlers/consolecommands/ConsoleCommandsPrelude.h"

namespace aion::gameserver::handlers::consolecommands {

/**
 * ///status: sends the GM console's SM_GM_SHOW_PLAYER_STATUS of the named or targeted player.
 */
class Status : public ConsoleCommand {
public:
	Status();

	void execute(Player& admin, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::consolecommands
