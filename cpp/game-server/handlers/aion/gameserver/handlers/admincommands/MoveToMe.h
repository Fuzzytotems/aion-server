#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //movetome: teleports a player (optionally his group or alliance) to the GM.
 *
 * @author Neon
 */
class MoveToMe : public AdminCommand {
public:
	MoveToMe();

	void execute(Player& player, std::span<const std::string> params) override;

private:
	void teleportPlayer(Player& playerToMove, Player& admin);
};

} // namespace aion::gameserver::handlers::admincommands
