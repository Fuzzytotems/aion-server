#include "aion/gameserver/model/templates/zone/Cylinder.h"

namespace aion::gameserver::model::templates::zone {

Cylinder::Cylinder(float xValue, float yValue, float radius, float topValue, float bottomValue)
	: top(topValue), bottom(bottomValue), x(xValue), y(yValue), r(radius) {
}

} // namespace aion::gameserver::model::templates::zone
