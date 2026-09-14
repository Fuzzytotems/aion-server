#include "aion/gameserver/spawnengine/SpawnEngine.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::spawnengine {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.spawnengine.SpawnEngine");

/** Java: static class StatsCollector implements Consumer<VisibleObject> (a local consumer of printWorldSpawnStats, fieldmap K5) */
class SpawnEngine::StatsCollector {
public:
	int32_t npcCount = 0;
	int32_t gatherableCount = 0;

	/** Java Consumer.accept */
	void accept(model::gameobjects::VisibleObject& object) { AION_UNPORTED(); }

	int32_t getNpcCount() const { return npcCount; }

	int32_t getGatherableCount() const { return gatherableCount; }
};

runtime::Ptr<model::gameobjects::VisibleObject> SpawnEngine::spawnObject(model::templates::spawns::SpawnTemplate& spawn, int32_t instanceIndex) {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::VisibleObject> SpawnEngine::getSpawnedObject(model::templates::spawns::SpawnTemplate& spawn, int32_t instanceIndex) {
	AION_UNPORTED();
}

runtime::Ref<model::templates::spawns::SpawnTemplate> SpawnEngine::newSingleTimeSpawn(int32_t worldId, int32_t npcId, float x, float y, float z,
	int8_t heading) {
	AION_UNPORTED();
}

runtime::Ref<model::templates::spawns::SpawnTemplate> SpawnEngine::newSingleTimeSpawn(int32_t worldId, int32_t npcId, float x, float y, float z,
	int8_t heading, runtime::Ptr<model::gameobjects::VisibleObject> creator, std::optional<std::string_view> aiName) {
	AION_UNPORTED();
}

runtime::Ref<model::templates::spawns::SpawnTemplate> SpawnEngine::newSingleTimeSpawn(int32_t worldId, int32_t npcId, float x, float y, float z,
	int8_t heading, int32_t creatorId) {
	AION_UNPORTED();
}

runtime::Ref<model::templates::spawns::SpawnTemplate> SpawnEngine::newSpawn(int32_t worldId, int32_t npcId, float x, float y, float z, int8_t heading,
	int32_t respawnTime) {
	AION_UNPORTED();
}

runtime::Ref<model::templates::spawns::SpawnTemplate> SpawnEngine::newSpawn(int32_t worldId, int32_t npcId, float x, float y, float z, int8_t heading,
	int32_t respawnTime, int32_t creatorId, std::optional<std::string_view> aiName, const model::templates::event::EventTemplate* eventTemplate) {
	AION_UNPORTED();
}

runtime::Ref<model::templates::spawns::siegespawns::SiegeSpawnTemplate> SpawnEngine::newSiegeSpawn(int32_t worldId, int32_t npcId, int32_t siegeId,
	model::siege::SiegeRace race, model::siege::SiegeModType mod, float x, float y, float z, int8_t heading) {
	AION_UNPORTED();
}

void SpawnEngine::bringIntoWorld(model::gameobjects::VisibleObject& visibleObject, model::templates::spawns::SpawnTemplate& spawn,
	int32_t instanceIndex) {
	AION_UNPORTED();
}

void SpawnEngine::bringIntoWorld(model::gameobjects::VisibleObject& visibleObject, int32_t worldId, int32_t instanceIndex, float x, float y, float z,
	int8_t h) {
	AION_UNPORTED();
}

void SpawnEngine::bringIntoWorld(model::gameobjects::VisibleObject& visibleObject) {
	AION_UNPORTED();
}

void SpawnEngine::spawnAll() {
	AION_UNPORTED();
}

void SpawnEngine::spawnInstance(world::WorldMapInstance& instance, int8_t difficultId, int32_t ownerId) {
	AION_UNPORTED();
}

void SpawnEngine::spawnEventSpawns(world::WorldMapInstance& instance, int8_t difficultId, int32_t ownerId,
	const model::templates::event::EventTemplate* eventTemplate) {
	AION_UNPORTED();
}

void SpawnEngine::spawnInstance(world::WorldMapInstance& instance, int8_t difficultId, int32_t ownerId,
	const model::templates::event::EventTemplate* eventTemplate) {
	AION_UNPORTED();
}

bool SpawnEngine::checkPool(model::templates::spawns::SpawnGroup& spawn) {
	AION_UNPORTED();
}

void SpawnEngine::printWorldSpawnStats() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::spawnengine
