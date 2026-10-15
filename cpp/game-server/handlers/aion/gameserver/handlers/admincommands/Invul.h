#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //invul: enables/disables invulnerability.
 *
 * @author Andy, Divinity
 */
class Invul : public AdminCommand {
public:
	Invul();

	void execute(Player& player, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::admincommands
