// The stage-0 player commands (m5j-plan.md §5.2): .help, .id, .gmlist (data/handlers/playercommands), on a real Player with a real
// AionConnection (CommandTestSupport.h). The texts are the Java literals.

#include "CommandTestSupport.h"

#include <map>
#include <string>
#include <vector>

#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/handlers/playercommands/GmList.h"
#include "aion/gameserver/handlers/playercommands/Help.h"
#include "aion/gameserver/handlers/playercommands/Id.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/JavaColor.h"
#include "aion/gameserver/utils/chathandlers/ChatProcessor.h"
#include "aion/gameserver/utils/chathandlers/PlayerCommand.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using serverpackets::SM_SYSTEM_MESSAGE;
using utils::ChatUtil;
using utils::JavaColor;

/** a player command of membership level 2, registered once for the process (ChatProcessor keeps its commands) */
class ZzPlayerCommand final : public utils::chathandlers::PlayerCommand {
public:
	ZzPlayerCommand() : PlayerCommand("zzplayer", "A test command.") {}
	void execute(Player& /*player*/, std::span<const std::string> /*params*/) override {}
};

class PlayerCommandsTest : public CommandTest {
protected:
	void SetUp() override {
		CommandTest::SetUp();
		std::map<std::string, int8_t, std::less<>> levels(*configs::administration::CommandsConfig::ACCESS_LEVELS.get());
		for (const char* alias : {"help", "id", "gmlist"}) // commands.properties: the player commands every player may use
			levels[alias] = 0;
		levels["zzplayer"] = 2;
		configs::administration::CommandsConfig::ACCESS_LEVELS.set(levels);
	}
};

// ---- .help (Help.java:28-55) --------------------------------------------------------------------------------------------------------------

TEST_F(PlayerCommandsTest, HelpListsTheAllowedCommandsSortedByAlias) {
	static handlers::playercommands::Help* const help = [] {
		auto* command = new handlers::playercommands::Help(); // immortal, like ChatProcessor.init's commands
		utils::chathandlers::ChatProcessor::getInstance().registerCommand(*command);
		utils::chathandlers::ChatProcessor::getInstance().registerCommand(*new ZzPlayerCommand());
		return command;
	}();

	Player& plain = connected(730400, "Plain", 0, 0);
	EXPECT_TRUE(help->process(plain, args({})));
	EXPECT_EQ(client()->sentBytes(),
		info("You are not allowed to use any chat commands other than " + ChatUtil::color(".help", JavaColor::WHITE) + "."))
		<< "only .help itself (the admin commands of other cases need access level 3)";

	Player& member = connected(730401, "Member", 0, 2);
	EXPECT_TRUE(help->process(member, args({})));
	EXPECT_EQ(client(1)->sentBytes(),
		info("List of available commands (2):\n\t" + ChatUtil::color(".help", JavaColor::WHITE) + " - Lists all commands you are allowed to use.\n\t" +
				 ChatUtil::color(".zzplayer", JavaColor::WHITE) + " - A test command.\nType <" + ChatUtil::color("command", JavaColor::WHITE) + "> " +
				 ChatUtil::color("help", JavaColor::WHITE) + " to get further information about a command.",
			1));
}

// ---- .id (Id.java:29-84) ------------------------------------------------------------------------------------------------------------------

TEST_F(PlayerCommandsTest, IdOfTheTarget) {
	Player& plain = connected(730402, "Plain", 0);
	handlers::playercommands::Id id;
	EXPECT_TRUE(id.process(plain, args({})));
	EXPECT_EQ(client()->sentBytes(), info(id.getSyntaxInfo())) << "no target";

	plain.setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(&plain));
	client()->clearSent();
	EXPECT_TRUE(id.process(plain, args({})));
	EXPECT_EQ(client()->sentBytes(), exactly({serialized(SM_SYSTEM_MESSAGE::STR_INVALID_TARGET(), client().con())}))
		<< "a regular player sees IDs of npcs and gatherables only";

	Player& gm = connected(730403, "Warden", 3);
	gm.setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(&gm));
	client(1)->clearSent();
	EXPECT_TRUE(id.process(gm, args({})));
	EXPECT_EQ(client(1)->sentBytes(), info("TestPlayer: " + ChatUtil::path(gm, true) + " (Object ID: 730403)", 1))
		<< "staff: any target, with the object ID (Java getSimpleName: the fixture's Player subclass)";

	client(1)->clearSent();
	EXPECT_TRUE(id.process(gm, args({"nothing"})));
	EXPECT_EQ(client(1)->sentBytes(), info(id.getSyntaxInfo(), 1)) << "neither an item nor a quest";
	plain.setTarget(nullptr);
	gm.setTarget(nullptr);
}

// ---- .gmlist (GmList.java:22-35) ----------------------------------------------------------------------------------------------------------

TEST_F(PlayerCommandsTest, GmListWithoutStaffOnline) {
	Player& plain = connected(730404, "Plain", 0);
	handlers::playercommands::GmList gmList;
	EXPECT_TRUE(gmList.process(plain, args({})));
	EXPECT_EQ(client()->sentBytes(), info("There is no GM online."));
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
