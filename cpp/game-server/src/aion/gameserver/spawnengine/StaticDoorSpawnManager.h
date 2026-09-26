#pragma once

#include "aion/gameserver/spawnengine/fwd.h"
#include "aion/gameserver/world/fwd.h"

namespace aion::gameserver::spawnengine {

/**
 * Spawns the static doors of a world map instance.
 * <p>
 * C++: a static-only class.
 *
 * @author MrPoke
 */
class StaticDoorSpawnManager {
public:
	StaticDoorSpawnManager() = delete;

	static void spawnTemplate(world::WorldMapInstance& instance);
};

} // namespace aion::gameserver::spawnengine
