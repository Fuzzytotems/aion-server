#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //unbanchar: lifts a character ban. @author nrg
 */
class UnBanChar : public AdminCommand {
public:
	UnBanChar();

	void execute(Player& admin, std::span<const std::string> params) override;

	void info(Player& player, std::optional<std::string_view> message) override;
};

} // namespace aion::gameserver::handlers::admincommands
