#include "aion/gameserver/world/WorldMap.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::world {

WorldMap::WorldMap(const model::templates::world::WorldMapTemplate* worldMapTemplateValue) : worldMapTemplate(worldMapTemplateValue) {
	// Java: worldOptions = worldMapTemplate.getFlags(); then one WorldMapInstanceFactory.createWorldMapInstance per getInstanceCount()
	AION_UNPORTED();
}

WorldMap::~WorldMap() = default;

runtime::Ref<WorldMap> WorldMap::create(const model::templates::world::WorldMapTemplate* worldMapTemplateValue) {
	return runtime::makeRef<WorldMap>(worldMapTemplateValue);
}

std::string WorldMap::getName() {
	AION_UNPORTED();
}

int32_t WorldMap::getWaterLevel() {
	AION_UNPORTED();
}

int32_t WorldMap::getDeathLevel() {
	AION_UNPORTED();
}

WorldType WorldMap::getWorldType() {
	AION_UNPORTED();
}

int32_t WorldMap::getWorldSize() {
	AION_UNPORTED();
}

WorldDropType WorldMap::getWorldDropType() {
	AION_UNPORTED();
}

int32_t WorldMap::getMapId() {
	AION_UNPORTED();
}

bool WorldMap::isFlightAllowed() {
	AION_UNPORTED();
}

bool WorldMap::isExceptBuff() {
	AION_UNPORTED();
}

bool WorldMap::canGlide() {
	AION_UNPORTED();
}

bool WorldMap::canPutKisk() {
	AION_UNPORTED();
}

bool WorldMap::canRecall() {
	AION_UNPORTED();
}

bool WorldMap::canRide() {
	AION_UNPORTED();
}

bool WorldMap::canFlyRide() {
	AION_UNPORTED();
}

bool WorldMap::isPvpAllowed() {
	AION_UNPORTED();
}

bool WorldMap::isSameRaceDuelsAllowed() {
	AION_UNPORTED();
}

bool WorldMap::isOtherRaceDuelsAllowed() {
	AION_UNPORTED();
}

bool WorldMap::canReturnToBattle() {
	AION_UNPORTED();
}

void WorldMap::setWorldOption(zone::ZoneAttributes option) {
	AION_UNPORTED();
}

void WorldMap::removeWorldOption(zone::ZoneAttributes option) {
	AION_UNPORTED();
}

bool WorldMap::hasOverridenOption(zone::ZoneAttributes option) {
	AION_UNPORTED();
}

int32_t WorldMap::getInstanceCount() {
	AION_UNPORTED();
}

runtime::Ptr<WorldMapInstance> WorldMap::getMainWorldMapInstance() {
	AION_UNPORTED();
}

runtime::Ptr<WorldMapInstance> WorldMap::getWorldMapInstance(int32_t instanceId) {
	AION_UNPORTED();
}

void WorldMap::removeWorldMapInstance(int32_t instanceId) {
	AION_UNPORTED();
}

void WorldMap::addInstance(int32_t instanceId, WorldMapInstance& instance) {
	AION_UNPORTED();
}

int32_t WorldMap::getNextInstanceId() {
	AION_UNPORTED();
}

bool WorldMap::isInstanceType() {
	AION_UNPORTED();
}

runtime::JavaIterator<runtime::Ptr<WorldMapInstance>> WorldMap::iterator() {
	AION_UNPORTED();
}

runtime::SnapshotIterator<runtime::Ptr<WorldMapInstance>> WorldMap::begin() {
	AION_UNPORTED();
}

std::vector<int32_t> WorldMap::getAvailableInstanceIds() {
	AION_UNPORTED();
}

void WorldMap::forEachObject(const std::function<void(model::gameobjects::VisibleObject&)>& consumer) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::world
