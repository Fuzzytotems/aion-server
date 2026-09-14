#include "aion/gameserver/services/FlyRingService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::services {

// Java instance field `Logger log` (hub-headers.md §11.3)
static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.FlyRingService");

FlyRingService::FlyRingService() {
	AION_UNPORTED();
}

FlyRingService::~FlyRingService() = default;

FlyRingService& FlyRingService::getInstance() {
	static FlyRingService instance; // Java SingletonHolder
	return instance;
}

} // namespace aion::gameserver::services
