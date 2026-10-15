#pragma once

#include "aion/gameserver/handlers/playercommands/PlayerCommandsPrelude.h"

namespace aion::gameserver::handlers::playercommands {

/**
 * .noexp: enables or disables your ability to gain experience.
 */
class NoExp : public PlayerCommand {
public:
	NoExp();

	void execute(Player& player, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::playercommands
