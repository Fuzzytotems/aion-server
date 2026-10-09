#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //kick: disconnects players from the server.
 *
 * @author Elusive, Neon
 */
class Kick : public AdminCommand {
public:
	Kick();

	void execute(Player& admin, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::admincommands
