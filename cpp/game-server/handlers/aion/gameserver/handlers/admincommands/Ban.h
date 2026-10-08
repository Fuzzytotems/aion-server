#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //ban: bans the account or the IP of a player in the login server. @author Watson
 */
class Ban : public AdminCommand {
public:
	Ban();

	void execute(Player& admin, std::span<const std::string> params) override;

	void info(Player& player, std::optional<std::string_view> message) override;
};

} // namespace aion::gameserver::handlers::admincommands
