#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "aion/gameserver/runtime/lifetime/LiveInstanceCounters.h"
#include "aion/gameserver/runtime/services/LeakCensus.h"

namespace aion::gameserver {

/**
 * C++ only: the report files of aion_game_server's --check-output mode (m5a-plan.md F-02, F-07, D8; the scenario gate §5.7 Q8 reads them).
 * main.cpp writes them into the check output directory:
 * - unported_trace.txt, partial_trace.txt: the AION_UNPORTED and AION_PARTIAL traces (runtime/base/Unported.h)
 * - live_counts_baseline.txt (after "Game server started") and live_counts.txt (after the runtime shut down): runtime/lifetime/LiveInstanceCounters.h
 * - census.txt: the final census after the ShutdownHook logged the players out (runFinalCensus)
 * - lockdep.txt: the lock order validator's reports; watchdog.txt: the watchdog dumps of the run
 * - m5a_summary.txt: "key value" lines (writeSummary)
 * Every writer throws commons::utils::IOException if the file cannot be written.
 */
class CheckOutput {
public:
	CheckOutput() = delete;

	static void writeUnportedTrace(const std::filesystem::path& dir);

	static void writePartialTrace(const std::filesystem::path& dir);

	static void writeLiveCounts(const std::filesystem::path& file);

	/**
	 * The final census (D8): waits until playersLeft() is false or logoutTimeout passed (the logout tasks of the network shutdown run on the
	 * instant pool), drains the instant and long-running pools with the same deadline (drainPools), configures LeakCensus with censusAfter and
	 * checkInterval 0 (zombie breaker off), runs Reclaimer::reclaimNow() twice and writes census.txt: "# final census v1", then one tab
	 * separated line per leak: className, objectId, refCount, the call sites of the pending tasks pinning it ("file:line kind", separated by
	 * "; "). Must run outside any TaskScope while LeakCensus is installed and the thread pools still run (before RuntimeLifecycle::shutdown).
	 * Afterwards it runs runBreakerPass(), so LeakCensus::zombieCutCount() and the stale-pin warnings are meaningful in the run that follows.
	 *
	 * @return the leaks written
	 */
	static std::vector<runtime::LeakCensus::LeakReport> runFinalCensus(const std::filesystem::path& dir, const std::function<bool()>& playersLeft,
		std::chrono::milliseconds logoutTimeout);

	/**
	 * One scan with the zombie breaker on and `zombieBreakAfter`, `stalePinAfter` and `stalePinCheckInterval` at 0, then a pool drain so the
	 * posted breakers ran, then one more reclaim. Runs after census.txt was written, so the census still reports what the breaker then cuts.
	 * <p>
	 * Why (m5a-plan.md §5.7 Q8): the configured thresholds are 30 minutes (zombie breaker) and 10 minutes (stale pins) against a one-to-three
	 * minute gate run, and the final census used to switch the breaker off - so the gate's "zombieCuts 0" and "no stale pin" rows could not fire
	 * whatever the run did. At 0 they apply to what is left at shutdown, which is when a leak of the run is visible. Neither can fire on a clean
	 * run: both only look at objects that left the world and are still referenced, i.e. exactly the objects census.txt reports.
	 */
	static void runBreakerPass();

	/**
	 * Waits (until `deadline`) for one barrier task per pool, so nothing that was queued before it is still pending. Without it a queued logout
	 * task still pins its Player and the census reports it as a leak (m5a-plan.md F-07 "drain the pools").
	 */
	static void drainPools(std::chrono::steady_clock::time_point deadline);

	/** census.txt's format (runFinalCensus) */
	static void writeCensus(std::ostream& out, const std::vector<runtime::LeakCensus::LeakReport>& leaks);

	/** lockdep.txt: "# lockdep reports v1", then each report's text followed by "occurrences N". @return the number of reports */
	static size_t writeLockdepReports(const std::filesystem::path& dir);

	/** watchdog.txt: "# watchdog dumps v1", then one line per dump ("REASON summary") */
	static void writeWatchdogDumps(const std::filesystem::path& dir, const std::vector<std::string>& dumps);

	/**
	 * The classes whose live instance count must be 0 once the runtime shut down (m5a-plan.md D8, §5.7 Q8, §10.2), as qualified names or
	 * trailing parts of one (runtime::liveInstancesOf). Everything here belongs to a character that logged out, to one of its per-session
	 * helpers or to a task of the character; nothing keeps one alive once its Player is gone, so a leftover is a leak of the free-threaded
	 * design (D7).
	 * <p>
	 * Deliberately NOT here:
	 * - `Item`: an account warehouse outlives its characters. See accountBoundedLiveClasses().
	 * - `Npc`, `Gatherable`, `StaticObject`, `House` and the other world objects: the shutdown does not despawn the world, so they are all still
	 *   alive when the reports are written (a gate run measures about 82,000 live Npc). Java keeps them too.
	 * - `Account`, `AccountTime`, `PlayerAccountData`, `PlayerCommonData`, `PlayerAppearance`, `ConnectionAliveChecker` and the `*Storage`
	 *   classes: Java's own shutdown keeps one of each per client that is still connected (AionConnection.java:239-243 returns from onDisconnect
	 *   before LoginServer.onDisconnect, and LoginServer.java:119 is the only place that unregisters the connection). Their bound is the number
	 *   of open connections, which the process cannot know here; the scenario gate checks it, because it knows how many clients it left open.
	 * - Creature-attached observers (`AttackCalcObserver`, `ShieldObserver`): an npc that stays in the world keeps its observers.
	 * <p>
	 * The last seven entries (Summon, Pet, Kisk, AbstractInteractionTask, GatheringTask, GatheringTask_ActionObserver, StanceObserver) are
	 * never created by the M5a scenario, so they cannot fail a gate run today: they are guards for the stress run, the real client and M5b
	 * (m5a-plan.md §10.2).
	 */
	static const std::vector<std::string>& zeroLiveClasses();

	/**
	 * The classes that are checked for 0 only while no `model::account::Account` survived the shutdown (m5a-plan.md §10.1), same name form as
	 * zeroLiveClasses(): currently `model::gameobjects::Item` alone.
	 * <p>
	 * Why Item cannot be a strict zero: a login loads the **account** warehouse (AccountService::loadAccountWarehouse →
	 * InventoryDAO::loadStorage + ItemStoneListDAO::load, AccountService.cpp:98-104, Java AccountService.java:95-100), and the logout only
	 * detaches its owner (`getAccount()->getAccountWarehouse().setOwner(nullptr)`, PlayerLeaveWorldService.cpp:170, Java
	 * PlayerLeaveWorldService.java:146) - the Storage and its Items stay on the Account.
	 * An Account whose connection never reached LoginServer::onDisconnect survives the shutdown (the
	 * same early return that bounds the account-level classes above), and with it that warehouse: every gate report shows exactly one live
	 * `ItemStorage` for it. Its Items are alive for the same, faithful reason. Today the scenario's two accounts have an empty account
	 * warehouse (`0 78 model::gameobjects::Item` in every report), so a strict 0 passes by accident; the first real-client run or M5b scenario
	 * with one item in an account warehouse would fail the gate for correct behaviour.
	 * <p>
	 * The check therefore keeps the strict 0 exactly where it is sound - no surviving Account means no surviving warehouse, so every Item of
	 * the run must be gone - and hands the bounded case to the reader of `live_counts.txt`: the count is still written there, and the scenario
	 * gate, which knows how many clients it left connected and what it put into their warehouses, bounds it like the storages (§5.7 Q8).
	 */
	static const std::vector<std::string>& accountBoundedLiveClasses();

	/**
	 * The live-count leak check: zeroLiveClasses() against the debug live-instance counters, plus accountBoundedLiveClasses() when no Account
	 * survived the shutdown. Logs one ERROR per offending class, so a run with `--check-output` fails the gate's "no ERROR line" assertion even
	 * where nothing reads the counts, and one WARN per class left unchecked because an Account survived.
	 * <p>
	 * **Limitation:** the counters are compiled out in a release build (runtime::LIVE_COUNTS_ENABLED, AION_CHECKED), where this check is a
	 * no-op that reports nothing. It then logs one WARN saying so, and `m5a_summary.txt` carries the `liveCountsEnabled false` row: a gate that
	 * wants the check to mean something must assert that row is `true` (m5a-plan.md §10.2).
	 *
	 * @return the classes that still have live instances, sorted by class name
	 */
	static std::vector<runtime::LiveCount> checkLiveCounts();

	/** The pure form of checkLiveCounts() over a given snapshot (for tests and for a report that was read from a file). */
	static std::vector<runtime::LiveCount> checkLiveCounts(const std::vector<runtime::LiveCount>& counts);

	/** m5a_summary.txt */
	struct Summary {
		int32_t exitCode = 0;
		bool started = false;
		std::optional<bool> atreianPassportDisabled;
		size_t censusLeaks = 0;
		uint64_t zombieCuts = 0;
		size_t lockdepReports = 0;
		uint64_t watchdogDumps = 0;
		std::vector<std::string> notPortedClientPackets;
		/** filled by writeSummary(dir, summary) from checkLiveCounts() */
		std::vector<runtime::LiveCount> liveLeaks;
	};

	/**
	 * Writes m5a_summary.txt: "started", "exitCode", "unportedHits", "partialHits", "partialSites", "censusLeaks", "zombieCuts", "lockdepReports",
	 * "watchdogDumps", "atreianPassportDisabled" (true, false or unknown), one "notPortedClientPacket <class>" line per client packet class
	 * that a client sent without a C++ port (sorted), "liveCountsEnabled" (false in a release build, where checkLiveCounts() cannot see
	 * anything), "liveLeaks <n>" and one "liveLeak <class> <live>" line per class of checkLiveCounts().
	 * <p>
	 * The dir form runs checkLiveCounts() itself when `summary.started` is set, i.e. only after a run mode that reached the final census: on a
	 * startup that never ran, every object of the run is still alive and the check would report the world.
	 */
	static void writeSummary(const std::filesystem::path& dir, const Summary& summary);

	static void writeSummary(std::ostream& out, const Summary& summary);
};

} // namespace aion::gameserver
