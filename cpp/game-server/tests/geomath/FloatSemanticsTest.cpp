// Java float semantics of the library as a whole: no fused multiply-add (FMA contraction) in any build configuration.
// Each probe is an expression P * Q + R * S (or P * Q + R) whose separately rounded result differs from every contracted variant, so a
// compiler that fuses either product fails the test. The random golden vectors (GoldenVectorsTest) cover the remaining expressions.

#include <bit>
#include <cmath>

#include <gtest/gtest.h>

#include "aion/gameserver/geoEngine/math/Matrix3f.h"
#include "aion/gameserver/geoEngine/math/Matrix4f.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"

#include "GeoMathTestSupport.h"

using namespace aion::gameserver::geoEngine::math;
using namespace aion::gameserver::geoEngine::math::test;

namespace {

// C * C = 1 + 2^-11 + 2^-24 exactly (rounds to 1 + 2^-11, a tie to even); D * D = 1 - 2^-12 + 2^-26 (rounds to 1 - 2^-12)
constexpr float C = 1.0f + 0x1p-12f;
constexpr float D = 1.0f - 0x1p-13f;
// C * C - D * D evaluated like Java: round(C * C) - round(D * D) = 3 * 2^-12
constexpr float SEPARATE = 0x3p-12f;

} // namespace

TEST(FloatSemanticsTest, ProbesDistinguishEveryContraction) {
	volatile float c = C; // runtime values, no constant folding in the test itself
	volatile float d = D;
	const float cc = static_cast<float>(c) * static_cast<float>(c);
	const float dd = static_cast<float>(d) * static_cast<float>(d);
	EXPECT_EQ(bitsOf(cc), bitsOf(1.0f + 0x1p-11f));
	EXPECT_EQ(bitsOf(dd), bitsOf(1.0f - 0x1p-12f));
	EXPECT_EQ(bitsOf(cc - dd), bitsOf(SEPARATE));
	// fma(C, C, -round(D * D)) and -fma(D, D, -round(C * C)) both differ from the separate result
	EXPECT_NE(bitsOf(std::fma(static_cast<float>(c), static_cast<float>(c), -dd)), bitsOf(SEPARATE));
	EXPECT_NE(bitsOf(-std::fma(static_cast<float>(d), static_cast<float>(d), -cc)), bitsOf(SEPARATE));
	EXPECT_NE(bitsOf(std::fma(static_cast<float>(c), static_cast<float>(c), -1.0f)), bitsOf(0x1p-11f));
}

TEST(FloatSemanticsTest, VectorOperationsDoNotContract) {
	EXPECT_FLOAT_BITS(Vector3f(C, -D, 0).dot(Vector3f(C, D, 0)), SEPARATE);    // x * vx + y * vy + z * vz
	EXPECT_FLOAT_BITS(Vector3f(C, D, 0).cross(Vector3f(D, C, 0)).z, SEPARATE); // x * otherY - y * otherX
	Vector3f crossLocal(C, D, 0);
	EXPECT_FLOAT_BITS(crossLocal.crossLocal(D, C, 0).z, SEPARATE);
	Vector3f scaleAdd(C, 0, 0);
	EXPECT_FLOAT_BITS(scaleAdd.scaleAdd(C, Vector3f(-1, 0, 0)).x, 0x1p-11f); // x * scalar + add.x
	Vector3f interpolated(0, 0, 0);
	// (1 - t) * this.x + t * final.x with t = C - 1 = 2^-12: (1 - 2^-12) * 0 + 2^-12 * C, no product with a non-zero sum to fuse into
	EXPECT_FLOAT_BITS(interpolated.interpolate(Vector3f(C, 0, 0), 0x1p-12f).x, 0x1p-12f * C);
}

TEST(FloatSemanticsTest, MatrixOperationsDoNotContract) {
	Matrix4f m4;
	m4.m00 = C;
	m4.m01 = -D;
	EXPECT_FLOAT_BITS(m4.mult(Vector3f(C, D, 0)).x, SEPARATE); // m00 * vx + m01 * vy + m02 * vz + m03
	Vector3f store;
	EXPECT_FLOAT_BITS(m4.multNormal(Vector3f(C, D, 0), store).x, SEPARATE);
	EXPECT_FLOAT_BITS((m4.multProj(Vector3f(C, D, 0), store), store.x), SEPARATE);
	Matrix4f across;
	across.m00 = C;
	across.m10 = -D;
	EXPECT_FLOAT_BITS(across.multAcross(Vector3f(C, D, 0), store).x, SEPARATE); // m00 * vx + m10 * vy + ...

	Matrix4f left;
	left.m00 = C;
	left.m01 = -D;
	Matrix4f right;
	right.m00 = C;
	right.m10 = D;
	EXPECT_FLOAT_BITS(left.mult(right).m00, SEPARATE); // m00 * in2.m00 + m01 * in2.m10 + ...

	Matrix4f det;
	det.m00 = C;
	det.m11 = C;
	det.m01 = D;
	det.m10 = D;
	EXPECT_FLOAT_BITS(det.determinant(), SEPARATE); // fA0 = m00 * m11 - m01 * m10, then fA0 * fB5 with fB5 = 1

	Matrix3f m3(C, -D, 0, 0, 1, 0, 0, 0, 1);
	EXPECT_FLOAT_BITS(m3.mult(Vector3f(C, D, 0)).x, SEPARATE);
	EXPECT_FLOAT_BITS(Matrix3f(1, 0, 0, 0, C, D, 0, D, C).determinant(), SEPARATE); // fCo00 = m11 * m22 - m12 * m21
	Matrix3f left3(C, -D, 0, 0, 1, 0, 0, 0, 1);
	const Matrix3f right3(C, 0, 0, D, 1, 0, 0, 0, 1);
	EXPECT_FLOAT_BITS(left3.multLocal(right3).get(0, 0), SEPARATE);
}
