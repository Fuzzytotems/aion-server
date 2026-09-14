#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/world/MapRegion.h"

namespace aion::gameserver::world {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.world.WorldPosition");

WorldPosition::WorldPosition(int32_t mapIdValue) : mapId(mapIdValue) {
}

WorldPosition::WorldPosition(int32_t mapIdValue, float xValue, float yValue, float zValue, int8_t h)
	: WorldPosition(mapIdValue, xValue, yValue, zValue, h, nullptr) {
}

WorldPosition::WorldPosition(int32_t mapIdValue, float xValue, float yValue, float zValue, int8_t h, runtime::Ptr<MapRegion> mapRegionValue)
	: mapId(mapIdValue), x(xValue), y(yValue), z(zValue), heading(h) {
	mapRegion.set(mapRegionValue);
}

WorldPosition::~WorldPosition() = default;

runtime::Ref<WorldPosition> WorldPosition::create(int32_t mapIdValue) {
	return runtime::makeRef<WorldPosition>(mapIdValue);
}

runtime::Ref<WorldPosition> WorldPosition::create(int32_t mapIdValue, float xValue, float yValue, float zValue, int8_t h) {
	return runtime::makeRef<WorldPosition>(mapIdValue, xValue, yValue, zValue, h);
}

runtime::Ref<WorldPosition> WorldPosition::create(int32_t mapIdValue, float xValue, float yValue, float zValue, int8_t h,
	runtime::Ptr<MapRegion> mapRegionValue) {
	return runtime::makeRef<WorldPosition>(mapIdValue, xValue, yValue, zValue, h, mapRegionValue);
}

int32_t WorldPosition::getMapId() {
	AION_UNPORTED();
}

bool WorldPosition::isMapRegionActive() {
	AION_UNPORTED();
}

int32_t WorldPosition::getInstanceId() {
	AION_UNPORTED();
}

bool WorldPosition::isInstanceMap() {
	AION_UNPORTED();
}

runtime::Ptr<WorldMapInstance> WorldPosition::getWorldMapInstance() {
	AION_UNPORTED();
}

void WorldPosition::setMapRegion(runtime::Ptr<MapRegion> r) {
	mapRegion.set(r);
}

void WorldPosition::setXYZH(std::optional<float> newX, std::optional<float> newY, std::optional<float> newZ, std::optional<int8_t> newHeading) {
	AION_UNPORTED();
}

int32_t WorldPosition::hashCode() const {
	AION_UNPORTED();
}

bool WorldPosition::equals(const WorldPosition& obj) const {
	AION_UNPORTED();
}

std::string WorldPosition::toString() {
	AION_UNPORTED();
}

std::string WorldPosition::toCoordString() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::world
