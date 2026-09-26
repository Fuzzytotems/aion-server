#include "aion/gameserver/model/geometry/SphereArea.h"

#include "aion/gameserver/model/geometry/Point3D.h"
#include "aion/gameserver/model/geometry/RectangleArea.h"
#include "aion/gameserver/model/templates/zone/Point2D.h"
#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::model::geometry {

SphereArea::SphereArea(const world::zone::ZoneName* zoneNameValue, int32_t worldIdValue, float xValue, float yValue, float zValue, float rValue)
	: x(xValue), y(yValue), z(zValue), r(rValue), worldId(worldIdValue), zoneName(zoneNameValue) {
}

runtime::Ref<SphereArea> SphereArea::create(const world::zone::ZoneName* zoneNameValue, int32_t worldIdValue, float xValue, float yValue,
	float zValue, float rValue) {
	return runtime::makeRef<SphereArea>(zoneNameValue, worldIdValue, xValue, yValue, zValue, rValue);
}

bool SphereArea::isInside2D(const templates::zone::Point2D*) {
	return false;
}

bool SphereArea::isInside2D(float, float) {
	return false;
}

bool SphereArea::isInside3D(Point3D& point) {
	return utils::PositionUtil::isInRange(x.get(), y.get(), z.get(), point.getX(), point.getY(), point.getZ(), r.get());
}

bool SphereArea::isInside3D(float xValue, float yValue, float zValue) {
	return utils::PositionUtil::isInRange(xValue, yValue, zValue, x.get(), y.get(), z.get(), r.get());
}

bool SphereArea::isInsideZ(Point3D& point) {
	return isInsideZ(point.getZ());
}

bool SphereArea::isInsideZ(float zValue) {
	return zValue >= this->getMinZ() && zValue <= this->getMaxZ();
}

double SphereArea::getDistance2D(const templates::zone::Point2D*) {
	return 0;
}

double SphereArea::getDistance2D(float, float) {
	return 0;
}

double SphereArea::getDistance3D(Point3D& point) {
	return getDistance3D(point.getX(), point.getY(), point.getZ());
}

double SphereArea::getDistance3D(float xValue, float yValue, float zValue) {
	double distance = utils::PositionUtil::getDistance(xValue, yValue, zValue, x.get(), y.get(), z.get()) - r.get();
	return distance > 0 ? distance : 0;
}

std::optional<templates::zone::Point2D> SphereArea::getClosestPoint(const templates::zone::Point2D*) {
	return std::nullopt;
}

std::optional<templates::zone::Point2D> SphereArea::getClosestPoint(float, float) {
	return std::nullopt;
}

runtime::Ref<Point3D> SphereArea::getClosestPoint(Point3D&) {
	return nullptr;
}

runtime::Ref<Point3D> SphereArea::getClosestPoint(float, float, float) {
	return nullptr;
}

float SphereArea::getMinZ() {
	return z.get() - r.get();
}

float SphereArea::getMaxZ() {
	return z.get() + r.get();
}

bool SphereArea::intersectsRectangle(RectangleArea& area) {
	if (area.getDistance3D(x.get(), y.get(), z.get()) <= r.get())
		return true;
	return false;
}

int32_t SphereArea::getWorldId() {
	return worldId.get();
}

const world::zone::ZoneName* SphereArea::getZoneName() {
	return zoneName.get();
}

SphereArea::~SphereArea() = default;

} // namespace aion::gameserver::model::geometry
