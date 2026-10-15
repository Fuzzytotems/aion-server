// P6I-1 (docs/design/p6-instances-plan.md §4): chunk I1's five handlers without an AI directory - AdmaStrongholdInstance,
// DraupnirCaveInstance, FireTempleInstance, HaramelInstance, LowerUdasTempleInstance (game-server/data/handlers/instance/*.java) - on a real
// instance of their map, each created through its AION_INSTANCE_HANDLER factory. The random arms run over a fixed range of seeds of the
// thread's Rnd (Rnd::seedCurrentThreadForTests): every outcome a case asserts is one Java's code allows, and the range is wide enough that each
// arm is reached (asserted too).
//
// Expectations are read from the Java files by hand: the npc ids, positions and headings of the spawns, the system messages.

#include "InstanceHandlerTestSupport.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/handlers/instance/AdmaStrongholdInstance.h"
#include "aion/gameserver/handlers/instance/DraupnirCaveInstance.h"
#include "aion/gameserver/handlers/instance/FireTempleInstance.h"
#include "aion/gameserver/handlers/instance/HaramelInstance.h"
#include "aion/gameserver/handlers/instance/LowerUdasTempleInstance.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/flyring/FlyRing.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAY_MOVIE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

namespace aion::gameserver::handlers::instance::test {
namespace {

using model::gameobjects::Npc;
using network::aion::serverpackets::SM_PLAY_MOVIE;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

/** (npc id, x, y, z, heading) of a spawned npc */
using Spot = std::tuple<int32_t, float, float, float, int32_t>;

Spot spotOf(Npc& npc) {
	return {npc.getNpcId(), npc.getX(), npc.getY(), npc.getZ(), npc.getHeading()};
}

std::multiset<Spot> spotsOf(const std::vector<runtime::Ptr<Npc>>& npcs) {
	std::multiset<Spot> spots;
	for (const runtime::Ptr<Npc>& npc : npcs)
		spots.insert(spotOf(*npc));
	return spots;
}

class SmallInstancesTest : public InstanceHandlerTest {};

// ---- registration --------------------------------------------------------------------------------------------------------------------

/** Every handler's Java @InstanceID is the map its factory's handler is created for (the marker's map id is the registry scan's check) */
TEST_F(SmallInstancesTest, EachHandlerIsTheJavaClassOfItsMap) {
	const std::map<std::string, int32_t> expected{{"AdmaStrongholdInstance", ADMA_STRONGHOLD}, {"DraupnirCaveInstance", DRAUPNIR_CAVE},
		{"FireTempleInstance", FIRE_TEMPLE}, {"HaramelInstance", HARAMEL}, {"LowerUdasTempleInstance", LOWER_UDAS_TEMPLE}};
	const std::filesystem::path javaDir = std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "data/handlers/instance";
	const std::regex instanceId(R"(@InstanceID\((\d+)\))");
	for (const auto& [javaClass, mapId] : expected) {
		std::ifstream in(javaDir / (javaClass + ".java"));
		ASSERT_TRUE(in) << javaClass;
		std::stringstream text;
		text << in.rdbuf();
		std::smatch match;
		std::string source = text.str();
		ASSERT_TRUE(std::regex_search(source, match, instanceId)) << javaClass;
		EXPECT_EQ(std::stoi(match[1].str()), mapId) << javaClass;
	}
	commons::utils::Rnd::seedCurrentThreadForTests(1);
	EXPECT_NE(dynamic_cast<AdmaStrongholdInstance*>(create(ADMA_STRONGHOLD, AdmaStrongholdInstance_instanceFactory).getInstanceHandler().get()), nullptr);
	EXPECT_NE(dynamic_cast<DraupnirCaveInstance*>(create(DRAUPNIR_CAVE, DraupnirCaveInstance_instanceFactory).getInstanceHandler().get()), nullptr);
	EXPECT_NE(dynamic_cast<FireTempleInstance*>(create(FIRE_TEMPLE, FireTempleInstance_instanceFactory).getInstanceHandler().get()), nullptr);
	EXPECT_NE(dynamic_cast<HaramelInstance*>(create(HARAMEL, HaramelInstance_instanceFactory).getInstanceHandler().get()), nullptr);
	EXPECT_NE(dynamic_cast<LowerUdasTempleInstance*>(create(LOWER_UDAS_TEMPLE, LowerUdasTempleInstance_instanceFactory).getInstanceHandler().get()),
		nullptr);
}

// ---- AdmaStrongholdInstance.java:19-27 -----------------------------------------------------------------------------------------------

TEST_F(SmallInstancesTest, AdmaPlacesTheChestKeeperAtOneOfThreePlacesHalfOfTheTime) {
	const std::set<Spot> places{{205224, 477.5849f, 398.20898f, 187.49918f, 50}, {205224, 347.76538f, 559.4453f, 180.33097f, 105},
		{205224, 528.25116f, 555.0943f, 189.49f, 78}};
	std::map<std::string, int> outcomes;
	for (uint64_t seed = 1; seed <= 40; ++seed) {
		commons::utils::Rnd::seedCurrentThreadForTests(seed);
		std::multiset<Spot> spots = spotsOf(npcs(create(ADMA_STRONGHOLD, AdmaStrongholdInstance_instanceFactory)));
		ASSERT_LE(spots.size(), 1u) << "seed " << seed;
		if (spots.empty()) {
			++outcomes["none"];
		} else {
			ASSERT_TRUE(places.contains(*spots.begin())) << "seed " << seed;
			++outcomes[std::to_string(std::get<4>(*spots.begin()))];
		}
	}
	EXPECT_EQ(outcomes.size(), 4u) << "no spawn, and each of the three places";
	EXPECT_GE(outcomes["none"], 10);
	EXPECT_LE(outcomes["none"], 30);
}

// ---- FireTempleInstance.java:19-62 ---------------------------------------------------------------------------------------------------

TEST_F(SmallInstancesTest, FireTempleSpawnsEachBossOrItsElite) {
	// (boss, elite) per place, in the Java order
	const std::vector<std::pair<Spot, Spot>> slots{
		{{212839, 127.1218f, 176.1912f, 99.67548f, 15}, {212790, 127.1218f, 176.1912f, 99.67548f, 15}},
		{{212842, 322.3193f, 431.2696f, 134.5296f, 80}, {212799, 322.3193f, 431.2696f, 134.5296f, 80}},
		{{212840, 153.0038f, 299.7786f, 123.0186f, 30}, {212794, 153.0038f, 299.7786f, 123.0186f, 30}},
		{{212843, 296.6911f, 201.9092f, 119.3652f, 30}, {212803, 296.6911f, 201.9092f, 119.3652f, 15}}, // Java's elite turns to 15
		{{212841, 350.9276f, 351.7389f, 146.8498f, 45}, {212799, 350.9276f, 351.7389f, 146.8498f, 45}},
		{{212845, 298.7095f, 89.42245f, 128.7143f, 15}, {214094, 298.7095f, 89.42245f, 128.7143f, 15}},
		{{212846, 421.9935f, 93.18915f, 117.3053f, 46}, {214621, 421.9935f, 93.18915f, 117.3053f, 46}}, // normal Kromede first, the stronger at 10 %
	};
	std::vector<std::pair<int, int>> seen(slots.size());
	for (uint64_t seed = 1; seed <= 60; ++seed) {
		commons::utils::Rnd::seedCurrentThreadForTests(seed);
		std::multiset<Spot> spots = spotsOf(npcs(create(FIRE_TEMPLE, FireTempleInstance_instanceFactory)));
		ASSERT_EQ(spots.size(), slots.size()) << "seed " << seed;
		for (size_t i = 0; i < slots.size(); ++i) {
			bool first = spots.contains(slots[i].first);
			bool second = spots.contains(slots[i].second);
			ASSERT_NE(first, second) << "seed " << seed << ", place " << i;
			(first ? seen[i].first : seen[i].second)++;
		}
		services::instance::InstanceService::destroyInstance(*instances.back());
		instances.pop_back();
	}
	for (size_t i = 0; i < slots.size(); ++i) {
		EXPECT_GT(seen[i].first, 0) << "place " << i;
		EXPECT_GT(seen[i].second, 0) << "place " << i;
	}
	for (size_t i = 0; i + 1 < slots.size(); ++i)
		EXPECT_GT(seen[i].first, seen[i].second) << "the boss at 75 %, place " << i;
	EXPECT_GT(seen.back().first, seen.back().second * 3) << "the stronger Kromede at 10 %";
}

// ---- HaramelInstance.java:23-62 ------------------------------------------------------------------------------------------------------

TEST_F(SmallInstancesTest, HamerunsDeathBringsTheChestOfTheTopDamageDealersClassAndTheGate) {
	const std::vector<std::pair<model::PlayerClass, int32_t>> chests{{model::PlayerClass::GLADIATOR, 700829}, {model::PlayerClass::TEMPLAR, 700829},
		{model::PlayerClass::ASSASSIN, 700830}, {model::PlayerClass::RANGER, 700830}, {model::PlayerClass::GUNNER, 700830},
		{model::PlayerClass::BARD, 700831}, {model::PlayerClass::SORCERER, 700831}, {model::PlayerClass::SPIRIT_MASTER, 700831},
		{model::PlayerClass::CLERIC, 700832}, {model::PlayerClass::CHANTER, 700832}, {model::PlayerClass::RIDER, 700832},
		{model::PlayerClass::WARRIOR, 0}};
	for (const auto& [playerClass, chest] : chests) {
		world::WorldMapInstance& instance = create(HARAMEL, HaramelInstance_instanceFactory);
		enter(instance, 224.0f, 300.0f, 142.0f, playerClass);
		runtime::Ptr<Npc> hamerun = spawnNpc(instance, 216922, 224.0f, 290.0f, 142.0f, int8_t{0});
		ASSERT_TRUE(hamerun);
		hit(*hamerun);
		instance.getInstanceHandler()->onDie(*hamerun);
		EXPECT_FALSE(hamerun->isSpawned()) << "deleted";
		std::multiset<Spot> spots = spotsOf(npcs(instance));
		std::multiset<Spot> expected{{700852, 224.5984f, 331.1431f, 141.8925f, 90}};
		if (chest != 0)
			expected.insert({chest, 224.137f, 268.608f, 144.898f, 90});
		EXPECT_EQ(spots, expected) << model::getClassId(playerClass);
		EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDNOVICE_HAMEROON_TREASUREBOX_SPAWN())), 1);
		EXPECT_EQ(countSent(serialized(SM_PLAY_MOVIE(false, 0, 0, 457, true))), 1);
		leave();
	}
}

TEST_F(SmallInstancesTest, HamerunWithoutAPlayerDamageOnlyGoesAndTheMinersVanish) {
	world::WorldMapInstance& instance = create(HARAMEL, HaramelInstance_instanceFactory);
	enter(instance, 224.0f, 300.0f, 142.0f, model::PlayerClass::GLADIATOR);
	runtime::Ptr<Npc> hamerun = spawnNpc(instance, 216922, 224.0f, 290.0f, 142.0f, int8_t{0});
	instance.getInstanceHandler()->onDie(*hamerun);
	EXPECT_FALSE(hamerun->isSpawned());
	EXPECT_TRUE(npcs(instance).empty()) << "no chest, no gate";
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDNOVICE_HAMEROON_TREASUREBOX_SPAWN())), 0);
	for (int32_t id : {216920, 216921, 217067, 700950}) {
		runtime::Ptr<Npc> npc = spawnNpc(instance, id, 200.0f, 300.0f, 142.0f, int8_t{0});
		instance.getInstanceHandler()->onDie(*npc);
		EXPECT_FALSE(npc->isSpawned()) << id;
	}
	runtime::Ptr<Npc> other = spawnNpc(instance, 700829, 200.0f, 300.0f, 142.0f, int8_t{0});
	instance.getInstanceHandler()->onDie(*other);
	EXPECT_TRUE(other->isSpawned()) << "no arm for other npcs";
}

// ---- DraupnirCaveInstance.java:33-81 -------------------------------------------------------------------------------------------------

TEST_F(SmallInstancesTest, DraupnirPlacesTheStartRingAndOneOfFourNamed) {
	const std::set<Spot> named{{213587, 567.438f, 700.875f, 538.701f, 7}, {213588, 166.8f, 536.285f, 505.802f, 9},
		{213771, 497.006f, 434.713f, 616.584f, 71}, {213773, 380.694f, 611.956f, 598.523f, 98}};
	std::set<Spot> seen;
	for (uint64_t seed = 1; seed <= 30; ++seed) {
		commons::utils::Rnd::seedCurrentThreadForTests(seed);
		world::WorldMapInstance& instance = create(DRAUPNIR_CAVE, DraupnirCaveInstance_instanceFactory);
		std::multiset<Spot> spots = spotsOf(npcs(instance));
		ASSERT_EQ(spots.size(), 1u) << "seed " << seed;
		ASSERT_TRUE(named.contains(*spots.begin())) << "seed " << seed;
		seen.insert(*spots.begin());
		int rings = 0;
		for (runtime::Ptr<model::gameobjects::VisibleObject> object : instance) {
			if (runtime::Ptr<model::flyring::FlyRing> ring = runtime::as<model::flyring::FlyRing>(object)) {
				++rings;
				EXPECT_EQ(ring->getName(), "INSTANCE_MSG_START");
			}
		}
		EXPECT_EQ(rings, 1);
	}
	EXPECT_EQ(seen, named);
}

TEST_F(SmallInstancesTest, DraupnirTheStartRingTellsOnceAndTheFourAdjutantsBringBakarma) {
	world::WorldMapInstance& instance = create(DRAUPNIR_CAVE, DraupnirCaveInstance_instanceFactory);
	enter(instance, 475.0f, 427.0f, 618.0f);
	::aion::gameserver::instance::handlers::InstanceHandler& handler = *instance.getInstanceHandler();
	EXPECT_FALSE(handler.onPassFlyingRing(player(), "OTHER_RING"));
	EXPECT_TRUE(handler.onPassFlyingRing(player(), "INSTANCE_MSG_START"));
	EXPECT_FALSE(handler.onPassFlyingRing(player(), "INSTANCE_MSG_START")) << "once";
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_BOSS_SPAWN_IDDF3_DRAGON_1())), 1);

	size_t before = npcs(instance).size();
	runtime::Ptr<Npc> stranger = spawnNpc(instance, 213587, 400.0f, 400.0f, 600.0f, int8_t{0});
	handler.onDie(*stranger); // not an adjutant: not counted
	const std::vector<int32_t> adjutants{213776, 213778, 213779, 213802};
	for (size_t i = 0; i < adjutants.size(); ++i) {
		runtime::Ptr<Npc> adjutant = spawnNpc(instance, adjutants[i], 400.0f + i, 400.0f, 600.0f, int8_t{0});
		handler.onDie(*adjutant);
		EXPECT_EQ(npcs(instance, 213780).size(), i == 3 ? 1u : 0u) << "after " << (i + 1);
	}
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_BOSS_SPAWN_IDDF3_DRAGON_2())), 1);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_BOSS_SPAWN_IDDF3_DRAGON_3())), 1);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_BOSS_SPAWN_IDDF3_DRAGON_4())), 1);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_BOSS_SPAWN_IDDF3_DRAGON())), 1);
	ASSERT_EQ(npcs(instance, 213780).size(), 1u);
	EXPECT_EQ(spotOf(*npcs(instance, 213780).front()), (Spot{213780, 813.189f, 432.255f, 318.928f, 30}));
	EXPECT_EQ(npcs(instance).size(), before + 2 + adjutants.size()) << "the stranger, the adjutants and Bakarma (onDie deletes nothing)";
}

// DraupnirCaveInstance.onReviveEvent (:75-81) has no case: its teleport reaches HousingService, which loads from the database (the
// AscensionWorldTest fixture's path); the revive is left to a gate.

// ---- LowerUdasTempleInstance.java:21-76 ----------------------------------------------------------------------------------------------

TEST_F(SmallInstancesTest, LowerUdasPlacesOneOrBothChests) {
	const Spot epic{216150, 455.984f, 1192.506f, 190.221f, 116};
	const Spot fabled{216645, 435.664f, 1182.577f, 190.221f, 116};
	std::map<std::string, int> outcomes;
	for (uint64_t seed = 1; seed <= 40; ++seed) {
		commons::utils::Rnd::seedCurrentThreadForTests(seed);
		std::multiset<Spot> spots = spotsOf(npcs(create(LOWER_UDAS_TEMPLE, LowerUdasTempleInstance_instanceFactory)));
		if (spots == std::multiset<Spot>{epic, fabled})
			++outcomes["both"];
		else if (spots == std::multiset<Spot>{epic})
			++outcomes["epic"];
		else if (spots == std::multiset<Spot>{fabled})
			++outcomes["fabled"];
		else
			ADD_FAILURE() << "seed " << seed << ": " << spots.size() << " npcs";
	}
	EXPECT_GT(outcomes["both"], 0);
	EXPECT_GT(outcomes["epic"], 0);
	EXPECT_GT(outcomes["fabled"], outcomes["both"]) << "50 % against 20 %";
}

TEST_F(SmallInstancesTest, LowerUdasTheFirstEntryPlacesTheLibrarianAndTheTrapsAnItemUseClearsTheTraps) {
	world::WorldMapInstance& instance = create(LOWER_UDAS_TEMPLE, LowerUdasTempleInstance_instanceFactory);
	enter(instance, 745.0f, 885.0f, 153.0f);
	size_t chests = npcs(instance).size();
	::aion::gameserver::instance::handlers::InstanceHandler& handler = *instance.getInstanceHandler();
	handler.onEnterInstance(player());
	ASSERT_EQ(npcs(instance, 216531).size(), 1u);
	EXPECT_EQ(spotOf(*npcs(instance, 216531).front()), (Spot{216531, 744.7521f, 885.8238f, 152.7852f, 30}));
	std::vector<runtime::Ptr<Npc>> traps = npcs(instance, 216530);
	ASSERT_EQ(traps.size(), 36u);
	std::multiset<Spot> trapSpots = spotsOf(traps);
	EXPECT_TRUE(trapSpots.contains(Spot{216530, 749.4821f, 880.4201f, 154.0f, 0})) << "the first position";
	EXPECT_TRUE(trapSpots.contains(Spot{216530, 749.3269f, 895.6159f, 154.0f, 0})) << "the last position";
	EXPECT_TRUE(trapSpots.contains(Spot{216530, 740.22205f, 889.76013f, 154.0f, 0}));
	EXPECT_EQ(std::set<Spot>(trapSpots.begin(), trapSpots.end()).size(), 36u) << "36 distinct positions";
	handler.onEnterInstance(player());
	EXPECT_EQ(npcs(instance, 216530).size(), 36u) << "once";

	runtime::Ptr<Npc> used = npcs(instance, 216531).front();
	handler.handleUseItemFinish(runtime::Ptr<model::gameobjects::player::Player>(player()), *used);
	EXPECT_TRUE(npcs(instance, 216530).empty()) << "the traps are deleted";
	EXPECT_EQ(npcs(instance, 216531).size(), 1u) << "Zhanim stays";
	EXPECT_EQ(npcs(instance).size(), chests + 1);
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
