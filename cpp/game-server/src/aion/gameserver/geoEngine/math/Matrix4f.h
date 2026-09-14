#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/geoEngine/math/Matrix3f.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"

namespace aion::gameserver::geoEngine::math {

/**
 * Java: java.lang.ArithmeticException, thrown by Matrix4f.invert for a singular matrix. Declared here because the runtime's Java exception
 * types (runtime/base/Exceptions.h) do not have it and this leaf library does not depend on the runtime.
 */
class ArithmeticException : public commons::utils::Exception {
public:
	using Exception::Exception;
};

/**
 * Java: com.aionemu.gameserver.geoEngine.math.Matrix4f (jMonkeyEngine) - a 4x4 matrix, m[row][col], column vectors on the right, translation
 * in the rightmost column (m03, m13, m23). Geometry.setTransform builds world matrices with loadIdentity/setRotationMatrix/scale/
 * setTranslation; BIHNode inverts them per ray.
 * <p>
 * Value type (trivially copyable). A new matrix is the identity. Store parameters are references (the Java null store is the overload
 * without it), methods returning `this` return Matrix4f&amp;, and argument aliasing (e.g. `inv.mult(r.getOrigin(), r.getOrigin())`) behaves
 * like Java. Java arrays become spans with Java's length checks.
 * <p>
 * Not ported: FloatBuffer methods (java.nio), fromFrustum, set(float[][]), getClassTag and the unused package-private equalIdentity; the Java
 * null checks of copy/setColumn/multAcross/mult(float[]) have no counterpart (references cannot be null).
 *
 * @author Mark Powell
 * @author Joshua Slack
 */
class Matrix4f {
public:
	static const Matrix4f IDENTITY;

	float m00 = 1.0f, m01 = 0.0f, m02 = 0.0f, m03 = 0.0f;
	float m10 = 0.0f, m11 = 1.0f, m12 = 0.0f, m13 = 0.0f;
	float m20 = 0.0f, m21 = 0.0f, m22 = 1.0f, m23 = 0.0f;
	float m30 = 0.0f, m31 = 0.0f, m32 = 0.0f, m33 = 1.0f;

	/** The identity matrix. */
	constexpr Matrix4f() noexcept = default;
	constexpr Matrix4f(float v00, float v01, float v02, float v03, float v10, float v11, float v12, float v13, float v20, float v21, float v22,
	                   float v23, float v30, float v31, float v32, float v33) noexcept
	    : m00(v00), m01(v01), m02(v02), m03(v03), m10(v10), m11(v11), m12(v12), m13(v13), m20(v20), m21(v21), m22(v22), m23(v23), m30(v30), m31(v31),
	      m32(v32), m33(v33) {}

	/**
	 * Java: Matrix4f(float[]) - an array of 16 floats in column-major format.
	 * @throws IllegalArgumentException if the array does not have 16 elements
	 */
	explicit Matrix4f(std::span<const float> array);

	/** Java: copy(Matrix4f) */
	void copy(const Matrix4f& matrix) noexcept;

	/**
	 * Copies the values into an array of 16 floats.
	 * @throws IllegalArgumentException if the array does not have 16 elements
	 */
	void get(std::span<float> matrix, bool rowMajor = true) const;

	/** @throws IllegalArgumentException (after logging a warning) if the position is invalid */
	float get(int32_t i, int32_t j) const;

	/** @throws IllegalArgumentException (after logging a warning) if i is not 0..3 */
	std::array<float, 4> getColumn(int32_t i) const;
	/** @throws IllegalArgumentException (after logging a warning) if i is not 0..3 */
	std::span<float, 4> getColumn(int32_t i, std::span<float, 4> store) const;
	/** @throws IllegalArgumentException (after logging a warning) if i is not 0..3 */
	void setColumn(int32_t i, std::span<const float, 4> column);

	/** @throws IllegalArgumentException (after logging a warning) if the position is invalid */
	void set(int32_t i, int32_t j, float value);

	Matrix4f& set(const Matrix4f& matrix) noexcept;

	/**
	 * Sets the values from an array of 16 floats.
	 * @throws IllegalArgumentException if the array does not have 16 elements
	 */
	void set(std::span<const float> matrix, bool rowMajor = true);

	/** Returns the transposed matrix as a new matrix. */
	Matrix4f transpose() const noexcept;
	Matrix4f& transposeLocal() noexcept;

	void fillFloatArray(std::span<float, 16> f, bool columnMajor) const noexcept;

	void loadIdentity() noexcept;

	/** Rotation of angle radians around the (normalized copy of the) axis. */
	void fromAngleAxis(float angle, const Vector3f& axis) noexcept;
	/** Rotation of angle radians around the normalized axis (the rest of the matrix becomes identity). */
	void fromAngleNormalAxis(float angle, const Vector3f& axis) noexcept;

	/** Multiplies every element by scalar (Java: void). */
	void multLocal(float scalar) noexcept;
	Matrix4f mult(float scalar) const noexcept;
	Matrix4f& mult(float scalar, Matrix4f& store) const noexcept;

	/** this * in2 as a new matrix */
	Matrix4f mult(const Matrix4f& in2) const noexcept;
	/** this * in2 stored in store (store may be this or in2) */
	Matrix4f& mult(const Matrix4f& in2, Matrix4f& store) const noexcept;
	/** this = this * in2 */
	Matrix4f& multLocal(const Matrix4f& in2) noexcept;

	/** Rotation plus translation of vec as a new vector */
	Vector3f mult(const Vector3f& vec) const noexcept;
	/** Rotation plus translation of vec stored in store (store may be vec) */
	Vector3f& mult(const Vector3f& vec, Vector3f& store) const noexcept;
	/** Rotation of vec without translation (store may be vec) */
	Vector3f& multNormal(const Vector3f& vec, Vector3f& store) const noexcept;
	/** Rotation of vec by the transpose without translation (store may be vec) */
	Vector3f& multNormalAcross(const Vector3f& vec, Vector3f& store) const noexcept;
	/** Rotation plus translation stored in store; returns the w value (store may be vec) */
	float multProj(const Vector3f& vec, Vector3f& store) const noexcept;
	/** vec * this (row vector with w = 1) stored in store (store may be vec) */
	Vector3f& multAcross(const Vector3f& vec, Vector3f& store) const noexcept;
	/** this * vec4f, stored in the array */
	std::span<float, 4> mult(std::span<float, 4> vec4f) const noexcept;
	/** vec4f * this, stored in the array */
	std::span<float, 4> multAcross(std::span<float, 4> vec4f) const noexcept;

	/**
	 * The inverse as a new matrix.
	 * @throws ArithmeticException if the determinant is 0
	 */
	Matrix4f invert() const;
	/**
	 * The inverse stored in store (like Java, store must not be this).
	 * @throws ArithmeticException if the determinant is 0
	 */
	Matrix4f& invert(Matrix4f& store) const;
	/** Inverts this matrix (the zero matrix if the determinant is 0). */
	Matrix4f& invertLocal() noexcept;

	Matrix4f adjoint() const noexcept;
	/** Like Java, store must not be this. */
	Matrix4f& adjoint(Matrix4f& store) const noexcept;

	float determinant() const noexcept;

	Matrix4f& zero() noexcept;

	Matrix4f add(const Matrix4f& mat) const noexcept;
	void addLocal(const Matrix4f& mat) noexcept;

	Vector3f toTranslationVector() const noexcept;
	void toTranslationVector(Vector3f& vector) const noexcept;
	Matrix3f toRotationMatrix() const noexcept;
	void toRotationMatrix(Matrix3f& mat) const noexcept;
	void setRotationMatrix(const Matrix3f& mat) noexcept;

	/** Multiplies the diagonal elements m00, m11, m22. */
	void setScale(float x, float y, float z) noexcept;
	void setScale(const Vector3f& scaleVector) noexcept;

	/** @throws IllegalArgumentException if translation does not have 3 elements */
	void setTranslation(std::span<const float> translation);
	void setTranslation(float x, float y, float z) noexcept;
	void setTranslation(const Vector3f& translation) noexcept;
	/** @throws IllegalArgumentException if translation does not have 3 elements */
	void setInverseTranslation(std::span<const float> translation);

	/** Rotation about the x, y and z axes by the angles in degrees, matrix = (Z * Y) * X; clears the translation. */
	void angleRotation(const Vector3f& angles) noexcept;

	/** @throws IllegalArgumentException if angles does not have 3 elements */
	void setInverseRotationRadians(std::span<const float> angles);
	/**
	 * Like Java, the angles are multiplied by RAD_TO_DEG (sic) before calling setInverseRotationRadians.
	 * @throws IllegalArgumentException if angles does not have 3 elements
	 */
	void setInverseRotationDegrees(std::span<const float> angles);

	/** @throws IllegalArgumentException if vec does not have 3 elements */
	void inverseTranslateVect(std::span<float> vec) const;
	void inverseTranslateVect(Vector3f& data) const noexcept;
	void translateVect(Vector3f& data) const noexcept;
	void inverseRotateVect(Vector3f& vec) const noexcept;
	void rotateVect(Vector3f& vec) const noexcept;

	/** "Matrix4f\n[\n m00  m01  m02  m03 \n ... \n]" with Java's Float.toString formatting */
	std::string toString() const;

	int32_t hashCode() const noexcept;
	/** Java: equals(Object) - Float.compare of each element */
	bool equals(const Matrix4f& o) const noexcept;
	bool operator==(const Matrix4f& o) const noexcept { return equals(o); }

	bool isIdentity() const noexcept;

	/** Scales the first three columns (including row 3) by the components of scaleVector. */
	void scale(const Vector3f& scaleVector) noexcept;
	/** Scales the first three columns (including row 3) by factor. */
	void scale(float factor) noexcept;

	constexpr Matrix4f clone() const noexcept { return *this; }
};

inline constexpr Matrix4f Matrix4f::IDENTITY{};

} // namespace aion::gameserver::geoEngine::math
