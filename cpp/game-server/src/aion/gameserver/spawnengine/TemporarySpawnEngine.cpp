#include "aion/gameserver/spawnengine/TemporarySpawnEngine.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/TemporarySpawn.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::spawnengine {

namespace {

using model::gameobjects::VisibleObject;
using model::templates::spawns::SpawnGroup;
using model::templates::spawns::SpawnTemplate;

/** Java: the class monitor of `static synchronized` methods (TemporarySpawnEngine.class) */
runtime::Monitor classMonitor{AION_LOCK_CLASS(TemporarySpawnEngine::classMonitor)};

} // namespace

runtime::HashMap<runtime::Ref<SpawnGroup>, runtime::Ref<runtime::RcHashSet<int32_t>>> TemporarySpawnEngine::spawnGroups{
	AION_LOCK_CLASS(TemporarySpawnEngine::spawnGroups)};
runtime::HashSet<runtime::Ref<VisibleObject>> TemporarySpawnEngine::spawnedObjects{AION_LOCK_CLASS(TemporarySpawnEngine::spawnedObjects)};

void TemporarySpawnEngine::onHourChange() {
	SYNCHRONIZED(classMonitor) {
		despawn();
		spawn();
	}
}

void TemporarySpawnEngine::despawn() {
	std::vector<runtime::Ptr<VisibleObject>> remainingObjects;
	spawnedObjects.forEach([&remainingObjects](const runtime::Ptr<VisibleObject>& object) {
		if (object->getSpawn()->getTemporarySpawn()->canDespawn()) {
			object->getController().deleteIfAliveOrCancelRespawn();
		} else {
			remainingObjects.push_back(object);
		}
	});
	spawnedObjects.retainAll(remainingObjects);
}

void TemporarySpawnEngine::spawn() {
	// Java: spawnedObjects.stream().collect(Collectors.groupingBy(o -> o.getSpawn().getGroup()))
	std::unordered_map<SpawnGroup*, std::vector<runtime::Ptr<VisibleObject>>> spawnedBySpawnGroup;
	for (runtime::Ptr<VisibleObject> o : spawnedObjects.snapshot())
		spawnedBySpawnGroup[&o->getSpawn()->getGroup()].push_back(o);
	for (auto [spawn, instanceIds] : spawnGroups.snapshot()) {
		if (instanceIds->isEmpty())
			continue;
		std::vector<runtime::Ptr<VisibleObject>>& spawned = spawnedBySpawnGroup[spawn.get()];
		if (spawn->hasPool()) {
			if (!spawn->getTemporarySpawn()->canSpawn())
				continue;
			std::vector<int32_t> ids = instanceIds->snapshot();
			std::unordered_set<int32_t> spawnableInstanceIds(ids.begin(), ids.end()); // Java: new HashSet<>(instanceIds) (iteration order unspecified)
			for (const runtime::Ptr<VisibleObject>& o : spawned)
				spawnableInstanceIds.erase(o->getInstanceId());
			for (int32_t instanceId : spawnableInstanceIds) {
				spawn->resetPoolSpots(instanceId);
				for (int32_t pool = 0; pool < spawn->getPool(); pool++) {
					runtime::Ptr<SpawnTemplate> template_ = spawn->reserveRandomFreePoolSpot(instanceId);
					SpawnEngine::spawnObject(*template_, instanceId); // Java: NullPointerException when no free spot is left
				}
			}
		} else {
			for (runtime::Ptr<SpawnTemplate> template_ : spawn->getSpawnTemplates().snapshot()) {
				if (!template_->getTemporarySpawn()->canSpawn())
					continue;
				std::vector<int32_t> ids = instanceIds->snapshot();
				std::unordered_set<int32_t> spawnableInstanceIds(ids.begin(), ids.end());
				for (const runtime::Ptr<VisibleObject>& o : spawned) {
					if (o->getSpawn() == template_) // Java: o.getSpawn().equals(template) (identity)
						spawnableInstanceIds.erase(o->getInstanceId());
				}
				for (int32_t instanceId : spawnableInstanceIds)
					SpawnEngine::spawnObject(*template_, instanceId);
			}
		}
	}
}

void TemporarySpawnEngine::registerSpawned(VisibleObject& object) {
	SYNCHRONIZED(classMonitor) {
		spawnedObjects.add(runtime::Ref<VisibleObject>(object));
	}
}

void TemporarySpawnEngine::unregisterSpawned(int32_t objectId) {
	SYNCHRONIZED(classMonitor) {
		spawnedObjects.removeIf([objectId](const runtime::Ptr<VisibleObject>& o) { return o->getObjectId() == objectId; });
	}
}

void TemporarySpawnEngine::addSpawnGroup(SpawnGroup& spawnGroup, int32_t instanceId) {
	SYNCHRONIZED(classMonitor) {
		spawnGroups.computeIfAbsent(runtime::Ref<SpawnGroup>(spawnGroup), [] { return runtime::RcHashSet<int32_t>::create(); })->add(instanceId);
	}
}

void TemporarySpawnEngine::unregister(const model::templates::event::EventTemplate* eventTemplate) {
	SYNCHRONIZED(classMonitor) {
		spawnedObjects.removeIf([eventTemplate](const runtime::Ptr<VisibleObject>& o) { return o->getSpawn()->getEventTemplate() == eventTemplate; });
		spawnGroups.keySet().removeIf([eventTemplate](const runtime::Ptr<SpawnGroup>& s) { return s->getEventTemplate() == eventTemplate; });
	}
}

void TemporarySpawnEngine::onInstanceDestroy(world::WorldMapInstance& instance) {
	SYNCHRONIZED(classMonitor) {
		spawnedObjects.removeIf([&instance](const runtime::Ptr<VisibleObject>& o) { return o->getWorldMapInstance().get() == &instance; });
		for (auto [spawnGroup, instanceIds] : spawnGroups.snapshot()) {
			if (spawnGroup->getWorldId() == instance.getMapId())
				instanceIds->remove(instance.getInstanceId());
		}
	}
}

} // namespace aion::gameserver::spawnengine
