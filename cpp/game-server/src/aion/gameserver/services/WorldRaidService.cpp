#include "aion/gameserver/services/WorldRaidService.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/main/EventsConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/WorldRaidData.h"
#include "aion/gameserver/model/templates/worldraid/WorldRaidLocation.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/worldraid/WorldRaid.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.WorldRaidService");

WorldRaidService::WorldRaidService() = default;

void WorldRaidService::initWorldRaidLocations() {
	log.debug("Initializing world raid locations...");
	// C++: Java assigns the data holder's map or Collections.emptyMap(); the port publishes a copy
	runtime::Ref<runtime::RcHashMap<int32_t, const model::templates::worldraid::WorldRaidLocation*>> locations =
		runtime::RcHashMap<int32_t, const model::templates::worldraid::WorldRaidLocation*>::create(AION_LOCK_CLASS(WorldRaidService::raidLocationsById));
	if (configs::main::EventsConfig::ENABLE_WORLDRAID.load()) {
		for (const auto& [id, location] : dataholders::DataManager::WORLD_RAID_DATA->getLocations())
			locations->put(id, location);
	}
	raidLocationsById.set(locations);
	log.debug("Finished initialization of world raid locations with size " + std::to_string(locations->size()));
}

void WorldRaidService::initWorldRaids() {
	log.debug("Initializing world raid schedules...");
	if (!configs::main::EventsConfig::ENABLE_WORLDRAID.load())
		return;
	// M5a keeps world raids disabled (plan D1): WorldRaidSchedules.load() and the WorldRaidRunnable cron jobs follow with the world raid work
	AION_UNPORTED();
}

std::vector<const model::templates::worldraid::WorldRaidLocation*> WorldRaidService::getActiveWorldRaidLocations() {
	std::vector<const model::templates::worldraid::WorldRaidLocation*> locations;
	for (const runtime::Ptr<worldraid::WorldRaid>& worldRaid : activeRaids.values().toVector())
		locations.push_back(raidLocationsById.get()->getOrDefault(worldRaid->getLocationId(), nullptr)); // Java: get (null if absent)
	return locations;
}

bool WorldRaidService::isValidWorldRaidLocation(int32_t locationId) {
	if (raidLocationsById.get()->containsKey(locationId))
		return true;
	log.debug("No world raid location found for id: " + std::to_string(locationId));
	return false;
}

bool WorldRaidService::isWorldRaidInProgress(int32_t locationId) {
	return activeRaids.containsKey(locationId);
}

void WorldRaidService::startRaid(int32_t locationId, bool useSpecialSpawnMsg) {
	AION_UNPORTED();
}

void WorldRaidService::stopRaid(int32_t locationId) {
	AION_UNPORTED();
}

WorldRaidService& WorldRaidService::getInstance() {
	static WorldRaidService instance; // Java SingletonHolder
	return instance;
}

} // namespace aion::gameserver::services
