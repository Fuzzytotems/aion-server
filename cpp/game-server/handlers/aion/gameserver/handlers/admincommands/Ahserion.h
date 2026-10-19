#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //ahserion: starts and stops Ahserion's Flight.
 *
 * @author Yeats, Neon
 */
class Ahserion : public AdminCommand {
public:
	Ahserion();

	void execute(Player& admin, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::admincommands
