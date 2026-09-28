#include "aion/gameserver/instance/InstanceEngine.h"

#include <exception>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::instance {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.instance.InstanceEngine");

InstanceEngine::InstanceEngine() = default;

InstanceEngine::~InstanceEngine() = default;

void InstanceEngine::init() {
	// Java: a ScriptManager with an InstanceHandlerClassListener loads InstanceConfig.HANDLER_DIRECTORY and calls addInstanceHandlerClass per
	// @InstanceID class; C++: the handlers are compiled in and registered by AION_INSTANCE_HANDLER markers (HandlerRegistry.h), one entry per map
	// id (duplicates are a build error), so the Java map size is the number of registry entries.
	log.info("Loaded " + std::to_string(gameserver::handlers::instanceHandlerEntries().size()) + " instance handlers.");
}

runtime::Ref<handlers::InstanceHandler> InstanceEngine::getNewInstanceHandler(world::WorldMapInstance& instance) {
	const gameserver::handlers::InstanceHandlerEntry* handlerClass = nullptr; // Java: instanceHandlers.get(instance.getMapId())
	for (const gameserver::handlers::InstanceHandlerEntry& entry : gameserver::handlers::instanceHandlerEntries()) {
		if (entry.mapId == instance.getMapId()) {
			handlerClass = &entry;
			break;
		}
	}
	runtime::Ref<handlers::InstanceHandler> instanceHandler;
	if (handlerClass != nullptr) {
		try {
			instanceHandler = handlerClass->create(instance);
		} catch (const std::exception& ex) {
			log.warn("Can't instantiate instance handler for map " + std::to_string(instance.getMapId()) + " (instanceId: " +
				std::to_string(instance.getInstanceId()) + ')', ex);
		}
	}

	return instanceHandler ? instanceHandler : runtime::Ref<handlers::InstanceHandler>(handlers::GeneralInstanceHandler::create(instance));
}

void InstanceEngine::addInstanceHandlerClass(const gameserver::handlers::InstanceHandlerEntry& handler) {
	AION_UNPORTED();
}

InstanceEngine& InstanceEngine::getInstance() {
	static InstanceEngine instance; // Java: SingletonHolder
	return instance;
}

} // namespace aion::gameserver::instance
