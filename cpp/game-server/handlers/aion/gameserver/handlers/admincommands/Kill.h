#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //kill: kills the specified NPC(s) or player.
 */
class Kill : public AdminCommand {
public:
	Kill();

	void execute(Player& player, std::span<const std::string> params) override;

private:
	bool kill(Player& attacker, Creature& target);
};

} // namespace aion::gameserver::handlers::admincommands
