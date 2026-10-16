#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //gag: bans a player from all chats.
 *
 * @author Watson, Neon
 */
class Gag : public AdminCommand {
public:
	Gag();

	void execute(Player& admin, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::admincommands
