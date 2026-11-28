#include "aion/gameserver/services/WorldRaidService.h"
#include "aion/gameserver/configs/schedule/WorldRaidSchedules.h"
#include "aion/gameserver/services/worldraid/WorldRaidRunnable.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/commons/utils/TimeUtils.h"

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
	// C++: Java assigns the data holder's map or Collections.emptyMap(); the port publishes a copy
	runtime::Ref<runtime::RcHashMap<int32_t, const model::templates::worldraid::WorldRaidLocation*>> locations =
		runtime::RcHashMap<int32_t, const model::templates::worldraid::WorldRaidLocation*>::create(AION_LOCK_CLASS(WorldRaidService::raidLocationsById));
	if (configs::main::EventsConfig::ENABLE_WORLDRAID.load()) {
		for (const auto& [id, location] : dataholders::DataManager::WORLD_RAID_DATA->getLocations())
			locations->put(id, location);
	}
	raidLocationsById.set(locations);
}

// Java WorldRaidService.java:42-51
void WorldRaidService::initWorldRaids() {
	if (!configs::main::EventsConfig::ENABLE_WORLDRAID.load())
		return;
	// Initialize Raid Schedules (C++: the loaded schedules are kept for the life of the server, as the runnables that hold them in Java)
	const configs::schedule::WorldRaidSchedules* schedules = configs::schedule::WorldRaidSchedules::load().release();
	for (const configs::schedule::WorldRaidSchedules_WorldRaidSchedule& worldRaidSchedule : schedules->getWorldRaidSchedules()) {
		for (const std::string& cronExpr : worldRaidSchedule.getRaidTimes()) {
			cron::CronService::getInstance().schedule(cron::CronJob(worldraid::WorldRaidRunnable{.worldRaidSchedule = &worldRaidSchedule}), cronExpr);
		}
	}
}

std::vector<const model::templates::worldraid::WorldRaidLocation*> WorldRaidService::getActiveWorldRaidLocations() {
	std::vector<const model::templates::worldraid::WorldRaidLocation*> locations;
	for (const runtime::Ptr<worldraid::WorldRaid>& worldRaid : activeRaids.values().toVector())
		locations.push_back(raidLocationsById.get()->getOrDefault(worldRaid->getLocationId(), nullptr)); // Java: get (null if absent)
	return locations;
}

bool WorldRaidService::isValidWorldRaidLocation(int32_t locationId) {
	return raidLocationsById.get()->containsKey(locationId);
}

bool WorldRaidService::isWorldRaidInProgress(int32_t locationId) {
	return activeRaids.containsKey(locationId);
}

// Java WorldRaidService.java:65-93
void WorldRaidService::startRaid(int32_t locationId, bool useSpecialSpawnMsg) {
	runtime::Ref<worldraid::WorldRaid> worldRaid;
	SYNCHRONIZED(*this) {
		if (!isValidWorldRaidLocation(locationId)) {
			log.error("Attempt to start world raid for an invalid location: {}", locationId);
			return;
		}
		if (isWorldRaidInProgress(locationId)) {
			log.error("Attempt to start world raid twice for location: {}", locationId);
			return;
		}
		bool sendMessages = true;
		const model::templates::worldraid::WorldRaidLocation* location = raidLocationsById.get()->get(locationId);
		const int64_t currentMillis = commons::utils::currentTimeMillis();
		if (lastMsgDateByMapId.containsKey(location->getMapId())) {
			const int64_t lastMsgDateMs = *lastMsgDateByMapId.get(location->getMapId());
			// 30 seconds until another message is allowed to be displayed
			if (lastMsgDateMs + 30000 < currentMillis)
				lastMsgDateByMapId.put(location->getMapId(), currentMillis);
			else
				sendMessages = false;
		} else {
			lastMsgDateByMapId.put(location->getMapId(), currentMillis);
		}
		worldRaid = worldraid::WorldRaid::create(location, useSpecialSpawnMsg, sendMessages);
		activeRaids.put(locationId, worldRaid);
	}
	worldRaid->startWorldRaid();
}

// Java WorldRaidService.java:95-104
void WorldRaidService::stopRaid(int32_t locationId) {
	runtime::Ref<worldraid::WorldRaid> raid;
	SYNCHRONIZED(*this) {
		raid = runtime::Ref<worldraid::WorldRaid>(activeRaids.remove(locationId));
	}
	if (!raid || raid->isFinished()) {
		return;
	}
	raid->stopWorldRaid();
}

WorldRaidService& WorldRaidService::getInstance() {
	static WorldRaidService instance; // Java SingletonHolder
	return instance;
}

} // namespace aion::gameserver::services
