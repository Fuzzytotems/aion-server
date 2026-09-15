// P4-05 PositionUtil against a float32 oracle (handlers-and-porting-plan.md §3.2): the expected values come from an independent Python model of
// Java's float/double promotions (every float operation rounded to float32 with struct, math.atan2, the JDK 9+ toDegrees constant), printed as
// hexadecimal literals so the comparison is bit-exact. The object overloads delegate to these float overloads.

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>

#include "aion/gameserver/model/templates/zone/Point2D.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::utils {
namespace {

struct AngleFromVector {
	float x1, y1, x2, y2;
	float angle;
	double distance2d;
	int8_t heading;
};

// x1, y1, x2, y2, calculateAngleFrom, getDistance (2D), getHeadingTowards
constexpr std::array ANGLE_FROM_VECTORS{
	AngleFromVector{0.0f, 0.0f, 0x1.0p+0f, 0x1.0p+0f, 0x1.68p+5f, 0x1.6a09e667f3bcdp+0, 15},
	AngleFromVector{0.0f, 0.0f, -0x1.0p+0f, 0.0f, 0x1.68p+7f, 0x1.0p+0, 60},
	AngleFromVector{0.0f, 0.0f, 0.0f, -0x1.0p+0f, 0x1.0ep+8f, 0x1.0p+0, 90},
	AngleFromVector{0x1.92p+6f, 0x1.908p+7f, 0x1.92p+6f, 0x1.908p+7f, 0.0f, 0.0, 0},
	AngleFromVector{0x1.0a199ap+11f, 0x1.e11334p+10f, 0x1.0a6334p+11f, 0x1.e02cccp+10f, 0x1.2e92ecp+8f, 0x1.116a4197bb54ap+2, 100},
	AngleFromVector{-0x1.4p+2f, 0x1.8p+1f, 0x1.cp+2f, -0x1.2p+3f, 0x1.3bp+8f, 0x1.0f876ccdf6cd9p+4, 105},
	AngleFromVector{0x1.d40b08p+8f, 0x1.409df2p+10f, 0x1.e3cd4ap+10f, 0x1.8bb95ep+9f, 0x1.557efcp+8f, 0x1.82c993a80ee63p+10, 113},
	AngleFromVector{0x1.ef90f4p+11f, 0x1.d6dfbcp+11f, 0x1.79ba4ap+11f, 0x1.659fap+11f, 0x1.bfb9a4p+7f, 0x1.46dfa073daee9p+10, 74},
	AngleFromVector{0x1.cfb202p+11f, 0x1.8d512p+11f, 0x1.251dfcp+8f, 0x1.868856p+11f, 0x1.69d1fep+7f, 0x1.ab1c0d779f368p+11, 60},
	AngleFromVector{0x1.844b68p+8f, 0x1.06886ap+10f, 0x1.7fd674p+11f, 0x1.27972ep+10f, 0x1.693bf2p+1f, 0x1.4fb5431dcff38p+11, 0},
	AngleFromVector{0x1.1b6c24p+11f, 0x1.d6d1aap+11f, 0x1.b6b83cp+11f, 0x1.c01796p+10f, 0x1.2e2ec6p+8f, 0x1.239261c7d6f72p+11, 100},
	AngleFromVector{0x1.8c933ap+10f, 0x1.2df7eep+11f, 0x1.a2b6p+10f, 0x1.e7f4a4p+11f, 0x1.5a60aap+6f, 0x1.74a1e3a7ddbb3p+10, 28},
	AngleFromVector{0x1.5319aep+11f, 0x1.4f7c82p+11f, 0x1.814b04p+11f, 0x1.77f568p+10f, 0x1.1f6364p+8f, 0x1.3524313f02d1ap+10, 95},
	AngleFromVector{0x1.bd25d8p+9f, 0x1.bf82e8p+9f, 0x1.2dec4ap+11f, 0x1.14c3acp+11f, 0x1.46dc1p+5f, 0x1.f81a119c8cf32p+10, 13},
	AngleFromVector{0x1.dda60cp+11f, 0x1.df7246p+11f, 0x1.a93a84p+11f, 0x1.420706p+6f, 0x1.07a0bp+8f, 0x1.d84d11928e45ep+11, 87},
	AngleFromVector{0x1.e9831p+7f, 0x1.f27f64p+9f, 0x1.29c44cp+11f, 0x1.aa67a2p+10f, 0x1.257a54p+4f, 0x1.19791c8feff12p+11, 6},
	AngleFromVector{0x1.ee2358p+11f, 0x1.4df3bp+10f, 0x1.4c4f34p+9f, 0x1.76b912p+9f, 0x1.7c3854p+7f, 0x1.a18b4ccce520dp+11, 63},
	AngleFromVector{0x1.978a46p+11f, 0x1.0f68a4p+11f, 0x1.f230ap+5f, 0x1.3f9ac8p+11f, 0x1.5a4036p+7f, 0x1.92a698b417fe4p+11, 57},
	AngleFromVector{0x1.2fbe96p+9f, 0x1.34c268p+11f, 0x1.82fd4cp+11f, 0x1.9dc54cp+11f, 0x1.2a795ep+4f, 0x1.484d0ccdc613ep+11, 6},
	AngleFromVector{0x1.70d6a6p+8f, 0x1.0ef3a4p+6f, 0x1.05e468p+11f, 0x1.13773p+11f, 0x1.987084p+5f, 0x1.574bd607cbf02p+11, 17},
	AngleFromVector{0x1.9b9252p+10f, 0x1.e4c932p+11f, 0x1.241cc2p+11f, 0x1.65b1ecp+10f, 0x1.1dc1e8p+8f, 0x1.3de271befa742p+11, 95},
	AngleFromVector{0x1.44d45ap+11f, 0x1.71ba9ep+11f, 0x1.8519f4p+11f, 0x1.b6f49ep+11f, 0x1.79010ap+5f, 0x1.79d9bfc1fc6fp+9, 15},
	AngleFromVector{0x1.d697p+10f, 0x1.9b1932p+10f, 0x1.518e1ep+10f, 0x1.28eb78p+10f, 0x1.b946c8p+7f, 0x1.5ea0c0c08bc57p+9, 73},
	AngleFromVector{0x1.807846p+9f, 0x1.66eddp+9f, 0x1.65c318p+9f, 0x1.7f057ap+11f, 0x1.6d3774p+6f, 0x1.255d7a43401bap+11, 30},
	AngleFromVector{0x1.726564p+9f, 0x1.ebe8f4p+8f, 0x1.011fdp+11f, 0x1.b24622p+11f, 0x1.08bea8p+6f, 0x1.977a122cdea54p+11, 22},
	AngleFromVector{0x1.59ff4ep+10f, 0x1.65fe1ep+9f, 0x1.798c1p+11f, 0x1.dbce2p+8f, 0x1.5fa66ap+8f, 0x1.9d7ae1f115531p+10, 117},
	AngleFromVector{0x1.2aba6p+10f, 0x1.8c5694p+10f, 0x1.e2d1a6p+10f, 0x1.3b515ep+11f, 0x1.9ebdfep+5f, 0x1.29f7be98d8d88p+10, 17},
	AngleFromVector{0x1.75c148p+11f, 0x1.a360f6p+11f, 0x1.7a53dap+9f, 0x1.8f37d6p+8f, 0x1.d1d9ecp+7f, 0x1.cf1689ae2e731p+11, 77},
	AngleFromVector{0x1.96fb4p+11f, 0x1.40e276p+6f, 0x1.4bad0ep+9f, 0x1.a50fdap+8f, 0x1.590536p+7f, 0x1.46d9e3294480ap+11, 57},
	AngleFromVector{0x1.6240f4p+8f, 0x1.8ec748p+11f, 0x1.59c13ep+11f, 0x1.742f42p+10f, 0x1.44cc24p+8f, 0x1.70f247e58fed8p+11, 108},
};

struct AngleTowardsVector {
	float x, y;
	int8_t heading;
	float targetX, targetY;
	float angle;
};

// x, y, heading, targetX, targetY, calculateAngleTowards
constexpr std::array ANGLE_TOWARDS_VECTORS{
	AngleTowardsVector{0.0f, 0.0f, -128, 0x1.0p+0f, 0x1.0p+0f, -0x1.14p+6f},
	AngleTowardsVector{0.0f, 0.0f, -60, -0x1.0p+0f, 0.0f, 0.0f},
	AngleTowardsVector{0.0f, 0.0f, -1, 0.0f, -0x1.0p+0f, 0x1.5cp+6f},
	AngleTowardsVector{0x1.92p+6f, 0x1.908p+7f, 0, 0x1.92p+6f, 0x1.908p+7f, 0.0f},
	AngleTowardsVector{0x1.0a199ap+11f, 0x1.e11334p+10f, 1, 0x1.0a6334p+11f, 0x1.e02cccp+10f, 0x1.e368ap+5f},
	AngleTowardsVector{-0x1.4p+2f, 0x1.8p+1f, 30, 0x1.cp+2f, -0x1.2p+3f, 0x1.0ep+7f},
	AngleTowardsVector{0x1.d40b08p+8f, 0x1.409df2p+10f, 45, 0x1.e3cd4ap+10f, 0x1.8bb95ep+9f, 0x1.330208p+7f},
	AngleTowardsVector{0x1.ef90f4p+11f, 0x1.d6dfbcp+11f, 60, 0x1.79ba4ap+11f, 0x1.659fap+11f, -0x1.5ee69p+5f},
	AngleTowardsVector{0x1.cfb202p+11f, 0x1.8d512p+11f, 90, 0x1.251dfcp+8f, 0x1.868856p+11f, 0x1.645c04p+6f},
	AngleTowardsVector{0x1.844b68p+8f, 0x1.06886ap+10f, 119, 0x1.7fd674p+11f, 0x1.27972ep+10f, -0x1.749ep+2f},
	AngleTowardsVector{0x1.1b6c24p+11f, 0x1.d6d1aap+11f, 120, 0x1.b6b83cp+11f, 0x1.c01796p+10f, 0x1.ce89dp+5f},
	AngleTowardsVector{0x1.8c933ap+10f, 0x1.2df7eep+11f, 127, 0x1.a2b6p+10f, 0x1.e7f4a4p+11f, -0x1.0660aap+6f},
};

struct NormalizeVector {
	float angle;
	float normalized;
};

// angle, normalizeAngle
constexpr std::array NORMALIZE_VECTORS{
	NormalizeVector{0.0f, 0.0f},
	NormalizeVector{0x1.67fd7p+8f, 0x1.67fd7p+8f},
	NormalizeVector{0x1.68p+8f, 0.0f},
	NormalizeVector{0x1.684p+9f, 0x1.0p-1f},
	NormalizeVector{-0x1.0p-1f, 0x1.678p+8f},
	NormalizeVector{-0x1.68p+8f, -0.0f},
	NormalizeVector{-0x1.6aap+9f, 0x1.62cp+8f},
	NormalizeVector{0x1.312dp+23f, 0x1.18p+8f},
	NormalizeVector{-0x1.312dp+23f, 0x1.4p+6f},
};

TEST(PositionUtilTest, CalculateAngleFromMatchesFloat32Oracle) {
	for (const AngleFromVector& v : ANGLE_FROM_VECTORS) {
		EXPECT_EQ(std::bit_cast<uint32_t>(PositionUtil::calculateAngleFrom(v.x1, v.y1, v.x2, v.y2)), std::bit_cast<uint32_t>(v.angle))
			<< v.x1 << " " << v.y1 << " " << v.x2 << " " << v.y2;
		EXPECT_EQ(PositionUtil::getDistance(v.x1, v.y1, v.x2, v.y2), v.distance2d);
		EXPECT_EQ(PositionUtil::getHeadingTowards(v.x1, v.y1, v.x2, v.y2), v.heading);
	}
}

TEST(PositionUtilTest, CalculateAngleTowardsMatchesFloat32Oracle) {
	for (const AngleTowardsVector& v : ANGLE_TOWARDS_VECTORS) {
		EXPECT_EQ(PositionUtil::calculateAngleTowards(v.x, v.y, v.heading, v.targetX, v.targetY), v.angle)
			<< v.x << " " << v.y << " " << static_cast<int>(v.heading);
	}
}

TEST(PositionUtilTest, NormalizeAngleMatchesFloat32Oracle) {
	for (const NormalizeVector& v : NORMALIZE_VECTORS)
		EXPECT_EQ(std::bit_cast<uint32_t>(PositionUtil::normalizeAngle(v.angle)), std::bit_cast<uint32_t>(v.normalized)) << v.angle;
}

TEST(PositionUtilTest, HeadingConversions) {
	EXPECT_EQ(PositionUtil::convertHeadingToAngle(0), 0.0f);
	EXPECT_EQ(PositionUtil::convertHeadingToAngle(30), 90.0f);
	EXPECT_EQ(PositionUtil::convertHeadingToAngle(-10), 330.0f);
	EXPECT_EQ(PositionUtil::convertHeadingToAngle(-128), 336.0f); // -384 -> fmod -24 -> 336
	EXPECT_EQ(PositionUtil::convertAngleToHeading(359.9f), 119);
	EXPECT_EQ(PositionUtil::convertAngleToHeading(-3.5f), -1); // (byte) (int) -1.1666666f
	EXPECT_EQ(PositionUtil::convertAngleToHeading(500.0f), -90); // (byte) 166
	EXPECT_EQ(PositionUtil::convertAngleToHeading(std::nanf("")), 0);
	EXPECT_EQ(PositionUtil::convertAngleToHeading(1e20f), -1); // (byte) Integer.MAX_VALUE
}

TEST(PositionUtilTest, DistancesAndRanges) {
	EXPECT_EQ(PositionUtil::getDistance(1.0f, 2.0f, 3.0f, 4.0f, 6.0f, 3.0f), 5.0);
	EXPECT_EQ(PositionUtil::getDistance(nullptr, nullptr), 0.0);
	EXPECT_TRUE(PositionUtil::isInRange(0, 0, 0, 3, 4, 0, 5.0001f));
	EXPECT_FALSE(PositionUtil::isInRange(0, 0, 0, 3, 4, 0, 5.0f)); // strict comparison of squares
	// float arithmetic: 16777217 is not representable in float, the difference is computed after rounding
	EXPECT_EQ(PositionUtil::getDistance(16777216.0f, 0.0f, 16777217.0f, 0.0f), 0.0);
}

TEST(PositionUtilTest, ClosestPointOnSegment) {
	model::templates::zone::Point2D inside = PositionUtil::getClosestPointOnSegment(0, 0, 10, 0, 4, 7);
	EXPECT_EQ(inside.getX(), 4.0f);
	EXPECT_EQ(inside.getY(), 0.0f);
	model::templates::zone::Point2D beforeStart = PositionUtil::getClosestPointOnSegment(0, 0, 10, 0, -4, 7);
	EXPECT_EQ(beforeStart.getX(), 0.0f);
	model::templates::zone::Point2D afterEnd = PositionUtil::getClosestPointOnSegment(0, 0, 10, 10, 20, 11);
	EXPECT_EQ(afterEnd.getX(), 10.0f);
	EXPECT_EQ(afterEnd.getY(), 10.0f);
	model::templates::zone::Point2D diagonal = PositionUtil::getClosestPointOnSegment(0, 0, 10, 10, 10, 0);
	EXPECT_EQ(diagonal.getX(), 5.0f);
	EXPECT_EQ(diagonal.getY(), 5.0f);
	try {
		PositionUtil::getClosestPointOnSegment(1, 1, 1, 1, 0, 0);
		FAIL() << "expected IllegalArgumentException";
	} catch (const runtime::IllegalArgumentException& e) {
		EXPECT_STREQ(e.what(), "Segment start equals segment end");
	}
}

} // namespace
} // namespace aion::gameserver::utils
