#include "aion/gameserver/world/WorldMapInstanceFactory.h"

#include "aion/gameserver/instance/InstanceEngine.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMap2DInstance.h"
#include "aion/gameserver/world/WorldMap3DInstance.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldMapType.h"
#include "aion/gameserver/world/WorldMapTypeInfo.h"

namespace aion::gameserver::world {

runtime::Ref<WorldMapInstance> WorldMapInstanceFactory::createWorldMapInstance(WorldMap& parent, int32_t maxPlayers) {
	// Java: InstanceEngine.getInstance()::getNewInstanceHandler
	return createWorldMapInstance(parent, 0,
		[](WorldMapInstance& mapInstance) { return gameserver::instance::InstanceEngine::getInstance().getNewInstanceHandler(mapInstance); }, maxPlayers);
}

runtime::Ref<WorldMapInstance> WorldMapInstanceFactory::createWorldMapInstance(WorldMap& parent, int32_t ownerId,
	const std::function<runtime::Ref<instance::handlers::InstanceHandler>(WorldMapInstance&)>& instanceHandlerSupplier, int32_t maxPlayers) {
	runtime::Ref<WorldMapInstance> instance;
	if (parent.getMapId() == getId(WorldMapType::RESHANTA)) {
		instance = WorldMap3DInstance::create(parent, parent.getNextInstanceId(), maxPlayers, instanceHandlerSupplier);
	} else {
		instance = WorldMap2DInstance::create(parent, parent.getNextInstanceId(), ownerId, maxPlayers, instanceHandlerSupplier);
	}
	parent.addInstance(instance->getInstanceId(), *instance);
	return instance;
}

} // namespace aion::gameserver::world
