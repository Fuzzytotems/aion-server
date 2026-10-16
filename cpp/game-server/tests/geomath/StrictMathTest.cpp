// StrictMath: the fdlibm special cases and dense sweeps against the C runtime (fdlibm and a good libm agree within one ulp; a wrong
// coefficient in the port would show up as a larger error). The bit-exact values are checked in GoldenVectorsTest.StrictMath.

#include "aion/gameserver/geoEngine/math/StrictMath.h"

#include <cmath>
#include <limits>
#include <numbers>

#include <gtest/gtest.h>

#include "GeoMathTestSupport.h"

using namespace aion::gameserver::geoEngine::math;
using namespace aion::gameserver::geoEngine::math::test;

namespace {

constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
constexpr double Inf = std::numeric_limits<double>::infinity();
constexpr double Pi = 0x1.921fb54442d18p+1;
constexpr double HalfPi = 0x1.921fb54442d18p+0;
constexpr double QuarterPi = 0x1.921fb54442d18p-1;

::testing::AssertionResult withinOneUlp(double actual, double expected) {
	if (std::isnan(actual) && std::isnan(expected))
		return ::testing::AssertionSuccess();
	if (actual == expected || std::nextafter(actual, Inf) == expected || std::nextafter(actual, -Inf) == expected)
		return ::testing::AssertionSuccess();
	return ::testing::AssertionFailure() << std::format("{:.17g} vs {:.17g}", actual, expected);
}

} // namespace

TEST(StrictMathTest, SpecialCases) {
	EXPECT_DOUBLE_BITS(StrictMath::acos(1.0), 0.0);
	EXPECT_DOUBLE_BITS(StrictMath::acos(-1.0), Pi);
	EXPECT_DOUBLE_BITS(StrictMath::acos(0.0), HalfPi);
	EXPECT_DOUBLE_BITS(StrictMath::acos(-0.0), HalfPi);
	EXPECT_TRUE(std::isnan(StrictMath::acos(1.0000000000000002)));
	EXPECT_TRUE(std::isnan(StrictMath::acos(NaN)));
	EXPECT_TRUE(std::isnan(StrictMath::acos(-Inf)));

	EXPECT_DOUBLE_BITS(StrictMath::asin(1.0), HalfPi);
	EXPECT_DOUBLE_BITS(StrictMath::asin(-1.0), -HalfPi);
	EXPECT_DOUBLE_BITS(StrictMath::asin(0.0), 0.0);
	EXPECT_DOUBLE_BITS(StrictMath::asin(-0.0), -0.0);
	EXPECT_DOUBLE_BITS(StrictMath::asin(1e-300), 1e-300); // tiny: returned unchanged
	EXPECT_TRUE(std::isnan(StrictMath::asin(-1.5)));

	EXPECT_DOUBLE_BITS(StrictMath::atan(0.0), 0.0);
	EXPECT_DOUBLE_BITS(StrictMath::atan(-0.0), -0.0);
	EXPECT_DOUBLE_BITS(StrictMath::atan(1.0), QuarterPi);
	EXPECT_DOUBLE_BITS(StrictMath::atan(Inf), HalfPi);
	EXPECT_DOUBLE_BITS(StrictMath::atan(-Inf), -HalfPi);
	EXPECT_DOUBLE_BITS(StrictMath::atan(1e300), HalfPi);
	EXPECT_TRUE(std::isnan(StrictMath::atan(NaN)));

	EXPECT_DOUBLE_BITS(StrictMath::atan2(0.0, 0.0), 0.0);
	EXPECT_DOUBLE_BITS(StrictMath::atan2(-0.0, 0.0), -0.0);
	EXPECT_DOUBLE_BITS(StrictMath::atan2(0.0, -0.0), Pi);
	EXPECT_DOUBLE_BITS(StrictMath::atan2(-0.0, -0.0), -Pi);
	EXPECT_DOUBLE_BITS(StrictMath::atan2(0.0, -5.0), Pi);
	EXPECT_DOUBLE_BITS(StrictMath::atan2(5.0, 0.0), HalfPi);
	EXPECT_DOUBLE_BITS(StrictMath::atan2(-5.0, -0.0), -HalfPi);
	EXPECT_DOUBLE_BITS(StrictMath::atan2(Inf, Inf), QuarterPi);
	EXPECT_DOUBLE_BITS(StrictMath::atan2(-Inf, Inf), -QuarterPi);
	EXPECT_DOUBLE_BITS(StrictMath::atan2(Inf, -Inf), 3.0 * QuarterPi);
	EXPECT_DOUBLE_BITS(StrictMath::atan2(-Inf, -Inf), -3.0 * QuarterPi);
	EXPECT_DOUBLE_BITS(StrictMath::atan2(1.0, Inf), 0.0);
	EXPECT_DOUBLE_BITS(StrictMath::atan2(-1.0, Inf), -0.0);
	EXPECT_DOUBLE_BITS(StrictMath::atan2(1.0, -Inf), Pi);
	EXPECT_DOUBLE_BITS(StrictMath::atan2(-1.0, -Inf), -Pi);
	EXPECT_DOUBLE_BITS(StrictMath::atan2(Inf, 1.0), HalfPi);
	EXPECT_DOUBLE_BITS(StrictMath::atan2(-Inf, -1.0), -HalfPi);
	EXPECT_DOUBLE_BITS(StrictMath::atan2(1e300, 1e-300), HalfPi); // |y/x| > 2^60
	EXPECT_DOUBLE_BITS(StrictMath::atan2(1e-300, -1e300), Pi);    // x < 0 and |y/x| < 2^-60: pi - (0 - pi_lo)
	EXPECT_DOUBLE_BITS(StrictMath::atan2(1e-300, 1e300), 0.0);    // x > 0: atan(|y/x|) underflows to 0
	EXPECT_DOUBLE_BITS(StrictMath::atan2(2.0, 1.0), StrictMath::atan(2.0));
	EXPECT_TRUE(std::isnan(StrictMath::atan2(NaN, 1.0)));
	EXPECT_TRUE(std::isnan(StrictMath::atan2(1.0, NaN)));
}

TEST(StrictMathTest, SymmetriesHoldExactly) {
	for (double x = 0.0; x <= 1.0; x += 0.001) {
		SCOPED_TRACE(x);
		EXPECT_DOUBLE_BITS(StrictMath::asin(-x), -StrictMath::asin(x));
		EXPECT_DOUBLE_BITS(StrictMath::atan(-x * 7), -StrictMath::atan(x * 7));
		EXPECT_DOUBLE_BITS(StrictMath::atan2(-x, 2.0), -StrictMath::atan2(x, 2.0));
	}
}

TEST(StrictMathTest, DenseSweepsAgreeWithTheCRuntimeWithinOneUlp) {
	int acosDiffer = 0;
	const int n = 200000;
	for (int i = 0; i <= n; i++) {
		const double x = -1.0 + 2.0 * i / n;
		ASSERT_TRUE(withinOneUlp(StrictMath::acos(x), std::acos(x))) << "acos " << x;
		ASSERT_TRUE(withinOneUlp(StrictMath::asin(x), std::asin(x))) << "asin " << x;
		acosDiffer += StrictMath::acos(x) != std::acos(x);
	}
	for (int i = -n; i <= n; i++) {
		const double x = std::ldexp(static_cast<double>(i) / n, (i % 41) - 20); // |x| from 2^-37 to 2^20
		ASSERT_TRUE(withinOneUlp(StrictMath::atan(x), std::atan(x))) << "atan " << x;
		const double y = std::ldexp(static_cast<double>((i * 7919) % n) / n, ((i * 31) % 23) - 11);
		ASSERT_TRUE(withinOneUlp(StrictMath::atan2(y, x), std::atan2(y, x))) << "atan2 " << y << ", " << x;
	}
	// the float arguments FastMath passes: every 97th float in [-1, 1]
	for (uint32_t bits = 0; bits < 0x3f800000u; bits += 97 * 4099) {
		const double x = static_cast<double>(std::bit_cast<float>(bits));
		ASSERT_TRUE(withinOneUlp(StrictMath::acos(x), std::acos(x))) << "acos " << x;
		ASSERT_TRUE(withinOneUlp(StrictMath::acos(-x), std::acos(-x))) << "acos " << -x;
	}
	// informational: fdlibm and the UCRT are both accurate but not identical in the last bit
	RecordProperty("acosDifferentLastBit", acosDiffer);
}
