// Vector3f: hand-derived values, Java edge cases and every method the game server uses outside the math package (usage survey in the
// P4-03 report: GeoMap, BIHNode, BIHTree, BoundingBox, Terrain, Plane3D, observers, SM_PLAYER_INFO, effects, AI and handlers).

#include "aion/gameserver/geoEngine/math/Vector3f.h"

#include <cmath>
#include <limits>
#include <type_traits>
#include <unordered_set>

#include <gtest/gtest.h>

#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/geoEngine/math/FastMath.h"

#include "GeoMathTestSupport.h"

using namespace aion::gameserver::geoEngine::math;
using namespace aion::gameserver::geoEngine::math::test;

namespace {

constexpr float NaN = std::numeric_limits<float>::quiet_NaN();
constexpr float Inf = std::numeric_limits<float>::infinity();

} // namespace

TEST(Vector3fTest, IsATriviallyCopyableValueType) {
	// required by Field<Vector3f> (runtime/fields/Field.h) and for copies in packets and captures
	EXPECT_TRUE(std::is_trivially_copyable_v<Vector3f>);
	EXPECT_EQ(sizeof(Vector3f), 3 * sizeof(float));
	constexpr Vector3f v(1, 2, 3);
	static_assert(v.getY() == 2.0f);
	const Vector3f zero;
	EXPECT_TRUE(sameVector(zero, 0.0f, 0.0f, 0.0f));
}

TEST(Vector3fTest, Constants) {
	EXPECT_TRUE(sameVector(Vector3f::ZERO, 0, 0, 0));
	EXPECT_TRUE(sameVector(Vector3f::UNIT_X, 1, 0, 0));
	EXPECT_TRUE(sameVector(Vector3f::UNIT_Y, 0, 1, 0));
	EXPECT_TRUE(sameVector(Vector3f::UNIT_Z, 0, 0, 1));
	EXPECT_TRUE(sameVector(Vector3f::UNIT_XYZ, 1, 1, 1));
	EXPECT_TRUE(sameVector(Vector3f::POSITIVE_INFINITY, Inf, Inf, Inf));
	EXPECT_TRUE(sameVector(Vector3f::NEGATIVE_INFINITY, -Inf, -Inf, -Inf));
	EXPECT_TRUE(std::isnan(Vector3f::NOT_A_NUMBER.x) && std::isnan(Vector3f::NOT_A_NUMBER.y) && std::isnan(Vector3f::NOT_A_NUMBER.z));
}

TEST(Vector3fTest, HandDerivedArithmetic) {
	const Vector3f a(1, 2, 3);
	const Vector3f b(4, 5, 6);
	EXPECT_FLOAT_BITS(a.dot(b), 32.0f);
	EXPECT_TRUE(sameVector(a.cross(b), -3, 6, -3)); // (2*6-3*5, 3*4-1*6, 1*5-2*4)
	EXPECT_TRUE(sameVector(Vector3f::UNIT_X.cross(Vector3f::UNIT_Y), 0, 0, 1));
	EXPECT_TRUE(sameVector(a.add(b), 5, 7, 9));
	EXPECT_TRUE(sameVector(a.add(1, 1, 1), 2, 3, 4));
	EXPECT_TRUE(sameVector(b.subtract(a), 3, 3, 3));
	EXPECT_TRUE(sameVector(b.subtract(1, 2, 3), 3, 3, 3));
	EXPECT_TRUE(sameVector(a.mult(2), 2, 4, 6));
	EXPECT_TRUE(sameVector(a.mult(b), 4, 10, 18));
	EXPECT_TRUE(sameVector(b.divide(Vector3f(2, 5, 3)), 2, 1, 2));
	EXPECT_TRUE(sameVector(a.negate(), -1, -2, -3));
	EXPECT_FLOAT_BITS(Vector3f(3, 4, 12).length(), 13.0f);
	EXPECT_FLOAT_BITS(Vector3f(3, 4, 12).lengthSquared(), 169.0f);
	EXPECT_FLOAT_BITS(a.distance(Vector3f(4, 6, 15)), 13.0f);
	// 1/sqrt(25) = 0.2f; 3 * 0.2f rounds to 0.6f, 4 * 0.2f is exact
	EXPECT_TRUE(sameVector(Vector3f(3, 4, 0).normalize(), 0.6f, 0.8f, 0.0f));
	// divide multiplies by the reciprocal: 1/3f = 0.33333334f, 3 * 0.33333334f rounds to 1
	EXPECT_TRUE(sameVector(Vector3f(3, 6, 9).divide(3), 1.0f, 2.0f, 3.0f));
	// project onto (0, 0, 2): n = 6, d = 4, normalized (0, 0, 1) * 1.5
	EXPECT_TRUE(sameVector(a.project(Vector3f(0, 0, 2)), 0, 0, 1.5f));
	EXPECT_FLOAT_BITS(Vector3f::UNIT_X.angleBetween(Vector3f::UNIT_Y), FastMath::HALF_PI);
	EXPECT_FLOAT_BITS(Vector3f::UNIT_X.angleBetween(Vector3f::UNIT_X), 0.0f);
	EXPECT_FLOAT_BITS(Vector3f::UNIT_X.angleBetween(Vector3f(-1, 0, 0)), FastMath::PI);
}

TEST(Vector3fTest, DistanceSquaredIsEvaluatedInDouble) {
	// Java: double dx = x - v.x; ...; (float) (dx * dx + dy * dy + dz * dz)
	const float c = 1.0f + 0x1p-12f;
	const Vector3f v(c, c, c);
	// float: each square (1 + 2^-11 + 2^-24) ties to even 1 + 2^-11, the sum is exact
	EXPECT_FLOAT_BITS(v.lengthSquared(), 3.0f + 3 * 0x1p-11f);
	// double: the sum 3 + 3*2^-11 + 3*2^-24 is rounded once, 0.75 ulp above the float sum -> one ulp up
	EXPECT_FLOAT_BITS(v.distanceSquared(Vector3f::ZERO), 3.0f + 3 * 0x1p-11f + 0x1p-22f);
	EXPECT_FLOAT_BITS(Vector3f(1e30f, 0, 0).distanceSquared(Vector3f::ZERO), Inf); // 1e60 overflows the float cast
}

TEST(Vector3fTest, NormalizeKeepsZeroAndUnitLengthVectors) {
	Vector3f zero;
	EXPECT_TRUE(sameVector(zero.normalizeLocal(), 0, 0, 0));
	EXPECT_TRUE(sameVector(Vector3f::ZERO.normalize(), 0, 0, 0));
	// NaN passes the != checks and poisons all components, infinity gives NaN (inf * 0) and 0
	Vector3f withNaN(NaN, 1, 1);
	withNaN.normalizeLocal();
	EXPECT_TRUE(std::isnan(withNaN.x) && std::isnan(withNaN.y) && std::isnan(withNaN.z));
	Vector3f infinite(Inf, 1, 0);
	infinite.normalizeLocal(); // 1 / sqrt(inf) = 0 -> inf * 0 = NaN, 1 * 0 = 0
	EXPECT_TRUE(std::isnan(infinite.x));
	EXPECT_FLOAT_BITS(infinite.y, 0.0f);
}

TEST(Vector3fTest, EqualsAndHashCodeFollowJava) {
	EXPECT_TRUE(Vector3f(1, 2, 3).equals(Vector3f(1, 2, 3)));
	EXPECT_FALSE(Vector3f(1, 2, 3).equals(Vector3f(1, 2, 4)));
	EXPECT_FALSE(Vector3f(0.0f, 0, 0).equals(Vector3f(-0.0f, 0, 0))); // Float.compare
	EXPECT_TRUE(Vector3f(NaN, 0, 0).equals(Vector3f(-NaN, 0, 0)));    // floatToIntBits canonicalizes NaN
	EXPECT_TRUE(Vector3f(NaN, 0, 0) == Vector3f(NaN, 0, 0));
	EXPECT_TRUE(Vector3f::ZERO.equals(Vector3f::ZERO));
	// hash = 37; hash += 37 * hash + floatToIntBits(c) per component, int arithmetic
	EXPECT_EQ(Vector3f::ZERO.hashCode(), 2030264);     // 37 -> 1406 -> 53428 -> 2030264
	EXPECT_EQ(Vector3f::UNIT_X.hashCode(), 773782200); // wraps: 38 * 1065354622 mod 2^32 ...
	// the sign bit of x is multiplied by 38 twice and wraps away (as in Java), the one of z survives
	EXPECT_EQ(Vector3f(-0.0f, 0, 0).hashCode(), Vector3f::ZERO.hashCode());
	EXPECT_EQ(Vector3f(0, 0, -0.0f).hashCode(), static_cast<int32_t>(2030264u + 0x80000000u));
	EXPECT_EQ(Vector3f(NaN, 0, 0).hashCode(), Vector3f(-NaN, 0, 0).hashCode());
	std::unordered_set<Vector3f> set = {Vector3f(1, 2, 3), Vector3f(1, 2, 3), Vector3f(-0.0f, 0, 0), Vector3f::ZERO};
	EXPECT_EQ(set.size(), 3u);
}

TEST(Vector3fTest, ToStringUsesJavaFloatFormatting) {
	EXPECT_EQ(Vector3f(1, -2.5f, 255.49063f).toString(), "(1.0, -2.5, 255.49063)");
	EXPECT_EQ(Vector3f(NaN, Inf, -0.0f).toString(), "(NaN, Infinity, -0.0)");
	EXPECT_EQ(Vector3f(1e7f, 0.001f, 1e-4f).toString(), "(1.0E7, 0.001, 1.0E-4)");
}

TEST(Vector3fTest, IndexedAccessThrowsLikeJava) {
	Vector3f v(1, 2, 3);
	EXPECT_FLOAT_BITS(v.get(0), 1.0f);
	EXPECT_FLOAT_BITS(v.get(1), 2.0f);
	EXPECT_FLOAT_BITS(v.get(2), 3.0f);
	EXPECT_THROW(v.get(3), aion::commons::utils::IllegalArgumentException);
	EXPECT_THROW(v.get(-1), aion::commons::utils::IllegalArgumentException);
	v.set(0, 7); // BIHTree: min.set(axis, value)
	v.set(2, 9);
	EXPECT_TRUE(sameVector(v, 7, 2, 9));
	EXPECT_THROW(v.set(3, 1.0f), aion::commons::utils::IllegalArgumentException);
}

TEST(Vector3fTest, IsValidVector) {
	EXPECT_TRUE(Vector3f::isValidVector(Vector3f(1, 2, 3)));
	EXPECT_FALSE(Vector3f::isValidVector(nullptr));
	EXPECT_FALSE(Vector3f::isValidVector(Vector3f(NaN, 0, 0)));
	EXPECT_FALSE(Vector3f::isValidVector(Vector3f(0, -Inf, 0)));
	EXPECT_FALSE(Vector3f::isValidVector(Vector3f(0, 0, Inf)));
	EXPECT_TRUE(Vector3f::isValidVector(&Vector3f::ZERO));
}

TEST(Vector3fTest, MinMaxLocalKeepThisOnNaN) {
	Vector3f v(1, 5, NaN);
	v.maxLocal(Vector3f(3, 2, 4)); // other.z > NaN is false: keeps NaN
	EXPECT_TRUE(sameVector(v, 3, 5, NaN));
	Vector3f w(1, 5, 2);
	w.minLocal(Vector3f(NaN, 2, -1));
	EXPECT_TRUE(sameVector(w, 1, 2, -1));
}

TEST(Vector3fTest, GettersSettersAndChains) {
	Vector3f origin(10, 20, 30);
	// GeoMap: return origin.setZ(origin.getZ() - COLLISION_CHECK_Z_OFFSET)
	Vector3f& chained = origin.setZ(origin.getZ() - 1.0f);
	EXPECT_EQ(&chained, &origin);
	EXPECT_FLOAT_BITS(origin.getZ(), 29.0f);
	EXPECT_TRUE(sameVector(origin.setX(1).setY(2), 1, 2, 29));
	EXPECT_EQ(&origin.set(4, 5, 6), &origin);
	Vector3f other;
	EXPECT_TRUE(sameVector(other.set(origin), 4, 5, 6));
	EXPECT_TRUE(sameVector(other.zero(), 0, 0, 0));
	EXPECT_EQ(Vector3f(1, 2, 3).toArray(), (std::array<float, 3>{1, 2, 3}));
	const Vector3f copy = origin.clone(); // Java clone: a copy
	origin.x = 100;
	EXPECT_FLOAT_BITS(copy.x, 4.0f);
}

TEST(Vector3fTest, UsagePatternsOfTheGameServer) {
	// AbstractCollisionObserver: Float limit = pos.distance(dir); dir.subtractLocal(pos).normalizeLocal();
	Vector3f pos(100, 200, 50);
	Vector3f dir(100, 200, 40);
	const float limit = pos.distance(dir);
	dir.subtractLocal(pos).normalizeLocal();
	EXPECT_FLOAT_BITS(limit, 10.0f);
	EXPECT_TRUE(sameVector(dir, 0, 0, -1));

	// SM_PLAYER_INFO: new Vector3f(dx, dy, dz).normalizeLocal().multLocal(movementSpeed)
	const Vector3f velocity = Vector3f(3, 0, 4).normalizeLocal().multLocal(5.0f);
	EXPECT_TRUE(sameVector(velocity, 3.0f, 0.0f, 4.0f));
	// 1/sqrt(25) = 0.2f; 3 * 0.2f = 0.6f (0x3f19999a); 0.6f * 5 is exactly halfway between 3 and its successor: ties to even 3

	// BoundingBox.collideWithRay: new Vector3f(ray.direction).multLocal(t[0]).addLocal(ray.origin)
	const Vector3f contact = Vector3f(Vector3f(0, 0, -1)).multLocal(2.5f).addLocal(Vector3f(1, 1, 10));
	EXPECT_TRUE(sameVector(contact, 1, 1, 7.5f));

	// BIHTree.sortTriangles: v1.addLocal(v2).addLocal(v3).multLocal(FastMath.ONE_THIRD); if (v1.get(axis) > split)
	Vector3f v1(0, 0, 0), v2(3, 0, 0), v3(0, 6, 0);
	v1.addLocal(v2).addLocal(v3).multLocal(FastMath::ONE_THIRD);
	EXPECT_TRUE(sameVector(v1, 1.0f, 2.0f, 0.0f));         // ONE_THIRD = 0.33333334f; 3 * ONE_THIRD and 6 * ONE_THIRD round back
	EXPECT_TRUE(Vector3f(0, 0, 0).equals(Vector3f::ZERO)); // BIHTree: exteriorExt.equals(Vector3f.ZERO)

	// BIHNode: planeNormal = v2.subtractLocal(v1).crossLocal(v3.subtractLocal(v1)).normalizeLocal(); angleBetween(UNIT_Z)
	Vector3f t1(0, 0, 0), t2(1, 0, 0), t3(0, 1, 0);
	const Vector3f planeNormal = t2.subtractLocal(t1).crossLocal(t3.subtractLocal(t1)).normalizeLocal();
	EXPECT_TRUE(sameVector(planeNormal, 0, 0, 1));
	EXPECT_FLOAT_BITS(planeNormal.angleBetween(Vector3f::UNIT_Z), 0.0f);
	Vector3f contactPoint = Vector3f(0, 0, -1).multLocal(2).addLocal(Vector3f(5, 5, 5)); // vect6.set(d).multLocal(t).addLocal(o)
	contactPoint.setZ(std::numeric_limits<float>::quiet_NaN());
	EXPECT_TRUE(std::isnan(contactPoint.getZ()));

	// Plane3D.intersection: normal.dot(pointOnPlane.subtract(rayStart)) / dotProduct; rayStart.add(rayDirection.multLocal(distance))
	const Vector3f p1(0, 0, 10), p2(1, 0, 10), p3(0, 1, 10);
	const Vector3f normal = p2.subtract(p1).cross(p3.subtract(p1));
	const Vector3f rayStart(0.5f, 0.5f, 0), rayEnd(0.5f, 0.5f, 20);
	Vector3f rayDirection = rayEnd.subtract(rayStart);
	const float dotProduct = normal.dot(rayDirection);
	const float distance = normal.dot(p1.subtract(rayStart)) / dotProduct;
	EXPECT_TRUE(sameVector(rayStart.add(rayDirection.multLocal(distance)), 0.5f, 0.5f, 10));

	// GeoMap.findMovementCollision: Vector3f dir = pos.subtract(direction).normalizeLocal(); pos.subtractLocal(dir.multLocal(offset))
	Vector3f p(10, 0, 0);
	Vector3f d = p.subtract(Vector3f(0, 0, 0)).normalizeLocal();
	p.subtractLocal(d.multLocal(0.5f));
	EXPECT_TRUE(sameVector(p, 9.5f, 0, 0));

	// CursedQueenModorAI: center.setX(center.x + (float) (Math.cos(angleRadians) * distance))
	Vector3f center(256.62f, 257.79f, 241.8f);
	center.setX(center.x + static_cast<float>(std::cos(0.0) * 4));
	EXPECT_FLOAT_BITS(center.x, 260.62f);
}

TEST(Vector3fTest, ScaleAddAndInterpolate) {
	Vector3f v(1, 2, 3);
	EXPECT_TRUE(sameVector(v.scaleAdd(2, Vector3f(1, 1, 1)), 3, 5, 7));
	Vector3f w;
	EXPECT_TRUE(sameVector(w.scaleAdd(0.5f, Vector3f(2, 4, 6), Vector3f(1, 0, -1)), 2, 2, 2));
	Vector3f i(0, 10, -10);
	EXPECT_TRUE(sameVector(i.interpolate(Vector3f(10, 10, 10), 0.25f), 2.5f, 10, -5));
	Vector3f j(99, 99, 99);
	EXPECT_TRUE(sameVector(j.interpolate(Vector3f(0, 0, 0), Vector3f(4, 8, 16), 0.5f), 2, 4, 8));
}

TEST(Vector3fTest, OrthonormalBasis) {
	Vector3f u, v, w(0, 0, 5);
	Vector3f::generateOrthonormalBasis(u, v, w);
	EXPECT_TRUE(sameVector(w, 0, 0, 1));
	EXPECT_TRUE(sameVector(u, -1, 0, 0)); // |w.x| >= |w.y|: u = (-w.z, 0, w.x) / sqrt(w.x^2 + w.z^2)
	EXPECT_TRUE(sameVector(v, 0, -1, 0)); // v = (w.y*u.z, w.z*u.x - w.x*u.z, -w.y*u.x)
	EXPECT_FLOAT_BITS(u.dot(v), 0.0f);
	EXPECT_FLOAT_BITS(u.dot(w), 0.0f); // -0 + 0 + 0
}

TEST(Vector3fTest, CrossAndSubtractWithAliasedArguments) {
	// a.cross(b, b): the components of b are read before the result is stored
	const Vector3f a(1, 2, 3);
	Vector3f b(4, 5, 6);
	a.cross(b, b);
	EXPECT_TRUE(sameVector(b, -3, 6, -3));
	// a.add(a, a) and a.subtract(a, a) per component
	Vector3f c(1, 2, 3);
	c.add(c, c);
	EXPECT_TRUE(sameVector(c, 2, 4, 6));
	c.subtract(c, c);
	EXPECT_TRUE(sameVector(c, 0, 0, 0));
	Vector3f m(2, 3, 4);
	m.mult(m, m);
	EXPECT_TRUE(sameVector(m, 4, 9, 16));
}
