#pragma once

#include <cstdint>
#include <functional>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/instance/handlers/fwd.h"
#include "aion/gameserver/world/fwd.h"

namespace aion::gameserver::world {

/**
 * Creates world map instances (3D regions for Reshanta, 2D regions otherwise) and adds them to their map.
 * <p>
 * C++: a static-only class. The instance handler supplier is Java's Function<WorldMapInstance, InstanceHandler>.
 *
 * @author ATracer
 */
class WorldMapInstanceFactory {
public:
	WorldMapInstanceFactory() = delete;

	/** Creates an instance whose handler comes from InstanceEngine::getNewInstanceHandler */
	static runtime::Ref<WorldMapInstance> createWorldMapInstance(WorldMap& parent, int32_t maxPlayers);

	static runtime::Ref<WorldMapInstance> createWorldMapInstance(WorldMap& parent, int32_t ownerId,
		const std::function<runtime::Ref<instance::handlers::InstanceHandler>(WorldMapInstance&)>& instanceHandlerSupplier, int32_t maxPlayers);
};

} // namespace aion::gameserver::world
