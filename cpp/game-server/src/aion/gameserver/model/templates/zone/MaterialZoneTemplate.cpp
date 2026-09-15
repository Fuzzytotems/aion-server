#include "aion/gameserver/model/templates/zone/MaterialZoneTemplate.h"

#include <cmath>
#include <memory>
#include <string>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/geoEngine/bounding/BoundingBox.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/geoEngine/scene/Spatial.h"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"
#include "aion/gameserver/model/templates/zone/AreaType.h"
#include "aion/gameserver/model/templates/zone/Cylinder.h"
#include "aion/gameserver/model/templates/zone/Semisphere.h"
#include "aion/gameserver/model/templates/zone/Sphere.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::templates::zone {

MaterialZoneTemplate::MaterialZoneTemplate(geoEngine::scene::Spatial& geometry, int32_t mapId) {
	mapid = mapId;
	const world::WorldMapTemplate* map = dataholders::DataManager::WORLD_MAPS_DATA->getTemplate(mapId);
	if (map == nullptr) // Java: NullPointerException on getTemplate(mapId).getFlags()
		throw runtime::NullPointerException("WorldMapsData.getTemplate(" + std::to_string(mapId) + ") is null");
	flags = map->getFlags();
	std::string geometryName = geometry.getName();
	setXmlName(geometryName + "_" + std::to_string(mapId));
	// Java: (BoundingBox) geometry.getWorldBound() - ClassCastException for another bound, NullPointerException for none
	auto* box = dynamic_cast<geoEngine::bounding::BoundingBox*>(geometry.getWorldBound().get());
	if (box == nullptr)
		throw runtime::ClassCastException("MaterialZoneTemplate: the world bound of " + geometryName + " is not a BoundingBox");
	geoEngine::math::Vector3f center = box->getCenter();
	// don't use polygons for small areas, they are bugged in Java API
	if (geometryName.find("CYLINDER") != std::string::npos || geometryName.find("CONE") != std::string::npos ||
		geometryName.find("H_COLUME") != std::string::npos) {
		areaType = AreaType::CYLINDER;
		float r = static_cast<float>(std::sqrt(static_cast<double>(box->getXExtent() * box->getXExtent() + box->getYExtent() * box->getYExtent())));
		cylinder = std::make_unique<Cylinder>(center.getX(), center.getY(), r + 1, center.getZ() + box->getZExtent() + 1,
			center.getZ() - box->getZExtent() - 1);
	} else if (geometryName.find("SEMISPHERE") != std::string::npos) {
		areaType = AreaType::SEMISPHERE;
		semisphere = std::make_unique<Semisphere>(center.getX(), center.getY(), center.getZ(), calculateDistanceFromCenterToCorner(*box) + 1);
	} else {
		areaType = AreaType::SPHERE;
		sphere = std::make_unique<Sphere>(center.getX(), center.getY(), center.getZ(), calculateDistanceFromCenterToCorner(*box) + 1);
	}
}

float MaterialZoneTemplate::calculateDistanceFromCenterToCorner(geoEngine::bounding::BoundingBox& box) {
	// all corners are the same distance from the center of the box
	float distanceFromCenterToEdgeSquared = box.getXExtent() * box.getXExtent() + box.getYExtent() * box.getYExtent();
	float distanceFromCenterToConerSquared = distanceFromCenterToEdgeSquared + box.getZExtent() * box.getZExtent();
	return static_cast<float>(std::sqrt(static_cast<double>(distanceFromCenterToConerSquared)));
}

} // namespace aion::gameserver::model::templates::zone
