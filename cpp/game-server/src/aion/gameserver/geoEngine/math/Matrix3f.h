#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include "aion/gameserver/geoEngine/math/Vector3f.h"

namespace aion::gameserver::geoEngine::math {

class Matrix4f;

/**
 * Java: com.aionemu.gameserver.geoEngine.math.Matrix3f (jMonkeyEngine) - a 3x3 matrix, m[row][col], vectors multiplied on the right.
 * <p>
 * Value type (trivially copyable). A new matrix is the identity. Store parameters are references (the Java null store is the overload
 * without it), methods returning `this` return Matrix3f&amp;, and argument aliasing (e.g. `a.mult(b, a)`) behaves like Java. The elements are
 * protected as in Java (Matrix4f reads them directly, like the Java package access).
 * <p>
 * Java arrays become spans with Java's length checks: `float[]` → std::span, `float[][]` → a 3x3 std::array. Not ported: fillFloatBuffer
 * (java.nio), getClassTag and the unused package-private equalIdentity. Matrix3f.set(null)/setColumn(i, null)/multLocal(null) have no
 * counterpart (references cannot be null).
 *
 * @author Mark Powell
 * @author Joshua Slack
 */
class Matrix3f {
public:
	/** The identity matrix. */
	constexpr Matrix3f() noexcept = default;
	constexpr Matrix3f(float v00, float v01, float v02, float v10, float v11, float v12, float v20, float v21, float v22) noexcept
	    : m00(v00), m01(v01), m02(v02), m10(v10), m11(v11), m12(v12), m20(v20), m21(v21), m22(v22) {}

	/** Takes the absolute value (FastMath.abs, so -0.0f stays -0.0f) of all matrix fields locally. */
	void absoluteLocal() noexcept;

	/** Java: set(Matrix3f) - copies the given matrix */
	Matrix3f& set(const Matrix3f& matrix) noexcept;

	/**
	 * @return the value at (i, j)
	 * @throws IllegalArgumentException (after logging a warning) if the position is invalid
	 */
	float get(int32_t i, int32_t j) const;

	/**
	 * Returns the matrix in row-major or column-major order into an array of 9 or 16 floats (only the upper 3x3 of a 16 element array).
	 * @throws IndexOutOfBoundsException if the array size is neither 9 nor 16
	 */
	void get(std::span<float> data, bool rowMajor) const;

	/** @throws IllegalArgumentException (after logging a warning) if i is not 0, 1 or 2 */
	Vector3f getColumn(int32_t i) const;
	Vector3f& getColumn(int32_t i, Vector3f& store) const;
	/** @throws IllegalArgumentException (after logging a warning) if i is not 0, 1 or 2 */
	Vector3f getRow(int32_t i) const;
	Vector3f& getRow(int32_t i, Vector3f& store) const;

	/** @throws IllegalArgumentException (after logging a warning) if i is not 0, 1 or 2 */
	Matrix3f& setColumn(int32_t i, const Vector3f& column);
	/** @throws IllegalArgumentException (after logging a warning) if i is not 0, 1 or 2 */
	Matrix3f& setRow(int32_t i, const Vector3f& row);

	/** @throws IllegalArgumentException (after logging a warning) if the position is invalid */
	Matrix3f& set(int32_t i, int32_t j, float value);

	/** Java: set(float[][]) (the size check is done by the type) */
	Matrix3f& set(const std::array<std::array<float, 3>, 3>& matrix) noexcept;

	/** Recreates the matrix using the provided axes as columns. */
	void fromAxes(const Vector3f& uAxis, const Vector3f& vAxis, const Vector3f& wAxis) noexcept;

	/**
	 * Sets the values from an array of 9 floats.
	 * @throws IllegalArgumentException if the array does not have 9 elements
	 */
	Matrix3f& set(std::span<const float> matrix, bool rowMajor = true);

	void loadIdentity() noexcept;
	bool isIdentity() const noexcept;

	/** Rotation of angle radians around the (normalized copy of the) axis. */
	void fromAngleAxis(float angle, const Vector3f& axis) noexcept;
	/** Rotation of angle radians around the normalized axis. */
	void fromAngleNormalAxis(float angle, const Vector3f& axis) noexcept;

	/** this * mat as a new matrix */
	Matrix3f mult(const Matrix3f& mat) const noexcept;
	/** this * mat stored in product (product may be this or mat) */
	Matrix3f& mult(const Matrix3f& mat, Matrix3f& product) const noexcept;
	/** this * vec as a new vector */
	Vector3f mult(const Vector3f& vec) const noexcept;
	/** this * vec stored in product (product may be vec) */
	Vector3f& mult(const Vector3f& vec, Vector3f& product) const noexcept;
	/** Multiplies every element by scale. */
	Matrix3f& multLocal(float scale) noexcept;
	/** vec = this * vec */
	Vector3f& multLocal(Vector3f& vec) const noexcept;
	/** this = this * mat */
	Matrix3f& multLocal(const Matrix3f& mat) noexcept;

	Matrix3f& transposeLocal() noexcept;

	/** The inverse as a new matrix (the zero matrix if |determinant| <= FLT_EPSILON). */
	Matrix3f invert() const noexcept;
	/** The inverse stored in store (zero if |determinant| <= FLT_EPSILON). Like Java, store must not be this. */
	Matrix3f& invert(Matrix3f& store) const noexcept;
	/** Inverts this matrix (zero if |determinant| <= FLT_EPSILON). */
	Matrix3f& invertLocal() noexcept;

	Matrix3f adjoint() const noexcept;
	/** Like Java, store must not be this. */
	Matrix3f& adjoint(Matrix3f& store) const noexcept;

	float determinant() const noexcept;

	Matrix3f& zero() noexcept;

	/** Adds the values of the parameter matrix to this matrix (deprecated in jME). */
	void add(const Matrix3f& mat) noexcept;

	/** Transposes this matrix locally (kept for jME compatibility). */
	Matrix3f& transpose() noexcept { return transposeLocal(); }
	Matrix3f transposeNew() const noexcept;

	/** "Matrix3f\n[\n m00  m01  m02 \n ... \n]" with Java's Float.toString formatting */
	std::string toString() const;

	int32_t hashCode() const noexcept;
	/** Java: equals(Object) - Float.compare of each element */
	bool equals(const Matrix3f& o) const noexcept;
	bool operator==(const Matrix3f& o) const noexcept { return equals(o); }

	/** Rotation matrix that rotates the normalized non-zero vector start into the normalized non-zero vector end (Möller-Hughes). */
	void fromStartEndVectors(const Vector3f& start, const Vector3f& end) noexcept;

	/** Scales the columns by the components of scale. */
	void scale(const Vector3f& scaleVector) noexcept;

	constexpr Matrix3f clone() const noexcept { return *this; }

protected:
	friend class Matrix4f;

	float m00 = 1.0f, m01 = 0.0f, m02 = 0.0f;
	float m10 = 0.0f, m11 = 1.0f, m12 = 0.0f;
	float m20 = 0.0f, m21 = 0.0f, m22 = 1.0f;
};

} // namespace aion::gameserver::geoEngine::math
