// P4-04 scene graph and GeoMap (Spatial.java, Node.java, Geometry.java, DespawnableNode.java, GeoMap.java): child management, clones,
// despawnable node gates with the GeoCallbacks test doubles (event theme, siege shield state), map chunks and despawnable registries, doors,
// and the GeoMap queries (getZ, canSee, getClosestCollision, findMovementCollision, getTerrainMaterialAt) on a small scene: a 10 x 10 m quad
// at z 5 over a flat 16 x 16 m terrain at z 2. Expectations are derived by hand from the Java code.

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "GeoTestSupport.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/geoEngine/GeoCallbacks.h"
#include "aion/gameserver/geoEngine/bounding/BoundingVolume.h"
#include "aion/gameserver/geoEngine/collision/CollisionIntentionInfo.h"
#include "aion/gameserver/geoEngine/collision/CollisionResults.h"
#include "aion/gameserver/geoEngine/collision/IgnoreProperties.h"
#include "aion/gameserver/geoEngine/math/Ray.h"
#include "aion/gameserver/geoEngine/models/GeoMap.h"
#include "aion/gameserver/geoEngine/models/Terrain.h"
#include "aion/gameserver/geoEngine/scene/CloneNotSupportedException.h"
#include "aion/gameserver/geoEngine/scene/DespawnableNode.h"
#include "aion/gameserver/geoEngine/scene/Geometry.h"
#include "aion/gameserver/geoEngine/scene/Node.h"
#include "aion/gameserver/model/house/HouseDoorState.h"

namespace aion::gameserver::geoEngine::test {
namespace {

using collision::CollisionResults;
using collision::IgnoreProperties;
using math::Ray;
using math::Vector3f;
using scene::DespawnableNode;
using scene::Geometry;
using scene::Node;
using DespawnableType = DespawnableNode::DespawnableType;

constexpr float NaN = std::numeric_limits<float>::quiet_NaN();

/** a 10 x 10 quad (two triangles) at z 0 in model space */
runtime::Ref<scene::Mesh> quadMesh(int8_t intentions = 1) {
	std::vector<float> vertices{0, 0, 0, 10, 0, 0, 0, 10, 0, 10, 10, 0};
	std::vector<int32_t> indices{0, 1, 2, 1, 3, 2};
	return makeMesh(vertices, indices, intentions);
}

/** a node with one quad geometry placed at (x, y, z) */
runtime::Ref<Node> quadNode(float x, float y, float z, std::string_view name = "levels/test/quad.cgf", int8_t intentions = 1) {
	runtime::Ref<Node> node = Node::create(std::nullopt);
	node->attachChild(Geometry::create(name, quadMesh(intentions)));
	node->setCollisionIntentions(intentions);
	node->setTransform(identityRotation(), Vector3f(x, y, z), Vector3f(1, 1, 1));
	node->updateModelBound();
	return node;
}

runtime::Ref<DespawnableNode> despawnable(DespawnableType type, int32_t id, float x = 0, float y = 0, float z = 5) {
	runtime::Ref<DespawnableNode> node = DespawnableNode::create();
	node->copyFrom(*quadNode(0, 0, 0));
	node->setType(type);
	node->setId(id);
	node->setTransform(identityRotation(), Vector3f(x, y, z), Vector3f(1, 1, 1));
	node->updateModelBound();
	return node;
}

int32_t collide(scene::Spatial& spatial, runtime::Ptr<IgnoreProperties> ignore = nullptr, int32_t instanceId = 1) {
	CollisionResults results(1, instanceId, ignore);
	Ray ray(Vector3f(3, 3, 20), Vector3f(0, 0, -1));
	return spatial.collideWith(ray, results);
}

TEST(NodeTest, ChildrenParentsAndClones) {
	GEO_TEST_SCOPE;
	runtime::Ref<Node> root = Node::create(std::string_view("root"));
	EXPECT_EQ(root->getCollisionIntentions(), -1) << "Node(String): CollisionIntention.ALL";
	EXPECT_EQ(Node::create()->getCollisionIntentions(), 0) << "Node(): no intentions";
	runtime::Ref<Node> a = Node::create(std::string_view("a"));
	runtime::Ref<Node> b = Node::create(std::string_view("b"));
	runtime::Ref<Geometry> leaf = Geometry::create("leaf", quadMesh());
	EXPECT_EQ(root->attachChild(a), 1);
	EXPECT_EQ(root->attachChild(b), 2);
	EXPECT_EQ(a->attachChild(leaf), 1);
	EXPECT_EQ(root->attachChild(a), 2) << "already a child: unchanged";
	EXPECT_EQ(root->attachChild(root), 2) << "a node is never its own child";
	EXPECT_THROW(root->attachChild(nullptr), runtime::NullPointerException);
	EXPECT_EQ(leaf->getParent().get(), a.get());
	EXPECT_TRUE(leaf->hasAncestor(*root));
	EXPECT_TRUE(root->hasChild(*leaf)) << "recursive";
	EXPECT_EQ(root->getChild(std::string_view("leaf")).get(), leaf.get());
	EXPECT_EQ(root->getChild(std::optional<std::string_view>()), nullptr);

	b->attachChild(leaf); // moves the leaf from a to b
	EXPECT_EQ(leaf->getParent().get(), b.get());
	EXPECT_EQ(a->getQuantity(), 0);
	root->swapChildren(0, 1);
	EXPECT_EQ(root->getChild(0).get(), b.get());
	EXPECT_EQ(root->getChildIndex(*a), 1);
	EXPECT_EQ(root->detachChildNamed(std::string_view("a")), 1);
	EXPECT_EQ(a->getParent(), nullptr);
	EXPECT_TRUE(leaf->removeFromParent());
	EXPECT_FALSE(leaf->removeFromParent());
	EXPECT_EQ(root->detachChild(leaf), -1);
	root->attachChildAt(a, 0);
	EXPECT_EQ(root->getChild(0).get(), a.get());
	a->attachChild(leaf);
	EXPECT_EQ(root->descendantMatches<Geometry>().size(), 1u);
	EXPECT_EQ(root->descendantMatches(std::string_view("[ab]")).size(), 2u);
	EXPECT_EQ(root->getTriangleCount(), 2);
	EXPECT_EQ(root->getVertexCount(), 12) << "Mesh.getVertexCount is the number of coordinates";
	EXPECT_EQ(leaf->toString(), "leaf (Geometry) use PHYSICAL");

	root->setMaterialId(static_cast<int8_t>(200));
	root->setCollisionIntentions(3);
	runtime::Ref<scene::Spatial> copy = root->clone();
	runtime::Ptr<Node> copyNode = runtime::cast<Node>(copy);
	EXPECT_EQ(copyNode->getName(), "root");
	EXPECT_EQ(copyNode->getMaterialId(), 200);
	EXPECT_EQ(copyNode->getCollisionIntentions(), 3);
	ASSERT_EQ(copyNode->getQuantity(), 2);
	runtime::Ptr<Geometry> copiedLeaf = runtime::cast<Geometry>(runtime::cast<Node>(copyNode->getChild(0))->getChild(0));
	EXPECT_NE(copiedLeaf.get(), leaf.get());
	EXPECT_EQ(copiedLeaf->getMesh().get(), leaf->getMesh().get()) << "the mesh is shared";
	root->detachAllChildren();
	EXPECT_EQ(root->getQuantity(), 0);
}

TEST(DespawnableNodeTest, ActivityAndClone) {
	GEO_TEST_SCOPE;
	runtime::Ref<DespawnableNode> node = despawnable(DespawnableType::PLACEABLE, 12);
	EXPECT_FALSE(node->isActive(1));
	node->setActive(1, true);
	node->setActive(70000, true);
	EXPECT_TRUE(node->isActive(1));
	EXPECT_TRUE(node->isActive(70000));
	EXPECT_THROW(node->setActive(-1, true), runtime::IndexOutOfBoundsException);
	runtime::Ptr<DespawnableNode> copy = runtime::cast<DespawnableNode>(node->clone());
	EXPECT_EQ(copy->type.get(), DespawnableType::PLACEABLE);
	EXPECT_EQ(copy->id.get(), 12);
	EXPECT_TRUE(copy->isActive(70000)) << "instances are copied";
	node->setActive(1, false);
	EXPECT_TRUE(copy->isActive(1)) << "a copy, not shared";
	EXPECT_EQ(copy->getQuantity(), 1);
}

TEST(DespawnableNodeTest, CollisionGates) {
	GEO_TEST_SCOPE;
	// inactive placeable, then active in instance 1 only
	runtime::Ref<DespawnableNode> placeable = despawnable(DespawnableType::PLACEABLE, 7);
	EXPECT_EQ(collide(*placeable), 0);
	placeable->setActive(1, true);
	EXPECT_EQ(collide(*placeable), 1);
	EXPECT_EQ(collide(*placeable, nullptr, 2), 0);
	EXPECT_EQ(collide(*placeable, IgnoreProperties::of(7)), 0) << "the ignored static id";
	EXPECT_EQ(collide(*placeable, IgnoreProperties::of(8)), 1);
	// houses are always active
	EXPECT_EQ(collide(*despawnable(DespawnableType::HOUSE, 1)), 1);

	// events: the node id must equal the event theme id
	GeoCallbacks::setEventThemeIdSupplier([] { return 4; });
	EXPECT_EQ(collide(*despawnable(DespawnableType::EVENT, 4)), 1);
	EXPECT_EQ(collide(*despawnable(DespawnableType::EVENT, 2)), 0);
	GeoCallbacks::setEventThemeIdSupplier(nullptr); // EventService: EventTheme.NONE (id 0)
	EXPECT_EQ(collide(*despawnable(DespawnableType::EVENT, 0)), 1);
	EXPECT_EQ(collide(*despawnable(DespawnableType::EVENT, 4)), 0);

	// shields: 1 has no siege location, 2 is not under shield, 3 is an Elyos shield, 4 a Balaur shield
	GeoCallbacks::setSiegeShieldLookup([](int32_t id) -> std::optional<GeoCallbacks::SiegeShieldState> {
		switch (id) {
			case 2:
				return GeoCallbacks::SiegeShieldState{false, model::siege::SiegeRace::ELYOS};
			case 3:
				return GeoCallbacks::SiegeShieldState{true, model::siege::SiegeRace::ELYOS};
			case 4:
				return GeoCallbacks::SiegeShieldState{true, model::siege::SiegeRace::BALAUR};
			default:
				return std::nullopt;
		}
	});
	EXPECT_EQ(collide(*despawnable(DespawnableType::SHIELD, 1)), 1);
	EXPECT_EQ(collide(*despawnable(DespawnableType::SHIELD, 1), IgnoreProperties::ANY_RACE), 0);
	EXPECT_EQ(collide(*despawnable(DespawnableType::SHIELD, 2)), 0);
	EXPECT_EQ(collide(*despawnable(DespawnableType::SHIELD, 3)), 1);
	EXPECT_EQ(collide(*despawnable(DespawnableType::SHIELD, 3), IgnoreProperties::ELYOS), 0) << "own race passes";
	EXPECT_EQ(collide(*despawnable(DespawnableType::SHIELD, 3), IgnoreProperties::ASMODIANS), 1);
	EXPECT_THROW(collide(*despawnable(DespawnableType::SHIELD, 3), IgnoreProperties::of(9)), runtime::NullPointerException)
		<< "Java: getRace().getRaceId() on a null race";
	EXPECT_EQ(collide(*despawnable(DespawnableType::SHIELD, 4), IgnoreProperties::BALAUR), 0);
	EXPECT_EQ(collide(*despawnable(DespawnableType::SHIELD, 4), IgnoreProperties::ELYOS), 1);
	GeoCallbacks::setSiegeShieldLookup(nullptr);
}

TEST(DespawnableNodeTest, CopyFromRejectsOtherSpatials) {
	GEO_TEST_SCOPE;
	runtime::Ref<Node> source = Node::create(std::string_view("source"));
	source->attachChild(Node::create(std::string_view("nested")));
	runtime::Ref<DespawnableNode> node = DespawnableNode::create();
	node->copyFrom(*source);
	EXPECT_EQ(node->getName(), "source");
	EXPECT_EQ(node->getCollisionIntentions(), -1);
	ASSERT_EQ(node->getQuantity(), 1);
	EXPECT_EQ(node->getChild(0)->getName(), "nested");
}

class GeoMapTest : public ::testing::Test {
protected:
	runtime::TaskScope scope{AION_TASK_INFO(runtime::TaskKind::TEST)};
	runtime::Ref<models::GeoMap> map = models::GeoMap::create(300280000); // RENTUS_BASE

	void SetUp() override {
		map->attachChild(quadNode(0, 0, 5));
		runtime::Ref<models::Terrain> terrain = models::Terrain::create();
		std::vector<int16_t> heights(64, 64); // 8 x 8, z 2
		terrain->setHeightmap(heights, 8, 8);
		std::vector<int8_t> materials(64, 3);
		terrain->setMaterials(materials, 8, 8);
		map->setTerrain(terrain);
		map->updateModelBound();
	}
};

TEST_F(GeoMapTest, GetZ) {
	EXPECT_TRUE(map->hasTerrain());
	EXPECT_TRUE(map->hasTerrainMaterials());
	EXPECT_GEO_FLOAT(map->getZ(3.3f, 4.4f, 20, -10, 1), 5.0f) << "the quad is closer than the terrain";
	EXPECT_GEO_FLOAT(map->getZ(12.5f, 3.3f, 20, -10, 1), 2.0f) << "only terrain";
	EXPECT_GEO_FLOAT(map->getZ(3.3f, 4.4f, 4, -10, 1), 2.0f) << "zMax below the quad";
	EXPECT_GEO_FLOAT(map->getZ(3.3f, 4.4f, 20, 10, 1), NaN) << "zMin above everything";
	EXPECT_GEO_FLOAT(map->getZ(100, 100, 20, -10, 1), NaN) << "outside";
	EXPECT_GEO_FLOAT(map->getZ(3.3f, 4.4f, 20, -10, 1, true), 5.0f) << "a flat quad is not a sloping surface";
}

TEST_F(GeoMapTest, LineOfSightAndCollisions) {
	EXPECT_FALSE(map->canSee(3, 3, 10, 3, 3, 0, 1, nullptr)) << "the quad at z 5 blocks";
	EXPECT_TRUE(map->canSee(3, 3, 10, 3, 3, 6, 1, nullptr));
	EXPECT_FALSE(map->canSee(3, 3, 10, 3, 90, 10, 1, nullptr)) << "more than 80 m";

	// downward from z 10 (+1): the quad at distance 6, the contact moved back by 0.5 m and set to the ground below (getZ = 5)
	Vector3f contact = map->getClosestCollision(3, 3, 10, 3, 3, 0, false, 1, collision::getId(collision::CollisionIntention::PHYSICAL), nullptr);
	EXPECT_TRUE(contact.equals(Vector3f(3, 3, 5))) << contact.toString();
	// no collision: the target, with atNearGroundZ the ground within +1/-2 m of it
	Vector3f free = map->getClosestCollision(12, 3, 10, 13, 3, 10, true, 1, collision::getId(collision::CollisionIntention::PHYSICAL), nullptr);
	EXPECT_TRUE(free.equals(Vector3f(13, 3, 10)));
	Vector3f grounded = map->getClosestCollision(12, 3, 10, 13, 3, 3, true, 1, collision::getId(collision::CollisionIntention::PHYSICAL), nullptr);
	EXPECT_TRUE(grounded.equals(Vector3f(13, 3, 2))) << grounded.toString();

	CollisionResults results = map->getCollisions(3, 3, 10, 3, 3, 0, 1, collision::getId(collision::CollisionIntention::PHYSICAL), nullptr);
	EXPECT_EQ(results.size(), 1) << "the quad (the terrain walk of a vertical ray has 0 / 0 distance factors and finds nothing)";

	Vector3f origin(1, 1, 5);
	Vector3f moved = map->findMovementCollision(origin, 4, 1, 1);
	EXPECT_TRUE(moved.equals(Vector3f(4, 1, 5))) << moved.toString();
	EXPECT_TRUE(origin.equals(Vector3f(4, 1, 5))) << "the origin is moved in place";
}

TEST_F(GeoMapTest, TerrainMaterial) {
	EXPECT_EQ(map->getTerrainMaterialAt(12.5f, 3.5f, 2, 1), 3);
	EXPECT_EQ(map->getTerrainMaterialAt(3.5f, 3.5f, 2, 1), 3) << "the quad at z 5 is outside z +- 1";
	EXPECT_EQ(map->getTerrainMaterialAt(3.5f, 3.5f, 5.5f, 1), 0) << "the terrain is not within z +- 1";
	EXPECT_EQ(map->getTerrainMaterialAt(3.5f, 3.5f, 4.5f, 1), 0) << "neither the terrain nor the quad within z +- 1";
}

TEST_F(GeoMapTest, ChunksDespawnablesAndDoors) {
	runtime::Ref<DespawnableNode> placeable = despawnable(DespawnableType::PLACEABLE, 11, 300, 10);
	map->attachChild(placeable);
	runtime::Ref<DespawnableNode> houseDoor = despawnable(DespawnableType::HOUSE_DOOR, 21, 10, 300);
	map->attachChild(houseDoor);
	runtime::Ref<DespawnableNode> town = despawnable(DespawnableType::TOWN_OBJECT, 31, 0, 0);
	town->levelBitMask.set(0b00110);
	map->attachChild(town);
	runtime::Ref<DespawnableNode> door = despawnable(DespawnableType::DOOR_STATE1, 41, 20, 20);
	map->attachChild(door);
	EXPECT_EQ(map->getEntityCount(), 5);
	EXPECT_EQ(map->getQuantity(), 3) << "chunks 0 (x 0..255, y 0..255), 1000 (x 256..) and 1 (y 256..)";
	EXPECT_EQ(map->getGeometries().size(), 5u);

	map->spawnPlaceableObject(1, 11);
	EXPECT_TRUE(placeable->isActive(1));
	map->despawnPlaceableObject(1, 11);
	EXPECT_FALSE(placeable->isActive(1));
	map->spawnPlaceableObject(1, 999); // unknown: ignored

	map->setHouseDoorState(1, 21, model::house::HouseDoorState::CLOSED);
	EXPECT_TRUE(houseDoor->isActive(1));
	map->setHouseDoorState(1, 21, model::house::HouseDoorState::OPEN);
	EXPECT_FALSE(houseDoor->isActive(1));

	map->updateTownToLevel(31, 2);
	EXPECT_TRUE(town->isActive(1)) << "bit 1 << (2 - 1) is in the mask";
	map->updateTownToLevel(31, 1);
	EXPECT_FALSE(town->isActive(1));

	const bool geoEnabled = configs::main::GeoDataConfig::GEO_ENABLE.load();
	configs::main::GeoDataConfig::GEO_ENABLE.store(true);
	{
		LogCapture log("com.aionemu.gameserver.geoEngine.models.GeoMap");
		map->setDoorState(1, 41, true);
		EXPECT_FALSE(door->isActive(1)) << "state 1 (closed) is inactive when open";
		EXPECT_TRUE(log.contains("warning|Door state 2 not available for door 41 in world 300280000"));
		map->setDoorState(1, 145, true);
		EXPECT_FALSE(log.contains("door 145")) << "an ignorable door of RENTUS_BASE";
		map->setDoorState(1, 146, true);
		EXPECT_TRUE(log.contains("warning|No geometry found for door 146 in world 300280000"));
	}
	runtime::Ref<models::GeoMap> unknown = models::GeoMap::create(12345);
	EXPECT_THROW(unknown->setDoorState(1, 1, true), runtime::NullPointerException) << "Java: switch on WorldMapType.getWorld(12345) == null";
	configs::main::GeoDataConfig::GEO_ENABLE.store(false);
	unknown->setDoorState(1, 1, true); // no geo: no warning and no switch
	configs::main::GeoDataConfig::GEO_ENABLE.store(geoEnabled);

	try {
		map->attachChild(despawnable(DespawnableType::SHIELD, 1));
		FAIL() << "expected IllegalArgumentException";
	} catch (const runtime::IllegalArgumentException& e) {
		EXPECT_STREQ(e.what(), "SHIELD is not implemented");
	}
}

} // namespace
} // namespace aion::gameserver::geoEngine::test
