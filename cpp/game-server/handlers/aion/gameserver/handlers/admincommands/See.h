#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //see: lets you see hidden NPCs and players.
 *
 * @author Mathew
 */
class See : public AdminCommand {
public:
	See();

	void execute(Player& admin, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::admincommands
