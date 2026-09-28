#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>

namespace aion::gameserver::geoEngine::math {

/**
 * Java: com.aionemu.gameserver.geoEngine.math.Vector3f (jMonkeyEngine) - a vector of three floats, the general 3D point/vector type of the
 * game server (geo engine, effects, AI, observers, SM_PLAYER_INFO).
 * <p>
 * Porting notes
 * - Value type (trivially copyable, usable in Field&lt;Vector3f&gt;). Java `new Vector3f(v)` and `v.clone()` are copies; a Java field or
 *   local that aliases another Vector3f object must stay a reference (Vector3f&amp;) where the aliasing matters.
 * - Methods with a Java `store`/`result` parameter take a reference and return it; the Java `null` store (allocate a new vector) is the
 *   overload without that parameter. Methods returning `this` return Vector3f&amp;: `Vector3f v = Vector3f(a).multLocal(t).addLocal(b);`
 *   copies before the temporary dies, but never bind `auto&amp;` to a chain that starts with a temporary.
 * - Argument aliasing behaves like Java (e.g. `a.cross(b, a)` or `m.mult(v, v)`), since the bodies are transliterated statement by statement.
 * - NAN is named NOT_A_NUMBER (NAN is a &lt;cmath&gt; macro).
 * - equals/operator== and hashCode follow Java (Float.compare: NaN equals NaN, 0.0f differs from -0.0f).
 *
 * @author Mark Powell
 * @author Joshua Slack
 */
class Vector3f {
public:
	static const Vector3f ZERO;
	/** Java: NAN */
	static const Vector3f NOT_A_NUMBER;
	static const Vector3f UNIT_X;
	static const Vector3f UNIT_Y;
	static const Vector3f UNIT_Z;
	static const Vector3f UNIT_XYZ;
	static const Vector3f POSITIVE_INFINITY;
	static const Vector3f NEGATIVE_INFINITY;

	/** the x value of the vector. */
	float x = 0.0f;
	/** the y value of the vector. */
	float y = 0.0f;
	/** the z value of the vector. */
	float z = 0.0f;

	/** Instantiates a new Vector3f with default values of (0,0,0). */
	constexpr Vector3f() noexcept = default;
	constexpr Vector3f(float newX, float newY, float newZ) noexcept : x(newX), y(newY), z(newZ) {}

	constexpr Vector3f& set(float newX, float newY, float newZ) noexcept {
		x = newX;
		y = newY;
		z = newZ;
		return *this;
	}
	constexpr Vector3f& set(const Vector3f& vect) noexcept {
		x = vect.x;
		y = vect.y;
		z = vect.z;
		return *this;
	}

	constexpr Vector3f add(const Vector3f& vec) const noexcept { return Vector3f(x + vec.x, y + vec.y, z + vec.z); }
	constexpr Vector3f& add(const Vector3f& vec, Vector3f& result) const noexcept {
		result.x = x + vec.x;
		result.y = y + vec.y;
		result.z = z + vec.z;
		return result;
	}
	constexpr Vector3f& addLocal(const Vector3f& vec) noexcept {
		x += vec.x;
		y += vec.y;
		z += vec.z;
		return *this;
	}
	constexpr Vector3f add(float addX, float addY, float addZ) const noexcept { return Vector3f(x + addX, y + addY, z + addZ); }
	constexpr Vector3f& addLocal(float addX, float addY, float addZ) noexcept {
		x += addX;
		y += addY;
		z += addZ;
		return *this;
	}

	/** Multiplies this vector by a scalar then adds the given Vector3f. */
	Vector3f& scaleAdd(float scalar, const Vector3f& addend) noexcept;
	/** Sets this vector to multiplier * scalar + addend. */
	Vector3f& scaleAdd(float scalar, const Vector3f& multiplier, const Vector3f& addend) noexcept;

	float dot(const Vector3f& vec) const noexcept;
	Vector3f cross(const Vector3f& v) const noexcept;
	Vector3f& cross(const Vector3f& v, Vector3f& result) const noexcept;
	Vector3f& cross(float otherX, float otherY, float otherZ, Vector3f& result) const noexcept;
	Vector3f& crossLocal(const Vector3f& v) noexcept;
	Vector3f& crossLocal(float otherX, float otherY, float otherZ) noexcept;
	Vector3f project(const Vector3f& other) const noexcept;

	float length() const noexcept;
	float lengthSquared() const noexcept;
	/** Evaluated in double from the float differences, cast to float (as in Java). */
	float distanceSquared(const Vector3f& v) const noexcept;
	float distance(const Vector3f& v) const noexcept;

	constexpr Vector3f mult(float scalar) const noexcept { return Vector3f(x * scalar, y * scalar, z * scalar); }
	constexpr Vector3f& mult(float scalar, Vector3f& product) const noexcept {
		product.x = x * scalar;
		product.y = y * scalar;
		product.z = z * scalar;
		return product;
	}
	constexpr Vector3f& multLocal(float scalar) noexcept {
		x *= scalar;
		y *= scalar;
		z *= scalar;
		return *this;
	}
	constexpr Vector3f& multLocal(const Vector3f& vec) noexcept {
		x *= vec.x;
		y *= vec.y;
		z *= vec.z;
		return *this;
	}
	constexpr Vector3f& multLocal(float factorX, float factorY, float factorZ) noexcept {
		x *= factorX;
		y *= factorY;
		z *= factorZ;
		return *this;
	}
	constexpr Vector3f mult(const Vector3f& vec) const noexcept { return Vector3f(x * vec.x, y * vec.y, z * vec.z); }
	constexpr Vector3f& mult(const Vector3f& vec, Vector3f& store) const noexcept { return store.set(x * vec.x, y * vec.y, z * vec.z); }

	/** Multiplies by 1 / scalar (not a division per component, as in Java). */
	constexpr Vector3f divide(float scalar) const noexcept {
		scalar = 1.0f / scalar;
		return Vector3f(x * scalar, y * scalar, z * scalar);
	}
	/** Multiplies by 1 / scalar (not a division per component, as in Java). */
	constexpr Vector3f& divideLocal(float scalar) noexcept {
		scalar = 1.0f / scalar;
		x *= scalar;
		y *= scalar;
		z *= scalar;
		return *this;
	}
	constexpr Vector3f divide(const Vector3f& scalar) const noexcept { return Vector3f(x / scalar.x, y / scalar.y, z / scalar.z); }
	constexpr Vector3f& divideLocal(const Vector3f& scalar) noexcept {
		x /= scalar.x;
		y /= scalar.y;
		z /= scalar.z;
		return *this;
	}

	constexpr Vector3f negate() const noexcept { return Vector3f(-x, -y, -z); }
	constexpr Vector3f& negateLocal() noexcept {
		x = -x;
		y = -y;
		z = -z;
		return *this;
	}

	constexpr Vector3f subtract(const Vector3f& vec) const noexcept { return Vector3f(x - vec.x, y - vec.y, z - vec.z); }
	constexpr Vector3f& subtractLocal(const Vector3f& vec) noexcept {
		x -= vec.x;
		y -= vec.y;
		z -= vec.z;
		return *this;
	}
	constexpr Vector3f& subtract(const Vector3f& vec, Vector3f& result) const noexcept {
		result.x = x - vec.x;
		result.y = y - vec.y;
		result.z = z - vec.z;
		return result;
	}
	constexpr Vector3f subtract(float subtractX, float subtractY, float subtractZ) const noexcept {
		return Vector3f(x - subtractX, y - subtractY, z - subtractZ);
	}
	constexpr Vector3f& subtractLocal(float subtractX, float subtractY, float subtractZ) noexcept {
		x -= subtractX;
		y -= subtractY;
		z -= subtractZ;
		return *this;
	}

	/** Returns the unit vector of this vector (a copy if the squared length is 0 or 1, NaN components stay NaN). */
	Vector3f normalize() const noexcept;
	/** Makes this vector a unit vector (unchanged if the squared length is 0 or 1). */
	Vector3f& normalizeLocal() noexcept;

	/** Component-wise maximum stored in this vector (Java: void). */
	void maxLocal(const Vector3f& other) noexcept;
	/** Component-wise minimum stored in this vector (Java: void). */
	void minLocal(const Vector3f& other) noexcept;

	constexpr Vector3f& zero() noexcept {
		x = y = z = 0;
		return *this;
	}

	/** The angle in radians between this and the given vector, both assumed to be unit vectors (FastMath.acos of the dot product). */
	float angleBetween(const Vector3f& otherVector) const noexcept;

	/** this = (1 - changeAmnt) * this + changeAmnt * finalVec */
	Vector3f& interpolate(const Vector3f& finalVec, float changeAmnt) noexcept;
	/** this = (1 - changeAmnt) * beginVec + changeAmnt * finalVec */
	Vector3f& interpolate(const Vector3f& beginVec, const Vector3f& finalVec, float changeAmnt) noexcept;

	/** False if the vector is null or has a NaN or infinite component. */
	static bool isValidVector(const Vector3f* vector) noexcept;
	static bool isValidVector(const Vector3f& vector) noexcept { return isValidVector(&vector); }

	static void generateOrthonormalBasis(Vector3f& u, Vector3f& v, Vector3f& w) noexcept;
	static void generateComplementBasis(Vector3f& u, Vector3f& v, const Vector3f& w) noexcept;

	constexpr Vector3f clone() const noexcept { return *this; }

	/** Java: toArray(null) */
	constexpr std::array<float, 3> toArray() const noexcept { return {x, y, z}; }

	/** Java: equals(Object) - Float.compare of each component */
	bool equals(const Vector3f& o) const noexcept;
	bool operator==(const Vector3f& o) const noexcept { return equals(o); }

	int32_t hashCode() const noexcept;

	/** "(x, y, z)" with Java's Float.toString formatting */
	std::string toString() const;

	constexpr float getX() const noexcept { return x; }
	constexpr Vector3f& setX(float newX) noexcept {
		x = newX;
		return *this;
	}
	constexpr float getY() const noexcept { return y; }
	constexpr Vector3f& setY(float newY) noexcept {
		y = newY;
		return *this;
	}
	constexpr float getZ() const noexcept { return z; }
	constexpr Vector3f& setZ(float newZ) noexcept {
		z = newZ;
		return *this;
	}

	/**
	 * @return x value if index == 0, y value if index == 1 or z value if index == 2
	 * @throws IllegalArgumentException if index is not one of 0, 1, 2.
	 */
	float get(int32_t index) const;
	/** @throws IllegalArgumentException if index is not one of 0, 1, 2. */
	void set(int32_t index, float value);
};

inline constexpr Vector3f Vector3f::ZERO{0.0f, 0.0f, 0.0f};
inline constexpr Vector3f Vector3f::NOT_A_NUMBER{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(),
                                                 std::numeric_limits<float>::quiet_NaN()};
inline constexpr Vector3f Vector3f::UNIT_X{1.0f, 0.0f, 0.0f};
inline constexpr Vector3f Vector3f::UNIT_Y{0.0f, 1.0f, 0.0f};
inline constexpr Vector3f Vector3f::UNIT_Z{0.0f, 0.0f, 1.0f};
inline constexpr Vector3f Vector3f::UNIT_XYZ{1.0f, 1.0f, 1.0f};
inline constexpr Vector3f Vector3f::POSITIVE_INFINITY{std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(),
                                                      std::numeric_limits<float>::infinity()};
inline constexpr Vector3f Vector3f::NEGATIVE_INFINITY{-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
                                                      -std::numeric_limits<float>::infinity()};

} // namespace aion::gameserver::geoEngine::math

/** std::hash via Java's hashCode, consistent with operator== (Java equals). */
template <>
struct std::hash<aion::gameserver::geoEngine::math::Vector3f> {
	size_t operator()(const aion::gameserver::geoEngine::math::Vector3f& v) const noexcept { return static_cast<size_t>(v.hashCode()); }
};
