// The login server side of the scenario harness against the real aion_login_server (m5a-plan.md F-04, §5.1, §5.2 step 1): ScenarioServers
// creates the login server test schema with game server 1, starts the login server as a child process in its own process group, FakeLoginClient
// logs in with an auto-created account and reads the server list (game server 1 registered but offline: no game server runs here), CM_PLAY is
// refused, and CTRL_BREAK stops the login server in order. Skipped without AION_TEST_GS_DATABASE_URL and AION_TEST_LS_DATABASE_URL.

#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <stdexcept>
#include <string>

#include "FakeLoginClient.h"
#include "ScenarioServers.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::scenario {
namespace {

using namespace std::chrono_literals;

TEST(LoginServerHarnessTest, FakeLoginClientLogsInAndTheLoginServerStopsOnCtrlBreak) {
	std::optional<ScenarioEnvironment> environment = ScenarioEnvironment::fromEnvironment();
	if (!environment)
		GTEST_SKIP() << "gs.scenario harness: skipped (set AION_TEST_GS_DATABASE_URL and AION_TEST_LS_DATABASE_URL)";

	ScenarioServers::Config config;
	config.gameServerExecutable = AION_GAME_SERVER_EXECUTABLE;
	config.loginServerExecutable = AION_LOGIN_SERVER_EXECUTABLE;
	config.gameServerJavaDir = AION_GAMESERVER_JAVA_DIR;
	config.loginServerJavaDir = AION_LOGINSERVER_JAVA_DIR;
	config.outputDir = std::filesystem::path(AION_SCENARIO_OUTPUT_DIR) / "selftest" / "login";
	config.startupTimeout = 120s;
	config.stopTimeout = 60s;
	ScenarioServers servers(config, *environment);
	servers.createSchemas();
	EXPECT_EQ(servers.loginDatabase().queryString(servers.loginSchema(), "SELECT mask FROM gameservers WHERE id = 1"), "127.0.0.1");
	EXPECT_EQ(servers.gameDatabase().queryLong(servers.gameSchema(), "SELECT COUNT(*) FROM players"), 0);

	servers.startLoginServer();
	{
		FakeLoginClient client(servers.loginClientPort());
		client.login("m5aselftest", "password1");
		EXPECT_GT(client.sessionKey().accountId, 0);
		FakeLoginClient::ServerList list = client.requestServerList();
		ASSERT_EQ(list.servers.size(), 1u);
		EXPECT_EQ(list.servers[0].id, 1);
		EXPECT_FALSE(list.servers[0].online);
		EXPECT_THROW(client.play(1), std::runtime_error); // SM_PLAY_FAIL: the game server is not connected
	}
	EXPECT_EQ(servers.loginDatabase().queryLong(servers.loginSchema(), "SELECT COUNT(*) FROM account_data WHERE name = 'm5aselftest'"), 1);

	std::optional<int32_t> exitCode = servers.stopLoginServer();
	ASSERT_TRUE(exitCode) << servers.loginServer()->readLog();
	std::string log = servers.loginServer()->readLog();
	EXPECT_TRUE(*exitCode == 0 || *exitCode == 98) << "exit code " << *exitCode << "\n" << log; // 98: Windows refused CTRL_BREAK (no console)
	EXPECT_EQ(log.find(" ERROR "), std::string::npos) << log;
	servers.loginDatabase().drop(servers.loginSchema());
	servers.gameDatabase().drop(servers.gameSchema());
}

} // namespace
} // namespace aion::gameserver::scenario
