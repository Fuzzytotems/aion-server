#pragma once

#include "aion/gameserver/handlers/playercommands/PlayerCommandsPrelude.h"

namespace aion::gameserver::handlers::playercommands {

/**
 * .questrestart: restarts an active quest (back to its first step).
 *
 * @author ginho1, Neon
 */
class Questrestart : public PlayerCommand {
public:
	Questrestart();

	void execute(Player& player, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::playercommands
