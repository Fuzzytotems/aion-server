#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //damage: damages your target's HP, MP, FP or DP.
 */
class Damage : public AdminCommand {
public:
	Damage();

	void execute(Player& player, std::span<const std::string> params) override;

	void info(Player& player, std::optional<std::string_view> message) override;
};

} // namespace aion::gameserver::handlers::admincommands
