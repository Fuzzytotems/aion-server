#include "aion/gameserver/services/DebugService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.DebugService");

// callback at DebugService.java:26 (fieldmap key DebugService@L26:55)
DebugService::DebugService() {
	AION_UNPORTED();
}

DebugService::~DebugService() = default;

DebugService& DebugService::getInstance() {
	static DebugService instance; // Java SingletonHolder
	return instance;
}

void DebugService::analyzeWorldPlayers() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
