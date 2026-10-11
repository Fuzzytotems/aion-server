#pragma once

#include <string>

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //moveto: moves the GM to coordinates (of the current or a named map), a position link, a player, an npc's spawn spot, or forward.
 *
 * @author Neon
 */
class MoveTo : public AdminCommand {
public:
	MoveTo();

	void execute(Player& admin, std::span<const std::string> params) override;

private:
	void moveForward(Player& admin, float distance);

	runtime::Ref<world::WorldPosition> parseWorldPosition(Player& admin, std::span<const std::string> params);

	void moveTo(Player& admin, runtime::Ref<world::WorldPosition> pos, const std::string& message);

	int32_t getNpcId(const std::string& nameOrId);
};

} // namespace aion::gameserver::handlers::admincommands
