#pragma once

#include "aion/gameserver/handlers/playercommands/PlayerCommandsPrelude.h"

namespace aion::gameserver::handlers::playercommands {

/**
 * .faction: the faction chat.
 *
 * @author Shepper, bobobear, Neon
 */
class Faction : public PlayerCommand {
public:
	Faction();

	void execute(Player& player, std::span<const std::string> params) override;

private:
	static std::string formattedSyntaxInfo();
};

} // namespace aion::gameserver::handlers::playercommands
