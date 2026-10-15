#include "aion/gameserver/geoEngine/math/Vector2f.h"

#include "aion/gameserver/geoEngine/math/FastMath.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/geoEngine/math/StrictFp.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"

namespace aion::gameserver::geoEngine::math {

float Vector2f::dot(const Vector2f& vec) const noexcept {
	return x * vec.x + y * vec.y;
}

Vector3f Vector2f::cross(const Vector2f& v) const noexcept {
	return Vector3f(0, 0, determinant(v));
}

float Vector2f::determinant(const Vector2f& v) const noexcept {
	return (x * v.y) - (y * v.x);
}

Vector2f& Vector2f::interpolate(const Vector2f& finalVec, float changeAmnt) noexcept {
	this->x = (1 - changeAmnt) * this->x + changeAmnt * finalVec.x;
	this->y = (1 - changeAmnt) * this->y + changeAmnt * finalVec.y;
	return *this;
}

Vector2f& Vector2f::interpolate(const Vector2f& beginVec, const Vector2f& finalVec, float changeAmnt) noexcept {
	this->x = (1 - changeAmnt) * beginVec.x + changeAmnt * finalVec.x;
	this->y = (1 - changeAmnt) * beginVec.y + changeAmnt * finalVec.y;
	return *this;
}

bool Vector2f::isValidVector(const Vector2f* vector) noexcept {
	if (vector == nullptr)
		return false;
	if (JavaFloat::isNaN(vector->x) || JavaFloat::isNaN(vector->y))
		return false;
	if (JavaFloat::isInfinite(vector->x) || JavaFloat::isInfinite(vector->y))
		return false;
	return true;
}

float Vector2f::length() const noexcept {
	return FastMath::sqrt(lengthSquared());
}

float Vector2f::lengthSquared() const noexcept {
	return x * x + y * y;
}

float Vector2f::distanceSquared(const Vector2f& v) const noexcept {
	const double dx = x - v.x;
	const double dy = y - v.y;
	return static_cast<float>(dx * dx + dy * dy);
}

float Vector2f::distanceSquared(float otherX, float otherY) const noexcept {
	const double dx = x - otherX;
	const double dy = y - otherY;
	return static_cast<float>(dx * dx + dy * dy);
}

float Vector2f::distance(const Vector2f& v) const noexcept {
	return FastMath::sqrt(distanceSquared(v));
}

Vector2f Vector2f::normalize() const noexcept {
	const float len = length();
	if (len != 0)
		return divide(len);
	return divide(1);
}

Vector2f& Vector2f::normalizeLocal() noexcept {
	const float len = length();
	if (len != 0)
		return divideLocal(len);
	return divideLocal(1);
}

float Vector2f::smallestAngleBetween(const Vector2f& otherVector) const noexcept {
	const float dotProduct = dot(otherVector);
	const float angle = FastMath::acos(dotProduct);
	return angle;
}

float Vector2f::angleBetween(const Vector2f& otherVector) const noexcept {
	const float angle = FastMath::atan2(otherVector.y, otherVector.x) - FastMath::atan2(y, x);
	return angle;
}

float Vector2f::getAngle() const noexcept {
	return -FastMath::atan2(y, x);
}

int32_t Vector2f::hashCode() const noexcept {
	// Java int arithmetic wraps: computed in uint32_t
	uint32_t hash = 37;
	hash += 37 * hash + static_cast<uint32_t>(JavaFloat::floatToIntBits(x));
	hash += 37 * hash + static_cast<uint32_t>(JavaFloat::floatToIntBits(y));
	return static_cast<int32_t>(hash);
}

bool Vector2f::equals(const Vector2f& o) const noexcept {
	if (this == &o)
		return true;
	if (JavaFloat::compare(x, o.x) != 0)
		return false;
	if (JavaFloat::compare(y, o.y) != 0)
		return false;
	return true;
}

std::string Vector2f::toString() const {
	return "(" + JavaFloat::toString(x) + ", " + JavaFloat::toString(y) + ")";
}

void Vector2f::rotateAroundOrigin(float angle, bool cw) noexcept {
	if (cw)
		angle = -angle;
	const float newX = FastMath::cos(angle) * x - FastMath::sin(angle) * y;
	const float newY = FastMath::sin(angle) * x + FastMath::cos(angle) * y;
	x = newX;
	y = newY;
}

} // namespace aion::gameserver::geoEngine::math
