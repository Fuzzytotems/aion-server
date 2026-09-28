// World chunk at M5a (P4-10/P4-11a, m5a-plan.md W-04 and W-05): World.storeObject/removeObject feed the leak census, and the FlyRing and Road
// constructors (FlyRingService, RoadService at startup) on the test maps of WorldTestSupport.h. Expectations are hand-derived from World.java,
// FlyRing.java, Road.java and Plane3D.java.

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "WorldTestSupport.h"

#include "aion/gameserver/controllers/FlyRingController.h"
#include "aion/gameserver/controllers/RoadController.h"
#include "aion/gameserver/model/flyring/FlyRing.h"
#include "aion/gameserver/model/road/Road.h"
#include "aion/gameserver/model/templates/flyring/FlyRingTemplate.bind.h"
#include "aion/gameserver/model/templates/flyring/FlyRingTemplate.h"
#include "aion/gameserver/model/templates/road/RoadTemplate.bind.h"
#include "aion/gameserver/model/templates/road/RoadTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/runtime/services/LeakCensus.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/knownlist/PlayerAwareKnownList.h"

namespace aion::gameserver::world::test {
namespace {

using model::gameobjects::VisibleObject;

class WorldM5aTest : public ::testing::Test {
protected:
	void SetUp() override {
		if (!publishTestStaticData())
			GTEST_SKIP() << "this process published the real static data (run the test on its own)";
		utils::ThreadPoolManager::installBackend(nullptr);
		utils::ThreadPoolManager::installBackend(std::make_unique<runtime::DeterministicExecutor>(clock, 9));
	}

	void TearDown() override {
		runtime::LeakCensus::getInstance().uninstall();
		runtime::LeakCensus::getInstance().configure(runtime::LeakCensus::Config{});
		runtime::Reclaimer::getInstance().drain();
		utils::ThreadPoolManager::installBackend(nullptr);
		runtime::Reclaimer::getInstance().drain();
	}

	runtime::ManualClock clock{0};
};

TEST_F(WorldM5aTest, RemoveObjectFeedsTheLeakCensusAndStoreObjectTakesTheObjectOutAgain) {
	runtime::LeakCensus& census = runtime::LeakCensus::getInstance();
	census.install();
	World* worldPointer = nullptr;
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		worldPointer = &World::getInstance();
	}
	World& world = *worldPointer;
	runtime::Ref<TestObject> object;
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		object = VisibleObject::create<TestObject>(88001, POETA, 95.0f);
		ASSERT_TRUE(world.setPosition(*object, POETA, 300, 300, 10, int8_t{0}));
		world.storeObject(*object);
	}
	runtime::Reclaimer::getInstance().drain();
	EXPECT_EQ(census.trackedCount(), 0u) << "an object that was never removed is not tracked";

	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		ASSERT_TRUE(world.removeObject(*object));
		EXPECT_FALSE(world.removeObject(*object)) << "a second removal is no census event";
	}
	runtime::Reclaimer::getInstance().drain();
	EXPECT_EQ(census.trackedCount(), 1u);

	// the final census of the check-output mode (m5a-plan.md D8): zero thresholds and two scans report every removed object still referenced
	runtime::LeakCensus::Config finalCensus;
	finalCensus.censusAfter = std::chrono::milliseconds(0);
	finalCensus.checkInterval = std::chrono::milliseconds(0);
	census.configure(finalCensus);
	runtime::Reclaimer::getInstance().reclaimNow();
	runtime::Reclaimer::getInstance().reclaimNow();
	std::vector<runtime::LeakCensus::LeakReport> leaks = census.getLeaks();
	ASSERT_EQ(leaks.size(), 1u);
	EXPECT_EQ(leaks[0].objectId, 88001);
	EXPECT_NE(leaks[0].className.find("TestObject"), std::string::npos) << leaks[0].className;

	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		world.storeObject(*object); // re-added (a respawn, a re-entering player)
	}
	runtime::Reclaimer::getInstance().drain();
	EXPECT_EQ(census.trackedCount(), 0u) << "storeObject takes the object out of the census";
	EXPECT_TRUE(census.getLeaks().empty());

	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		ASSERT_TRUE(world.removeObject(*object));
	}
	object.reset();
	runtime::Reclaimer::getInstance().drain();
	runtime::Reclaimer::getInstance().drain();
	EXPECT_EQ(census.trackedCount(), 0u) << "destroyed objects leave the census";
}

const char* const FLY_RING_XML =
	R"(<fly_ring name="POETA_FLY_RING_TEST" map="210010000" radius="5"><center x="100" y="100" z="50"/><p1 x="100" y="110" z="50"/>)"
	R"(<p2 x="100" y="100" z="60"/></fly_ring>)";

TEST_F(WorldM5aTest, FlyRingIsPositionedAtItsCenterWithAPlayerAwareKnownList) {
	xml::LoadContext context;
	const model::templates::flyring::FlyRingTemplate* template_ =
		xml::bindString<model::templates::flyring::FlyRingTemplate>(context, FLY_RING_XML).release(); // immortal static data
	utils::idfactory::IDFactory::getInstance().resetForTests();
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	runtime::Ref<model::flyring::FlyRing> ring = VisibleObject::create<model::flyring::FlyRing>(template_, 1);
	EXPECT_EQ(ring->getWorldId(), POETA);
	EXPECT_EQ(ring->getInstanceId(), 1);
	EXPECT_FLOAT_EQ(ring->getX(), 100.0f);
	EXPECT_FLOAT_EQ(ring->getZ(), 50.0f);
	EXPECT_EQ(ring->getName(), "POETA_FLY_RING_TEST");
	EXPECT_EQ(ring->getTemplate(), template_);
	EXPECT_EQ(ring->getSpawn(), nullptr);
	EXPECT_EQ(ring->getObjectTemplate(), nullptr) << "Java: super(..., null, null, position, true)";
	EXPECT_NE(dynamic_cast<knownlist::PlayerAwareKnownList*>(&ring->getKnownList()), nullptr);
	EXPECT_EQ(&static_cast<controllers::FlyRingController&>(ring->getController()).getOwner(), ring.get());
	// the plane x = 100 through the center: a move from x 95 to 105 at the center crosses it within the radius, one 20 m aside does not
	EXPECT_TRUE(ring->isCrossed(geoEngine::math::Vector3f(95, 100, 50), geoEngine::math::Vector3f(105, 100, 50)));
	EXPECT_FALSE(ring->isCrossed(geoEngine::math::Vector3f(95, 120, 50), geoEngine::math::Vector3f(105, 120, 50)));
	EXPECT_THROW(static_cast<void>(VisibleObject::create<model::flyring::FlyRing>(template_, 9)), runtime::IllegalArgumentException)
		<< "World.createPosition -> WorldMap.getWorldMapInstance: Poeta has fewer instances than 9";
}

TEST_F(WorldM5aTest, RoadNeedsAnInstanceIdAndIsStoredWhenSpawned) {
	xml::LoadContext context;
	const model::templates::road::RoadTemplate* template_ = xml::bindString<model::templates::road::RoadTemplate>(context,
		R"(<road name="POETA_ROAD_TEST" map="210010000" radius="3"><center x="200" y="200" z="20"/><p1 x="200" y="210" z="20"/>)"
		R"(<p2 x="200" y="200" z="30"/></road>)")
		.release();
	utils::idfactory::IDFactory::getInstance().resetForTests();
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	EXPECT_THROW(static_cast<void>(VisibleObject::create<model::road::Road>(template_, std::nullopt)), runtime::NullPointerException)
		<< "Java: the Integer instance id is unboxed";
	runtime::Ref<model::road::Road> road = VisibleObject::create<model::road::Road>(template_, 1);
	EXPECT_EQ(road->getName(), "POETA_ROAD_TEST");
	EXPECT_EQ(&static_cast<controllers::RoadController&>(road->getController()).getOwner(), road.get());
	EXPECT_FALSE(World::getInstance().findVisibleObject(road->getObjectId()));
	road->spawn();
	EXPECT_EQ(World::getInstance().findVisibleObject(road->getObjectId()).get(), road.get()) << "Road.spawn stores the object";
	EXPECT_TRUE(road->isSpawned());
	EXPECT_TRUE(World::getInstance().removeObject(*road));
}

} // namespace
} // namespace aion::gameserver::world::test
