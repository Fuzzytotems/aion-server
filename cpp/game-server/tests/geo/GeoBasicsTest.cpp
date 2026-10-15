// P4-04 small classes: CollisionIntention and DespawnableType companions, IgnoreProperties, TempVars, CollisionResult(s), the Java float
// min/max helpers and GeoWorldLoader.getVectorHash. Expectations are derived by hand from CollisionIntention.java, DespawnableNode.java,
// IgnoreProperties.java, TempVars.java, CollisionResult.java, CollisionResults.java, java.lang.Math and GeoWorldLoader.java.

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <set>
#include <string>
#include <vector>

#include "GeoTestSupport.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/geoEngine/GeoWorldLoader.h"
#include "aion/gameserver/geoEngine/collision/CollisionIntentionInfo.h"
#include "aion/gameserver/geoEngine/collision/CollisionResult.h"
#include "aion/gameserver/geoEngine/collision/CollisionResults.h"
#include "aion/gameserver/geoEngine/collision/IgnoreProperties.h"
#include "aion/gameserver/geoEngine/scene/DespawnableNode_DespawnableTypeInfo.h"
#include "aion/gameserver/geoEngine/scene/Geometry.h"
#include "aion/gameserver/geoEngine/utils/JavaMathFloat.h"
#include "aion/gameserver/geoEngine/utils/TempVars.h"

namespace aion::gameserver::geoEngine::test {
namespace {

using collision::CollisionIntention;
using collision::CollisionResult;
using collision::CollisionResults;
using math::Vector3f;

constexpr float NaN = std::numeric_limits<float>::quiet_NaN();

TEST(CollisionIntentionTest, JavaByteIds) {
	EXPECT_EQ(collision::getId(CollisionIntention::NONE), 0);
	EXPECT_EQ(collision::getId(CollisionIntention::PHYSICAL), 1);
	EXPECT_EQ(collision::getId(CollisionIntention::MATERIAL), 2);
	EXPECT_EQ(collision::getId(CollisionIntention::MOVEABLE), 64);
	EXPECT_EQ(collision::getId(CollisionIntention::PHYSICAL_SEE_THROUGH), -128) << "(byte) (1 << 7)";
	EXPECT_EQ(collision::getId(CollisionIntention::DEFAULT_COLLISIONS), -111) << "1 | 16 | -128";
	EXPECT_EQ(collision::getId(CollisionIntention::CANT_SEE_COLLISIONS), 17);
	EXPECT_EQ(collision::getId(CollisionIntention::ALL), -1);
}

TEST(CollisionIntentionTest, FlagsAndToString) {
	EXPECT_EQ(collision::getFlagsFromValue(17), (std::set<CollisionIntention>{CollisionIntention::PHYSICAL, CollisionIntention::DOOR,
													 CollisionIntention::CANT_SEE_COLLISIONS}));
	EXPECT_EQ(collision::getFlagsFromValue(-1).size(), 10u) << "every constant but NONE and ALL";
	EXPECT_EQ(collision::collisionIntentionToString(3), "PHYSICAL, MATERIAL");
	EXPECT_EQ(collision::collisionIntentionToString(0), "");
	EXPECT_EQ(collision::collisionIntentionToString(-111), "PHYSICAL, DOOR, PHYSICAL_SEE_THROUGH, DEFAULT_COLLISIONS, CANT_SEE_COLLISIONS")
		<< "a sign-extended byte value";
	EXPECT_EQ(collision::collisionIntentionToString(128), "") << "128 & -128 is 128, not -128";
}

TEST(DespawnableTypeTest, GetById) {
	EXPECT_EQ(scene::getById(0), scene::DespawnableNode_DespawnableType::NONE);
	EXPECT_EQ(scene::getById(5), scene::DespawnableNode_DespawnableType::TOWN_OBJECT);
	EXPECT_EQ(scene::getById(8), scene::DespawnableNode_DespawnableType::SHIELD);
	EXPECT_EQ(scene::getId(scene::DespawnableNode_DespawnableType::DOOR_STATE2), 7);
	try {
		scene::getById(9);
		FAIL() << "expected IllegalArgumentException";
	} catch (const runtime::IllegalArgumentException& e) {
		EXPECT_STREQ(e.what(), "Invalid ID 9");
	}
	EXPECT_THROW(scene::getById(-1), runtime::IllegalArgumentException);
}

TEST(IgnorePropertiesTest, SharedConstantsAndNewObjects) {
	GEO_TEST_SCOPE;
	using collision::IgnoreProperties;
	EXPECT_EQ(IgnoreProperties::of(model::Race::ELYOS).get(), IgnoreProperties::ELYOS.get());
	EXPECT_EQ(IgnoreProperties::of(model::Race::ASMODIANS, 0).get(), IgnoreProperties::ASMODIANS.get());
	EXPECT_EQ(IgnoreProperties::of(model::Race::DRAKAN).get(), IgnoreProperties::BALAUR.get());
	runtime::Ref<IgnoreProperties> withStaticId = IgnoreProperties::of(model::Race::ELYOS, 5);
	EXPECT_NE(withStaticId.get(), IgnoreProperties::ELYOS.get());
	EXPECT_EQ(withStaticId->getRace(), model::Race::ELYOS);
	EXPECT_EQ(withStaticId->getStaticId(), 5);
	runtime::Ref<IgnoreProperties> staticOnly = IgnoreProperties::of(7);
	EXPECT_FALSE(staticOnly->getRace().has_value()) << "Java: null race";
	EXPECT_NE(IgnoreProperties::of(std::nullopt, 0).get(), IgnoreProperties::ANY_RACE.get()) << "of(null, 0) creates a new object";
	EXPECT_EQ(IgnoreProperties::ANY_RACE->toString(), "[IgnoreProperties] Race: null staticId: 0");
	EXPECT_EQ(withStaticId->toString(), "[IgnoreProperties] Race: ELYOS staticId: 5");
}

TEST(TempVarsTest, StackDiscipline) {
	using utils::TempVars;
	TempVars& a = TempVars::get();
	TempVars& b = TempVars::get();
	EXPECT_NE(&a, &b);
	b.release();
	TempVars& again = TempVars::get();
	EXPECT_EQ(&again, &b) << "the released instance is reused";
	again.release();
	a.release();
	try {
		a.release();
		FAIL() << "expected IllegalStateException";
	} catch (const runtime::IllegalStateException& e) {
		EXPECT_STREQ(e.what(), "This instance of TempVars was already released!");
	}

	TempVars& first = TempVars::get();
	TempVars& second = TempVars::get();
	try {
		first.release(); // wrong order: the stack top is `second`
		FAIL() << "expected IllegalStateException";
	} catch (const runtime::IllegalStateException& e) {
		EXPECT_STREQ(e.what(), "An instance of TempVars has not been released in a called method!");
	}
	EXPECT_THROW(second.release(), runtime::IllegalStateException) << "the index already moved, so the check fails again";

	std::vector<TempVars*> held;
	for (int i = 0; i < 5; i++)
		held.push_back(&TempVars::get());
	EXPECT_THROW(TempVars::get(), runtime::ArrayIndexOutOfBoundsException) << "STACK_SIZE is 5";
	for (auto it = held.rbegin(); it != held.rend(); ++it)
		(*it)->release();
	TempVars::get().release(); // the stack is usable again
}

TEST(JavaMathFloatTest, NaNAndSignedZero) {
	using utils::javaMax;
	using utils::javaMin;
	EXPECT_TRUE(std::isnan(javaMax(NaN, 1.0f)));
	EXPECT_TRUE(std::isnan(javaMax(1.0f, NaN)));
	EXPECT_TRUE(std::isnan(javaMin(1.0f, NaN)));
	EXPECT_EQ(bitsOf(javaMax(-0.0f, 0.0f)), 0u);
	EXPECT_EQ(bitsOf(javaMax(0.0f, -0.0f)), 0u);
	EXPECT_EQ(bitsOf(javaMin(0.0f, -0.0f)), 0x80000000u);
	EXPECT_EQ(bitsOf(javaMin(-0.0f, 0.0f)), 0x80000000u);
	EXPECT_EQ(javaMax(2.0f, 3.0f), 3.0f);
	EXPECT_EQ(javaMin(2.0f, 3.0f), 2.0f);
}

TEST(CollisionResultsTest, SortingSkipsNaNAndIsStable) {
	CollisionResults results(1, 1);
	results.addCollision(CollisionResult(Vector3f(5, 0, 0), 5));
	results.addCollision(CollisionResult(Vector3f(9, 9, 9), NaN));
	results.addCollision(CollisionResult(Vector3f(1, 0, 0), 1));
	results.addCollision(CollisionResult(Vector3f(3, 0, 0), 3));
	results.addCollision(CollisionResult(Vector3f(1, 1, 1), 1));
	ASSERT_EQ(results.size(), 4) << "NaN distances are dropped";
	EXPECT_EQ(results.getCollisionDirect(0).getDistance(), 5.0f) << "insertion order before sorting";
	EXPECT_TRUE(results.getClosestCollision()->getContactPoint().equals(Vector3f(1, 0, 0))) << "stable: the first of the equal distances";
	EXPECT_TRUE(results.getCollision(1).getContactPoint().equals(Vector3f(1, 1, 1)));
	EXPECT_EQ(results.getFarthestCollision()->getDistance(), 5.0f);
	EXPECT_EQ(results.getCollisionDirect(3).getDistance(), 5.0f) << "sorted in place";
	EXPECT_THROW(results.getCollision(4), runtime::IndexOutOfBoundsException);

	std::vector<float> distances;
	for (const CollisionResult& result : results)
		distances.push_back(result.getDistance());
	EXPECT_EQ(distances, (std::vector<float>{1, 1, 3, 5}));

	auto it = results.iterator();
	it.next();
	it.remove();
	it.next();
	it.next();
	it.remove();
	EXPECT_EQ(results.size(), 2);
	EXPECT_TRUE(results.getCollision(0).getContactPoint().equals(Vector3f(1, 1, 1)));
	EXPECT_EQ(results.getCollision(1).getDistance(), 5.0f);

	results.clear();
	EXPECT_FALSE(results.getClosestCollision().has_value()) << "Java: null";
	EXPECT_EQ(results.toString(), "CollisionResults[]");
}

TEST(CollisionResultsTest, OnlyFirstKeepsInsertionOrder) {
	CollisionResults results(1, 1, true);
	results.addCollision(CollisionResult(Vector3f(5, 0, 0), 5));
	results.addCollision(CollisionResult(Vector3f(1, 0, 0), 1));
	EXPECT_EQ(results.getClosestCollision()->getDistance(), 5.0f) << "addCollision leaves `sorted` true when onlyFirst";
	EXPECT_TRUE(results.isOnlyFirst());
	EXPECT_EQ(results.getIntentions(), 1);
	EXPECT_EQ(results.getInstanceId(), 1);
	EXPECT_EQ(results.getIgnoreProperties(), nullptr);
}

TEST(CollisionResultTest, CompareToAndEquals) {
	GEO_TEST_SCOPE;
	std::vector<float> vertices{0, 0, 0, 1, 0, 0, 0, 1, 0};
	std::vector<int32_t> indices{0, 1, 2};
	runtime::Ref<scene::Mesh> mesh = makeMesh(vertices, indices, 1);
	runtime::Ref<scene::Geometry> a = scene::Geometry::create("a.cgf", mesh);
	runtime::Ref<scene::Geometry> b = scene::Geometry::create("a.cgf", mesh);
	CollisionResult first(Vector3f(1, 2, 3), 1.0f);
	CollisionResult second(Vector3f(1, 2, 3), 2.0f);
	EXPECT_EQ(first.compareTo(second), -1);
	EXPECT_EQ(second.compareTo(first), 1);
	EXPECT_EQ(CollisionResult(Vector3f(), -0.0f).compareTo(CollisionResult(Vector3f(), 0.0f)), -1) << "Float.compare";
	CollisionResult sameAsFirst(Vector3f(1, 2, 3), 1.0f);
	first.setGeometry(a);
	sameAsFirst.setGeometry(b);
	EXPECT_TRUE(first.equals(sameAsFirst)) << "the geometry names are compared";
	EXPECT_FALSE(first.equals(second));
	CollisionResult noGeometry(Vector3f(1, 2, 3), 1.0f);
	EXPECT_THROW(noGeometry.equals(sameAsFirst), runtime::NullPointerException) << "Java: geometry.getName() on null";
	EXPECT_TRUE(noGeometry.equals(noGeometry)) << "identity first";

	CollisionResults results(1, 1);
	results.addCollision(first);
	EXPECT_THROW(results.setGeometryDirect(1, a), runtime::IndexOutOfBoundsException);
	results.setGeometryDirect(0, b);
	EXPECT_EQ(results.getCollisionDirect(0).getGeometry().get(), b.get());
}

TEST(GeoWorldLoaderTest, VectorHash) {
	// (long) floatToIntBits: 1f = 0x3F800000, 2f = 0x40000000, 3f = 0x40400000;
	// 1065353216 * 73856093 ^ 1073741824 * 19349669 ^ 1077936128 * 83492791 = 27557996166905856, % 700001 = 696145
	EXPECT_EQ(GeoWorldLoader::getVectorHash(Vector3f(1, 2, 3)), 696145);
	// -1f = 0xBF800000 (negative long), NaN = 0x7FC00000 (canonical): the remainder keeps the dividend's sign
	EXPECT_EQ(GeoWorldLoader::getVectorHash(Vector3f(-1, 0, NaN)), -256815);
	EXPECT_EQ(GeoWorldLoader::getVectorHash(Vector3f(1234.5678f, -876.25f, 99.125f)), -558370);
}

} // namespace
} // namespace aion::gameserver::geoEngine::test
