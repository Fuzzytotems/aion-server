// World chunk (P4-10): World, WorldMap, WorldMapInstance (2D and 3D regions), MapRegion neighbours and zones, and ZoneService's zone instances
// over the small test holders of WorldTestSupport.h. Expected region and zone counts are derived by hand from WorldMap2DInstance.java,
// WorldMap3DInstance.java (neighbour loops), WorldMapInstance.filterZones and the intersectsRectangle rules of PolyArea (java.awt Polygon
// .intersects), CylinderArea (2D distance < radius) and SphereArea (3D distance <= radius).

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <vector>

#include "WorldTestSupport.h"

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"

#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/templates/zone/ZoneClassName.h"
#include "aion/gameserver/model/templates/zone/ZoneTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/world/MapRegion.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMap2DInstance.h"
#include "aion/gameserver/world/WorldMap3DInstance.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldMapInstanceFactory.h"
#include "aion/gameserver/world/WorldMapType.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/zone/FlyZoneInstance.h"
#include "aion/gameserver/world/zone/PvPZoneInstance.h"
#include "aion/gameserver/world/zone/ZoneAttributes.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"
#include "aion/gameserver/world/zone/ZoneName.h"
#include "aion/gameserver/world/zone/ZoneService.h"
#include "aion/gameserver/world/zone/handler/GeneralZoneHandler.h"

namespace aion::gameserver::world::test {
namespace {

using model::templates::zone::ZoneClassName;

class WorldZonesTest : public ::testing::Test {
protected:
	void SetUp() override {
		if (!publishTestStaticData())
			GTEST_SKIP() << "this process published the real static data (run the test on its own)";
	}

	runtime::TaskScope scope{AION_TASK_INFO(runtime::TaskKind::TEST)};

	static runtime::Ptr<WorldMapInstance> poeta() { return World::getInstance().getWorldMap(POETA)->getMainWorldMapInstance(); }

	/** the region of the 2D cell (x, y) of Poeta's main instance */
	static runtime::Ptr<MapRegion> cell(int32_t x, int32_t y) {
		return poeta()->getRegion(static_cast<float>(x * 128 + 1), static_cast<float>(y * 128 + 1), 0);
	}
};

TEST_F(WorldZonesTest, ZoneServiceCreatesTheZoneInstancesOfAMap) {
	std::unordered_map<const zone::ZoneName*, runtime::Ref<zone::ZoneInstance>> zones =
		zone::ZoneService::getInstance().getZoneInstancesByWorldId(POETA);
	ASSERT_EQ(zones.size(), 5u) << "the whole map zone plus the four zones of the map";

	const zone::ZoneName* wholeMap = zone::ZoneName::get("210010000");
	ASSERT_TRUE(zones.contains(wholeMap)) << "WorldZoneTemplate names the whole map zone by the map id";
	EXPECT_EQ(zones.at(wholeMap)->getZoneTemplate()->getZoneType(), ZoneClassName::DUMMY);
	EXPECT_EQ(typeid(*zones.at(wholeMap)), typeid(zone::ZoneInstance));

	runtime::Ref<zone::ZoneInstance> fly = zones.at(zone::ZoneName::get("FLY_AREA_210010000"));
	EXPECT_NE(dynamic_cast<zone::FlyZoneInstance*>(fly.get()), nullptr);
	runtime::Ref<zone::ZoneInstance> pvp = zones.at(zone::ZoneName::get("PVP_AREA_210010000"));
	EXPECT_NE(dynamic_cast<zone::PvPZoneInstance*>(pvp.get()), nullptr);
	runtime::Ref<zone::ZoneInstance> sub = zones.at(zone::ZoneName::get("SUB_PRIORITY_210010000"));
	EXPECT_EQ(typeid(*sub), typeid(zone::ZoneInstance)) << "SUB zones outside the invasion list are plain zone instances";

	EXPECT_TRUE(fly->isInsideCordinate(200, 200, 100));
	EXPECT_FALSE(fly->isInsideCordinate(200, 200, 250)) << "above the polygon's top";
	EXPECT_TRUE(pvp->isInsideCordinate(630, 600, 50));
	EXPECT_FALSE(pvp->isInsideCordinate(660, 600, 50));
	EXPECT_TRUE(sub->isInsideCordinate(140, 150, 55));

	// flags: the FLY zone has its own flags (8 = FLY), the whole map zone (flags of the map) and a zone without flags (-1) use the map
	EXPECT_TRUE(fly->canFly());
	EXPECT_FALSE(fly->canGlide()) << "zone flags 8 contain FLY only";
	EXPECT_TRUE(sub->canGlide()) << "flags -1: the map's GLIDE";
	EXPECT_TRUE(zones.at(wholeMap)->canRecall());
	EXPECT_FALSE(zones.at(wholeMap)->canRide());
	EXPECT_TRUE(pvp->isPvpAllowed()) << "PVP zone flags 64 = PVP_ENABLED";
	EXPECT_FALSE(sub->isPvpAllowed()) << "not a PVP zone: the map, which has no PVP flag";

	// each call creates new instances (every WorldMapInstance owns its zone instances) sharing the whole map template
	auto again = zone::ZoneService::getInstance().getZoneInstancesByWorldId(POETA);
	EXPECT_NE(again.at(wholeMap).get(), zones.at(wholeMap).get());
	EXPECT_EQ(again.at(wholeMap)->getZoneTemplate(), zones.at(wholeMap)->getZoneTemplate());
	EXPECT_THROW(zone::ZoneService::getInstance().getZoneInstancesByWorldId(1), runtime::NullPointerException);

	runtime::Ref<zone::handler::ZoneHandler> handler = zone::ZoneService::getInstance().getNewZoneHandler(zone::ZoneName::get("FLY_AREA_210010000"));
	EXPECT_NE(dynamic_cast<zone::handler::GeneralZoneHandler*>(handler.get()), nullptr) << "no registered or collidable handler";
}

TEST_F(WorldZonesTest, WorldCreatesTheMapsAndTheirInstances) {
	World& world = World::getInstance();
	ASSERT_TRUE(world.getWorldMap(POETA));
	EXPECT_FALSE(world.getWorldMap(1));
	runtime::Ptr<WorldMap> poetaMap = world.getWorldMap(POETA);
	EXPECT_EQ(poetaMap->getName(), "Poeta");
	EXPECT_EQ(poetaMap->getWorldSize(), 1024);
	EXPECT_TRUE(poetaMap->isFlightAllowed());
	EXPECT_TRUE(poetaMap->canGlide());
	EXPECT_FALSE(poetaMap->canRide());
	EXPECT_TRUE(poetaMap->canReturnToBattle());
	EXPECT_EQ(poetaMap->getInstanceCount(), 1);
	EXPECT_EQ(poetaMap->getAvailableInstanceIds(), std::vector<int32_t>{1});
	EXPECT_EQ(typeid(*poetaMap->getMainWorldMapInstance()), typeid(WorldMap2DInstance));
	EXPECT_THROW(poetaMap->getWorldMapInstance(2), runtime::IllegalArgumentException) << "not an instance map: more than the twin count";
	EXPECT_EQ(poetaMap->getWorldMapInstance(0), poetaMap->getMainWorldMapInstance()) << "instance id 0 is the default instance";
	// C++ only (M4 check mode): the names of the instance's zones, the whole map zone plus the four zones of the map
	std::vector<std::string> zoneNames;
	for (const zone::ZoneName* zoneName : poetaMap->getMainWorldMapInstance()->getZoneNames())
		zoneNames.push_back(zoneName->name());
	std::ranges::sort(zoneNames);
	EXPECT_EQ(zoneNames, (std::vector<std::string>{"210010000", "FLY_AREA_210010000", "PVP_AREA_210010000", "SUB_PLAIN_210010000",
							 "SUB_PRIORITY_210010000"}));

	runtime::Ptr<WorldMap> ishalgen = world.getWorldMap(ISHALGEN);
	EXPECT_EQ(ishalgen->getInstanceCount(), 3) << "two twins plus one beginner twin";
	EXPECT_EQ(ishalgen->getAvailableInstanceIds().size(), 3u);
	EXPECT_FALSE(ishalgen->getWorldMapInstance(2)->isBeginnerInstance());
	EXPECT_TRUE(ishalgen->getWorldMapInstance(3)->isBeginnerInstance());

	runtime::Ptr<WorldMap> dredgion = world.getWorldMap(DREDGION);
	EXPECT_TRUE(dredgion->isInstanceType());
	runtime::Ptr<instance::handlers::InstanceHandler> handler = dredgion->getMainWorldMapInstance()->getInstanceHandler();
	EXPECT_NE(dynamic_cast<instance::handlers::GeneralInstanceHandler*>(handler.get()), nullptr);
	EXPECT_FALSE(dredgion->getWorldMapInstance(5)) << "instance maps accept any id (no instance 5 exists)";

	runtime::Ptr<WorldMap> reshanta = world.getWorldMap(RESHANTA);
	EXPECT_EQ(typeid(*reshanta->getMainWorldMapInstance()), typeid(WorldMap3DInstance));

	// setWorldOption / removeWorldOption / hasOverridenOption
	ishalgen->setWorldOption(zone::ZoneAttributes::GLIDE);
	EXPECT_TRUE(ishalgen->canGlide());
	EXPECT_TRUE(ishalgen->hasOverridenOption(zone::ZoneAttributes::GLIDE));
	ishalgen->removeWorldOption(zone::ZoneAttributes::RIDE);
	EXPECT_FALSE(ishalgen->canRide());
	EXPECT_TRUE(ishalgen->hasOverridenOption(zone::ZoneAttributes::RIDE));
	ishalgen->setWorldOption(zone::ZoneAttributes::RIDE);
	ishalgen->removeWorldOption(zone::ZoneAttributes::GLIDE);
	EXPECT_FALSE(ishalgen->hasOverridenOption(zone::ZoneAttributes::RIDE));
	EXPECT_FALSE(ishalgen->hasOverridenOption(zone::ZoneAttributes::GLIDE));

	EXPECT_EQ(poetaMap->getMainWorldMapInstance()->toString(), "WorldMapInstance 210010000 [1]");
}

TEST_F(WorldZonesTest, TwoDimensionalRegionsAndNeighbours) {
	runtime::Ptr<WorldMapInstance> instance = poeta();
	// 1024 / 128 + 1 = 9 cells per axis
	for (int32_t x = 0; x <= 8; x++) {
		for (int32_t y = 0; y <= 8; y++) {
			runtime::Ptr<MapRegion> region = cell(x, y);
			ASSERT_TRUE(region) << x << "," << y;
			EXPECT_EQ(region->getRegionId(), x * 1000 + y);
			EXPECT_EQ(&region->getParent(), instance.get());
			int32_t expectedNeighbours = (x == 0 || x == 8 ? 2 : 3) * (y == 0 || y == 8 ? 2 : 3); // including the region itself
			runtime::Ptr<runtime::Array<MapRegion*>> neighbours = region->getNeighbours();
			ASSERT_EQ(neighbours->length(), expectedNeighbours) << x << "," << y;
			EXPECT_EQ(neighbours->get(0), region.get()) << "the region itself comes first";
		}
	}
	EXPECT_FALSE(instance->getRegion(1200, 10, 0)) << "outside the map";
	EXPECT_FALSE(instance->getRegion(10, -200, 0));

	// zones per region: the whole map zone everywhere, the FLY polygon (100..300) in the cells 0..2, the spheres (140, 140, r 20) in the cells
	// 0..1, the PVP cylinder (600, 600, r 50) in (4, 4), (5, 4) and (4, 5) but not (5, 5) (distance 56.6 > 50)
	EXPECT_EQ(cell(0, 0)->getZoneCount(), 4);
	EXPECT_EQ(cell(1, 1)->getZoneCount(), 4);
	EXPECT_EQ(cell(2, 2)->getZoneCount(), 2);
	EXPECT_EQ(cell(2, 1)->getZoneCount(), 2);
	EXPECT_EQ(cell(3, 3)->getZoneCount(), 1);
	EXPECT_EQ(cell(4, 4)->getZoneCount(), 2);
	EXPECT_EQ(cell(5, 4)->getZoneCount(), 2);
	EXPECT_EQ(cell(4, 5)->getZoneCount(), 2);
	EXPECT_EQ(cell(5, 5)->getZoneCount(), 1);
	EXPECT_EQ(cell(8, 8)->getZoneCount(), 1);

	EXPECT_TRUE(cell(1, 1)->isInsideZone(zone::ZoneName::get("FLY_AREA_210010000"), 200, 200, 100));
	EXPECT_FALSE(cell(3, 3)->isInsideZone(zone::ZoneName::get("FLY_AREA_210010000"), 200, 200, 100)) << "the region does not hold the zone";
	runtime::Ref<WorldPosition> inPvp = WorldPosition::create(POETA, 620, 610, 10, int8_t{0});
	EXPECT_TRUE(instance->isInsideZone(*inPvp, zone::ZoneName::get("PVP_AREA_210010000")));
	runtime::Ref<WorldPosition> outside = WorldPosition::create(POETA, 700, 700, 10, int8_t{0});
	EXPECT_FALSE(instance->isInsideZone(*outside, zone::ZoneName::get("PVP_AREA_210010000")));
	EXPECT_FALSE(cell(0, 0)->isActive()) << "regions activate with players";
}

TEST_F(WorldZonesTest, ThreeDimensionalRegionsAndNeighbours) {
	runtime::Ptr<WorldMapInstance> instance = World::getInstance().getWorldMap(RESHANTA)->getMainWorldMapInstance();
	// 512 / 128 + 1 = 5 cells for x and y; z from 0 while z < round(512 / 128) * 128 = 512: 4 cells
	int32_t regions = 0;
	for (int32_t x = 0; x <= 4; x++) {
		for (int32_t y = 0; y <= 4; y++) {
			for (int32_t z = 0; z < 4; z++) {
				runtime::Ptr<MapRegion> region = instance->getRegion(static_cast<float>(x * 128 + 5), static_cast<float>(y * 128 + 5), static_cast<float>(z * 128 + 5));
				ASSERT_TRUE(region);
				regions++;
				EXPECT_EQ(region->getRegionId(), x * 1000000 + y * 1000 + z);
				// Java's z loop stops before z + regionSize: the neighbours are the cells z - 1 and z (not z + 1)
				int32_t expected = (x == 0 || x == 4 ? 2 : 3) * (y == 0 || y == 4 ? 2 : 3) * (z == 0 ? 1 : 2);
				EXPECT_EQ(region->getNeighbours()->length(), expected) << x << "," << y << "," << z;
				EXPECT_EQ(region->getZoneCount(), 1) << "only the whole map zone (Reshanta has no zones in the test data)";
			}
		}
	}
	EXPECT_EQ(regions, 100);
	EXPECT_FALSE(instance->getRegion(5, 5, 520)) << "above the last z cell";
	EXPECT_FALSE(instance->isPersonal());
	EXPECT_EQ(instance->getOwnerId(), 0);
}

// Review finding (wave 3b-2): WorldMap and WorldMap3DInstance placed quiescent points under any QuiescentScope a caller had opened. They now
// require World creation's explicit QuiescentOptIn (main's world step, World::World's elements); a quiescent point shows as a new scope id.
TEST_F(WorldZonesTest, WorldMapsTakeQuiescentPointsOnlyUnderTheWorldCreationOptIn) {
	const auto* ishalgen = dataholders::DataManager::WORLD_MAPS_DATA->getTemplate(ISHALGEN);
	const auto* reshanta = dataholders::DataManager::WORLD_MAPS_DATA->getTemplate(RESHANTA);
	ASSERT_NE(ishalgen, nullptr);
	ASSERT_NE(reshanta, nullptr);
	auto takesQuiescentPoints = [](const std::function<void()>& create) {
		uint64_t before = runtime::TaskScope::currentScopeId();
		create();
		return runtime::TaskScope::currentScopeId() != before;
	};
	runtime::QuiescentScope quiescent; // quiescent-safe: this body holds Refs and template pointers only
	runtime::Ref<WorldMap> reshantaMap = WorldMap::create(reshanta);
	EXPECT_FALSE(takesQuiescentPoints([&] { WorldMap::create(ishalgen); })) << "a QuiescentScope alone: 3 instances without quiescent points";
	EXPECT_FALSE(takesQuiescentPoints([&] { WorldMapInstanceFactory::createWorldMapInstance(*reshantaMap, 0); })) << "3D regions";
	{
		runtime::QuiescentOptIn worldCreation(runtime::QuiescentOptIn::WORLD_CREATION);
		EXPECT_TRUE(takesQuiescentPoints([&] { WorldMap::create(ishalgen); }));
		runtime::Ref<WorldMapInstance> instance;
		EXPECT_TRUE(takesQuiescentPoints([&] { instance = WorldMapInstanceFactory::createWorldMapInstance(*reshantaMap, 0); })) << "3D regions";
		{
			runtime::Ptr<MapRegion> region = instance->getRegion(5, 5, 5);
			ASSERT_TRUE(region) << "the same regions";
			EXPECT_EQ(region->getNeighbours()->length(), 4) << "and neighbours (x 0, y 0, z 0: 2 * 2 * 1)";
		}
		{
			runtime::QuiescentScope later; // opened below the opt-in by a frame it does not vouch for
			EXPECT_FALSE(takesQuiescentPoints([&] { WorldMap::create(ishalgen); }));
		}
	}
}

TEST_F(WorldZonesTest, CreatePositionAndSetPosition) {
	World& world = World::getInstance();
	runtime::Ref<WorldPosition> position = world.createPosition(POETA, 300, 400, 10, int8_t{20}, 1);
	ASSERT_TRUE(position->getMapRegion());
	EXPECT_EQ(position->getMapRegion()->getRegionId(), 2003);
	EXPECT_EQ(position->getInstanceId(), 1);
	EXPECT_FALSE(position->isInstanceMap());
	EXPECT_EQ(position->getWorldMapInstance(), poeta());
	EXPECT_THROW(world.createPosition(1, 0, 0, 0, int8_t{0}, 1), runtime::NullPointerException);
	EXPECT_THROW(world.createPosition(DREDGION, 0, 0, 0, int8_t{0}, 9), runtime::NullPointerException);
	EXPECT_THROW(world.createPosition(POETA, 5000, 0, 0, int8_t{0}, 1), runtime::NullPointerException);
	EXPECT_FALSE(world.setPosition(runtime::Ptr<model::gameobjects::VisibleObject>(), POETA, 1, 0, 0, 0, int8_t{0}));
}

} // namespace
} // namespace aion::gameserver::world::test
