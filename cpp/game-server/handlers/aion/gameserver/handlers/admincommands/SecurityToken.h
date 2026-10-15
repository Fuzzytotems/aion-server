#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //stoken: shows or generates the security token of a player's account. @author Artur
 */
class SecurityToken : public AdminCommand {
public:
	SecurityToken();

	void execute(Player& admin, std::span<const std::string> params) override;

	void info(Player& player, std::optional<std::string_view> message) override;
};

} // namespace aion::gameserver::handlers::admincommands
