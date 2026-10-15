#pragma once

#include "aion/gameserver/handlers/consolecommands/ConsoleCommandsPrelude.h"

namespace aion::gameserver::handlers::consolecommands {

/**
 * ///skill: sends the GM console's skill list (SM_GM_SHOW_PLAYER_SKILLS) of the named or targeted player.
 */
class Skill : public ConsoleCommand {
public:
	Skill();

	void execute(Player& admin, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::consolecommands
