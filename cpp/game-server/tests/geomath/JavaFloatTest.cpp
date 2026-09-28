// JavaFloat: java.lang.Float semantics (bits, compare, toString of JDK 19+) used by equals/hashCode/toString of the math classes.

#include "aion/gameserver/geoEngine/math/JavaFloat.h"

#include <cmath>
#include <limits>

#include <gtest/gtest.h>

using aion::gameserver::geoEngine::math::JavaFloat;

namespace {

constexpr float NaN = std::numeric_limits<float>::quiet_NaN();
constexpr float Inf = std::numeric_limits<float>::infinity();

} // namespace

TEST(JavaFloatTest, Bits) {
	EXPECT_EQ(JavaFloat::floatToIntBits(1.0f), 0x3f800000);
	EXPECT_EQ(JavaFloat::floatToIntBits(-0.0f), static_cast<int32_t>(0x80000000u));
	EXPECT_EQ(JavaFloat::floatToIntBits(NaN), 0x7fc00000);
	EXPECT_EQ(JavaFloat::floatToIntBits(-NaN), 0x7fc00000);
	EXPECT_EQ(JavaFloat::floatToIntBits(JavaFloat::intBitsToFloat(0x7f800001)), 0x7fc00000); // signaling NaN canonicalized too
	EXPECT_EQ(JavaFloat::floatToRawIntBits(JavaFloat::intBitsToFloat(0x7f800001)), 0x7f800001);
	EXPECT_EQ(JavaFloat::floatToIntBits(Inf), 0x7f800000);
	EXPECT_EQ(JavaFloat::intBitsToFloat(0x40490fdb), static_cast<float>(3.14159265358979323846));
	static_assert(JavaFloat::floatToIntBits(2.0f) == 0x40000000);
}

TEST(JavaFloatTest, CompareOrdersNegativeZeroAndNaN) {
	EXPECT_EQ(JavaFloat::compare(1.0f, 2.0f), -1);
	EXPECT_EQ(JavaFloat::compare(2.0f, 1.0f), 1);
	EXPECT_EQ(JavaFloat::compare(1.0f, 1.0f), 0);
	EXPECT_EQ(JavaFloat::compare(-0.0f, 0.0f), -1);
	EXPECT_EQ(JavaFloat::compare(0.0f, -0.0f), 1);
	EXPECT_EQ(JavaFloat::compare(NaN, NaN), 0);
	EXPECT_EQ(JavaFloat::compare(NaN, Inf), 1);
	EXPECT_EQ(JavaFloat::compare(-Inf, NaN), -1);
}

TEST(JavaFloatTest, PredicatesAndConversions) {
	EXPECT_TRUE(JavaFloat::isNaN(NaN));
	EXPECT_FALSE(JavaFloat::isNaN(Inf));
	EXPECT_TRUE(JavaFloat::isInfinite(-Inf));
	EXPECT_FALSE(JavaFloat::isInfinite(NaN));
	EXPECT_FALSE(JavaFloat::isInfinite(std::numeric_limits<float>::max()));
	EXPECT_EQ(JavaFloat::signum(-3.0f), -1.0f);
	EXPECT_EQ(JavaFloat::signum(1e-40f), 1.0f);
	EXPECT_TRUE(std::signbit(JavaFloat::signum(-0.0f)));
	EXPECT_TRUE(std::isnan(JavaFloat::signum(NaN)));
	EXPECT_FALSE(std::signbit(JavaFloat::mathAbs(-0.0f)));
	EXPECT_EQ(JavaFloat::doubleToInt(std::nan("")), 0);
	EXPECT_EQ(JavaFloat::doubleToInt(1e10), 2147483647);
	EXPECT_EQ(JavaFloat::doubleToInt(-1e10), -2147483647 - 1);
	EXPECT_EQ(JavaFloat::doubleToInt(-2.9), -2);
	EXPECT_EQ(JavaFloat::doubleToInt(2147483646.99), 2147483646);
}

TEST(JavaFloatTest, ToStringMatchesJava) {
	// special values
	EXPECT_EQ(JavaFloat::toString(NaN), "NaN");
	EXPECT_EQ(JavaFloat::toString(Inf), "Infinity");
	EXPECT_EQ(JavaFloat::toString(-Inf), "-Infinity");
	EXPECT_EQ(JavaFloat::toString(0.0f), "0.0");
	EXPECT_EQ(JavaFloat::toString(-0.0f), "-0.0");
	// plain notation in [10^-3, 10^7)
	EXPECT_EQ(JavaFloat::toString(1.0f), "1.0");
	EXPECT_EQ(JavaFloat::toString(-2.5f), "-2.5");
	EXPECT_EQ(JavaFloat::toString(100.0f), "100.0");
	EXPECT_EQ(JavaFloat::toString(0.1f), "0.1");
	EXPECT_EQ(JavaFloat::toString(0.3f), "0.3");
	EXPECT_EQ(JavaFloat::toString(1.0f / 3.0f), "0.33333334");
	EXPECT_EQ(JavaFloat::toString(0.001f), "0.001");
	EXPECT_EQ(JavaFloat::toString(0.00123f), "0.00123");
	EXPECT_EQ(JavaFloat::toString(3.14159265f), "3.1415927");
	EXPECT_EQ(JavaFloat::toString(255.49063f), "255.49063");
	EXPECT_EQ(JavaFloat::toString(123456.789f), "123456.79");
	EXPECT_EQ(JavaFloat::toString(9999999.0f), "9999999.0");
	EXPECT_EQ(JavaFloat::toString(-65504.0f), "-65504.0");
	// computerized scientific notation outside that range
	EXPECT_EQ(JavaFloat::toString(1e7f), "1.0E7");
	EXPECT_EQ(JavaFloat::toString(16777216.0f), "1.6777216E7");
	EXPECT_EQ(JavaFloat::toString(12345678.0f), "1.2345678E7");
	EXPECT_EQ(JavaFloat::toString(1e10f), "1.0E10");
	EXPECT_EQ(JavaFloat::toString(1e-4f), "1.0E-4");
	EXPECT_EQ(JavaFloat::toString(1e-5f), "1.0E-5");
	EXPECT_EQ(JavaFloat::toString(std::numeric_limits<float>::max()), "3.4028235E38");
	EXPECT_EQ(JavaFloat::toString(-std::numeric_limits<float>::max()), "-3.4028235E38");
	// a single digit would round-trip, Java prints the closest two digits
	EXPECT_EQ(JavaFloat::toString(std::numeric_limits<float>::denorm_min()), "1.4E-45");
	EXPECT_EQ(JavaFloat::toString(7 * std::numeric_limits<float>::denorm_min()), "9.8E-45");
}
