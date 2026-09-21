// The process side of the scenario harness (m5a-plan.md F-04, D4): ChildProcess (command line quoting, log redirection, exit codes,
// termination), and ScenarioServers with the stub game server (StubGameServer.cmake run by cmake): the M5a arguments, readiness on
// "Game server started", the stop file, the exit code and the check output reports. Needs no database.

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <cstdint>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "ChildProcess.h"
#include "ScenarioServers.h"

#include <Windows.h>

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::scenario {
namespace {

using namespace std::chrono_literals;

std::filesystem::path outputDir(std::string_view test) {
	return std::filesystem::path(AION_SCENARIO_OUTPUT_DIR) / "selftest" / test;
}

/** An environment whose URLs are never connected (the stub game server needs no database) */
ScenarioEnvironment offlineEnvironment() {
	ScenarioEnvironment environment;
	environment.gsUrl = "jdbc:mysql://127.0.0.1:1/aion_cpp_test?characterEncoding=UTF-8";
	environment.gsUser = "root";
	environment.lsUrl = "jdbc:mysql://127.0.0.1:1/aion_ls_test";
	environment.lsUser = "root";
	return environment;
}

ScenarioServers::Config stubConfig(std::string_view test) {
	ScenarioServers::Config config;
	config.gameServerExecutable = AION_SCENARIO_CMAKE_COMMAND;
	config.checkClientPort = false; // the stub game server listens on no port
	config.gameServerLeadingArguments = {"-P", AION_SCENARIO_STUB_GAME_SERVER, "--"};
	config.loginServerExecutable = AION_LOGIN_SERVER_EXECUTABLE;
	config.gameServerJavaDir = AION_GAMESERVER_JAVA_DIR;
	config.loginServerJavaDir = AION_LOGINSERVER_JAVA_DIR;
	config.outputDir = outputDir(test);
	config.startupTimeout = 60s;
	config.stopTimeout = 60s;
	return config;
}

TEST(ChildProcessTest, CommandLinesAreQuotedForTheMsvcRuntime) {
	EXPECT_EQ(ChildProcess::commandLine("C:/a b/x.exe", {"plain", "with space", "quote\"d", "trailing\\", "back\\\\slash \"x\"", ""}),
		LR"("C:/a b/x.exe" plain "with space" "quote\"d" trailing\ "back\\slash \"x\"" "")");
}

TEST(ChildProcessTest, RedirectsTheOutputAndReportsTheExitCode) {
	ChildProcess::Options options;
	options.executable = AION_SCENARIO_CMAKE_COMMAND;
	options.arguments = {"-E", "echo", "hello from the child", "with space"};
	options.logFile = outputDir("echo") / "child.log";
	ChildProcess child(options);
	std::optional<int32_t> exitCode = child.waitForExit(30s);
	ASSERT_TRUE(exitCode);
	EXPECT_EQ(*exitCode, 0);
	EXPECT_NE(child.readLog().find("hello from the child with space"), std::string::npos) << child.readLog();
	EXPECT_FALSE(child.isRunning());

	options.arguments = {"-E", "false"};
	options.logFile = outputDir("echo") / "false.log";
	ChildProcess failing(options);
	EXPECT_EQ(failing.waitForExit(30s), 1);
}

TEST(ChildProcessTest, TerminatesARunningProcess) {
	ChildProcess::Options options;
	options.executable = AION_SCENARIO_CMAKE_COMMAND;
	options.arguments = {"-E", "sleep", "60"};
	options.logFile = outputDir("sleep") / "child.log";
	options.newProcessGroup = true;
	ChildProcess child(options);
	EXPECT_TRUE(child.isRunning());
	EXPECT_FALSE(child.waitForExit(200ms));
	EXPECT_FALSE(child.waitForLog("never logged", 300ms));
	child.terminate(7);
	EXPECT_EQ(child.waitForExit(10s), 7);
	EXPECT_THROW(ChildProcess({"C:/no/such/executable.exe", {}, {}, outputDir("sleep") / "none.log", false}), std::runtime_error);
}

TEST(ChildProcessTest, ReadsTheLogWithoutHoldingItInMemory) {
	// stage 2: the game server's log can grow to hundreds of megabytes while the harness waits for "Game server started", so waitForLog scans
	// only what it has not seen yet, and the report cases read single lines or the tail instead of the whole file
	ChildProcess::Options options;
	options.executable = AION_SCENARIO_CMAKE_COMMAND;
	options.arguments = {"-E", "echo", "first line", ";", "wanted marker", ";", "last line"};
	options.logFile = outputDir("logreading") / "child.log";
	ChildProcess child(options);
	ASSERT_EQ(child.waitForExit(30s), 0);

	EXPECT_TRUE(child.waitForLog("wanted marker", 5s));
	// the same text again: the scan offset stays at the match instead of running past it
	EXPECT_TRUE(child.waitForLog("wanted marker", 5s));
	EXPECT_FALSE(child.waitForLog("never logged", 300ms));

	std::vector<std::string> lines = child.findLogLines("marker");
	ASSERT_EQ(lines.size(), 1u) << child.readLog();
	EXPECT_NE(lines[0].find("wanted marker"), std::string::npos);
	EXPECT_TRUE(child.findLogLines("never logged").empty());
	EXPECT_EQ(child.findLogLines("line", 1).size(), 1u) << "maxMatches stops the scan";

	EXPECT_NE(child.readLogTail(4096).find("last line"), std::string::npos);
	EXPECT_EQ(child.readLogTail(5).find("first line"), std::string::npos) << "a short tail must not reach the first line";
}

TEST(ChildProcessTest, KeepsTheStandardErrorOfAToolOutOfItsOutput) {
	// Oracle.cpp parses the tool's stdout as JSON, so its stderr must not be mixed into the same file
	ChildProcess::Options options;
	options.executable = AION_SCENARIO_CMAKE_COMMAND;
	options.arguments = {"-E", "cat", "C:/no/such/file/for/the/scenario/harness"};
	options.logFile = outputDir("stderr") / "out.txt";
	options.errorFile = outputDir("stderr") / "err.txt";
	ChildProcess child(options);
	EXPECT_NE(child.waitForExit(30s), 0);
	EXPECT_EQ(child.readLog(), "") << "the failure message belongs in the error file";
	std::ifstream errors(options.errorFile, std::ios::binary);
	std::stringstream content;
	content << errors.rdbuf();
	EXPECT_FALSE(content.str().empty()) << "cmake -E cat of a missing file writes to stderr";
}

TEST(ChildProcessTest, ClosesEveryHandleWhenTheChildCannotBeStarted) {
	// Stage 2 review: "the new errorFile path leaks the NUL stdin handle when the error file cannot be created". The log file and the NUL stdin
	// handle are opened before the error file, so every failure after them has to close both - including the create_directories of the error
	// file's parent, which throws when a path component is a file and closed nothing at all. The handle count is what proves it: a leak of one
	// handle per attempt is invisible in a single run and exhausts nothing in a short test, so only counting catches it.
	const std::filesystem::path directory = outputDir("handles");
	std::filesystem::create_directories(directory / "err_is_a_directory");
	{
		std::ofstream file(directory / "not_a_directory.txt", std::ios::binary | std::ios::trunc);
		file << "x";
	}

	ChildProcess::Options options;
	options.executable = AION_SCENARIO_CMAKE_COMMAND;
	options.arguments = {"-E", "true"};
	options.logFile = directory / "child.log";

	const std::vector<std::pair<std::string, ChildProcess::Options>> failures = [&] {
		std::vector<std::pair<std::string, ChildProcess::Options>> cases;
		ChildProcess::Options errorIsADirectory = options;
		errorIsADirectory.errorFile = directory / "err_is_a_directory"; // CreateFileW fails: the path is a directory
		cases.emplace_back("the error file is a directory", errorIsADirectory);
		ChildProcess::Options errorUnderAFile = options;
		errorUnderAFile.errorFile = directory / "not_a_directory.txt" / "err.txt"; // create_directories throws
		cases.emplace_back("the error file's parent is a file", errorUnderAFile);
		ChildProcess::Options noExecutable = options;
		noExecutable.executable = "C:/no/such/executable/for/the/scenario/harness.exe"; // CreateProcess fails, after the job object was created
		cases.emplace_back("the executable does not exist", noExecutable);
		return cases;
	}();

	const auto handleCount = [] {
		DWORD count = 0;
		GetProcessHandleCount(GetCurrentProcess(), &count);
		return static_cast<int64_t>(count);
	};
	for (const auto& [name, failing] : failures) // the first attempt of each kind may make the runtime cache a handle of its own
		EXPECT_THROW(ChildProcess{failing}, std::exception) << name;
	const int64_t before = handleCount();
	for (int32_t round = 0; round < 20; round++)
		for (const auto& [name, failing] : failures)
			EXPECT_THROW(ChildProcess{failing}, std::exception) << name;
	const int64_t after = handleCount();
	EXPECT_LE(after - before, 4) << "60 failed starts leaked " << (after - before) << " handles (the log file, the NUL stdin handle or the job)";
}

TEST(ScenarioServersTest, ReservedPortsAreDistinctAndTheThreeScenarioPortsDoNotCollide) {
	// reservePorts holds every acceptor open until all ports are known, so the OS cannot hand out one port twice
	std::vector<uint16_t> ports = ScenarioServers::reservePorts(8);
	ASSERT_EQ(ports.size(), 8u);
	std::set<uint16_t> distinct(ports.begin(), ports.end());
	EXPECT_EQ(distinct.size(), ports.size()) << "two reserved ports were equal";
	for (uint16_t port : ports)
		EXPECT_NE(port, 0);

	ScenarioServers servers(stubConfig("ports"), offlineEnvironment());
	std::set<uint16_t> scenarioPorts{servers.loginClientPort(), servers.loginGameServerPort(), servers.gameClientPort()};
	EXPECT_EQ(scenarioPorts.size(), 3u) << "the LS client, LS link and GS client ports must differ";
}

TEST(ScenarioServersTest, TheGameServerGetsTheM5aProfileAndTheScenarioArguments) {
	ScenarioServers::Config config = stubConfig("arguments");
	config.gameServerProperties["gameserver.shutdown.delay"] = "5";
	ScenarioServers servers(config, offlineEnvironment());
	EXPECT_TRUE(servers.gameSchema().starts_with("aion_gs_test_m5a_"));
	EXPECT_TRUE(servers.loginSchema().starts_with("aion_ls_test_m5a_"));
	EXPECT_NE(servers.gameClientPort(), 0);
	std::vector<std::string> arguments = servers.gameServerArguments();
	auto has = [&arguments](std::string_view argument) { return std::ranges::find(arguments, argument) != arguments.end(); };
	EXPECT_TRUE(has("-Dgameserver.dev.missing_ai_handlers=warn"));
	EXPECT_TRUE(has("-Dgameserver.siege.enable=false"));
	EXPECT_TRUE(has("-Dgameserver.event.service.disabled_events=*"));
	EXPECT_TRUE(has("-Dgameserver.geodata.enable=false"));
	EXPECT_TRUE(has("-Dgameserver.character.reentry.time=1"));
	EXPECT_TRUE(has("-Dgameserver.shutdown.delay=5")); // the configured key wins over the profile
	EXPECT_TRUE(has("-Dgameserver.network.client.socket_address=127.0.0.1:" + std::to_string(servers.gameClientPort())));
	EXPECT_TRUE(has("-Dgameserver.network.login.address=127.0.0.1:" + std::to_string(servers.loginGameServerPort())));
	EXPECT_TRUE(has("-Ddatabase.url=jdbc:mysql://127.0.0.1:1/" + servers.gameSchema() + "?serverTimezone=${gameserver.timezone}&characterEncoding=UTF-8"));
	EXPECT_TRUE(has("-Ddatabase.user=root"));
	EXPECT_TRUE(has("--stop-file=" + servers.stopFile().string()));
	EXPECT_TRUE(has("--check-output=" + servers.checkOutputDir().string()));
	// the gate's game server must never write the shared game-server/log: Logging::init archives and DELETES the *.log files it finds there
	EXPECT_TRUE(has("--log-folder=" + servers.logFolder().string()));
	EXPECT_NE(servers.logFolder(), std::filesystem::path("log"));

	std::vector<std::string> ls = servers.loginServerArguments();
	auto lsHas = [&ls](std::string_view argument) { return std::ranges::find(ls, argument) != ls.end(); };
	EXPECT_TRUE(lsHas("-Dloginserver.network.client.socket_address=127.0.0.1:" + std::to_string(servers.loginClientPort())));
	EXPECT_TRUE(lsHas("-Dloginserver.network.gameserver.socket_address=127.0.0.1:" + std::to_string(servers.loginGameServerPort())));
	EXPECT_TRUE(lsHas("-Dloginserver.accounts.autocreate=true"));
	EXPECT_TRUE(lsHas("-Ddatabase.url=jdbc:mysql://127.0.0.1:1/" + servers.loginSchema() + "?serverTimezone=&characterEncoding=UTF-8"));
	// The login server has no --log-folder: it writes ./log in its working directory, so the gate gives it one of its own. Without this the
	// child archives and DELETES the shared login-server/log of the other build trees and of the user's own login server.
	EXPECT_EQ(servers.loginServerWorkingDirectory(), config.outputDir / "ls_run");
	EXPECT_EQ(servers.loginLogFolder(), config.outputDir / "ls_run" / "log");
	EXPECT_NE(servers.loginLogFolder(), std::filesystem::path(AION_LOGINSERVER_JAVA_DIR) / "log");
}

TEST(ScenarioServersTest, AStubGameServerThatStopsInOrderReportsNoStopProblem) {
	ScenarioServers servers(stubConfig("stopproblems-clean"), offlineEnvironment());
	servers.startGameServer();
	EXPECT_EQ(servers.stopGameServer(), 0);
	EXPECT_TRUE(servers.stopProblems().empty());
}

TEST(ScenarioServersTest, StopProblemsNameAGameServerThatDidNotExitWithZero) {
	// Stage 2 review: "nothing checks the game server's exit code when case 7 does not run". The gate's case 7 asserts it, but case 7 is skipped
	// when an earlier case failed, so the harness itself collects what went wrong - and the destructor reports a list nobody read.
	ScenarioServers servers(stubConfig("stopproblems-exitcode"), offlineEnvironment());
	servers.startGameServer();
	ASSERT_NE(servers.gameServer(), nullptr);
	servers.gameServer()->terminate(3); // as if the game server had died instead of shutting down on the stop file
	EXPECT_EQ(servers.stopGameServer(), 3);
	const std::vector<std::string> problems = servers.stopProblems();
	ASSERT_EQ(problems.size(), 1u) << (problems.empty() ? "" : problems[0]);
	EXPECT_EQ(problems[0], "the game server exited with code 3 (expected 0)");
}

TEST(ScenarioServersTest, StopProblemsNameAServerThatWasStillRunningAtTheEnd) {
	ScenarioServers servers(stubConfig("stopproblems-running"), offlineEnvironment());
	servers.startGameServer();
	const std::vector<std::string> problems = servers.stopProblems(); // read here, so the destructor does not report it as a failure
	ASSERT_EQ(problems.size(), 1u) << (problems.empty() ? "" : problems[0]);
	EXPECT_EQ(problems[0], "the game server was still running at the end of the run and had to be terminated");
}

TEST(ScenarioServersTest, StubGameServerStartsStopsAndWritesItsReports) {
	ScenarioServers servers(stubConfig("stub"), offlineEnvironment());
	servers.startGameServer();
	ASSERT_NE(servers.gameServer(), nullptr);
	EXPECT_TRUE(servers.gameServer()->isRunning());
	std::optional<int32_t> exitCode = servers.stopGameServer();
	ASSERT_TRUE(exitCode) << servers.gameServer()->readLog();
	EXPECT_EQ(*exitCode, 0) << servers.gameServer()->readLog();
	std::string log = servers.gameServer()->readLog();
	EXPECT_NE(log.find("Stop file"), std::string::npos) << log;
	EXPECT_NE(log.find("Runtime shut down"), std::string::npos) << log;
	EXPECT_FALSE(std::filesystem::exists(servers.stopFile()));

	std::map<std::string, std::vector<std::string>> summary = servers.readSummary();
	EXPECT_EQ(summary["started"], std::vector<std::string>{"true"});
	EXPECT_EQ(summary["notPortedClientPacket"], (std::vector<std::string>{"CM_A", "CM_B"}));
	EXPECT_TRUE(servers.readReportLines("unported_trace.txt").empty());
	EXPECT_THROW(servers.readReportLines("missing.txt"), std::runtime_error);
}

TEST(ScenarioServersTest, AGameServerThatStopsDuringTheStartupFailsTheReadiness) {
	ScenarioServers::Config config = stubConfig("stub-fail");
	config.gameServerProperties["gameserver.stub.fail"] = "true";
	ScenarioServers servers(config, offlineEnvironment());
	EXPECT_THROW(servers.startGameServer(), std::runtime_error);
	ASSERT_NE(servers.gameServer(), nullptr);
	EXPECT_EQ(servers.gameServer()->waitForExit(30s), 1);
	EXPECT_NE(servers.gameServer()->readLog().find("startup stopped at an unported function"), std::string::npos);
}

} // namespace
} // namespace aion::gameserver::scenario
