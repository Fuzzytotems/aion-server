// M5i I-10 (m5i-plan.md 15.3): //vortexraid (data/handlers/admincommands/VortexRaid.java) on a real Player with a real AionConnection
// (CommandTestSupport.h), over the vortex location of Theobomos (dimensional_vortex.xml, id 0): the syntax, an unknown world, and a stop with no
// raid. Starting a raid spawns its npcs and is the gate's (I-11, the vortex).

#include "CommandTestSupport.h"

#include <map>
#include <string>

#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/VortexData.bind.h"
#include "aion/gameserver/dataholders/VortexData.h"
#include "aion/gameserver/handlers/admincommands/VortexRaid.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

/** ChatCommand.worldName is protected: the test reads it through the command */
struct VortexRaidWorldName : handlers::admincommands::VortexRaid {
	using VortexRaid::worldName;
};

class VortexRaidCommandTest : public CommandTest {
protected:
	void SetUp() override {
		CommandTest::SetUp();
		std::map<std::string, int8_t, std::less<>> levels(*configs::administration::CommandsConfig::ACCESS_LEVELS.get());
		levels["vortexraid"] = 9; // commands.properties: vortexraid = 9
		configs::administration::CommandsConfig::ACCESS_LEVELS.set(levels);
		// VORTEX_DATA: published once for the whole executable (Theobomos' location of dimensional_vortex.xml)
		static const bool published = [] {
			static xml::LoadContext context;
			dataholders::DataManager::VORTEX_DATA.publish(xml::bindString<dataholders::VortexData>(context,
				R"(<dimensional_vortex><vortex_location id="0" defends_race="ELYOS" offence_race="ASMODIANS">)"
				R"(<home_point map="120080000" x="559.4" y="207.8" z="93.5" h="0"/>)"
				R"(<resurrection_point map="210060000" x="951.0" y="2433.0" z="107.0" h="0"/>)"
				R"(<start_point map="210060000" x="951.0" y="2433.0" z="107.0" h="0"/></vortex_location></dimensional_vortex>)"));
			return true;
		}();
		static_cast<void>(published);
	}

	Player& gm() { return connected(730400, "Vortexwarden", 9); }
};

TEST_F(VortexRaidCommandTest, WithoutTwoArgumentsItSendsTheSyntax) {
	Player& admin = gm();
	handlers::admincommands::VortexRaid raid;
	EXPECT_TRUE(raid.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), info(raid.getSyntaxInfo()));
	client()->clearSent();
	EXPECT_TRUE(raid.process(admin, args({"start"})));
	EXPECT_EQ(client()->sentBytes(), info(raid.getSyntaxInfo()));
}

TEST_F(VortexRaidCommandTest, AnUnknownWorldIsAnInvalidLocation) {
	Player& admin = gm();
	handlers::admincommands::VortexRaid raid;
	EXPECT_TRUE(raid.process(admin, args({"start", "Poeta"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid location.")) << "Poeta has no vortex";
	client()->clearSent();
	EXPECT_TRUE(raid.process(admin, args({"start", "Atlantis"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid location.")) << "getMapId of an unknown name is 0";
}

TEST_F(VortexRaidCommandTest, AStopWithoutARaidAndAnUnknownArmAreAnswered) {
	Player& admin = gm();
	handlers::admincommands::VortexRaid raid;
	EXPECT_TRUE(raid.process(admin, args({"STOP", "theobomos"})));
	EXPECT_EQ(client()->sentBytes(), info(VortexRaidWorldName::worldName(210060000) + " is not under siege."));
	client()->clearSent();
	EXPECT_TRUE(raid.process(admin, args({"pause", "theobomos"})));
	EXPECT_EQ(client()->sentBytes(), info(raid.getSyntaxInfo()));
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
