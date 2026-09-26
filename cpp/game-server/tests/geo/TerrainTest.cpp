// P4-04 Terrain (Terrain.java): heightmap indexing with the zero perimeter, the two triangles per cell, the ray walk of collide and the
// material lookup (whose isLeft test is always true for the triangle it checks, so the second branch never answers). Expectations are
// computed by hand from the Java formulas: z = unsigned sample * 2048 / 65536.

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <vector>

#include "GeoTestSupport.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/geoEngine/collision/CollisionResults.h"
#include "aion/gameserver/geoEngine/math/Ray.h"
#include "aion/gameserver/geoEngine/models/Terrain.h"

namespace aion::gameserver::geoEngine::test {
namespace {

using collision::CollisionResults;
using math::Ray;
using math::Vector3f;
using models::Terrain;

Ray downward(float x, float y, float zMax, float zMin) {
	Vector3f origin(x, y, zMax);
	Vector3f direction = Vector3f(x, y, zMin).subtractLocal(origin).normalizeLocal();
	Ray ray(origin, direction);
	ray.setLimit(zMax - zMin);
	return ray;
}

/** 3 x 3 heightmap, index = yIndex + xIndex * 3; interior points (1,1)=10 (1,2)=20 (2,1)=30 (2,2)=40 (sample = z * 32) */
runtime::Ref<Terrain> slopedTerrain() {
	runtime::Ref<Terrain> terrain = Terrain::create();
	std::vector<int16_t> heights(9, 0);
	heights[1 + 1 * 3] = 320;
	heights[2 + 1 * 3] = 640;
	heights[1 + 2 * 3] = 960;
	heights[2 + 2 * 3] = 1280;
	terrain->setHeightmap(heights, 3, 3);
	return terrain;
}

TEST(TerrainTest, CollideAtOriginUsesBothCellTriangles) {
	GEO_TEST_SCOPE;
	runtime::Ref<Terrain> terrain = slopedTerrain();
	EXPECT_TRUE(terrain->hasHeightmap());
	EXPECT_FALSE(terrain->hasMaterials());

	// cell x 2..4, y 2..4: p1 (2,2,10), p2 (2,4,20), p3 (4,2,30), p4 (4,4,40)
	// (2.5, 2.5) lies in (p1, p2, p3): z = 10 + 10 * (x - 2) + 5 * (y - 2) = 17.5, t = 330 / 4 = 82.5
	Ray ray = downward(2.5f, 2.5f, 100, 0);
	CollisionResults results(1, 1);
	terrain->collideAtOrigin(ray, results);
	ASSERT_EQ(results.size(), 1);
	EXPECT_GEO_FLOAT(results.getClosestCollision()->getContactPoint().z, 17.5f);
	EXPECT_GEO_FLOAT(results.getClosestCollision()->getDistance(), 82.5f);

	// (3.5, 3.5) lies in (p2, p3, p4): z = -20 + 10 * x + 5 * y = 32.5
	Ray second = downward(3.5f, 3.5f, 100, 0);
	CollisionResults secondResults(1, 1);
	terrain->collideAtOrigin(second, secondResults);
	ASSERT_EQ(secondResults.size(), 1);
	EXPECT_GEO_FLOAT(secondResults.getClosestCollision()->getContactPoint().z, 32.5f);

	// the limit cuts the collision off
	Ray tooShort = downward(2.5f, 2.5f, 100, 90);
	CollisionResults none(1, 1);
	terrain->collideAtOrigin(tooShort, none);
	EXPECT_EQ(none.size(), 0);

	// the perimeter points are 0: cell x 0..2, y 0..2 has z 0 at p1, p2, p3 and 10 at p4; (0.5, 0.5) hits z 0
	Ray corner = downward(0.5f, 0.5f, 10, -10);
	CollisionResults cornerResults(1, 1);
	terrain->collideAtOrigin(corner, cornerResults);
	ASSERT_EQ(cornerResults.size(), 1);
	EXPECT_GEO_FLOAT(cornerResults.getClosestCollision()->getContactPoint().z, 0.0f);

	// outside the terrain (index > size): no collision
	Ray outside = downward(9, 9, 10, -10);
	CollisionResults outsideResults(1, 1);
	terrain->collideAtOrigin(outside, outsideResults);
	EXPECT_EQ(outsideResults.size(), 0);
}

TEST(TerrainTest, NoDataPointsAndSlopingSurfaces) {
	GEO_TEST_SCOPE;
	runtime::Ref<Terrain> terrain = Terrain::create();
	std::vector<int16_t> heights(9, 320);
	heights[2 + 1 * 3] = -1; // (1,2) = z2 of the cell at x 2..4, y 2..4: no data
	terrain->setHeightmap(heights, 3, 3);
	Ray ray = downward(2.5f, 2.5f, 100, 0);
	CollisionResults results(1, 1);
	terrain->collideAtOrigin(ray, results);
	EXPECT_EQ(results.size(), 0) << "z2 is NaN";

	// sloping: height differences over 2 m make the contact z NaN when the results ask for it (zDiff 30 here)
	runtime::Ref<Terrain> sloped = slopedTerrain();
	CollisionResults slopeResults(1, 1);
	slopeResults.setInvalidateSlopingSurface(true);
	Ray slopeRay = downward(2.5f, 2.5f, 100, 0);
	sloped->collideAtOrigin(slopeRay, slopeResults);
	ASSERT_EQ(slopeResults.size(), 1);
	EXPECT_TRUE(std::isnan(slopeResults.getClosestCollision()->getContactPoint().z));

	// flat terrain: all samples equal are stored as one value
	runtime::Ref<Terrain> flat = Terrain::create();
	std::vector<int16_t> flatHeights(16, 64); // z = 2
	flat->setHeightmap(flatHeights, 4, 4);
	CollisionResults flatResults(1, 1);
	Ray flatRay = downward(2.5f, 3, 10, -10);
	flat->collideAtOrigin(flatRay, flatResults);
	ASSERT_EQ(flatResults.size(), 1);
	EXPECT_GEO_FLOAT(flatResults.getClosestCollision()->getContactPoint().z, 2.0f);
}

TEST(TerrainTest, CollideWalksTheRayInTwoMeterSteps) {
	GEO_TEST_SCOPE;
	runtime::Ref<Terrain> terrain = slopedTerrain();
	// from (0.5, 2.5, 25) towards (5.5, 2.5, 25): the first step misses the cell x 0..2 (z <= 20), the step at x + 2 checks the cell x 2..4,
	// where z = 10 + 10 (x - 2) + 5 (y - 2) = 25 at x 3.25 (t = 2.75)
	Vector3f origin(0.5f, 2.5f, 25);
	Vector3f target(5.5f, 2.5f, 25);
	float limit = origin.distance(target);
	Vector3f direction = Vector3f(target).subtractLocal(origin).normalizeLocal();
	Ray ray(origin, direction);
	ray.setLimit(limit);
	CollisionResults results(1, 1);
	EXPECT_TRUE(terrain->collide(ray, target.x, target.y, &results));
	ASSERT_EQ(results.size(), 1);
	EXPECT_GEO_FLOAT(results.getClosestCollision()->getContactPoint().x, 3.25f);
	EXPECT_GEO_FLOAT(results.getClosestCollision()->getDistance(), 2.75f);
	EXPECT_TRUE(terrain->collide(ray, target.x, target.y, nullptr)) << "null results (GeoMap.canSee)";

	Vector3f high(0.5f, 2.5f, 100);
	Ray above(high, direction);
	above.setLimit(limit);
	EXPECT_FALSE(terrain->collide(above, 5.5f, 2.5f, nullptr));
}

TEST(TerrainTest, MaterialLookupAndSizeChecks) {
	GEO_TEST_SCOPE;
	runtime::Ref<Terrain> terrain = slopedTerrain();
	std::vector<int8_t> materials(9, 5);
	materials[0] = static_cast<int8_t>(200);
	materials[1] = static_cast<int8_t>(200);
	materials[3] = static_cast<int8_t>(200);
	terrain->setMaterials(materials, 3, 3);
	EXPECT_TRUE(terrain->hasMaterials());
	EXPECT_EQ(terrain->getTerrainMaterialAt(2.5f, 2.5f), 5) << "index 1 + 1 * 3, equal to 5 and 7";
	EXPECT_EQ(terrain->getTerrainMaterialAt(0.5f, 0.5f), 200) << "unsigned byte";
	EXPECT_EQ(terrain->getTerrainMaterialAt(6.5f, 0.5f), 0) << "outside";
	EXPECT_EQ(terrain->getTerrainMaterialAt(-0.5f, 0.5f), 200) << "(int) -0.25 is 0, so the negative x passes the index check";
	EXPECT_THROW(terrain->getTerrainMaterialAt(4.5f, 4.5f), runtime::ArrayIndexOutOfBoundsException) << "materials[8 + 1] (Java AIOOBE)";

	std::vector<int8_t> bigger(16, 1);
	EXPECT_THROW(terrain->setMaterials(bigger, 4, 4), runtime::IllegalArgumentException);
	runtime::Ref<Terrain> other = Terrain::create();
	other->setMaterials(bigger, 4, 4);
	std::vector<int16_t> small(9, 1);
	try {
		other->setHeightmap(small, 3, 3);
		FAIL() << "expected IllegalArgumentException";
	} catch (const runtime::IllegalArgumentException& e) {
		EXPECT_STREQ(e.what(), "Terrain heightmap must not be smaller than terrain materials");
	}
	try {
		Terrain::create()->setHeightmap(small, 2, 5);
		FAIL() << "expected IllegalArgumentException";
	} catch (const runtime::IllegalArgumentException& e) {
		EXPECT_STREQ(e.what(), "Expected terrain heightmap length differs by -1 bytes");
	}
}

} // namespace
} // namespace aion::gameserver::geoEngine::test
