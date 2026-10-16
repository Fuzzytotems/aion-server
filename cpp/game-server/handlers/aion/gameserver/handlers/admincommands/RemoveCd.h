#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //removecd: clears cooldowns for skills, items and instances.
 */
class RemoveCd : public AdminCommand {
public:
	RemoveCd();

	void execute(Player& player, std::span<const std::string> params) override;

	static void removeItemCooldowns(Player& player);
};

} // namespace aion::gameserver::handlers::admincommands
