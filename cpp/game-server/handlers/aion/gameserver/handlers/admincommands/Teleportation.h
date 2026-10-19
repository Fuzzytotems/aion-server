#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //teleportation: toggles the teleportation mode of mouse moves.
 *
 * @author cura, Neon
 */
class Teleportation : public AdminCommand {
public:
	Teleportation();

	void execute(Player& admin, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::admincommands
