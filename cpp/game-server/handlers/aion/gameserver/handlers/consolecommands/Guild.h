#pragma once

#include "aion/gameserver/handlers/consolecommands/ConsoleCommandsPrelude.h"

namespace aion::gameserver::handlers::consolecommands {

/**
 * ///guild: displays the legion of the given player (SM_GM_SHOW_LEGION_INFO and the member list).
 */
class Guild : public ConsoleCommand {
public:
	Guild();

	void execute(Player& admin, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::consolecommands
