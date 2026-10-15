#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //unbanmac: lifts a MAC ban of this game server. @author KID
 */
class UnBanMac : public AdminCommand {
public:
	UnBanMac();

	void execute(Player& admin, std::span<const std::string> params) override;

	void info(Player& player, std::optional<std::string_view> message) override;
};

} // namespace aion::gameserver::handlers::admincommands
