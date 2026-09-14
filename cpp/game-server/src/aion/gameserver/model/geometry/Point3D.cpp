#include "aion/gameserver/model/geometry/Point3D.h"

#include "aion/gameserver/model/templates/zone/Point2D.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::geometry {

Point3D::Point3D() {
}

runtime::Ref<Point3D> Point3D::create() {
	return runtime::makeRef<Point3D>();
}

Point3D::Point3D(const templates::zone::Point2D* point, float zValue) : Point3D(point->getX(), point->getY(), zValue) {
}

runtime::Ref<Point3D> Point3D::create(const templates::zone::Point2D* point, float zValue) {
	return runtime::makeRef<Point3D>(point, zValue);
}

Point3D::Point3D(Point3D& point) : Point3D(point.getX(), point.getY(), point.getZ()) {
}

runtime::Ref<Point3D> Point3D::create(Point3D& point) {
	return runtime::makeRef<Point3D>(point);
}

Point3D::Point3D(float xValue, float yValue, float zValue) : x(xValue), y(yValue), z(zValue) {
}

runtime::Ref<Point3D> Point3D::create(float xValue, float yValue, float zValue) {
	return runtime::makeRef<Point3D>(xValue, yValue, zValue);
}

Point3D::Point3D(double xValue, double yValue, double zValue)
	: x(static_cast<float>(xValue)), y(static_cast<float>(yValue)), z(static_cast<float>(zValue)) {
}

runtime::Ref<Point3D> Point3D::create(double xValue, double yValue, double zValue) {
	return runtime::makeRef<Point3D>(xValue, yValue, zValue);
}

bool Point3D::equals(const Point3D& o) const {
	AION_UNPORTED();
}

int32_t Point3D::hashCode() const {
	AION_UNPORTED();
}

runtime::Ref<Point3D> Point3D::clone() {
	AION_UNPORTED();
}

std::string Point3D::toString() {
	AION_UNPORTED();
}

Point3D::~Point3D() = default;

} // namespace aion::gameserver::model::geometry
