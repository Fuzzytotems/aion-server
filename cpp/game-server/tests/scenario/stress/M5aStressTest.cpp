// The M5a stress nightly (m5a-plan.md G-01, §11 second row; registered as gs.scenario.m5a_stress by tests/scenario/stress/StressTests.cmake):
// twenty FakeGameClients against one real game server and one real login server for thirty minutes, each of them logging in, entering the world,
// moving, logging out and logging in again, with SQL failures injected into the logout path and with every logout below full HP.
//
// WHAT THIS PROVES THAT THE GATE CANNOT (m5a-plan.md §10.3). `gs.scenario.m5a` walks the scripted path once in one to three minutes, so the
// TIMED half of the lifetime machinery never runs there: the leak census threshold is 10 minutes, the zombie breaker 30 and the stale-pin scan
// 10, and the gate's own "census empty / zombieCuts 0" rows are therefore statements about a final scan with the thresholds forced to 0, not
// about a server that ran. Here the thresholds are ONE and TWO minutes against a thirty minute run, so the census reports a leaked object while
// the run is still going, the periodic scan and the breaker really fire, and the same assertions mean "nothing survived its logout for a minute
// at any point in half an hour".
//
// THE THRESHOLD KEYS ARE NOT THE ONES THE PLAN NAMES. G-01 asks for `-Dgameserver.debug.leak_census_seconds=60` and
// `-Dgameserver.runtime.zombie_break_seconds=120`; neither key exists. They were a header request against RuntimeConfig.h (header-requests.md
// row 5a-pre-8, "deferred to stage 3 (G-01)") and RuntimeConfig.h is a frozen spine header of chunk P4-01, which this lane does not own. What
// the runtime actually reads is `gameserver.runtime.zombie_break_minutes` and `gameserver.debug.leak_census_minutes` (RuntimeConfig.cpp:13-14,
// bound as integer minutes and turned into the LeakCensus::Config in RuntimeConfig::leakCensusConfig()), and 60 s and 120 s ARE 1 and 2 whole
// minutes - so the run asks for exactly the thresholds G-01 wanted through the keys that exist, and no key is invented. See the lane report.
//
// It is one GoogleTest case because it owns one pair of server processes, and it is registered under its own label "stress" so that a default
// ctest never starts it; the discovered case is disabled (StressTests.cmake), exactly as the two gates of M5aScenarioTest.cpp are.

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "ScenarioServers.h"
#include "stress/StressRun.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::scenario::stress {
namespace {

using namespace std::chrono_literals;

/**
 * §5.7 Q8 / §10.2: the classes whose live instance count must be 0 when the reports are written. This run drains every client before the stop
 * file, so no Account survives, which is what makes the strict 0 sound for `Item` as well (m5a-plan.md §10.1: an Account that survives the
 * shutdown legitimately keeps its account warehouse and the items in it).
 */
const std::set<std::string>& strictlyZeroLiveClasses() {
	static const std::set<std::string> classes = {
		"Player", "AbyssRank", "Item", "Account",
		// the restore task of a character that logged out below full HP - the pin this run creates thousands of
		"HpMpRestoreTask",
		"KnownObject", "PlayerSkillList", "PlayerSkillEntry", "QuestEnv",
	};
	return classes;
}

/** the classes whose "created" column proves that the run exercised what it claims to */
const std::set<std::string>& mustHaveBeenCreated() {
	static const std::set<std::string> classes = {"Player", "HpMpRestoreTask"};
	return classes;
}

struct LiveCount {
	int64_t live = 0;
	int64_t created = 0;
	std::string line;
};

/**
 * How many matching log lines the scans below read at most. It is deliberately far above what a thirty minute run produces (a measured 90 second
 * run with four clients wrote 340 ERROR lines, all of them the injection's): the scan for ERROR lines the injection did NOT cause must see every
 * ERROR line of the run, and findLogLines stops at its limit - a cap of a few hundred would hide the one unexpected error behind the thousands
 * of expected ones.
 */
constexpr size_t LOG_SCAN_LIMIT = 200000;

/** the rows of live_counts.txt ("<live>\t<created>\t<qualified class>") by their unqualified class name */
std::vector<std::pair<std::string, LiveCount>> readLiveCounts(const ScenarioServers& servers, std::string_view fileName) {
	std::vector<std::pair<std::string, LiveCount>> counts;
	for (const std::string& line : servers.readReportLines(fileName)) {
		const size_t firstTab = line.find('\t');
		const size_t lastTab = line.rfind('\t');
		if (firstTab == std::string::npos || lastTab == firstTab)
			continue;
		const std::string qualified = line.substr(lastTab + 1);
		const size_t colons = qualified.rfind("::");
		LiveCount count;
		count.line = line;
		try {
			count.live = std::stoll(line.substr(0, firstTab));
			count.created = std::stoll(line.substr(firstTab + 1, lastTab - firstTab - 1));
		} catch (const std::exception&) {
			continue;
		}
		counts.emplace_back(colons == std::string::npos ? qualified : qualified.substr(colons + 2), count);
	}
	return counts;
}

std::string join(const std::vector<std::string>& values, std::string_view separator = "\n") {
	std::string text;
	for (size_t i = 0; i < values.size(); i++) {
		if (i > 0)
			text += separator;
		text += values[i];
	}
	return text;
}

/** the value of a "key value" row of m5a_summary.txt */
std::string summaryValue(const std::map<std::string, std::vector<std::string>>& summary, std::string_view key) {
	const auto found = summary.find(std::string(key));
	if (found == summary.end() || found->second.empty())
		return "(missing)";
	return found->second[0];
}

} // namespace

TEST(M5aStress, Run) {
	// A skipped run is not a passed run (wave A's policy, ScenarioTests.cmake): with AION_SCENARIO_REQUIRE set - which the registration passes
	// unless the tree was configured with -DAION_GS_ALLOW_MILESTONE_SKIP=ON - a missing database URL fails instead of skipping.
	const char* requireEnvironment = std::getenv("AION_SCENARIO_REQUIRE");
	const bool required = requireEnvironment != nullptr && *requireEnvironment != '\0' && std::string_view(requireEnvironment) != "0";
	const auto unavailable = [&](std::string_view reason) {
		if (required)
			ADD_FAILURE() << "gs.scenario.m5a_stress was not configured and AION_SCENARIO_REQUIRE is set: " << reason;
		else
			GTEST_SKIP() << "gs.scenario.m5a_stress: skipped (" << reason << ")";
	};

	std::optional<ScenarioEnvironment> environment = ScenarioEnvironment::fromEnvironment();
	if (!environment) {
		unavailable("set AION_TEST_GS_DATABASE_URL and AION_TEST_LS_DATABASE_URL");
		return;
	}

	StressOptions options;
	try {
		options = StressOptions::fromEnvironment();
	} catch (const std::exception& exception) {
		FAIL() << "the stress options are unusable: " << exception.what();
	}
	std::cout << "gs.scenario.m5a_stress: " << options.describe() << std::endl;

	// its own output directory, so its schema pair (derived from that directory by ScenarioServers), its --log-folder, its check output and its
	// two server logs are its own and never collide with gs.scenario.m5a or gs.scenario.m5a_geo
	const std::filesystem::path outputDir = std::filesystem::path(AION_SCENARIO_OUTPUT_DIR) / "m5a_stress";
	ScenarioServers::Config config;
	config.gameServerExecutable = AION_GAME_SERVER_EXECUTABLE;
	config.loginServerExecutable = AION_LOGIN_SERVER_EXECUTABLE;
	config.gameServerJavaDir = AION_GAMESERVER_JAVA_DIR;
	config.loginServerJavaDir = AION_LOGINSERVER_JAVA_DIR;
	config.outputDir = outputDir;
	config.startupTimeout = 10min;
	config.stopTimeout = 5min;
	// The thresholds of G-01 through the keys the runtime really reads (see the file header): a leak census every minute and a zombie breaker
	// after two, so both fire DURING the run instead of only in the final scan with the thresholds forced to 0.
	config.gameServerProperties["gameserver.debug.leak_census_minutes"] = "1";
	config.gameServerProperties["gameserver.runtime.zombie_break_minutes"] = "2";
	ScenarioServers servers(config, *environment);

	bool started = false;
	try {
		servers.createSchemas();
		servers.startLoginServer();
		servers.startGameServer();
		started = true;
	} catch (const std::exception& exception) {
		ADD_FAILURE() << "the servers did not start: " << exception.what();
	}

	std::chrono::milliseconds wallClock{0};
	StressTotals totals;
	if (started) {
		StressRun run(servers, options);
		try {
			run.installFaultInjection();
		} catch (const std::exception& exception) {
			ADD_FAILURE() << "the DAO fault injection could not be installed: " << exception.what();
		}
		wallClock = run.run();
		totals = run.totals();
		std::cout << run.report() << "the clients ran for " << wallClock.count() / 1000 << " s" << std::endl;

		// ---- what the clients saw ----
		for (const ClientStats& client : run.stats())
			EXPECT_TRUE(client.failures.empty()) << "client " << client.characterName << " failed:\n  " << join(client.failures, "\n  ");
		// The characters must stay where the npcs are. A logout stores the position and the next enter world resumes there, so a walk that always
		// goes the same way adds up over hundreds of rounds: the first thirty minute run carried every character 5.4 km east, out of the
		// populated part of Poeta after the first minute and past the edge of the region grid after 22 ("New MapRegion for Player ... doesn't
		// exist at coordinates", 40 times) - a run that still measured logout lifetimes but had stopped measuring visibility churn. The walk now
		// alternates direction, so a spawn may differ from the first one by at most one round's walk.
		const float allowedDrift = 3.0f * static_cast<float>(run.options().moveSteps) * run.options().moveStep;
		for (const ClientStats& client : run.stats()) {
			EXPECT_GT(client.enters, 0) << "client " << client.characterName << " never entered the world";
			EXPECT_GT(client.logouts, 0) << "client " << client.characterName << " never logged out";
			EXPECT_LE(client.maxDrift, allowedDrift)
				<< "client " << client.characterName << " drifted " << client.maxDrift << " m from its first spawn point (x " << client.homeX
				<< "): the walk is not returning, so the later rounds happened away from the npcs this run is supposed to churn through";
		}
		EXPECT_GE(wallClock, options.duration) << "the clients stopped before the requested duration";
		// the run must really have churned: an enter world per client per round for the whole duration
		EXPECT_GT(totals.enters, options.clients) << "fewer enter worlds than clients: the run did not loop";
		EXPECT_EQ(totals.logouts, totals.enters) << "an enter world without a logout";
		// G-01's "logouts with HP below maximum": only the very first enter of a character finds no player_life_stats row to halve
		EXPECT_GE(totals.entersBelowMaxHp, totals.enters - options.clients)
			<< "enters below full HP: " << totals.entersBelowMaxHp << " of " << totals.enters << " enters, with " << options.clients
			<< " first enters at full HP by construction";
		// the injection arms every n-th enter world of a client, so a run with fewer enters than n could not have reached it
		if (options.faultEveryNthRound > 0 && totals.enters >= options.faultEveryNthRound)
			EXPECT_GT(totals.faultLogouts, 0) << "G-01's injected DAO exceptions: no logout ran with the injection armed";
	}

	// ---- stop the servers and read the reports ----
	// Every client closed its socket, but the server notices a close on its own thread: the live-count rows below demand 0 Accounts, and
	// AionConnection::onDisconnect -> LoginServer::onDisconnect is what releases the last one (m5a-plan.md §10.1). Without this pause the last
	// client's disconnect could still be in flight when the stop file is written, and the run would report a leak that is only a race with
	// itself.
	if (started)
		std::this_thread::sleep_for(10s);
	const std::optional<int32_t> gameServerExit = servers.stopGameServer();
	const std::optional<int32_t> loginServerExit = servers.stopLoginServer();
	// EXPECT and not ASSERT, here and below: an ASSERT returns from the test, and everything after it - the reports, the log scans and the drop
	// of the two schemas at the end - would be skipped, so one failure would cost the run its evidence and leave a schema pair behind
	EXPECT_TRUE(gameServerExit) << "the game server did not exit after the stop file was written";
	if (gameServerExit)
		EXPECT_EQ(*gameServerExit, 0) << "the game server exited with " << *gameServerExit;
	EXPECT_TRUE(loginServerExit) << "the login server did not exit on CTRL_BREAK";

	const bool reportsWritten = std::filesystem::is_regular_file(servers.checkOutputDir() / "m5a_summary.txt");
	if (started && !reportsWritten)
		ADD_FAILURE() << "the game server wrote no check output in " << servers.checkOutputDir();
	if (started && reportsWritten && servers.gameServer() != nullptr) {
		const std::map<std::string, std::vector<std::string>> summary = servers.readSummary();

		// G-01: the final census is empty. With leak_census_minutes at 1 this is not only the final scan's verdict: an object that survived its
		// logout by a minute at any time in the run was already reported as a leak while the run was going.
		EXPECT_TRUE(servers.readReportLines("census.txt").empty())
			<< "the final census reports leaks:\n" << join(servers.readReportLines("census.txt"));
		// G-01: the zombie breaker cut nothing. With zombie_break_minutes at 2 the breaker really ran during the half hour, so a cut here is a
		// missing cycle breaker rather than an unreachable row (m5a-plan.md §10.3).
		EXPECT_EQ(summaryValue(summary, "zombieCuts"), "0") << "the zombie breaker cut references during the run";
		EXPECT_EQ(summaryValue(summary, "liveLeaks"), "0") << "the server's own live-count check found leaks; see the liveLeak rows of "
		                                                   << (servers.checkOutputDir() / "m5a_summary.txt");
		// §10.2: in a release build the counters are compiled out and "liveLeaks 0" means "not measured"
		EXPECT_EQ(summaryValue(summary, "liveCountsEnabled"), "true")
			<< "the game server reports liveCountsEnabled false, so its live-count rows measured nothing: build it checked";
		EXPECT_EQ(summaryValue(summary, "started"), "true");
		EXPECT_EQ(summaryValue(summary, "exitCode"), "0");
		EXPECT_EQ(summaryValue(summary, "knownListNotifyFailures"), "0") << "KnownList swallowed notification exceptions";
		EXPECT_TRUE(servers.readReportLines("unported_trace.txt").empty())
			<< "AION_UNPORTED sites were reached:\n" << join(servers.readReportLines("unported_trace.txt"));
		EXPECT_TRUE(servers.readReportLines("lockdep.txt").empty())
			<< "the lock order validator reported:\n" << join(servers.readReportLines("lockdep.txt"));
		EXPECT_TRUE(servers.readReportLines("watchdog.txt").empty())
			<< "the watchdog dumped:\n" << join(servers.readReportLines("watchdog.txt"));

		// THE TWO THRESHOLD KEYS MUST EXIST. This run's whole reason to be is that the leak census and the zombie breaker fire DURING it
		// (see the file header), and both are asked for with a -D key. A key the server does not know is silently ignored - Config.cpp:158
		// logs "Config property <key> is unknown and therefore ignored." and carries on with the 10 and 30 minute defaults, against which a
		// thirty minute run proves nothing. This is also the guard against the plan's own `..._seconds` keys, which do not exist, creeping
		// back in: the run would look identical and mean nothing.
		const std::vector<std::string> unknownProperties = servers.gameServer()->findLogLines("is unknown and therefore ignored", 50);
		EXPECT_TRUE(unknownProperties.empty()) << "the game server ignored -D keys, so it ran with defaults instead of what this run asked for:\n"
											   << join(unknownProperties);

		// ---- live_counts.txt ----
		std::set<std::string> seen;
		std::cout << "live instance counts of the classes this run asserts (live / created / class):\n";
		for (const auto& [name, count] : readLiveCounts(servers, "live_counts.txt")) {
			if (!strictlyZeroLiveClasses().contains(name))
				continue;
			seen.insert(name);
			std::cout << "  " << count.line << "\n";
			EXPECT_EQ(count.live, 0) << "live instances left after " << totals.logouts << " logouts: " << count.line;
			if (mustHaveBeenCreated().contains(name))
				EXPECT_GT(count.created, 0) << "nothing of this class was ever created, so its 0 proves nothing: " << count.line;
		}
		std::cout << "census.txt: " << servers.readReportLines("census.txt").size() << " leak(s); m5a_summary.txt: zombieCuts "
		          << summaryValue(summary, "zombieCuts") << ", liveLeaks " << summaryValue(summary, "liveLeaks") << ", liveCountsEnabled "
		          << summaryValue(summary, "liveCountsEnabled") << ", exitCode " << summaryValue(summary, "exitCode") << ", unportedHits "
		          << summaryValue(summary, "unportedHits") << std::endl;
		for (const std::string& name : mustHaveBeenCreated())
			EXPECT_TRUE(seen.contains(name)) << "live_counts.txt has no row for " << name
											 << ", so the run cannot show that it created and released any";

		// ---- the logs ----
		// G-01: no reused object id. See reusedObjectIdWarnings() for what exists to scan for and what does not.
		std::vector<std::string> reused;
		for (const std::string& pattern : reusedObjectIdWarnings()) {
			for (const std::string& line : servers.gameServer()->findLogLines(pattern, 5))
				reused.push_back("game server: " + line);
			if (servers.loginServer() != nullptr)
				for (const std::string& line : servers.loginServer()->findLogLines(pattern, 5))
					reused.push_back("login server: " + line);
		}
		EXPECT_TRUE(reused.empty()) << "object ids were reused or held twice:\n" << join(reused);

		EXPECT_TRUE(servers.gameServer()->findLogLines("did not leave world cleanly", 5).empty())
			<< "objects did not leave the world cleanly:\n" << join(servers.gameServer()->findLogLines("did not leave world cleanly", 5));
		EXPECT_TRUE(servers.gameServer()->findLogLines("stale pin", 5).empty())
			<< "stale pins (a periodic task still pins an object that left the world):\n"
			<< join(servers.gameServer()->findLogLines("stale pin", 5));

		// THE TRANSIENT LEAK, which is the whole reason leak_census_minutes is 1 here. The final census, liveLeaks and the live counts all speak
		// about the END of the run, so an object that outlived its logout by a minute and was then reclaimed leaves every one of them green. The
		// periodic census is what sees it, and it says so in the log (LeakCensus.cpp: "Leak census: <class> (object id N) is still alive ..."),
		// as does the breaker it arms ("Zombie breaker: ..."). Without this pair the run can watch dozens of leaked Players go by and pass.
		for (const char* pattern : {"Leak census:", "Zombie breaker:"}) {
			const std::vector<std::string> reported = servers.gameServer()->findLogLines(pattern, 20);
			EXPECT_TRUE(reported.empty()) << "the game server's own periodic census reported while the run was going (" << pattern
			                              << "), so an object survived its logout during the run even though the final scan is clean:\n"
			                              << join(reported);
		}

		// The injection must have FIRED: without this row every assertion above would still pass with a trigger that was never armed, and the
		// run would silently be an ordinary load test. The marker is part of the DAO's own message
		// (PlayerLifeStatsDAO.cpp:68 appends e.what()), so finding it proves a real SQLException came out of a real prepared statement.
		const std::vector<std::string> injected = servers.gameServer()->findLogLines(StressRun::FAULT_MARKER, LOG_SCAN_LIMIT);
		if (options.faultEveryNthRound > 0 && totals.faultLogouts > 0) {
			EXPECT_FALSE(injected.empty()) << "no injected DAO failure reached the server log, so the fault injection did nothing";
			std::cout << "injected DAO failures that reached the log: " << injected.size() << "\n  first: "
			          << (injected.empty() ? std::string() : injected.front()) << std::endl;
		}

		// Every ERROR line must be one of the injection's own, and nothing else. The gate's bar is "no ERROR line at all"; this run deliberately
		// produces some, so it keeps the bar for everything it did not cause.
		std::vector<std::string> unexpected;
		for (const std::string& line : servers.gameServer()->findLogLines(" ERROR ", LOG_SCAN_LIMIT))
			if (!isInjectedDaoError(line))
				unexpected.push_back("game server: " + line);
		if (servers.loginServer() != nullptr)
			for (const std::string& line : servers.loginServer()->findLogLines(" ERROR ", LOG_SCAN_LIMIT))
				unexpected.push_back("login server: " + line);
		EXPECT_TRUE(unexpected.empty()) << "ERROR lines that the DAO fault injection did not cause:\n" << join(unexpected);

		const std::filesystem::path errorLog = servers.logFolder() / "server_errors.log";
		if (std::filesystem::exists(errorLog)) {
			// the loggers with additivity="false" never reach the console the harness captures, so the server's own error file is read as well
			std::vector<std::string> fileErrors;
			std::ifstream in(errorLog, std::ios::binary);
			std::string line;
			while (std::getline(in, line) && fileErrors.size() < 50) {
				if (!line.empty() && line.back() == '\r')
					line.pop_back();
				if (!line.empty() && line.find(" ERROR ") != std::string::npos && !isInjectedDaoError(line))
					fileErrors.push_back(line);
			}
			EXPECT_TRUE(fileErrors.empty()) << errorLog << " has ERROR lines the fault injection did not cause:\n" << join(fileErrors);
		} else {
			ADD_FAILURE() << "the game server wrote no " << errorLog << " (is --log-folder still passed?)";
		}
	}

	// the stop problems are the net under the exit codes above; taken here, while the verdict still counts (the destructor runs afterwards)
	for (const std::string& problem : servers.stopProblemsReported())
		ADD_FAILURE() << "gs.scenario.m5a_stress: " << problem;

	const bool failed = ::testing::Test::HasFailure();
	if (failed)
		std::cout << "gs.scenario.m5a_stress failed.\n"
		          << "logs: " << (outputDir / "game_server.log") << ", " << (outputDir / "login_server.log") << "\n"
		          << "reports: " << servers.checkOutputDir() << std::endl;
	if (failed && ScenarioServers::keepSchemasOnFailure()) {
		std::cout << "the schemas " << servers.gameSchema() << " and " << servers.loginSchema()
		          << " were kept for the post mortem (AION_SCENARIO_KEEP_SCHEMAS is set)" << std::endl;
		return;
	}
	try {
		servers.dropSchemas();
		if (failed)
			std::cout << "the schemas " << servers.gameSchema() << " and " << servers.loginSchema()
			          << " were dropped; set AION_SCENARIO_KEEP_SCHEMAS=1 and run again to keep them" << std::endl;
	} catch (const std::exception& exception) {
		std::cout << "the schemas could not be dropped (" << exception.what() << ")" << std::endl;
	}
}

} // namespace aion::gameserver::scenario::stress
