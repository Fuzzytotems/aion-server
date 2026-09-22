#pragma once

// The stress nightly of m5a-plan.md G-01 (stage 3): N FakeGameClients against one real game server (and one real login server) for a fixed
// wall-clock duration, each of them logging in, entering the world, moving, logging out and logging in again, with DAO exceptions injected into
// the logout path and with every logout happening below full HP.
//
// What it is FOR, i.e. what the M5a gate cannot prove (m5a-plan.md §10.3): the gate's run is one to three minutes and walks the path once, so
// the timed half of the lifetime machinery - the leak census (`gameserver.debug.leak_census_minutes`), the zombie breaker
// (`gameserver.runtime.zombie_break_minutes`) and the stale-pin scan - never fires there, the id factory never wraps a quarantine window, and no
// object ever outlives a second enter world. This run lowers the two thresholds to one and two minutes (see StressOptions) and then leaves the
// server running for half an hour with a few thousand logins, so an object that survives one logout is reported while the run is still going and
// the final census has something to be empty ABOUT.
//
// THE TWO INJECTIONS, and why they are done from the database rather than from the server:
//   - DAO exceptions at logout. The game server runs as a child process (plan D4), so no test seam can reach into its DAOs; what the harness
//     does reach is the schema they write to. `armFault()` inserts the character's id into the helper table `aion_stress_fault`, and two BEFORE
//     UPDATE triggers (installed by `installFaultInjection()`) then make every UPDATE of that character's rows in `players` and
//     `player_life_stats` fail with SQLSTATE 45000 and the message AION_STRESS_FAULT_MARKER. That is a real SQLException out of a real prepared
//     statement inside `PlayerLeaveWorldService::leaveWorld` - five call sites in one logout: `PlayerLifeStatsDAO::updatePlayerLifeStat`
//     (PlayerLeaveWorldService.cpp:129), `PlayerDAO::storePlayer` through `PlayerService::storePlayer` (line 166) and
//     `storeOldCharacterLevel` / `storeLastOnlineTime` / `onlinePlayer(false)` (lines 172-174).
//     Every one of those five catches its exception and logs it, exactly as Java does, so the logout still completes; the run asserts that the
//     marker really appeared in the log (an injection that silently does nothing would otherwise leave every other assertion looking good) and
//     that the character was cleaned up anyway. `onlinePlayer(false)` is the one whose failure is persistent - `players.online` stays 1 and the
//     next CM_ENTER_WORLD would be answered with REENTRY_TIME (PlayerEnterWorldService.cpp:252) - so `disarmFault()` repairs that column, which
//     is the harness undoing its own damage and touches nothing the run measures.
//   - Logouts below full HP. There is no damage packet in the M5a client packet set, so HP is lowered the way §5.7 Q3 does it: the harness
//     halves `player_life_stats.hp` before each enter world. The character then enters with current HP below its maximum, which is what creates
//     `LifeStatsRestoreService::HpMpRestoreTask` - a periodic task that PINS the Player (m5a-plan.md §10.2) - and the logout has to cancel it.
//     A leaked restore task is therefore a leaked Player, and the run asserts both that the tasks were created (so the case was exercised) and
//     that none is alive at the end.
//
// Everything in this header is harness code of chunk P5-SC; it drives the servers through ScenarioServers, FakeLoginClient and GameSession and
// reads their reports, and it contains no assertion of its own: the test case (M5aStressTest.cpp) makes every assertion, so that a failure is
// reported once, at the place that decides.

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ScenarioServers.h"

namespace aion::commons::database {
class Connection;
} // namespace aion::commons::database

namespace aion::gameserver::scenario::stress {

/**
 * The parameters of a run. The defaults are SMALL on purpose, so that a developer who runs the binary by hand gets a 45 second run with two
 * clients; the registered CTest (StressTests.cmake) passes the real numbers of G-01 through the environment.
 */
struct StressOptions {
	/** AION_STRESS_CLIENTS (G-01: 20) */
	int32_t clients = 2;
	/** AION_STRESS_SECONDS, or AION_STRESS_MINUTES * 60 (G-01: 30 minutes) */
	std::chrono::seconds duration{45};
	/** enter world / play / log out rounds per connection; after the last one the client disconnects and logs in from the login server again */
	int32_t roundsPerConnection = 2;
	/** every n-th round of a client arms the DAO fault injection for its logout (0: never) */
	int32_t faultEveryNthRound = 2;
	/** CM_MOVE steps per round */
	int32_t moveSteps = 4;
	/** distance of one CM_MOVE step in metres (small enough that AntiHackService sees a walk) */
	float moveStep = 2.0f;
	std::chrono::milliseconds moveInterval{300};
	/** pause between two connection cycles of one client */
	std::chrono::milliseconds cyclePause{250};
	/**
	 * How long to wait before re-entering the world after CM_QUIT(1). `gameserver.character.reentry.time` is 1 second in the scenario profile
	 * and PlayerEnterWorldService.cpp:152-156 answers SM_ENTER_WORLD_CHECK(REENTRY_TIME) below it.
	 */
	std::chrono::milliseconds reentryPause{1300};
	/** how long a client waits for one server packet before it counts the round as failed */
	std::chrono::milliseconds packetTimeout{60000};

	/** the options with the AION_STRESS_* environment variables applied over the defaults. @throws std::runtime_error on an unusable value */
	static StressOptions fromEnvironment();

	/** the same, over the given values (the environment reader and its test share this) */
	static StressOptions fromValues(const std::map<std::string, std::string, std::less<>>& values);

	std::string describe() const;
};

/** What one client did. Every counter is written by that client's thread alone and read after the threads were joined. */
struct ClientStats {
	int32_t index = 0;
	std::string account;
	std::string characterName;
	int32_t playerId = 0;
	/** login server + game server logins (one per connection cycle) */
	int32_t connections = 0;
	/** CM_ENTER_WORLD rounds that reached SM_ENTER_WORLD_CHECK(0) and SM_PLAYER_SPAWN */
	int32_t enters = 0;
	/** rounds that ended with an SM_QUIT_RESPONSE */
	int32_t logouts = 0;
	/** logouts with the DAO fault injection armed */
	int32_t faultLogouts = 0;
	/** enters whose SM_STATS_INFO reported current HP below max HP */
	int32_t entersBelowMaxHp = 0;
	int32_t moves = 0;
	/** the position of the first SM_PLAYER_SPAWN of this client: every round's walk starts and ends there (StressRun.cpp) */
	float homeX = 0;
	float homeY = 0;
	float homeZ = 0;
	/** the largest distance in x between a later SM_PLAYER_SPAWN and homeX - the run fails if the character wanders off */
	float maxDrift = 0;
	/** what went wrong, one line each (the test fails on any of them) */
	std::vector<std::string> failures;
};

/** The sums over every client, for the report and the assertions */
struct StressTotals {
	int32_t connections = 0;
	int32_t enters = 0;
	int32_t logouts = 0;
	int32_t faultLogouts = 0;
	int32_t entersBelowMaxHp = 0;
	int32_t moves = 0;
	int32_t failures = 0;
};

/**
 * One stress run over servers that are already started. Owns the client threads and the fault injection; makes no assertion (see the header
 * comment) and never throws out of a client thread.
 */
class StressRun {
public:
	/** The message text of the injected SQL failures, so a log line can be attributed to the injection and to nothing else */
	static constexpr std::string_view FAULT_MARKER = "AION_STRESS_FAULT_MARKER";
	/** the helper table whose rows arm the triggers */
	static constexpr std::string_view FAULT_TABLE = "aion_stress_fault";

	StressRun(ScenarioServers& servers, StressOptions options);
	~StressRun();

	StressRun(const StressRun&) = delete;
	StressRun& operator=(const StressRun&) = delete;

	/**
	 * Creates the helper table and the two BEFORE UPDATE triggers of the injection in the game server's schema. Called once, before the clients
	 * start. @throws std::runtime_error if the statements fail
	 */
	void installFaultInjection();

	/** Runs the clients until the duration has passed and joins their threads. @return the wall clock the run took */
	std::chrono::milliseconds run();

	const std::vector<ClientStats>& stats() const noexcept { return clientStats; }
	const StressOptions& options() const noexcept { return options_; }
	StressTotals totals() const;
	/** a table of what every client did, for the test's report */
	std::string report() const;

	/** the account name of client `index` (unique per schema, so parallel build trees do not collide) */
	std::string accountOf(int32_t index) const;
	/** the character name of client `index`: letters only, as NameConfig's character pattern [a-zA-Z]{2,16} demands */
	static std::string characterNameOf(int32_t index);

private:
	class Client;

	ScenarioServers& servers;
	// trailing underscore: a member spelled like a method of its class (CONVENTIONS.md "Keyword, reserved-name and macro rule")
	StressOptions options_;
	std::vector<ClientStats> clientStats;
};

/**
 * The log lines that report an object id being handed out or held twice. G-01 asserts that a run produces none of them, and this is the list it
 * scans for; each entry is a literal of a message that exists in the tree, named with the place that writes it.
 *
 * NOT in this list, because it does not exist: the checked-build reuse warning of C16. `IDFactory::recentlyReleased(id)` (IDFactory.h:35,
 * IDFactory.cpp:276) is documented as the source of "World.storeObject warns when an id released less than an hour ago is reassigned", but
 * nothing calls it - `World::storeObject` (World.cpp:114-132) does not - so no message about a reused id exists to scan for. See the lane report
 * and docs/deviations: the missing call site is in chunk P4-10, not in this chunk's files.
 */
std::vector<std::string> reusedObjectIdWarnings();

/**
 * The ERROR message prefixes a run with the DAO fault injection is allowed to produce, and no others: the five catch blocks the injection
 * reaches in one logout, plus AionConnection::safeLogout's own wrapper. Everything else in a server log is a failure of the run.
 * <p>
 * The prefixes are compared against the whole log line, which starts with the timestamp, so a line "matches" when it CONTAINS one of them.
 */
std::vector<std::string> injectedDaoErrorMessages();

/** true if the log line is one of injectedDaoErrorMessages() */
bool isInjectedDaoError(std::string_view logLine);

} // namespace aion::gameserver::scenario::stress
