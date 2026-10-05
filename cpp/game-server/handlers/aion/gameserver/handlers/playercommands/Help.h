#pragma once

#include "aion/gameserver/handlers/playercommands/PlayerCommandsPrelude.h"

namespace aion::gameserver::handlers::playercommands {

/**
 * .help: lists all commands you are allowed to use.
 */
class Help : public PlayerCommand {
public:
	Help();

	void execute(Player& player, std::span<const std::string> params) override;

private:
	std::vector<ChatCommand*> findAllowedCommands(Player& player);
};

} // namespace aion::gameserver::handlers::playercommands
