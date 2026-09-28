#include "aion/gameserver/model/geometry/AbstractArea.h"

#include <string>

#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/model/geometry/Point3D.h"
#include "aion/gameserver/model/templates/zone/Point2D.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::geometry {

AbstractArea::AbstractArea(const world::zone::ZoneName* zoneNameValue, int32_t worldIdValue, float minZValue, float maxZValue)
	: minZ(minZValue), maxZ(maxZValue), zoneName(zoneNameValue), worldId(worldIdValue) {
	if (minZValue > maxZValue) {
		throw runtime::IllegalArgumentException("minZ(" + geoEngine::math::JavaFloat::toString(minZValue) + ") > maxZ(" +
			geoEngine::math::JavaFloat::toString(maxZValue) + ")");
	}
}

bool AbstractArea::isInside2D(const templates::zone::Point2D* point) {
	return isInside2D(point->getX(), point->getY());
}

bool AbstractArea::isInside3D(Point3D& point) {
	return isInside3D(point.getX(), point.getY(), point.getZ());
}

bool AbstractArea::isInside3D(float x, float y, float z) {
	return isInsideZ(z) && isInside2D(x, y);
}

bool AbstractArea::isInsideZ(Point3D& point) {
	return isInsideZ(point.getZ());
}

bool AbstractArea::isInsideZ(float z) {
	return z >= getMinZ() && z <= getMaxZ();
}

double AbstractArea::getDistance2D(const templates::zone::Point2D* point) {
	return getDistance2D(point->getX(), point->getY());
}

double AbstractArea::getDistance3D(Point3D& point) {
	return getDistance3D(point.getX(), point.getY(), point.getZ());
}

std::optional<templates::zone::Point2D> AbstractArea::getClosestPoint(const templates::zone::Point2D* point) {
	return getClosestPoint(point->getX(), point->getY());
}

runtime::Ref<Point3D> AbstractArea::getClosestPoint(Point3D& point) {
	return getClosestPoint(point.getX(), point.getY(), point.getZ());
}

runtime::Ref<Point3D> AbstractArea::getClosestPoint(float x, float y, float z) {
	templates::zone::Point2D closest2d = getClosestPoint2D(x, y);
	float zCoord;
	if (isInsideZ(z))
		zCoord = z;
	else if (z < getMinZ())
		zCoord = getMinZ();
	else
		zCoord = getMaxZ();
	return Point3D::create(closest2d.getX(), closest2d.getY(), zCoord);
}

AbstractArea::~AbstractArea() = default;

} // namespace aion::gameserver::model::geometry
