#include "aion/gameserver/model/geometry/SemisphereArea.h"

#include "aion/gameserver/model/geometry/Point3D.h"
#include "aion/gameserver/model/geometry/RectangleArea.h"
#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::model::geometry {

SemisphereArea::SemisphereArea(const world::zone::ZoneName* zoneNameValue, int32_t worldIdValue, float xValue, float yValue, float zValue,
	float rValue)
	: SphereArea(zoneNameValue, worldIdValue, xValue, yValue, zValue, rValue) {
}

runtime::Ref<SemisphereArea> SemisphereArea::create(const world::zone::ZoneName* zoneNameValue, int32_t worldIdValue, float xValue, float yValue,
	float zValue, float rValue) {
	return runtime::makeRef<SemisphereArea>(zoneNameValue, worldIdValue, xValue, yValue, zValue, rValue);
}

bool SemisphereArea::isInside3D(Point3D& point) {
	return this->z.get() < point.getZ() &&
		utils::PositionUtil::isInRange(x.get(), y.get(), z.get(), point.getX(), point.getY(), point.getZ(), r.get());
}

bool SemisphereArea::isInside3D(float xValue, float yValue, float zValue) {
	return this->z.get() < zValue && utils::PositionUtil::isInRange(xValue, yValue, zValue, this->x.get(), this->y.get(), this->z.get(), r.get());
}

bool SemisphereArea::isInsideZ(Point3D& point) {
	return isInsideZ(point.getZ());
}

float SemisphereArea::getMinZ() {
	return z.get();
}

float SemisphereArea::getMaxZ() {
	return z.get() + r.get();
}

double SemisphereArea::getDistance3D(Point3D& point) {
	return getDistance3D(point.getX(), point.getY(), point.getZ());
}

double SemisphereArea::getDistance3D(float xValue, float yValue, float zValue) {
	double distance = utils::PositionUtil::getDistance(xValue, yValue, zValue, this->x.get(), this->y.get(), this->z.get()) - r.get();
	if (zValue < this->z.get())
		return distance;
	return distance > 0 ? distance : 0;
}

bool SemisphereArea::intersectsRectangle(RectangleArea& area) {
	if ((area.getMaxZ() >= z.get() || z.get() <= area.getMinZ()) && area.getDistance3D(x.get(), y.get(), z.get()) <= r.get())
		return true;
	return false;
}

SemisphereArea::~SemisphereArea() = default;

} // namespace aion::gameserver::model::geometry
