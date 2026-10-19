// M5h-2 H2-6 (header request m5h-t1): the town level-up - Town.increaseLevel and broadcastUpdate (Town.java:109-131), reached through the
// synchronized increasePoints (Town.java:60-80). No database: a Town of the import constructor (Town(id, race), level 1, levelUpDate 60 s
// after the epoch) on a town spawn map whose levels have no spawns (Town.spawnNewObjects iterates the empty lists), two online players of
// TeamTestSupport.h (one Elyos, one Asmodian) recording what they are sent.

#include "../team/P5-10b/TeamTestSupport.h"
#include "LegionHouseTestSupport.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <unordered_map>

#include <gtest/gtest.h>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/TownSpawnsData.bind.h"
#include "aion/gameserver/dataholders/TownSpawnsData.h"
#include "aion/gameserver/dataholders/WorldMapsData.bind.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/town/Town.h"
#include "aion/gameserver/network/aion/serverpackets/SM_TOWNS_LIST.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/geo/GeoService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::team {
namespace {

namespace lh = ::aion::gameserver::legionhouse::test;
using model::town::Town;
using runtime::Ptr;
using serverpackets::SM_TOWNS_LIST;

constexpr const char* TOWN_SPAWNS = R"(<town_spawns_data><spawn_map map_id="700010000"><town_spawn town_id="1001">)"
									R"(<town_level level="1"/><town_level level="2"/><town_level level="3"/><town_level level="4"/><town_level level="5"/>)"
									R"(</town_spawn></spawn_map></town_spawns_data>)";

class TownLevelUpTest : public TeamTest {
protected:
	void SetUp() override {
		// the same Poeta row and the two housing worlds, whose geo maps GeoService.updateTown (the constructor and increaseLevel) reads, in
		// place of the process-wide Poeta holder for this test only. publishPoetaWorldDataOnce publishes that holder once per process, when
		// nothing is published: calling it first makes it publish here if no earlier test did, so the holder TearDown puts back is the one
		// every later fixture expects (whichever order the tests run in)
		items::publishPoetaWorldDataOnce();
		previousWorldMaps = dataholders::DataManager::WORLD_MAPS_DATA.get();
		dataholders::DataManager::WORLD_MAPS_DATA.resetForTests();
		worldMaps.emplace(dataholders::DataManager::WORLD_MAPS_DATA,
			lh::bindXml<dataholders::WorldMapsData>(
				R"(<world_maps><map id="210010000" cName="LF1" name="Poeta" name_id="1" water_level="16" death_level="0")"
				R"( world_type="ELYSEA" world_size="1024" flags="FLY GLIDE RECALL"/>)"
				R"(<map id="700010000" cName="Oriel" death_level="0" water_level="0" flags="BIND"/>)"
				R"(<map id="710010000" cName="Pernon" death_level="0" water_level="0" flags="BIND"/></world_maps>)"));
		TeamTest::SetUp();
		townSpawns.emplace(dataholders::DataManager::TOWN_SPAWNS_DATA, lh::bindXml<dataholders::TownSpawnsData>(TOWN_SPAWNS));
		world::geo::GeoService::getInstance().init();
		elyos = &online("Townelyos", model::Race::ELYOS);
		asmodian = &online("Townasmo", model::Race::ASMODIANS);
	}

	void TearDown() override {
		for (Member* m : stored)
			world::World::getInstance().removeObject(m->player());
		stored.clear();
		town = nullptr;
		towns.clear();
		townSpawns.reset();
		TeamTest::TearDown();
		worldMaps.reset(); // after the map instance of TeamTest that reads the Poeta template
		// published holders are immortal (HolderRef::publish releases them), so the previous one is published again as it was
		if (previousWorldMaps != nullptr)
			dataholders::DataManager::WORLD_MAPS_DATA.publish(
				std::unique_ptr<dataholders::WorldMapsData>(const_cast<dataholders::WorldMapsData*>(previousWorldMaps)));
	}

	/** an online player of `race` in the World (World.forEachPlayer lists him) */
	Member& online(std::string_view name, model::Race race) {
		Member& m = addMember(name);
		m.player().getCommonData()->setRace(race);
		world::World::getInstance().storeObject(m.player());
		stored.push_back(&m);
		m.clearSent();
		return m;
	}

	/** SM_TOWNS_LIST of the one town, as broadcastUpdate builds it (new HashMap<>(1) with the town) */
	SM_TOWNS_LIST townsList() {
		std::unordered_map<int32_t, Ptr<Town>> data;
		data.emplace(town->getId(), Ptr<Town>(town));
		return SM_TOWNS_LIST(data);
	}

	static int64_t millisOf(const std::optional<commons::database::Timestamp>& timestamp) {
		return std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.value().time_since_epoch()).count();
	}

	std::optional<lh::PublishedHolder<dataholders::TownSpawnsData>> townSpawns;
	std::optional<lh::PublishedHolder<dataholders::WorldMapsData>> worldMaps;
	const dataholders::WorldMapsData* previousWorldMaps = nullptr; // the holder SetUp replaced, published again by TearDown
	std::vector<Member*> stored;
	Member* elyos = nullptr;
	Member* asmodian = nullptr;
	runtime::Ptr<Town> town;
	std::vector<runtime::Ref<Town>> towns; // keeps the towns alive

	/** Town(id, race), the import constructor (level 1, no points), after SetUp published the spawn data */
	Ptr<Town> importedTown() { return towns.emplace_back(Town::create(1001, model::Race::ELYOS)); }
};

TEST_F(TownLevelUpTest, AThousandPointsLevelTheTownUpAndTellItsRace) {
	town = importedTown();
	ASSERT_EQ(town->getLevel(), 1);
	ASSERT_EQ(millisOf(town->getLevelUpDate()), 60000) << "Town(id, race): new Timestamp(60000)";
	const int64_t before = commons::utils::currentTimeMillis();

	town->increasePoints(1000);

	const int64_t after = commons::utils::currentTimeMillis();
	EXPECT_EQ(town->getLevel(), 2) << "case 1: points + amount >= 1000 -> increaseLevel (level++)";
	EXPECT_EQ(town->getPoints(), 1000) << "the points are added after the level-up";
	EXPECT_GE(millisOf(town->getLevelUpDate()), before) << "levelUpDate.setTime(System.currentTimeMillis())";
	EXPECT_LE(millisOf(town->getLevelUpDate()), after);
	EXPECT_EQ(elyos->count(townsList()), 1) << "broadcastUpdate: SM_TOWNS_LIST of the town to every player of its race";
	EXPECT_EQ(asmodian->count(townsList()), 0) << "player.getRace() == race";
	EXPECT_EQ(town->getPersistentState(), model::gameobjects::Persistable::PersistentState::NEW) << "a NEW town stays NEW (setPersistentState)";
}

TEST_F(TownLevelUpTest, PointsBelowTheThresholdOnlyAddUp) {
	town = importedTown();

	town->increasePoints(999);

	EXPECT_EQ(town->getLevel(), 1);
	EXPECT_EQ(town->getPoints(), 999);
	EXPECT_EQ(millisOf(town->getLevelUpDate()), 60000) << "no level-up: the date stays";
	EXPECT_EQ(elyos->sent().size(), 0u) << "no broadcast";
}

TEST_F(TownLevelUpTest, EachLevelHasItsOwnThresholdAndLevelFiveIsTheLast) {
	town = importedTown();
	town->increasePoints(1000); // 1 -> 2 at 1000
	town->increasePoints(999);  // 1999: level 2's threshold is 2000
	EXPECT_EQ(town->getLevel(), 2);
	town->increasePoints(1); // 2000
	EXPECT_EQ(town->getLevel(), 3);
	town->increasePoints(1000); // 3000
	EXPECT_EQ(town->getLevel(), 4);
	town->increasePoints(1000); // 4000
	EXPECT_EQ(town->getLevel(), 5);
	town->increasePoints(100000);
	EXPECT_EQ(town->getLevel(), 5) << "no case for level 5: the points still add up";
	EXPECT_EQ(town->getPoints(), 104000);
	EXPECT_EQ(elyos->count(opcodeOf<SM_TOWNS_LIST>), 4) << "one broadcast per level-up";
}

TEST_F(TownLevelUpTest, ADatabaseTownLevelsUpFromItsStoredLevel) {
	town = towns.emplace_back(Town::create(1001, 4, 3500, model::Race::ELYOS, commons::database::Timestamp(std::chrono::milliseconds(1700000000000LL))));
	ASSERT_EQ(town->getPersistentState(), model::gameobjects::Persistable::PersistentState::UPDATED) << "the DAO constructor";

	town->increasePoints(500); // 4000: case 4

	EXPECT_EQ(town->getLevel(), 5);
	EXPECT_GT(millisOf(town->getLevelUpDate()), 1700000000000LL);
	EXPECT_EQ(town->getPersistentState(), model::gameobjects::Persistable::PersistentState::UPDATE_REQUIRED);
	EXPECT_EQ(elyos->count(townsList()), 1);
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::team
