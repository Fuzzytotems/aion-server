// M5j stage 4 CP6 (m5j-plan.md §18.5, the orphaned commands of M5b-3): .easter (Easter.java), .symphony (Symphony.java) and //dropinfo's
// syntax arm (DropInfo.java) on a real Player with a real AionConnection (CommandTestSupport.h). The texts are the Java literals. The
// exchanges' ItemService.addItem is ItemService's (tests/itemsvc); //dropinfo on an npc runs in the gate (Z17, real drop data).

#include "CommandTestSupport.h"

#include <map>
#include <tuple>
#include <vector>
#include <string>

#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/handlers/admincommands/DropInfo.h"
#include "aion/gameserver/handlers/playercommands/Easter.h"
#include "aion/gameserver/handlers/playercommands/Symphony.h"
#include "aion/gameserver/utils/ChatUtil.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

class ExchangeCommandsTest : public CommandTest {
protected:
	void SetUp() override {
		CommandTest::SetUp();
		std::map<std::string, int8_t, std::less<>> levels(*configs::administration::CommandsConfig::ACCESS_LEVELS.get());
		levels["dropinfo"] = 5; // commands.properties
		levels["easter"] = 10;
		levels["symphony"] = 10;
		configs::administration::CommandsConfig::ACCESS_LEVELS.set(levels);
	}

	Player& gm(int32_t objectId) {
		Player& player = connected(objectId, "Warden", 10, 10); // membership 10: the player commands
		spawnInPoeta(player);
		return player;
	}

	static std::string item(int32_t itemId) { return utils::ChatUtil::item(itemId); }
};

/** Easter.java:46-80: the syntax built from the two lists, invalid ids, no eggs */
TEST_F(ExchangeCommandsTest, Easter) {
	Player& player = gm(730501);
	handlers::playercommands::Easter easter;
	// ChatCommand's syntax parsing colors the leading words; the Java lines follow them
	const std::string syntax = easter.getSyntaxInfo();
	EXPECT_NE(syntax.find("Type in .easter <ID> to get your reward:"), std::string::npos) << syntax;
	EXPECT_NE(syntax.find(" - (25 eggs) Random item"), std::string::npos) << syntax;
	size_t at = 0;
	for (auto [eggs, itemId, count] : std::vector<std::tuple<int32_t, int32_t, int32_t>>{{50, 186000147, 2}, {50, 186000055, 3},
			 {75, 166020000, 5}, {75, 188053609, 3}, {75, 166200013, 1}, {75, 188053113, 3}, {100, 188053295, 1}, {100, 166030005, 5},
			 {300, 188053702, 1}}) {
		at = syntax.find(" - (" + std::to_string(eggs) + " eggs) " + std::to_string(count) + "x " + item(itemId), at);
		EXPECT_NE(at, std::string::npos) << itemId << " in order: " << syntax;
	}
	EXPECT_TRUE(easter.process(player, args({})));
	EXPECT_EQ(client()->sentBytes(), info(easter.getSyntaxInfo()));
	for (const char* id : {"0", "11"}) {
		client()->clearSent();
		EXPECT_TRUE(easter.process(player, args({id})));
		EXPECT_EQ(client()->sentBytes(), info("Invalid reward ID.")) << id;
	}
	client()->clearSent();
	EXPECT_TRUE(easter.process(player, args({"10"})));
	EXPECT_EQ(client()->sentBytes(), info("You need 300 " + item(186000175) + " for this.")) << "the last reward";
	client()->clearSent();
	EXPECT_TRUE(easter.process(player, args({"1"})));
	EXPECT_EQ(client()->sentBytes(), info("You need 25 " + item(186000175) + " for this.")) << "a random reward: every one costs 25";
}

/** Symphony.java:41-80: the syntax, invalid ids, no copies */
TEST_F(ExchangeCommandsTest, Symphony) {
	Player& player = gm(730502);
	handlers::playercommands::Symphony symphony;
	EXPECT_NE(symphony.getSyntaxInfo().find(" - (3 copies): 10x " + item(186000236)), std::string::npos) << symphony.getSyntaxInfo();
	EXPECT_NE(symphony.getSyntaxInfo().find(" - (250 copies): 1x " + item(187000090)), std::string::npos) << symphony.getSyntaxInfo();
	EXPECT_TRUE(symphony.process(player, args({})));
	EXPECT_EQ(client()->sentBytes(), info(symphony.getSyntaxInfo()));
	for (const char* id : {"0", "16"}) {
		client()->clearSent();
		EXPECT_TRUE(symphony.process(player, args({id})));
		EXPECT_EQ(client()->sentBytes(), info("Invalid prize.")) << id;
	}
	client()->clearSent();
	EXPECT_TRUE(symphony.process(player, args({"15"})));
	EXPECT_EQ(client()->sentBytes(), info("You need 250 " + item(182007170) + " to buy this."));
	client()->clearSent();
	EXPECT_TRUE(symphony.process(player, args({"4"})));
	EXPECT_EQ(client()->sentBytes(), info("You need 40 " + item(182007170) + " to buy this."));
}

/** DropInfo.java:34-41: without an npc target, the syntax */
TEST_F(ExchangeCommandsTest, DropInfoWithoutNpc) {
	Player& player = gm(730503);
	handlers::admincommands::DropInfo dropInfo;
	EXPECT_TRUE(dropInfo.process(player, args({"all"})));
	EXPECT_EQ(client()->sentBytes(), info(dropInfo.getSyntaxInfo()));
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
