#pragma once

#include "aion/gameserver/handlers/playercommands/PlayerCommandsPrelude.h"

namespace aion::gameserver::handlers::playercommands {

/**
 * .del: deletes items from your inventory.
 */
class Del : public PlayerCommand {
public:
	Del();

	void execute(Player& player, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::playercommands
