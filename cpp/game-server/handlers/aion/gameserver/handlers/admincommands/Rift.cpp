#include "aion/gameserver/handlers/admincommands/Rift.h"

#include <set>
#include <string>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/model/rift/RiftLocation.h"
#include "aion/gameserver/services/RiftService.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Rift);

Rift::Rift()
	: AdminCommand("rift", "Opens or closes rifts in the world.", R"(list - Lists all rift locations.
open <location ID|world ID> [g] - Opens the rifts at the given location. If g is specified and spawns are defined, guards will spawn.
close <location ID|world ID> - Closes the rifts at the given location.
open all [g] - Opens the rifts in every zone.
close all - Closes the rifts in every zone.
)") {
}

// Java Rift.java:18-35
void Rift::execute(Player& player, std::span<const std::string> params) {
	namespace StringUtils = commons::utils::StringUtils;
	if (!params.empty() && StringUtils::equalsIgnoreCase("list", params[0])) {
		sendInfo(player, "Rift locations:");
		// Java: HashMap.forEach; C++: the shim's order (docs/deviations/P5-12b.md)
		for (const auto& [id, loc] : RiftService::getInstance().getRiftLocations()->snapshot())
			sendInfo(player, "ID: " + std::to_string(id) + ", world ID: " + std::to_string(loc->getWorldId()) + (loc->isOpened() ? " (open)" : ""));
	} else if (params.size() > 1 && StringUtils::equalsIgnoreCase("open", params[0]) && StringUtils::equalsIgnoreCase("all", params[1])) {
		// C++-dev only (owner, 2026-10-31): every zone's rifts, as //rift open <world ID> for each world that has rift locations
		const bool guards = params.size() > 2 && StringUtils::equalsIgnoreCase(params[2], "g");
		int zones = 0;
		for (int32_t worldId : riftWorlds())
			zones += RiftService::getInstance().openRifts(worldId, guards) ? 1 : 0;
		sendInfo(player, zones > 0 ? "Opened rifts in " + std::to_string(zones) + " zones." : std::string("Rifts are already open."));
	} else if (params.size() > 1 && StringUtils::equalsIgnoreCase("close", params[0]) && StringUtils::equalsIgnoreCase("all", params[1])) {
		// C++-dev only (owner, 2026-10-31): closes every open rift, the ones that never close on their own too
		int zones = 0;
		for (int32_t worldId : riftWorlds())
			zones += RiftService::getInstance().closeRifts(worldId) ? 1 : 0;
		sendInfo(player, zones > 0 ? "Closed rifts in " + std::to_string(zones) + " zones." : std::string("Rifts were already closed."));
	} else if (params.size() > 1 && StringUtils::equalsIgnoreCase("open", params[0])) {
		const int32_t id = parseId(params[1]);
		const bool guards = params.size() > 2 && StringUtils::equalsIgnoreCase(params[2], "g");
		const bool result = RiftService::getInstance().openRifts(id, guards);
		sendInfo(player, result ? "Opened rifts at location " + std::to_string(id) + "." : std::string("Rifts are already open."));
	} else if (params.size() > 1 && StringUtils::equalsIgnoreCase("close", params[0])) {
		const int32_t id = parseId(params[1]);
		const bool result = RiftService::getInstance().closeRifts(parseId(params[1]));
		sendInfo(player, result ? "Closed rifts at location " + std::to_string(id) + "." : std::string("Rifts were already closed."));
	} else {
		sendInfo(player);
	}
}

std::set<int32_t> Rift::riftWorlds() {
	std::set<int32_t> worlds;
	for (const auto& [id, loc] : RiftService::getInstance().getRiftLocations()->snapshot())
		worlds.insert(loc->getWorldId());
	return worlds;
}

// Java Rift.java:37-42
int32_t Rift::parseId(const std::string& idParam) {
	const int32_t id = commons::utils::parseInt(idParam);
	if (!RiftService::getInstance().isValidId(id))
		throw commons::utils::IllegalArgumentException("Invalid rift world ID or location ID.");
	return id;
}

} // namespace aion::gameserver::handlers::admincommands
