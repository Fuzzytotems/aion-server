#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/collections/HashSet.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/templates/event/fwd.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"
#include "aion/gameserver/spawnengine/fwd.h"
#include "aion/gameserver/world/fwd.h"

namespace aion::gameserver::spawnengine {

/**
 * Spawns and despawns the temporary spawns (spawn groups with spawn times) when the game hour changes.
 * <p>
 * C++: a static-only class. Java's `static synchronized` methods lock one Monitor of the class (defined in TemporarySpawnEngine.cpp).
 *
 * @author xTz, Neon
 */
class TemporarySpawnEngine {
private:
	// defined in the .cpp (the Ref element types are incomplete here)
	static runtime::HashMap<runtime::Ref<model::templates::spawns::SpawnGroup>, runtime::Ref<runtime::RcHashSet<int32_t>>> spawnGroups;
	static runtime::HashSet<runtime::Ref<model::gameobjects::VisibleObject>> spawnedObjects;

public:
	TemporarySpawnEngine() = delete;

	static void onHourChange(); // synchronized

private:
	static void despawn();

	static void spawn();

public:
	static void registerSpawned(model::gameobjects::VisibleObject& object); // synchronized

	static void unregisterSpawned(int32_t objectId); // synchronized

	static void addSpawnGroup(model::templates::spawns::SpawnGroup& spawnGroup, int32_t instanceId); // synchronized

	static void unregister(const model::templates::event::EventTemplate* eventTemplate); // synchronized

	static void onInstanceDestroy(world::WorldMapInstance& instance); // synchronized
};

} // namespace aion::gameserver::spawnengine
