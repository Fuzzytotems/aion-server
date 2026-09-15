#include "aion/gameserver/model/geometry/Plane3D.h"

namespace aion::gameserver::model::geometry {

using geoEngine::math::Vector3f;

Plane3D::Plane3D(const Vector3f& p1, const Vector3f& p2, const Vector3f& p3) : pointOnPlane(p1), normal(normalOf(p1, p2, p3)) {
}

runtime::Ref<Plane3D> Plane3D::create(const Vector3f& p1, const Vector3f& p2, const Vector3f& p3) {
	return runtime::makeRef<Plane3D>(p1, p2, p3);
}

Vector3f Plane3D::normalOf(const Vector3f& p1, const Vector3f& p2, const Vector3f& p3) noexcept {
	Vector3f vector1 = p2.subtract(p1);
	Vector3f vector2 = p3.subtract(p1);
	return vector1.cross(vector2);
}

std::optional<Vector3f> Plane3D::intersection(const Vector3f& rayStart, const Vector3f& rayEnd) const {
	Vector3f rayDirection = rayEnd.subtract(rayStart);
	float dotProduct = normal.dot(rayDirection);
	if (dotProduct == 0) // ray is parallel to the plane
		return std::nullopt;
	float distance = normal.dot(pointOnPlane.subtract(rayStart)) / dotProduct;
	if (distance < 0 || distance > 1) // intersection point is outside the range of the ray
		return std::nullopt;
	return rayStart.add(rayDirection.multLocal(distance));
}

Plane3D::~Plane3D() = default;

} // namespace aion::gameserver::model::geometry
