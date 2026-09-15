#include "aion/gameserver/model/templates/zone/MaterialZoneTemplate.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::templates::zone {

MaterialZoneTemplate::MaterialZoneTemplate(geoEngine::scene::Spatial& geometry, int32_t mapId) {
	// Java: mapid, flags = DataManager.WORLD_MAPS_DATA.getTemplate(mapId).getFlags(), setXmlName(geometry.getName() + "_" + mapId), then a
	// cylinder, semisphere or sphere from the BoundingBox world bound. WorldMapsData::getTemplate exists (header request pre-2); BoundingBox
	// (P4-04) has no declaration header yet.
	static_cast<void>(geometry);
	static_cast<void>(mapId);
	AION_UNPORTED();
}

float MaterialZoneTemplate::calculateDistanceFromCenterToCorner(geoEngine::bounding::BoundingBox& box) {
	// Java: sqrt(x * x + y * y + z * z) of the box extents; BoundingBox (P4-04) has no declaration header yet
	static_cast<void>(box);
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::templates::zone
