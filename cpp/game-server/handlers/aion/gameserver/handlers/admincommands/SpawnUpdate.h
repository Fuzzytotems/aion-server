#pragma once

#include <optional>

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //spawnu: updates the spawn data (coordinates, heading, walker id) of the targeted npc or gatherable.
 *
 * @author KID, Rolandas, Neon
 */
class SpawnUpdate : public AdminCommand {
public:
	SpawnUpdate();

	void execute(Player& admin, std::span<const std::string> params) override;

private:
	void updateWalker(Player& admin, Npc& target, std::optional<std::string> walkerId);
};

} // namespace aion::gameserver::handlers::admincommands
