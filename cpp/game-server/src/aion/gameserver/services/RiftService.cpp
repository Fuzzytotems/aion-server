#include "aion/gameserver/services/RiftService.h"

#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/schedule/RiftSchedule.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/RiftData.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/rift/RiftLocation.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/model/templates/rift/OpenRift.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/riftspawns/RiftSpawnTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/RespawnService.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/services/rift/RiftInformer.h"
#include "aion/gameserver/services/rift/RiftManager.h"
#include "aion/gameserver/services/rift/RiftOpenRunnable.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::services {

void RiftService::initRiftLocations() {
	// C++: Java assigns the data holder's map (or Collections.emptyMap()); the port publishes a copy (hub-headers.md §11.1 empty collections)
	runtime::Ref<runtime::RcHashMap<int32_t, runtime::Ref<model::rift::RiftLocation>>> riftLocations =
		runtime::RcHashMap<int32_t, runtime::Ref<model::rift::RiftLocation>>::create(AION_LOCK_CLASS(RiftService::locations));
	if (configs::main::CustomConfig::RIFT_ENABLED.load()) {
		for (const auto& [id, location] : dataholders::DataManager::RIFT_DATA->getRiftLocations())
			riftLocations->put(id, location);
	}
	locations.set(std::move(riftLocations));
}

// Java RiftService.java:43-50
void RiftService::initRifts() {
	if (configs::main::CustomConfig::RIFT_ENABLED.load()) {
		// C++: Java keeps the loaded schedule in the field for the life of the service; the bound schedule is released into it (immortal)
		schedule.set(configs::schedule::RiftSchedule::load().release());
		for (const configs::schedule::RiftSchedule_Rift& rift : schedule.get()->getRiftsList())
			for (const model::templates::rift::OpenRift& open : rift.getRift())
				cron::CronService::getInstance().schedule(
					cron::CronJob(rift::RiftOpenRunnable{.worldId = rift.getWorldId(), .guards = open.spawnGuards()}), open.getSchedule());
	}
}

bool RiftService::isValidId(int32_t id) {
	if (isRift(id)) {
		return RiftService::getInstance().getRiftLocations()->containsKey(id);
	} else {
		for (const runtime::Ptr<model::rift::RiftLocation>& loc : RiftService::getInstance().getRiftLocations()->values()) {
			if (loc->getWorldId() == id)
				return true;
		}
	}
	return false;
}

bool RiftService::isRift(int32_t id) {
	return id < 10000;
}

// Java RiftService.java:68-94
bool RiftService::openRifts(int32_t id, bool guards) {
	if (isValidId(id)) {
		if (isRift(id)) {
			runtime::Ptr<model::rift::RiftLocation> rift = getRiftLocation(id);
			if (rift->getSpawned().isEmpty()) {
				openRifts(*rift, guards);
				// Broadcast rift spawn on map
				rift::RiftInformer::sendRiftsInfo(rift->getWorldId());
				return true;
			}
		} else {
			bool opened = false;
			for (const runtime::Ptr<model::rift::RiftLocation>& rift : getRiftLocations()->values()) {
				if (rift->getWorldId() == id && rift->getSpawned().isEmpty()) {
					openRifts(*rift, guards);
					opened = true;
				}
			}
			// Broadcast rift spawn on map
			rift::RiftInformer::sendRiftsInfo(id);
			return opened;
		}
	}
	return false;
}

// Java RiftService.java:96-116
bool RiftService::closeRifts(int32_t id) {
	if (isValidId(id)) {
		if (isRift(id)) {
			runtime::Ptr<model::rift::RiftLocation> rift = getRiftLocation(id);
			if (!rift->getSpawned().isEmpty()) {
				closeRift(*rift);
				return true;
			}
		} else {
			bool opened = false;
			for (const runtime::Ptr<model::rift::RiftLocation>& rift : getRiftLocations()->values()) {
				if (rift->getWorldId() == id && !rift->getSpawned().isEmpty()) {
					closeRift(*rift);
					opened = true;
				}
			}
			return opened;
		}
	}
	return false;
}

// Java RiftService.java:124-160
void RiftService::prepareRiftOpening(int32_t id, bool guards) {
	if (id != 210070000 && id != 220080000) {
		if (!guards && commons::utils::Rnd::nextBoolean())
			return;
	}
	std::vector<runtime::Ptr<model::rift::RiftLocation>> possibleLocs;
	for (const runtime::Ptr<model::rift::RiftLocation>& loc : locations.get()->values()) {
		if (loc->getWorldId() == id) {
			if (guards) {
				if (loc->hasSpawns() && loc->isAutoCloseable())
					possibleLocs.push_back(loc);
				continue;
			}
			if (!loc->hasSpawns() && loc->isAutoCloseable())
				possibleLocs.push_back(loc);
		}
	}
	int32_t maxCount = 1;
	if (!guards) {
		switch (id) {
			case 210050000:
			case 220070000:
				maxCount = 3;
				break;
			default:
				maxCount = 4;
		}
	}
	const int32_t count = commons::utils::Rnd::get(1, maxCount);
	while (static_cast<int32_t>(possibleLocs.size()) > count)
		possibleLocs.erase(possibleLocs.begin() + commons::utils::Rnd::get(0, static_cast<int32_t>(possibleLocs.size()) - 1));
	for (const runtime::Ptr<model::rift::RiftLocation>& loc : possibleLocs)
		openRifts(*loc, guards);
}

// Java RiftService.java:162-180
void RiftService::openRifts(model::rift::RiftLocation& location, bool isWithGuards) {
	if (activeRifts.putIfAbsent(location.getId(), runtime::Ref<model::rift::RiftLocation>(location)))
		return;
	location.setOpened(true);

	// Spawn NPC guards
	if (isWithGuards) {
		const std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>>* locSpawns =
			dataholders::DataManager::SPAWNS_DATA->getRiftSpawnsByLocId(location.getId());
		if (locSpawns == nullptr)
			throw runtime::NullPointerException("no rift spawns for location " + std::to_string(location.getId())); // Java: for (... : null)
		for (const runtime::Ref<model::templates::spawns::SpawnGroup>& group : *locSpawns) {
			for (const runtime::Ptr<model::templates::spawns::SpawnTemplate>& st : group->getSpawnTemplates().snapshot()) {
				runtime::Ptr<model::templates::spawns::riftspawns::RiftSpawnTemplate> spawnTemplate =
					runtime::cast<model::templates::spawns::riftspawns::RiftSpawnTemplate>(st);
				location.addSpawned(*spawnengine::SpawnEngine::spawnObject(*spawnTemplate, 1));
			}
		}
	}

	// Spawn rifts
	rift::RiftManager::getInstance().spawnRift(location, isWithGuards);
}

// Java RiftService.java:182-202
void RiftService::closeRift(model::rift::RiftLocation& location) {
	if (!activeRifts.remove(location.getId(), runtime::Ref<model::rift::RiftLocation>(location)))
		return;
	location.setOpened(false);

	// Despawn rift NPCs and cancel their respawns
	for (const auto& [npcObjectId, npcSpawnTemplate] : location.getSpawned().snapshot()) {
		runtime::Ptr<model::gameobjects::VisibleObject> npc = world::World::getInstance().findVisibleObject(npcObjectId);
		// npcObjectId may have been released and reassigned to a completely unrelated mob, if the original (now despawned) rift npc was a non
		// respawning mob. spawned list doesn't clean up removed spawns but only updates respawning npcObjectIds, therefore the npcSpawnTemplate check
		if (npc && npc->getSpawn().get() == npcSpawnTemplate.get())
			npc->getController().deleteIfAliveOrCancelRespawn();
		else
			RespawnService::cancelRespawn(npcObjectId, *npcSpawnTemplate);
	}

	// Clear spawned list
	location.getSpawned().clear();
}

bool RiftService::isRiftOpened(int32_t riftId) {
	return activeRifts.containsKey(riftId);
}

// Java RiftService.java:208-214
void RiftService::closeAutoCloseableRifts(int32_t worldId) {
	for (const runtime::Ptr<model::rift::RiftLocation>& rift : activeRifts.values()) {
		if (rift->isAutoCloseable() && rift->getWorldId() == worldId) {
			closeRift(*rift);
		}
	}
}

void RiftService::updateSpawned(int32_t oldObjectId, model::gameobjects::VisibleObject& respawn) {
	for (const runtime::Ptr<model::rift::RiftLocation>& loc : locations.get()->values()) {
		if (loc->replaceSpawned(oldObjectId, respawn))
			break;
	}
}

int32_t RiftService::getDuration() {
	return configs::main::CustomConfig::RIFT_DURATION.load();
}

runtime::Ptr<model::rift::RiftLocation> RiftService::getRiftLocation(int32_t id) {
	return locations.get()->get(id);
}

RiftService& RiftService::getInstance() {
	static RiftService instance; // Java RiftServiceHolder
	return instance;
}

} // namespace aion::gameserver::services
