// Vector2f: hand-derived values and the GeoMap.findMovementCollision usage.

#include "aion/gameserver/geoEngine/math/Vector2f.h"

#include <cmath>
#include <limits>
#include <type_traits>

#include <gtest/gtest.h>

#include "aion/gameserver/geoEngine/math/FastMath.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"

#include "GeoMathTestSupport.h"

using namespace aion::gameserver::geoEngine::math;
using namespace aion::gameserver::geoEngine::math::test;

TEST(Vector2fTest, ValueTypeAndConstants) {
	EXPECT_TRUE(std::is_trivially_copyable_v<Vector2f>);
	EXPECT_EQ(Vector2f::ZERO.x, 0.0f);
	EXPECT_EQ(Vector2f::UNIT_XY.y, 1.0f);
	const Vector2f v;
	EXPECT_EQ(v.x, 0.0f);
	EXPECT_EQ(v.y, 0.0f);
}

TEST(Vector2fTest, GeoMapFindMovementCollision) {
	// Vector2f xyOffset = targetXY.subtract(origin.getX(), origin.getY()).normalizeLocal().multLocal(COLLISION_CHECK_Z_OFFSET)
	const Vector2f targetXY(13, 14);
	const Vector3f origin(10, 10, 0);
	const Vector2f xyOffset = targetXY.subtract(origin.getX(), origin.getY()).normalizeLocal().multLocal(1.0f);
	// length 5: divides (not multiplies by the reciprocal) -> 3/5 = 0.6f, 4/5 = 0.8f
	EXPECT_EQ(xyOffset.getX(), 0.6f);
	EXPECT_EQ(xyOffset.getY(), 0.8f);
	// zero length: divided by 1, stays zero
	Vector2f zero;
	EXPECT_EQ(zero.normalizeLocal().x, 0.0f);
	EXPECT_EQ(Vector2f(0, 0).normalize().y, 0.0f);
}

TEST(Vector2fTest, HandDerivedValues) {
	const Vector2f a(3, 4);
	const Vector2f b(-4, 3);
	EXPECT_EQ(a.dot(b), 0.0f);
	EXPECT_EQ(a.determinant(b), 25.0f); // 3*3 - 4*(-4)
	EXPECT_TRUE(sameVector(a.cross(b), 0, 0, 25));
	EXPECT_EQ(a.length(), 5.0f);
	EXPECT_EQ(a.lengthSquared(), 25.0f);
	EXPECT_EQ(a.distanceSquared(b), 50.0f);
	EXPECT_EQ(a.distance(b), static_cast<float>(std::sqrt(50.0)));
	EXPECT_EQ(a.distanceSquared(0, 0), 25.0f);
	EXPECT_FLOAT_BITS(Vector2f(0, 1).getAngle(), -FastMath::HALF_PI);
	EXPECT_FLOAT_BITS(Vector2f(1, 0).angleBetween(Vector2f(0, 1)), FastMath::HALF_PI);
	EXPECT_FLOAT_BITS(Vector2f(1, 0).smallestAngleBetween(Vector2f(-1, 0)), FastMath::PI);
	EXPECT_EQ(a.add(b).x, -1.0f);
	EXPECT_EQ(a.subtract(b).y, 1.0f);
	EXPECT_EQ(a.mult(2).y, 8.0f);
	EXPECT_EQ(a.divide(2).x, 1.5f);
	EXPECT_EQ(a.negate().x, -3.0f);
	Vector2f interpolated(0, 10);
	interpolated.interpolate(Vector2f(10, 20), 0.5f);
	EXPECT_EQ(interpolated.x, 5.0f);
	EXPECT_EQ(interpolated.y, 15.0f);
}

TEST(Vector2fTest, EqualsHashCodeToString) {
	EXPECT_TRUE(Vector2f(1, 2).equals(Vector2f(1, 2)));
	EXPECT_FALSE(Vector2f(0.0f, 1).equals(Vector2f(-0.0f, 1)));
	const float nan = std::numeric_limits<float>::quiet_NaN();
	EXPECT_TRUE(Vector2f(nan, 1) == Vector2f(nan, 1));
	EXPECT_EQ(Vector2f::ZERO.hashCode(), 53428); // 37 -> 1406 -> 53428
	EXPECT_EQ(Vector2f(1, -0.5f).toString(), "(1.0, -0.5)");
	EXPECT_FALSE(Vector2f::isValidVector(Vector2f(nan, 0)));
	EXPECT_FALSE(Vector2f::isValidVector(nullptr));
	EXPECT_TRUE(Vector2f::isValidVector(Vector2f(1, 1)));
}

TEST(Vector2fTest, RotateAroundOrigin) {
	Vector2f v(1, 0);
	v.rotateAroundOrigin(FastMath::HALF_PI, false);
	const float c = static_cast<float>(std::cos(static_cast<double>(FastMath::HALF_PI)));
	EXPECT_FLOAT_BITS(v.x, c); // cos(HALF_PI) * 1 - sin(HALF_PI) * 0
	EXPECT_EQ(v.y, 1.0f);
	Vector2f w(1, 0);
	w.rotateAroundOrigin(FastMath::HALF_PI, true); // clockwise: negated angle
	EXPECT_EQ(w.y, -1.0f);
}
