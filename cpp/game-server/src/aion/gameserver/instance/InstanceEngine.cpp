#include "aion/gameserver/instance/InstanceEngine.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"

namespace aion::gameserver::instance {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.instance.InstanceEngine");

InstanceEngine::InstanceEngine() = default;

InstanceEngine::~InstanceEngine() = default;

void InstanceEngine::init() {
	AION_UNPORTED();
}

runtime::Ref<handlers::InstanceHandler> InstanceEngine::getNewInstanceHandler(world::WorldMapInstance& instance) {
	AION_UNPORTED();
}

void InstanceEngine::addInstanceHandlerClass(const gameserver::handlers::InstanceHandlerEntry& handler) {
	AION_UNPORTED();
}

InstanceEngine& InstanceEngine::getInstance() {
	static InstanceEngine instance; // Java: SingletonHolder
	return instance;
}

} // namespace aion::gameserver::instance
