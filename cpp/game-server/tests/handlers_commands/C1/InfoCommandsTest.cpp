// The stage-0 info commands (m5j-plan.md §5.2): //coords, //online, //time, //weather, //zone and //info (data/handlers/admincommands), on a
// real Player with a real AionConnection (CommandTestSupport.h). The texts are the Java literals; numbers the Java prints through
// String.valueOf(float) are written as Java prints them.

#include "CommandTestSupport.h"

#include <algorithm>
#include <string>
#include <vector>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/HouseData.bind.h"
#include "aion/gameserver/dataholders/HouseData.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/handlers/admincommands/Coords.h"
#include "aion/gameserver/handlers/admincommands/Info.h"
#include "aion/gameserver/handlers/admincommands/Online.h"
#include "aion/gameserver/handlers/admincommands/Time.h"
#include "aion/gameserver/handlers/admincommands/Weather.h"
#include "aion/gameserver/handlers/admincommands/Zone.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GAME_TIME.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/GameTimeService.h"
#include "aion/gameserver/utils/time/gametime/GameTime.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using serverpackets::SM_SYSTEM_MESSAGE;

class InfoCommandsTest : public CommandTest {
protected:
	void SetUp() override {
		CommandTest::SetUp();
		// //info's TownService (createZoneInfo, the town residence) is built from HOUSE_DATA when the town table is empty (no database here:
		// TownDAO.load logs its error and returns nothing), as tests/effects_mz/RecoveryEffectsTest.cpp publishes it
		if (!dataholders::DataManager::HOUSE_DATA)
			dataholders::DataManager::HOUSE_DATA.publish(xml::bindString<dataholders::HouseData>(houseContext(), "<house_lands/>"));
	}

	static xml::LoadContext& houseContext() {
		static xml::LoadContext context; // outlives every case: HOUSE_DATA is never reset
		return context;
	}

	void TearDown() override {
		for (Player* player : stored)
			world::World::getInstance().removeObject(*player);
		stored.clear();
		CommandTest::TearDown();
	}

	/** a connected character that World.getAllPlayers lists (Java: PlayerEnterWorldService stores it) */
	Player& online(int32_t objectId, std::string_view name, int8_t accessLevel, model::Race race = model::Race::ELYOS) {
		Player& player = connected(objectId, name, accessLevel);
		player.getCommonData()->setRace(race);
		world::World::getInstance().storeObject(player);
		stored.push_back(&player);
		return player;
	}

	bool sent(const std::vector<uint8_t>& packet, size_t index = 0) {
		const std::vector<std::vector<uint8_t>> all = client(index)->sentBytes();
		return std::ranges::find(all, packet) != all.end();
	}

	std::vector<Player*> stored;
};

// ---- //coords (Coords.java:18-22) --------------------------------------------------------------------------------------------------------

TEST_F(InfoCommandsTest, CoordsShowsTheOwnPositionWithoutATarget) {
	Player& gm = connected(730040, "Warden", 3);
	handlers::admincommands::Coords coords;
	EXPECT_TRUE(coords.process(gm, args({})));
	EXPECT_EQ(client()->sentBytes(),
		info("[charname:Warden;1 1 1]'s position:\nMap ID: 210010000, Instance ID: " + std::to_string(gm.getInstanceId()) +
			 "\nX: 100.0, Y: 100.0, Z: 50.0, Heading: 0"));
}

TEST_F(InfoCommandsTest, CoordsShowsTheTargetsPosition) {
	Player& gm = connected(730041, "Warden", 3);
	Player& other = connected(730042, "Bystander", 0);
	other.setPosition(world::WorldPosition::create(220010000, 1.5f, 2.25f, 3.0f, int8_t{7}));
	gm.setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(&other));
	client()->clearSent(); // the GM's client saw the second character enter
	handlers::admincommands::Coords coords;
	EXPECT_TRUE(coords.process(gm, args({})));
	EXPECT_EQ(client()->sentBytes(),
		info("[charname:Bystander;1 1 1]'s position:\nMap ID: 220010000, Instance ID: " + std::to_string(other.getInstanceId()) +
			 "\nX: 1.5, Y: 2.25, Z: 3.0, Heading: 7"));
}

// ---- //online (Online.java:20-32) --------------------------------------------------------------------------------------------------------

TEST_F(InfoCommandsTest, OnlineCountsElyosAndAsmodians) {
	Player& gm = online(730043, "Warden", 3);
	handlers::admincommands::Online command;
	EXPECT_TRUE(command.process(gm, args({})));
	EXPECT_EQ(client()->sentBytes(), exactly({serialized(SM_SYSTEM_MESSAGE::STR_LIST_USER("1 (1 Elyos / 0 Asmos)"), client().con())}));

	online(730044, "Darkling", 0, model::Race::ASMODIANS);
	client()->clearSent();
	EXPECT_TRUE(command.process(gm, args({})));
	EXPECT_EQ(client()->sentBytes(), exactly({serialized(SM_SYSTEM_MESSAGE::STR_LIST_USER("2 (1 Elyos / 1 Asmo)"), client().con())}))
		<< "one Asmodian: no plural s";
}

// ---- //time (Time.java:24-71) ------------------------------------------------------------------------------------------------------------

TEST_F(InfoCommandsTest, TimeSetsTheDayTimesAndHours) {
	Player& gm = online(730045, "Warden", 3);
	handlers::admincommands::Time time;
	runtime::Ptr<utils::time::gametime::GameTime> gameTime = services::GameTimeService::getInstance().getGameTime();
	const int32_t day = gameTime->getTime() / (24 * 60);

	EXPECT_TRUE(time.process(gm, args({"NIGHT"})));
	EXPECT_EQ(gameTime->getHour(), 22);
	EXPECT_EQ(gameTime->getMinute(), 0);
	EXPECT_EQ(gameTime->getTime() / (24 * 60), day) << "the hour offset stays inside the same day";
	EXPECT_TRUE(sent(serialized(serverpackets::SM_GAME_TIME(), client().con()))) << "broadcastToWorld";
	EXPECT_TRUE(sent(info("You changed the time to 22:00.")[0]));

	client()->clearSent();
	EXPECT_TRUE(time.process(gm, args({"7", "5"})));
	EXPECT_EQ(gameTime->getHour(), 7);
	EXPECT_EQ(gameTime->getMinute(), 5);
	EXPECT_TRUE(sent(info("You changed the time to 7:05.")[0]));

	client()->clearSent();
	EXPECT_TRUE(time.process(gm, args({"dawn"})));
	EXPECT_EQ(gameTime->getHour(), 4);
	EXPECT_EQ(gameTime->getMinute(), 0) << "minute 0 subtracts the current minutes";

	for (auto [arm, hour] : {std::pair{"dusk", 18}, std::pair{"Day", 9}, std::pair{"13", 13}}) {
		EXPECT_TRUE(time.process(gm, args({arm})));
		EXPECT_EQ(gameTime->getHour(), hour) << arm;
	}
}

TEST_F(InfoCommandsTest, TimeRefusesOutOfRangeValues) {
	Player& gm = connected(730046, "Warden", 3);
	handlers::admincommands::Time time;
	runtime::Ptr<utils::time::gametime::GameTime> gameTime = services::GameTimeService::getInstance().getGameTime();
	const int32_t before = gameTime->getTime();

	EXPECT_TRUE(time.process(gm, args({"24"})));
	EXPECT_EQ(client()->sentBytes(), info("Hour must be between 0 and 23."));
	client()->clearSent();
	EXPECT_TRUE(time.process(gm, args({"-1"})));
	EXPECT_EQ(client()->sentBytes(), info("Hour must be between 0 and 23."));
	client()->clearSent();
	EXPECT_TRUE(time.process(gm, args({"5", "60"})));
	EXPECT_EQ(client()->sentBytes(), info("Minute must be between 0 and 59."));
	client()->clearSent();
	EXPECT_TRUE(time.process(gm, args({})));
	EXPECT_EQ(client()->sentBytes(), info(time.getSyntaxInfo()));
	client()->clearSent();
	EXPECT_TRUE(time.process(gm, args({"noon"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid number: \"noon\"")) << "Integer.parseInt's NumberFormatException, ChatCommand.toErrorMessage";
	EXPECT_EQ(gameTime->getTime(), before);
}

// ---- //weather (Weather.java:26-78) ------------------------------------------------------------------------------------------------------

TEST_F(InfoCommandsTest, WeatherArmsWithoutAChange) {
	Player& gm = connected(730047, "Warden", 3);
	handlers::admincommands::Weather weather;
	EXPECT_TRUE(weather.process(gm, args({})));
	EXPECT_EQ(client()->sentBytes(), info(weather.getSyntaxInfo()));

	client()->clearSent();
	EXPECT_TRUE(weather.process(gm, args({"INFO"})));
	EXPECT_EQ(client()->sentBytes(), info("No weather found for this region.")) << "Poeta's test data has no weather zone";

	client()->clearSent();
	EXPECT_TRUE(weather.process(gm, args({"set", "13"})));
	EXPECT_EQ(client()->sentBytes(), info("Weather code must be between 0 and 12."));
	client()->clearSent();
	EXPECT_TRUE(weather.process(gm, args({"set"})));
	EXPECT_EQ(client()->sentBytes(), info("Weather code must be between 0 and 12.")) << "no code is -1";

	client()->clearSent();
	EXPECT_TRUE(weather.process(gm, args({"next", "1"})));
	EXPECT_EQ(client()->sentBytes(), info(weather.getSyntaxInfo()));

	client()->clearSent();
	EXPECT_TRUE(weather.process(gm, args({"rain"})));
	EXPECT_TRUE(client()->sentBytes().empty()) << "the switch has no default arm";
}

// ---- //zone (Zone.java:30-74) ------------------------------------------------------------------------------------------------------------

TEST_F(InfoCommandsTest, ZoneArms) {
	Player& gm = connected(730048, "Warden", 3);
	handlers::admincommands::Zone zone;
	EXPECT_TRUE(zone.process(gm, args({})));
	EXPECT_EQ(client()->sentBytes(), info("[charname:Warden;1 1 1] is not in any zone."));

	client()->clearSent();
	EXPECT_TRUE(zone.process(gm, args({"a", "b"})));
	EXPECT_EQ(client()->sentBytes(), info(zone.getSyntaxInfo()));

	client()->clearSent();
	EXPECT_TRUE(zone.process(gm, args({"REFRESH"})));
	EXPECT_TRUE(client()->sentBytes().empty());

	client()->clearSent();
	EXPECT_TRUE(zone.process(gm, args({"NO_SUCH_ZONE_NAME_1"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid zone name.")) << "IllegalArgumentException, ChatCommand.toErrorMessage";
}

// ---- //info (Info.java:40-195) -----------------------------------------------------------------------------------------------------------

// On a Player, the second line reads TownService.getTownResidence -> Player.getActiveHouse -> HousingService, which loads the houses from the
// database (HousesDAO, PlayerDAO.getUsedIDs). Without one the command stops there with ChatCommand.run's error line; the full listing is the
// M5i gate's (a GM account on the test database). This case pins the header line and that the failure is caught as Java catches it.
TEST_F(InfoCommandsTest, InfoOnAPlayerStartsWithTheHeaderLine) {
	Player& gm = connected(730049, "Warden", 3);
	handlers::admincommands::Info command;
	EXPECT_TRUE(command.process(gm, args({})));
	const std::vector<std::vector<uint8_t>> all = client()->sentBytes();
	ASSERT_EQ(all.size(), 2u);
	// Java: getClass().getSimpleName() - the fixture's Player subclass (tests/cm_ak/InWorldPacketRunSupport.h); in the game it is "Player"
	EXPECT_EQ(all[0], info("[Info about TestPlayer]\n\tName: [charname:Warden;1 1 1], ID: " + std::to_string(gm.getObjectTemplate()->getTemplateId()) +
						   ", ObjectId: 730049")[0]);
	EXPECT_EQ(all[1], info("<Error while executing command>")[0]) << "AdminCommand.process: run returned false (no database here)";
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
