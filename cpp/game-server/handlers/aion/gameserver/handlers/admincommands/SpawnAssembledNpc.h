#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //spawnAssembledNpc: shows an assembled npc (a dredgion's route) to every player of the world.
 *
 * @author xTz
 */
class SpawnAssembledNpc : public AdminCommand {
public:
	SpawnAssembledNpc();

	void execute(Player& admin, std::span<const std::string> params) override;

	void info(Player& player, std::optional<std::string_view> message) override;
};

} // namespace aion::gameserver::handlers::admincommands
