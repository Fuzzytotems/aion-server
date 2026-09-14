#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"

#include <utility>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/templates/spawns/Spawn.h"
#include "aion/gameserver/model/templates/spawns/SpawnSpotTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/siegespawns/SiegeSpawnTemplate.h"

namespace aion::gameserver::model::templates::spawns {

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
	// Java: for each spot, a BaseSpawnTemplate(this, template) with setId(id) and setOccupier(occupier), added to spots
	static_cast<void>(id);
	static_cast<void>(occupier);
	AION_UNPORTED();
}

SpawnGroup::SpawnGroup(int32_t worldIdValue, const Spawn* spawn, int32_t id)
	: SpawnGroup(worldIdValue, spawn, std::vector<std::unique_ptr<SpawnTemplate>>()) {
	// Java: for each spot, a RiftSpawnTemplate(this, template) with setId(id), added to spots
	static_cast<void>(id);
	AION_UNPORTED();
}

SpawnGroup::SpawnGroup(int32_t worldIdValue, const Spawn* spawn, int32_t id, model::vortex::VortexStateType type)
	: SpawnGroup(worldIdValue, spawn, std::vector<std::unique_ptr<SpawnTemplate>>()) {
	// Java: for each spot, a VortexSpawnTemplate(this, template) with setId(id) and setStateType(type), added to spots
	static_cast<void>(id);
	static_cast<void>(type);
	AION_UNPORTED();
}

SpawnGroup::SpawnGroup(int32_t worldIdValue, const Spawn* spawn, int32_t siegeId, model::siege::SiegeRace race, model::siege::SiegeModType mod)
	: SpawnGroup(worldIdValue, spawn, std::vector<std::unique_ptr<SpawnTemplate>>()) {
	for (const SpawnSpotTemplate& template_ : spawn->getSpawnSpotTemplates())
		spots.add(std::make_unique<siegespawns::SiegeSpawnTemplate>(siegeId, race, mod, *this, &template_));
}

SpawnGroup::SpawnGroup(int32_t worldIdValue, const Spawn* spawn, int32_t stage, services::panesterra::ahserion::PanesterraFaction faction)
	: SpawnGroup(worldIdValue, spawn, std::vector<std::unique_ptr<SpawnTemplate>>()) {
	// Java: for each spot, an AhserionsFlightSpawnTemplate(this, template) with setStage(stage) and setPanesterraTeam(faction), added to spots
	static_cast<void>(stage);
	static_cast<void>(faction);
	AION_UNPORTED();
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
	AION_UNPORTED();
}

bool SpawnGroup::isTemporarySpawn() {
	AION_UNPORTED();
}

runtime::Ptr<SpawnTemplate> SpawnGroup::reserveRandomFreePoolSpot(int32_t instanceId) {
	AION_UNPORTED();
}

void SpawnGroup::resetPoolSpot(int32_t instanceId, SpawnTemplate& template_) {
	AION_UNPORTED();
}

void SpawnGroup::resetPoolSpots(int32_t instanceId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::templates::spawns
