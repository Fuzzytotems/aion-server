#pragma once

#include <vector>

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //debug: helps fixing runtime problems (the connected clients and players, disconnecting players stuck outside the world).
 *
 * @author Neon
 */
class Debug : public AdminCommand {
public:
	Debug();

	void execute(Player& admin, std::span<const std::string> params) override;

private:
	std::vector<runtime::Ptr<Player>> findConnectedPlayers();
};

} // namespace aion::gameserver::handlers::admincommands
