#include "aion/gameserver/services/RoadService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.RoadService");

RoadService& RoadService::getInstance() {
	static RoadService instance; // Java SingletonHolder
	return instance;
}

RoadService::RoadService() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
