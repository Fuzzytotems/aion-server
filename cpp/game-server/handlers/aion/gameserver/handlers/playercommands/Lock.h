#pragma once

#include "aion/gameserver/handlers/playercommands/PlayerCommandsPrelude.h"

namespace aion::gameserver::handlers::playercommands {

/**
 * .lock: enables or disables blocking the account's logins from other computers (the allowed HDD serial).
 *
 * @author ViAl, Neon
 */
class Lock : public PlayerCommand {
public:
	Lock();

	void execute(Player& player, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::playercommands
