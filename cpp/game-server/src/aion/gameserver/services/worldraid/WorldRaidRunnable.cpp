#include "aion/gameserver/services/worldraid/WorldRaidRunnable.h"

#include <string>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/WorldRaidService.h"

namespace aion::gameserver::services::worldraid {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.worldraid.WorldRaidRunnable");

/** Java List<Integer>.toString() */
static std::string listToString(const std::vector<int32_t>& values) {
	std::string text = "[";
	for (size_t i = 0; i < values.size(); i++)
		text += (i == 0 ? "" : ", ") + std::to_string(values[i]);
	return text + "]";
}

// Java WorldRaidRunnable.java:27-57
void WorldRaidRunnable::operator()() const {
	if (!worldRaidSchedule->getLocations())
		throw runtime::NullPointerException("world raid schedule " + worldRaidSchedule->getId() + " without locations"); // Java: null.stream()
	const std::vector<int32_t>& locations = *worldRaidSchedule->getLocations();
	log.debug("Attempting to start world raid with ID: " + worldRaidSchedule->getId() + " and location pool: " + listToString(locations));
	std::vector<int32_t> validRaidLocations;
	for (int32_t locationId : locations)
		if (WorldRaidService::getInstance().isValidWorldRaidLocation(locationId) && !WorldRaidService::getInstance().isWorldRaidInProgress(locationId))
			validRaidLocations.push_back(locationId);

	if (validRaidLocations.size() != locations.size()) {
		log.warn("Invalid world raid location count for raid with ID: " + worldRaidSchedule->getId()
			+ ". Some locations may be invalid due to a misconfiguration or due to currently running raids!");
		return;
	}

	// determine location count
	int32_t spawnLocationCount = static_cast<int32_t>(locations.size());
	if (worldRaidSchedule->getMinCount() > 0 && spawnLocationCount > worldRaidSchedule->getMinCount())
		spawnLocationCount = worldRaidSchedule->getMinCount();
	if (worldRaidSchedule->getMaxCount() > 0 && worldRaidSchedule->getMaxCount() > spawnLocationCount)
		spawnLocationCount = commons::utils::Rnd::get(spawnLocationCount, worldRaidSchedule->getMaxCount());

	// remove unused locations due to location count restriction
	while (static_cast<int32_t>(validRaidLocations.size()) > spawnLocationCount)
		validRaidLocations.erase(validRaidLocations.begin() + commons::utils::Rnd::nextInt(static_cast<int32_t>(validRaidLocations.size())));

	// start actual world raids using the remaining locations
	for (int32_t locationId : validRaidLocations)
		WorldRaidService::getInstance().startRaid(locationId, worldRaidSchedule->isSpecialRaid());

	if (!validRaidLocations.empty())
		log.debug("Started scheduled world raid with ID " + worldRaidSchedule->getId() + " at the following raid locations: " +
			listToString(validRaidLocations));
}

} // namespace aion::gameserver::services::worldraid
