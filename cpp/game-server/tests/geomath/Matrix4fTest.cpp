// Matrix4f: hand-derived values, Java edge cases and the Geometry/BIHNode/BoundingBox usage.

#include "aion/gameserver/geoEngine/math/Matrix4f.h"

#include <array>
#include <cmath>
#include <limits>
#include <type_traits>

#include <gtest/gtest.h>

#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/geoEngine/math/FastMath.h"
#include "aion/gameserver/geoEngine/math/Matrix3f.h"
#include "aion/gameserver/geoEngine/math/Ray.h"

#include "GeoMathTestSupport.h"

using namespace aion::gameserver::geoEngine::math;
using namespace aion::gameserver::geoEngine::math::test;
using aion::commons::utils::IllegalArgumentException;

namespace {

std::array<float, 16> rowMajor(const Matrix4f& m) {
	std::array<float, 16> values{};
	m.get(values);
	return values;
}

} // namespace

TEST(Matrix4fTest, NewMatrixIsIdentity) {
	EXPECT_TRUE(std::is_trivially_copyable_v<Matrix4f>);
	EXPECT_TRUE(Matrix4f().isIdentity());
	EXPECT_TRUE(Matrix4f::IDENTITY.isIdentity());
	EXPECT_EQ(Matrix4f::IDENTITY.toString(), "Matrix4f\n[\n 1.0  0.0  0.0  0.0 \n 0.0  1.0  0.0  0.0 \n 0.0  0.0  1.0  0.0 \n 0.0  0.0  0.0  1.0 \n]");
	Matrix4f m;
	m.m23 = 1;
	EXPECT_FALSE(m.isIdentity());
	m.loadIdentity();
	EXPECT_TRUE(m.equals(Matrix4f::IDENTITY));
}

TEST(Matrix4fTest, ArraysAndIndexedAccess) {
	const std::array<float, 16> values = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
	Matrix4f rows;
	rows.set(values);
	EXPECT_EQ(rows.m03, 4.0f);
	EXPECT_EQ(rows.m30, 13.0f);
	EXPECT_EQ(rows.get(2, 1), 10.0f);
	const Matrix4f columns(values); // Java Matrix4f(float[]) is column-major
	EXPECT_EQ(columns.m03, 13.0f);
	EXPECT_EQ(columns.m30, 4.0f);
	EXPECT_TRUE(columns.equals(rows.transpose()));
	Matrix4f transposed = rows;
	EXPECT_TRUE(transposed.transposeLocal().equals(columns));
	std::array<float, 16> columnMajor{};
	rows.get(columnMajor, false);
	std::array<float, 16> filled{};
	rows.fillFloatArray(filled, true);
	EXPECT_EQ(columnMajor, filled);
	EXPECT_EQ(rows.getColumn(1), (std::array<float, 4>{2, 6, 10, 14}));
	rows.setColumn(3, std::array<float, 4>{-1, -2, -3, -4});
	EXPECT_EQ(rows.get(1, 3), -2.0f);
	rows.set(3, 0, 99);
	EXPECT_EQ(rows.m30, 99.0f);
	EXPECT_THROW(rows.get(4, 0), IllegalArgumentException);
	EXPECT_THROW(rows.set(0, 4, 1), IllegalArgumentException);
	EXPECT_THROW(rows.getColumn(-1), IllegalArgumentException);
	std::array<float, 4> column{};
	EXPECT_THROW(rows.setColumn(4, column), IllegalArgumentException);
	std::array<float, 9> nine{};
	EXPECT_THROW(rows.set(std::span<const float>(nine)), IllegalArgumentException);
	EXPECT_THROW(rows.get(nine), IllegalArgumentException);
	EXPECT_THROW({ const Matrix4f fromNine{std::span<const float>(nine)}; }, IllegalArgumentException);
	std::array<float, 2> two{};
	EXPECT_THROW(rows.setTranslation(std::span<const float>(two)), IllegalArgumentException);
	EXPECT_THROW(rows.setInverseRotationRadians(two), IllegalArgumentException);
}

TEST(Matrix4fTest, GeometrySetTransformAndTranslation) {
	// Geometry.setTransform(rotation, loc, scale)
	Matrix3f rotation;
	rotation.fromAngleNormalAxis(FastMath::PI, Vector3f::UNIT_Z);
	Matrix4f world;
	world.loadIdentity();
	world.setRotationMatrix(rotation);
	world.scale(Vector3f(2, 2, 2));
	world.setTranslation(Vector3f(100, 200, 300));
	EXPECT_EQ(world.m03, 100.0f);
	EXPECT_EQ(world.m13, 200.0f);
	EXPECT_EQ(world.m23, 300.0f);
	EXPECT_EQ(world.m22, 2.0f);
	EXPECT_EQ(world.m33, 1.0f); // scale does not touch row 3 column 3
	EXPECT_TRUE(sameVector(world.toTranslationVector(), 100, 200, 300));
	Matrix3f back;
	world.toRotationMatrix(back);
	EXPECT_TRUE(back.equals(world.toRotationMatrix()));

	// translation only: mult adds it, multNormal does not, the inverse is exact
	Matrix4f translation;
	translation.setTranslation(1, 2, 3);
	EXPECT_TRUE(sameVector(translation.mult(Vector3f(1, 1, 1)), 2, 3, 4));
	Vector3f normal;
	EXPECT_TRUE(sameVector(translation.multNormal(Vector3f(1, 1, 1), normal), 1, 1, 1));
	const Matrix4f inverse = translation.invert();
	EXPECT_EQ(rowMajor(inverse), (std::array<float, 16>{1, 0, 0, -1, 0, 1, 0, -2, 0, 0, 1, -3, 0, 0, 0, 1}));
	EXPECT_TRUE(translation.mult(inverse).isIdentity());
	Vector3f vector(5, 5, 5);
	translation.translateVect(vector);
	EXPECT_TRUE(sameVector(vector, 6, 7, 8));
	translation.inverseTranslateVect(vector);
	EXPECT_TRUE(sameVector(vector, 5, 5, 5));
	std::array<float, 3> array = {1, 1, 1};
	translation.inverseTranslateVect(array);
	EXPECT_EQ(array, (std::array<float, 3>{0, -1, -2}));
	translation.setInverseTranslation(std::array<float, 3>{4, 5, 6});
	EXPECT_TRUE(sameVector(translation.toTranslationVector(), -4, -5, -6));
	translation.setTranslation(std::array<float, 3>{7, 8, 9});
	Vector3f t;
	translation.toTranslationVector(t);
	EXPECT_TRUE(sameVector(t, 7, 8, 9));
}

TEST(Matrix4fTest, SingularMatrix) {
	Matrix4f singular;
	singular.m22 = 0;
	EXPECT_EQ(singular.determinant(), 0.0f);
	EXPECT_THROW(singular.invert(), ArithmeticException);
	Matrix4f store;
	EXPECT_THROW(singular.invert(store), ArithmeticException);
	try {
		singular.invert();
	} catch (const ArithmeticException& e) {
		EXPECT_STREQ(e.what(), "This matrix cannot be inverted");
	}
	Matrix4f local = singular;
	EXPECT_EQ(rowMajor(local.invertLocal()), (std::array<float, 16>{}));
	// unlike Matrix3f, a tiny non-zero determinant is inverted
	Matrix4f tiny;
	tiny.m00 = tiny.m11 = tiny.m22 = 1e-3f;
	EXPECT_NO_THROW(tiny.invert());
	// NaN passes the |det| <= 0 check
	Matrix4f withNaN;
	withNaN.m12 = std::numeric_limits<float>::quiet_NaN();
	EXPECT_NO_THROW(withNaN.invert());
}

TEST(Matrix4fTest, BihNodeRayTransformRoundTrip) {
	// BIHNode.intersectWhere: transform the ray into model space in place and restore it afterwards
	Matrix4f world;
	world.setTranslation(10, 20, 30);
	world.scale(4.0f);
	const Matrix4f inv = world.invert();
	Ray r(Vector3f(14, 28, 42), Vector3f(0, 0, -8));
	const Vector3f o = Vector3f().set(r.getOrigin());
	const Vector3f d = Vector3f().set(r.getDirection());
	inv.mult(r.getOrigin(), r.getOrigin());
	inv.multNormal(r.getDirection(), r.getDirection());
	EXPECT_TRUE(sameVector(r.getOrigin(), 1, 2, 3));
	EXPECT_TRUE(sameVector(r.getDirection(), 0, 0, -2));
	r.getDirection().normalizeLocal();
	EXPECT_TRUE(sameVector(r.getDirection(), 0, 0, -1));
	r.setOrigin(o);
	r.setDirection(d);
	EXPECT_TRUE(sameVector(r.origin, 14, 28, 42));
	EXPECT_TRUE(sameVector(r.direction, 0, 0, -8));
}

TEST(Matrix4fTest, ProjectionAndAcross) {
	Matrix4f m(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
	Vector3f store;
	EXPECT_EQ(m.multProj(Vector3f(1, 1, 1), store), 58.0f); // 13 + 14 + 15 + 16
	EXPECT_TRUE(sameVector(store, 10, 26, 42));
	EXPECT_TRUE(sameVector(m.multAcross(Vector3f(1, 1, 1), store), 28, 32, 36)); // columns + row 3
	EXPECT_TRUE(sameVector(m.multNormalAcross(Vector3f(1, 0, 0), store), 1, 2, 3));
	std::array<float, 4> vec4 = {1, 0, 0, 0};
	m.mult(vec4);
	EXPECT_EQ(vec4, (std::array<float, 4>{1, 5, 9, 13}));
	std::array<float, 4> across = {1, 0, 0, 0};
	m.multAcross(across);
	EXPECT_EQ(across, (std::array<float, 4>{1, 2, 3, 4}));
	EXPECT_EQ(m.determinant(), 0.0f);
	EXPECT_TRUE(m.add(m).equals(m.mult(2.0f)));
	Matrix4f sum = m;
	sum.addLocal(m);
	Matrix4f doubled;
	EXPECT_TRUE(sum.equals(m.mult(2.0f, doubled)));
	Vector3f v(1, 2, 3);
	Matrix4f identity;
	identity.rotateVect(v);
	identity.inverseRotateVect(v);
	EXPECT_TRUE(sameVector(v, 1, 2, 3));
}

TEST(Matrix4fTest, ScaleVariants) {
	Matrix4f m(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1);
	Matrix4f columns = m;
	columns.scale(Vector3f(2, 3, 4)); // columns 0..2 including row 3
	EXPECT_EQ(rowMajor(columns), (std::array<float, 16>{2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, 1}));
	Matrix4f uniform = m;
	uniform.scale(5.0f);
	EXPECT_EQ(rowMajor(uniform), (std::array<float, 16>{5, 5, 5, 1, 5, 5, 5, 1, 5, 5, 5, 1, 5, 5, 5, 1}));
	Matrix4f diagonal = m;
	diagonal.setScale(2, 3, 4); // only m00, m11, m22
	EXPECT_EQ(rowMajor(diagonal), (std::array<float, 16>{2, 1, 1, 1, 1, 3, 1, 1, 1, 1, 4, 1, 1, 1, 1, 1}));
	Matrix4f diagonalVector = m;
	diagonalVector.setScale(Vector3f(2, 3, 4));
	EXPECT_TRUE(diagonalVector.equals(diagonal));
	Matrix4f all = m;
	all.multLocal(3.0f); // Java: void multLocal(float)
	EXPECT_EQ(all.m33, 3.0f);
}

TEST(Matrix4fTest, Rotations) {
	Matrix4f angle;
	angle.fromAngleAxis(FastMath::HALF_PI, Vector3f(0, 0, 3));
	EXPECT_EQ(angle.m33, 1.0f);
	EXPECT_EQ(angle.m01, -1.0f);
	EXPECT_EQ(angle.m10, 1.0f);
	Matrix4f euler;
	euler.m03 = 5;
	euler.angleRotation(Vector3f(0, 0, 90)); // clears the translation
	EXPECT_EQ(euler.m03, 0.0f);
	EXPECT_NEAR(euler.m10, 1.0f, 1e-6f);
	EXPECT_NEAR(euler.m01, -1.0f, 1e-6f);
	Matrix4f inverseRotation;
	inverseRotation.setInverseRotationRadians(std::array<float, 3>{0, 0, 0});
	EXPECT_TRUE(inverseRotation.isIdentity());
	// Java multiplies degrees by RAD_TO_DEG (a jME bug kept as is): 0 stays 0
	inverseRotation.setInverseRotationDegrees(std::array<float, 3>{0, 0, 0});
	EXPECT_TRUE(inverseRotation.isIdentity());
}

TEST(Matrix4fTest, EqualsAndHashCode) {
	Matrix4f a;
	Matrix4f b;
	EXPECT_TRUE(a == b);
	EXPECT_EQ(a.hashCode(), b.hashCode());
	b.m32 = -0.0f;
	EXPECT_FALSE(a == b);
	EXPECT_NE(a.hashCode(), b.hashCode());
	a.m01 = std::numeric_limits<float>::quiet_NaN();
	Matrix4f c;
	c.m01 = -std::numeric_limits<float>::quiet_NaN();
	EXPECT_TRUE(a.equals(c));
	const Matrix4f copy = c.clone();
	Matrix4f copied;
	copied.copy(c);
	EXPECT_TRUE(copy.equals(copied));
}
