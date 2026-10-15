#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //worldraid: starts/stops the Beritra Invasion event.
 * <p>
 * Java: data/handlers/admincommands/WorldRaid.java. M5i I-5.
 */
class WorldRaid : public AdminCommand {
public:
	WorldRaid();

	void execute(Player& player, std::span<const std::string> params) override;

private:
	std::string createLocationList(const std::vector<const WorldRaidLocation*>& locations, std::string_view header);

	std::string createPositionString(const WorldRaidLocation& location);
};

} // namespace aion::gameserver::handlers::admincommands
