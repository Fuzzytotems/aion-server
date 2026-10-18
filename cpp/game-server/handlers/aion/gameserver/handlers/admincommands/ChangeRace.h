#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //changerace: switches the GM's race to the opposite faction.
 *
 * @author ginho1
 */
class ChangeRace : public AdminCommand {
public:
	ChangeRace();

	void execute(Player& admin, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::admincommands
