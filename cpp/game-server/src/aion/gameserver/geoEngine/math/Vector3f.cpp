#include "aion/gameserver/geoEngine/math/Vector3f.h"

#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/geoEngine/math/FastMath.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/geoEngine/math/StrictFp.h"

namespace aion::gameserver::geoEngine::math {

Vector3f& Vector3f::scaleAdd(float scalar, const Vector3f& addend) noexcept {
	x = x * scalar + addend.x;
	y = y * scalar + addend.y;
	z = z * scalar + addend.z;
	return *this;
}

Vector3f& Vector3f::scaleAdd(float scalar, const Vector3f& multiplier, const Vector3f& addend) noexcept {
	this->x = multiplier.x * scalar + addend.x;
	this->y = multiplier.y * scalar + addend.y;
	this->z = multiplier.z * scalar + addend.z;
	return *this;
}

float Vector3f::dot(const Vector3f& vec) const noexcept {
	return x * vec.x + y * vec.y + z * vec.z;
}

Vector3f Vector3f::cross(const Vector3f& v) const noexcept {
	Vector3f result;
	cross(v, result);
	return result;
}

Vector3f& Vector3f::cross(const Vector3f& v, Vector3f& result) const noexcept {
	return cross(v.x, v.y, v.z, result);
}

Vector3f& Vector3f::cross(float otherX, float otherY, float otherZ, Vector3f& result) const noexcept {
	const float resX = ((y * otherZ) - (z * otherY));
	const float resY = ((z * otherX) - (x * otherZ));
	const float resZ = ((x * otherY) - (y * otherX));
	result.set(resX, resY, resZ);
	return result;
}

Vector3f& Vector3f::crossLocal(const Vector3f& v) noexcept {
	return crossLocal(v.x, v.y, v.z);
}

Vector3f& Vector3f::crossLocal(float otherX, float otherY, float otherZ) noexcept {
	const float tempx = (y * otherZ) - (z * otherY);
	const float tempy = (z * otherX) - (x * otherZ);
	z = (x * otherY) - (y * otherX);
	x = tempx;
	y = tempy;
	return *this;
}

Vector3f Vector3f::project(const Vector3f& other) const noexcept {
	const float n = this->dot(other);      // A . B
	const float d = other.lengthSquared(); // |B|^2
	return Vector3f(other).normalizeLocal().multLocal(n / d);
}

float Vector3f::length() const noexcept {
	return FastMath::sqrt(lengthSquared());
}

float Vector3f::lengthSquared() const noexcept {
	return x * x + y * y + z * z;
}

float Vector3f::distanceSquared(const Vector3f& v) const noexcept {
	const double dx = x - v.x;
	const double dy = y - v.y;
	const double dz = z - v.z;
	return static_cast<float>(dx * dx + dy * dy + dz * dz);
}

float Vector3f::distance(const Vector3f& v) const noexcept {
	return FastMath::sqrt(distanceSquared(v));
}

Vector3f Vector3f::normalize() const noexcept {
	float length = x * x + y * y + z * z;
	if (length != 1.0f && length != 0.0f) {
		length = 1.0f / FastMath::sqrt(length);
		return Vector3f(x * length, y * length, z * length);
	}
	return clone();
}

Vector3f& Vector3f::normalizeLocal() noexcept {
	float length = x * x + y * y + z * z;
	if (length != 1.0f && length != 0.0f) {
		length = 1.0f / FastMath::sqrt(length);
		x *= length;
		y *= length;
		z *= length;
	}
	return *this;
}

void Vector3f::maxLocal(const Vector3f& other) noexcept {
	x = other.x > x ? other.x : x;
	y = other.y > y ? other.y : y;
	z = other.z > z ? other.z : z;
}

void Vector3f::minLocal(const Vector3f& other) noexcept {
	x = other.x < x ? other.x : x;
	y = other.y < y ? other.y : y;
	z = other.z < z ? other.z : z;
}

float Vector3f::angleBetween(const Vector3f& otherVector) const noexcept {
	const float dotProduct = dot(otherVector);
	const float angle = FastMath::acos(dotProduct);
	return angle;
}

Vector3f& Vector3f::interpolate(const Vector3f& finalVec, float changeAmnt) noexcept {
	this->x = (1 - changeAmnt) * this->x + changeAmnt * finalVec.x;
	this->y = (1 - changeAmnt) * this->y + changeAmnt * finalVec.y;
	this->z = (1 - changeAmnt) * this->z + changeAmnt * finalVec.z;
	return *this;
}

Vector3f& Vector3f::interpolate(const Vector3f& beginVec, const Vector3f& finalVec, float changeAmnt) noexcept {
	this->x = (1 - changeAmnt) * beginVec.x + changeAmnt * finalVec.x;
	this->y = (1 - changeAmnt) * beginVec.y + changeAmnt * finalVec.y;
	this->z = (1 - changeAmnt) * beginVec.z + changeAmnt * finalVec.z;
	return *this;
}

bool Vector3f::isValidVector(const Vector3f* vector) noexcept {
	if (vector == nullptr)
		return false;
	if (JavaFloat::isNaN(vector->x) || JavaFloat::isNaN(vector->y) || JavaFloat::isNaN(vector->z))
		return false;
	if (JavaFloat::isInfinite(vector->x) || JavaFloat::isInfinite(vector->y) || JavaFloat::isInfinite(vector->z))
		return false;
	return true;
}

void Vector3f::generateOrthonormalBasis(Vector3f& u, Vector3f& v, Vector3f& w) noexcept {
	w.normalizeLocal();
	generateComplementBasis(u, v, w);
}

void Vector3f::generateComplementBasis(Vector3f& u, Vector3f& v, const Vector3f& w) noexcept {
	float fInvLength;

	if (FastMath::abs(w.x) >= FastMath::abs(w.y)) {
		// w.x or w.z is the largest magnitude component, swap them
		fInvLength = FastMath::invSqrt(w.x * w.x + w.z * w.z);
		u.x = -w.z * fInvLength;
		u.y = 0.0f;
		u.z = +w.x * fInvLength;
		v.x = w.y * u.z;
		v.y = w.z * u.x - w.x * u.z;
		v.z = -w.y * u.x;
	} else {
		// w.y or w.z is the largest magnitude component, swap them
		fInvLength = FastMath::invSqrt(w.y * w.y + w.z * w.z);
		u.x = 0.0f;
		u.y = +w.z * fInvLength;
		u.z = -w.y * fInvLength;
		v.x = w.y * u.z - w.z * u.y;
		v.y = -w.x * u.z;
		v.z = w.x * u.y;
	}
}

bool Vector3f::equals(const Vector3f& o) const noexcept {
	if (this == &o)
		return true;
	if (JavaFloat::compare(x, o.x) != 0)
		return false;
	if (JavaFloat::compare(y, o.y) != 0)
		return false;
	if (JavaFloat::compare(z, o.z) != 0)
		return false;
	return true;
}

int32_t Vector3f::hashCode() const noexcept {
	// Java int arithmetic wraps: computed in uint32_t
	uint32_t hash = 37;
	hash += 37 * hash + static_cast<uint32_t>(JavaFloat::floatToIntBits(x));
	hash += 37 * hash + static_cast<uint32_t>(JavaFloat::floatToIntBits(y));
	hash += 37 * hash + static_cast<uint32_t>(JavaFloat::floatToIntBits(z));
	return static_cast<int32_t>(hash);
}

std::string Vector3f::toString() const {
	return "(" + JavaFloat::toString(x) + ", " + JavaFloat::toString(y) + ", " + JavaFloat::toString(z) + ")";
}

float Vector3f::get(int32_t index) const {
	switch (index) {
		case 0:
			return x;
		case 1:
			return y;
		case 2:
			return z;
	}
	throw commons::utils::IllegalArgumentException("index must be either 0, 1 or 2");
}

void Vector3f::set(int32_t index, float value) {
	switch (index) {
		case 0:
			x = value;
			return;
		case 1:
			y = value;
			return;
		case 2:
			z = value;
			return;
	}
	throw commons::utils::IllegalArgumentException("index must be either 0, 1 or 2");
}

} // namespace aion::gameserver::geoEngine::math
