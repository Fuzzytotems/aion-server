#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //state: views and adjusts your target's creature states.
 */
class State : public AdminCommand {
public:
	State();

	void execute(Player& player, std::span<const std::string> params) override;

private:
	std::string getStateDescription(int32_t state);
	std::string findStateName(int32_t creatureStateId, std::string_view defaultName);
};

} // namespace aion::gameserver::handlers::admincommands
