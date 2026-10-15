#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //coords: shows the target's current coordinates.
 */
class Coords : public AdminCommand {
public:
	Coords();

	void execute(Player& player, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::admincommands
