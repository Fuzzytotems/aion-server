// World chunk (P4-10): what the world's own containers keep of an object that World.removeObject took out of the world. Regression tests of
// the leak of the 2026-09-24 client session: the leak census warned that an Npc (object id 25582) was still alive 10 minutes after its removal
// from the world with refcount 1 and no pinning task, and the same npc was still alive at shutdown, after every task had run or been cancelled
// and no player was left - one reference from a structure that lives until shutdown and reaches no player.
//
// World.despawn leaves the zones and the object map of the object's FINAL map region only (World.java:320-323), and MapRegion.revalidateZones
// tests isSpawned and the zone geometry before ZoneInstance.onEnter takes the zone's monitor (MapRegion.java:147-161). ZoneUpdateService
// revalidates a moving npc on its own thread every 500 ms while MoveTaskManager moves it every 200 ms, so live play (a kiting Mage, an npc
// chasing it across a 128 m region line, a corpse that decays) produces interleavings the scenario gates never do:
// - a revalidation that tested a zone against the position before a region crossing enters the zone after the mover left it; when the zone is
//   not in the new region's list, no later revalidation - World.despawn included - ever leaves it again;
// - a revalidation that tested isSpawned before the corpse decayed (World.removeObject) enters a zone that despawn had just left;
// - two concurrent position updates leave the object in a second region's object map, which despawn does not visit.
// Java keeps the same ghosts, rooted in World, so its garbage collector does not collect them either. The C++ port closes the first two in
// MapRegion.revalidateZones, which tests and enters or leaves each zone under that zone's monitor (every leave path takes it too), and drops the
// third in World.removeObject (docs/deviations/P4-10.md). The races run under the PCT scheduler with directed schedules: the revalidation
// stops before each of its monitor acquisitions in turn while the other thread runs to completion.
//
// The npc is a plain NpcController npc spawned the server's way (VisibleObjectSpawner.spawnNpc) on the Poeta test map of WorldTestSupport.h:
// cell (1, 1) holds both SUB spheres around (140, 140, 50), cell (2, 1) holds neither; the whole map zone and the FLY polygon cover both.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <vector>

#include <spdlog/sinks/ostream_sink.h>

#include "WorldTestSupport.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/NpcSkillData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/zone/ZoneTemplate.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/YieldPoint.h"
#include "aion/gameserver/runtime/lifetime/Pct.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/runtime/services/LeakCensus.h"
#include "aion/gameserver/spawnengine/VisibleObjectSpawner.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/MapRegion.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldLeakProbe.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"
#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::world::test {
namespace {

using model::gameobjects::Npc;
using model::gameobjects::VisibleObject;
using runtime::Ptr;
using runtime::Ref;
namespace pct = runtime::pct;

constexpr int32_t NPC_ID = 210663;

const char* const NPC_TEMPLATES = R"(<npc_templates>)"
								  R"(<npc_template npc_id="210663" level="2" name_id="1" name="test monster" race="ELYOS" rank="NOVICE" rating="NORMAL" tribe="GENERAL">)"
								  R"(<stats maxHp="500" maxMp="200"/></npc_template>)"
								  R"(</npc_templates>)";

/** inside both SUB spheres of cell (1, 1) (10 m from their center) and inside the FLY polygon */
constexpr float INSIDE_X = 150;
constexpr float INSIDE_Y = 140;
constexpr float Z = 50;
/** 1 m east of the line x = 256: cell (2, 1), outside both spheres, still inside the FLY polygon */
constexpr float EAST_X = 257;

class WorldContainerLifetimeTest : public ::testing::Test {
protected:
	void SetUp() override {
		if (!publishTestStaticData())
			GTEST_SKIP() << "this process published the real static data (run the test on its own)";
		utils::ThreadPoolManager::installBackend(nullptr);
		executor = new runtime::DeterministicExecutor(clock, 4);
		utils::ThreadPoolManager::installBackend(std::unique_ptr<runtime::DeterministicExecutor>(executor));
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		dataholders::DataManager::NPC_SKILL_DATA.publish(std::make_unique<dataholders::NpcSkillData>());
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(context, NPC_TEMPLATES));
		spawnGroup = model::templates::spawns::SpawnGroup::create(POETA, NPC_ID, 0, nullptr);
		spawnTemplate = model::templates::spawns::SpawnTemplate::create(*spawnGroup, INSIDE_X, INSIDE_Y, Z, int8_t{0}, 0, std::nullopt, 0);
		world = &World::getInstance();
	}

	void TearDown() override {
		{
			runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
			for (const Ref<Npc>& npc : spawned) { // a failed test leaves its npc in the world
				if (world->findVisibleObject(npc->getObjectId()).get() == npc.get())
					world->removeObject(*npc);
			}
		}
		spawned.clear();
		spawnTemplate = nullptr;
		spawnGroup = nullptr;
		runtime::Reclaimer::getInstance().drain();
		utils::ThreadPoolManager::installBackend(nullptr);
		executor = nullptr;
		runtime::Reclaimer::getInstance().drain();
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::NPC_SKILL_DATA.resetForTests();
	}

	/**
	 * The server's spawn path at (INSIDE_X, INSIDE_Y, Z): VisibleObjectSpawner.spawnNpc gives the npc its NpcKnownList and EffectController and
	 * brings it into the world (storeObject, setPosition, spawn, whose onAfterSpawn revalidates the zones of the npc's region)
	 */
	Ref<Npc> spawnNpc() {
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		Ref<VisibleObject> object = spawnengine::VisibleObjectSpawner::spawnNpc(*spawnTemplate, 1);
		Ref<Npc> npc(runtime::as<Npc>(Ptr<VisibleObject>(object)));
		EXPECT_TRUE(npc && npc->isSpawned());
		spawned.push_back(npc);
		return npc;
	}

	/** the names of the zones of the npc's region and its neighbours that have an entry for it (Java MapRegion.findZones) */
	static std::vector<std::string> zonesHolding(Npc& npc) {
		std::vector<std::string> names;
		std::vector<zone::ZoneInstance*> seen;
		for (MapRegion* region : *npc.getPosition()->getMapRegion()->getNeighbours()) {
			for (Ptr<zone::ZoneInstance> zone : region->findZones(npc)) {
				if (std::ranges::find(seen, zone.get()) != seen.end())
					continue;
				seen.push_back(zone.get());
				names.push_back(zone->getZoneTemplate()->getName()->name());
			}
		}
		std::ranges::sort(names);
		return names;
	}

	/** the ids of the npc's region and its neighbours whose object map holds this very npc */
	static std::vector<int32_t> regionsHolding(Npc& npc) {
		std::vector<int32_t> ids;
		for (MapRegion* region : *npc.getPosition()->getMapRegion()->getNeighbours()) {
			if (region->getObjects().get(npc.getObjectId()).get() == &npc)
				ids.push_back(region->getRegionId());
		}
		std::ranges::sort(ids);
		return ids;
	}

	static std::string describe(const std::vector<std::string>& names) {
		std::ostringstream out;
		for (const std::string& name : names)
			out << (out.tellp() > 0 ? ", " : "") << name;
		return "[" + out.str() + "]";
	}

	/** drops the fixture's reference to an npc that left the world, so the test's own Ref is the only one that should remain */
	void forget(Npc& npc) {
		std::erase_if(spawned, [&npc](const Ref<Npc>& held) { return held.get() == &npc; });
	}

	/** World.removeObject, what the RespawnService DecayTask does with a corpse */
	void removeFromWorld(Npc& npc) {
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		EXPECT_TRUE(world->removeObject(npc));
		forget(npc);
	}

	/** runs what is due and destroys what was retired (a released map node frees its Ref only then), outside any task scope */
	void settle() {
		executor->runReady();
		runtime::Reclaimer::getInstance().drain();
		runtime::Reclaimer::getInstance().drain();
	}

#if AION_PCT
	using NpcAction = std::function<void(Npc&)>;
	using ScheduleCheck = std::function<void(Ref<Npc>&, const std::string&)>;

	/** the two thread bodies of one schedule, each in its own task scope (a pool task) */
	static std::vector<std::function<void()>> bodies(const Ref<Npc>& npc, const NpcAction& first, const NpcAction& second) {
		return {
			[npc, first] {
				runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
				first(*npc);
			},
			[npc, second] {
				runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
				second(*npc);
			},
		};
	}

	/**
	 * Directed PCT schedules: a solo run counts the monitor acquisitions of `first` on a freshly spawned npc; then, for each of them in turn and a
	 * fresh npc each time, `first` runs up to right before that acquisition, `second` runs to completion and `first` finishes. `check` gets the
	 * npc and a name of the schedule after each one.
	 */
	void interleaveBeforeEveryLock(const NpcAction& first, const NpcAction& second, const ScheduleCheck& check) {
		uint32_t locks = 0;
		{
			Ref<Npc> npc = spawnNpc();
			pct::Options options;
			options.keepTrace = true;
			options.script = {{0, ""}, {1, ""}};
			pct::ScheduleResult solo = pct::PctScheduler(options).run(bodies(npc, first, second));
			ASSERT_TRUE(solo.completed) << solo.failure;
			for (const std::string& site : solo.trace)
				locks += site == "t0:Monitor::lock" ? 1 : 0;
			ASSERT_GE(locks, 4u) << "a revalidation of cell (1, 1) locks its four zones";
		}
		for (uint32_t occurrence = 1; occurrence <= locks; ++occurrence) {
			Ref<Npc> npc = spawnNpc();
			{
				runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
				ASSERT_EQ(zonesHolding(*npc).size(), 4u) << "the spawn entered the four zones of cell (1, 1): " << describe(zonesHolding(*npc));
			}
			pct::Options options;
			options.script = {{0, "Monitor::lock", occurrence}, {1, ""}, {0, ""}};
			pct::ScheduleResult result = pct::PctScheduler(options).run(bodies(npc, first, second));
			std::string schedule = "the other thread before lock " + std::to_string(occurrence) + " of " + std::to_string(locks);
			ASSERT_TRUE(result.completed) << schedule << ": " << result.failure;
			check(npc, schedule);
		}
	}
#endif

	runtime::ManualClock clock{0};
	runtime::DeterministicExecutor* executor = nullptr; // owned by ThreadPoolManager
	xml::LoadContext context;
	Ref<model::templates::spawns::SpawnGroup> spawnGroup;
	Ref<model::templates::spawns::SpawnTemplate> spawnTemplate;
	World* world = nullptr;
	std::vector<Ref<Npc>> spawned;
};

TEST_F(WorldContainerLifetimeTest, AStaleSecondRegionEntryIsDroppedWhenTheNpcIsDeleted) {
	Ref<Npc> npc = spawnNpc();
	Ref<MapRegion> cell11;
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		cell11 = Ref<MapRegion>(npc->getPosition()->getMapRegion());
		world->updatePosition(*npc, EAST_X, INSIDE_Y, Z, int8_t{0});
		ASSERT_EQ(regionsHolding(*npc), std::vector<int32_t>{2001}) << "updatePosition moved the npc from cell (1, 1) to cell (2, 1)";
		// a second World.updatePosition that read cell (1, 1) as the old region before the first one stored cell (2, 1) removes the npc from
		// cell (1, 1) again and adds it to its own new region; with both moves ending in cells next to each other the npc sits in two maps
		cell11->add(*npc);
		ASSERT_EQ(regionsHolding(*npc), (std::vector<int32_t>{1001, 2001}));
	}
	removeFromWorld(*npc);
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		EXPECT_FALSE(cell11->getObjects().get(npc->getObjectId())) << "World.despawn removes the npc from cell (2, 1) only";
		EXPECT_TRUE(regionsHolding(*npc).empty());
	}
	settle();
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	EXPECT_EQ(npc->refCount(), 1u) << "only this test's reference may be left; regions still holding the npc: " << regionsHolding(*npc).size();
}

TEST_F(WorldContainerLifetimeTest, AZoneRevalidationRacingARegionCrossingLeavesNoEntryBehind) {
#if !AION_PCT
	GTEST_SKIP() << "PCT yield points exist only in checked builds";
#else
	// ZoneUpdateService.callTask (Creature.revalidateZones on cell (1, 1)) against MoveTaskManager moving the npc across the line x = 256 into cell
	// (2, 1), which does not list the spheres. Before the fix the schedules that stop the revalidation at a sphere's onEnter leave the npc in
	// that sphere for good.
	interleaveBeforeEveryLock([](Npc& npc) { npc.revalidateZones(); },
		[this](Npc& npc) { world->updatePosition(npc, EAST_X, INSIDE_Y, Z, int8_t{0}); },
		[this](Ref<Npc>& npc, const std::string& schedule) {
			{
				runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
				EXPECT_EQ(zonesHolding(*npc), (std::vector<std::string>{"210010000", "FLY_AREA_210010000"}))
					<< schedule << ": the npc is in cell (2, 1), 117 m from the spheres; zones holding it " << describe(zonesHolding(*npc));
			}
			removeFromWorld(*npc);
			settle();
			runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
			EXPECT_EQ(npc->refCount(), 1u) << schedule << ": only this test's reference may be left after the removal; zones still holding the npc "
										  << describe(zonesHolding(*npc));
		});
#endif
}

TEST_F(WorldContainerLifetimeTest, AZoneRevalidationRacingTheCorpsesRemovalLeavesNoEntryBehind) {
#if !AION_PCT
	GTEST_SKIP() << "PCT yield points exist only in checked builds";
#else
	// ZoneUpdateService.callTask against the RespawnService DecayTask (World.removeObject: despawn leaves every zone of cell (1, 1)). Before the
	// fix every schedule that stops the revalidation at a zone's onEnter leaves the npc in that zone for good.
	interleaveBeforeEveryLock([](Npc& npc) { npc.revalidateZones(); }, [this](Npc& npc) { world->removeObject(npc); },
		[this](Ref<Npc>& npc, const std::string& schedule) {
			forget(*npc);
			{
				runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
				EXPECT_TRUE(zonesHolding(*npc).empty()) << schedule << ": zones holding the removed npc " << describe(zonesHolding(*npc));
			}
			settle();
			runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
			EXPECT_EQ(npc->refCount(), 1u) << schedule << ": only this test's reference may be left; zones still holding the npc "
										  << describe(zonesHolding(*npc));
		});
#endif
}

TEST_F(WorldContainerLifetimeTest, TheLeakProbeNamesTheWorldStructuresThatHoldARemovedNpc) {
	// what the leak census cannot say: whose reference keeps a removed object alive (WorldLeakProbe, the census holder probe)
	Ref<Npc> leaked = spawnNpc();
	Ref<Npc> watcher = spawnNpc();
	Ref<MapRegion> cell11;
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		cell11 = Ref<MapRegion>(leaked->getPosition()->getMapRegion());
		ASSERT_EQ(zonesHolding(*leaked).size(), 4u) << describe(zonesHolding(*leaked));
		// a lost position update: the position says cell (2, 1), while the world still has the npc in cell (1, 1) and in both spheres. The
		// removal leaves cell (2, 1)'s zones (the whole map zone and the FLY polygon are shared with cell (1, 1)), drops the cell (1, 1) entry
		// and logs the spheres, which it cannot leave silently.
		leaked->getPosition()->setXYZH(EAST_X, INSIDE_Y, Z, int8_t{0});
		leaked->getPosition()->setMapRegion(World::getInstance().getWorldMap(POETA)->getMainWorldMapInstance()->getRegion(EAST_X, INSIDE_Y, Z));
	}
	removeFromWorld(*leaked);
	std::vector<std::string> holders;
	std::string watcherName;
	std::string ishalgenRegion;
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		watcher->setTarget(Ptr<VisibleObject>(*leaked)); // a target kept after the removal (CreatureController.notSee in TARGET_LOST)
		cell11->add(*leaked); // an updatePosition that passed its isSpawned test before the removal
		// an entry in another map's instance, which the removal does not visit: the old map of a World.setPosition that raced an updatePosition
		Ptr<MapRegion> farRegion = World::getInstance().getWorldMap(ISHALGEN)->getWorldMapInstance(2)->getRegion(100, 100, 0);
		ASSERT_TRUE(farRegion);
		farRegion->add(*leaked);
		ishalgenRegion = std::to_string(farRegion->getRegionId());
		holders = WorldLeakProbe::findHolders(*leaked);
		std::ranges::sort(holders);
		watcherName = watcher->toString();
		// the test's own cleanup of what it planted
		watcher->setTarget(nullptr);
		cell11->remove(*leaked);
		farRegion->remove(*leaked);
		for (MapRegion* region : *cell11->getNeighbours())
			for (Ptr<zone::ZoneInstance> zone : region->findZones(*leaked))
				zone->onLeave(*leaked);
	}
	const std::string instance = " of WorldMapInstance 210010000 [1]";
	std::vector<std::string> expected{
		"MapRegion.objects of region 1001" + instance,
		"MapRegion.objects of region " + ishalgenRegion + " of WorldMapInstance 220010000 [2]",
		"ZoneInstance.creatures of zone SUB_PLAIN_210010000" + instance,
		"ZoneInstance.creatures of zone SUB_PRIORITY_210010000" + instance,
		"the target of " + watcherName,
	};
	std::ranges::sort(expected);
	EXPECT_EQ(holders, expected);
}

TEST_F(WorldContainerLifetimeTest, TheLeakProbeSaysWhatItSearchedWhenNothingHoldsTheObject) {
	// the census reported a removed npc that nothing in the world holds (the S-1 warning: refcount 1, no task): the probe names what it searched
	// and what it could not, instead of staying silent (the census's HolderProbe entry, WorldLeakProbe::probe, logs through logHolders)
	Ref<Npc> leaked = spawnNpc();
	removeFromWorld(*leaked);
	std::ostringstream captured;
	auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(captured);
	sink->set_pattern("%v");
	commons::logging::LoggerFactory::configure("com.aionemu.gameserver.world.WorldLeakProbe", {.sinks = {sink}, .additive = false});
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		EXPECT_TRUE(WorldLeakProbe::findHolders(*leaked).empty());
		const runtime::LeakCensus::ProbedLeak leak{leaked.get(), "Npc", leaked->getObjectId()};
		WorldLeakProbe::probe(std::span<const runtime::LeakCensus::ProbedLeak>(&leak, 1));
	}
	commons::logging::LoggerFactory::removeConfig("com.aionemu.gameserver.world.WorldLeakProbe");
	std::string log = captured.str();
	EXPECT_NE(log.find("Leak probe: Npc (object id " + std::to_string(leaked->getObjectId()) + ", "), std::string::npos) << log;
	EXPECT_NE(log.find(std::string("): no holder found. Searched by identity: ") + WorldLeakProbe::SEARCHED + ". Not searched: " +
				  WorldLeakProbe::NOT_SEARCHED),
		std::string::npos)
		<< log;
	for (const char* structure : {"World.allObjects", "Player.postman", "House.spawns", "every instance of every world map", "WalkerGroup",
			 "SiegeLocation.creatures"})
		EXPECT_NE(std::string(WorldLeakProbe::SEARCHED).find(structure), std::string::npos) << structure;
	for (const char* structure : {"InstanceWalkerFormations.groupedSpawnObjects", "MoveTaskManager.movingCreatures",
			 "PlayerMoveTaskManager.movingPlayers", "TemporarySpawnEngine.spawnedObjects", "Town.spawnedNpcs", "Base.flag", "WorldRaid",
			 "ObserveController observers", "the other fields of world objects", "objects outside the world"})
		EXPECT_NE(std::string(WorldLeakProbe::NOT_SEARCHED).find(structure), std::string::npos) << structure;
}

TEST_F(WorldContainerLifetimeTest, TheLeakProbeNamesTheWorldsOwnMapsOfAnObjectStillInTheWorld) {
	// the searches that only an object still in the world answers: World.allObjects and the map instance's worldMapObjects and worldMapNpcs,
	// besides its region and its four zones (cell (1, 1))
	Ref<Npc> npc = spawnNpc();
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	std::vector<std::string> holders = WorldLeakProbe::findHolders(*npc);
	std::ranges::sort(holders);
	const std::string instance = " of WorldMapInstance 210010000 [1]";
	std::vector<std::string> expected{
		"World.allObjects",
		"WorldMapInstance.worldMapObjects" + instance,
		"WorldMapInstance.worldMapNpcs" + instance,
		"MapRegion.objects of region 1001" + instance,
		"ZoneInstance.creatures of zone 210010000" + instance,
		"ZoneInstance.creatures of zone FLY_AREA_210010000" + instance,
		"ZoneInstance.creatures of zone SUB_PLAIN_210010000" + instance,
		"ZoneInstance.creatures of zone SUB_PRIORITY_210010000" + instance,
	};
	std::ranges::sort(expected);
	EXPECT_EQ(holders, expected);
}

} // namespace
} // namespace aion::gameserver::world::test
