#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //energy: shows, adds or resets the target's energy of repose and salvation, or refreshes its stats.
 *
 * @author Source
 */
class EnergyBuff : public AdminCommand {
public:
	EnergyBuff();

	void execute(Player& admin, std::span<const std::string> params) override;

	void info(Player& player, std::optional<std::string_view> message) override;
};

} // namespace aion::gameserver::handlers::admincommands
