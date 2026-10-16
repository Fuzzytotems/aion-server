#pragma once

#include "aion/gameserver/handlers/consolecommands/ConsoleCommandsPrelude.h"

namespace aion::gameserver::handlers::consolecommands {

/**
 * ///clearusercoolt: removes the instance cooldowns of a player.
 */
class Clearusercoolt : public ConsoleCommand {
public:
	Clearusercoolt();

	void execute(Player& player, std::span<const std::string> params) override;

	static void clearAllInstanceCooldowns(Player& admin, Player& player);
};

} // namespace aion::gameserver::handlers::consolecommands
