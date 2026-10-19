#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //map: freezes or unfreezes every npc of the GM's map instance.
 *
 * @author Rolandas, Neon
 */
class MapCommand : public AdminCommand {
public:
	MapCommand();

	void execute(Player& admin, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::admincommands
