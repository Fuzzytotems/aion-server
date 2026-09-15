#include "aion/gameserver/model/geometry/CylinderArea.h"

#include <cmath>

#include "aion/gameserver/model/geometry/Point2DFactory.h"
#include "aion/gameserver/model/geometry/RectangleArea.h"
#include "aion/gameserver/model/templates/zone/Point2D.h"
#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::model::geometry {

CylinderArea::CylinderArea(const world::zone::ZoneName* zoneNameValue, int32_t worldIdValue, const templates::zone::Point2D* center,
	float radiusValue, float minZValue, float maxZValue)
	: CylinderArea(zoneNameValue, worldIdValue, center->getX(), center->getY(), radiusValue, minZValue, maxZValue) {
}

runtime::Ref<CylinderArea> CylinderArea::create(const world::zone::ZoneName* zoneNameValue, int32_t worldIdValue,
	const templates::zone::Point2D* center, float radiusValue, float minZValue, float maxZValue) {
	return runtime::makeRef<CylinderArea>(zoneNameValue, worldIdValue, center, radiusValue, minZValue, maxZValue);
}

CylinderArea::CylinderArea(const world::zone::ZoneName* zoneNameValue, int32_t worldIdValue, float x, float y, float radiusValue, float minZValue,
	float maxZValue)
	: AbstractArea(zoneNameValue, worldIdValue, minZValue, maxZValue), centerX(x), centerY(y), radius(radiusValue) {
}

runtime::Ref<CylinderArea> CylinderArea::create(const world::zone::ZoneName* zoneNameValue, int32_t worldIdValue, float x, float y,
	float radiusValue, float minZValue, float maxZValue) {
	return runtime::makeRef<CylinderArea>(zoneNameValue, worldIdValue, x, y, radiusValue, minZValue, maxZValue);
}

bool CylinderArea::isInside2D(float x, float y) {
	return utils::PositionUtil::getDistance(centerX, centerY, x, y) < radius;
}

double CylinderArea::getDistance2D(float x, float y) {
	if (isInside2D(x, y))
		return 0;
	return std::fabs(utils::PositionUtil::getDistance(centerX, centerY, x, y) - radius);
}

double CylinderArea::getDistance3D(float x, float y, float z) {
	if (isInside3D(x, y, z))
		return 0;
	if (isInsideZ(z))
		return getDistance2D(x, y);
	if (z < getMinZ())
		return utils::PositionUtil::getDistance(centerX, centerY, getMinZ(), x, y, z);
	return utils::PositionUtil::getDistance(centerX, centerY, getMaxZ(), x, y, z);
}

std::optional<templates::zone::Point2D> CylinderArea::getClosestPoint(float x, float y) {
	return getClosestPoint2D(x, y);
}

templates::zone::Point2D CylinderArea::getClosestPoint2D(float x, float y) {
	if (isInside2D(x, y))
		return makePoint2D(x, y);
	float vX = x - this->centerX;
	float vY = y - this->centerY;
	double magV = utils::PositionUtil::getDistance(centerX, centerY, x, y);
	double pointX = centerX + vX / magV * radius;
	double pointY = centerY + vY / magV * radius;
	return makePoint2D(static_cast<float>(pointX), static_cast<float>(pointY));
}

bool CylinderArea::intersectsRectangle(RectangleArea& area) {
	if (area.getMinZ() > getMaxZ() || area.getMaxZ() < getMinZ())
		return false;
	if (area.getDistance2D(centerX, centerY) < radius)
		return true;
	return false;
}

CylinderArea::~CylinderArea() = default;

} // namespace aion::gameserver::model::geometry
