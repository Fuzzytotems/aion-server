// M5h-2 H2-5 (m5h-plan.md 14.3): the argument handling of //auction, //house and //legion (data/handlers/admincommands/Auction.java,
// HouseCommand.java, LegionCommand.java) on a real Player with a real AionConnection (CommandTestSupport.h): the syntax info, the refusals the
// commands answer before they reach HousingService, HousingBidService or LegionService, and ChatCommand.run's messages for a bad number or an
// unknown enum constant. The arms that reach the services are driven by the gate gs.scenario.m5h2 (G1-G3), on its database.

#include "CommandTestSupport.h"

#include <string>
#include <vector>

#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/handlers/admincommands/Auction.h"
#include "aion/gameserver/handlers/admincommands/HouseCommand.h"
#include "aion/gameserver/handlers/admincommands/LegionCommand.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

class HousingCommandsTest : public CommandTest {
protected:
	void SetUp() override {
		CommandTest::SetUp();
		std::map<std::string, int8_t, std::less<>> levels(*configs::administration::CommandsConfig::ACCESS_LEVELS.get());
		for (const char* alias : {"auction", "house", "legion"}) // commands.properties: auction = 9, house = 9, legion = 9
			levels[alias] = 9;
		configs::administration::CommandsConfig::ACCESS_LEVELS.set(levels);
	}

	Player& gm() { return connected(730090, "Steward", 9); }
};

// ---- //auction (Auction.java:35-142) ------------------------------------------------------------------------------------------------------

TEST_F(HousingCommandsTest, AuctionWithoutArgumentsSendsTheSyntax) {
	Player& admin = gm();
	handlers::admincommands::Auction auction;
	EXPECT_TRUE(auction.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), info(auction.getSyntaxInfo()));
}

TEST_F(HousingCommandsTest, AuctionEndWithoutAnAddressSendsTheSyntax) {
	Player& admin = gm();
	handlers::admincommands::Auction auction;
	EXPECT_TRUE(auction.process(admin, args({"end"})));
	EXPECT_EQ(client()->sentBytes(), info(auction.getSyntaxInfo())) << "params.length < 2";
}

TEST_F(HousingCommandsTest, AuctionAddWithTheWrongArgumentCountSendsTheSyntax) {
	Player& admin = gm();
	handlers::admincommands::Auction auction;
	EXPECT_TRUE(auction.process(admin, args({"add", "elyos", "house"})));
	EXPECT_EQ(client()->sentBytes(), info(auction.getSyntaxInfo())) << "params.length < 4";
}

TEST_F(HousingCommandsTest, AuctionAddRefusesACountBelowOne) {
	Player& admin = gm();
	handlers::admincommands::Auction auction;
	EXPECT_TRUE(auction.process(admin, args({"add", "elyos", "house", "0"})));
	EXPECT_EQ(client()->sentBytes(), info("Count must be positive."));
}

TEST_F(HousingCommandsTest, AuctionAddWithABadCountAnswersInvalidNumber) {
	Player& admin = gm();
	handlers::admincommands::Auction auction;
	EXPECT_TRUE(auction.process(admin, args({"add", "elyos", "house", "many"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid number: \"many\"")) << "Integer.parseInt's NumberFormatException (ChatCommand.run)";
}

TEST_F(HousingCommandsTest, AuctionAddWithAnUnknownHouseTypeListsTheTypes) {
	Player& admin = gm();
	handlers::admincommands::Auction auction;
	EXPECT_TRUE(auction.process(admin, args({"add", "elyos", "castle", "1"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid house type.\nPossible values:\nESTATE, MANSION, HOUSE, STUDIO, PALACE"))
		<< "HouseType.valueOf's IllegalArgumentException (ChatCommand.run)";
}

TEST_F(HousingCommandsTest, AuctionWithAnUnknownArmSendsTheSyntax) {
	Player& admin = gm();
	handlers::admincommands::Auction auction;
	EXPECT_TRUE(auction.process(admin, args({"sell", "10001"})));
	EXPECT_EQ(client()->sentBytes(), info(auction.getSyntaxInfo()));
}

// ---- //house (HouseCommand.java:39-55) -----------------------------------------------------------------------------------------------------

TEST_F(HousingCommandsTest, HouseWithoutAnAddressSendsTheSyntax) {
	Player& admin = gm();
	handlers::admincommands::HouseCommand house;
	EXPECT_TRUE(house.process(admin, args({"own"})));
	EXPECT_EQ(client()->sentBytes(), info(house.getSyntaxInfo())) << "own needs params.length >= 2";
}

TEST_F(HousingCommandsTest, HouseWithABadAddressAnswersInvalidNumber) {
	Player& admin = gm();
	handlers::admincommands::HouseCommand house;
	EXPECT_TRUE(house.process(admin, args({"tp", "home"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid number: \"home\"")) << "getHouse: Integer.parseInt";
}

// ---- //legion (LegionCommand.java:37-132) ---------------------------------------------------------------------------------------------------

TEST_F(HousingCommandsTest, LegionWithOneArgumentSendsTheSyntax) {
	Player& admin = gm();
	handlers::admincommands::LegionCommand legion;
	EXPECT_TRUE(legion.process(admin, args({"info"})));
	EXPECT_EQ(client()->sentBytes(), info(legion.getSyntaxInfo())) << "params.length < 2";
}

TEST_F(HousingCommandsTest, LegionSetLevelWithoutALevelSendsTheSyntax) {
	Player& admin = gm();
	handlers::admincommands::LegionCommand legion;
	EXPECT_TRUE(legion.process(admin, args({"setlevel", "Banner"})));
	EXPECT_EQ(client()->sentBytes(), info(legion.getSyntaxInfo())) << "setlevel needs params.length >= 3: the last arm";
}

TEST_F(HousingCommandsTest, LegionWithAnUnknownArmSendsTheSyntax) {
	Player& admin = gm();
	handlers::admincommands::LegionCommand legion;
	EXPECT_TRUE(legion.process(admin, args({"promote", "Banner"})));
	EXPECT_EQ(client()->sentBytes(), info(legion.getSyntaxInfo()));
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
