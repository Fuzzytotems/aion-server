#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //passkeyreset: sets a new passkey for a player's account. @author cura
 */
class PasskeyReset : public AdminCommand {
public:
	PasskeyReset();

	void execute(Player& admin, std::span<const std::string> params) override;

	void info(Player& player, std::optional<std::string_view> message) override;
};

} // namespace aion::gameserver::handlers::admincommands
