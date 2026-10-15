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

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
