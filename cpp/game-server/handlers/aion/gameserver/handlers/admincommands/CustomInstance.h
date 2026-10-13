#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //cinstance: the custom instance's cooldown and rank of the targeted player (D12, owner 2026-10-13).
 */
class CustomInstance : public AdminCommand {
public:
	CustomInstance();

	void execute(Player& player, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::admincommands
