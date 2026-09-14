#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace aion::gameserver::geoEngine::math {

class Vector3f;

/**
 * Java: com.aionemu.gameserver.geoEngine.math.Vector2f (jMonkeyEngine) - a vector of two floats (used by GeoMap.findMovementCollision).
 * <p>
 * Same porting rules as Vector3f (value type, store parameters by reference, chains return Vector2f&amp;, Java equals/hashCode). The Java
 * null-argument checks (log a warning and return null) have no C++ counterpart since arguments are references. Not ported:
 * readExternal/writeExternal (Java serialization) and getClassTag.
 *
 * @author Mark Powell
 * @author Joshua Slack
 */
class Vector2f {
public:
	static const Vector2f ZERO;
	static const Vector2f UNIT_XY;

	/** the x value of the vector. */
	float x = 0.0f;
	/** the y value of the vector. */
	float y = 0.0f;

	/** Creates a Vector2f with x and y set to 0. */
	constexpr Vector2f() noexcept = default;
	constexpr Vector2f(float newX, float newY) noexcept : x(newX), y(newY) {}

	constexpr Vector2f& set(float newX, float newY) noexcept {
		x = newX;
		y = newY;
		return *this;
	}
	constexpr Vector2f& set(const Vector2f& vec) noexcept {
		x = vec.x;
		y = vec.y;
		return *this;
	}

	constexpr Vector2f add(const Vector2f& vec) const noexcept { return Vector2f(x + vec.x, y + vec.y); }
	constexpr Vector2f& addLocal(const Vector2f& vec) noexcept {
		x += vec.x;
		y += vec.y;
		return *this;
	}
	constexpr Vector2f& addLocal(float addX, float addY) noexcept {
		x += addX;
		y += addY;
		return *this;
	}
	constexpr Vector2f& add(const Vector2f& vec, Vector2f& result) const noexcept {
		result.x = x + vec.x;
		result.y = y + vec.y;
		return result;
	}

	float dot(const Vector2f& vec) const noexcept;
	/** Returns (0, 0, determinant(v)). */
	Vector3f cross(const Vector2f& v) const noexcept;
	float determinant(const Vector2f& v) const noexcept;

	/** this = (1 - changeAmnt) * this + changeAmnt * finalVec */
	Vector2f& interpolate(const Vector2f& finalVec, float changeAmnt) noexcept;
	/** this = (1 - changeAmnt) * beginVec + changeAmnt * finalVec */
	Vector2f& interpolate(const Vector2f& beginVec, const Vector2f& finalVec, float changeAmnt) noexcept;

	/** False if the vector is null or has a NaN or infinite component. */
	static bool isValidVector(const Vector2f* vector) noexcept;
	static bool isValidVector(const Vector2f& vector) noexcept { return isValidVector(&vector); }

	float length() const noexcept;
	float lengthSquared() const noexcept;
	/** Evaluated in double from the float differences, cast to float (as in Java). */
	float distanceSquared(const Vector2f& v) const noexcept;
	float distanceSquared(float otherX, float otherY) const noexcept;
	float distance(const Vector2f& v) const noexcept;

	constexpr Vector2f mult(float scalar) const noexcept { return Vector2f(x * scalar, y * scalar); }
	constexpr Vector2f& multLocal(float scalar) noexcept {
		x *= scalar;
		y *= scalar;
		return *this;
	}
	constexpr Vector2f& multLocal(const Vector2f& vec) noexcept {
		x *= vec.x;
		y *= vec.y;
		return *this;
	}
	constexpr Vector2f& mult(float scalar, Vector2f& product) const noexcept {
		product.x = x * scalar;
		product.y = y * scalar;
		return product;
	}

	/** Divides each component (unlike Vector3f.divide, which multiplies by the reciprocal). */
	constexpr Vector2f divide(float scalar) const noexcept { return Vector2f(x / scalar, y / scalar); }
	constexpr Vector2f& divideLocal(float scalar) noexcept {
		x /= scalar;
		y /= scalar;
		return *this;
	}

	constexpr Vector2f negate() const noexcept { return Vector2f(-x, -y); }
	constexpr Vector2f& negateLocal() noexcept {
		x = -x;
		y = -y;
		return *this;
	}

	constexpr Vector2f subtract(const Vector2f& vec) const noexcept { return Vector2f(x - vec.x, y - vec.y); }
	constexpr Vector2f& subtract(const Vector2f& vec, Vector2f& store) const noexcept {
		store.x = x - vec.x;
		store.y = y - vec.y;
		return store;
	}
	constexpr Vector2f subtract(float valX, float valY) const noexcept { return Vector2f(x - valX, y - valY); }
	constexpr Vector2f& subtractLocal(const Vector2f& vec) noexcept {
		x -= vec.x;
		y -= vec.y;
		return *this;
	}
	constexpr Vector2f& subtractLocal(float valX, float valY) noexcept {
		x -= valX;
		y -= valY;
		return *this;
	}

	/** Returns this vector divided by its length (divided by 1 if the length is 0). */
	Vector2f normalize() const noexcept;
	/** Divides this vector by its length (by 1 if the length is 0). */
	Vector2f& normalizeLocal() noexcept;

	/** FastMath.acos of the dot product (both vectors assumed to be unit vectors). */
	float smallestAngleBetween(const Vector2f& otherVector) const noexcept;
	/** atan2(other.y, other.x) - atan2(y, x) */
	float angleBetween(const Vector2f& otherVector) const noexcept;

	constexpr float getX() const noexcept { return x; }
	constexpr Vector2f& setX(float newX) noexcept {
		x = newX;
		return *this;
	}
	constexpr float getY() const noexcept { return y; }
	constexpr Vector2f& setY(float newY) noexcept {
		y = newY;
		return *this;
	}

	/** -atan2(y, x) */
	float getAngle() const noexcept;

	constexpr Vector2f& zero() noexcept {
		x = y = 0;
		return *this;
	}

	int32_t hashCode() const noexcept;

	constexpr Vector2f clone() const noexcept { return *this; }

	/** Java: toArray(null) */
	constexpr std::array<float, 2> toArray() const noexcept { return {x, y}; }

	/** Java: equals(Object) - Float.compare of each component */
	bool equals(const Vector2f& o) const noexcept;
	bool operator==(const Vector2f& o) const noexcept { return equals(o); }

	/** "(x, y)" with Java's Float.toString formatting */
	std::string toString() const;

	/** Rotates around the origin by angle radians (clockwise if cw). */
	void rotateAroundOrigin(float angle, bool cw) noexcept;
};

inline constexpr Vector2f Vector2f::ZERO{0.0f, 0.0f};
inline constexpr Vector2f Vector2f::UNIT_XY{1.0f, 1.0f};

} // namespace aion::gameserver::geoEngine::math

/** std::hash via Java's hashCode, consistent with operator== (Java equals). */
template <>
struct std::hash<aion::gameserver::geoEngine::math::Vector2f> {
	size_t operator()(const aion::gameserver::geoEngine::math::Vector2f& v) const noexcept { return static_cast<size_t>(v.hashCode()); }
};
