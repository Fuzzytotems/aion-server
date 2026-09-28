#include "aion/gameserver/model/templates/flyring/FlyRingTemplate.h"

#include <memory>
#include <string>

namespace aion::gameserver::model::templates::flyring {

FlyRingTemplate::FlyRingTemplate(std::string_view nameValue, int32_t mapId, geometry::Point3D& centerValue, geometry::Point3D& p1Value,
                                 geometry::Point3D& p2Value, int32_t radiusValue) {
	this->name = std::string(nameValue);
	this->map = mapId;
	this->radius = static_cast<float>(radiusValue);
	this->center = std::make_unique<FlyRingPoint>(centerValue);
	this->p1 = std::make_unique<FlyRingPoint>(p1Value);
	this->p2 = std::make_unique<FlyRingPoint>(p2Value);
}

} // namespace aion::gameserver::model::templates::flyring
