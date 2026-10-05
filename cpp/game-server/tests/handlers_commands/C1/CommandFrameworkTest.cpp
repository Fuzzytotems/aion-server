// The chat command framework (P5-14, m5j-plan.md K-01; ChatCommand.java:60-241, AdminCommand.java:37-58, PlayerCommand.java:28-44,
// ConsoleCommand.java:41-62) and the four commands of the default gameserver.administration.login.execute_commands (T0: //invis, //invul,
// //enemy, //see; data/handlers/admincommands), driven on a real Player with a real AionConnection (tests/cm_ak/InWorldPacketRunSupport.h,
// by relative path as tests/playersvc includes tests/instance's fixture).
//
// Expectations are written from the Java: what reaches the client is PacketSendUtility.sendMessage's SM_MESSAGE(0, null, text, GOLDEN_YELLOW)
// per ChatUtil.split part, compared with the server's serialization of that packet (as GMServiceLoginTest does); the texts are the Java
// literals. The access levels are set per case (CommandsConfig.ACCESS_LEVELS, the commands.properties map) and restored.

#include "CommandTestSupport.h"

#include <map>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/handlers/admincommands/Enemy.h"
#include "aion/gameserver/handlers/admincommands/Invis.h"
#include "aion/gameserver/handlers/admincommands/Invul.h"
#include "aion/gameserver/handlers/admincommands/See.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/gameobjects/player/CustomPlayerState.h"
#include "aion/gameserver/model/gameobjects/player/motion/MotionList.h"
#include "aion/gameserver/model/gameobjects/state/CreatureSeeState.h"
#include "aion/gameserver/model/gameobjects/state/CreatureVisualState.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketLookups.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/EnumValueOf.h"
#include "aion/gameserver/utils/chathandlers/AdminCommand.h"
#include "aion/gameserver/utils/chathandlers/ChatProcessor.h"
#include "aion/gameserver/utils/chathandlers/ConsoleCommand.h"
#include "aion/gameserver/utils/chathandlers/PlayerCommand.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using model::gameobjects::player::CustomPlayerState;
using model::gameobjects::state::CreatureVisualState;
using serverpackets::SM_SYSTEM_MESSAGE;

/** A command whose execute does what the case asks (throws, records) */
class ScriptedAdminCommand final : public utils::chathandlers::AdminCommand {
public:
	ScriptedAdminCommand() : AdminCommand("fwtest", "Framework test.", "<number> - Parses a number.") {}

	void execute(Player& /*player*/, std::span<const std::string> params) override {
		executed.assign(params.begin(), params.end());
		if (action)
			action();
	}

	std::function<void()> action;
	std::vector<std::string> executed;
};

class ScriptedPlayerCommand final : public utils::chathandlers::PlayerCommand {
public:
	ScriptedPlayerCommand() : PlayerCommand("fwplayer", "Player framework test.") {}

	void execute(Player& /*player*/, std::span<const std::string> params) override { executed.assign(params.begin(), params.end()); }

	std::vector<std::string> executed;
};

class ScriptedConsoleCommand final : public utils::chathandlers::ConsoleCommand {
public:
	ScriptedConsoleCommand() : ConsoleCommand("fwconsole") {}

	void execute(Player& /*player*/, std::span<const std::string> params) override { executed.assign(params.begin(), params.end()); }

	std::vector<std::string> executed;
};

class CommandFrameworkTest : public CommandTest {};


// ---- AdminCommand.validateAccess / process (AdminCommand.java:37-58) -------------------------------------------------------------------------

TEST_F(CommandFrameworkTest, ARegularPlayerWithoutAccessGetsFalseSoTheLineIsChat) {
	Player& player = connected(730001, "Plain", 0);
	ScriptedAdminCommand command;
	EXPECT_FALSE(command.process(player, args({"1"}))) << "return player.isStaff(): the chat sends the text";
	EXPECT_TRUE(command.executed.empty());
	EXPECT_TRUE(client()->sentBytes().empty()) << "no access message for a non-staff player";
}

TEST_F(CommandFrameworkTest, AStaffMemberBelowTheLevelIsToldTheLevel) {
	Player& gm = connected(730002, "Junior", 2);
	ScriptedAdminCommand command;
	EXPECT_TRUE(command.process(gm, args({"1"}))) << "a staff member's line is never chat";
	EXPECT_TRUE(command.executed.empty());
	EXPECT_EQ(client()->sentBytes(), info("<You need access level 3 or higher to use //fwtest>"));
}

TEST_F(CommandFrameworkTest, WithTheLevelTheCommandRunsAndIsAudited) {
	Player& gm = connected(730003, "Warden", 3);
	ScriptedAdminCommand command;
	network::test::LogCapture capture({"ADMINAUDIT_LOG"}, spdlog::level::info);
	EXPECT_TRUE(command.process(gm, args({"12", "x"})));
	EXPECT_EQ(command.executed, args({"12", "x"}));
	EXPECT_TRUE(client()->sentBytes().empty());
	EXPECT_TRUE(capture.contains("info|ADMINAUDIT_LOG|[Admin Command] > [Player: Warden]: //fwtest 12 x")) << capture.dump();
}

// ---- ChatCommand.run (ChatCommand.java:60-79) --------------------------------------------------------------------------------------------

TEST_F(CommandFrameworkTest, HelpSendsTheDescriptionAndTheSyntax) {
	Player& gm = connected(730004, "Warden", 3);
	ScriptedAdminCommand command;
	EXPECT_TRUE(command.process(gm, args({"HELP"}))) << "equalsIgnoreCase";
	EXPECT_TRUE(command.executed.empty()) << "help does not execute";
	EXPECT_EQ(client()->sentBytes(),
		info("Command: " + utils::ChatUtil::color("//fwtest", utils::JavaColor::WHITE) + "\n\tFramework test.\n" + command.getSyntaxInfo()));
}

TEST_F(CommandFrameworkTest, AnIllegalArgumentIsAnsweredWithItsMessage) {
	Player& gm = connected(730005, "Warden", 3);
	ScriptedAdminCommand command;
	command.action = [] { throw runtime::IllegalArgumentException("Bad target."); };
	EXPECT_TRUE(command.process(gm, args({})));
	EXPECT_EQ(client()->sentBytes(), info("Bad target."));
}

TEST_F(CommandFrameworkTest, AnIllegalArgumentWithoutMessageSendsTheSyntax) {
	Player& gm = connected(730006, "Warden", 3);
	ScriptedAdminCommand command;
	command.action = [] { throw runtime::IllegalArgumentException(""); }; // Java: new IllegalArgumentException() - a null message
	EXPECT_TRUE(command.process(gm, args({})));
	EXPECT_EQ(client()->sentBytes(), info(command.getSyntaxInfo()));
}

TEST_F(CommandFrameworkTest, ANumberFormatErrorIsAnInvalidNumber) {
	Player& gm = connected(730007, "Warden", 3);
	ScriptedAdminCommand command;
	command.action = [] { throw commons::utils::NumberFormatException("For input string: \"abc\""); };
	EXPECT_TRUE(command.process(gm, args({"abc"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid number: \"abc\""));
	client()->clearSent();
	command.action = [] { throw commons::utils::NumberFormatException("Cannot parse null string: null"); };
	EXPECT_TRUE(command.process(gm, args({})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid number."));
	client()->clearSent();
	command.action = [] { throw commons::utils::NumberFormatException(""); };
	EXPECT_TRUE(command.process(gm, args({})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid number.")) << "no message (Java null or \"\") is still a number error, not the syntax";
}

TEST_F(CommandFrameworkTest, AnUnknownEnumConstantListsThePossibleValues) {
	Player& gm = connected(730008, "Warden", 3);
	ScriptedAdminCommand command;
	command.action = [] { utils::enumValueOf<model::Race>("ELVES"); };
	EXPECT_TRUE(command.process(gm, args({"ELVES"})));
	std::string values;
	for (std::string_view name : xml::EnumTraits<model::Race>::names)
		values += (values.empty() ? "" : ", ") + std::string(name);
	EXPECT_EQ(client()->sentBytes(), info("Invalid race.\nPossible values:\n" + values)) << "\"Race\" split at its case changes, lower-cased";
}

TEST_F(CommandFrameworkTest, AnyOtherExceptionIsLoggedAndAnsweredWithTheErrorLine) {
	Player& gm = connected(730009, "Warden", 3);
	ScriptedAdminCommand command;
	command.action = [] { throw std::runtime_error("boom"); };
	network::test::LogCapture capture({"com.aionemu.gameserver.utils.chathandlers.ChatCommand"}, spdlog::level::info);
	EXPECT_TRUE(command.process(gm, args({"a", "b"})));
	EXPECT_EQ(client()->sentBytes(), info("<Error while executing command>")) << "run answered false";
	EXPECT_TRUE(capture.contains("Exception executing chat command \"//fwtest a b\" - Player: Warden, Target: null")) << capture.dump();
}

// ---- PlayerCommand / ConsoleCommand -----------------------------------------------------------------------------------------------------

TEST_F(CommandFrameworkTest, APlayerCommandChecksTheMembershipNotTheAccessLevel) {
	Player& member = connected(730010, "Member", 0, 2);
	ScriptedPlayerCommand command;
	EXPECT_TRUE(command.process(member, args({"x"})));
	EXPECT_EQ(command.executed, args({"x"}));

	Player& staff = connected(730011, "Staff", 5, 0);
	ScriptedPlayerCommand denied;
	EXPECT_TRUE(denied.process(staff, args({"x"}))) << "a staff member's line is never chat";
	EXPECT_TRUE(denied.executed.empty()) << "hasPermission(2) answers the membership, 0 here";
	EXPECT_EQ(client(1)->sentBytes(), info("<You need membership level 2 or higher to use .fwplayer>", 1));
}

TEST_F(CommandFrameworkTest, AConsoleCommandIsAuditedAsAConsoleCommand) {
	Player& gm = connected(730012, "Warden", 3);
	ScriptedConsoleCommand command;
	network::test::LogCapture capture({"ADMINAUDIT_LOG"}, spdlog::level::info);
	EXPECT_TRUE(command.process(gm, args({"1"})));
	EXPECT_EQ(command.executed, args({"1"}));
	EXPECT_TRUE(capture.contains("info|ADMINAUDIT_LOG|[Console Command] > [Player: Warden]: fwconsole 1")) << capture.dump();
}

// ---- T0: the login commands --------------------------------------------------------------------------------------------------------------

TEST_F(CommandFrameworkTest, InvisTogglesTheHideAbnormalAndVisualState) {
	Player& gm = connected(730020, "Warden", 3);
	handlers::admincommands::Invis invis;
	EXPECT_TRUE(invis.process(gm, args({})));
	EXPECT_TRUE(gm.isInVisualState(CreatureVisualState::HIDE20));
	EXPECT_TRUE(gm.getEffectController()->isAbnormalSet(skillengine::effect::AbnormalState::HIDE));
	const std::vector<std::vector<uint8_t>> on = client()->sentBytes();
	ASSERT_EQ(on.size(), 3u) << "STR_SKILL_EFFECT_INVISIBLE_BEGIN, SM_PLAYER_STATE (to himself), SM_ABNORMAL_STATE";
	EXPECT_EQ(on[0], serialized(SM_SYSTEM_MESSAGE::STR_SKILL_EFFECT_INVISIBLE_BEGIN(), client().con()));

	client()->clearSent();
	EXPECT_TRUE(invis.process(gm, args({})));
	EXPECT_FALSE(gm.isInVisualState(CreatureVisualState::HIDE20));
	EXPECT_FALSE(gm.getEffectController()->isAbnormalSet(skillengine::effect::AbnormalState::HIDE));
	ASSERT_EQ(client()->sentBytes().size(), 3u);
	EXPECT_EQ(client()->sentBytes()[0], serialized(SM_SYSTEM_MESSAGE::STR_SKILL_EFFECT_INVISIBLE_END(), client().con()));
}

TEST_F(CommandFrameworkTest, InvulTogglesTheInvulnerableState) {
	Player& gm = connected(730021, "Warden", 3);
	handlers::admincommands::Invul invul;
	EXPECT_TRUE(invul.process(gm, args({})));
	EXPECT_TRUE(gm.isInvulnerable());
	EXPECT_EQ(client()->sentBytes(), info(utils::ChatUtil::l10n(293440)));
	client()->clearSent();
	EXPECT_TRUE(invul.process(gm, args({})));
	EXPECT_FALSE(gm.isInvulnerable());
	EXPECT_EQ(client()->sentBytes(), exactly({message("You are now mortal.")})) << "sendMessage, not sendInfo";
}

TEST_F(CommandFrameworkTest, SeeTogglesSearch20) {
	Player& gm = connected(730022, "Warden", 3);
	handlers::admincommands::See see;
	EXPECT_TRUE(see.process(gm, args({})));
	EXPECT_EQ(gm.getSeeState(), 20) << "CreatureSeeState.SEARCH20's id";
	ASSERT_FALSE(client()->sentBytes().empty());
	EXPECT_EQ(client()->sentBytes()[0], info(utils::ChatUtil::l10n(288645))[0]);
	client()->clearSent();
	EXPECT_TRUE(see.process(gm, args({})));
	EXPECT_LT(gm.getSeeState(), 2);
	ASSERT_FALSE(client()->sentBytes().empty());
	EXPECT_EQ(client()->sentBytes()[0], message("You lost vision."));
}

TEST_F(CommandFrameworkTest, EnemyWalksItsArms) {
	Player& gm = connected(730023, "Warden", 3);
	handlers::admincommands::Enemy enemy;
	EXPECT_TRUE(enemy.process(gm, args({"none"})));
	EXPECT_TRUE(gm.isInCustomState(CustomPlayerState::NEUTRAL_TO_EVERYONE));
	EXPECT_EQ(firstSent(), info("You are now neutral to everyone.")[0]);

	client()->clearSent();
	EXPECT_TRUE(enemy.process(gm, args({"ALL"})));
	EXPECT_FALSE(gm.isInCustomState(CustomPlayerState::NEUTRAL_TO_EVERYONE));
	EXPECT_TRUE(gm.isInCustomState(CustomPlayerState::ENEMY_OF_EVERYONE));
	EXPECT_EQ(firstSent(), info("You are now an enemy to all.")[0]);

	client()->clearSent();
	EXPECT_TRUE(enemy.process(gm, args({"all", "npcs"})));
	EXPECT_FALSE(gm.isInCustomState(CustomPlayerState::ENEMY_OF_EVERYONE));
	EXPECT_TRUE(gm.isInCustomState(CustomPlayerState::ENEMY_OF_ALL_NPCS));

	client()->clearSent();
	EXPECT_TRUE(enemy.process(gm, args({"none", "players"})));
	EXPECT_TRUE(gm.isInCustomState(CustomPlayerState::NEUTRAL_TO_ALL_PLAYERS));
	EXPECT_EQ(firstSent(), info("You are now neutral to all players.")[0]);

	client()->clearSent();
	EXPECT_TRUE(enemy.process(gm, args({"cancel"})));
	EXPECT_EQ(firstSent(), info("You appear regular to everyone again.")[0]);

	client()->clearSent();
	EXPECT_TRUE(enemy.process(gm, args({"sideways"})));
	EXPECT_EQ(client()->sentBytes(), info(enemy.getSyntaxInfo())) << "an unknown arm sends the syntax and returns before onChangedPlayerAttributes";
}

// ---- ChatProcessor dispatch (ChatProcessor.java:58-70 -> process) -----------------------------------------------------------------------

TEST_F(CommandFrameworkTest, AChatLineReachesARegisteredCommand) {
	Player& gm = connected(730030, "Warden", 3);
	static auto* registered = [] {
		auto* command = new ScriptedAdminCommand(); // immortal, like ChatProcessor.init's commands
		utils::chathandlers::ChatProcessor::getInstance().registerCommand(*command);
		return command;
	}();
	registered->executed.clear();
	EXPECT_TRUE(utils::chathandlers::ChatProcessor::getInstance().handleChatCommand(gm, "//FWTEST 1  [item: 100;x y]"));
	EXPECT_EQ(registered->executed, args({"1", "[item: 100;x y]"})) << "case-insensitive alias, the bracket kept whole";
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
