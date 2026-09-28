#pragma once

#include "aion/gameserver/model/templates/zone/Point2D.h"

namespace aion::gameserver::model::geometry {

/**
 * C++ only: Java `new Point2D(x, y)` for the run-time points of PositionUtil and the areas (closest points).
 */
inline templates::zone::Point2D makePoint2D(float x, float y) {
	return templates::zone::Point2D(x, y);
}

} // namespace aion::gameserver::model::geometry
