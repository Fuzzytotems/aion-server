// M5i I-5 (m5i-plan.md 15.3): //worldraid (data/handlers/admincommands/WorldRaid.java) on a real Player with a real AionConnection
// (CommandTestSupport.h): the disabled answer, the syntax, the active list, and the start/stop answers over test locations published as
// WORLD_RAID_DATA. A raid's timeline is the gate's (I-11); a raid started here never reaches its first minute.

#include "CommandTestSupport.h"

#include <map>
#include <memory>
#include <string>

#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/configs/main/EventsConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/WorldRaidData.bind.h"
#include "aion/gameserver/dataholders/WorldRaidData.h"
#include "aion/gameserver/handlers/admincommands/WorldRaid.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/WorldRaidService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

class WorldRaidCommandTest : public CommandTest {
protected:
	void SetUp() override {
		CommandTest::SetUp();
		std::map<std::string, int8_t, std::less<>> levels(*configs::administration::CommandsConfig::ACCESS_LEVELS.get());
		levels["worldraid"] = 9; // commands.properties: worldraid = 9
		configs::administration::CommandsConfig::ACCESS_LEVELS.set(levels);
		savedEnabled = configs::main::EventsConfig::ENABLE_WORLDRAID.load();
		configs::main::EventsConfig::ENABLE_WORLDRAID.store(true);
		dataholders::DataManager::WORLD_RAID_DATA.publish(xml::bindString<dataholders::WorldRaidData>(context,
			R"(<world_raid_locations><world_raid_location location_id="1" map_id="210050000" x="1" y="2" z="3">)"
			R"(<world_raid_npcs><world_raid_npc npc_id="235000"/></world_raid_npcs><location_markers><spot x="1" y="1" z="1"/></location_markers>)"
			R"(</world_raid_location></world_raid_locations>)"));
		services::WorldRaidService::getInstance().initWorldRaidLocations();
	}

	void TearDown() override {
		try {
			services::WorldRaidService::getInstance().stopRaid(1); // removes the raid; its finish throws for the flag it has not spawned
		} catch (const runtime::NullPointerException&) {
		}
		configs::main::EventsConfig::ENABLE_WORLDRAID.store(false);
		services::WorldRaidService::getInstance().initWorldRaidLocations();
		dataholders::DataManager::WORLD_RAID_DATA.resetForTests();
		configs::main::EventsConfig::ENABLE_WORLDRAID.store(savedEnabled);
		CommandTest::TearDown();
	}

	Player& gm() { return connected(730300, "Raidwarden", 9); }

	xml::LoadContext context;
	bool savedEnabled = false;
};

TEST_F(WorldRaidCommandTest, DisabledWorldRaidsAnswerOnlyThat) {
	Player& admin = gm();
	configs::main::EventsConfig::ENABLE_WORLDRAID.store(false);
	handlers::admincommands::WorldRaid worldRaid;
	EXPECT_TRUE(worldRaid.process(admin, args({"list"})));
	EXPECT_EQ(client()->sentBytes(), info("World raid currently is disabled."));
}

TEST_F(WorldRaidCommandTest, TheSyntaxAndAnEmptyActiveList) {
	Player& admin = gm();
	handlers::admincommands::WorldRaid worldRaid;
	EXPECT_TRUE(worldRaid.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), info(worldRaid.getSyntaxInfo()));
	client()->clearSent();
	EXPECT_TRUE(worldRaid.process(admin, args({"start"})));
	EXPECT_EQ(client()->sentBytes(), info(worldRaid.getSyntaxInfo())) << "start needs a location id";
	client()->clearSent();
	EXPECT_TRUE(worldRaid.process(admin, args({"Active"})));
	EXPECT_EQ(client()->sentBytes(), info("Currently active world raids:\n\tNo locations available!"));
}

TEST_F(WorldRaidCommandTest, StartAndStopAnswerTheRaidState) {
	Player& admin = gm();
	handlers::admincommands::WorldRaid worldRaid;
	EXPECT_TRUE(worldRaid.process(admin, args({"start", "99"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid world raid location: 99"));
	client()->clearSent();
	EXPECT_TRUE(worldRaid.process(admin, args({"stop", "1"})));
	EXPECT_EQ(client()->sentBytes(), info("World raid for location 1 is not started."));
	client()->clearSent();
	EXPECT_TRUE(worldRaid.process(admin, args({"start", "1"})));
	EXPECT_EQ(client()->sentBytes(), info("Starting world raid for location 1"));
	EXPECT_TRUE(services::WorldRaidService::getInstance().isWorldRaidInProgress(1));
	client()->clearSent();
	EXPECT_TRUE(worldRaid.process(admin, args({"start", "1"})));
	EXPECT_EQ(client()->sentBytes(), info("World raid for location 1 is already in progress"));
	client()->clearSent();
	EXPECT_TRUE(worldRaid.process(admin, args({"pause", "1"})));
	EXPECT_EQ(client()->sentBytes(), info(worldRaid.getSyntaxInfo())) << "an unknown arm with a valid location";
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
