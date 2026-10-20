#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //relinquishcraft: removes a player's crafting expert or master status.
 *
 * @author synchro2, Neon
 */
class RelinquishCraft : public AdminCommand {
public:
	RelinquishCraft();

	void execute(Player& admin, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::admincommands
