#pragma once

#include <cstdint>

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"
#include "aion/gameserver/spawnengine/fwd.h"

namespace aion::gameserver::spawnengine {

/**
 * Spawns the static objects (spawn groups with the STATIC handler).
 * <p>
 * C++: a static-only class.
 *
 * @author ATracer
 */
class StaticObjectSpawnManager {
public:
	StaticObjectSpawnManager() = delete;

	static void spawnTemplate(model::templates::spawns::SpawnGroup& spawn, int32_t instanceIndex);

private:
	static void bringIntoWorld(model::gameobjects::VisibleObject& visibleObject, model::templates::spawns::SpawnTemplate& spawn, int32_t instanceIndex);
};

} // namespace aion::gameserver::spawnengine
