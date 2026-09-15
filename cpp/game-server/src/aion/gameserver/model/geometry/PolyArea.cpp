#include "aion/gameserver/model/geometry/PolyArea.h"

#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/configs/main/WorldConfig.h"
#include "aion/gameserver/model/geometry/Point3D.h"
#include "aion/gameserver/model/geometry/Polygon2D.h"
#include "aion/gameserver/model/geometry/RectangleArea.h"
#include "aion/gameserver/model/templates/zone/Point2D.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::model::geometry {

PolyArea::PolyArea(const world::zone::ZoneName* zoneNameValue, int32_t worldIdValue, std::span<const templates::zone::Point2D> points,
	float zMin, float zMax)
	: AbstractArea(zoneNameValue, worldIdValue, zMin, zMax), poly(newPolygon(points)) {
}

runtime::Ref<PolyArea> PolyArea::create(const world::zone::ZoneName* zoneNameValue, int32_t worldIdValue,
	std::span<const templates::zone::Point2D> points, float zMin, float zMax) {
	return runtime::makeRef<PolyArea>(zoneNameValue, worldIdValue, points, zMin, zMax);
}

runtime::Ref<Polygon2D> PolyArea::newPolygon(std::span<const templates::zone::Point2D> points) {
	if (points.size() < 3)
		throw runtime::IllegalArgumentException("Not enough points, needed at least 3 but got " + std::to_string(points.size()));
	std::vector<float> xPoints(points.size());
	std::vector<float> yPoints(points.size());
	for (size_t i = 0, n = points.size(); i < n; i++) {
		const templates::zone::Point2D& p = points[i];
		xPoints[i] = p.getX();
		yPoints[i] = p.getY();
	}
	return Polygon2D::create(std::span<const float>(xPoints), std::span<const float>(yPoints), static_cast<int32_t>(points.size()));
}

bool PolyArea::isInside2D(float x, float y) {
	return poly->contains(static_cast<double>(x), static_cast<double>(y));
}

double PolyArea::getDistance2D(float x, float y) {
	if (isInside2D(x, y))
		return 0;
	templates::zone::Point2D cp = getClosestPoint2D(x, y);
	return utils::PositionUtil::getDistance(cp.getX(), cp.getY(), x, y);
}

double PolyArea::getDistance3D(float x, float y, float z) {
	if (isInside3D(x, y, z))
		return 0;
	if (isInsideZ(z))
		return getDistance2D(x, y);
	runtime::Ref<Point3D> cp = getClosestPoint(x, y, z);
	return utils::PositionUtil::getDistance(cp->getX(), cp->getY(), cp->getZ(), x, y, z);
}

std::optional<templates::zone::Point2D> PolyArea::getClosestPoint(float x, float y) {
	return getClosestPoint2D(x, y);
}

templates::zone::Point2D PolyArea::getClosestPoint2D(float x, float y) {
	std::optional<templates::zone::Point2D> closestPoint;
	double closestDistance = 0;
	runtime::Ptr<runtime::Array<float>> xpoints = poly->xpoints.get();
	runtime::Ptr<runtime::Array<float>> ypoints = poly->ypoints.get();
	for (int32_t i = 0; i < xpoints->length(); i++) {
		int32_t nextIndex = i + 1;
		if (nextIndex == xpoints->length())
			nextIndex = 0;
		float p1x = (*xpoints)[i].get();
		float p1y = (*ypoints)[i].get();
		float p2x = (*xpoints)[nextIndex].get();
		float p2y = (*ypoints)[nextIndex].get();
		templates::zone::Point2D point = utils::PositionUtil::getClosestPointOnSegment(p1x, p1y, p2x, p2y, x, y);
		if (!closestPoint) {
			closestPoint = point;
			closestDistance = utils::PositionUtil::getDistance(closestPoint->getX(), closestPoint->getY(), x, y);
		} else {
			double newDistance = utils::PositionUtil::getDistance(point.getX(), point.getY(), x, y);
			if (newDistance < closestDistance) {
				closestPoint = point;
				closestDistance = newDistance;
			}
		}
	}
	// the constructor requires at least 3 points, so the loop always sets closestPoint (Java returns null otherwise)
	return *closestPoint;
}

bool PolyArea::intersectsRectangle(RectangleArea& area) {
	if (area.getMinZ() > getMaxZ() || area.getMaxZ() < getMinZ())
		return false;
	const double regionSize = configs::main::WorldConfig::WORLD_REGION_SIZE.load();
	return poly->intersects(area.getMinX(), area.getMinY(), regionSize, regionSize);
}

PolyArea::~PolyArea() = default;

} // namespace aion::gameserver::model::geometry
