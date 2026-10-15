#include "aion/gameserver/world/WorldMap2DInstance.h"

#include <cmath>
#include <vector>

#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/world/MapRegion.h"
#include "aion/gameserver/world/RegionUtil.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"

namespace aion::gameserver::world {

WorldMap2DInstance::WorldMap2DInstance(WorldMap& parentValue, int32_t instanceIdValue, int32_t ownerIdValue, int32_t maxPlayersValue,
	const std::function<runtime::Ref<instance::handlers::InstanceHandler>(WorldMapInstance&)>& instanceHandlerSupplier)
	: WorldMapInstance(parentValue, instanceIdValue, maxPlayersValue, instanceHandlerSupplier), ownerId(ownerIdValue) {
}

WorldMap2DInstance::~WorldMap2DInstance() = default;

runtime::Ref<WorldMap2DInstance> WorldMap2DInstance::create(WorldMap& parentValue, int32_t instanceIdValue, int32_t ownerIdValue,
	int32_t maxPlayersValue, const std::function<runtime::Ref<instance::handlers::InstanceHandler>(WorldMapInstance&)>& instanceHandlerSupplier) {
	runtime::Ref<WorldMap2DInstance> instance =
		runtime::makeRef<WorldMap2DInstance>(parentValue, instanceIdValue, ownerIdValue, maxPlayersValue, instanceHandlerSupplier);
	instance->initMapRegions(); // Java: the last statement of the WorldMapInstance constructor (before ownerId is set; it does not read it)
	return instance;
}

std::unique_ptr<MapRegion> WorldMap2DInstance::createMapRegion(int32_t regionId) {
	float startX = static_cast<float>(RegionUtil::getXFrom2dRegionId(regionId));
	float startY = static_cast<float>(RegionUtil::getYFrom2dRegionId(regionId));
	int32_t size = getParent()->getWorldSize();
	// Java: float maxZ = Math.round((float) size / regionSize) * regionSize (Math.round(float) = (int) floor(x + 0.5f))
	float maxZ = static_cast<float>(static_cast<int32_t>(std::floor(static_cast<float>(size) / static_cast<float>(regionSize()) + 0.5f)) * regionSize());
	std::vector<runtime::Ptr<zone::ZoneInstance>> regionZones = filterZones(getMapId(), regionId, startX, startY, 0, maxZ);
	return std::make_unique<MapRegion>(regionId, *this, regionZones);
}

void WorldMap2DInstance::initMapRegions() {
	int32_t size = getParent()->getWorldSize();
	// Create all mapRegion
	for (int32_t x = 0; x <= size; x = x + regionSize()) {
		for (int32_t y = 0; y <= size; y = y + regionSize()) {
			int32_t regionId = RegionUtil::get2dRegionId(static_cast<float>(x), static_cast<float>(y));
			regions.put(regionId, createMapRegion(regionId));
		}
	}

	// Add Neighbour
	for (int32_t x = 0; x <= size; x = x + regionSize()) {
		for (int32_t y = 0; y <= size; y = y + regionSize()) {
			int32_t regionId = RegionUtil::get2dRegionId(static_cast<float>(x), static_cast<float>(y));
			runtime::Ptr<MapRegion> mapRegion = regions.get(regionId);
			for (int32_t x2 = x - regionSize(); x2 <= x + regionSize(); x2 += regionSize()) {
				for (int32_t y2 = y - regionSize(); y2 <= y + regionSize(); y2 += regionSize()) {
					if (x2 == x && y2 == y)
						continue;
					int32_t neighbourId = RegionUtil::get2dRegionId(static_cast<float>(x2), static_cast<float>(y2));
					runtime::Ptr<MapRegion> neighbour = regions.get(neighbourId);
					if (neighbour)
						mapRegion->addNeighbourRegion(*neighbour);
				}
			}
		}
	}
}

runtime::Ptr<MapRegion> WorldMap2DInstance::getRegion(float x, float y, float z) {
	int32_t regionId = RegionUtil::get2dRegionId(x, y);
	return regions.get(regionId);
}

int32_t WorldMap2DInstance::getOwnerId() {
	return ownerId;
}

bool WorldMap2DInstance::isPersonal() {
	return ownerId != 0;
}

} // namespace aion::gameserver::world
