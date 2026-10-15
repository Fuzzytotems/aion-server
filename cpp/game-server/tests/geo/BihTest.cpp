// P4-04 BIH tree (BIHTree.java, BIHNode.java) against a brute force over all triangles: 10,000 seeded random rays through transformed
// meshes (random triangle soups, a grid, a single triangle, degenerate and duplicate triangles) must find identical collisions, bit for bit.
// The brute force applies the same gates as Geometry.collideWith and BIHTree.collideWithRay (world bound, box clip or origin inside) and
// the same per-triangle computation as the BIHNode leaf loop; only the tree traversal differs. It also checks that construct() keeps the
// set of triangles (it sorts them in place) and that the ray is restored after the collision check.

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <tuple>
#include <vector>

#include "GeoTestSupport.h"
#include "aion/gameserver/geoEngine/bounding/BoundingBox.h"
#include "aion/gameserver/geoEngine/collision/CollisionResults.h"
#include "aion/gameserver/geoEngine/collision/bih/BIHNode.h"
#include "aion/gameserver/geoEngine/collision/bih/BIHTree.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/geoEngine/math/Matrix4f.h"
#include "aion/gameserver/geoEngine/math/Ray.h"
#include "aion/gameserver/geoEngine/scene/Geometry.h"
#include "aion/gameserver/geoEngine/scene/Mesh.h"
#include "aion/gameserver/geoEngine/utils/TempVars.h"

namespace aion::gameserver::geoEngine::test {
namespace {

using collision::CollisionResults;
using math::Matrix3f;
using math::Matrix4f;
using math::Ray;
using math::Vector3f;

constexpr float INF = std::numeric_limits<float>::infinity();

using Hit = std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>; // distance, contact x, y, z bits

std::vector<Hit> hitsOf(CollisionResults& results) {
	std::vector<Hit> hits;
	for (int32_t i = 0; i < results.size(); i++) {
		collision::CollisionResult result = results.getCollisionDirect(i);
		Vector3f p = result.getContactPoint();
		hits.emplace_back(bitsOf(result.getDistance()), bitsOf(p.x), bitsOf(p.y), bitsOf(p.z));
	}
	std::sort(hits.begin(), hits.end());
	return hits;
}

/** Geometry.collideWith + BIHTree.collideWithRay gates, then BIHNode.intersectWhere's leaf computation for every triangle */
std::vector<Hit> bruteForce(scene::Geometry& geometry, const Ray& worldRay) {
	CollisionResults results(1, 1);
	runtime::Ptr<bounding::BoundingVolume> bound = geometry.getWorldBound();
	if (!bound->intersects(worldRay))
		return {};
	Ray gateRay = worldRay;
	CollisionResults wbCollisions(1, 1);
	bound->collideWith(gateRay, wbCollisions);
	if (!(wbCollisions.size() > 0 || bound->contains(worldRay.getOrigin())))
		return {};
	const Matrix4f worldMatrix = geometry.getWorldMatrix();
	const Vector3f o = worldRay.getOrigin();
	const Vector3f d = worldRay.getDirection();
	Ray local = worldRay;
	Matrix4f inv = worldMatrix.invert();
	inv.mult(local.getOrigin(), local.getOrigin());
	inv.multNormal(local.getDirection(), local.getDirection());
	local.getDirection().normalizeLocal();
	runtime::Ptr<scene::Mesh> mesh = geometry.getMesh();
	for (int32_t i = 0; i < mesh->getTriangleCount(); i++) {
		Vector3f v1, v2, v3;
		mesh->getTriangle(i, v1, v2, v3);
		float t = local.intersects(v1, v2, v3);
		if (math::JavaFloat::isInfinite(t))
			continue;
		worldMatrix.mult(v1, v1);
		worldMatrix.mult(v2, v2);
		worldMatrix.mult(v3, v3);
		float tWorld = Ray(o, d).intersects(v1, v2, v3);
		Vector3f contact = Vector3f(d).multLocal(tWorld).addLocal(o);
		float distance = o.distance(contact);
		if (distance > worldRay.limit)
			continue;
		results.addCollision(collision::CollisionResult(contact, distance));
	}
	return hitsOf(results);
}

struct TestGeometry {
	runtime::Ref<scene::Geometry> geometry;
	std::vector<std::array<float, 9>> triangles;
};

std::vector<std::array<float, 9>> trianglesOf(scene::Mesh& mesh) {
	std::vector<std::array<float, 9>> triangles;
	for (int32_t i = 0; i < mesh.getTriangleCount(); i++) {
		Vector3f v1, v2, v3;
		mesh.getTriangle(i, v1, v2, v3);
		triangles.push_back({v1.x, v1.y, v1.z, v2.x, v2.y, v2.z, v3.x, v3.y, v3.z});
	}
	std::sort(triangles.begin(), triangles.end());
	return triangles;
}

Matrix3f rotationOf(double yaw, double pitch, double roll) {
	double cy = std::cos(yaw), sy = std::sin(yaw), cp = std::cos(pitch), sp = std::sin(pitch), cr = std::cos(roll), sr = std::sin(roll);
	return Matrix3f(static_cast<float>(cy * cp), static_cast<float>(cy * sp * sr - sy * cr), static_cast<float>(cy * sp * cr + sy * sr),
		static_cast<float>(sy * cp), static_cast<float>(sy * sp * sr + cy * cr), static_cast<float>(sy * sp * cr - cy * sr),
		static_cast<float>(-sp), static_cast<float>(cp * sr), static_cast<float>(cp * cr));
}

TestGeometry place(runtime::Ref<scene::Mesh> mesh, const Matrix3f& rotation, const Vector3f& location, const Vector3f& scale) {
	TestGeometry result;
	result.triangles = trianglesOf(*mesh);
	result.geometry = scene::Geometry::create("test.cgf", mesh);
	result.geometry->setTransform(rotation, location, scale);
	result.geometry->updateModelBound();
	mesh->createCollisionData();
	return result;
}

TEST(BihTest, TreeCollisionsEqualBruteForceOnRandomRays) {
	GEO_TEST_SCOPE;
	std::mt19937 random(4804);
	std::uniform_real_distribution<float> unit(-1.0f, 1.0f);
	std::vector<TestGeometry> geometries;

	// 1) a soup of 300 random triangles (short indices)
	{
		std::vector<float> vertices;
		std::vector<int32_t> indices;
		for (int i = 0; i < 900; i++) {
			vertices.push_back(unit(random) * 10);
			vertices.push_back(unit(random) * 10);
			vertices.push_back(unit(random) * 10);
			indices.push_back(i);
		}
		geometries.push_back(place(makeMesh(vertices, indices, 1), identityRotation(), Vector3f(0, 0, 0), Vector3f(1, 1, 1)));
		geometries.push_back(
			place(makeMesh(vertices, indices, 1), rotationOf(0.7, -0.3, 1.1), Vector3f(100.5f, -20.25f, 7), Vector3f(2, 0.5f, 1.5f)));
	}
	// 2) an 11 x 11 height grid (200 triangles, byte indices) with a mirrored scale
	{
		std::vector<float> vertices;
		std::vector<int32_t> indices;
		for (int x = 0; x <= 10; x++) {
			for (int y = 0; y <= 10; y++) {
				vertices.push_back(static_cast<float>(x) * 3);
				vertices.push_back(static_cast<float>(y) * 3);
				vertices.push_back(unit(random) * 2);
			}
		}
		for (int x = 0; x < 10; x++) {
			for (int y = 0; y < 10; y++) {
				int p = x * 11 + y;
				indices.insert(indices.end(), {p, p + 1, p + 11, p + 1, p + 12, p + 11});
			}
		}
		geometries.push_back(place(makeMesh(vertices, indices, 1, 0, true), rotationOf(0, 0, 0.2), Vector3f(-40, 5, 0), Vector3f(-1, 1, 1)));
	}
	// 3) a single triangle (the root is a leaf)
	{
		std::vector<float> vertices{0, 0, 0, 5, 0, 0, 0, 5, 0};
		std::vector<int32_t> indices{0, 1, 2};
		geometries.push_back(place(makeMesh(vertices, indices, 1), rotationOf(0.1, 0.2, 0.3), Vector3f(10, 10, 10), Vector3f(1, 1, 1)));
	}
	// 4) axis-aligned boxes of duplicate, degenerate and touching triangles (split planes equal to ray coordinates)
	{
		std::vector<float> vertices;
		std::vector<int32_t> indices;
		int next = 0;
		for (int i = 0; i < 60; i++) {
			float x = static_cast<float>(i % 6) * 2, y = static_cast<float>((i / 6) % 5) * 2, z = static_cast<float>(i % 4);
			std::array<float, 9> quad{x, y, z, x + 2, y, z, x, y + 2, z};
			for (int copy = 0; copy < (i % 7 == 0 ? 2 : 1); copy++) {
				vertices.insert(vertices.end(), quad.begin(), quad.end());
				indices.insert(indices.end(), {next, next + 1, next + 2});
				next += 3;
			}
			if (i % 5 == 0) { // degenerate: three collinear points
				vertices.insert(vertices.end(), {x, y, z, x + 1, y, z, x + 2, y, z});
				indices.insert(indices.end(), {next, next + 1, next + 2});
				next += 3;
			}
		}
		geometries.push_back(place(makeMesh(vertices, indices, 1), identityRotation(), Vector3f(0, 0, 50), Vector3f(1, 1, 1)));
	}

	for (TestGeometry& g : geometries) {
		runtime::Ptr<scene::Mesh> mesh = g.geometry->getMesh();
		ASSERT_NE(mesh->getCollisionTree(), nullptr);
		EXPECT_EQ(trianglesOf(*mesh), g.triangles) << "construct() only reorders the triangles";
	}

	int32_t hits = 0;
	int32_t raysWithHits = 0;
	for (int ray = 0; ray < 10000; ray++) {
		TestGeometry& g = geometries[static_cast<size_t>(ray) % geometries.size()];
		runtime::Ptr<bounding::BoundingBox> bound = runtime::cast<bounding::BoundingBox>(g.geometry->getWorldBound());
		Vector3f center = bound->getCenter();
		Vector3f extent = bound->getExtent();
		Vector3f origin(center.x + unit(random) * (extent.x + 5), center.y + unit(random) * (extent.y + 5), center.z + unit(random) * (extent.z + 5));
		Vector3f direction(unit(random), unit(random), unit(random));
		if (ray % 10 == 0) // axis-parallel rays: infinite inverse directions and NaN split distances
			direction = Vector3f(0, 0, ray % 20 == 0 ? -1.0f : 1.0f);
		else if (ray % 10 == 1)
			direction = Vector3f(ray % 20 == 1 ? 1.0f : -1.0f, 0, 0);
		direction.normalizeLocal();
		Ray worldRay(origin, direction);
		if (ray % 3 != 0)
			worldRay.setLimit(std::abs(unit(random)) * 60);

		std::vector<Hit> expected = bruteForce(*g.geometry, worldRay);
		CollisionResults results(1, 1);
		Ray treeRay = worldRay;
		int32_t added = g.geometry->collideWith(treeRay, results);
		std::vector<Hit> actual = hitsOf(results);
		ASSERT_EQ(actual, expected) << "ray " << ray << " origin " << origin.toString() << " direction " << direction.toString();
		EXPECT_EQ(added, static_cast<int32_t>(actual.size()));
		EXPECT_TRUE(treeRay.getOrigin().equals(worldRay.getOrigin()) && treeRay.getDirection().equals(worldRay.getDirection()))
			<< "the ray is restored";
		for (int32_t i = 0; i < results.size(); i++)
			EXPECT_EQ(results.getCollisionDirect(i).getGeometry().get(), g.geometry.get());
		hits += static_cast<int32_t>(actual.size());
		raysWithHits += actual.empty() ? 0 : 1;
	}
	EXPECT_GT(raysWithHits, 1200) << "the rays must exercise the trees";
	EXPECT_GT(hits, 2500) << hits;
}

TEST(BihTest, OnlyFirstStopsAtTheFirstCollision) {
	GEO_TEST_SCOPE;
	std::vector<float> vertices;
	std::vector<int32_t> indices;
	for (int layer = 0; layer < 40; layer++) { // 40 stacked quads, 80 triangles
		float z = static_cast<float>(layer);
		int base = static_cast<int>(vertices.size() / 3);
		vertices.insert(vertices.end(), {-5, -5, z, 5, -5, z, -5, 5, z, 5, 5, z});
		indices.insert(indices.end(), {base, base + 1, base + 2, base + 1, base + 3, base + 2});
	}
	TestGeometry g = place(makeMesh(vertices, indices, 1), identityRotation(), Vector3f(0, 0, 0), Vector3f(1, 1, 1));
	Ray ray(Vector3f(0.3f, 0.2f, 100), Vector3f(0, 0, -1));
	CollisionResults all(1, 1);
	EXPECT_EQ(g.geometry->collideWith(ray, all), 40);
	EXPECT_EQ(all.getClosestCollision()->getContactPoint().z, 39.0f);
	CollisionResults first(1, 1, true);
	EXPECT_EQ(g.geometry->collideWith(ray, first), 1);
	EXPECT_EQ(first.size(), 1);
	{
		// Deviation (L15): the onlyFirst stop leaves pushed far nodes in Java's ThreadLocal stack; the C++ stack is empty after the call
		utils::TempVars& vars = utils::TempVars::get();
		EXPECT_TRUE(vars.bihStack.empty()) << vars.bihStack.size() << " Ref<BIHNode> entries left in the thread-local stack";
		vars.release();
	}

	runtime::Ptr<collision::bih::BIHTree> tree = runtime::cast<collision::bih::BIHTree>(g.geometry->getMesh()->getCollisionTree());
	runtime::Ptr<collision::bih::BIHNode> root = tree->getRoot();
	ASSERT_NE(root, nullptr);
	EXPECT_NE(root->getAxis(), 3) << "80 triangles need splits (at most 21 per leaf)";
}

TEST(BihTest, AnExceptionLeavesNoNodeInTheThreadLocalStack) {
	GEO_TEST_SCOPE;
	// one triangle in the plane x = 20; a hand-made tree whose near (left) leaf names a triangle the mesh does not have
	const std::vector<float> vertices{20, -5, -5, 20, 5, -5, 20, 0, 5};
	const std::vector<int32_t> indices{0, 1, 2};
	runtime::Ref<scene::Mesh> mesh = makeMesh(vertices, indices, 1);
	runtime::Ref<collision::bih::BIHTree> tree = collision::bih::BIHTree::create(*mesh);
	runtime::Ref<collision::bih::BIHNode> root = collision::bih::BIHNode::create(0);
	runtime::Ref<collision::bih::BIHNode> brokenLeaf = collision::bih::BIHNode::create(1000, 1000);
	runtime::Ref<collision::bih::BIHNode> farLeaf = collision::bih::BIHNode::create(0, 0);
	root->setLeftChild(brokenLeaf);
	root->setLeftPlane(10);
	root->setRightChild(farLeaf);
	root->setRightPlane(0);
	const uint32_t farLeafRefs = farLeaf->refCount();

	// ray along +x from x = -5: tNearSplit 15, tFarSplit 5, so the far leaf is pushed and the near leaf throws
	Ray ray(Vector3f(-5, 0, 0), Vector3f(1, 0, 0));
	CollisionResults results(1, 1);
	EXPECT_ANY_THROW(static_cast<void>(root->intersectWhere(ray, Matrix4f::IDENTITY, *tree, 0, 100, results)));

	utils::TempVars& vars = utils::TempVars::get();
	EXPECT_TRUE(vars.bihStack.empty()) << "the far leaf entry must not stay in the thread-local stack";
	EXPECT_EQ(farLeaf->refCount(), farLeafRefs) << "no Ref<BIHNode> left behind";
	vars.release();

	// the same tree without the broken leaf collides normally and leaves the stack empty
	root->setLeftChild(collision::bih::BIHNode::create(0, 0));
	Ray again(Vector3f(-5, 0, 0), Vector3f(1, 0, 0));
	EXPECT_EQ(root->intersectWhere(again, Matrix4f::IDENTITY, *tree, 0, 100, results), 2) << "both leaves name the one triangle";
	utils::TempVars& after = utils::TempVars::get();
	EXPECT_TRUE(after.bihStack.empty());
	after.release();
}

} // namespace
} // namespace aion::gameserver::geoEngine::test
