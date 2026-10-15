// GMService::onPlayerLogin (P4-05; GMService.java:50-56), the step PlayerEnterWorldService runs for every character that enters the world
// (PlayerEnterWorldService.java:323, C++ PlayerEnterWorldService.cpp:562). Its staff arm was AION_UNPORTED, so with the shipped
// gameserver.administration.login.execute_commands a staff character threw out of enterWorld, whose catch sent
// SM_ENTER_WORLD_CHECK(CONNECTION_ERROR): a GM account could not enter the world (m5j-plan.md I-02, D4, A-I4).
//
// The cases drive the real GMService singleton, the real ChatProcessor and a real Player with a real AionConnection (InWorldPacketRunSupport.h)
// in the state PlayerEnterWorldService leaves him at :323: the connection's active player, the friend list ONLINE (:191), in the World. The
// configuration is the shipped one where the arm reads it: login.execute_commands "//invis, //invul, //enemy none, //see",
// login.announce_levels "*", login.announce_to_all_players true (AdminConfig.java:77-86; ConfigDefaultsTest pins the C++ binding), restored
// afterwards.
//
// What the four login commands do (Invis, Invul, Enemy, See: game-server/data/handlers/admincommands) needs the command framework (K-01:
// ChatCommand::run, AdminCommand::process and validateAccess, ...) and the four commands, none of them ported (M5j stage 0); the C++ server
// registers no command, so handleChatCommand returns false for each, as Java's does for an unregistered alias. One case runs exactly that
// state. The other stands RecordingAdminCommand in for the four, registered under their aliases, so the loop's calls - the order, the
// player, the parsed parameters - are observable.

#include "InWorldPacketRunSupport.h"
#include "ItemPacketTestSupport.h"

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/gameobjects/player/FriendList.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/audit/GMService.h"
#include "aion/gameserver/utils/chathandlers/AdminCommand.h"
#include "aion/gameserver/utils/chathandlers/ChatProcessor.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using model::gameobjects::player::FriendList;
using model::gameobjects::player::Player;
using serverpackets::SM_MESSAGE;
using serverpackets::SM_SYSTEM_MESSAGE;
using utils::audit::GMService;
using utils::chathandlers::ChatProcessor;

/** AdminConfig.java:77, the shipped login.execute_commands, as the C++ binding splits it (ConfigDefaultsTest.AdminConfig) */
const std::vector<std::string> LOGIN_EXECUTE_COMMANDS{"//invis", "//invul", "//enemy none", "//see"};

/** GMService.java:87-88 */
constexpr std::string_view ANNOUNCE_PENDING =
	"Your login will be announced in 15s.\nYou can disable this by setting whisper off or changing your online status to invisible.";
/** GMService.java:92 */
constexpr std::string_view ANNOUNCED = "Your login has been announced.";

/** ChatUtil.charName(player) for a name without a custom tag (AdminConfig.NAME_TAGS is empty here): "[charname:" + name + ";1 1 1]" */
std::string charName(std::string_view name) {
	return "[charname:" + std::string(name) + ";1 1 1]";
}

/** One process call of a RecordingAdminCommand */
struct CommandCall {
	std::string command;
	int32_t playerObjectId = 0;
	std::vector<std::string> params;

	bool operator==(const CommandCall&) const = default;
};

/**
 * Stands in for one of the four login commands (their classes are not ported): ChatProcessor.handleChatCommand finds it under its alias and
 * calls process(player, params), which records the call. Registered once per process (commands are immortal in ChatProcessor).
 */
class RecordingAdminCommand final : public utils::chathandlers::AdminCommand {
public:
	explicit RecordingAdminCommand(std::string_view alias) : AdminCommand(alias) {}

	bool validateAccess(Player&) override { return true; }

	bool process(Player& player, std::span<const std::string> params) override {
		calls().push_back({getAliasWithPrefix(), player.getObjectId(), std::vector<std::string>(params.begin(), params.end())});
		return true;
	}

	void execute(Player&, std::span<const std::string>) override {}

	static std::vector<CommandCall>& calls() {
		static std::vector<CommandCall> recorded;
		return recorded;
	}
};

/** Registers the four stand-ins with ChatProcessor once (registerCommand reads each alias's level from commands.access_level) */
void registerRecordingCommands() {
	static const bool registered = [] {
		auto previousLevels = configs::administration::CommandsConfig::ACCESS_LEVELS.get();
		std::map<std::string, int8_t, std::less<>> levels(*previousLevels);
		for (const char* alias : {"invis", "invul", "enemy", "see"})
			levels[alias] = 1;
		configs::administration::CommandsConfig::ACCESS_LEVELS.set(levels);
		for (const char* alias : {"invis", "invul", "enemy", "see"})
			ChatProcessor::getInstance().registerCommand(*new RecordingAdminCommand(alias)); // immortal, like ChatProcessor.init's commands
		configs::administration::CommandsConfig::ACCESS_LEVELS.set(*previousLevels);
		return true;
	}();
	static_cast<void>(registered);
}

class GMServiceLoginTest : public InWorldPacketTest {
protected:
	void SetUp() override {
		InWorldPacketTest::SetUp();
		// the base fixture's DeterministicExecutor again, with a handle the cases advance to the announcement 15 s later
		utils::ThreadPoolManager::installBackend(nullptr);
		auto backend = std::make_unique<runtime::DeterministicExecutor>(clock, 17);
		executor = backend.get(); // owned by ThreadPoolManager until the base TearDown installs no backend
		utils::ThreadPoolManager::installBackend(std::move(backend));
		items::publishPoetaWorldDataOnce(); // World::getInstance() (broadcastToWorld, World.storeObject)
		previousCommands = configs::administration::AdminConfig::LOGIN_EXECUTE_COMMANDS.get();
		previousAnnounceLevels = configs::administration::AdminConfig::ANNOUNCE_LEVELS.get();
		previousAnnounceToAll = configs::administration::AdminConfig::ANNOUNCE_LOGIN_TO_ALL_PLAYERS.load();
		configs::administration::AdminConfig::LOGIN_EXECUTE_COMMANDS.set(LOGIN_EXECUTE_COMMANDS);
		configs::administration::AdminConfig::ANNOUNCE_LEVELS.set({"*"});
		configs::administration::AdminConfig::ANNOUNCE_LOGIN_TO_ALL_PLAYERS.store(true);
		RecordingAdminCommand::calls().clear();
	}

	void TearDown() override {
		for (PlayerFixture& fixture : players) {
			// Java PlayerLeaveWorldService: the friend list goes OFFLINE before GMService.onPlayerLogout, so nothing is announced here
			fixture.player->getFriendList().setStatus(FriendList::Status::OFFLINE, *fixture.commonData);
			GMService::getInstance().onPlayerLogout(*fixture.player);
			world::World::getInstance().removeObject(*fixture.player);
			fixture.player->setClientConnection(nullptr);
		}
		clients.clear();
		players.clear();
		RecordingAdminCommand::calls().clear();
		configs::administration::AdminConfig::LOGIN_EXECUTE_COMMANDS.set(*previousCommands);
		configs::administration::AdminConfig::ANNOUNCE_LEVELS.set(*previousAnnounceLevels);
		configs::administration::AdminConfig::ANNOUNCE_LOGIN_TO_ALL_PLAYERS.store(previousAnnounceToAll);
		InWorldPacketTest::TearDown();
	}

	/**
	 * A character of an account with `accessLevel` as PlayerEnterWorldService has him when it reaches GMService.onPlayerLogin: his connection
	 * IN_GAME with him as the active player, the friend list ONLINE (PlayerEnterWorldService.java:191) and stored in the World
	 */
	Player& entered(int32_t objectId, std::string_view name, int8_t accessLevel) {
		PlayerFixture& fixture = players.emplace_back(makePlayer(objectId, objectId + 1000, name));
		fixture.account->setAccessLevel(accessLevel);
		TestClient& client = *clients.emplace_back(std::make_unique<TestClient>());
		client.enterWorld(fixture);
		fixture.player->getFriendList().setStatus(FriendList::Status::ONLINE, *fixture.commonData);
		world::World::getInstance().storeObject(*fixture.player);
		client->clearSent();
		return *fixture.player;
	}

	TestClient& clientOf(size_t index) { return *clients[index]; }

	static bool isOnlineStaff(Player& player) {
		for (const runtime::Ptr<Player>& staff : GMService::getInstance().getOnlineStaffMembers()) {
			if (staff.get() == &player)
				return true;
		}
		return false;
	}

	static std::vector<uint8_t> message(TestClient& to, std::string_view text) {
		return serialized(SM_MESSAGE(0, "", text, model::ChatType::GOLDEN_YELLOW), to.con()); // PacketSendUtility.sendMessage(player, msg)
	}

	runtime::DeterministicExecutor* executor = nullptr;
	std::vector<PlayerFixture> players;
	std::vector<std::unique_ptr<TestClient>> clients;
	std::shared_ptr<const std::vector<std::string>> previousCommands;
	std::shared_ptr<const std::vector<std::string>> previousAnnounceLevels;
	bool previousAnnounceToAll = false;
};

// I-02's case: the shipped configuration and no registered command (the C++ server's ChatProcessor registers none). Before the port the arm
// threw UnportedException here, before the staff registration
TEST_F(GMServiceLoginTest, AStaffLoginWithTheShippedCommandsAndNoRegisteredCommandCompletes) {
	for (const std::string& command : LOGIN_EXECUTE_COMMANDS) {
		if (ChatProcessor::getInstance().isCommandExists(command.substr(0, command.find(' '))))
			GTEST_SKIP() << "a case of this process registered " << command << " (run the test on its own)";
	}
	Player& gm = entered(720301, "Warden", 3);
	runtime::resetUnportedHitsForTests();

	EXPECT_NO_THROW(GMService::getInstance().onPlayerLogin(gm));

	EXPECT_EQ(runtime::unportedHitCount(), 0u) << "the staff arm reaches no unported body";
	EXPECT_TRUE(isOnlineStaff(gm)) << "staffMembers.put(player.getObjectId(), player)";
	EXPECT_EQ(clientOf(0)->sentBytes(), exactly({message(clientOf(0), ANNOUNCE_PENDING)})) << "scheduleBroadcastLogin: announce_levels \"*\"";
	executor->advance(std::chrono::milliseconds(15000));
	EXPECT_EQ(clientOf(0)->sentBytes(), exactly({message(clientOf(0), ANNOUNCE_PENDING), message(clientOf(0), ANNOUNCED)}));
}

// Every configured command goes to ChatProcessor.handleChatCommand, in the configured order, for the entering player, with the parameters
// getParamsFromString splits off ("//enemy none" -> {"none"}); then the staff registration and the announcement, 15 s later, to every other
// player (announce_to_all_players)
TEST_F(GMServiceLoginTest, AStaffLoginRunsEveryLoginCommandThenRegistersAndAnnouncesTheStaffMember) {
	registerRecordingCommands();
	Player& gm = entered(720311, "Warden", 3);
	entered(720312, "Bystander", 0); // another player in the World: clientOf(1)

	GMService::getInstance().onPlayerLogin(gm);

	EXPECT_EQ(RecordingAdminCommand::calls(), (std::vector<CommandCall>{{"//invis", gm.getObjectId(), {}},
												  {"//invul", gm.getObjectId(), {}},
												  {"//enemy", gm.getObjectId(), {"none"}},
												  {"//see", gm.getObjectId(), {}}}));
	EXPECT_TRUE(isOnlineStaff(gm));
	EXPECT_EQ(clientOf(0)->sentBytes(), exactly({message(clientOf(0), ANNOUNCE_PENDING)}));
	EXPECT_TRUE(clientOf(1)->sentBytes().empty());

	executor->advance(std::chrono::milliseconds(14999));
	EXPECT_EQ(clientOf(0)->sentBytes().size(), 1u) << "delay * 1000 = 15000 ms";
	EXPECT_TRUE(clientOf(1)->sentBytes().empty());
	executor->advance(std::chrono::milliseconds(1));
	EXPECT_EQ(clientOf(1)->sentBytes(), exactly({serialized(SM_SYSTEM_MESSAGE::STR_NOTIFY_LOGIN_BUDDY(charName("Warden")), clientOf(1).con())}))
		<< "broadcastConnectionStatus(gm, true) to every player but the GM";
	EXPECT_EQ(clientOf(0)->sentBytes(), exactly({message(clientOf(0), ANNOUNCE_PENDING), message(clientOf(0), ANNOUNCED)}));
}

// A normal account (access level 0, player.isStaff() false) is unchanged: no command, no staff registration, nothing sent, nothing scheduled
TEST_F(GMServiceLoginTest, ANormalLoginRunsNoCommandAndIsNotRegistered) {
	registerRecordingCommands();
	Player& player = entered(720321, "Plain", 0);
	const size_t pending = executor->pendingTasksCount();

	GMService::getInstance().onPlayerLogin(player);

	EXPECT_TRUE(RecordingAdminCommand::calls().empty());
	EXPECT_FALSE(isOnlineStaff(player));
	EXPECT_TRUE(clientOf(0)->sentBytes().empty());
	EXPECT_EQ(executor->pendingTasksCount(), pending) << "no announcement is scheduled";
	executor->advance(std::chrono::milliseconds(15000));
	EXPECT_TRUE(clientOf(0)->sentBytes().empty());
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
