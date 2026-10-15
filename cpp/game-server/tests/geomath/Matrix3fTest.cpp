// Matrix3f: hand-derived values, Java edge cases and the GeoWorldLoader/BoundingBox/Geometry usage.

#include "aion/gameserver/geoEngine/math/Matrix3f.h"

#include <array>
#include <cmath>
#include <limits>
#include <type_traits>

#include <gtest/gtest.h>

#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/geoEngine/math/FastMath.h"

#include "GeoMathTestSupport.h"

using namespace aion::gameserver::geoEngine::math;
using namespace aion::gameserver::geoEngine::math::test;
using aion::commons::utils::IllegalArgumentException;
using aion::commons::utils::IndexOutOfBoundsException;

namespace {

std::array<float, 9> rowMajor(const Matrix3f& m) {
	std::array<float, 9> values{};
	m.get(values, true);
	return values;
}

} // namespace

TEST(Matrix3fTest, NewMatrixIsIdentity) {
	EXPECT_TRUE(std::is_trivially_copyable_v<Matrix3f>);
	const Matrix3f m;
	EXPECT_TRUE(m.isIdentity());
	EXPECT_EQ(rowMajor(m), (std::array<float, 9>{1, 0, 0, 0, 1, 0, 0, 0, 1}));
	Matrix3f z = m;
	z.zero();
	EXPECT_FALSE(z.isIdentity());
	z.loadIdentity();
	EXPECT_TRUE(z.equals(m));
}

TEST(Matrix3fTest, IndexedAccess) {
	// GeoWorldLoader: matrix3f.set(i, j, geo.getFloat())
	Matrix3f m;
	float value = 1;
	for (int32_t i = 0; i < 3; i++) {
		for (int32_t j = 0; j < 3; j++)
			m.set(i, j, value++);
	}
	EXPECT_EQ(rowMajor(m), (std::array<float, 9>{1, 2, 3, 4, 5, 6, 7, 8, 9}));
	EXPECT_EQ(m.get(1, 2), 6.0f);
	EXPECT_THROW(m.get(3, 0), IllegalArgumentException);
	EXPECT_THROW(m.get(0, -1), IllegalArgumentException);
	EXPECT_THROW(m.set(2, 3, 1.0f), IllegalArgumentException);
	EXPECT_TRUE(sameVector(m.getColumn(1), 2, 5, 8));
	EXPECT_TRUE(sameVector(m.getRow(2), 7, 8, 9));
	EXPECT_THROW(m.getColumn(3), IllegalArgumentException);
	EXPECT_THROW(m.getRow(-1), IllegalArgumentException);
	m.setColumn(0, Vector3f(10, 11, 12)).setRow(2, Vector3f(20, 21, 22));
	EXPECT_EQ(rowMajor(m), (std::array<float, 9>{10, 2, 3, 11, 5, 6, 20, 21, 22}));
	std::array<float, 9> columnMajor{};
	m.get(columnMajor, false);
	EXPECT_EQ(columnMajor, (std::array<float, 9>{10, 11, 20, 2, 5, 21, 3, 6, 22}));
	std::array<float, 16> sixteen{};
	sixteen.fill(-1);
	m.get(sixteen, true); // only the upper 3x3 of a 4x4 layout
	EXPECT_EQ(sixteen, (std::array<float, 16>{10, 2, 3, -1, 11, 5, 6, -1, 20, 21, 22, -1, -1, -1, -1, -1}));
	std::array<float, 8> wrong{};
	EXPECT_THROW(m.get(wrong, true), IndexOutOfBoundsException);
	EXPECT_THROW(m.set(std::span<const float>(wrong), true), IllegalArgumentException);
	Matrix3f fromColumns;
	fromColumns.set(columnMajor, false);
	EXPECT_TRUE(fromColumns.equals(m));
	Matrix3f axes;
	axes.fromAxes(Vector3f(1, 2, 3), Vector3f(4, 5, 6), Vector3f(7, 8, 9));
	EXPECT_EQ(rowMajor(axes), (std::array<float, 9>{1, 4, 7, 2, 5, 8, 3, 6, 9}));
	Matrix3f fromArray;
	fromArray.set(std::array<std::array<float, 3>, 3>{{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}});
	EXPECT_EQ(rowMajor(fromArray), (std::array<float, 9>{1, 2, 3, 4, 5, 6, 7, 8, 9}));
}

TEST(Matrix3fTest, HandDerivedArithmetic) {
	const Matrix3f a(1, 2, 3, 4, 5, 6, 7, 8, 10);
	const Matrix3f b(2, 0, 0, 0, 3, 0, 0, 0, 4);
	EXPECT_EQ(rowMajor(a.mult(b)), (std::array<float, 9>{2, 6, 12, 8, 15, 24, 14, 24, 40}));
	EXPECT_TRUE(sameVector(a.mult(Vector3f(1, 1, 1)), 6, 15, 25));
	EXPECT_EQ(a.determinant(), -3.0f); // 1*(50-48) + 2*(42-40) + 3*(32-35)
	EXPECT_EQ(b.determinant(), 24.0f);
	// diag(2, 4, 8)^-1: adjoint diag(32, 16, 8) * (1/64)
	EXPECT_EQ(rowMajor(Matrix3f(2, 0, 0, 0, 4, 0, 0, 0, 8).invert()), (std::array<float, 9>{0.5f, 0, 0, 0, 0.25f, 0, 0, 0, 0.125f}));
	EXPECT_EQ(rowMajor(a.adjoint()), (std::array<float, 9>{2, 4, -3, 2, -11, 6, -3, 6, -3}));
	Matrix3f transposed = a;
	transposed.transposeLocal();
	EXPECT_EQ(rowMajor(transposed), (std::array<float, 9>{1, 4, 7, 2, 5, 8, 3, 6, 10}));
	EXPECT_TRUE(a.transposeNew().equals(transposed));
	Matrix3f scaled = a;
	scaled.multLocal(2);
	EXPECT_EQ(rowMajor(scaled), (std::array<float, 9>{2, 4, 6, 8, 10, 12, 14, 16, 20}));
	Matrix3f added = a;
	added.add(b);
	EXPECT_EQ(rowMajor(added), (std::array<float, 9>{3, 2, 3, 4, 8, 6, 7, 8, 14}));
	Matrix3f columnScaled = a;
	columnScaled.scale(Vector3f(1, 10, 100));
	EXPECT_EQ(rowMajor(columnScaled), (std::array<float, 9>{1, 20, 300, 4, 50, 600, 7, 80, 1000}));
}

TEST(Matrix3fTest, SingularMatrixInvertsToZero) {
	const Matrix3f singular(1, 2, 3, 2, 4, 6, 1, 1, 1);
	EXPECT_EQ(singular.determinant(), 0.0f);
	EXPECT_EQ(rowMajor(singular.invert()), (std::array<float, 9>{}));
	Matrix3f local = singular;
	EXPECT_EQ(rowMajor(local.invertLocal()), (std::array<float, 9>{}));
	// |det| <= FLT_EPSILON counts as singular for Matrix3f (Matrix4f only rejects exactly 0)
	const Matrix3f tiny(1e-3f, 0, 0, 0, 1e-3f, 0, 0, 0, 1e-1f);
	EXPECT_EQ(rowMajor(tiny.invert()), (std::array<float, 9>{}));
}

TEST(Matrix3fTest, AbsoluteLocalUsesFastMathAbs) {
	// BoundingBox.transform: transMatrix.absoluteLocal()
	Matrix3f m(-1, 2, -3, -0.0f, 0, 5, -6, 7, -8);
	m.absoluteLocal();
	const auto values = rowMajor(m);
	EXPECT_EQ(values[0], 1.0f);
	EXPECT_EQ(values[2], 3.0f);
	EXPECT_TRUE(std::signbit(values[3])); // FastMath.abs(-0.0f) is -0.0f
	EXPECT_EQ(values[8], 8.0f);
}

TEST(Matrix3fTest, AngleAxisAndStartEnd) {
	Matrix3f rotation;
	rotation.fromAngleNormalAxis(FastMath::HALF_PI, Vector3f::UNIT_Z);
	const Vector3f rotated = rotation.mult(Vector3f::UNIT_X);
	EXPECT_NEAR(rotated.x, 0.0f, 1e-7f);
	EXPECT_EQ(rotated.y, 1.0f);
	EXPECT_EQ(rotated.z, 0.0f);
	Matrix3f fromAxis;
	fromAxis.fromAngleAxis(FastMath::HALF_PI, Vector3f(0, 0, 7)); // normalizes a copy of the axis
	EXPECT_TRUE(fromAxis.equals(rotation));

	// fromStartEndVectors(UNIT_X, UNIT_Y): e = 0, v = (0, 0, 1), h = 1 -> exact 90 degree rotation about z
	Matrix3f startEnd;
	startEnd.fromStartEndVectors(Vector3f::UNIT_X, Vector3f::UNIT_Y);
	EXPECT_EQ(rowMajor(startEnd), (std::array<float, 9>{0, -1, 0, 1, 0, 0, 0, 0, 1}));
	// identical vectors take the nearly parallel branch and give the identity
	Matrix3f same;
	same.fromStartEndVectors(Vector3f::UNIT_Z, Vector3f::UNIT_Z);
	EXPECT_TRUE(same.isIdentity());
}

TEST(Matrix3fTest, MultWithAliasedArguments) {
	const Matrix3f a(1, 2, 3, 4, 5, 6, 7, 8, 10);
	Matrix3f product(2, 0, 0, 0, 3, 0, 0, 0, 4);
	a.mult(product, product); // safe for mat and product to be the same object
	EXPECT_EQ(rowMajor(product), (std::array<float, 9>{2, 6, 12, 8, 15, 24, 14, 24, 40}));
	Matrix3f self = a;
	self.multLocal(self);
	EXPECT_TRUE(self.equals(a.mult(a)));
	Vector3f v(1, 1, 1);
	EXPECT_TRUE(sameVector(a.multLocal(v), 6, 15, 25));
	EXPECT_TRUE(sameVector(v, 6, 15, 25));
}

TEST(Matrix3fTest, EqualsHashCodeToString) {
	const Matrix3f m;
	EXPECT_EQ(m.toString(), "Matrix3f\n[\n 1.0  0.0  0.0 \n 0.0  1.0  0.0 \n 0.0  0.0  1.0 \n]");
	Matrix3f negativeZero;
	negativeZero.set(0, 1, -0.0f);
	EXPECT_FALSE(negativeZero.equals(m));
	EXPECT_NE(negativeZero.hashCode(), m.hashCode());
	Matrix3f nan1, nan2;
	nan1.set(2, 2, std::numeric_limits<float>::quiet_NaN());
	nan2.set(2, 2, -std::numeric_limits<float>::quiet_NaN());
	EXPECT_TRUE(nan1 == nan2);
	EXPECT_EQ(nan1.hashCode(), nan2.hashCode());
	const Matrix3f copy = m.clone();
	EXPECT_TRUE(copy.equals(m));
}
