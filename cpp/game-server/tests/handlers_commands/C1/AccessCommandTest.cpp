// M5j stage 4 CP6 (m5j-plan.md §18.5, the orphaned commands of M5c): //access (Access.java) on a real Player with a real AionConnection
// (CommandTestSupport.h). The texts are the Java literals. The add/remove/removeall arms load the player's common data and write
// CommandsAccessDAO (a database); the cases cover the syntax and the in-memory "level" arm.

#include "CommandTestSupport.h"

#include <map>
#include <string>

#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/handlers/admincommands/Access.h"
#include "aion/gameserver/model/account/Account.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

class AccessCommandTest : public CommandTest {
protected:
	void SetUp() override {
		CommandTest::SetUp();
		std::map<std::string, int8_t, std::less<>> levels(*configs::administration::CommandsConfig::ACCESS_LEVELS.get());
		levels["access"] = 9; // commands.properties
		configs::administration::CommandsConfig::ACCESS_LEVELS.set(levels);
	}

	Player& gm(int32_t objectId) {
		Player& player = connected(objectId, "Warden", 9);
		spawnInPoeta(player);
		return player;
	}

	/** whether the client got the sendInfo text (among other packets) */
	bool got(std::string_view text) {
		const std::vector<std::vector<uint8_t>> wanted = info(text);
		for (const std::vector<uint8_t>& packet : client()->sentBytes())
			if (packet == wanted.front())
				return true;
		return false;
	}
};

/** Access.java:31-38, :40-44, :107-108: the syntax */
TEST_F(AccessCommandTest, Syntax) {
	Player& admin = gm(730701);
	handlers::admincommands::Access access;
	for (std::vector<std::string> params : {args({}), args({"level"}), args({"add", "warden"}), args({"grant", "x"})}) {
		client()->clearSent();
		EXPECT_TRUE(access.process(admin, params));
		EXPECT_EQ(client()->sentBytes(), info(access.getSyntaxInfo())) << params.size();
	}
}

/** Access.java:69-105: level <n>, level reset */
TEST_F(AccessCommandTest, Level) {
	Player& admin = gm(730702);
	handlers::admincommands::Access access;
	EXPECT_TRUE(access.process(admin, args({"level", "reset"})));
	EXPECT_EQ(client()->sentBytes(), info("Nothing to reset."));
	client()->clearSent();
	EXPECT_TRUE(access.process(admin, args({"level", "10"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid access level.")) << "above the level held";
	client()->clearSent();
	EXPECT_TRUE(access.process(admin, args({"level", "-1"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid access level."));
	client()->clearSent();
	EXPECT_TRUE(access.process(admin, args({"level", "9"})));
	EXPECT_EQ(client()->sentBytes(), info("You already have access level 9."));
	client()->clearSent();
	EXPECT_TRUE(access.process(admin, args({"level", "300"})));
	EXPECT_EQ(admin.getAccount()->getAccessLevel(), 9) << "Byte.parseByte's NumberFormatException";
	client()->clearSent();
	EXPECT_TRUE(access.process(admin, args({"LEVEL", "5"})));
	EXPECT_EQ(admin.getAccount()->getAccessLevel(), 5);
	EXPECT_TRUE(got("Your access level has been changed."));
	// below //access's level 9 the admin keeps the command only through CommandsAccessService.giveTemporaryAccess, which needs the command
	// registered in ChatProcessor (not in this fixture: "There is no such admin command"); the arm itself runs through execute
	client()->clearSent();
	access.execute(admin, args({"level", "7"}));
	EXPECT_EQ(admin.getAccount()->getAccessLevel(), 7) << "the old level 9 is still the maximum";
	client()->clearSent();
	access.execute(admin, args({"level", "reset"}));
	EXPECT_EQ(admin.getAccount()->getAccessLevel(), 9);
	EXPECT_TRUE(got("Your access level has been reset."));
	client()->clearSent();
	EXPECT_TRUE(access.process(admin, args({"level", "reset"})));
	EXPECT_EQ(client()->sentBytes(), info("Nothing to reset.")) << "the old level is forgotten";
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
