#include "aion/gameserver/world/zone/RegionZone.h"

#include "aion/gameserver/configs/main/WorldConfig.h"

namespace aion::gameserver::world::zone {

RegionZone::RegionZone(float startX, float startY, float minZ, float maxZ)
	: RectangleArea(nullptr, 0, startX, startY, startX + static_cast<float>(configs::main::WorldConfig::WORLD_REGION_SIZE.load()),
		  startY + static_cast<float>(configs::main::WorldConfig::WORLD_REGION_SIZE.load()), minZ, maxZ) {
}

RegionZone::~RegionZone() = default;

runtime::Ref<RegionZone> RegionZone::create(float startX, float startY, float minZ, float maxZ) {
	return runtime::makeRef<RegionZone>(startX, startY, minZ, maxZ);
}

bool RegionZone::isInside(model::geometry::AbstractArea& area) {
	return true;
}

} // namespace aion::gameserver::world::zone
