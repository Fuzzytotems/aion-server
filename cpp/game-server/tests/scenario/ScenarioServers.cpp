#include "ScenarioServers.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <cwctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <thread>

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::scenario {

namespace {

/**
 * m5c-plan.md G-07: New Year's midnight of 2000, 2100 and 2101 - a schedule whose next fire time no gate run reaches, accepted by every
 * consumer of a wall-clock key (m5aProfile below): CronService needs one fire time after now, and AbstractCronTask's constructor needs a
 * second one after it and one before now (findLastPlannedRun, AbstractCronTask.java:108-118: the interval between the next two fire times,
 * then back from now in half intervals until a fire time lies before now). A single year such as "0 0 0 1 1 ? 2100" has no second fire time,
 * and the housing tasks' constructors throw a NullPointerException at startup (Java: on getTime() of the null Date), and a schedule without a
 * past fire time never leaves that loop. The last fire time before the run is 2000-01-01, so no server stop after it makes shouldRunOnStart
 * true. Every year lies inside Quartz's 1970 to the current year + 100 and the C++ CronExpression's 1970-2299.
 */
constexpr std::string_view FAR_FUTURE_CRON = "0 0 0 1 1 ? 2000,2100,2101";

/**
 * true if the file names are equal in any letter case: Windows opens ./config/myls.properties whatever the case of the file's name, while
 * std::filesystem::path compares case-sensitively (the review of lane H)
 */
bool sameFileNameIgnoringCase(const std::filesystem::path& name, const std::filesystem::path& other) {
	const std::wstring a = name.wstring();
	const std::wstring b = other.wstring();
	return std::ranges::equal(a, b, [](wchar_t x, wchar_t y) { return std::towlower(x) == std::towlower(y); });
}

} // namespace

std::map<std::string, std::string> ScenarioServers::m5aProfile() {
	return {
		// D1
		{"gameserver.dev.missing_ai_handlers", "warn"},
		{"gameserver.siege.enable", "false"},
		{"gameserver.autogroup.enable", "false"},
		{"gameserver.rift.enable", "false"},
		{"gameserver.vortex.enable", "false"},
		{"gameserver.worldraid.enable", "false"},
		{"gameserver.cp.enable", "false"},
		{"gameserver.limits.enable", "false"},
		{"gameserver.event.service.disabled_events", "*"},
		// the scenario keys
		{"gameserver.geodata.enable", "false"},
		{"gameserver.character.reentry.time", "1"},
		{"gameserver.shutdown.delay", "2"},
		// m5c-plan.md G-07: every configurable wall-clock cron job that reaches unported code or spawns into the world, out of every gate run. A
		// gate whose server was up at Sunday 18:50 on 2026-09-27 failed its unported bar on PanesterraService::startAhserionRaid (P5-SC.md "M5c
		// stage 1 integration"). The census of the jobs a gate's server schedules (P5-SC.md "M5c stage 2", the rerun rule):
		// - CronJobService, whatever gameserver.siege.enable says (CronJobService.java:35, 62): the Ahserion raid (Sunday 18:50, AION_UNPORTED)
		//   and the Moltenus spawn (Sunday 22:00, a boss in Reshanta);
		// - the housing AbstractCronTasks (GameServer.java:120-121): AuctionEndTask (Sunday 12:00, AION_PARTIAL at AuctionEndTask.cpp:81) and
		//   AuctionAutoFillTask (Monday 00:00, AION_PARTIAL at AuctionAutoFillTask.cpp:38, as gameserver.housing.auction.enable is true);
		// - AbyssRankUpdateService (AbyssRankUpdateService.java:30, 33): the rank update (daily 00:00) and the GP loss (daily 12:00), both
		//   AION_PARTIAL (AbyssRankUpdateService.cpp:37, 58): no row of M5a's list, §B rows (asserted unhit) of the M5b, M5b-2, M5b-3 and M5c
		//   lists (the M5b lists had them in §C until the M5c stage-2 integration).
		// Each key gets FAR_FUTURE_CRON (above: a past-only year is refused as "the given trigger will never fire", CronService.cpp:318-321,
		// and a single future year throws in the housing tasks' constructors). The keys live here and in ScenarioServersTest only, never in
		// the Java tree's m5c.properties.example, whose lines the owner copies into mygs.properties to play with a real client. No key moves
		// the hard-coded jobs: LegionDominion's weekly calculation ("0 0 9 ? * WED *", CronJobService.cpp:185-187, AION_UNPORTED; the M5c gate
		// names a hit of it as the cron's, m5c-plan.md §20.7 item 2), QuestEngine's reset notice and AtreianPassportService's stamp reset (daily
		// 09:00; packets to the online players, no unported code). MaintenanceTask (Monday 00:00) reaches its AION_PARTIAL only for an owned
		// house, and no gate owns one.
		{"gameserver.siege.panesterra.ahserion.time", std::string(FAR_FUTURE_CRON)},
		{"gameserver.moltenus.time", std::string(FAR_FUTURE_CRON)},
		{"gameserver.housing.auction.end_time", std::string(FAR_FUTURE_CRON)},
		{"gameserver.housing.auction.auto_fill.time", std::string(FAR_FUTURE_CRON)},
		{"gameserver.topranking.updaterule", std::string(FAR_FUTURE_CRON)},
		{"gameserver.topranking.daily.gploss.time", std::string(FAR_FUTURE_CRON)},
	};
}

uint16_t ScenarioServers::freePort() {
	return reservePorts(1).front();
}

std::vector<uint16_t> ScenarioServers::reservePorts(size_t count) {
	// every acceptor stays open until all ports are known: the OS cannot hand out the same ephemeral port twice, and no other process can take
	// a released one in between (a duplicate would surface minutes into the gate as an opaque bind error inside a child process)
	asio::io_context io;
	std::vector<asio::ip::tcp::acceptor> acceptors;
	std::vector<uint16_t> ports;
	acceptors.reserve(count);
	ports.reserve(count);
	for (size_t i = 0; i < count; i++) {
		acceptors.emplace_back(io, asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0));
		ports.push_back(acceptors.back().local_endpoint().port());
	}
	return ports;
}

ScenarioServers::ScenarioServers(Config configValue, ScenarioEnvironment environmentValue)
	: config(std::move(configValue)), environment(std::move(environmentValue)),
	  lsDatabase(environment.lsUrl, environment.lsUser, environment.lsPassword),
	  gsDatabase(environment.gsUrl, environment.gsUser, environment.gsPassword) {
	std::string suffix = schemaSuffix(config.schemaSeed.empty() ? config.outputDir.generic_string() : config.schemaSeed);
	lsSchema = "aion_ls_test_" + config.schemaPrefix + "_" + suffix;
	gsSchema = "aion_gs_test_" + config.schemaPrefix + "_" + suffix;
	const std::vector<uint16_t> ports = reservePorts(3); // reserved together: three freePort() calls in a row could repeat a port
	loginPort = ports[0];
	gameServerLinkPort = ports[1];
	gamePort = ports[2];
	std::filesystem::create_directories(config.outputDir);
}

ScenarioServers::~ScenarioServers() {
	// A run that never looked at how its servers stopped must not pass with a killed or hung server: the gate's own case 7 asserts the game
	// server's exit code, but case 7 does not run when an earlier case failed (stage 2 review). Reported before the children are terminated,
	// because "was still running at the end" is one of the problems.
	if (stopProblemsUnreported()) {
		const std::vector<std::string> problems = stopProblems();
		if (!problems.empty()) {
			std::string text;
			for (const std::string& problem : problems)
				text += "\n  " + problem;
			ADD_FAILURE() << "the scenario servers did not stop cleanly:" << text;
		}
	}
	// a failed test: never leave servers behind (ChildProcess terminates on destruction)
	gs.reset();
	ls.reset();
}

void ScenarioServers::createSchemas() {
	// The in-use markers come first: they tell a sweep in another build tree - and the one below - that these two schemas belong to a running
	// gate. MariaDB drops a session lock when the connection dies, so they disappear by themselves if this process is killed (CTest TIMEOUT).
	lsLease = lsDatabase.lease(lsSchema);
	gsLease = gsDatabase.lease(gsSchema);
	if (!lsLease.held() || !gsLease.held())
		throw std::runtime_error("another scenario run is using " + (lsLease.held() ? gsSchema : lsSchema) +
		                         " (its in-use marker is held); run one gate per build tree");
	// Schemas of runs that were killed before they could drop their own: with the gate's TIMEOUT 900 and a crash, nothing runs a destructor,
	// so this is the only place that ever reclaims them. Nothing a live run uses can be dropped here (marker held, or younger than an hour).
	for (const std::string& schema : lsDatabase.dropAbandonedSchemas("aion_ls_test_" + config.schemaPrefix + "_", ABANDONED_SCHEMA_AGE))
		std::cout << "the scenario harness dropped the abandoned schema " << schema << " of an earlier run" << std::endl;
	for (const std::string& schema : gsDatabase.dropAbandonedSchemas("aion_gs_test_" + config.schemaPrefix + "_", ABANDONED_SCHEMA_AGE))
		std::cout << "the scenario harness dropped the abandoned schema " << schema << " of an earlier run" << std::endl;
	lsDatabase.recreate(lsSchema, config.loginServerJavaDir / "sql" / "aion_ls.sql");
	gsDatabase.recreate(gsSchema, config.gameServerJavaDir / "sql" / "aion_gs.sql");
	lsDatabase.execute(lsSchema, "INSERT INTO gameservers (id, mask, password) VALUES (1, '127.0.0.1', '1234')");
}

void ScenarioServers::dropSchemas() {
	// the markers go last: dropping a schema whose in-use marker this process holds is exactly what the marker is for, and releasing them first
	// would open a window in which another run's sweep could see a free marker on a schema that still exists
	gsDatabase.drop(gsSchema);
	lsDatabase.drop(lsSchema);
	gsLease.release();
	lsLease.release();
}

bool ScenarioServers::keepSchemasOnFailure() {
	const char* value = std::getenv("AION_SCENARIO_KEEP_SCHEMAS");
	return value != nullptr && *value != '\0' && std::string_view(value) != "0";
}

std::vector<std::string> ScenarioServers::loginServerArguments() const {
	std::string url = schemaUrl(environment.lsUrl, lsSchema, configuredDatabaseUrl(config.loginServerJavaDir));
	return {
		"-Ddatabase.url=" + url,
		"-Ddatabase.user=" + environment.lsUser,
		"-Ddatabase.password=" + environment.lsPassword,
		"-Dloginserver.network.client.socket_address=127.0.0.1:" + std::to_string(loginPort),
		"-Dloginserver.network.gameserver.socket_address=127.0.0.1:" + std::to_string(gameServerLinkPort),
		"-Dloginserver.accounts.autocreate=true",
	};
}

std::vector<std::string> ScenarioServers::gameServerArguments() const {
	std::string url = schemaUrl(environment.gsUrl, gsSchema, configuredDatabaseUrl(config.gameServerJavaDir));
	std::vector<std::string> arguments = {
		"-Ddatabase.url=" + url,
		"-Ddatabase.user=" + environment.gsUser,
		"-Ddatabase.password=" + environment.gsPassword,
	};
	std::map<std::string, std::string> properties = m5aProfile();
	for (const auto& [key, value] : config.gameServerProperties)
		properties[key] = value;
	properties["gameserver.network.client.socket_address"] = "127.0.0.1:" + std::to_string(gamePort);
	properties["gameserver.network.login.address"] = "127.0.0.1:" + std::to_string(gameServerLinkPort);
	for (const auto& [key, value] : properties)
		arguments.push_back("-D" + key + "=" + value);
	arguments.push_back("--stop-file=" + stopFile().string());
	arguments.push_back("--check-output=" + checkOutputDir().string());
	// The gate's game server gets its own log directory (main.cpp --log-folder). Without it the child writes the shared game-server/log, which
	// Logging::init archives and DELETES at startup: two build trees running the gate, or a gate run next to the user's own play server, then
	// destroy each other's logs. It also puts the server's own server_errors.log where Q8 can read it (RunStartupSmoke.cmake does the same).
	arguments.push_back("--log-folder=" + logFolder().string());
	// The same for the HTML cache: HTMLCache writes `./cache/html.cache` below the shared working directory whenever the file is missing, so two
	// gates starting at once (ScenarioTests.cmake runs two at a time) would truncate and read one file. A -D key, not a profile entry: it is a
	// path of this run, not a server setting.
	arguments.push_back("-Dgameserver.html.cache.file=" + htmlCacheFile().string());
	// The operator's play profile stays out (main.cpp --ignore-mygs-properties, the C++ test hook of Config::setOverrideFileIgnored): the
	// working directory is the Java module directory, whose untracked config/mygs.properties the owner writes to play (on 2026-09-29 it set
	// gameserver.simple.secondclass.enable = true, p6q-ascension-route.md §7). Without the switch a key a gate does not pin took the owner's
	// value in the main tree and the shipped default in CI and in a worktree; with it every gate runs the shipped defaults and its own keys.
	arguments.push_back(std::string(IGNORE_MYGS_PROPERTIES));
	return arguments;
}

void ScenarioServers::prepareLoginServerDirectory() const {
	// The login server reads ./config/main, ./config/network and ./config/myls.properties and writes ./log, all relative to its working
	// directory (Config.cpp:63-67, Logging::Config::logFolder "log"), and unlike the game server it has no --log-folder argument. Started in
	// the Java module directory it therefore writes the shared login-server/log, which Logging::init archives and DELETES at startup: two build
	// trees running the gate, or a gate run next to the user's own login server, destroy each other's logs (the game server's half of this was
	// fixed with --log-folder in stage 2). The child gets a working directory of its own with a copy of config/ instead - four small files.
	// The copy leaves config/myls.properties out, the operator's untracked override file (loginServerOverrideFile()): like the game server's
	// --ignore-mygs-properties, the gate's login server runs the shipped defaults and the harness's own -D keys on every machine. Only the
	// copy is built here; the Java module directory is only read.
	const std::filesystem::path directory = loginServerWorkingDirectory();
	const std::filesystem::path source = config.loginServerJavaDir / "config";
	if (!std::filesystem::is_directory(source))
		throw std::runtime_error("the login server has no configuration directory " + source.string());
	std::filesystem::create_directories(directory);
	std::filesystem::remove_all(directory / "config");
	std::filesystem::create_directories(directory / "config");
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(source)) {
		if (sameFileNameIgnoringCase(entry.path().filename(), loginServerOverrideFile().filename()))
			continue;
		std::filesystem::copy(entry.path(), directory / "config" / entry.path().filename(),
			std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
	}
}

std::string ScenarioServers::gameServerProfileProblem(std::string_view log) {
	if (log.find(GAME_SERVER_PROFILE_LOADED) != std::string_view::npos)
		return "the game server read the operator's config/mygs.properties (its log says '" + std::string(GAME_SERVER_PROFILE_LOADED) +
		       "'), so the gate would run with the owner's play profile";
	if (log.find(GAME_SERVER_PROFILE_IGNORED) == std::string_view::npos)
		return "the game server's log does not say that it left config/mygs.properties out ('" + std::string(GAME_SERVER_PROFILE_IGNORED) +
		       "' is missing): was it started without " + std::string(IGNORE_MYGS_PROPERTIES) + "?";
	return {};
}

std::string ScenarioServers::loginServerProfileProblem(std::string_view log) {
	if (log.find(LOGIN_SERVER_NO_PROFILE) == std::string_view::npos)
		return "the login server read an override file: its log does not say '" + std::string(LOGIN_SERVER_NO_PROFILE) +
		       "', so its working directory held a config/myls.properties (the owner's play profile?)";
	return {};
}

void ScenarioServers::startLoginServer() {
	prepareLoginServerDirectory();
	ChildProcess::Options options;
	options.executable = config.loginServerExecutable;
	options.arguments = loginServerArguments();
	options.workingDirectory = loginServerWorkingDirectory();
	options.logFile = config.outputDir / "login_server.log";
	options.newProcessGroup = true;
	ls = std::make_unique<ChildProcess>(std::move(options));
	const std::string listening = "Listening on 127.0.0.1:" + std::to_string(loginPort);
	if (!ls->waitForLog(listening, config.startupTimeout))
		throw std::runtime_error("the login server did not log '" + listening + "' (see " + ls->options().logFile.string() + ")");
	const std::string gameServers = "Listening on 127.0.0.1:" + std::to_string(gameServerLinkPort);
	if (!ls->waitForLog(gameServers, config.startupTimeout))
		throw std::runtime_error("the login server did not log '" + gameServers + "' (see " + ls->options().logFile.string() + ")");
	if (const std::string problem = loginServerProfileProblem(ls->readLog()); !problem.empty())
		throw std::runtime_error(problem + " (see " + ls->options().logFile.string() + ")");
	loginServerReady = true;
}

void ScenarioServers::startGameServer() {
	std::error_code error;
	std::filesystem::remove(stopFile(), error);
	std::filesystem::remove_all(checkOutputDir(), error);
	// every start parses, compacts and writes the HTML cache itself, instead of reading what the previous run of this gate wrote
	std::filesystem::remove(htmlCacheFile(), error);
	ChildProcess::Options options;
	options.executable = config.gameServerExecutable;
	options.arguments = config.gameServerLeadingArguments;
	for (std::string& argument : gameServerArguments())
		options.arguments.push_back(std::move(argument));
	options.workingDirectory = config.gameServerJavaDir;
	options.logFile = config.outputDir / "game_server.log";
	gs = std::make_unique<ChildProcess>(std::move(options));
	if (!gs->waitForLog("Game server started", config.startupTimeout))
		throw std::runtime_error("the game server did not log 'Game server started' (see " + gs->options().logFile.string() + ")");
	if (const std::string problem = gameServerProfileProblem(gs->readLog()); !problem.empty())
		throw std::runtime_error(problem + " (see " + gs->options().logFile.string() + ")");
	if (ls && !ls->waitForLog("Gameserver #1 is now online", config.startupTimeout))
		throw std::runtime_error("the login server did not authenticate game server 1 (see " + ls->options().logFile.string() + ")");
	if (config.checkClientPort && !waitForClientPort(std::chrono::seconds(30)))
		throw std::runtime_error("the game server does not accept client connections on port " + std::to_string(gamePort));
	gameServerReady = true;
}

bool ScenarioServers::waitForClientPort(std::chrono::milliseconds timeout) const {
	const auto deadline = std::chrono::steady_clock::now() + timeout;
	for (;;) {
		try {
			asio::io_context io;
			asio::ip::tcp::socket socket(io);
			socket.connect(asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), gamePort));
			socket.close();
			return true;
		} catch (const std::exception&) {
			if (std::chrono::steady_clock::now() >= deadline)
				return false;
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}
}

std::optional<int32_t> ScenarioServers::stopGameServer() {
	if (!gs)
		return std::nullopt;
	{
		std::ofstream stop(stopFile(), std::ios::binary | std::ios::trunc);
		stop << "stop\n";
	}
	gameServerStopped = true;
	gameServerExit = gs->waitForExit(config.stopTimeout);
	return gameServerExit;
}

std::optional<int32_t> ScenarioServers::stopLoginServer() {
	if (!ls)
		return std::nullopt;
	loginServerStopped = true;
	if (!ls->sendCtrlBreak() && ls->isRunning()) {
		// Windows refused the event (no shared console) and the login server is still up: it is killed, which is NOT an orderly shutdown - it
		// is reported as a problem instead of passing as exit code 98. A refusal after the server has already exited is not a kill.
		loginServerKilled = true;
		ls->terminate(98);
	}
	loginServerExit = ls->waitForExit(config.stopTimeout);
	return loginServerExit;
}

std::vector<std::string> ScenarioServers::stopProblemsReported() {
	stopProblemsRead = true;
	return stopProblems();
}

std::vector<std::string> ScenarioServers::stopProblems() const {
	std::vector<std::string> problems;
	const auto seconds = [this] { return std::to_string(std::chrono::duration_cast<std::chrono::seconds>(config.stopTimeout).count()); };
	if (gameServerStopped && !gameServerExit)
		problems.push_back("the game server did not exit within " + seconds() + " s after the stop file was written");
	if (gameServerExit && *gameServerExit != 0)
		problems.push_back("the game server exited with code " + std::to_string(*gameServerExit) + " (expected 0)");
	if (loginServerKilled)
		problems.push_back("the login server was killed: Windows refused CTRL_BREAK, so it never shut down in order");
	if (loginServerStopped && !loginServerExit)
		problems.push_back("the login server did not exit within " + seconds() + " s after CTRL_BREAK");
	if (loginServerExit && !loginServerKilled && *loginServerExit != 0)
		problems.push_back("the login server exited with code " + std::to_string(*loginServerExit) + " (expected 0)");
	if (gameServerReady && gs && gs->isRunning())
		problems.push_back("the game server was still running at the end of the run and had to be terminated");
	if (loginServerReady && ls && ls->isRunning())
		problems.push_back("the login server was still running at the end of the run and had to be terminated");
	return problems;
}

std::map<std::string, std::vector<std::string>> ScenarioServers::readSummary() const {
	std::map<std::string, std::vector<std::string>> summary;
	for (const std::string& line : readReportLines("m5a_summary.txt")) {
		size_t space = line.find(' ');
		summary[line.substr(0, space)].push_back(space == std::string::npos ? "" : line.substr(space + 1));
	}
	return summary;
}

std::vector<std::string> ScenarioServers::readReportLines(std::string_view fileName) const {
	std::ifstream in(checkOutputDir() / fileName, std::ios::binary);
	if (!in)
		throw std::runtime_error("cannot read " + (checkOutputDir() / fileName).string());
	std::vector<std::string> lines;
	std::string line;
	while (std::getline(in, line)) {
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		if (line.empty() || line.starts_with('#'))
			continue;
		lines.push_back(line);
	}
	return lines;
}

} // namespace aion::gameserver::scenario
