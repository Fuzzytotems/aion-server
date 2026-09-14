#include "aion/gameserver/services/WorldRaidService.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/templates/worldraid/WorldRaidLocation.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/worldraid/WorldRaid.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.WorldRaidService");

WorldRaidService::WorldRaidService() = default;

void WorldRaidService::initWorldRaidLocations() {
	AION_UNPORTED();
}

void WorldRaidService::initWorldRaids() {
	AION_UNPORTED();
}

std::vector<const model::templates::worldraid::WorldRaidLocation*> WorldRaidService::getActiveWorldRaidLocations() {
	AION_UNPORTED();
}

bool WorldRaidService::isValidWorldRaidLocation(int32_t locationId) {
	AION_UNPORTED();
}

bool WorldRaidService::isWorldRaidInProgress(int32_t locationId) {
	AION_UNPORTED();
}

void WorldRaidService::startRaid(int32_t locationId, bool useSpecialSpawnMsg) { // lint: L7 unported stub; the port adds the Java synchronized block
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
