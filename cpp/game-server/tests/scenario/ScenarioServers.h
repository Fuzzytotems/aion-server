#pragma once

// ScenarioServers (m5a-plan.md F-04, §5.1): the login server and game server child processes of one scenario run. It creates the test schemas
// (aion_ls_test_m5a_<suffix>, aion_gs_test_m5a_<suffix>) from the Java SQL scripts, registers game server 1 in the login server database,
// picks free ports, starts aion_login_server in its own process group and aion_game_server with the M5a profile, a stop file and a check
// output directory, waits for both to be ready, and stops them in order: the stop file ends the game server (its ShutdownHook), CTRL_BREAK the
// login server; both exits are awaited before their logs and the game server's report files are read.
//
// Three things make a run survivable for everything around it:
//   - Neither child writes a shared log directory: the game server gets --log-folder, and the login server - which has no such argument - gets
//     a working directory of its own with a copy of its config (Logging::init archives and DELETES the log files it finds).
//   - Both schemas carry an in-use marker (SchemaLease) for the whole run, and createSchemas() drops the schemas of runs that were killed
//     before they could drop their own. A CTest TIMEOUT runs no destructor; the marker is a session lock, so it dies with the process.
//   - stopProblems() collects what went wrong while stopping, and the destructor reports the list as a test failure unless a caller took
//     responsibility for it with stopProblemsReported(), so no run passes with a hung game server or a killed login server.
//   - A run drops its two schemas at the end, a failed one as well unless AION_SCENARIO_KEEP_SCHEMAS asks for a post mortem (dropSchemas()).

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ChildProcess.h"
#include "ScenarioDatabase.h"

namespace aion::gameserver::scenario {

class ScenarioServers {
public:
	struct Config {
		std::filesystem::path gameServerExecutable;
		std::filesystem::path loginServerExecutable;
		/** the Java module directories (working directories of the servers, SQL scripts, config/network/database.properties) */
		std::filesystem::path gameServerJavaDir;
		std::filesystem::path loginServerJavaDir;
		/** logs, the stop file and the check output go below this directory */
		std::filesystem::path outputDir;
		/** seed of the schema name suffix (default: the output directory) */
		std::string schemaSeed;
		/** C++ test seam: false skips the check that the client port accepts connections (the stub game server has no port) */
		bool checkClientPort = true;
		/** C++ test seam: arguments put before the game server arguments (the stub game server: "-P", script, "--") */
		std::vector<std::string> gameServerLeadingArguments;
		/** -D keys of the game server on top of the M5a profile (key -> value) */
		std::map<std::string, std::string> gameServerProperties;
		std::chrono::milliseconds startupTimeout{std::chrono::minutes(10)};
		std::chrono::milliseconds stopTimeout{std::chrono::minutes(3)};
	};

	/** The D1 profile and the scenario keys of m5a-plan.md §2 (-D key=value) */
	static std::map<std::string, std::string> m5aProfile();

	/** A port that was free when it was checked (bound to 127.0.0.1:0 and released) */
	static uint16_t freePort();

	/**
	 * `count` distinct ports that were free when they were checked. All acceptors are held open until every port is known, so two calls in a row
	 * cannot be given the same ephemeral port; they are released together. `freePort()` on its own does not guarantee that.
	 */
	static std::vector<uint16_t> reservePorts(size_t count);

	ScenarioServers(Config config, ScenarioEnvironment environment);
	~ScenarioServers();

	ScenarioServers(const ScenarioServers&) = delete;
	ScenarioServers& operator=(const ScenarioServers&) = delete;

	/**
	 * Takes the in-use markers of both schemas, drops the schemas of earlier runs that were killed before they could drop their own, recreates
	 * both schemas and inserts gameservers(id 1, mask 127.0.0.1, password 1234).
	 *
	 * @throws std::runtime_error if another scenario run holds the in-use marker of one of the two schemas (same output directory)
	 */
	void createSchemas();

	/** How long a scenario schema must have been untouched before dropAbandonedSchemas() may drop it (see there; a gate run has TIMEOUT 900) */
	static constexpr std::chrono::minutes ABANDONED_SCHEMA_AGE{60};

	/**
	 * Drops both test schemas and releases their in-use markers. Safe to call more than once and when createSchemas() never ran.
	 * <p>
	 * A gate calls this at the end of a run. Before stage 3 only a **passed** run did, so every failed run left its two schemas in MariaDB: the
	 * next run of the same gate recreates them (the schema name is a hash of the output directory, so it is the same pair every time) and
	 * createSchemas() sweeps the ones an hour old, but neither happens if the gate is never run again - and with a second gate the tree now
	 * leaves four. A failed run therefore drops them too unless keepSchemasOnFailure() says otherwise.
	 */
	void dropSchemas();

	/**
	 * Whether a FAILED run keeps its two schemas for a post mortem: true when the environment variable AION_SCENARIO_KEEP_SCHEMAS is set to
	 * anything but "0" or "".
	 * <p>
	 * The default is to drop them. What a post mortem actually reads is the run's own report files and the two server logs, which a failed run
	 * keeps and names; the database rows are worth a repeat run with this variable set, and are not worth leaving a schema per failed run of
	 * every build tree behind in MariaDB. The gate prints the variable in its failure message, so the way back is one environment variable away.
	 */
	static bool keepSchemasOnFailure();

	/** Starts the login server and waits until it listens for clients and game servers */
	void startLoginServer();

	/** Starts the game server and waits for "Game server started" and the authenticated login server link ("Gameserver #1 is now online") */
	void startGameServer();

	/** @return true if the game server's client port accepts a connection within the timeout (§5.1 "Readiness") */
	bool waitForClientPort(std::chrono::milliseconds timeout) const;

	/** Writes the stop file and waits for the game server's exit. @return its exit code, std::nullopt if it did not exit */
	std::optional<int32_t> stopGameServer();

	/** CTRL_BREAK to the login server (terminated if Windows refuses the event) and waits for its exit. @return its exit code */
	std::optional<int32_t> stopLoginServer();

	/**
	 * What went wrong while the servers were stopped, in words: a server that did not exit within `stopTimeout`, a non-zero exit code, a login
	 * server that had to be **killed** because Windows refused CTRL_BREAK (exit code 98), and a server that was still running when the harness
	 * was destroyed and had to be terminated. Empty when both children shut down in order.
	 * <p>
	 * A list that nobody reports is reported by the destructor as a GoogleTest failure, so a run that never checks the exit codes still cannot
	 * pass with a killed or hung server (stage 2 review: "nothing checks the game server's exit code when case 7 does not run, and a forced
	 * login-server kill counts as success").
	 * <p>
	 * This reader is **pure**: it neither latches nor clears anything, so every caller sees the same list and a diagnostic read (a failure
	 * message, a log line) cannot silence the destructor for the caller that would have reported it. Whoever takes responsibility for reporting
	 * says so with stopProblemsReported(); until then the destructor keeps the last word. It used to latch on read, which meant that the first
	 * reader - including one that only printed the list - turned the destructor's safety net off for everybody.
	 */
	std::vector<std::string> stopProblems() const;

	/**
	 * Declares that the caller has reported stopProblems() itself, so that the destructor does not report them a second time.
	 *
	 * @return the problems, for `const std::vector<std::string> problems = servers.stopProblemsReported();`
	 */
	std::vector<std::string> stopProblemsReported();

	/** true while nobody has called stopProblemsReported(), i.e. while the destructor would still report a non-empty list */
	bool stopProblemsUnreported() const noexcept { return !stopProblemsRead; }

	uint16_t loginClientPort() const noexcept { return loginPort; }
	uint16_t loginGameServerPort() const noexcept { return gameServerLinkPort; }
	uint16_t gameClientPort() const noexcept { return gamePort; }
	const std::string& loginSchema() const noexcept { return lsSchema; }
	const std::string& gameSchema() const noexcept { return gsSchema; }
	const ScenarioDatabase& loginDatabase() const noexcept { return lsDatabase; }
	const ScenarioDatabase& gameDatabase() const noexcept { return gsDatabase; }
	std::filesystem::path stopFile() const { return config.outputDir / "stop"; }
	std::filesystem::path checkOutputDir() const { return config.outputDir / "check"; }
	/** the game server's own log directory (main.cpp --log-folder), so the gate never writes the shared game-server/log */
	std::filesystem::path logFolder() const { return config.outputDir / "gs_log"; }
	/**
	 * The login server's working directory: a copy of its `config` directory, made by startLoginServer(). The login server has no
	 * `--log-folder` of its own and resolves `./config` and `./log` against its working directory, so running it in the Java module directory
	 * would write the shared `login-server/log` - which `Logging::init` archives and DELETES at startup.
	 */
	std::filesystem::path loginServerWorkingDirectory() const { return config.outputDir / "ls_run"; }
	/** the login server's own log directory (Logging::Config::logFolder is "log", relative to its working directory) */
	std::filesystem::path loginLogFolder() const { return loginServerWorkingDirectory() / "log"; }
	ChildProcess* loginServer() noexcept { return ls.get(); }
	ChildProcess* gameServer() noexcept { return gs.get(); }

	/** the login server arguments (§5.1 "LS child") */
	std::vector<std::string> loginServerArguments() const;

	/** the game server arguments (§5.1 "GS child") */
	std::vector<std::string> gameServerArguments() const;

	/** a report of the check output ("key value" lines of m5a_summary.txt) */
	std::map<std::string, std::vector<std::string>> readSummary() const;

	/** the lines of a check output file without its "# ..." header lines */
	std::vector<std::string> readReportLines(std::string_view fileName) const;

private:
	/** Copies the login server's `config` directory into loginServerWorkingDirectory(), so its `./log` is the run's own */
	void prepareLoginServerDirectory() const;

	Config config;
	ScenarioEnvironment environment;
	ScenarioDatabase lsDatabase;
	ScenarioDatabase gsDatabase;
	std::string lsSchema;
	std::string gsSchema;
	/** the in-use markers of the two schemas, held for the whole run (SchemaLease) */
	SchemaLease lsLease;
	SchemaLease gsLease;
	uint16_t loginPort = 0;
	uint16_t gameServerLinkPort = 0;
	uint16_t gamePort = 0;
	std::unique_ptr<ChildProcess> ls;
	std::unique_ptr<ChildProcess> gs;
	/** the stop sequence, for stopProblems() */
	bool gameServerReady = false;
	bool loginServerReady = false;
	bool gameServerStopped = false;
	bool loginServerStopped = false;
	bool loginServerKilled = false;
	std::optional<int32_t> gameServerExit;
	std::optional<int32_t> loginServerExit;
	bool stopProblemsRead = false;
};

} // namespace aion::gameserver::scenario
