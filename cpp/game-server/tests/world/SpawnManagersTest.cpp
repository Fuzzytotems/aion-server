// The two spawn managers of the M5a world (P4-10, m5a-plan.md W-04): StaticDoorSpawnManager::spawnTemplate and
// StaticObjectSpawnManager::spawnTemplate. Expectations are hand-derived from StaticDoorSpawnManager.java:22-36 and
// StaticObjectSpawnManager.java:23-53 (the null item template early return, the pool branch with resetPoolSpots/reserveRandomFreePoolSpot, the
// PlayerAwareKnownList, the GeoService.setDoorState call and the "Spawned N static doors in <instance>" log). The Java server is never run.

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "WorldTestSupport.h"

#include "aion/gameserver/controllers/StaticObjectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/SpawnsData.bind.h"
#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/dataholders/StaticDoorData.bind.h"
#include "aion/gameserver/dataholders/StaticDoorData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/gameobjects/StaticDoor.h"
#include "aion/gameserver/model/gameobjects/StaticObject.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/staticdoor/StaticDoorState.h"
#include "aion/gameserver/model/templates/staticdoor/StaticDoorTemplate.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/spawnengine/StaticDoorSpawnManager.h"
#include "aion/gameserver/spawnengine/StaticObjectSpawnManager.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/geo/GeoService.h"
#include "aion/gameserver/world/knownlist/PlayerAwareKnownList.h"

namespace aion::gameserver::spawnengine::test {
namespace {

using model::gameobjects::VisibleObject;
using model::templates::staticdoor::StaticDoorState;
using world::test::POETA;
using world::test::publishTestStaticData;

/** Two doors on the Poeta test map: id 11 state 2 (CLICKABLE, closed) and id 12 state 3 (OPENED | CLICKABLE, open). */
const char* const STATIC_DOORS_XML = R"(<staticdoor_templates>
	<world world="210010000">
		<staticdoor id="11" x="110.0" y="120.0" z="30.0" state="2"/>
		<staticdoor id="12" x="130.0" y="140.0" z="31.0" state="3"/>
	</world>
</staticdoor_templates>)";

/** One item template: StaticObjectSpawnManager looks the spawn group's npc id up in ITEM_DATA, not in NPC_DATA. */
const char* const ITEM_TEMPLATES_XML = R"(<item_templates>
	<item_template id="700001" name="M5aStaticObject" level="1"/>
</item_templates>)";

constexpr int32_t STATIC_OBJECT_ITEM_ID = 700001;
constexpr int32_t UNKNOWN_ITEM_ID = 700002;

/**
 * Publishes the two holders this file needs, once per process. Both are unpublished in aion_gs_world_tests unless WorldRealDataTest ran first,
 * which publishTestStaticData() reports through its return value.
 */
void publishSpawnManagerData() {
	static const bool published = [] {
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		xml::LoadContext context;
		if (!dataholders::DataManager::STATICDOOR_DATA)
			dataholders::DataManager::STATICDOOR_DATA.publish(xml::bindString<dataholders::StaticDoorData>(context, STATIC_DOORS_XML));
		if (!dataholders::DataManager::ITEM_DATA)
			dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(context, ITEM_TEMPLATES_XML));
		return true;
	}();
	static_cast<void>(published);
}

class SpawnManagersTest : public ::testing::Test {
protected:
	void SetUp() override {
		if (!publishTestStaticData())
			GTEST_SKIP() << "this process published the real static data (run the test on its own)";
		publishSpawnManagerData();
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		if (dataholders::DataManager::STATICDOOR_DATA->getStaticDoors(POETA).size() != 2)
			GTEST_SKIP() << "another test published the real static door data";
		// GeoDataConfig::GEO_ENABLE is false by default: init() only creates an empty GeoMap per world map, which is what setDoorState needs to find
		static const bool geoInitialised = [] {
			world::geo::GeoService::getInstance().init();
			return true;
		}();
		static_cast<void>(geoInitialised);
	}

	void TearDown() override {
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		for (const runtime::Ref<VisibleObject>& object : spawned)
			world::World::getInstance().removeObject(*object);
		spawned.clear();
		spawns.reset();
		runtime::Reclaimer::getInstance().drain();
	}

	/** The main Poeta instance; a Ptr may only be used in the task scope it was borrowed in, so every test takes it in its own scope. */
	static world::WorldMapInstance& poeta() { return *world::World::getInstance().getWorldMap(POETA)->getMainWorldMapInstance(); }

	static std::set<int32_t> objectIdsIn(world::WorldMapInstance& instance) {
		std::set<int32_t> ids;
		for (const runtime::Ptr<VisibleObject>& object : instance)
			ids.insert(object->getObjectId());
		return ids;
	}

	/** The objects of the instance that were not there before the manager ran. */
	static std::vector<runtime::Ref<VisibleObject>> collectSpawned(world::WorldMapInstance& instance, const std::set<int32_t>& before) {
		std::vector<runtime::Ref<VisibleObject>> result;
		for (const runtime::Ptr<VisibleObject>& object : instance) {
			if (!before.contains(object->getObjectId()))
				result.emplace_back(*object);
		}
		return result;
	}

	/** The spawn groups of one npc id, the way SpawnEngine.spawnAll takes them out of SPAWNS_DATA. */
	std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>> groupsOf(const char* xml, int32_t npcId) {
		xml::LoadContext context;
		spawns = xml::bindString<dataholders::SpawnsData>(context, xml);
		return spawns->getSpawnsForNpc(POETA, npcId);
	}

	std::unique_ptr<dataholders::SpawnsData> spawns;
	std::vector<runtime::Ref<VisibleObject>> spawned;
};

TEST_F(SpawnManagersTest, StaticDoorSpawnManagerSpawnsEveryDoorOfTheMapWithItsStaticIdAndGeoState) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	world::WorldMapInstance& instance = poeta();
	const std::set<int32_t> before = objectIdsIn(instance);

	StaticDoorSpawnManager::spawnTemplate(instance);

	spawned = collectSpawned(instance, before);
	ASSERT_EQ(spawned.size(), 2u) << "one door per STATICDOOR_DATA entry of the map";
	std::set<int32_t> staticIds;
	for (const runtime::Ref<VisibleObject>& object : spawned) {
		auto* door = dynamic_cast<model::gameobjects::StaticDoor*>(object.get());
		ASSERT_NE(door, nullptr);
		// Java: SpawnEngine.newSingleTimeSpawn(mapId, 300001, x, y, z, (byte) 0); spawn.setStaticId(data.getId())
		ASSERT_TRUE(door->getSpawn());
		EXPECT_EQ(door->getSpawn()->getNpcId(), 300001);
		staticIds.insert(door->getSpawn()->getStaticId());
		EXPECT_EQ(door->getWorldId(), POETA);
		EXPECT_EQ(door->getInstanceId(), instance.getInstanceId());
		EXPECT_TRUE(door->isSpawned()) << "bringIntoWorld stores, positions and spawns the door";
		EXPECT_NE(dynamic_cast<world::knownlist::PlayerAwareKnownList*>(&door->getKnownList()), nullptr);
		ASSERT_NE(door->getObjectTemplate(), nullptr);
		// the open state the manager passes to GeoService.setDoorState comes from the template's state flags
		EXPECT_EQ(door->isOpen(), door->getStates().contains(StaticDoorState::OPENED));
		if (door->getSpawn()->getStaticId() == 11) {
			EXPECT_FALSE(door->isOpen()) << "state 2 is CLICKABLE only";
			EXPECT_EQ(door->getStates(), (std::set<StaticDoorState>{StaticDoorState::CLICKABLE}));
			EXPECT_FLOAT_EQ(door->getX(), 110.0f);
			EXPECT_FLOAT_EQ(door->getY(), 120.0f);
			EXPECT_FLOAT_EQ(door->getZ(), 30.0f);
		} else {
			EXPECT_EQ(door->getSpawn()->getStaticId(), 12);
			EXPECT_TRUE(door->isOpen()) << "state 3 is OPENED | CLICKABLE";
			EXPECT_EQ(door->getStates(), (std::set<StaticDoorState>{StaticDoorState::OPENED, StaticDoorState::CLICKABLE}));
		}
	}
	EXPECT_EQ(staticIds, (std::set<int32_t>{11, 12}));
	// Java: setDoorState finds no geometry (geo data is disabled), which is a no-op and must not throw
	EXPECT_NO_THROW(world::geo::GeoService::getInstance().setDoorState(POETA, instance.getInstanceId(), 11, true));
}

TEST_F(SpawnManagersTest, StaticObjectSpawnManagerReturnsWithoutAnItemTemplate) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	world::WorldMapInstance& instance = poeta();
	const std::set<int32_t> before = objectIdsIn(instance);

	// Java: ItemData.getItemTemplate returns null for an unknown id and spawnTemplate returns at once
	ASSERT_EQ(dataholders::DataManager::ITEM_DATA->getItemTemplate(UNKNOWN_ITEM_ID), nullptr);
	std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>> groups =
		groupsOf(R"(<spawns><spawn_map map_id="210010000"><spawn npc_id="700002" respawn_time="0">)"
				 R"(<spot x="300" y="300" z="20"/></spawn></spawn_map></spawns>)",
			UNKNOWN_ITEM_ID);
	ASSERT_EQ(groups.size(), 1u);

	StaticObjectSpawnManager::spawnTemplate(*groups[0], instance.getInstanceId());

	EXPECT_EQ(objectIdsIn(instance), before) << "no item template, no object";
}

TEST_F(SpawnManagersTest, StaticObjectSpawnManagerSpawnsOneObjectPerSpotWithoutAPool) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	world::WorldMapInstance& instance = poeta();
	const std::set<int32_t> before = objectIdsIn(instance);
	std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>> groups =
		groupsOf(R"(<spawns><spawn_map map_id="210010000"><spawn npc_id="700001" respawn_time="0">)"
				 R"(<spot x="200" y="210" z="20"/><spot x="220" y="230" z="21"/><spot x="240" y="250" z="22"/>)"
				 R"(</spawn></spawn_map></spawns>)",
			STATIC_OBJECT_ITEM_ID);
	ASSERT_EQ(groups.size(), 1u);
	ASSERT_FALSE(groups[0]->hasPool());

	StaticObjectSpawnManager::spawnTemplate(*groups[0], instance.getInstanceId());

	spawned = collectSpawned(instance, before);
	ASSERT_EQ(spawned.size(), 3u) << "one StaticObject per spot of the group";
	std::set<float> xs;
	for (const runtime::Ref<VisibleObject>& object : spawned) {
		auto* staticObject = dynamic_cast<model::gameobjects::StaticObject*>(object.get());
		ASSERT_NE(staticObject, nullptr);
		EXPECT_EQ(staticObject->getObjectTemplate(), dataholders::DataManager::ITEM_DATA->getItemTemplate(STATIC_OBJECT_ITEM_ID));
		EXPECT_TRUE(staticObject->isSpawned()) << "bringIntoWorld stores, positions and spawns the object";
		EXPECT_EQ(staticObject->getInstanceId(), instance.getInstanceId());
		EXPECT_EQ(staticObject->getWorldId(), POETA);
		EXPECT_NE(dynamic_cast<world::knownlist::PlayerAwareKnownList*>(&staticObject->getKnownList()), nullptr);
		EXPECT_EQ(&static_cast<controllers::StaticObjectController&>(staticObject->getController()).getOwner(), staticObject);
		xs.insert(staticObject->getX());
	}
	EXPECT_EQ(xs, (std::set<float>{200.0f, 220.0f, 240.0f}));
}

TEST_F(SpawnManagersTest, StaticObjectSpawnManagerPoolBranchSpawnsPoolDistinctSpots) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	world::WorldMapInstance& instance = poeta();
	const std::set<int32_t> before = objectIdsIn(instance);
	// Java: a pool of 2 over 4 spots; resetPoolSpots, then one reserveRandomFreePoolSpot per iteration
	std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>> groups =
		groupsOf(R"(<spawns><spawn_map map_id="210010000"><spawn npc_id="700001" respawn_time="0" pool="2">)"
				 R"(<spot x="400" y="400" z="20"/><spot x="410" y="400" z="20"/><spot x="420" y="400" z="20"/><spot x="430" y="400" z="20"/>)"
				 R"(</spawn></spawn_map></spawns>)",
			STATIC_OBJECT_ITEM_ID);
	ASSERT_EQ(groups.size(), 1u);
	ASSERT_TRUE(groups[0]->hasPool());
	EXPECT_EQ(groups[0]->getPool(), 2);

	StaticObjectSpawnManager::spawnTemplate(*groups[0], instance.getInstanceId());

	spawned = collectSpawned(instance, before);
	ASSERT_EQ(spawned.size(), 2u) << "getPool() objects, one per reserved free spot";
	std::set<float> xs;
	for (const runtime::Ref<VisibleObject>& object : spawned) {
		EXPECT_TRUE(object->isSpawned());
		xs.insert(object->getX());
	}
	EXPECT_EQ(xs.size(), 2u) << "reserveRandomFreePoolSpot never returns the same spot twice";
	for (float x : xs)
		EXPECT_TRUE(x == 400.0f || x == 410.0f || x == 420.0f || x == 430.0f) << x;
}

} // namespace
} // namespace aion::gameserver::spawnengine::test
