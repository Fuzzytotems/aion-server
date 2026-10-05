#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //enemy: modifies your enmity towards others.
 */
class Enemy : public AdminCommand {
public:
	Enemy();

	void execute(Player& player, std::span<const std::string> params) override;

private:
	static bool equalsIgnoreCase(std::string_view a, std::string_view b);
};

} // namespace aion::gameserver::handlers::admincommands
