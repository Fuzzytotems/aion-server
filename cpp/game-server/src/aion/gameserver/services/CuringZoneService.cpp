#include "aion/gameserver/services/CuringZoneService.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/curingzone/CuringObject.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.CuringZoneService");

CuringZoneService::CuringZoneService() {
	AION_UNPORTED();
}

CuringZoneService::~CuringZoneService() = default;

CuringZoneService& CuringZoneService::getInstance() {
	static CuringZoneService instance; // Java SingletonHolder
	return instance;
}

// anonymous Runnable at CuringZoneService.java:38 (fieldmap key CuringZoneService$1); argument 1 of scheduleAtFixedRate(); storage: task
// anonymous Consumer at CuringZoneService.java:43 (fieldmap key CuringZoneService$2); argument 1 of forEachPlayer(); storage: sync
void CuringZoneService::startTask() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
