// M5i I-5 (m5i-plan.md 15.3): //rift (data/handlers/admincommands/Rift.java) on a real Player with a real AionConnection (CommandTestSupport.h):
// the syntax, the location list and the id argument, with rifts disabled (no rift locations). Opening and closing real rifts is the gate's
// (I-11, a rift).

#include "CommandTestSupport.h"

#include <map>
#include <string>

#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/handlers/admincommands/Rift.h"
#include "aion/gameserver/services/RiftService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

class RiftCommandTest : public CommandTest {
protected:
	void SetUp() override {
		CommandTest::SetUp();
		std::map<std::string, int8_t, std::less<>> levels(*configs::administration::CommandsConfig::ACCESS_LEVELS.get());
		levels["rift"] = 9; // commands.properties: rift = 9
		configs::administration::CommandsConfig::ACCESS_LEVELS.set(levels);
		savedRiftEnabled = configs::main::CustomConfig::RIFT_ENABLED.load();
		configs::main::CustomConfig::RIFT_ENABLED.store(false);
		services::RiftService::getInstance().initRiftLocations(); // no rift locations
	}

	void TearDown() override {
		configs::main::CustomConfig::RIFT_ENABLED.store(savedRiftEnabled);
		CommandTest::TearDown();
	}

	Player& gm() { return connected(730200, "Riftwarden", 9); }

	bool savedRiftEnabled = false;
};

TEST_F(RiftCommandTest, WithoutAnArmItSendsTheSyntax) {
	Player& admin = gm();
	handlers::admincommands::Rift rift;
	EXPECT_TRUE(rift.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), info(rift.getSyntaxInfo()));
	client()->clearSent();
	EXPECT_TRUE(rift.process(admin, args({"open"})));
	EXPECT_EQ(client()->sentBytes(), info(rift.getSyntaxInfo())) << "open needs an id";
}

TEST_F(RiftCommandTest, TheListOfNoLocationsIsItsHeader) {
	Player& admin = gm();
	handlers::admincommands::Rift rift;
	EXPECT_TRUE(rift.process(admin, args({"LIST"})));
	EXPECT_EQ(client()->sentBytes(), info("Rift locations:"));
}

TEST_F(RiftCommandTest, AnUnknownIdIsRefused) {
	Player& admin = gm();
	handlers::admincommands::Rift rift;
	EXPECT_TRUE(rift.process(admin, args({"open", "2120", "g"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid rift world ID or location ID."));
	client()->clearSent();
	EXPECT_TRUE(rift.process(admin, args({"close", "210020000"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid rift world ID or location ID."));
	client()->clearSent();
	EXPECT_TRUE(rift.process(admin, args({"open", "eltnen"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid number: \"eltnen\"")) << "Integer.parseInt's NumberFormatException (ChatCommand.run)";
}

// C++-dev only (owner, 2026-10-31): open all and close all go through every world that has rift locations; with none, nothing changes
TEST_F(RiftCommandTest, OpenAllAndCloseAllWithNoLocationsChangeNothing) {
	Player& admin = gm();
	handlers::admincommands::Rift rift;
	EXPECT_TRUE(rift.process(admin, args({"open", "ALL", "g"})));
	EXPECT_EQ(client()->sentBytes(), info("Rifts are already open.")) << "all is not parsed as an id";
	client()->clearSent();
	EXPECT_TRUE(rift.process(admin, args({"close", "all"})));
	EXPECT_EQ(client()->sentBytes(), info("Rifts were already closed."));
	const std::string syntax = rift.getSyntaxInfo(); // parseSyntaxInfo's formatting of each line
	for (const char* part : {"[color:open;1 1 1] [color:all;1 1 1] [[color:g;1 1 1]] - Opens the rifts in every zone.",
	                         "[color:close;1 1 1] [color:all;1 1 1] - Closes the rifts in every zone."})
		EXPECT_NE(syntax.find(part), std::string::npos) << part << " in " << syntax;
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
