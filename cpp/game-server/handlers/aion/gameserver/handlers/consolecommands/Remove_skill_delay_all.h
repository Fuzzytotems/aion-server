#pragma once

#include "aion/gameserver/handlers/consolecommands/ConsoleCommandsPrelude.h"

namespace aion::gameserver::handlers::consolecommands {

/**
 * ///remove_skill_delay_all: enables/disables the GM's skill cooldowns.
 */
class Remove_skill_delay_all : public ConsoleCommand {
public:
	Remove_skill_delay_all();

	void execute(Player& player, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::consolecommands
