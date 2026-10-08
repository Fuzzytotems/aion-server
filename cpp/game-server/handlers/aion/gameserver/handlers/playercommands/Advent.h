#pragma once

#include "aion/gameserver/handlers/playercommands/PlayerCommandsPrelude.h"

namespace aion::gameserver::handlers::playercommands {

/**
 * .advent: gets the advent reward of the day.
 *
 * @author Neon
 */
class Advent : public PlayerCommand {
public:
	Advent();

	void execute(Player& player, std::span<const std::string> params) override;

	bool validateAccess(Player& player) override;
};

} // namespace aion::gameserver::handlers::playercommands
