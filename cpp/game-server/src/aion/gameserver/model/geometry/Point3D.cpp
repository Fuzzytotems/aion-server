#include "aion/gameserver/model/geometry/Point3D.h"

#include "aion/gameserver/model/templates/zone/Point2D.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"

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
	if (this == &o)
		return true;
	return x.get() == o.x.get() && y.get() == o.y.get() && z.get() == o.z.get();
}

int32_t Point3D::hashCode() const {
	float result = x.get();
	result = 31 * result + y.get();
	result = 31 * result + z.get();
	return geoEngine::math::JavaFloat::doubleToInt(static_cast<double>(result * 100)); // Java (int) cast of a float
}

runtime::Ref<Point3D> Point3D::clone() {
	return create(*this);
}

std::string Point3D::toString() {
	std::string sb;
	sb += "Point3D";
	sb += "{x=" + geoEngine::math::JavaFloat::toString(x.get());
	sb += ", y=" + geoEngine::math::JavaFloat::toString(y.get());
	sb += ", z=" + geoEngine::math::JavaFloat::toString(z.get());
	sb += '}';
	return sb;
}

Point3D::~Point3D() = default;

} // namespace aion::gameserver::model::geometry
