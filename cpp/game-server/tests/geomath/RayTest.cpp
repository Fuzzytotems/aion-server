// Ray: hand-derived intersections (Terrain, BIHNode, GeoMap usage), edge cases and accessors.

#include "aion/gameserver/geoEngine/math/Ray.h"

#include <cmath>
#include <limits>
#include <type_traits>

#include <gtest/gtest.h>

#include "aion/gameserver/geoEngine/math/FastMath.h"

#include "GeoMathTestSupport.h"

using namespace aion::gameserver::geoEngine::math;
using namespace aion::gameserver::geoEngine::math::test;

namespace {

constexpr float Inf = std::numeric_limits<float>::infinity();
const Vector3f V0(0, 0, 0);
const Vector3f V1(1, 0, 0);
const Vector3f V2(0, 1, 0);

} // namespace

TEST(RayTest, DefaultsAndAccessors) {
	EXPECT_TRUE(std::is_trivially_copyable_v<Ray>);
	Ray r;
	EXPECT_TRUE(sameVector(r.getOrigin(), 0, 0, 0));
	EXPECT_TRUE(sameVector(r.getDirection(), 0, 0, 0));
	EXPECT_EQ(r.getLimit(), Inf);
	EXPECT_EQ(r.toString(), "Ray [Origin: (0.0, 0.0, 0.0), Direction: (0.0, 0.0, 0.0)]");
	r.setLimit(12.5f); // GeoMap: r.setLimit(zMax - zMin)
	EXPECT_EQ(r.limit, 12.5f);
	r.setOrigin(Vector3f(1, 2, 3));
	r.setDirection(Vector3f(0, 0, -1));
	Ray copy;
	copy.set(r); // copies origin and direction, not the limit
	EXPECT_TRUE(sameVector(copy.origin, 1, 2, 3));
	EXPECT_TRUE(sameVector(copy.direction, 0, 0, -1));
	EXPECT_EQ(copy.limit, Inf);
	const Ray cloned = r.clone();
	EXPECT_EQ(cloned.limit, 12.5f);
	r.getOrigin().x = 50; // getOrigin() is a reference, like the Java object
	EXPECT_EQ(r.origin.x, 50.0f);
	EXPECT_EQ(cloned.origin.x, 1.0f);
}

TEST(RayTest, VerticalRayLikeGeoMapGetZ) {
	// GeoMap.getZ: origin (x, y, zMax), direction (target - origin).normalizeLocal()
	Vector3f origin(0.25f, 0.5f, 5);
	Vector3f target(0.25f, 0.5f, -5);
	target.subtractLocal(origin).normalizeLocal();
	const Ray r(origin, target);
	EXPECT_TRUE(sameVector(r.direction, 0, 0, -1));
	EXPECT_EQ(r.intersects(V0, V1, V2), 5.0f);
	Vector3f contact;
	ASSERT_TRUE(r.intersectWhere(V0, V1, V2, contact));
	EXPECT_TRUE(sameVector(contact, 0.25f, 0.5f, 0));
	EXPECT_TRUE(r.intersectWhere(V0, V1, V2));
	// both windings hit (no back face culling)
	EXPECT_EQ(r.intersects(V0, V2, V1), 5.0f);
	// the limit is not checked here (Terrain/BIHNode compare the distance with ray.limit themselves)
	Ray limited = r;
	limited.setLimit(1);
	EXPECT_EQ(limited.intersects(V0, V1, V2), 5.0f);
}

TEST(RayTest, Misses) {
	const Ray outside(Vector3f(0.75f, 0.75f, 5), Vector3f(0, 0, -1)); // u + v = 1.5 > 1
	EXPECT_EQ(outside.intersects(V0, V1, V2), Inf);
	Vector3f untouched(7, 8, 9);
	EXPECT_FALSE(outside.intersectWhere(V0, V1, V2, untouched));
	EXPECT_TRUE(sameVector(untouched, 7, 8, 9));

	const Ray behind(Vector3f(0.25f, 0.25f, -5), Vector3f(0, 0, -1)); // triangle behind the origin
	EXPECT_EQ(behind.intersects(V0, V1, V2), Inf);
	EXPECT_FALSE(behind.intersectWhere(V0, V1, V2));

	const Ray parallel(Vector3f(0.25f, 0.25f, 1), Vector3f(1, 0, 0)); // dirDotNorm within FLT_EPSILON
	EXPECT_EQ(parallel.intersects(V0, V1, V2), Inf);
	EXPECT_FALSE(parallel.intersectWhere(V0, V1, V2));

	const Ray nearlyParallel(Vector3f(0.25f, 0.25f, 1), Vector3f(1, 0, -1e-7f)); // |dirDotNorm| = 1e-7 <= FLT_EPSILON
	EXPECT_EQ(nearlyParallel.intersects(V0, V1, V2), Inf);

	const Ray zeroDirection(Vector3f(0.25f, 0.25f, 1), Vector3f(0, 0, 0));
	EXPECT_EQ(zeroDirection.intersects(V0, V1, V2), Inf);

	const Ray degenerateTriangle(Vector3f(0.25f, 0.25f, 1), Vector3f(0, 0, -1));
	EXPECT_EQ(degenerateTriangle.intersects(V0, V1, Vector3f(2, 0, 0)), Inf);
}

TEST(RayTest, EdgesAndVerticesCount) {
	const Ray onEdge(Vector3f(0.5f, 0.5f, 2), Vector3f(0, 0, -1)); // u + v == 1 exactly
	EXPECT_EQ(onEdge.intersects(V0, V1, V2), 2.0f);
	const Ray onVertex(Vector3f(0, 0, 2), Vector3f(0, 0, -1));
	EXPECT_EQ(onVertex.intersects(V0, V1, V2), 2.0f);
	const Ray startingOnTriangle(Vector3f(0.25f, 0.25f, 0), Vector3f(0, 0, -1)); // diffDotNorm == 0 counts
	EXPECT_EQ(startingOnTriangle.intersects(V0, V1, V2), 0.0f);
}

TEST(RayTest, PlanarQuad) {
	// store receives (t, w1, w2): w1 along v1 - v0, w2 along v2 - v0
	const Ray r(Vector3f(0.25f, 0.5f, 5), Vector3f(0, 0, -1));
	Vector3f tuv;
	ASSERT_TRUE(r.intersectWherePlanarQuad(V0, V1, V2, tuv));
	EXPECT_TRUE(sameVector(tuv, 5, 0.25f, 0.5f));
	// the quad extends the triangle up to (1, 1)
	const Ray outsideTriangle(Vector3f(0.75f, 0.75f, 5), Vector3f(0, 0, -1));
	ASSERT_TRUE(outsideTriangle.intersectWherePlanarQuad(V0, V1, V2, tuv));
	EXPECT_TRUE(sameVector(tuv, 5, 0.75f, 0.75f));
	const Ray outsideQuad(Vector3f(0.5f, 1.25f, 5), Vector3f(0, 0, -1)); // w2 = 1.25 > 1
	EXPECT_FALSE(outsideQuad.intersectWherePlanarQuad(V0, V1, V2, tuv));
	const Ray negativeW1(Vector3f(-0.25f, 0.5f, 5), Vector3f(0, 0, -1));
	EXPECT_FALSE(negativeW1.intersectWherePlanarQuad(V0, V1, V2, tuv));
	// as in jME, the quad test only bounds w2 from above: w1 = 1.25 still hits
	const Ray w1AboveOne(Vector3f(1.25f, 0.5f, 5), Vector3f(0, 0, -1));
	ASSERT_TRUE(w1AboveOne.intersectWherePlanarQuad(V0, V1, V2, tuv));
	EXPECT_TRUE(sameVector(tuv, 5, 1.25f, 0.5f));
}

TEST(RayTest, DistanceSquared) {
	const Ray r(Vector3f(0, 0, 0), Vector3f(1, 0, 0));
	EXPECT_EQ(r.distanceSquared(Vector3f(5, 3, 4)), 25.0f);  // closest point (5, 0, 0)
	EXPECT_EQ(r.distanceSquared(Vector3f(-2, 3, 0)), 13.0f); // behind the origin: distance to the origin
	EXPECT_EQ(r.distanceSquared(Vector3f(7, 0, 0)), 0.0f);
}

TEST(RayTest, TerrainQuadSplitPattern) {
	// Terrain.collideWithRay: two triangles per height map cell sharing p2 and p3, one reused store for p1 and p4
	Vector3f p1or4, contactPoint;
	const Vector3f p2(0, 1, 10), p3(1, 0, 10);
	const Ray ray(Vector3f(0.9f, 0.9f, 20), Vector3f(0, 0, -1));
	const bool first = ray.intersectWhere(p1or4.set(0, 0, 10), p2, p3, contactPoint);
	const bool second = !first && ray.intersectWhere(p1or4.set(1, 1, 10), p2, p3, contactPoint);
	EXPECT_FALSE(first);
	EXPECT_TRUE(second);
	EXPECT_TRUE(sameVector(contactPoint, 0.9f, 0.9f, 10));
	EXPECT_EQ(contactPoint.distance(ray.origin), 10.0f);
}
