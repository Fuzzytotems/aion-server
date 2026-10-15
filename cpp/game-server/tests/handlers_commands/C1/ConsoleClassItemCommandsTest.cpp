// M5j stage 4 CP6 (m5j-plan.md §18.5, the orphaned console commands of M5c and M5e): ///setinventorygrowth, ///set_enchantcount,
// ///changeclass and ///classup on a real Player with a real AionConnection (CommandTestSupport.h). The texts are the Java literals. The
// services they end in (CubeExpandService, EnchantService, ClassChangeService) have their own tests; the cases cover the commands' parsing,
// their checks and their answers.

#include "CommandTestSupport.h"

#include <map>
#include <string>

#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/handlers/consolecommands/Changeclass.h"
#include "aion/gameserver/handlers/consolecommands/Classup.h"
#include "aion/gameserver/handlers/consolecommands/Set_enchantcount.h"
#include "aion/gameserver/handlers/consolecommands/Setinventorygrowth.h"
#include "aion/gameserver/model/PlayerClass.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using model::PlayerClass;

class ConsoleClassItemCommandsTest : public CommandTest {
protected:
	void SetUp() override {
		CommandTest::SetUp();
		std::map<std::string, int8_t, std::less<>> levels(*configs::administration::CommandsConfig::ACCESS_LEVELS.get());
		for (const char* alias : {"setinventorygrowth", "set_enchantcount", "changeclass", "classup"})
			levels[alias] = 9; // commands.properties
		configs::administration::CommandsConfig::ACCESS_LEVELS.set(levels);
	}

	Player& gm(int32_t objectId) {
		Player& player = connected(objectId, "Warden", 9);
		spawnInPoeta(player);
		return player;
	}
};

/** Changeclass.java:31-39: the client's four old names, then PlayerClass.valueOf of the upper case name */
TEST_F(ConsoleClassItemCommandsTest, ParsePlayerClass) {
	using handlers::consolecommands::Changeclass;
	EXPECT_EQ(Changeclass::parsePlayerClass("fighter"), PlayerClass::GLADIATOR);
	EXPECT_EQ(Changeclass::parsePlayerClass("Knight"), PlayerClass::TEMPLAR);
	EXPECT_EQ(Changeclass::parsePlayerClass("WIZARD"), PlayerClass::SORCERER);
	EXPECT_EQ(Changeclass::parsePlayerClass("elementalist"), PlayerClass::SPIRIT_MASTER);
	EXPECT_EQ(Changeclass::parsePlayerClass("chanter"), PlayerClass::CHANTER);
	EXPECT_THROW(Changeclass::parsePlayerClass("bard_x"), runtime::IllegalArgumentException) << "Enum.valueOf";
}

/** Changeclass.java:19-29, Classup.java:19-29: the syntax; an unknown class is ChatCommand's error text */
TEST_F(ConsoleClassItemCommandsTest, ClassCommandsSyntax) {
	Player& admin = gm(730601);
	handlers::consolecommands::Changeclass changeclass;
	handlers::consolecommands::Classup classup;
	EXPECT_TRUE(changeclass.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), info(changeclass.getSyntaxInfo()));
	client()->clearSent();
	EXPECT_TRUE(classup.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), info(classup.getSyntaxInfo()));
	client()->clearSent();
	const PlayerClass before = admin.getPlayerClass();
	EXPECT_TRUE(classup.process(admin, args({"nobody"})));
	const std::vector<std::vector<uint8_t>> sent = client()->sentBytes();
	ASSERT_FALSE(sent.empty()) << "ChatCommand's answer to PlayerClass.valueOf's IllegalArgumentException (ChatCommand.java:155-182)";
	EXPECT_NE(sent, info(classup.getSyntaxInfo()));
	EXPECT_EQ(admin.getPlayerClass(), before) << "unchanged";
}

/** Setinventorygrowth.java:17-40 */
TEST_F(ConsoleClassItemCommandsTest, Setinventorygrowth) {
	Player& admin = gm(730602);
	handlers::consolecommands::Setinventorygrowth command;
	EXPECT_TRUE(command.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("syntax ///setinventorygrowth <?>")}));
}

/** Set_enchantcount.java:22-82: the syntax, words, an unknown item */
TEST_F(ConsoleClassItemCommandsTest, Set_enchantcount) {
	Player& admin = gm(730603);
	handlers::consolecommands::Set_enchantcount command;
	EXPECT_TRUE(command.process(admin, args({"1"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("syntax ///set_enchantcount <item Id> <enchantcount> <unk>")}));
	client()->clearSent();
	EXPECT_TRUE(command.process(admin, args({"1", "x"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Invalid params.")}));
	client()->clearSent();
	EXPECT_TRUE(command.process(admin, args({"123456", "1"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Invalid item.")})) << "neither in the inventory nor equipped";
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
