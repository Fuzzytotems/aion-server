#include "aion/gameserver/world/WorldMap.h"

#include <memory>
#include <string>

#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldMapInstanceFactory.h"
#include "aion/gameserver/world/zone/ZoneAttributes.h"
#include "aion/gameserver/world/zone/ZoneAttributesInfo.h"

namespace aion::gameserver::world {

WorldMap::WorldMap(const model::templates::world::WorldMapTemplate* worldMapTemplateValue) : worldMapTemplate(worldMapTemplateValue) {
	worldOptions.set(worldMapTemplate->getFlags());

	for (int32_t i = 1; i <= getInstanceCount(); i++) {
		// Performance (World creation, see World::World): only inside World creation's explicit opt-in (not under any QuiescentScope a caller
		// opened); this frame holds no borrow between instances
		if (runtime::QuiescentOptIn::active(runtime::QuiescentOptIn::WORLD_CREATION))
			runtime::quiescentPoint(); // quiescent-safe: the instances are held by this map under construction, kept by makeRef's frame
		if (isInstanceType()) // default instances are inaccessible but its handler methods are sometimes called via MainWorldMapInstance, e.g. on relog
			WorldMapInstanceFactory::createWorldMapInstance(*this, 0,
				[](WorldMapInstance& mapInstance) -> runtime::Ref<gameserver::instance::handlers::InstanceHandler> {
					return gameserver::instance::handlers::GeneralInstanceHandler::create(mapInstance); // Java: GeneralInstanceHandler::new
				},
				0);
		else
			WorldMapInstanceFactory::createWorldMapInstance(*this, 0);
	}
}

WorldMap::~WorldMap() = default;

runtime::Ref<WorldMap> WorldMap::create(const model::templates::world::WorldMapTemplate* worldMapTemplateValue) {
	return runtime::makeRef<WorldMap>(worldMapTemplateValue);
}

std::string WorldMap::getName() {
	return worldMapTemplate->getName();
}

int32_t WorldMap::getWaterLevel() {
	return worldMapTemplate->getWaterLevel();
}

int32_t WorldMap::getDeathLevel() {
	return worldMapTemplate->getDeathLevel();
}

WorldType WorldMap::getWorldType() {
	return worldMapTemplate->getWorldType();
}

int32_t WorldMap::getWorldSize() {
	return worldMapTemplate->getWorldSize();
}

WorldDropType WorldMap::getWorldDropType() {
	return worldMapTemplate->getWorldDropType();
}

int32_t WorldMap::getMapId() {
	return worldMapTemplate->getMapId();
}

bool WorldMap::isFlightAllowed() {
	return (worldOptions.get() & getId(zone::ZoneAttributes::FLY)) != 0;
}

bool WorldMap::isExceptBuff() {
	return worldMapTemplate->isExceptBuff();
}

bool WorldMap::canGlide() {
	return (worldOptions.get() & getId(zone::ZoneAttributes::GLIDE)) != 0;
}

bool WorldMap::canPutKisk() {
	return (worldOptions.get() & getId(zone::ZoneAttributes::BIND)) != 0;
}

bool WorldMap::canRecall() {
	return (worldOptions.get() & getId(zone::ZoneAttributes::RECALL)) != 0;
}

bool WorldMap::canRide() {
	return (worldOptions.get() & getId(zone::ZoneAttributes::RIDE)) != 0;
}

bool WorldMap::canFlyRide() {
	return (worldOptions.get() & getId(zone::ZoneAttributes::FLY_RIDE)) != 0;
}

bool WorldMap::isPvpAllowed() {
	return (worldOptions.get() & getId(zone::ZoneAttributes::PVP_ENABLED)) != 0;
}

bool WorldMap::isSameRaceDuelsAllowed() {
	return (worldOptions.get() & getId(zone::ZoneAttributes::DUEL_SAME_RACE_ENABLED)) != 0;
}

bool WorldMap::isOtherRaceDuelsAllowed() {
	return (worldOptions.get() & getId(zone::ZoneAttributes::DUEL_OTHER_RACE_ENABLED)) != 0;
}

bool WorldMap::canReturnToBattle() {
	return (worldOptions.get() & getId(zone::ZoneAttributes::NO_RETURN_BATTLE)) == 0;
}

void WorldMap::setWorldOption(zone::ZoneAttributes option) {
	// Deviation (D6): Java's unsynchronized `worldOptions |= id` loses a concurrent update of another bit; a compare-and-set loop does not
	int32_t current = worldOptions.get();
	while (!worldOptions.compareAndSet(current, current | getId(option)))
		current = worldOptions.get();
}

void WorldMap::removeWorldOption(zone::ZoneAttributes option) {
	// Deviation (D6): Java's unsynchronized `worldOptions &= ~id` loses a concurrent update of another bit; a compare-and-set loop does not
	int32_t current = worldOptions.get();
	while (!worldOptions.compareAndSet(current, current & ~getId(option)))
		current = worldOptions.get();
}

bool WorldMap::hasOverridenOption(zone::ZoneAttributes option) {
	if ((worldMapTemplate->getFlags() & getId(option)) == 0)
		return (worldOptions.get() & getId(option)) != 0;
	return (worldOptions.get() & getId(option)) == 0;
}

int32_t WorldMap::getInstanceCount() {
	int32_t twinCount = worldMapTemplate->getTwinCount();
	if (twinCount == 0)
		twinCount = 1;
	twinCount += worldMapTemplate->getBeginnerTwinCount();
	return twinCount;
}

runtime::Ptr<WorldMapInstance> WorldMap::getMainWorldMapInstance() {
	// TODO Balance players into instances.
	return instances.get(1);
}

runtime::Ptr<WorldMapInstance> WorldMap::getWorldMapInstance(int32_t instanceId) {
	// instanceId is a count, some code still uses 0 for the default instance
	if (instanceId == 0)
		instanceId = 1;
	if (!isInstanceType()) {
		if (instanceId > getInstanceCount()) {
			throw runtime::IllegalArgumentException("WorldMapInstance " + std::to_string(getMapId()) + " has lower instances count than " +
				std::to_string(instanceId));
		}
	}
	return instances.get(instanceId);
}

void WorldMap::removeWorldMapInstance(int32_t instanceId) {
	// instanceId is a count, some code still uses 0 for the default instance
	if (instanceId == 0)
		instanceId = 1;
	instances.remove(instanceId);
}

void WorldMap::addInstance(int32_t instanceId, WorldMapInstance& instance) {
	// instanceId is a count, some code still uses 0 for the default instance
	if (instanceId == 0)
		instanceId = 1;
	instances.put(instanceId, runtime::Ref<WorldMapInstance>(instance));
}

int32_t WorldMap::getNextInstanceId() {
	return nextInstanceId.incrementAndGet();
}

bool WorldMap::isInstanceType() {
	return worldMapTemplate->isInstance();
}

runtime::JavaIterator<runtime::Ptr<WorldMapInstance>> WorldMap::iterator() {
	return instances.values().iterator();
}

runtime::SnapshotIterator<runtime::Ptr<WorldMapInstance>> WorldMap::begin() {
	return runtime::SnapshotIterator<runtime::Ptr<WorldMapInstance>>(
		std::make_shared<const std::vector<runtime::Ptr<WorldMapInstance>>>(instances.values().toVector()));
}

std::vector<int32_t> WorldMap::getAvailableInstanceIds() {
	return instances.keySet().toVector();
}

void WorldMap::forEachObject(const std::function<void(model::gameobjects::VisibleObject&)>& consumer) {
	for (runtime::Ptr<WorldMapInstance> instance : instances.values())
		instance->forEachObject(consumer);
}

} // namespace aion::gameserver::world
