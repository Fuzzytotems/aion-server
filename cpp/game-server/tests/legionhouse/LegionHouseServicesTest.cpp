// The P5-11 services on the test database (m5a-plan.md E2-04, E2-05): HousingService's load (houses by address, the inactive state of a second
// house, the lookups of the character list and SM_HOUSE_OWNER_INFO), HousingBidService without bids, LegionService's lookups for characters without
// a legion (the character list path), and LegionDominionService.initLocations (SM_LEGION_DOMINION_LOC_INFO).
//
// Expectations are derived by hand from HousingService.java:57-95,184-213,289-296; HousingBidService.java:40-52,326-343;
// LegionService.java:102-160,518-532; LegionDominionService.java:46-62 and LegionDominionLocation.java:30-33. Each test constructs the singletons it
// uses: one process per test case.

#include <gtest/gtest.h>

#include <format>
#include <string_view>
#include <vector>

#include "LegionHouseTestSupport.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/HouseData.bind.h"
#include "aion/gameserver/dataholders/HouseData.h"
#include "aion/gameserver/dataholders/LegionDominionData.bind.h"
#include "aion/gameserver/dataholders/LegionDominionData.h"
#include "aion/gameserver/dataholders/MaterialData.bind.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/MaterialData.h"
#include "aion/gameserver/dataholders/ShieldData.bind.h"
#include "aion/gameserver/dataholders/TownSpawnsData.bind.h"
#include "aion/gameserver/dataholders/TownSpawnsData.h"
#include "aion/gameserver/dataholders/ShieldData.h"
#include "aion/gameserver/dataholders/WorldMapsData.bind.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/dataholders/ZoneData.bind.h"
#include "aion/gameserver/dataholders/ZoneData.h"
#include "aion/gameserver/configs/main/HousingConfig.h"
#include "aion/gameserver/model/gameobjects/player/Mailbox.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/house/HouseDoorState.h"
#include "aion/gameserver/model/legionDominion/LegionDominionLocation.h"
#include "aion/gameserver/model/templates/housing/HouseAddress.h"
#include "aion/gameserver/services/HousingBidService.h"
#include "aion/gameserver/services/HousingService.h"
#include "aion/gameserver/services/LegionDominionService.h"
#include "aion/gameserver/services/LegionService.h"
#include "aion/gameserver/services/TownService.h"
#include "aion/gameserver/model/town/Town.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/geo/GeoService.h"

namespace aion::gameserver::legionhouse::test {
namespace {

using model::house::House;
using runtime::Ptr;

constexpr std::string_view HOUSE_LANDS = R"(<house_lands>
	<land id="325001" teleport_npc="810003" manager_npc="810017" sign_home="810007" sign_waiting="810006" sign_sale="810005" sign_nosale="810004">
		<addresses>
			<address id="10001" map="700010000" town="1001" x="696.159973" y="1999.969971" z="174.42577"/>
			<address id="10002" map="700010000" town="1001" x="609.904907" y="2057.186279" z="174.90712"/>
			<address id="10003" map="700010000" town="1001" x="779.445618" y="2061.070068" z="174.19347"/>
		</addresses>
		<buildings>
			<building id="350000" default="true" type="PERSONAL_FIELD" size="HOUSE"/>
		</buildings>
		<sale level="50" gold_price="1000000000" point_price="0"/>
		<fee>20000000</fee>
		<caps room="false" floor="false" emblemId="2" addon="true"/>
	</land>
</house_lands>)";

constexpr std::string_view LEGION_DOMINIONS = R"(<legion_dominion_template>
	<legion_dominion_location id="2" world_id="220080000" zone="LegionDominionArea_02" race="ASMODIANS" name_id="404624">
		<reward rank="1" item_id="188053896" count="1"/>
	</legion_dominion_location>
	<legion_dominion_location id="1" world_id="210070000" zone="LegionDominionArea_01" race="ELYOS" name_id="404623">
		<reward rank="1" item_id="188053896" count="1"/>
	</legion_dominion_location>
</legion_dominion_template>)";

class LegionHouseServicesTest : public LegionHouseTest {
protected:
	static void insertHouse(int32_t id, int32_t playerId, int32_t address, std::string_view acquireTime) {
		execute(std::format("INSERT INTO houses (id, player_id, building_id, address, acquire_time, settings) VALUES ({}, {}, 350000, {}, {}, 257)", id,
			playerId, address, acquireTime.empty() ? std::string("NULL") : "'" + std::string(acquireTime) + "'"));
	}
};

TEST_F(LegionHouseServicesTest, HousingServiceLoadsHousesAndMarksTheNewerHouseOfAnOwnerInactive) {
	LEGIONHOUSE_REQUIRE_DATABASE();
	PublishedHolder houseData(dataholders::DataManager::HOUSE_DATA, bindXml<dataholders::HouseData>(HOUSE_LANDS));
	insertPlayer(100, "Owner", 1);
	insertHouse(900001, 100, 10002, "2025-02-01 10:00:00"); // acquired later -> inactive
	insertHouse(900002, 100, 10001, "2025-01-01 10:00:00"); // oldest -> active
	insertHouse(900003, 0, 10003, "");                      // unoccupied -> active

	services::HousingService& service = services::HousingService::getInstance();

	std::vector<Ptr<House>> houses = service.getCustomHouses();
	EXPECT_EQ(houses.size(), 3u);
	Ptr<House> active = service.findActiveHouse(100);
	ASSERT_TRUE(active);
	EXPECT_EQ(active->getObjectId(), 900002);
	Ptr<House> inactive = service.findInactiveHouse(100);
	ASSERT_TRUE(inactive);
	EXPECT_EQ(inactive->getObjectId(), 900001);
	EXPECT_EQ(service.findPlayerHouses(100).size(), 2u);
	EXPECT_TRUE(service.findPlayerHouses(200).empty());
	EXPECT_FALSE(service.findActiveHouse(200));
	EXPECT_FALSE(service.findInactiveHouse(0)) << "unoccupied houses are never inactive";
	ASSERT_TRUE(service.getHouseByAddress(10003));
	EXPECT_EQ(service.getHouseByAddress(10003)->getObjectId(), 900003);
	EXPECT_FALSE(service.getHouseByAddress(10004));
	EXPECT_EQ(service.findHouse(900001), inactive);
	EXPECT_FALSE(service.findStudio(900001));
	EXPECT_EQ(service.findHouseOrStudio(900003)->getAddress()->getId(), 10003);
	EXPECT_FALSE(service.getPlayerStudio(100));
	EXPECT_TRUE(active->isShowOwnerName()) << "settings 257 = show owner name, door state OPEN";

	// HousingBidService: no bids; setBidInfoToHouses leaves the houses without bids
	services::HousingBidService& bids = services::HousingBidService::getInstance();
	EXPECT_FALSE(bids.getBidInfo(*active));
	EXPECT_FALSE(active->getBids());
	EXPECT_NO_THROW(bids.disableBids(100));
	// a player without houses: onPlayerDeleted only disables the (absent) bids
	EXPECT_NO_THROW(service.onPlayerDeleted(200));
}

TEST_F(LegionHouseServicesTest, DeletedPlayerLosesHisHouses) {
	LEGIONHOUSE_REQUIRE_DATABASE();
	PublishedHolder houseData(dataholders::DataManager::HOUSE_DATA, bindXml<dataholders::HouseData>(HOUSE_LANDS));
	insertPlayer(100, "Owner", 1);
	insertHouse(900001, 100, 10002, "2025-02-01 10:00:00");
	insertHouse(900002, 100, 10001, "2025-01-01 10:00:00");
	// notifyAboutOwnerChange looks the owner up in the World, whose construction reads the world maps, zones, shields and materials (none here)
	PublishedHolder worldMaps(dataholders::DataManager::WORLD_MAPS_DATA, bindXml<dataholders::WorldMapsData>("<world_maps></world_maps>"));
	PublishedHolder zones(dataholders::DataManager::ZONE_DATA, bindXml<dataholders::ZoneData>("<zones></zones>"));
	PublishedHolder shields(dataholders::DataManager::SHIELD_DATA, bindXml<dataholders::ShieldData>("<shields/>"));
	PublishedHolder materials(dataholders::DataManager::MATERIAL_DATA, bindXml<dataholders::MaterialData>("<material_templates/>"));
	services::HousingService& service = services::HousingService::getInstance();
	ASSERT_EQ(service.findPlayerHouses(100).size(), 2u);

	// changeOwner(house, 0) for each house: registry reset, no owner, closed door, no acquire time, stored (HousingService.java:97-140)
	service.onPlayerDeleted(100);

	EXPECT_TRUE(service.findPlayerHouses(100).empty());
	Ptr<House> house = service.findHouse(900002);
	ASSERT_TRUE(house);
	EXPECT_EQ(house->getOwnerId(), 0);
	EXPECT_FALSE(house->isInactive());
	EXPECT_FALSE(house->getAcquiredTime());
	EXPECT_EQ(house->getDoorState(), model::house::HouseDoorState::CLOSED);
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM houses WHERE player_id = 100"), 0);
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM houses WHERE player_id = 0 AND acquire_time IS NULL"), 2);
}

TEST_F(LegionHouseServicesTest, CanOwnHouseNeedsTheCompletedHousingQuest) {
	LEGIONHOUSE_REQUIRE_DATABASE();
	PublishedHolder houseData(dataholders::DataManager::HOUSE_DATA, bindXml<dataholders::HouseData>(HOUSE_LANDS));
	PlayerFixture fixture = makePlayer(100001, 1001);
	fixture.player->setQuestStateList(model::gameobjects::player::QuestStateList::create());
	EXPECT_FALSE(services::HousingService::getInstance().canOwnHouse(*fixture.player, false)) << "quest 18802 is not completed";
}

TEST_F(LegionHouseServicesTest, LegionLookupsForCharactersWithoutLegion) {
	LEGIONHOUSE_REQUIRE_DATABASE();
	insertPlayer(100001, "Tester", 1001);
	PlayerFixture fixture = makePlayer(100001, 1001);
	services::LegionService& service = services::LegionService::getInstance();

	EXPECT_FALSE(service.getLegionMember(*fixture.commonData));
	EXPECT_FALSE(service.getLegionMember(100001));
	EXPECT_FALSE(service.getLegion(1234567)) << "no legions row";
	EXPECT_FALSE(service.getLegion("Nobody"));
	EXPECT_TRUE(service.getCachedLegions().empty());
	EXPECT_NO_THROW(service.LegionWhUpdate(*fixture.player)) << "no legion: nothing to store";
}

TEST_F(LegionHouseServicesTest, LegionDominionLocationsAreCreatedAndSortedById) {
	LEGIONHOUSE_REQUIRE_DATABASE();
	PublishedHolder dominions(dataholders::DataManager::LEGION_DOMINION_DATA, bindXml<dataholders::LegionDominionData>(LEGION_DOMINIONS));
	services::LegionDominionService& service = services::LegionDominionService::getInstance();

	service.initLocations();

	std::vector<Ptr<model::legionDominion::LegionDominionLocation>> locations = service.getLegionDominions();
	ASSERT_EQ(locations.size(), 2u);
	EXPECT_EQ(locations[0]->getLocationId(), 1) << "TreeMap order";
	EXPECT_EQ(locations[1]->getLocationId(), 2);
	EXPECT_EQ(locations[0]->getZoneNameAsString(), "LegionDominionArea_01_210070000");
	EXPECT_EQ(locations[1]->getWorldId(), 220080000);
	EXPECT_EQ(locations[1]->getRace(), model::Race::ASMODIANS);
	EXPECT_EQ(locations[0]->getLegionId(), 0);
	ASSERT_TRUE(locations[0]->getParticipantInfo());
	EXPECT_TRUE(locations[0]->getParticipantInfo()->isEmpty());
	EXPECT_FALSE(locations[0]->getParticipantInfo(4711));
	EXPECT_EQ(service.getLegionDominionLoc(2), locations[1]);
	EXPECT_FALSE(service.getLegionDominionLoc(3));
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM legion_dominion_locations"), 2) << "LegionDominionDAO.loadOrCreateLegionDominionLocations inserts them";
}


TEST_F(LegionHouseServicesTest, TownServiceCreatesTheTownsOfTheHouseAddressesAndSendsTheListOnlyInTheHousingMap) {
	LEGIONHOUSE_REQUIRE_DATABASE();
	// the three addresses of the land all name town 1001, and its manager npc is of tribe GENERAL, so the town belongs to the elyos
	PublishedHolder houseData(dataholders::DataManager::HOUSE_DATA, bindXml<dataholders::HouseData>(HOUSE_LANDS));
	PublishedHolder npcData(dataholders::DataManager::NPC_DATA,
		bindXml<dataholders::NpcData>(R"(<npc_templates><npc_template npc_id="810017" level="1" name="palace butler" name_id="461896")"
									  R"( rank="DISCIPLINED" rating="NORMAL" tribe="GENERAL" type="HOUSING" attack_speed="2000">)"
									  R"(<stats maxHp="2256"><speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23")"
									  R"( group_run_fight="4.23"/></stats></npc_template></npc_templates>)"));
	// the town has a spawn map with an empty level 1, so Town.spawnNewObjects spawns nothing (Java iterates the empty list)
	PublishedHolder townSpawns(dataholders::DataManager::TOWN_SPAWNS_DATA,
		bindXml<dataholders::TownSpawnsData>(R"(<town_spawns_data><spawn_map map_id="700010000"><town_spawn town_id="1001">)"
											 R"(<town_level level="1"/></town_spawn></spawn_map></town_spawns_data>)"));
	// the Town constructor calls GeoService.updateTown, which needs the geo maps of the two housing worlds
	PublishedHolder worldMaps(dataholders::DataManager::WORLD_MAPS_DATA,
		bindXml<dataholders::WorldMapsData>(R"(<world_maps><map id="700010000" cName="Oriel" death_level="0" water_level="0" flags="BIND"/>)"
											R"(<map id="710010000" cName="Pernon" death_level="0" water_level="0" flags="BIND"/></world_maps>)"));
	world::geo::GeoService::getInstance().init();
	ASSERT_EQ(queryLong("SELECT COUNT(*) FROM towns"), 0) << "the import branch runs only for an empty towns table";

	services::TownService& service = services::TownService::getInstance();

	Ptr<model::town::Town> town = service.getTownById(1001);
	ASSERT_TRUE(town) << "one town for the three addresses of town 1001";
	EXPECT_EQ(town->getId(), 1001);
	EXPECT_EQ(town->getLevel(), 1) << "Town(id, race): level 1, no points";
	EXPECT_EQ(town->getPoints(), 0);
	EXPECT_EQ(town->getRace(), model::Race::ELYOS);
	EXPECT_EQ(town->getL10nId(), 403330) << "403330 + (1001 - 1001)";
	EXPECT_TRUE(town->getLevelUpDate()) << "the C++ member is never null (docs/deviations/P5-11.md)";
	EXPECT_FALSE(service.getTownById(2001)) << "no asmodian land in the test data";
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM towns WHERE id = 1001 AND level = 1"), 1) << "TownDAO.store inserted the new town";

	// increasePoints below the level 1 threshold of 1000 only adds points (Town.java:60-80)
	town->increasePoints(50);
	EXPECT_EQ(town->getPoints(), 50);
	EXPECT_EQ(town->getLevel(), 1);

	PlayerFixture fixture = makePlayer(100001, 1001);
	fixture.player->setPosition(Ptr<world::WorldPosition>(world::WorldPosition::create(210010000, 1.0f, 2.0f, 3.0f, int8_t{0})));
	EXPECT_EQ(service.getTownResidence(*fixture.player), 0) << "no active house";
	EXPECT_EQ(service.getTownIdByPosition(*fixture.player), 0) << "no town spawn template and not spawned";

	// onEnterWorld sends SM_TOWNS_LIST only in the elyos housing map (TownService.java:104-119)
	runtime::resetUnportedHitsForTests();
	EXPECT_NO_THROW(service.onEnterWorld(*fixture.player)) << "Poeta: no towns list";
	fixture.player->setPosition(Ptr<world::WorldPosition>(world::WorldPosition::create(700010000, 1.0f, 2.0f, 3.0f, int8_t{0})));
	EXPECT_NO_THROW(service.onEnterWorld(*fixture.player)) << "Oriel: the elyos towns list";
	PlayerFixture asmodian = makePlayer(100002, 1002, "Asmo", model::Race::ASMODIANS);
	asmodian.player->setPosition(Ptr<world::WorldPosition>(world::WorldPosition::create(710010000, 1.0f, 2.0f, 3.0f, int8_t{0})));
	EXPECT_NO_THROW(service.onEnterWorld(*asmodian.player)) << "Pernon: the (empty) asmodian towns list";
	EXPECT_EQ(runtime::unportedHitCount(), 0u) << "the enter-world path of E2-05 reaches no unported body";
}


/** Sets an atomic configuration field for the scope and restores it (the tests share the process-wide configuration) */
template <class T>
class ConfigScope {
public:
	ConfigScope(std::atomic<T>& configValue, T value) : config(configValue), previous(configValue.load()) { config.store(value); }
	~ConfigScope() { config.store(previous); }
	ConfigScope(const ConfigScope&) = delete;
	ConfigScope& operator=(const ConfigScope&) = delete;

private:
	std::atomic<T>& config;
	const T previous;
};

TEST_F(LegionHouseServicesTest, HousingOnPlayerLoginChecksTheOverdueHouseAndTheMailboxOfACharacterWithoutOne) {
	LEGIONHOUSE_REQUIRE_DATABASE();
	PublishedHolder houseData(dataholders::DataManager::HOUSE_DATA, bindXml<dataholders::HouseData>(HOUSE_LANDS));
	insertPlayer(100, "Owner", 1);
	insertHouse(900001, 100, 10001, "2025-01-01 10:00:00");
	services::HousingService& service = services::HousingService::getInstance();

	// the owner's only house is active and its maintenance fee is overdue: STR_MSG_HOUSING_OVERDUE, then SM_HOUSE_OWNER_INFO
	ConfigScope<bool> housePay(configs::main::HousingConfig::ENABLE_HOUSE_PAY, true);
	PlayerFixture owner = makePlayer(100, 1, "Owner");
	Ptr<House> house = owner.player->getActiveHouse();
	ASSERT_TRUE(house);
	house->setNextPay(commons::database::Timestamp(std::chrono::milliseconds(1)));
	runtime::resetUnportedHitsForTests();
	EXPECT_NO_THROW(service.onPlayerLogin(*owner.player));
	EXPECT_EQ(runtime::unportedHitCount(), 0u) << "the overdue branch reaches no unported body";

	// a character without a house: the empty mailbox is scanned for the $$HS_OVERDUE_ letters, then SM_HOUSE_OWNER_INFO
	PlayerFixture other = makePlayer(200, 2, "Other");
	other.player->setMailbox(std::make_unique<model::gameobjects::player::Mailbox>(*other.player));
	ASSERT_FALSE(other.player->getActiveHouse());
	EXPECT_NO_THROW(service.onPlayerLogin(*other.player));
	EXPECT_EQ(runtime::unportedHitCount(), 0u) << "the mailbox branch reaches no unported body";

	// HousingBidService.onPlayerLogin without an auction mail: nothing to parse (HousingBidService.java:326-343)
	services::HousingBidService& bids = services::HousingBidService::getInstance();
	EXPECT_NO_THROW(bids.onPlayerLogin(*other.player));
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

} // namespace
} // namespace aion::gameserver::legionhouse::test
