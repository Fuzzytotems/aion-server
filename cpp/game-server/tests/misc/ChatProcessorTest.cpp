// ChatProcessor (m5a-plan.md F-03): init over the command registry of the test executable (empty) and the handler directories,
// registerCommand's checks with the access levels of CommandsConfig, the case-insensitive lookup, getCommand<T>, and the parameter split of
// ChatProcessor.getParamsFromString (Java: params.trim().split(" +(?=[^\\]]*(\\[|$))")). Expectations derived by hand from ChatProcessor.java
// and ChatCommand.getLevel.

#include <gtest/gtest.h>

#include <map>
#include <string>
#include <vector>

#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/chathandlers/AdminCommand.h"
#include "aion/gameserver/utils/chathandlers/ChatProcessor.h"
#include "aion/gameserver/utils/chathandlers/PlayerCommand.h"

namespace aion::gameserver::utils::chathandlers {

struct ChatProcessorTestAccess {
	static std::vector<std::string> params(std::string_view text) { return ChatProcessor::getInstance().getParamsFromString(text); }
	static void clear() { ChatProcessor::getInstance().commandHandlers.clear(); }
};

namespace {

using Strings = std::vector<std::string>;
using configs::administration::CommandsConfig;

class TestAdminCommand final : public AdminCommand {
public:
	explicit TestAdminCommand(std::string_view alias) : AdminCommand(alias, "a test command") {}

	void execute(model::gameobjects::player::Player&, std::span<const std::string>) override {}
};

class OtherAdminCommand final : public AdminCommand {
public:
	OtherAdminCommand() : AdminCommand("Other") {}

	void execute(model::gameobjects::player::Player&, std::span<const std::string>) override {}
};

class ChatProcessorTest : public testing::Test {
protected:
	void SetUp() override {
		accessLevels = *CommandsConfig::ACCESS_LEVELS.get();
		ChatProcessorTestAccess::clear();
	}

	void TearDown() override {
		CommandsConfig::ACCESS_LEVELS.set(accessLevels);
		ChatProcessorTestAccess::clear();
	}

	std::map<std::string, int8_t, std::less<>> accessLevels;
};

TEST_F(ChatProcessorTest, InitWithAnEmptyRegistryLoadsNoCommands) {
	ChatProcessor& processor = ChatProcessor::getInstance();
	processor.init();
	EXPECT_TRUE(processor.getCommandList().empty());
	EXPECT_FALSE(processor.isCommandExists("//add"));
}

TEST_F(ChatProcessorTest, RegisterCommandChecksTheLevelAndDuplicates) {
	ChatProcessor& processor = ChatProcessor::getInstance();
	// commands are never freed (RT-11): leaked like the registry's commands
	auto* test = new TestAdminCommand("Test");
	auto* other = new OtherAdminCommand();
	auto* duplicate = new TestAdminCommand("test");
	auto* invalid = new TestAdminCommand("invalid");
	auto* missing = new TestAdminCommand("missing");
	CommandsConfig::ACCESS_LEVELS.set({{"Test", 3}, {"test", 3}, {"Other", 0}, {"invalid", -1}});

	processor.registerCommand(*test);
	processor.registerCommand(*other);
	EXPECT_TRUE(processor.isCommandExists("//test"));
	EXPECT_TRUE(processor.isCommandExists("//TEST"));
	EXPECT_FALSE(processor.isCommandExists("test"));
	EXPECT_EQ(processor.getCommandList().size(), 2u);
	EXPECT_EQ(processor.getCommand<OtherAdminCommand>(), other);
	EXPECT_EQ(processor.getCommand<PlayerCommand>(), nullptr); // exact class only

	try {
		processor.registerCommand(*duplicate);
		ADD_FAILURE() << "duplicate alias registered";
	} catch (const runtime::IllegalArgumentException& e) {
		EXPECT_EQ(std::string(e.what()), "Failed to register TestAdminCommand: //test is already registered.");
	}
	try {
		processor.registerCommand(*invalid);
		ADD_FAILURE() << "negative level registered";
	} catch (const runtime::IllegalArgumentException& e) {
		EXPECT_EQ(std::string(e.what()), "Failed to register TestAdminCommand: Invalid access level for //invalid.");
	}
	try {
		processor.registerCommand(*missing);
		ADD_FAILURE() << "missing level registered";
	} catch (const runtime::NullPointerException& e) {
		EXPECT_EQ(std::string(e.what()), "Missing access level for //missing");
	}
	EXPECT_EQ(processor.getCommandList().size(), 2u);
}

TEST_F(ChatProcessorTest, ParametersSplitOnSpacesOutsideOfSquareBrackets) {
	EXPECT_EQ(ChatProcessorTestAccess::params(""), Strings{});
	EXPECT_EQ(ChatProcessorTestAccess::params("   "), Strings{});
	EXPECT_EQ(ChatProcessorTestAccess::params(" add 100 "), (Strings{"add", "100"}));
	EXPECT_EQ(ChatProcessorTestAccess::params("a   b"), (Strings{"a", "b"}));
	// an item link keeps its spaces
	EXPECT_EQ(ChatProcessorTestAccess::params("give [item:1000;Sword of Doom] 5"), (Strings{"give", "[item:1000;Sword of Doom]", "5"}));
	// spaces followed by a ']' before any '[' do not split
	EXPECT_EQ(ChatProcessorTestAccess::params("a b] c"), (Strings{"a b]", "c"}));
	EXPECT_EQ(ChatProcessorTestAccess::params("[a b] [c d]"), (Strings{"[a b]", "[c d]"}));
}

} // namespace
} // namespace aion::gameserver::utils::chathandlers
