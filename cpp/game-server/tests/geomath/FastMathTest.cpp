// FastMath: constants, clamping of the inverse trigonometric functions, Java edge cases (NaN, -0.0f, infinities) and hand-derived values.

#include "aion/gameserver/geoEngine/math/FastMath.h"

#include <cmath>
#include <limits>
#include <numbers>

#include <gtest/gtest.h>

#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/geoEngine/math/Vector2f.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"

#include "GeoMathTestSupport.h"

using namespace aion::gameserver::geoEngine::math;
using namespace aion::gameserver::geoEngine::math::test;

namespace {

constexpr float NaN = std::numeric_limits<float>::quiet_NaN();
constexpr float Inf = std::numeric_limits<float>::infinity();

} // namespace

TEST(FastMathTest, ConstantsHaveTheJavaBits) {
	EXPECT_EQ(bitsOf(FastMath::PI), 0x40490fdbu);     // (float) Math.PI
	EXPECT_EQ(bitsOf(FastMath::TWO_PI), 0x40c90fdbu); // exact doubling
	EXPECT_EQ(bitsOf(FastMath::HALF_PI), 0x3fc90fdbu);
	EXPECT_EQ(bitsOf(FastMath::QUARTER_PI), 0x3f490fdbu);
	EXPECT_EQ(bitsOf(FastMath::FLOAT_EPSILON), 0x34000000u); // 2^-23
	EXPECT_EQ(FastMath::DOUBLE_EPSILON, 0x1p-52);
	EXPECT_EQ(FastMath::ONE_THIRD, 0.33333334f);
	EXPECT_EQ(FastMath::ZERO_TOLERANCE, 0.0001f);
	EXPECT_EQ(FastMath::DEG_TO_RAD, FastMath::PI / 180.0f);
	EXPECT_EQ(FastMath::RAD_TO_DEG, 180.0f / FastMath::PI);
	static_assert(FastMath::TWO_PI == 2.0f * FastMath::PI);
}

TEST(FastMathTest, InverseTrigonometricClamping) {
	// acos: PI unless -1 < v, 0 unless v < 1 (NaN fails -1 < v)
	EXPECT_FLOAT_BITS(FastMath::acos(1.0f), 0.0f);
	EXPECT_FLOAT_BITS(FastMath::acos(1.5f), 0.0f);
	EXPECT_FLOAT_BITS(FastMath::acos(Inf), 0.0f);
	EXPECT_FLOAT_BITS(FastMath::acos(-1.0f), FastMath::PI);
	EXPECT_FLOAT_BITS(FastMath::acos(-1.0001f), FastMath::PI);
	EXPECT_FLOAT_BITS(FastMath::acos(NaN), FastMath::PI);
	EXPECT_FLOAT_BITS(FastMath::acos(0.0f), FastMath::HALF_PI);                      // (float) (pio2_hi + pio2_lo)
	EXPECT_FLOAT_BITS(FastMath::acos(0.5f), static_cast<float>(1.0471975511965979)); // fdlibm acos(0.5) = pi/3
	// asin: -HALF_PI unless -1 < v, HALF_PI unless v < 1
	EXPECT_FLOAT_BITS(FastMath::asin(1.0f), FastMath::HALF_PI);
	EXPECT_FLOAT_BITS(FastMath::asin(2.0f), FastMath::HALF_PI);
	EXPECT_FLOAT_BITS(FastMath::asin(-1.0f), -FastMath::HALF_PI);
	EXPECT_FLOAT_BITS(FastMath::asin(NaN), -FastMath::HALF_PI);
	EXPECT_FLOAT_BITS(FastMath::asin(-0.0f), -0.0f);
	EXPECT_FLOAT_BITS(FastMath::asin(0.5f), static_cast<float>(0.5235987755982989));
	// atan/atan2 are not clamped
	EXPECT_FLOAT_BITS(FastMath::atan(Inf), FastMath::HALF_PI);
	EXPECT_FLOAT_BITS(FastMath::atan(1.0f), FastMath::QUARTER_PI);
	EXPECT_FLOAT_BITS(FastMath::atan2(0.0f, -1.0f), FastMath::PI);
	EXPECT_FLOAT_BITS(FastMath::atan2(-0.0f, -1.0f), -FastMath::PI);
	EXPECT_FLOAT_BITS(FastMath::atan2(1.0f, 0.0f), FastMath::HALF_PI);
	EXPECT_FLOAT_BITS(FastMath::atan2(-0.0f, 0.0f), -0.0f);
	EXPECT_TRUE(std::isnan(FastMath::atan2(NaN, 1.0f)));
}

TEST(FastMathTest, AbsSignClamp) {
	EXPECT_FLOAT_BITS(FastMath::abs(-2.5f), 2.5f);
	EXPECT_FLOAT_BITS(FastMath::abs(-0.0f), -0.0f); // fValue < 0 is false for -0.0f
	EXPECT_TRUE(std::isnan(FastMath::abs(NaN)));
	EXPECT_FLOAT_BITS(FastMath::abs(-Inf), Inf);
	EXPECT_FLOAT_BITS(FastMath::sign(-7.0f), -1.0f);
	EXPECT_FLOAT_BITS(FastMath::sign(0.001f), 1.0f);
	EXPECT_FLOAT_BITS(FastMath::sign(-0.0f), -0.0f);
	EXPECT_TRUE(std::isnan(FastMath::sign(NaN)));
	EXPECT_EQ(FastMath::sign(-3), -1);
	EXPECT_EQ(FastMath::sign(0), 0);
	EXPECT_EQ(FastMath::sign(42), 1);
	EXPECT_FLOAT_BITS(FastMath::clamp(5.0f, 0.0f, 1.0f), 1.0f);
	EXPECT_FLOAT_BITS(FastMath::clamp(-5.0f, 0.0f, 1.0f), 0.0f);
	EXPECT_TRUE(std::isnan(FastMath::clamp(NaN, 0.0f, 1.0f)));
	EXPECT_FLOAT_BITS(FastMath::saturate(0.25f), 0.25f);
	EXPECT_FLOAT_BITS(FastMath::sqr(-3.0f), 9.0f);
}

TEST(FastMathTest, CopysignIsTheJmeVariant) {
	EXPECT_FLOAT_BITS(FastMath::copysign(-2.0f, 1.0f), 2.0f);
	EXPECT_FLOAT_BITS(FastMath::copysign(2.0f, -1.0f), -2.0f);
	EXPECT_FLOAT_BITS(FastMath::copysign(2.0f, 1.0f), 2.0f);
	EXPECT_FLOAT_BITS(FastMath::copysign(0.0f, 1.0f), -0.0f); // x <= -0 holds for 0.0f: negated
	EXPECT_FLOAT_BITS(FastMath::copysign(0.0f, -1.0f), -0.0f);
	EXPECT_FLOAT_BITS(FastMath::copysign(-0.0f, -0.0f), 0.0f); // y >= 0 holds for -0.0f
	EXPECT_TRUE(std::isnan(FastMath::copysign(NaN, 1.0f)));
}

TEST(FastMathTest, SquareRoots) {
	EXPECT_FLOAT_BITS(FastMath::sqrt(16.0f), 4.0f);
	EXPECT_FLOAT_BITS(FastMath::sqrt(-0.0f), -0.0f);
	EXPECT_TRUE(std::isnan(FastMath::sqrt(-1.0f)));
	EXPECT_FLOAT_BITS(FastMath::sqrt(Inf), Inf);
	EXPECT_FLOAT_BITS(FastMath::invSqrt(4.0f), 0.5f);
	EXPECT_FLOAT_BITS(FastMath::invSqrt(0.0f), Inf);
	EXPECT_FLOAT_BITS(FastMath::invSqrt(-0.0f), -Inf); // 1 / sqrt(-0.0)
	EXPECT_TRUE(std::isnan(FastMath::invSqrt(-4.0f)));
	EXPECT_FLOAT_BITS(FastMath::invSqrt(Inf), 0.0f);
	// fastInvSqrt(1): i = 0x3f800000, guess 0x5f375a86 - 0x1fc00000 = 0x3f775a86 (0.96622...), one Newton step
	const float guess = std::bit_cast<float>(0x3f775a86);
	EXPECT_FLOAT_BITS(FastMath::fastInvSqrt(1.0f), guess * (1.5f - 0.5f * guess * guess));
	EXPECT_NEAR(FastMath::fastInvSqrt(1.0f), 1.0f, 2e-3f); // 0.9983081
	EXPECT_NEAR(FastMath::fastInvSqrt(4.0f), 0.5f, 1e-3f);
	// negative input: guess 0x5f375a86 - (0xbf800000 >> 1, arithmetic) = 0x7f675a86 (~3.07e38), x * x overflows: infinity, not NaN
	EXPECT_FLOAT_BITS(FastMath::fastInvSqrt(-1.0f), Inf);
}

TEST(FastMathTest, Interpolation) {
	EXPECT_FLOAT_BITS(FastMath::interpolateLinear(0.5f, 2.0f, 2.0f), 2.0f);
	EXPECT_FLOAT_BITS(FastMath::interpolateLinear(-1.0f, 2.0f, 4.0f), 2.0f);
	EXPECT_FLOAT_BITS(FastMath::interpolateLinear(0.0f, 2.0f, 4.0f), 2.0f);
	EXPECT_FLOAT_BITS(FastMath::interpolateLinear(1.0f, 2.0f, 4.0f), 4.0f);
	EXPECT_FLOAT_BITS(FastMath::interpolateLinear(0.25f, 2.0f, 4.0f), 2.5f);
	EXPECT_TRUE(std::isnan(FastMath::interpolateLinear(NaN, 2.0f, 4.0f))); // NaN passes both checks
	EXPECT_TRUE(sameVector(FastMath::interpolateLinear(0.5f, Vector3f(0, 0, 0), Vector3f(2, 4, 6)), 1, 2, 3));
	// Catmull-Rom passes through p1 at u = 0 and p2 at u = 1
	EXPECT_FLOAT_BITS(FastMath::interpolateCatmullRom(0.0f, 0.5f, 1, 2, 3, 4), 2.0f);
	EXPECT_FLOAT_BITS(FastMath::interpolateCatmullRom(1.0f, 0.5f, 1, 2, 3, 4), 3.0f);
	EXPECT_TRUE(sameVector(FastMath::interpolateCatmullRom(0.5f, 0.5f, Vector3f(0, 0, 0), Vector3f(1, 1, 1), Vector3f(2, 2, 2), Vector3f(3, 3, 3)),
	                       1.5f, 1.5f, 1.5f));
}

TEST(FastMathTest, AnglesAndTrigonometry) {
	EXPECT_FLOAT_BITS(FastMath::reduceSinAngle(FastMath::PI), FastMath::PI - FastMath::PI); // |PI| > HALF_PI -> PI - PI
	EXPECT_FLOAT_BITS(FastMath::reduceSinAngle(0.5f), 0.5f);
	EXPECT_FLOAT_BITS(FastMath::reduceSinAngle(-0.5f), -0.5f);
	EXPECT_TRUE(std::isnan(FastMath::reduceSinAngle(Inf)));
	for (float angle = -10.0f; angle <= 10.0f; angle += 0.37f) {
		SCOPED_TRACE(angle);
		EXPECT_NEAR(FastMath::sin2(angle), std::sin(angle), 2e-6f);
		EXPECT_NEAR(FastMath::cos2(angle), std::cos(angle), 2e-6f);
	}
	EXPECT_FLOAT_BITS(FastMath::sin(0.0f), 0.0f);
	EXPECT_FLOAT_BITS(FastMath::cos(0.0f), 1.0f);
	EXPECT_FLOAT_BITS(FastMath::tan(0.0f), 0.0f);
	EXPECT_FLOAT_BITS(FastMath::exp(0.0f), 1.0f);
	EXPECT_FLOAT_BITS(FastMath::log(1.0f), 0.0f);
	EXPECT_FLOAT_BITS(FastMath::log(8.0f, 2.0f), 3.0f);
	EXPECT_FLOAT_BITS(FastMath::pow(2.0f, 10.0f), 1024.0f);
	EXPECT_FLOAT_BITS(FastMath::floor(-1.5f), -2.0f);
	EXPECT_FLOAT_BITS(FastMath::ceil(-1.5f), -1.0f);
	EXPECT_FLOAT_BITS(FastMath::normalize(7.0f, -FastMath::PI, FastMath::PI), 7.0f - FastMath::TWO_PI);
	EXPECT_FLOAT_BITS(FastMath::normalize(-7.0f, -FastMath::PI, FastMath::PI), -7.0f + FastMath::TWO_PI);
	EXPECT_FLOAT_BITS(FastMath::normalize(NaN, 0, 1), 0.0f);
	EXPECT_FLOAT_BITS(FastMath::normalize(-Inf, 0, 1), 0.0f);
}

TEST(FastMathTest, IntegerHelpers) {
	EXPECT_TRUE(FastMath::isPowerOfTwo(1));
	EXPECT_TRUE(FastMath::isPowerOfTwo(1024));
	EXPECT_FALSE(FastMath::isPowerOfTwo(0));
	EXPECT_FALSE(FastMath::isPowerOfTwo(-8));
	EXPECT_FALSE(FastMath::isPowerOfTwo(12));
	EXPECT_FALSE(FastMath::isPowerOfTwo(std::numeric_limits<int32_t>::min()));
	EXPECT_EQ(FastMath::nearestPowerOfTwo(5), 8);
	EXPECT_EQ(FastMath::nearestPowerOfTwo(8), 8);
	EXPECT_EQ(FastMath::nearestPowerOfTwo(1000), 1024);
	EXPECT_EQ(FastMath::nearestPowerOfTwo(1), 1);
	EXPECT_EQ(FastMath::nearestPowerOfTwo(0), 0);                                                                     // (int) pow(2, -inf)
	EXPECT_EQ(FastMath::nearestPowerOfTwo(-3), 0);                                                                    // (int) NaN
	EXPECT_EQ(FastMath::nearestPowerOfTwo(std::numeric_limits<int32_t>::max()), std::numeric_limits<int32_t>::max()); // 2^31 saturates
}

TEST(FastMathTest, Determinant) {
	EXPECT_FLOAT_BITS(FastMath::determinant(1, 0, 0, 0, 0, 2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 4), 24.0f);
	EXPECT_FLOAT_BITS(FastMath::determinant(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16), 0.0f);
	EXPECT_FLOAT_BITS(FastMath::determinant(0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1), -1.0f);
}

TEST(FastMathTest, TrianglePredicates) {
	const Vector2f a(0, 0), b(4, 0), c(0, 4);
	EXPECT_EQ(FastMath::counterClockwise(a, b, c), 1);
	EXPECT_EQ(FastMath::counterClockwise(a, c, b), -1);
	EXPECT_EQ(FastMath::counterClockwise(a, b, Vector2f(2, 0)), 0);   // between p0 and p1
	EXPECT_EQ(FastMath::counterClockwise(a, b, Vector2f(8, 0)), 1);   // collinear beyond p1
	EXPECT_EQ(FastMath::counterClockwise(a, b, Vector2f(-1, 0)), -1); // collinear behind p0
	EXPECT_EQ(FastMath::pointInsideTriangle(a, b, c, Vector2f(1, 1)), 1);
	EXPECT_EQ(FastMath::pointInsideTriangle(a, c, b, Vector2f(1, 1)), -1);
	EXPECT_EQ(FastMath::pointInsideTriangle(a, b, c, Vector2f(3, 3)), 0);
	EXPECT_EQ(FastMath::pointInsideTriangle(a, b, c, Vector2f(2, 0)), 1); // on an edge
}

TEST(FastMathTest, SphericalCoordinates) {
	Vector3f store;
	FastMath::sphericalToCartesian(Vector3f(2, 0, 0), store); // radius 2, azimuth 0, polar 0 -> (2, 0, 0)
	EXPECT_TRUE(sameVector(store, 2, 0, 0));
	Vector3f cart(0, 3, 0);
	Vector3f spherical;
	FastMath::cartesianToSpherical(cart, spherical);
	EXPECT_EQ(cart.x, FastMath::FLOAT_EPSILON); // Java modifies the argument when x == 0
	EXPECT_EQ(spherical.x, 3.0f);
	EXPECT_NEAR(spherical.z, FastMath::HALF_PI, 1e-6f);
	Vector3f cartZ(0, 0, 5);
	FastMath::cartesianZToSpherical(cartZ, spherical);
	EXPECT_EQ(spherical.x, 5.0f);
	FastMath::sphericalToCartesianZ(Vector3f(2, 0, 0), store);
	EXPECT_TRUE(sameVector(store, 2, 0, 0));
}

TEST(FastMathTest, HalfFloats) {
	EXPECT_EQ(FastMath::convertFloatToHalf(1.0f), 0x3c00);
	EXPECT_EQ(FastMath::convertFloatToHalf(-2.0f), static_cast<int16_t>(0xc000));
	EXPECT_EQ(FastMath::convertFloatToHalf(0.0f), 0);
	EXPECT_EQ(FastMath::convertFloatToHalf(-0.0f), 0); // flt == 0f catches -0.0f first
	EXPECT_EQ(FastMath::convertFloatToHalf(Inf), 0x7c00);
	EXPECT_EQ(FastMath::convertFloatToHalf(-Inf), static_cast<int16_t>(0xfc00));
	EXPECT_EQ(FastMath::convertFloatToHalf(1e6f), 0x7bff);
	EXPECT_EQ(FastMath::convertFloatToHalf(-1e6f), static_cast<int16_t>(0xfbff));
	EXPECT_EQ(FastMath::convertFloatToHalf(1e-9f), 0x0001);
	EXPECT_EQ(FastMath::convertFloatToHalf(-1e-9f), static_cast<int16_t>(0x8001));
	EXPECT_THROW(FastMath::convertFloatToHalf(NaN), aion::commons::utils::UnsupportedOperationException);
	EXPECT_FLOAT_BITS(FastMath::convertHalfToFloat(0x3c00), 1.0f);
	EXPECT_FLOAT_BITS(FastMath::convertHalfToFloat(static_cast<int16_t>(0x8000)), -0.0f);
	EXPECT_FLOAT_BITS(FastMath::convertHalfToFloat(0x7c00), Inf);
	EXPECT_FLOAT_BITS(FastMath::convertHalfToFloat(static_cast<int16_t>(0xfc00)), -Inf);
	EXPECT_FLOAT_BITS(FastMath::convertHalfToFloat(0x7bff), 65504.0f);
	EXPECT_FLOAT_BITS(FastMath::convertHalfToFloat(static_cast<int16_t>(0xc000)), -2.0f);
}
