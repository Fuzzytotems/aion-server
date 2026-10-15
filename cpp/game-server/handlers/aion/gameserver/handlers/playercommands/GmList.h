#pragma once

#include "aion/gameserver/handlers/playercommands/PlayerCommandsPrelude.h"

namespace aion::gameserver::handlers::playercommands {

/**
 * .gmlist: lists all available team members.
 */
class GmList : public PlayerCommand {
public:
	GmList();

	void execute(Player& player, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::playercommands
