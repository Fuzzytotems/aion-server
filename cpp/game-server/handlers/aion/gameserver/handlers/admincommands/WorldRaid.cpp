#include "aion/gameserver/handlers/admincommands/WorldRaid.h"

#include <algorithm>
#include <string>
#include <vector>

#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/configs/main/EventsConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/WorldRaidData.h"
#include "aion/gameserver/model/templates/worldraid/WorldRaidLocation.h"
#include "aion/gameserver/services/WorldRaidService.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/JavaColor.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(WorldRaid);

WorldRaid::WorldRaid()
	: AdminCommand("worldraid", "Starts/stops the Beritra Invasion event.", R"(list - Shows all available world raid locations.
active - Shows all active world raid locations.
start <location ID> - Starts the world raid for the given location.
stop <location ID> - Stops the world raid for the given location.
)") {
}

// Java WorldRaid.java:31-73
void WorldRaid::execute(Player& player, std::span<const std::string> params) {
	namespace StringUtils = commons::utils::StringUtils;
	if (!EventsConfig::ENABLE_WORLDRAID.load()) {
		sendInfo(player, "World raid currently is disabled.");
		return;
	}

	if (params.empty()) {
		sendInfo(player);
		return;
	}

	if (StringUtils::equalsIgnoreCase("list", params[0])) {
		// Java: WORLD_RAID_DATA.getLocations().values(), a HashMap's values; C++: the data's order (docs/deviations/C1.md)
		std::vector<const WorldRaidLocation*> locations;
		for (const auto& [id, location] : dataholders::DataManager::WORLD_RAID_DATA->getLocations())
			locations.push_back(location);
		sendInfo(player, createLocationList(locations, "World raid locations:"));
	} else if (StringUtils::equalsIgnoreCase("active", params[0])) {
		sendInfo(player, createLocationList(WorldRaidService::getInstance().getActiveWorldRaidLocations(), "Currently active world raids:"));
	} else {
		if (params.size() < 2) {
			sendInfo(player);
			return;
		}

		const int32_t locationId = commons::utils::parseInt(params[1]);
		if (!WorldRaidService::getInstance().isValidWorldRaidLocation(locationId)) {
			sendInfo(player, "Invalid world raid location: " + std::to_string(locationId));
			return;
		}

		if (StringUtils::equalsIgnoreCase("start", params[0])) {
			if (WorldRaidService::getInstance().isWorldRaidInProgress(locationId)) {
				sendInfo(player, "World raid for location " + std::to_string(locationId) + " is already in progress");
				return;
			}
			sendInfo(player, "Starting world raid for location " + std::to_string(locationId));
			WorldRaidService::getInstance().startRaid(locationId, false);
		} else if (StringUtils::equalsIgnoreCase("stop", params[0])) {
			if (!WorldRaidService::getInstance().isWorldRaidInProgress(locationId)) {
				sendInfo(player, "World raid for location " + std::to_string(locationId) + " is not started.");
				return;
			}
			sendInfo(player, "Stopped world raid for location " + std::to_string(locationId));
			WorldRaidService::getInstance().stopRaid(locationId);
		} else {
			sendInfo(player);
		}
	}
}

// Java WorldRaid.java:75-91
std::string WorldRaid::createLocationList(const std::vector<const WorldRaidLocation*>& locations, std::string_view header) {
	std::string sb;
	if (!header.empty())
		sb += header;
	if (locations.empty()) {
		sb += "\n\tNo locations available!";
		return sb;
	}
	// Java: groupingBy(getMapId, LinkedHashMap::new, toList()) - the maps in the order they first appear
	std::vector<std::pair<int32_t, std::vector<const WorldRaidLocation*>>> byMap;
	for (const WorldRaidLocation* location : locations) {
		auto it = std::find_if(byMap.begin(), byMap.end(), [location](const auto& entry) { return entry.first == location->getMapId(); });
		if (it == byMap.end())
			byMap.emplace_back(location->getMapId(), std::vector<const WorldRaidLocation*>{location});
		else
			it->second.push_back(location);
	}
	for (const auto& [mapId, locationsForMap] : byMap) {
		sb += "\n\t" + ChatUtil::color(worldName(mapId), utils::JavaColor::WHITE) + " - ";
		for (size_t i = 0; i < locationsForMap.size(); i++)
			sb += (i == 0 ? "" : ", ") + createPositionString(*locationsForMap[i]);
	}
	return sb;
}

// Java WorldRaid.java:93-95
std::string WorldRaid::createPositionString(const WorldRaidLocation& location) {
	return ChatUtil::position(std::to_string(location.getLocationId()), location.getMapId(), location.getX(), location.getY(), location.getZ());
}

} // namespace aion::gameserver::handlers::admincommands
