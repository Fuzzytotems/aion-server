#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //invis: sets/unsets advanced invisibility (the default login.execute_commands' first entry, m5j-plan.md §5.2 T0).
 *
 * @author Divinity, Neon
 */
class Invis : public AdminCommand {
public:
	Invis();

	void execute(Player& player, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::admincommands
