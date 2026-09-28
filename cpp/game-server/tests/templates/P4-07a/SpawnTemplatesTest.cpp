// P4-07a spawn templates: the bound spawn data (Spawn, SpawnSpotTemplate hook, TemporarySpawn expressions and time checks with explicit clocks,
// SpawnMap, HouseSpawns, SpawnType companion) and the run-time spawn family (SpawnGroup constructors of every template kind, the spawn pool
// reservation, SpawnTemplate delegation). Expected values follow SpawnGroup.java, SpawnTemplate.java and TemporarySpawn.java.

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataException.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/templates/spawns/HouseSpawns.bind.h"
#include "aion/gameserver/model/templates/spawns/Spawn.bind.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnMap.bind.h"
#include "aion/gameserver/model/templates/spawns/SpawnSearchResult.h"
#include "aion/gameserver/model/templates/spawns/SpawnSpotTemplate.bind.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTypeInfo.h"
#include "aion/gameserver/model/templates/spawns/TemporarySpawn.bind.h"
#include "aion/gameserver/model/templates/spawns/basespawns/BaseSpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/housing/TownSpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/panesterra/AhserionsFlightSpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/riftspawns/RiftSpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/siegespawns/SiegeSpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/vortexspawns/VortexSpawnTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"

namespace aion::gameserver::model::templates::spawns {
namespace {

using java::time::DayOfWeek;

template <class T>
std::unique_ptr<T> bindXml(std::string_view text) {
	xml::LoadContext context;
	return xml::bindString<T>(context, text);
}

static_assert(value(SpawnType::TELEPORT) == "TELEPORT");

TEST(SpawnDataTest, SpotsAndTheirHook) {
	std::unique_ptr<Spawn> spawn = bindXml<Spawn>(R"(<spawn npc_id="210001" respawn_time="295" pool="1" difficult_id="2" handler="RIFT">)"
		R"(<spot x="1.5" y="2.5" z="3.5" h="-10" static_id="7" random_walk="4" walker_id="W1" walker_index="2" anchor="a" state="16")"
		R"( aerial_spawn="true" ai="dummy"/>)"
		R"(<spot x="1" y="2" z="3"/></spawn>)");
	EXPECT_EQ(spawn->getNpcId(), 210001);
	EXPECT_EQ(spawn->getRespawnTime(), 295);
	EXPECT_EQ(spawn->getPool(), 1);
	EXPECT_EQ(spawn->getDifficultId(), 2);
	EXPECT_EQ(spawn->getSpawnHandlerType(), spawnengine::SpawnHandlerType::RIFT);
	EXPECT_FALSE(spawn->isCustom());
	EXPECT_FALSE(spawn->isEventSpawn());
	const std::vector<SpawnSpotTemplate>& spots = static_cast<const Spawn&>(*spawn).getSpawnSpotTemplates();
	ASSERT_EQ(spots.size(), 2u);
	EXPECT_FLOAT_EQ(spots[0].getX(), 1.5f);
	EXPECT_EQ(spots[0].getHeading(), -10);
	EXPECT_EQ(spots[0].getStaticId(), 7);
	EXPECT_EQ(spots[0].getRandomWalk(), 4);
	EXPECT_EQ(spots[0].getWalkerId(), "W1");
	EXPECT_EQ(spots[0].getWalkerIndex(), 2);
	EXPECT_EQ(spots[0].getAnchor(), "a");
	EXPECT_EQ(spots[0].getState(), 16);
	EXPECT_TRUE(spots[0].isAerialSpawn());
	EXPECT_EQ(spots[0].getAi(), "dummy") << "the hook only interns the ai name";
	EXPECT_EQ(spots[1].getStaticId(), 0);
	EXPECT_FALSE(spots[1].isAerialSpawn()) << "Boolean null";
	EXPECT_EQ(spots[1].getWalkerIndex(), std::nullopt);

	SpawnSpotTemplate custom(1, 2, 3, 4, -3, std::nullopt, std::nullopt);
	EXPECT_EQ(custom.getRandomWalk(), 0) << "Java sets randomWalk only if > 0";
}

TEST(SpawnDataTest, SearchResultOwnsItsSpot) {
	// Java SpawnsData.toSpawnSearchResult: new SpawnSearchResult(worldId, new SpawnSpotTemplate(spot.getX(), ..., spot.getWalkerIndex())) from
	// the current coordinates of a run-time SpawnTemplate; the result is the only owner of that spot
	std::optional<SpawnSearchResult> result;
	{
		std::string walker = "WALKER_1";
		SpawnSpotTemplate spot(10.5f, -20.25f, 30.0f, 90, 5, std::string_view(walker), 3);
		result.emplace(110010000, std::move(spot));
		walker.assign("changed after the search");
	}
	EXPECT_EQ(result->getWorldId(), 110010000);
	const SpawnSpotTemplate& spot = result->getSpot();
	EXPECT_FLOAT_EQ(spot.getX(), 10.5f);
	EXPECT_FLOAT_EQ(spot.getY(), -20.25f);
	EXPECT_FLOAT_EQ(spot.getZ(), 30.0f);
	EXPECT_EQ(spot.getHeading(), 90);
	EXPECT_EQ(spot.getRandomWalk(), 5);
	EXPECT_EQ(spot.getWalkerId(), "WALKER_1") << "the result keeps its own copy after the source spot is gone";
	EXPECT_EQ(spot.getWalkerIndex(), 3);
	EXPECT_EQ(spot.getStaticId(), 0);
	EXPECT_EQ(spot.getTemporarySpawn(), nullptr);

	SpawnSearchResult moved = std::move(*result); // std::optional<SpawnSearchResult> is the C++ form of Java's null search result
	EXPECT_FLOAT_EQ(moved.getSpot().getX(), 10.5f);
}

TEST(SpawnDataTest, SpawnMapAndHouseSpawnLists) {
	std::unique_ptr<SpawnMap> map = bindXml<SpawnMap>(R"(<spawn_map map_id="210010000"><spawn npc_id="1"/><spawn npc_id="2"/></spawn_map>)");
	EXPECT_EQ(map->getMapId(), 210010000);
	EXPECT_EQ(static_cast<const SpawnMap&>(*map).getSpawns().size(), 2u);
	EXPECT_TRUE(map->getBaseSpawns().empty()) << "Collections.emptyList()";
	EXPECT_TRUE(map->getAhserionSpawns().empty());
	SpawnMap custom(600010000);
	EXPECT_EQ(custom.getMapId(), 600010000);
	EXPECT_TRUE(custom.getSpawns().empty());

	std::unique_ptr<HouseSpawns> house = bindXml<HouseSpawns>(R"(<house address="1001"><spawn x="1" y="2" z="3" type="SIGN"/></house>)");
	EXPECT_EQ(house->getAddress(), 1001);
	ASSERT_EQ(house->getSpawns().size(), 1u);
	EXPECT_EQ(house->getSpawns()[0].getType(), SpawnType::SIGN);
	EXPECT_EQ(fromValue("MANAGER"), SpawnType::MANAGER);
	EXPECT_THROW(fromValue("manager"), commons::utils::IllegalArgumentException);
}

// ---- TemporarySpawn --------------------------------------------------------------------------------------------------------------------------

std::unique_ptr<TemporarySpawn> temporary(std::string_view attributes) {
	return bindXml<TemporarySpawn>("<temporary_spawn " + std::string(attributes) + "/>");
}

TEST(TemporarySpawnTest, SpawnTimesAtFixedHours) {
	// spawn at hour 6 and despawn at hour 18 of every day and month
	std::unique_ptr<TemporarySpawn> spawn = temporary(R"(spawn_time="6.*.*" despawn_time="18.*.*")");
	EXPECT_TRUE(spawn->canSpawn(DayOfWeek::MONDAY, {6, 1, 1}));
	EXPECT_FALSE(spawn->canSpawn(DayOfWeek::MONDAY, {7, 1, 1}));
	EXPECT_TRUE(spawn->canDespawn(DayOfWeek::MONDAY, {18, 3, 4}));
	EXPECT_FALSE(spawn->canDespawn(DayOfWeek::MONDAY, {6, 3, 4}));
	EXPECT_TRUE(spawn->isInSpawnTime(DayOfWeek::MONDAY, {6, 1, 1}));
	EXPECT_TRUE(spawn->isInSpawnTime(DayOfWeek::MONDAY, {17, 1, 1}));
	EXPECT_FALSE(spawn->isInSpawnTime(DayOfWeek::MONDAY, {18, 1, 1})) << "checkHour: current < despawn hour";
	EXPECT_FALSE(spawn->isInSpawnTime(DayOfWeek::MONDAY, {5, 1, 1}));

	// over midnight: spawn at 22, despawn at 4
	std::unique_ptr<TemporarySpawn> night = temporary(R"(spawn_time="22.*.*" despawn_time="4.*.*")");
	EXPECT_TRUE(night->isInSpawnTime(DayOfWeek::MONDAY, {23, 1, 1}));
	EXPECT_TRUE(night->isInSpawnTime(DayOfWeek::MONDAY, {3, 1, 1}));
	EXPECT_FALSE(night->isInSpawnTime(DayOfWeek::MONDAY, {4, 1, 1}));
	// equal hours: always
	EXPECT_TRUE(temporary(R"(spawn_time="5.*.*" despawn_time="5.*.*")")->isInSpawnTime(DayOfWeek::MONDAY, {12, 1, 1}));
	// no despawn: any later hour
	std::unique_ptr<TemporarySpawn> noDespawn = temporary(R"(spawn_time="9.*.*")");
	EXPECT_TRUE(noDespawn->isInSpawnTime(DayOfWeek::MONDAY, {9, 1, 1}));
	EXPECT_FALSE(noDespawn->isInSpawnTime(DayOfWeek::MONDAY, {8, 1, 1}));
	EXPECT_TRUE(noDespawn->canDespawn(DayOfWeek::MONDAY, {0, 0, 0})) << "no despawn time: every field null, isTime is true";
}

TEST(TemporarySpawnTest, EveryNthExpressions) {
	// "/3" parses to -3: every 3rd hour (hour % -3 == 0)
	std::unique_ptr<TemporarySpawn> spawn = temporary(R"(spawn_time="/3.*.*" despawn_time="/2.*.*")");
	EXPECT_TRUE(spawn->canSpawn(DayOfWeek::MONDAY, {9, 1, 1}));
	EXPECT_FALSE(spawn->canSpawn(DayOfWeek::MONDAY, {10, 1, 1}));
	EXPECT_TRUE(spawn->canDespawn(DayOfWeek::MONDAY, {10, 1, 1}));
	// checkWithDespawnExpression: current >= |spawn| and |spawn| == |despawn|
	EXPECT_FALSE(spawn->isInSpawnTime(DayOfWeek::MONDAY, {9, 1, 1})) << "3 != 2";
	std::unique_ptr<TemporarySpawn> same = temporary(R"(spawn_time="/2.*.*" despawn_time="/2.*.*")");
	EXPECT_TRUE(same->isInSpawnTime(DayOfWeek::MONDAY, {2, 1, 1}));
	EXPECT_FALSE(same->isInSpawnTime(DayOfWeek::MONDAY, {1, 1, 1}));
	// day and month parts: spawn from day 10 to 20 in month 3
	std::unique_ptr<TemporarySpawn> days = temporary(R"(spawn_time="*.10.3" despawn_time="*.20.3")");
	EXPECT_TRUE(days->isInSpawnTime(DayOfWeek::MONDAY, {0, 15, 3}));
	EXPECT_TRUE(days->isInSpawnTime(DayOfWeek::MONDAY, {0, 20, 3})) << "checkDate: current <= despawn date";
	EXPECT_FALSE(days->isInSpawnTime(DayOfWeek::MONDAY, {0, 21, 3}));
	EXPECT_FALSE(days->isInSpawnTime(DayOfWeek::MONDAY, {0, 15, 4}));
	EXPECT_TRUE(days->canSpawn(DayOfWeek::MONDAY, {23, 10, 3})) << "hour *";
}

TEST(TemporarySpawnTest, WeekdaysRestrictEveryCheck) {
	std::unique_ptr<TemporarySpawn> spawn = temporary(R"(weekdays="SATURDAY SUNDAY" spawn_time="6.*.*" despawn_time="18.*.*")");
	EXPECT_TRUE(spawn->canSpawn(DayOfWeek::SUNDAY, {6, 1, 1}));
	EXPECT_FALSE(spawn->canSpawn(DayOfWeek::FRIDAY, {6, 1, 1}));
	EXPECT_TRUE(spawn->canDespawn(DayOfWeek::FRIDAY, {6, 1, 1})) << "an excluded weekday despawns";
	EXPECT_FALSE(spawn->isInSpawnTime(DayOfWeek::FRIDAY, {12, 1, 1}));
	EXPECT_TRUE(spawn->isInSpawnTime(DayOfWeek::SATURDAY, {12, 1, 1}));
	EXPECT_TRUE(temporary(R"(weekdays="" spawn_time="6.*.*")")->canSpawn(DayOfWeek::FRIDAY, {6, 1, 1})) << "an empty list does not restrict";
}

TEST(TemporarySpawnTest, MalformedExpressionsFailTheLoad) {
	EXPECT_THROW(temporary(R"(spawn_time="6.*")"), xml::StaticDataException) << "Java split(): index 2 out of bounds";
	EXPECT_THROW(temporary(R"(spawn_time="6.x.*")"), xml::StaticDataException) << "Integer.parseInt";
	EXPECT_NO_THROW(temporary(R"(despawn_time="6.*.*.")")) << "Java split() drops the trailing empty string";
}

// ---- SpawnGroup and SpawnTemplate ----------------------------------------------------------------------------------------------------------------

std::unique_ptr<Spawn> spawnWithSpots(int32_t pool, int32_t spots) {
	std::string xml = "<spawn npc_id=\"217000\" respawn_time=\"60\" pool=\"" + std::to_string(pool) + "\">";
	for (int32_t i = 0; i < spots; ++i)
		xml += "<spot x=\"" + std::to_string(i) + "\" y=\"1\" z=\"2\" h=\"3\" walker_id=\"walker" + std::to_string(i) + "\"/>";
	return bindXml<Spawn>(xml + "</spawn>");
}

class SpawnGroupTest : public testing::Test {
protected:
	runtime::TaskScope scope{AION_TASK_INFO(runtime::TaskKind::TEST)};
};

TEST_F(SpawnGroupTest, SpotConstructorsCreateOneTemplateOfTheirKindPerSpot) {
	std::unique_ptr<Spawn> spawn = spawnWithSpots(0, 2);
	runtime::Ref<SpawnGroup> plain = SpawnGroup::create(210010000, spawn.get());
	EXPECT_EQ(plain->getNpcId(), 217000);
	EXPECT_EQ(plain->getRespawnTime(), 60);
	EXPECT_EQ(plain->getWorldId(), 210010000);
	EXPECT_FALSE(plain->hasPool());
	EXPECT_FALSE(plain->isTemporarySpawn());
	std::vector<runtime::Ptr<SpawnTemplate>> templates = plain->getSpawnTemplates().snapshot();
	ASSERT_EQ(templates.size(), 2u);
	EXPECT_FLOAT_EQ(templates[1]->getX(), 1.0f);
	EXPECT_EQ(templates[1]->getWalkerId(), "walker1");
	EXPECT_EQ(templates[1]->getNpcId(), 217000);
	EXPECT_EQ(templates[1]->getWorldId(), 210010000);
	EXPECT_EQ(templates[1]->getRespawnTime(), 60);
	EXPECT_FALSE(templates[1]->isNoRespawn());
	EXPECT_EQ(&templates[1]->getGroup(), plain.get());
	EXPECT_EQ(templates[1]->getTemporarySpawn(), nullptr);
	EXPECT_EQ(templates[1]->getEventTemplate(), nullptr);
	EXPECT_FALSE(templates[1]->isEventSpawn());

	runtime::Ref<SpawnGroup> base = SpawnGroup::create(1, spawn.get(), 5, model::base::BaseOccupier::BALAUR);
	for (const runtime::Ptr<SpawnTemplate>& t : base->getSpawnTemplates().snapshot()) {
		auto* baseTemplate = dynamic_cast<basespawns::BaseSpawnTemplate*>(t.get());
		ASSERT_NE(baseTemplate, nullptr);
		EXPECT_EQ(baseTemplate->getId(), 5);
		EXPECT_EQ(baseTemplate->getOccupier(), model::base::BaseOccupier::BALAUR);
	}
	runtime::Ref<SpawnGroup> rift = SpawnGroup::create(1, spawn.get(), 9);
	auto* riftTemplate = dynamic_cast<riftspawns::RiftSpawnTemplate*>(rift->getSpawnTemplates().get(0).get());
	ASSERT_NE(riftTemplate, nullptr);
	EXPECT_EQ(riftTemplate->getId(), 9);
	runtime::Ref<SpawnGroup> vortex = SpawnGroup::create(1, spawn.get(), 2, model::vortex::VortexStateType::INVASION);
	auto* vortexTemplate = dynamic_cast<vortexspawns::VortexSpawnTemplate*>(vortex->getSpawnTemplates().get(1).get());
	ASSERT_NE(vortexTemplate, nullptr);
	EXPECT_EQ(vortexTemplate->getId(), 2);
	EXPECT_TRUE(vortexTemplate->isInvasion());
	EXPECT_FALSE(vortexTemplate->isPeace());
	runtime::Ref<SpawnGroup> siege = SpawnGroup::create(1, spawn.get(), 1011, model::siege::SiegeRace::BALAUR, model::siege::SiegeModType::ASSAULT);
	auto* siegeTemplate = dynamic_cast<siegespawns::SiegeSpawnTemplate*>(siege->getSpawnTemplates().get(0).get());
	ASSERT_NE(siegeTemplate, nullptr);
	EXPECT_EQ(siegeTemplate->getSiegeId(), 1011);
	runtime::Ref<SpawnGroup> ahserion =
		SpawnGroup::create(400030000, spawn.get(), 3, services::panesterra::ahserion::PanesterraFaction::IVY_TEMPLE);
	auto* ahserionTemplate = dynamic_cast<panesterra::AhserionsFlightSpawnTemplate*>(ahserion->getSpawnTemplates().get(1).get());
	ASSERT_NE(ahserionTemplate, nullptr);
	EXPECT_EQ(ahserionTemplate->getStage(), 3);
	EXPECT_EQ(ahserionTemplate->getFaction(), services::panesterra::ahserion::PanesterraFaction::IVY_TEMPLE);

	// Town.java:138 creates a TownSpawnTemplate that is not part of the spots
	SpawnTemplate& town = plain->adoptDetachedTemplate(std::make_unique<housing::TownSpawnTemplate>(*plain, &spawn->getSpawnSpotTemplates()[0], 4));
	EXPECT_EQ(static_cast<housing::TownSpawnTemplate&>(town).getTownId(), 4);
	EXPECT_EQ(plain->getSpawnTemplates().size(), 2);
}

TEST_F(SpawnGroupTest, CoordinateTemplatesAddThemselvesAndDelegateToTheGroup) {
	runtime::Ref<SpawnGroup> group = SpawnGroup::create(300010000, 700001, 0, nullptr);
	EXPECT_FALSE(group->hasPool());
	runtime::Ref<SpawnTemplate> spawnTemplate = SpawnTemplate::create(*group, 1, 2, 3, 4, 0, std::nullopt, 0, 99, "noaction");
	EXPECT_EQ(group->getSpawnTemplates().size(), 1) << "addTemplate()";
	EXPECT_TRUE(spawnTemplate->isNoRespawn()) << "respawn time 0";
	EXPECT_EQ(spawnTemplate->getCreatorId(), 99);
	EXPECT_EQ(spawnTemplate->getAiName(), "noaction");
	EXPECT_EQ(spawnTemplate->getWalkerId(), std::nullopt);
	EXPECT_FALSE(spawnTemplate->hasPool());
	EXPECT_EQ(spawnTemplate->getHandlerType(), std::nullopt);
	EXPECT_THROW(spawnTemplate->changeTemplate(1), runtime::UnsupportedOperationException) << "Java: Collections.emptyMap().computeIfAbsent";
	spawnTemplate->resetPoolSpot(1); // Java: getOrDefault(emptySet).remove(): no effect
}

TEST_F(SpawnGroupTest, PoolReservesFreeSpotsPerInstance) {
	commons::utils::Rnd::seedCurrentThreadForTests(20260914);
	std::unique_ptr<Spawn> spawn = spawnWithSpots(2, 3);
	runtime::Ref<SpawnGroup> group = SpawnGroup::create(210010000, spawn.get());
	EXPECT_TRUE(group->hasPool());
	std::set<SpawnTemplate*> reserved;
	for (int i = 0; i < 3; ++i) {
		runtime::Ptr<SpawnTemplate> spot = group->reserveRandomFreePoolSpot(1);
		ASSERT_NE(spot, nullptr);
		EXPECT_TRUE(reserved.insert(spot.get()).second) << "a reserved spot is never handed out twice";
	}
	EXPECT_EQ(group->reserveRandomFreePoolSpot(1), nullptr) << "all spots are used (logged)";
	ASSERT_NE(group->reserveRandomFreePoolSpot(2), nullptr) << "instances have their own reservations";

	SpawnTemplate* released = *reserved.begin();
	released->resetPoolSpot(1);
	runtime::Ptr<SpawnTemplate> again = group->reserveRandomFreePoolSpot(1);
	EXPECT_EQ(again.get(), released) << "the only free spot";
	group->resetPoolSpots(1);
	for (int i = 0; i < 3; ++i)
		EXPECT_NE(group->getSpawnTemplates().get(0)->changeTemplate(1), nullptr);
	EXPECT_EQ(group->reserveRandomFreePoolSpot(1), nullptr);
	group->resetPoolSpots(77); // an instance without reservations
}

} // namespace
} // namespace aion::gameserver::model::templates::spawns
