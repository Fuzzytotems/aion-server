#include "aion/gameserver/model/geometry/RectangleArea.h"

#include "aion/gameserver/model/geometry/Point2DFactory.h"
#include "aion/gameserver/model/templates/zone/Point2D.h"
#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::model::geometry {

RectangleArea::RectangleArea(const world::zone::ZoneName* zoneNameValue, int32_t worldIdValue, float minXValue, float minYValue, float maxXValue,
	float maxYValue, float minZValue, float maxZValue)
	: AbstractArea(zoneNameValue, worldIdValue, minZValue, maxZValue), minX(minXValue), maxX(maxXValue), minY(minYValue), maxY(maxYValue) {
}

runtime::Ref<RectangleArea> RectangleArea::create(const world::zone::ZoneName* zoneNameValue, int32_t worldIdValue, float minXValue,
	float minYValue, float maxXValue, float maxYValue, float minZValue, float maxZValue) {
	return runtime::makeRef<RectangleArea>(zoneNameValue, worldIdValue, minXValue, minYValue, maxXValue, maxYValue, minZValue, maxZValue);
}

bool RectangleArea::isInside2D(float x, float y) {
	return x >= minX && x <= maxX && y >= minY && y <= maxY;
}

bool RectangleArea::isInside3D(float x, float y, float z) {
	return isInside2D(x, y) && isInsideZ(z);
}

double RectangleArea::getDistance2D(float x, float y) {
	if (isInside2D(x, y))
		return 0;
	templates::zone::Point2D cp = getClosestPoint2D(x, y);
	return utils::PositionUtil::getDistance(x, y, cp.getX(), cp.getY());
}

double RectangleArea::getDistance3D(float x, float y, float z) {
	if (isInside3D(x, y, z))
		return 0;
	if (isInsideZ(z))
		return getDistance2D(x, y);
	// Java: Point3D cp = getClosestPoint(x, y, z). C++: the coordinates AbstractArea::getClosestPoint(x, y, z) would store, as values (z is
	// outside [minZ, maxZ] here, so it is clamped to the nearer bound): the same floats without a RefCounted Point3D per call (World creation
	// tests Reshanta's 3D regions and zones about 11 million times)
	templates::zone::Point2D cp = getClosestPoint2D(x, y);
	float cpZ = z < getMinZ() ? getMinZ() : getMaxZ();
	return utils::PositionUtil::getDistance(x, y, z, cp.getX(), cp.getY(), cpZ);
}

std::optional<templates::zone::Point2D> RectangleArea::getClosestPoint(float x, float y) {
	return getClosestPoint2D(x, y);
}

templates::zone::Point2D RectangleArea::getClosestPoint2D(float x, float y) {
	if (isInside2D(x, y))
		return makePoint2D(x, y);
	// bottom edge
	templates::zone::Point2D closestPoint = utils::PositionUtil::getClosestPointOnSegment(minX, minY, maxX, minY, x, y);
	double distance = utils::PositionUtil::getDistance(x, y, closestPoint.getX(), closestPoint.getY());
	// top edge
	templates::zone::Point2D cp = utils::PositionUtil::getClosestPointOnSegment(minX, maxY, maxX, maxY, x, y);
	double d = utils::PositionUtil::getDistance(x, y, cp.getX(), cp.getY());
	if (d < distance) {
		closestPoint = cp;
		distance = d;
	}
	// left edge
	cp = utils::PositionUtil::getClosestPointOnSegment(minX, minY, minX, maxY, x, y);
	d = utils::PositionUtil::getDistance(x, y, cp.getX(), cp.getY());
	if (d < distance) {
		closestPoint = cp;
		distance = d;
	}
	// Right edge
	cp = utils::PositionUtil::getClosestPointOnSegment(maxX, minY, maxX, maxY, x, y);
	d = utils::PositionUtil::getDistance(x, y, cp.getX(), cp.getY());
	if (d < distance) {
		closestPoint = cp;
		// distance = d;
	}
	return closestPoint;
}

bool RectangleArea::intersectsRectangle(RectangleArea& area) {
	// TODO Auto-generated method stub
	return false;
}

RectangleArea::~RectangleArea() = default;

} // namespace aion::gameserver::model::geometry
