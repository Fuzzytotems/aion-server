#pragma once

#include "aion/gameserver/handlers/playercommands/PlayerCommandsPrelude.h"

namespace aion::gameserver::handlers::playercommands {

/**
 * .pvp: joins or leaves the custom PvP map, or tells how many players are on it.
 *
 * @author Yeats
 */
class Pvp : public PlayerCommand {
public:
	Pvp();

	void execute(Player& player, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::playercommands
