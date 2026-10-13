// M5j stage 4 CP7 (m5j-plan.md §18.5 item 7, L-04): the command smoke run at unit level. Every registered admin, console and player command
// (CommandSmokeList.inc, generated from the AION_*_COMMAND markers; TheListMatchesTheMarkers keeps it in step) is created through its marker's
// factory and run on a real Player with a real AionConnection (CommandTestSupport.h) in a Poeta map instance:
//
// - the access check with commands.properties' levels (read from the Java tree): one level (or membership) below the command's, a staff member
//   is answered "<You need access level N or higher to use X>" and a regular player gets nothing, the chat text passing through
//   (AdminCommand/ConsoleCommand/PlayerCommand.process);
// - with access: "help" (ChatCommand.run's description and syntax), no parameters and an unknown parameter. The command may answer anything,
//   including Java's "<Error while executing command>" for a service this executable has not got (the database, static data), but it must not
//   crash the process and must not reach an AION_UNPORTED site (unportedHitCount), except the sites listed in EXPECTED_UNPORTED.
//
// ctest runs every command in its own process, so a crash fails one case. The CRT reports of this process go to stderr instead of a dialog.

#include "CommandTestSupport.h"

#include <crtdbg.h>
#include <windows.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/handlers/admincommands/GoTo.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/utils/chathandlers/AdminCommand.h"
#include "aion/gameserver/utils/chathandlers/ChatCommand.h"
#include "aion/gameserver/utils/chathandlers/ConsoleCommand.h"
#include "aion/gameserver/utils/chathandlers/ChatProcessor.h"
#include "aion/gameserver/utils/chathandlers/PlayerCommand.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {

struct SmokeCommand {
	const char* javaClass; // e.g. "admincommands.Add"
	::aion::gameserver::handlers::CommandFactory* create;
};

} // namespace aion::gameserver::network::aion::clientpackets::testing

#include "CommandSmokeList.inc"

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

/**
 * The AION_UNPORTED sites a command may reach with "help", no parameters or an unknown parameter: javaClass -> why. Empty: no command reaches
 * one with these forms.
 */
const std::map<std::string_view, std::string_view>& expectedUnported() {
	static const auto* const expected = new std::map<std::string_view, std::string_view>{};
	return *expected;
}

/** cpp/ of this source tree (this file is cpp/game-server/tests/handlers_commands/C1/CommandSmokeTest.cpp) */
std::filesystem::path cppRoot() {
	return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path().parent_path();
}

/** game-server/config/administration/commands.properties of the Java tree: alias -> level */
std::map<std::string, int8_t, std::less<>> javaCommandLevels() {
	std::map<std::string, int8_t, std::less<>> levels;
	std::ifstream in(cppRoot().parent_path() / "game-server" / "config" / "administration" / "commands.properties");
	std::string line;
	while (std::getline(in, line)) {
		std::string_view text = commons::utils::StringUtils::trim(line);
		if (text.empty() || text.front() == '#')
			continue;
		size_t eq = text.find('=');
		if (eq == std::string_view::npos || text.starts_with("gameserver.")) // the alias levels only (not handler_directories)
			continue;
		levels[std::string(commons::utils::StringUtils::trim(text.substr(0, eq)))] =
			static_cast<int8_t>(std::stoi(std::string(commons::utils::StringUtils::trim(text.substr(eq + 1)))));
	}
	return levels;
}

/** No CRT dialog in this process: assertions and invalid parameters are reported on stderr and end the process */
void noDialogs() {
	static const bool done = [] {
		_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
		_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
		_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
		_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
		_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
		SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
		return true;
	}();
	static_cast<void>(done);
}

std::string testName(const ::testing::TestParamInfo<SmokeCommand>& info) {
	std::string name = info.param.javaClass;
	for (char& c : name)
		if (c == '.')
			c = '_';
	return name;
}

class CommandSmokeTest : public CommandTest, public ::testing::WithParamInterface<SmokeCommand> {
protected:
	void SetUp() override {
		noDialogs();
		CommandTest::SetUp();
		std::map<std::string, int8_t, std::less<>> levels(*configs::administration::CommandsConfig::ACCESS_LEVELS.get());
		for (const auto& [alias, level] : javaCommandLevels())
			levels[alias] = level;
		configs::administration::CommandsConfig::ACCESS_LEVELS.set(levels);
		// what the server has before any command runs: the item templates (//megaphone's colors read them; none here) and ChatProcessor's
		// commands that others delegate to (///teleportto's syntax is //goto's)
		if (!dataholders::DataManager::ITEM_DATA) {
			dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(context, "<item_templates/>"));
			itemsPublished = true;
		}
		if (!utils::chathandlers::ChatProcessor::getInstance().isCommandExists("//goto"))
			utils::chathandlers::ChatProcessor::getInstance().registerCommand(*new handlers::admincommands::GoTo()); // immortal, like init's
	}

	void TearDown() override {
		for (Player* player : stored)
			world::World::getInstance().removeObject(*player);
		stored.clear();
		CommandTest::TearDown();
		if (itemsPublished)
			dataholders::DataManager::ITEM_DATA.resetForTests();
	}

	Player& online(int32_t objectId, std::string_view name, int8_t accessLevel, int8_t membership) {
		Player& player = connected(objectId, name, accessLevel, membership);
		spawnInPoeta(player);
		world::World::getInstance().storeObject(player);
		stored.push_back(&player);
		return player;
	}

	std::vector<Player*> stored;
	xml::LoadContext context;
	bool itemsPublished = false;
};

// The list is the markers': a command added or removed without regenerating CommandSmokeList.inc fails here
TEST(CommandSmokeListTest, TheListMatchesTheMarkers) {
	std::set<std::string> markers;
	const std::filesystem::path handlers = cppRoot() / "game-server" / "handlers" / "aion" / "gameserver" / "handlers";
	const std::regex marker(R"(^AION_(ADMIN|PLAYER|CONSOLE)_COMMAND\((\w+)\);)");
	for (const char* pkg : {"admincommands", "playercommands", "consolecommands"}) {
		for (const auto& file : std::filesystem::directory_iterator(handlers / pkg)) {
			if (file.path().extension() != ".cpp")
				continue;
			std::ifstream in(file.path());
			std::string line;
			while (std::getline(in, line)) {
				std::smatch m;
				if (std::regex_search(line, m, marker))
					markers.insert(std::string(pkg) + "." + m[2].str());
			}
		}
	}
	std::set<std::string> listed;
	for (const SmokeCommand& command : SMOKE_COMMANDS)
		listed.insert(command.javaClass);
	EXPECT_EQ(listed, markers);
	EXPECT_GE(listed.size(), 120u);
}

TEST_P(CommandSmokeTest, AccessHelpNoParametersAndAnUnknownParameter) {
	const SmokeCommand& entry = GetParam();
	const std::string javaClass = entry.javaClass;
	std::unique_ptr<utils::chathandlers::ChatCommand> command = entry.create();
	ASSERT_NE(command, nullptr);
	const bool playerCommand = dynamic_cast<utils::chathandlers::PlayerCommand*>(command.get()) != nullptr;
	const int8_t level = command->getLevel(); // commands.properties has every alias (Java: an NPE at registration otherwise)
	const int32_t base = 732000 + static_cast<int32_t>(&entry - SMOKE_COMMANDS) * 4;

	// the access check, one below the command's level
	if (level > 0) {
		const int8_t below = static_cast<int8_t>(level - 1);
		Player& weak = playerCommand ? online(base + 1, "Weak", 0, below) : online(base + 1, "Weak", below, 0);
		const size_t index = clients.size() - 1;
		const bool staff = weak.isStaff();
		EXPECT_EQ(command->process(weak, args({"zzsmoke"})), staff) << javaClass << ": the chat text passes for a regular player";
		if (staff && !playerCommand)
			EXPECT_EQ(client(index)->sentBytes(),
				info("<You need access level " + std::to_string(level) + " or higher to use " + command->getAliasWithPrefix() + ">", index))
				<< javaClass;
		else if (!staff)
			EXPECT_TRUE(client(index)->sentBytes().empty()) << javaClass;
	}

	// with access: help, no parameters, an unknown parameter
	Player& gm = online(base + 2, "Warden", 9, 10);
	const size_t gmIndex = clients.size() - 1;
	client(gmIndex)->clearSent();
	EXPECT_TRUE(command->process(gm, args({"help"}))) << javaClass;
	EXPECT_FALSE(client(gmIndex)->sentBytes().empty()) << javaClass << ": ChatCommand.run's help";

	const uint64_t unportedBefore = runtime::unportedHitCount();
	for (std::vector<std::string> params : {args({}), args({"zzsmoke"})}) {
		client(gmIndex)->clearSent();
		command->process(gm, params); // any answer: Java's error text for a missing service included
	}
	const uint64_t reached = runtime::unportedHitCount() - unportedBefore;
	if (expectedUnported().contains(entry.javaClass))
		EXPECT_GT(reached, 0u) << javaClass << ": the listed site (" << expectedUnported().at(entry.javaClass) << ") is no longer reached";
	else
		EXPECT_EQ(reached, 0u) << javaClass << ": an AION_UNPORTED site was reached";
}

INSTANTIATE_TEST_SUITE_P(Registered, CommandSmokeTest, ::testing::ValuesIn(SMOKE_COMMANDS), testName);

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
