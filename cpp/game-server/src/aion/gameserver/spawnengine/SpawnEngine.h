#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/siege/fwd.h"
#include "aion/gameserver/model/templates/event/fwd.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"
#include "aion/gameserver/model/templates/spawns/siegespawns/fwd.h"
#include "aion/gameserver/spawnengine/fwd.h"
#include "aion/gameserver/world/fwd.h"

namespace aion::gameserver::spawnengine {

/**
 * This class is responsible for NPCs spawn management. Current implementation is temporal and will be replaced in the future.
 * <p>
 * Hub header (docs/design/hub-headers.md). A static-only class (fieldmap K5). The spawn template factories return `Ref<SpawnTemplate>`: a
 * runtime SpawnTemplate is an OwnedPart of its new SpawnGroup, so the Ref retains the group (runtime-architecture.md §9). spawnAll's top loop
 * opens a QuiescentScope per map (§2.6).
 *
 * @author Luno, ATracer, Source, Wakizashi, xTz, nrg
 */
class SpawnEngine {
public:
	/** Java: static class StatsCollector implements Consumer<VisibleObject> (used only by printWorldSpawnStats, defined in SpawnEngine.cpp) */
	class StatsCollector;

	/** Creates VisibleObject instance and spawns it using given {@link SpawnTemplate} instance. @return the object or null */
	static runtime::Ptr<model::gameobjects::VisibleObject> spawnObject(model::templates::spawns::SpawnTemplate& spawn, int32_t instanceIndex);

private:
	static runtime::Ptr<model::gameobjects::VisibleObject> getSpawnedObject(model::templates::spawns::SpawnTemplate& spawn, int32_t instanceIndex);

public:
	static runtime::Ref<model::templates::spawns::SpawnTemplate> newSingleTimeSpawn(int32_t worldId, int32_t npcId, float x, float y, float z,
		int8_t heading);

	/**
	 * @param creator
	 *          may be null (no creator id and event template)
	 * @param aiName
	 *          absent (Java null): the template's AI
	 */
	static runtime::Ref<model::templates::spawns::SpawnTemplate> newSingleTimeSpawn(int32_t worldId, int32_t npcId, float x, float y, float z,
		int8_t heading, runtime::Ptr<model::gameobjects::VisibleObject> creator, std::optional<std::string_view> aiName);

	static runtime::Ref<model::templates::spawns::SpawnTemplate> newSingleTimeSpawn(int32_t worldId, int32_t npcId, float x, float y, float z,
		int8_t heading, int32_t creatorId);

	static runtime::Ref<model::templates::spawns::SpawnTemplate> newSpawn(int32_t worldId, int32_t npcId, float x, float y, float z, int8_t heading,
		int32_t respawnTime);

private:
	static runtime::Ref<model::templates::spawns::SpawnTemplate> newSpawn(int32_t worldId, int32_t npcId, float x, float y, float z, int8_t heading,
		int32_t respawnTime, int32_t creatorId, std::optional<std::string_view> aiName, const model::templates::event::EventTemplate* eventTemplate);

public:
	static runtime::Ref<model::templates::spawns::siegespawns::SiegeSpawnTemplate> newSiegeSpawn(int32_t worldId, int32_t npcId, int32_t siegeId,
		model::siege::SiegeRace race, model::siege::SiegeModType mod, float x, float y, float z, int8_t heading);

	/** Java package-private */
	static void bringIntoWorld(model::gameobjects::VisibleObject& visibleObject, model::templates::spawns::SpawnTemplate& spawn, int32_t instanceIndex);

	static void bringIntoWorld(model::gameobjects::VisibleObject& visibleObject, int32_t worldId, int32_t instanceIndex, float x, float y, float z,
		int8_t h);

	/** @throws IllegalArgumentException if the position is null */
	static void bringIntoWorld(model::gameobjects::VisibleObject& visibleObject);

	/** Spawn all NPC's from templates */
	static void spawnAll();

	static void spawnInstance(world::WorldMapInstance& instance, int8_t difficultId, int32_t ownerId);

	static void spawnEventSpawns(world::WorldMapInstance& instance, int8_t difficultId, int32_t ownerId,
		const model::templates::event::EventTemplate* eventTemplate);

private:
	static void spawnInstance(world::WorldMapInstance& instance, int8_t difficultId, int32_t ownerId,
		const model::templates::event::EventTemplate* eventTemplate);

public:
	static bool checkPool(model::templates::spawns::SpawnGroup& spawn);

	static void printWorldSpawnStats();
};

} // namespace aion::gameserver::spawnengine
