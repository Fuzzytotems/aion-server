// P4-04 BoundingBox and BoundingVolume (BoundingBox.java, BoundingVolume.java): the expectations are computed by hand from the Java formulas.

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <vector>

#include "GeoTestSupport.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/geoEngine/bounding/BoundingBox.h"
#include "aion/gameserver/geoEngine/collision/CollisionResults.h"
#include "aion/gameserver/geoEngine/math/Matrix4f.h"
#include "aion/gameserver/geoEngine/math/Ray.h"
#include "aion/gameserver/geoEngine/utils/TempVars.h"

namespace aion::gameserver::geoEngine::test {
namespace {

using bounding::BoundingBox;
using math::Ray;
using math::Vector3f;

constexpr float INF = std::numeric_limits<float>::infinity();

TEST(BoundingBoxTest, ContainAabb) {
	GEO_TEST_SCOPE;
	runtime::Ref<BoundingBox> box = BoundingBox::create();
	std::vector<float> points{0, 0, 0, 2, 4, -6};
	box->computeFromPoints(points);
	EXPECT_TRUE(box->getCenter().equals(Vector3f(1, 2, -3)));
	EXPECT_EQ(box->getXExtent(), 1.0f);
	EXPECT_EQ(box->getYExtent(), 2.0f);
	EXPECT_EQ(box->getZExtent(), 3.0f);
	EXPECT_TRUE(box->getMin().equals(Vector3f(0, 0, -6)));
	EXPECT_TRUE(box->getMax().equals(Vector3f(2, 4, 0)));
	EXPECT_EQ(box->getVolume(), 48.0f);
	EXPECT_EQ(box->toString(), "BoundingBox [Center: (1.0, 2.0, -3.0)  xExtent: 1.0  yExtent: 2.0  zExtent: 3.0]");

	std::vector<float> two{1, 2};
	EXPECT_THROW(box->containAABB(two), runtime::IllegalArgumentException) << "limit <= 2";
	std::vector<float> four{1, 2, 3, 4};
	EXPECT_THROW(box->containAABB(four), runtime::IndexOutOfBoundsException) << "FloatBuffer.get past the limit";
	for (int i = 0; i < 6; i++)
		EXPECT_THROW(box->containAABB(four), runtime::IndexOutOfBoundsException);
	std::vector<utils::TempVars*> held; // Deviation: the TempVars taken before the exceptions were released (Java leaks them)
	for (int i = 0; i < 5; i++)
		held.push_back(&utils::TempVars::get());
	for (auto it = held.rbegin(); it != held.rend(); ++it)
		(*it)->release();
	EXPECT_THROW(box->setXExtent(-1), runtime::IllegalArgumentException);
	EXPECT_EQ(box->getType(), BoundingBox::Type::AABB);
}

TEST(BoundingBoxTest, TransformRotatesScalesAndTranslates) {
	GEO_TEST_SCOPE;
	runtime::Ref<BoundingBox> box = BoundingBox::create(Vector3f(1, 2, 3), 1, 1, 1);
	// Geometry.setTransform: rotation 90 degrees around z (m01 = -1, m10 = 1), then columns scaled by (2, 3, 4), translation (10, 20, 30)
	math::Matrix4f matrix;
	math::Matrix3f rotation(0, -1, 0, 1, 0, 0, 0, 0, 1);
	matrix.setRotationMatrix(rotation);
	matrix.scale(Vector3f(2, 3, 4));
	matrix.setTranslation(Vector3f(10, 20, 30));
	runtime::Ref<bounding::BoundingVolume> world = box->transform(matrix, nullptr);
	runtime::Ptr<BoundingBox> worldBox = runtime::cast<BoundingBox>(world);
	// center: (0*1 - 3*2 + 0*3 + 10, 2*1 + 0 + 0 + 20, 0 + 0 + 4*3 + 30); extents |m| * (1, 1, 1): (3, 2, 4)
	EXPECT_TRUE(worldBox->getCenter().equals(Vector3f(4, 22, 42)));
	EXPECT_EQ(worldBox->getXExtent(), 3.0f);
	EXPECT_EQ(worldBox->getYExtent(), 2.0f);
	EXPECT_EQ(worldBox->getZExtent(), 4.0f);
	runtime::Ref<bounding::BoundingVolume> reused = box->transform(matrix, world);
	EXPECT_EQ(reused.get(), world.get()) << "an AABB store is reused";
	runtime::Ref<bounding::BoundingVolume> cloned = box->clone(world);
	EXPECT_EQ(cloned.get(), world.get());
	EXPECT_TRUE(worldBox->getCenter().equals(Vector3f(1, 2, 3)));
}

TEST(BoundingBoxTest, MergeLocal) {
	GEO_TEST_SCOPE;
	runtime::Ref<BoundingBox> a = BoundingBox::create(Vector3f(0, 0, 0), 1, 1, 1);
	runtime::Ref<BoundingBox> b = BoundingBox::create(Vector3f(3, 0, 0), 1, 2, 1);
	EXPECT_EQ(a->mergeLocal(b).get(), a.get());
	// x: low min(-1, 2) = -1, high max(1, 4) = 4 -> center 1.5, extent 2.5; y: -2..2; z: -1..1
	EXPECT_TRUE(a->getCenter().equals(Vector3f(1.5f, 0, 0)));
	EXPECT_EQ(a->getXExtent(), 2.5f);
	EXPECT_EQ(a->getYExtent(), 2.0f);
	EXPECT_EQ(a->getZExtent(), 1.0f);
	EXPECT_EQ(a->mergeLocal(nullptr).get(), a.get());
	runtime::Ref<BoundingBox> infinite = BoundingBox::create(Vector3f(5, 5, 5), INF, 1, 1);
	a->mergeLocal(infinite);
	EXPECT_EQ(a->getCenter().x, 0.0f);
	EXPECT_EQ(a->getXExtent(), INF);
}

TEST(BoundingBoxTest, RayIntersectionAndClipping) {
	GEO_TEST_SCOPE;
	runtime::Ref<BoundingBox> box = BoundingBox::create(Vector3f(0, 0, 0), 1, 1, 1);
	EXPECT_TRUE(box->intersects(Ray(Vector3f(-5, 0, 0), Vector3f(1, 0, 0))));
	EXPECT_FALSE(box->intersects(Ray(Vector3f(-5, 0, 0), Vector3f(-1, 0, 0)))) << "pointing away";
	EXPECT_FALSE(box->intersects(Ray(Vector3f(-5, 3, 0), Vector3f(1, 0, 0)))) << "passing beside";

	// clip: t0 = 4 (entering x = -1), t1 = 6 (leaving x = 1)
	Ray ray(Vector3f(-5, 0, 0), Vector3f(1, 0, 0));
	collision::CollisionResults results(1, 1);
	EXPECT_EQ(box->collideWith(ray, results), 2);
	ASSERT_EQ(results.size(), 2);
	EXPECT_EQ(results.getCollision(0).getDistance(), 4.0f);
	EXPECT_TRUE(results.getCollision(0).getContactPoint().equals(Vector3f(-1, 0, 0)));
	EXPECT_EQ(results.getCollision(1).getDistance(), 6.0f);

	// a limit of 5 cuts t1
	ray.setLimit(5);
	collision::CollisionResults limited(1, 1);
	EXPECT_EQ(box->collideWith(ray, limited), 2);
	EXPECT_EQ(limited.getFarthestCollision()->getDistance(), 5.0f);
	EXPECT_TRUE(limited.getFarthestCollision()->getContactPoint().equals(Vector3f(0, 0, 0)));

	// from inside: t0 stays 0, t1 becomes 1
	Ray inside(Vector3f(0, 0, 0), Vector3f(1, 0, 0));
	collision::CollisionResults insideResults(1, 1);
	EXPECT_EQ(box->collideWith(inside, insideResults), 2);
	EXPECT_EQ(insideResults.getClosestCollision()->getDistance(), 0.0f);
	EXPECT_EQ(insideResults.getFarthestCollision()->getDistance(), 1.0f);

	// entirely inside the limit: nothing changes, no collision
	Ray shortRay(Vector3f(0, 0, 0), Vector3f(1, 0, 0));
	shortRay.setLimit(0.5f);
	collision::CollisionResults none(1, 1);
	EXPECT_EQ(box->collideWith(shortRay, none), 0);
}

TEST(BoundingBoxTest, PointTests) {
	GEO_TEST_SCOPE;
	runtime::Ref<BoundingBox> box = BoundingBox::create(Vector3f(0, 0, 0), 1, 1, 1);
	EXPECT_FALSE(box->contains(Vector3f(1, 0, 0))) << "strictly inside";
	EXPECT_TRUE(box->intersects(Vector3f(1, 0, 0))) << "touching counts";
	EXPECT_TRUE(box->contains(Vector3f(0.5f, -0.5f, 0.9f)));
	EXPECT_EQ(box->distanceToEdge(Vector3f(4, 0, 5)), 5.0f) << "sqrt(3^2 + 4^2)";
	EXPECT_EQ(box->distanceToEdge(Vector3f(0, 0, 0)), 0.0f);
	EXPECT_EQ(box->distanceTo(Vector3f(3, 4, 0)), 5.0f);
	EXPECT_EQ(box->distanceSquaredTo(Vector3f(3, 4, 0)), 25.0f);
	Vector3f store;
	EXPECT_TRUE(box->getCenter(store).equals(Vector3f(0, 0, 0)));
	runtime::Ref<BoundingBox> other = BoundingBox::create(Vector3f(2, 0, 0), 1, 1, 1);
	EXPECT_TRUE(box->intersects(static_cast<bounding::BoundingVolume&>(*other))) << "touching faces";
	runtime::Ref<BoundingBox> far = BoundingBox::create(Vector3f(2.5f, 0, 0), 1, 1, 1);
	EXPECT_FALSE(box->intersectsBoundingBox(*far));
}

} // namespace
} // namespace aion::gameserver::geoEngine::test
