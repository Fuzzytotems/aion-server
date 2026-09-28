#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/world/MapRegion.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::world {

using geoEngine::math::JavaFloat;

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
	if (mapId == 0)
		log.warn("WorldPosition has (mapId == 0) " + toString());
	return mapId;
}

bool WorldPosition::isMapRegionActive() {
	runtime::Ptr<MapRegion> region = mapRegion.get();
	return region ? region->isActive() : false;
}

int32_t WorldPosition::getInstanceId() {
	runtime::Ptr<MapRegion> region = mapRegion.get();
	return region ? region->getParent().getInstanceId() : 1;
}

bool WorldPosition::isInstanceMap() {
	runtime::Ptr<MapRegion> region = mapRegion.get();
	return region ? region->getParent().getParent()->isInstanceType() : false;
}

runtime::Ptr<WorldMapInstance> WorldPosition::getWorldMapInstance() {
	runtime::Ptr<MapRegion> region = mapRegion.get();
	if (!region)
		throw runtime::NullPointerException("WorldPosition.getWorldMapInstance: mapRegion is null"); // Java: NPE on mapRegion.getParent()
	return region->getParent();
}

void WorldPosition::setMapRegion(runtime::Ptr<MapRegion> r) {
	mapRegion.set(r);
}

void WorldPosition::setXYZH(std::optional<float> newX, std::optional<float> newY, std::optional<float> newZ, std::optional<int8_t> newHeading) {
	if (newX)
		x.set(*newX);
	if (newY)
		y.set(*newY);
	if (newZ)
		z.set(*newZ);
	if (newHeading)
		heading.set(*newHeading);
}

int32_t WorldPosition::hashCode() const {
	// Java: prime 31 over heading, mapId and the floatToIntBits of x, y, z (int arithmetic wraps: unsigned)
	constexpr uint32_t prime = 31;
	uint32_t result = 1;
	result = prime * result + static_cast<uint32_t>(static_cast<int32_t>(heading.get()));
	result = prime * result + static_cast<uint32_t>(mapId);
	result = prime * result + static_cast<uint32_t>(JavaFloat::floatToIntBits(x.get()));
	result = prime * result + static_cast<uint32_t>(JavaFloat::floatToIntBits(y.get()));
	result = prime * result + static_cast<uint32_t>(JavaFloat::floatToIntBits(z.get()));
	return static_cast<int32_t>(result);
}

bool WorldPosition::equals(const WorldPosition& obj) const {
	if (this == &obj)
		return true;
	return mapId == obj.mapId && x.get() == obj.x.get() && y.get() == obj.y.get() && z.get() == obj.z.get() && heading.get() == obj.heading.get();
}

std::string WorldPosition::toString() {
	return "WorldPosition [mapId=" + std::to_string(mapId) + ", x=" + JavaFloat::toString(x.get()) + ", y=" + JavaFloat::toString(y.get()) +
		", z=" + JavaFloat::toString(z.get()) + ", heading=" + std::to_string(heading.get()) + ", isSpawned=" + (isSpawned() ? "true" : "false") + "]";
}

std::string WorldPosition::toCoordString() {
	return "Map ID: " + std::to_string(mapId) + ", Instance ID: " + std::to_string(getInstanceId()) + ", X: " + JavaFloat::toString(x.get()) +
		", Y: " + JavaFloat::toString(y.get()) + ", Z: " + JavaFloat::toString(z.get()) + ", Heading: " + std::to_string(heading.get());
}

} // namespace aion::gameserver::world
