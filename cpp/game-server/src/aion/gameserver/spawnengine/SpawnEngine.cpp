#include "aion/gameserver/spawnengine/SpawnEngine.h"

#include <string>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/gameobjects/Gatherable.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/templates/event/EventTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/TemporarySpawn.h"
#include "aion/gameserver/model/templates/spawns/riftspawns/RiftSpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/siegespawns/SiegeSpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/vortexspawns/VortexSpawnTemplate.h"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/HousingService.h"
#include "aion/gameserver/services/rift/RiftManager.h"
#include "aion/gameserver/spawnengine/SpawnHandlerType.h"
#include "aion/gameserver/spawnengine/StaticDoorSpawnManager.h"
#include "aion/gameserver/spawnengine/StaticObjectSpawnManager.h"
#include "aion/gameserver/spawnengine/TemporarySpawnEngine.h"
#include "aion/gameserver/spawnengine/VisibleObjectSpawner.h"
#include "aion/gameserver/spawnengine/WalkerFormator.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::spawnengine {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.spawnengine.SpawnEngine");

/** Java: static class StatsCollector implements Consumer<VisibleObject> (a local consumer of printWorldSpawnStats, fieldmap K5) */
class SpawnEngine::StatsCollector {
public:
	int32_t npcCount = 0;
	int32_t gatherableCount = 0;

	/** Java Consumer.accept */
	void accept(model::gameobjects::VisibleObject& object) {
		if (dynamic_cast<model::gameobjects::Npc*>(&object) != nullptr) {
			npcCount++;
		} else if (dynamic_cast<model::gameobjects::Gatherable*>(&object) != nullptr) {
			gatherableCount++;
		}
	}

	int32_t getNpcCount() const { return npcCount; }

	int32_t getGatherableCount() const { return gatherableCount; }
};

runtime::Ptr<model::gameobjects::VisibleObject> SpawnEngine::spawnObject(model::templates::spawns::SpawnTemplate& spawn, int32_t instanceIndex) {
	runtime::Ptr<model::gameobjects::VisibleObject> visObj = getSpawnedObject(spawn, instanceIndex);
	if (visObj) {
		if (visObj->getSpawn() && visObj->getSpawn()->isTemporarySpawn())
			TemporarySpawnEngine::registerSpawned(*visObj);
		if (visObj->isSpawned()) // WalkerFormator.processClusteredNpc delays spawn of pooled walkers
			visObj->getPosition()->getWorldMapInstance()->getInstanceHandler()->onSpawn(*visObj);
	}
	return visObj;
}

runtime::Ptr<model::gameobjects::VisibleObject> SpawnEngine::getSpawnedObject(model::templates::spawns::SpawnTemplate& spawn, int32_t instanceIndex) {
	int32_t npcId = spawn.getNpcId();

	// the created object stays referenced by the world (or, for a failed spawn, until the task ends: a borrow is valid for the task)
	if (npcId > 400000 && npcId < 499999) {
		return VisibleObjectSpawner::spawnGatherable(spawn, instanceIndex);
	} else if (auto* riftSpawn = dynamic_cast<model::templates::spawns::riftspawns::RiftSpawnTemplate*>(&spawn)) {
		return VisibleObjectSpawner::spawnRiftNpc(*riftSpawn, instanceIndex);
	} else if (auto* siegeSpawn = dynamic_cast<model::templates::spawns::siegespawns::SiegeSpawnTemplate*>(&spawn)) {
		return VisibleObjectSpawner::spawnSiegeNpc(*siegeSpawn, instanceIndex);
	} else if (auto* vortexSpawn = dynamic_cast<model::templates::spawns::vortexspawns::VortexSpawnTemplate*>(&spawn)) {
		return VisibleObjectSpawner::spawnInvasionNpc(*vortexSpawn, instanceIndex);
	} else {
		return VisibleObjectSpawner::spawnNpc(spawn, instanceIndex);
	}
}

runtime::Ref<model::templates::spawns::SpawnTemplate> SpawnEngine::newSingleTimeSpawn(int32_t worldId, int32_t npcId, float x, float y, float z,
	int8_t heading) {
	return newSpawn(worldId, npcId, x, y, z, heading, 0, 0, std::nullopt, nullptr);
}

runtime::Ref<model::templates::spawns::SpawnTemplate> SpawnEngine::newSingleTimeSpawn(int32_t worldId, int32_t npcId, float x, float y, float z,
	int8_t heading, runtime::Ptr<model::gameobjects::VisibleObject> creator, std::optional<std::string_view> aiName) {
	int32_t creatorId = !creator ? 0 : creator->getObjectId();
	const model::templates::event::EventTemplate* eventTemplate = !creator || !creator->getSpawn() ? nullptr : creator->getSpawn()->getEventTemplate();
	return newSpawn(worldId, npcId, x, y, z, heading, 0, creatorId, aiName, eventTemplate);
}

runtime::Ref<model::templates::spawns::SpawnTemplate> SpawnEngine::newSingleTimeSpawn(int32_t worldId, int32_t npcId, float x, float y, float z,
	int8_t heading, int32_t creatorId) {
	return newSpawn(worldId, npcId, x, y, z, heading, 0, creatorId, std::nullopt, nullptr);
}

runtime::Ref<model::templates::spawns::SpawnTemplate> SpawnEngine::newSpawn(int32_t worldId, int32_t npcId, float x, float y, float z, int8_t heading,
	int32_t respawnTime) {
	return newSpawn(worldId, npcId, x, y, z, heading, respawnTime, 0, std::nullopt, nullptr);
}

runtime::Ref<model::templates::spawns::SpawnTemplate> SpawnEngine::newSpawn(int32_t worldId, int32_t npcId, float x, float y, float z, int8_t heading,
	int32_t respawnTime, int32_t creatorId, std::optional<std::string_view> aiName, const model::templates::event::EventTemplate* eventTemplate) {
	runtime::Ref<model::templates::spawns::SpawnGroup> group = model::templates::spawns::SpawnGroup::create(worldId, npcId, respawnTime, eventTemplate);
	return model::templates::spawns::SpawnTemplate::create(*group, x, y, z, heading, 0, std::nullopt, 0, creatorId, aiName);
}

runtime::Ref<model::templates::spawns::siegespawns::SiegeSpawnTemplate> SpawnEngine::newSiegeSpawn(int32_t worldId, int32_t npcId, int32_t siegeId,
	model::siege::SiegeRace race, model::siege::SiegeModType mod, float x, float y, float z, int8_t heading) {
	runtime::Ref<model::templates::spawns::SpawnGroup> group = model::templates::spawns::SpawnGroup::create(worldId, npcId, 0, nullptr);
	return model::templates::spawns::siegespawns::SiegeSpawnTemplate::create(siegeId, race, mod, *group, x, y, z, heading, 0, std::nullopt, 0);
}

void SpawnEngine::bringIntoWorld(model::gameobjects::VisibleObject& visibleObject, model::templates::spawns::SpawnTemplate& spawn,
	int32_t instanceIndex) {
	bringIntoWorld(visibleObject, spawn.getWorldId(), instanceIndex, spawn.getX(), spawn.getY(), spawn.getZ(), spawn.getHeading());
}

void SpawnEngine::bringIntoWorld(model::gameobjects::VisibleObject& visibleObject, int32_t worldId, int32_t instanceIndex, float x, float y, float z,
	int8_t h) {
	world::World& world = world::World::getInstance();
	world.storeObject(visibleObject);
	world.setPosition(runtime::Ptr<model::gameobjects::VisibleObject>(visibleObject), worldId, instanceIndex, x, y, z, h);
	world.spawn(runtime::Ptr<model::gameobjects::VisibleObject>(visibleObject));
}

void SpawnEngine::bringIntoWorld(model::gameobjects::VisibleObject& visibleObject) {
	if (!visibleObject.getPosition())
		throw runtime::IllegalArgumentException("Position is null");
	world::World& world = world::World::getInstance();
	world.storeObject(visibleObject);
	world.spawn(runtime::Ptr<model::gameobjects::VisibleObject>(visibleObject));
}

void SpawnEngine::spawnAll() {
	dataholders::DataManager::WORLD_MAPS_DATA->forEachParalllel([](const model::templates::world::WorldMapTemplate& worldMapTemplate) {
		runtime::Ptr<world::WorldMap> worldMap = world::World::getInstance().getWorldMap(worldMapTemplate.getMapId());
		if (!worldMap->isInstanceType()) {
			for (runtime::Ptr<world::WorldMapInstance> instance : *worldMap)
				spawnInstance(*instance, int8_t{0}, instance->getOwnerId());
		}
	});
	printWorldSpawnStats();
}

void SpawnEngine::spawnInstance(world::WorldMapInstance& instance, int8_t difficultId, int32_t ownerId) {
	spawnInstance(instance, difficultId, ownerId, nullptr);
}

void SpawnEngine::spawnEventSpawns(world::WorldMapInstance& instance, int8_t difficultId, int32_t ownerId,
	const model::templates::event::EventTemplate* eventTemplate) {
	spawnInstance(instance, difficultId, ownerId, eventTemplate);
}

void SpawnEngine::spawnInstance(world::WorldMapInstance& instance, int8_t difficultId, int32_t ownerId,
	const model::templates::event::EventTemplate* eventTemplate) {
	// Java: a List, Collections.emptyList() for a map without spawns (never null); C++: a snapshot
	std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>> worldSpawns = dataholders::DataManager::SPAWNS_DATA->getSpawnsByWorldId(instance.getMapId());
	if (eventTemplate == nullptr)
		StaticDoorSpawnManager::spawnTemplate(instance);

	int32_t spawnedCounter = 0;
	{ // Java: if (worldSpawns != null), always true: WalkerFormator.organizeAndSpawn also runs for maps without spawn groups
		for (const runtime::Ref<model::templates::spawns::SpawnGroup>& spawn : worldSpawns) {
			if (eventTemplate != nullptr && eventTemplate != spawn->getEventTemplate())
				continue;
			if (spawn->getDifficultId() != 0 && spawn->getDifficultId() != difficultId)
				continue;

			if (spawn->isTemporarySpawn()) {
				TemporarySpawnEngine::addSpawnGroup(*spawn, instance.getInstanceId());
				if (!spawn->getTemporarySpawn()->isInSpawnTime())
					continue;
			}

			if (spawn->getHandlerType()) {
				switch (*spawn->getHandlerType()) {
					case SpawnHandlerType::RIFT:
						services::rift::RiftManager::addRiftSpawnTemplate(*spawn);
						break;
					case SpawnHandlerType::STATIC:
						StaticObjectSpawnManager::spawnTemplate(*spawn, instance.getInstanceId());
						break;
					default:
						break;
				}
			} else if (spawn->hasPool() && checkPool(*spawn)) {
				spawn->resetPoolSpots(instance.getInstanceId());
				for (int32_t i = 0; i < spawn->getPool(); i++) {
					runtime::Ptr<model::templates::spawns::SpawnTemplate> template_ = spawn->reserveRandomFreePoolSpot(instance.getInstanceId());
					if (!template_)
						break;
					spawnObject(*template_, instance.getInstanceId());
					spawnedCounter++;
				}
			} else {
				for (runtime::Ptr<model::templates::spawns::SpawnTemplate> template_ : spawn->getSpawnTemplates().snapshot()) {
					if (template_->getTemporarySpawn() != nullptr && !template_->getTemporarySpawn()->isInSpawnTime())
						continue;
					spawnObject(*template_, instance.getInstanceId());
					spawnedCounter++;
				}
			}
		}
		if (eventTemplate == nullptr)
			WalkerFormator::organizeAndSpawn(instance.getMapId(), instance.getInstanceId());
	}
	if (spawnedCounter > 0) {
		if (eventTemplate == nullptr)
			log.info("Spawned " + std::to_string(spawnedCounter) + " objects in " + instance.toString());
		else
			log.info("[" + eventTemplate->getName() + "] Spawned " + std::to_string(spawnedCounter) + " event objects in " + instance.toString());
	}
	if (eventTemplate == nullptr)
		services::HousingService::getInstance().spawnHouses(instance, ownerId);
}

bool SpawnEngine::checkPool(model::templates::spawns::SpawnGroup& spawn) {
	if (spawn.getPool() >= spawn.getSpawnTemplates().size()) {
		log.warn("Spawn pool size must be smaller than spots to take effect, npcId: " + std::to_string(spawn.getNpcId()) +
			", worldId: " + std::to_string(spawn.getWorldId()));
		return false;
	}
	return true;
}

void SpawnEngine::printWorldSpawnStats() {
	StatsCollector function;
	world::World::getInstance().forEachObject([&function](model::gameobjects::VisibleObject& object) { function.accept(object); });
	log.info("Loaded " + std::to_string(function.getNpcCount()) + " npc spawns");
	log.info("Loaded " + std::to_string(function.getGatherableCount()) + " gatherable spawns");
}

} // namespace aion::gameserver::spawnengine
