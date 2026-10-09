// The stage-1 administrative and social commands (m5j-plan.md §5.4, §18.1 stage 1 CP5, item S-10): //ban, //banchar, //unban, //unbanchar,
// //banip, //unbanip, //banmac, //unbanmac, //banhdd, //sprison, //rprison, //ranking, //grant, //passkeyreset, //stoken, //announcements,
// //playerinfo, .faction and .advent, on real Players with real AionConnections (CommandTestSupport.h). The login server link is not up
// (LoginServer.sendPacket answers false) and this executable has no database, so the cases assert the texts, the arms and the in-memory
// effects; the gate's Z7 drives //sprison and //rprison, Z8 //ranking. The texts are the Java literals.

#include "CommandTestSupport.h"

#include <map>
#include <string>
#include <vector>

#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/EventsConfig.h"
#include "aion/gameserver/handlers/admincommands/Announcements.h"
#include "aion/gameserver/handlers/admincommands/Ban.h"
#include "aion/gameserver/handlers/admincommands/BanChar.h"
#include "aion/gameserver/handlers/admincommands/BanHdd.h"
#include "aion/gameserver/handlers/admincommands/BanIp.h"
#include "aion/gameserver/handlers/admincommands/BanMac.h"
#include "aion/gameserver/handlers/admincommands/Grant.h"
#include "aion/gameserver/handlers/admincommands/PasskeyReset.h"
#include "aion/gameserver/handlers/admincommands/PlayerInfo.h"
#include "aion/gameserver/handlers/admincommands/RPrison.h"
#include "aion/gameserver/handlers/admincommands/Ranking.h"
#include "aion/gameserver/handlers/admincommands/SPrison.h"
#include "aion/gameserver/handlers/admincommands/SecurityToken.h"
#include "aion/gameserver/handlers/admincommands/UnBan.h"
#include "aion/gameserver/handlers/admincommands/UnBanChar.h"
#include "aion/gameserver/handlers/admincommands/UnBanIp.h"
#include "aion/gameserver/handlers/admincommands/UnBanMac.h"
#include "aion/gameserver/handlers/playercommands/Advent.h"
#include "aion/gameserver/handlers/playercommands/Faction.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/network/BannedMacManager.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using serverpackets::SM_SYSTEM_MESSAGE;

template <class T>
class ConfigScope {
public:
	ConfigScope(std::atomic<T>& config, T value) : config_(config), previous_(config.load()) { config.store(value); }
	~ConfigScope() { config_.store(previous_); }

private:
	std::atomic<T>& config_;
	const T previous_;
};

class AdminSocialCommandsTest : public CommandTest {
protected:
	void SetUp() override {
		CommandTest::SetUp();
		std::map<std::string, int8_t, std::less<>> levels(*configs::administration::CommandsConfig::ACCESS_LEVELS.get());
		for (const char* alias : {"ban", "banchar", "unban", "unbanchar", "banip", "unbanip", "banmac", "unbanmac", "banhdd", "sprison", "rprison",
				 "ranking", "grant", "passkeyreset", "stoken", "announcements", "playerinfo"})
			levels[alias] = 3;
		levels["faction"] = 0;
		levels["advent"] = 0;
		configs::administration::CommandsConfig::ACCESS_LEVELS.set(levels);
	}

	void TearDown() override {
		for (Player* player : stored)
			world::World::getInstance().removeObject(*player);
		stored.clear();
		CommandTest::TearDown();
	}

	Player& online(int32_t objectId, std::string_view name, int8_t accessLevel, model::Race race = model::Race::ELYOS) {
		Player& player = connected(objectId, name, accessLevel);
		player.getCommonData()->setRace(race);
		spawnInPoeta(player);
		world::World::getInstance().storeObject(player);
		player.getCommonData()->setOnline(true);
		stored.push_back(&player);
		return player;
	}

	std::vector<uint8_t> system(SM_SYSTEM_MESSAGE&& packet, size_t index = 0) { return serialized(packet, client(index).con()); }

	void clearAll() {
		for (const std::unique_ptr<TestClient>& c : clients)
			(*c)->clearSent();
	}

	std::vector<Player*> stored;
};

// ---- //ban, //unban, //banip, //unbanip (Ban.java, UnBan.java, BanIp.java, UnBanIp.java) -----------------------------------------------------

TEST_F(AdminSocialCommandsTest, BanAndUnbanAnswerTheirSyntax) {
	Player& gm = online(731000, "Warden", 3);
	online(731001, "Rogue", 0);
	handlers::admincommands::Ban ban;
	handlers::admincommands::UnBan unban;
	const std::vector<uint8_t> banSyntax = message("Syntax: //ban <player> [account|ip|full] [time in minutes]");
	EXPECT_TRUE(ban.process(gm, args({})));
	EXPECT_EQ(client()->sentBytes(), exactly({banSyntax}));
	clearAll();
	EXPECT_TRUE(ban.process(gm, args({"Rogue", "x"})));
	EXPECT_EQ(client()->sentBytes(), exactly({banSyntax})) << "an unknown ban type";
	clearAll();
	EXPECT_TRUE(ban.process(gm, args({"Rogue", "acc", "ten"})));
	EXPECT_EQ(client()->sentBytes(), exactly({banSyntax})) << "a bad time";
	clearAll();
	EXPECT_TRUE(ban.process(gm, args({"Rogue", "i", "5"})));
	EXPECT_TRUE(client()->sentBytes().empty()) << "the ban packet goes to the login server";
	clearAll();
	EXPECT_TRUE(ban.process(gm, args({"Nobody"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Player Nobody was not found!"), banSyntax})) << "no database: account 0";

	clearAll();
	EXPECT_TRUE(unban.process(gm, args({})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Syntax: //unban <player> [account|ip|full]")}));
	clearAll();
	EXPECT_TRUE(unban.process(gm, args({"Nobody"})));
	EXPECT_EQ(client()->sentBytes(),
		exactly({message("Player Nobody was not found!"), message("Syntax: //unban <player> [account|ip|full]")}));

	handlers::admincommands::BanIp banIp;
	handlers::admincommands::UnBanIp unbanIp;
	clearAll();
	EXPECT_TRUE(banIp.process(gm, args({"10.0.0.*", "x"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Syntax: //banip <mask> [time in minutes]")})) << "info(player, e.getMessage())";
	clearAll();
	EXPECT_TRUE(banIp.process(gm, args({"10.0.0.*"})));
	EXPECT_TRUE(unbanIp.process(gm, args({"10.0.0.*"})));
	EXPECT_TRUE(client()->sentBytes().empty());
	EXPECT_TRUE(unbanIp.process(gm, args({})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Syntax: //unbanip <mask>")}));
}

// ---- //banchar, //unbanchar (BanChar.java, UnBanChar.java) -----------------------------------------------------------------------------------

TEST_F(AdminSocialCommandsTest, BanCharKicksAnOnlineCharacter) {
	Player& gm = online(731010, "Warden", 3);
	online(731011, "Rogue", 0);
	handlers::admincommands::BanChar banChar;
	const std::vector<uint8_t> syntax = message("Syntax: //banChar <playername> <days>/0 (for permanent) <reason>");
	const std::vector<uint8_t> note = message("Note: the current day is defined as a whole day even if it has just a few hours left!");
	EXPECT_TRUE(banChar.process(gm, args({"Rogue", "2"})));
	EXPECT_EQ(client()->sentBytes(), exactly({syntax, note}));
	clearAll();
	EXPECT_TRUE(banChar.process(gm, args({"Rogue", "two", "bot"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Second parameter is not an int"), syntax, note}));
	clearAll();
	EXPECT_TRUE(banChar.process(gm, args({"Rogue", "-1", "bot"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Second parameter has to be a positive daycount or 0 for infinity"), syntax, note}));
	clearAll();
	EXPECT_TRUE(banChar.process(gm, args({"Nobody", "2", "bot"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Player Nobody was not found!"), syntax, note}));

	clearAll();
	EXPECT_TRUE(banChar.process(gm, args({"Rogue", "2", "a", "bot"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Char Rogue is now banned for the next 2 days!")}));
	EXPECT_TRUE(client(1)->isPendingClose()) << "PunishmentService.banChar kicks him";

	handlers::admincommands::UnBanChar unbanChar;
	clearAll();
	EXPECT_TRUE(unbanChar.process(gm, args({"Nobody"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Player Nobody was not found!"), message("Syntax: //unbanchar <player>")}));
}

// ---- //banmac, //unbanmac (BanMac.java, UnBanMac.java) ---------------------------------------------------------------------------------------

TEST_F(AdminSocialCommandsTest, BanMacBansTheTargetsAddressAndUnbanMacLiftsIt) {
	Player& gm = online(731020, "Warden", 3);
	Player& rogue = online(731021, "Rogue", 0);
	client(1)->setMacAddress("aa-bb-cc-dd-ee-ff");
	handlers::admincommands::BanMac banMac;
	handlers::admincommands::UnBanMac unbanMac;
	const std::vector<uint8_t> syntax = message("Syntax: //banmac [time in minutes] <mac>");
	const std::vector<uint8_t> note = message("Note: 0 minutes will cause permanent ban");

	EXPECT_TRUE(banMac.process(gm, args({"ten"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Please enter a valid integer amount of minutes"), syntax, note}));
	clearAll();
	EXPECT_TRUE(banMac.process(gm, args({"10"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("You should select a player or give me any mac address"), syntax, note}));
	clearAll();
	gm.setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(gm));
	clearAll();
	EXPECT_TRUE(banMac.process(gm, args({"10"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Omg, disselect yourself please."), syntax, note}));

	gm.setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(rogue));
	EXPECT_TRUE(banMac.process(gm, args({"0"})));
	EXPECT_TRUE(network::BannedMacManager::getInstance().isBanned("aa-bb-cc-dd-ee-ff"));
	EXPECT_TRUE(client(1)->isPendingClose() || client(1)->isClosed()) << "the target's connection is closed";
	gm.setTarget(nullptr);

	clearAll();
	EXPECT_TRUE(unbanMac.process(gm, args({"aa-bb-cc-dd-ee-ff"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("mac aa-bb-cc-dd-ee-ff has unbanned")}));
	EXPECT_FALSE(network::BannedMacManager::getInstance().isBanned("aa-bb-cc-dd-ee-ff"));
	clearAll();
	EXPECT_TRUE(unbanMac.process(gm, args({"aa-bb-cc-dd-ee-ff"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("mac aa-bb-cc-dd-ee-ff is not banned")}));
	clearAll();
	EXPECT_TRUE(unbanMac.process(gm, args({})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Syntax: //unbanmac <mac>")}));
}

/** BanHdd.java:24-37: a missing parameter or a bad number answers the syntax (Java's catch of every Exception) */
TEST_F(AdminSocialCommandsTest, BanHddAnswersItsSyntax) {
	Player& gm = online(731025, "Warden", 3);
	handlers::admincommands::BanHdd banHdd;
	const std::vector<uint8_t> syntax = message("Syntax: //banhdd <hdd_serial> <time_in_minutes|0 - infinite>");
	EXPECT_TRUE(banHdd.process(gm, args({"SERIAL"})));
	EXPECT_EQ(client()->sentBytes(), exactly({syntax}));
	clearAll();
	EXPECT_TRUE(banHdd.process(gm, args({"SERIAL", "x"})));
	EXPECT_EQ(client()->sentBytes(), exactly({syntax}));
}

// ---- //sprison, //rprison (SPrison.java, RPrison.java) ---------------------------------------------------------------------------------------

TEST_F(AdminSocialCommandsTest, PrisonCommandsAnswerTheirSyntax) {
	Player& gm = online(731030, "Warden", 3);
	handlers::admincommands::SPrison sprison;
	handlers::admincommands::RPrison rprison;
	const std::vector<uint8_t> syntax = message("syntax //sprison <player> <delay> <reason>");
	EXPECT_TRUE(sprison.process(gm, args({"Rogue"})));
	EXPECT_EQ(client()->sentBytes(), exactly({syntax}));
	clearAll();
	EXPECT_TRUE(sprison.process(gm, args({"Rogue", "5"})));
	EXPECT_EQ(client()->sentBytes(), exactly({syntax})) << "no reason: params[2] out of bounds, caught";
	clearAll();
	EXPECT_TRUE(sprison.process(gm, args({"Rogue", "five", "x"})));
	EXPECT_EQ(client()->sentBytes(), exactly({syntax}));
	clearAll();
	EXPECT_TRUE(sprison.process(gm, args({"Nobody", "5", "x"})));
	EXPECT_TRUE(client()->sentBytes().empty()) << "nobody online of that name";

	clearAll();
	EXPECT_TRUE(rprison.process(gm, args({})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("syntax //rprison <player>")}));
	clearAll();
	EXPECT_TRUE(rprison.process(gm, args({"a", "b", "c"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("syntax //rprison <player>")}));
	clearAll();
	EXPECT_TRUE(rprison.process(gm, args({"Nobody"})));
	EXPECT_TRUE(client()->sentBytes().empty());
}

// ---- //ranking (Ranking.java) --------------------------------------------------------------------------------------------------------------

TEST_F(AdminSocialCommandsTest, RankingUpdateSchedulesTheRankUpdate) {
	Player& gm = online(731040, "Warden", 3);
	handlers::admincommands::Ranking ranking;
	EXPECT_TRUE(ranking.process(gm, args({"now"})));
	EXPECT_EQ(client()->sentBytes(), info(ranking.getSyntaxInfo()));
	auto* executor = dynamic_cast<runtime::DeterministicExecutor*>(utils::ThreadPoolManager::installedBackend());
	ASSERT_NE(executor, nullptr);
	const size_t before = executor->pendingTaskCount();
	clearAll();
	EXPECT_TRUE(ranking.process(gm, args({"UPDATE"})));
	EXPECT_TRUE(client()->sentBytes().empty());
	EXPECT_EQ(executor->pendingTaskCount(), before + 1) << "AbyssRankUpdateService.performUpdate's task";
}

// ---- //grant (Grant.java) -----------------------------------------------------------------------------------------------------------------

TEST_F(AdminSocialCommandsTest, GrantChecksItsTargetAndTheAccessLevels) {
	Player& gm = online(731050, "Warden", 3);
	Player& peer = online(731051, "Peer", 3);
	handlers::admincommands::Grant grant;
	EXPECT_TRUE(grant.process(gm, args({"a"})));
	EXPECT_EQ(client()->sentBytes(), info(grant.getSyntaxInfo()));
	clearAll();
	EXPECT_TRUE(grant.process(gm, args({"x", "1"})));
	EXPECT_EQ(client()->sentBytes(), info(grant.getSyntaxInfo()));
	clearAll();
	EXPECT_TRUE(grant.process(gm, args({"m", "-1"})));
	EXPECT_EQ(client()->sentBytes(), info("Level must not be negative."));
	clearAll();
	EXPECT_TRUE(grant.process(gm, args({"m", "1"})));
	EXPECT_EQ(client()->sentBytes(), exactly({system(SM_SYSTEM_MESSAGE::STR_INVALID_TARGET())})) << "no target";
	clearAll();
	EXPECT_TRUE(grant.process(gm, args({"m", "1", "Nobody"})));
	EXPECT_EQ(client()->sentBytes(), exactly({system(SM_SYSTEM_MESSAGE::STR_NO_SUCH_USER("Nobody"))}));
	clearAll();
	EXPECT_TRUE(grant.process(gm, args({"a", "1", "Peer"})));
	EXPECT_EQ(client()->sentBytes(),
		info("You are not allowed change the access level of players with the same or higher access level than your own."));
	clearAll();
	gm.setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(peer));
	clearAll();
	EXPECT_TRUE(grant.process(gm, args({"m", "2"})));
	EXPECT_TRUE(client()->sentBytes().empty()) << "a membership grant goes to the login server";
	gm.setTarget(nullptr);
}

// ---- //passkeyreset, //stoken (PasskeyReset.java, SecurityToken.java) ----------------------------------------------------------------------

TEST_F(AdminSocialCommandsTest, PasskeyResetAndSecurityToken) {
	Player& gm = online(731060, "Warden", 3);
	Player& rogue = online(731061, "Rogue", 0);
	handlers::admincommands::PasskeyReset passkeyReset;
	EXPECT_TRUE(passkeyReset.process(gm, args({"Rogue"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("syntax: //passkeyreset <player> <passkey>")}));
	clearAll();
	EXPECT_TRUE(passkeyReset.process(gm, args({"Nobody", "123456"})));
	EXPECT_EQ(client()->sentBytes(),
		exactly({message("player Nobody can't find!"), message("syntax: //passkeyreset <player> <passkey>")})) << "no database: account 0";

	handlers::admincommands::SecurityToken token;
	const std::vector<uint8_t> syntax = message("Syntax: //stoken <playername> || //stoken show <playername>");
	clearAll();
	EXPECT_TRUE(token.process(gm, args({"show"})));
	EXPECT_EQ(client()->sentBytes(), exactly({syntax}));
	clearAll();
	EXPECT_TRUE(token.process(gm, args({"show", "Nobody"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Can't find this player, maybe he's not online")}));
	clearAll();
	EXPECT_TRUE(token.process(gm, args({"show", "Rogue"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("This player haven't an Security Token!")}));
	rogue.getAccount()->setSecurityToken("TOKEN42");
	clearAll();
	EXPECT_TRUE(token.process(gm, args({"show", "Rogue"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("The Security Token of this player is: TOKEN42")}));
	clearAll();
	EXPECT_TRUE(token.process(gm, args({"Nobody"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Can't find this player, maybe he's not online")}));
}

// ---- //announcements (Announcements.java) -------------------------------------------------------------------------------------------------

TEST_F(AdminSocialCommandsTest, AnnouncementsValidateTheirParameters) {
	Player& gm = online(731070, "Warden", 3);
	handlers::admincommands::Announcements announcements;
	EXPECT_TRUE(announcements.process(gm, args({})));
	EXPECT_EQ(client()->sentBytes(), info(announcements.getSyntaxInfo()));
	clearAll();
	EXPECT_TRUE(announcements.process(gm, args({"list"})));
	EXPECT_EQ(client()->sentBytes(), info("There are no active announcements."));
	clearAll();
	EXPECT_TRUE(announcements.process(gm, args({"add", "both", "system", "300", "hi"})));
	EXPECT_EQ(client()->sentBytes(), info("Please specify a valid faction parameter."));
	clearAll();
	EXPECT_TRUE(announcements.process(gm, args({"add", "elyos", "green", "300", "hi"})));
	EXPECT_EQ(client()->sentBytes(), info("Please specify a valid chat type parameter."));
	clearAll();
	EXPECT_TRUE(announcements.process(gm, args({"add", "all", "shout", "299", "hi"})));
	EXPECT_EQ(client()->sentBytes(), info("Delay must be at least 300s (5 minutes)."));
	clearAll();
	EXPECT_TRUE(announcements.process(gm, args({"add", "all", "shout"})));
	EXPECT_EQ(client()->sentBytes(), info(announcements.getSyntaxInfo())) << "fewer than 4 parameters";
	clearAll();
	EXPECT_TRUE(announcements.process(gm, args({"add", "all", "shout", "300"})));
	EXPECT_EQ(client()->sentBytes(), info("The message cannot be empty."));
	clearAll();
	EXPECT_TRUE(announcements.process(gm, args({"delete"})));
	EXPECT_EQ(client()->sentBytes(), info("Please specify the ID of the announcement to delete."));
	clearAll();
	EXPECT_TRUE(announcements.process(gm, args({"what"})));
	EXPECT_EQ(client()->sentBytes(), info(announcements.getSyntaxInfo()));
}

// ---- //playerinfo (PlayerInfo.java) ------------------------------------------------------------------------------------------------------

TEST_F(AdminSocialCommandsTest, PlayerInfoShowsTheBasicsAndTheAbyssPoints) {
	Player& gm = online(731080, "Warden", 3);
	Player& rogue = online(731081, "Rogue", 0);
	rogue.setAbyssRank(model::gameobjects::player::AbyssRank::create(7, 0, 1234, 1, 2, 0, 9, 1, 0, 0, 0, 0, 0, 0, 0));
	handlers::admincommands::PlayerInfo playerInfo;
	EXPECT_TRUE(playerInfo.process(gm, args({"Nobody"})));
	EXPECT_EQ(client()->sentBytes(), exactly({system(SM_SYSTEM_MESSAGE::STR_NO_SUCH_USER("Nobody"))}));
	clearAll();
	EXPECT_TRUE(playerInfo.process(gm, args({"Rogue", "ap"})));
	std::vector<std::vector<uint8_t>> sent = client()->sentBytes();
	ASSERT_EQ(sent.size(), 6u) << "the basic info, then the four AP lines after their title";
	EXPECT_EQ(sent[1], message("- AP info:"));
	EXPECT_EQ(sent[2], message("\tTotal AP = 1234"));
	EXPECT_EQ(sent[3], message("\tTotal Kills = 9"));
	EXPECT_EQ(sent[4], message("\tToday Kills = " + std::to_string(rogue.getAbyssRank()->getDailyKill())));
	EXPECT_EQ(sent[5], message("\tToday AP = " + std::to_string(rogue.getAbyssRank()->getDailyAP())));
	clearAll();
	EXPECT_TRUE(playerInfo.process(gm, args({"Rogue", "chars"})));
	sent = client()->sentBytes();
	ASSERT_EQ(sent.size(), 3u);
	EXPECT_EQ(sent[1], message("- Characters (1):"));
	EXPECT_EQ(sent[2], message("\tRogue"));
	clearAll();
	EXPECT_TRUE(playerInfo.process(gm, args({"Rogue", "party"})));
	sent = client()->sentBytes();
	ASSERT_EQ(sent.size(), 2u);
	EXPECT_EQ(sent[1], message("- Party: none"));
	clearAll();
	EXPECT_TRUE(playerInfo.process(gm, args({"Rogue", "legion"})));
	sent = client()->sentBytes();
	ASSERT_EQ(sent.size(), 2u);
	EXPECT_EQ(sent[1], message("- Legion: none"));
}

// ---- .faction, .advent (Faction.java, Advent.java) ----------------------------------------------------------------------------------------

TEST_F(AdminSocialCommandsTest, FactionChatReachesTheRaceAndTheStaff) {
	Player& elyo = online(731090, "Elyo", 0, model::Race::ELYOS);
	online(731091, "Peer", 0, model::Race::ELYOS);
	online(731092, "Asmo", 0, model::Race::ASMODIANS);
	online(731093, "Watcher", 3, model::Race::ASMODIANS);
	handlers::playercommands::Faction faction;
	{
		ConfigScope<bool> off(configs::main::CustomConfig::FACTION_CMD_CHANNEL, false);
		EXPECT_TRUE(faction.process(elyo, args({"hi"})));
		EXPECT_EQ(client()->sentBytes(), info("The faction channel is disabled."));
	}
	ConfigScope<bool> on(configs::main::CustomConfig::FACTION_CMD_CHANNEL, true);
	ConfigScope<bool> plain(configs::main::CustomConfig::FACTION_CHAT_CHANNEL, false);
	clearAll();
	EXPECT_TRUE(faction.process(elyo, args({"hello", "all"})));
	EXPECT_EQ(client(0)->sentBytes(), exactly({serialized(SM_MESSAGE(731090, "Elyo", "Elyo: hello all", model::ChatType::BRIGHT_YELLOW), client(0).con())}));
	EXPECT_EQ(client(1)->sentBytes(), exactly({serialized(SM_MESSAGE(731090, "Elyo", "Elyo: hello all", model::ChatType::BRIGHT_YELLOW), client(1).con())}));
	EXPECT_TRUE(client(2)->sentBytes().empty()) << "the other race";
	EXPECT_EQ(client(3)->sentBytes(), exactly({serialized(SM_MESSAGE(731090, "(E) Elyo", "Elyo: hello all", model::ChatType::BRIGHT_YELLOW), client(3).con())}))
		<< "the staff reads both factions";
}

TEST_F(AdminSocialCommandsTest, AdventIsClosedWhileDisabled) {
	Player& gm = online(731095, "Warden", 3);
	Player& player = online(731096, "Plain", 0);
	handlers::playercommands::Advent advent;
	ConfigScope<bool> off(configs::main::EventsConfig::ENABLE_ADVENT_CALENDAR, false);
	EXPECT_FALSE(advent.validateAccess(gm));
	EXPECT_EQ(client(0)->sentBytes(), info("The advent calendar is currently disabled."));
	EXPECT_FALSE(advent.validateAccess(player));
	EXPECT_TRUE(client(1)->sentBytes().empty()) << "only the staff is told";
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
