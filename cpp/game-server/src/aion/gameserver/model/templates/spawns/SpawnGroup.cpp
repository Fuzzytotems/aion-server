#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"

#include <string>
#include <utility>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/model/templates/spawns/Spawn.h"
#include "aion/gameserver/model/templates/spawns/SpawnSpotTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/basespawns/BaseSpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/panesterra/AhserionsFlightSpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/riftspawns/RiftSpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/siegespawns/SiegeSpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/vortexspawns/VortexSpawnTemplate.h"

namespace aion::gameserver::model::templates::spawns {

// Java: LoggerFactory.getLogger(SpawnGroup.class) inline in reserveRandomFreePoolSpot
static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.model.templates.spawns.SpawnGroup");

SpawnGroup::SpawnGroup(int32_t worldIdValue, int32_t npcIdValue, int32_t respawnTimeValue, const event::EventTemplate* eventTemplateValue)
	: SpawnGroup(worldIdValue, npcIdValue, 0, respawnTimeValue, 0, std::nullopt, nullptr, std::vector<std::unique_ptr<SpawnTemplate>>(),
		  eventTemplateValue) {
}

SpawnGroup::SpawnGroup(int32_t worldIdValue, const Spawn* spawn)
	: SpawnGroup(worldIdValue, spawn, std::vector<std::unique_ptr<SpawnTemplate>>()) {
	for (const SpawnSpotTemplate& template_ : spawn->getSpawnSpotTemplates())
		spots.add(std::make_unique<SpawnTemplate>(*this, &template_));
}

SpawnGroup::SpawnGroup(int32_t worldIdValue, const Spawn* spawn, int32_t id, model::base::BaseOccupier occupier)
	: SpawnGroup(worldIdValue, spawn, std::vector<std::unique_ptr<SpawnTemplate>>()) {
	for (const SpawnSpotTemplate& template_ : spawn->getSpawnSpotTemplates()) {
		auto spawnTemplate = std::make_unique<basespawns::BaseSpawnTemplate>(*this, &template_);
		spawnTemplate->setId(id);
		spawnTemplate->setOccupier(occupier);
		spots.add(std::move(spawnTemplate));
	}
}

SpawnGroup::SpawnGroup(int32_t worldIdValue, const Spawn* spawn, int32_t id)
	: SpawnGroup(worldIdValue, spawn, std::vector<std::unique_ptr<SpawnTemplate>>()) {
	for (const SpawnSpotTemplate& template_ : spawn->getSpawnSpotTemplates()) {
		auto spawnTemplate = std::make_unique<riftspawns::RiftSpawnTemplate>(*this, &template_);
		spawnTemplate->setId(id);
		spots.add(std::move(spawnTemplate));
	}
}

SpawnGroup::SpawnGroup(int32_t worldIdValue, const Spawn* spawn, int32_t id, model::vortex::VortexStateType type)
	: SpawnGroup(worldIdValue, spawn, std::vector<std::unique_ptr<SpawnTemplate>>()) {
	for (const SpawnSpotTemplate& template_ : spawn->getSpawnSpotTemplates()) {
		auto spawnTemplate = std::make_unique<vortexspawns::VortexSpawnTemplate>(*this, &template_);
		spawnTemplate->setId(id);
		spawnTemplate->setStateType(type);
		spots.add(std::move(spawnTemplate));
	}
}

SpawnGroup::SpawnGroup(int32_t worldIdValue, const Spawn* spawn, int32_t siegeId, model::siege::SiegeRace race, model::siege::SiegeModType mod)
	: SpawnGroup(worldIdValue, spawn, std::vector<std::unique_ptr<SpawnTemplate>>()) {
	for (const SpawnSpotTemplate& template_ : spawn->getSpawnSpotTemplates())
		spots.add(std::make_unique<siegespawns::SiegeSpawnTemplate>(siegeId, race, mod, *this, &template_));
}

SpawnGroup::SpawnGroup(int32_t worldIdValue, const Spawn* spawn, int32_t stage, services::panesterra::ahserion::PanesterraFaction faction)
	: SpawnGroup(worldIdValue, spawn, std::vector<std::unique_ptr<SpawnTemplate>>()) {
	for (const SpawnSpotTemplate& template_ : spawn->getSpawnSpotTemplates()) {
		auto ahserionTemplate = std::make_unique<panesterra::AhserionsFlightSpawnTemplate>(*this, &template_);
		ahserionTemplate->setStage(stage);
		ahserionTemplate->setPanesterraTeam(faction);
		spots.add(std::move(ahserionTemplate));
	}
}

SpawnGroup::SpawnGroup(int32_t worldIdValue, const Spawn* spawn, std::vector<std::unique_ptr<SpawnTemplate>> spotsValue)
	: SpawnGroup(worldIdValue, spawn->getNpcId(), spawn->getPool(), spawn->getRespawnTime(), spawn->getDifficultId(), spawn->getSpawnHandlerType(),
		  spawn->getTemporarySpawn(), std::move(spotsValue), spawn->getEventTemplate()) {
}

SpawnGroup::SpawnGroup(int32_t worldIdValue, int32_t npcIdValue, int32_t poolValue, int32_t respawnTimeValue, int8_t difficultIdValue,
	std::optional<spawnengine::SpawnHandlerType> handlerTypeValue, const TemporarySpawn* temporarySpawnValue,
	std::vector<std::unique_ptr<SpawnTemplate>> spotsValue, const event::EventTemplate* eventTemplateValue)
	: worldId(worldIdValue), npcId(npcIdValue), pool(poolValue), respawnTime(respawnTimeValue), difficultId(difficultIdValue),
	  handlerType(handlerTypeValue), temporarySpawn(temporarySpawnValue), eventTemplate(eventTemplateValue) {
	for (std::unique_ptr<SpawnTemplate>& spot : spotsValue)
		spots.add(std::move(spot));
	// Java: poolUsedTemplates = hasPool() ? new HashMap<>() : Collections.emptyMap() (the C++ map always exists)
}

SpawnGroup::~SpawnGroup() = default;

runtime::Ref<SpawnGroup> SpawnGroup::create(int32_t worldIdValue, int32_t npcIdValue, int32_t respawnTimeValue,
	const event::EventTemplate* eventTemplateValue) {
	return runtime::makeRef<SpawnGroup>(worldIdValue, npcIdValue, respawnTimeValue, eventTemplateValue);
}

runtime::Ref<SpawnGroup> SpawnGroup::create(int32_t worldIdValue, const Spawn* spawn) {
	return runtime::makeRef<SpawnGroup>(worldIdValue, spawn);
}

runtime::Ref<SpawnGroup> SpawnGroup::create(int32_t worldIdValue, const Spawn* spawn, int32_t id, model::base::BaseOccupier occupier) {
	return runtime::makeRef<SpawnGroup>(worldIdValue, spawn, id, occupier);
}

runtime::Ref<SpawnGroup> SpawnGroup::create(int32_t worldIdValue, const Spawn* spawn, int32_t id) {
	return runtime::makeRef<SpawnGroup>(worldIdValue, spawn, id);
}

runtime::Ref<SpawnGroup> SpawnGroup::create(int32_t worldIdValue, const Spawn* spawn, int32_t id, model::vortex::VortexStateType type) {
	return runtime::makeRef<SpawnGroup>(worldIdValue, spawn, id, type);
}

runtime::Ref<SpawnGroup> SpawnGroup::create(int32_t worldIdValue, const Spawn* spawn, int32_t siegeId, model::siege::SiegeRace race,
	model::siege::SiegeModType mod) {
	return runtime::makeRef<SpawnGroup>(worldIdValue, spawn, siegeId, race, mod);
}

runtime::Ref<SpawnGroup> SpawnGroup::create(int32_t worldIdValue, const Spawn* spawn, int32_t stage,
	services::panesterra::ahserion::PanesterraFaction faction) {
	return runtime::makeRef<SpawnGroup>(worldIdValue, spawn, stage, faction);
}

// lint: L7 Java synchronized (spots) is the PartList's own Monitor, taken by PartList::add
SpawnTemplate& SpawnGroup::addSpawnTemplate(std::unique_ptr<SpawnTemplate> spawnTemplate) {
	return spots.add(std::move(spawnTemplate));
}

SpawnTemplate& SpawnGroup::adoptDetachedTemplate(std::unique_ptr<SpawnTemplate> spawnTemplate) {
	return detachedTemplates.add(std::move(spawnTemplate));
}

bool SpawnGroup::hasPool() {
	return pool > 0;
}

bool SpawnGroup::isTemporarySpawn() {
	return temporarySpawn != nullptr;
}

runtime::Ptr<SpawnTemplate> SpawnGroup::reserveRandomFreePoolSpot(int32_t instanceId) {
	SYNCHRONIZED(poolUsedTemplates) {
		if (!hasPool()) // Java: poolUsedTemplates is Collections.emptyMap() without a pool, whose computeIfAbsent throws
			throw runtime::UnsupportedOperationException("SpawnGroup of npc " + std::to_string(npcId) + " has no pool");
		runtime::Ptr<runtime::RcHashSet<SpawnTemplate*>> occupiedSpots = poolUsedTemplates.computeIfAbsent(
			instanceId, [this]() { return runtime::RcHashSet<SpawnTemplate*>::create(pool); });
		std::vector<runtime::Ptr<SpawnTemplate>> freeSpots;
		for (const runtime::Ptr<SpawnTemplate>& spot : spots.snapshot()) {
			if (!occupiedSpots->contains(spot.get()))
				freeSpots.push_back(spot);
		}
		runtime::Ptr<SpawnTemplate>* freeSpot = commons::utils::Rnd::get(freeSpots);
		if (freeSpot == nullptr) {
			log.warn("All spots are used, could not get random spot for npcId: " + std::to_string(npcId) + ", worldId: " + std::to_string(worldId));
			return nullptr;
		}
		occupiedSpots->add(freeSpot->get());
		return *freeSpot;
	}
}

void SpawnGroup::resetPoolSpot(int32_t instanceId, SpawnTemplate& template_) {
	SYNCHRONIZED(poolUsedTemplates) {
		runtime::Ptr<runtime::RcHashSet<SpawnTemplate*>> occupiedSpots = poolUsedTemplates.get(instanceId); // Java: getOrDefault(emptySet)
		if (occupiedSpots != nullptr)
			occupiedSpots->remove(&template_);
	}
}

void SpawnGroup::resetPoolSpots(int32_t instanceId) {
	SYNCHRONIZED(poolUsedTemplates) {
		runtime::Ptr<runtime::RcHashSet<SpawnTemplate*>> occupiedSpots = poolUsedTemplates.get(instanceId); // Java: getOrDefault(emptySet)
		if (occupiedSpots != nullptr)
			occupiedSpots->clear();
	}
}

} // namespace aion::gameserver::model::templates::spawns
