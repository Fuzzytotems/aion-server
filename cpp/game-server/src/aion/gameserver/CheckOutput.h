#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

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
	 *
	 * @return the leaks written
	 */
	static std::vector<runtime::LeakCensus::LeakReport> runFinalCensus(const std::filesystem::path& dir, const std::function<bool()>& playersLeft,
		std::chrono::milliseconds logoutTimeout);

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
	};

	/**
	 * Writes m5a_summary.txt: "started", "exitCode", "unportedHits", "partialHits", "partialSites", "censusLeaks", "zombieCuts", "lockdepReports",
	 * "watchdogDumps", "atreianPassportDisabled" (true, false or unknown) and one "notPortedClientPacket <class>" line per client packet class
	 * that a client sent without a C++ port (sorted).
	 */
	static void writeSummary(const std::filesystem::path& dir, const Summary& summary);

	static void writeSummary(std::ostream& out, const Summary& summary);
};

} // namespace aion::gameserver
