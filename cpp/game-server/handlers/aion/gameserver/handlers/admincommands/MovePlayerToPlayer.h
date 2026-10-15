#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //moveplayertoplayer: teleports a player to another player.
 */
class MovePlayerToPlayer : public AdminCommand {
public:
	MovePlayerToPlayer();

	void execute(Player& player, std::span<const std::string> params) override;

	void info(Player& player, std::optional<std::string_view> message) override;
};

} // namespace aion::gameserver::handlers::admincommands
