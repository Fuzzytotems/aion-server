#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //zone: shows zone information.
 */
class Zone : public AdminCommand {
public:
	Zone();

	void execute(Player& player, std::span<const std::string> params) override;

private:
	std::vector<runtime::Ptr<ZoneInstance>> findZones(Creature& creature, std::optional<std::string_view> zoneNameFilter);
};

} // namespace aion::gameserver::handlers::admincommands
