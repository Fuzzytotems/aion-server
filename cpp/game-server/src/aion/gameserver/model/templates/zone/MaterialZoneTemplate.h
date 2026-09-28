#pragma once

#include <cstdint>

#include "aion/gameserver/geoEngine/bounding/fwd.h"
#include "aion/gameserver/geoEngine/scene/fwd.h"
#include "aion/gameserver/model/templates/zone/ZoneTemplate.h"
#include "aion/gameserver/model/templates/zone/fwd.h"

namespace aion::gameserver::model::templates::zone {

/**
 * The zone template of a material geometry of the geo data (ZoneService.createMaterialZoneTemplate): a cylinder, semisphere or sphere around the
 * geometry's world bound. A value class (fieldmap K5) built at run time.
 *
 * @author Rolandas, Neon
 */
class MaterialZoneTemplate : public ZoneTemplate {
public:
	MaterialZoneTemplate(geoEngine::scene::Spatial& geometry, int32_t mapId);

private:
	static float calculateDistanceFromCenterToCorner(geoEngine::bounding::BoundingBox& box);
};

} // namespace aion::gameserver::model::templates::zone
