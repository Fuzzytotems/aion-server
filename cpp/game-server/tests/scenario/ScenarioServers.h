#pragma once

// ScenarioServers (m5a-plan.md F-04, §5.1): the login server and game server child processes of one scenario run. It creates the test schemas
// (aion_ls_test_m5a_<suffix>, aion_gs_test_m5a_<suffix>) from the Java SQL scripts, registers game server 1 in the login server database,
// picks free ports, starts aion_login_server in its own process group and aion_game_server with the M5a profile, a stop file and a check
// output directory, waits for both to be ready, and stops them in order: the stop file ends the game server (its ShutdownHook), CTRL_BREAK the
// login server; both exits are awaited before their logs and the game server's report files are read.

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

	/** Recreates both schemas and inserts gameservers(id 1, mask 127.0.0.1, password 1234) */
	void createSchemas();

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
	Config config;
	ScenarioEnvironment environment;
	ScenarioDatabase lsDatabase;
	ScenarioDatabase gsDatabase;
	std::string lsSchema;
	std::string gsSchema;
	uint16_t loginPort = 0;
	uint16_t gameServerLinkPort = 0;
	uint16_t gamePort = 0;
	std::unique_ptr<ChildProcess> ls;
	std::unique_ptr<ChildProcess> gs;
};

} // namespace aion::gameserver::scenario
