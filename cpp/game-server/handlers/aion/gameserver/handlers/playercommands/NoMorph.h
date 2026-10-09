#pragma once

#include "aion/gameserver/handlers/playercommands/PlayerCommandsPrelude.h"

namespace aion::gameserver::handlers::playercommands {

/**
 * .nomorph: enables or disables your current transformation appearance (Java class Nomorph in NoMorph.java).
 */
class Nomorph : public PlayerCommand {
public:
	Nomorph();

	void execute(Player& player, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::playercommands
