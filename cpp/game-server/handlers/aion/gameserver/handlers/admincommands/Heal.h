#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //heal: restores HP, MP, DP, flight time and energy of repose.
 */
class Heal : public AdminCommand {
public:
	Heal();

	void execute(Player& player, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::admincommands
