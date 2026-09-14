#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "aion/gameserver/runtime/base/YieldPoint.h"

namespace aion::gameserver::runtime::pct {

/**
 * Probabilistic Concurrency Testing scheduler for kernel tests (design §12.4 item 1; Burckhardt et al., "A Randomized Scheduler with
 * Probabilistic Guarantees of Finding Bugs", ASPLOS 2010).
 *
 * run() executes the given thread bodies on real threads but lets exactly one of them run at a time. Control changes only at yield points
 * (AION_YIELD_POINT in kernel operations, or pct::yieldPoint in test code) and blocking brackets (AION_PCT_BLOCKING_BEGIN/END): at each point
 * the runnable thread with the highest priority continues. Priorities are a random permutation chosen from the seed; `depth - 1` random
 * priority change points (step indices in [1, k]) lower the running thread's priority below all others, which finds every bug of depth
 * `depth` with probability >= 1 / (n * k^(d-1)) per schedule (n threads, k steps).
 *
 * Script mode (Options::script, for directed interleavings): the scheduler first follows the script. Step {t, site, occurrences} runs thread t
 * until it arrives at the yield point `site` for the `occurrences`-th time since the step began (it stays paused there, before the protected
 * step executes); an empty site runs t to completion. When a scripted thread finishes before reaching its site, the schedule fails with a
 * "script" message (so renamed yield points break tests loudly). After the script, scheduling continues by priority.
 *
 * A thread that enters a blocking bracket gives up its turn until afterBlocking; its return from the OS wait is real-time dependent, so
 * schedules with blocking are replayable only up to OS timing. If the schedule exceeds `timeout` (every thread blocked or a livelock) it ends
 * with `deadlock = true`: the scheduler stops controlling, lets the threads run freely for a grace period and detaches those that still do not
 * finish (the test should treat it as a failure).
 * Threads that are not part of the schedule pass through yield points without waiting.
 *
 * Every schedule is replayable from its seed (lifetime scenarios without blocking are fully deterministic). Exceptions thrown by thread
 * bodies are captured: the first one is reported in `failure` (the other threads still run to completion under control). Only one
 * PctScheduler may run at a time in a process (it installs the global PctHooks); a second concurrent run() throws std::logic_error.
 * Before a controlled thread finishes, its lifetime retire list is flushed inside the schedule.
 */
struct ScriptStep {
	/** index into the thread bodies */
	uint32_t thread = 0;
	/** yield-point site to stop at (exact match); empty = run the thread to completion */
	std::string untilSite;
	uint32_t occurrences = 1;
};

struct Options {
	uint64_t seed = 1;
	/** bug depth d: number of priority change points + 1 */
	uint32_t depth = 3;
	/**
	 * expected upper bound of yield points per schedule (k), used to place the change points. 0 = automatic: run() uses 1000, explore()
	 * calibrates from the step count of the previous schedule.
	 */
	uint32_t maxSteps = 0;
	/** wall-clock bound for one schedule */
	std::chrono::milliseconds timeout{10000};
	/** record every yield-point site in ScheduleResult::trace (as "t<index>:<site>") */
	bool keepTrace = false;
	/** directed prefix of the schedule (see above) */
	std::vector<ScriptStep> script;
};

struct ScheduleResult {
	uint64_t seed = 0;
	/** all bodies finished without exception, script satisfied, no deadlock */
	bool completed = false;
	bool deadlock = false;
	uint32_t steps = 0;
	/** what() of the first exception thrown by a thread body or the script error, empty if none */
	std::string failure;
	/** yield-point sites in execution order (only with Options::keepTrace) */
	std::vector<std::string> trace;
};

class PctScheduler {
public:
	explicit PctScheduler(Options options);
	~PctScheduler();
	PctScheduler(const PctScheduler&) = delete;
	PctScheduler& operator=(const PctScheduler&) = delete;

	/** Runs one schedule of `threadBodies` (each on its own thread). Blocks until all bodies finished, a deadlock or the timeout. */
	ScheduleResult run(std::vector<std::function<void()>> threadBodies);

private:
	Options options;
};

/** Scenario factory: fresh thread bodies (and fresh shared state captured by them) for the given seed. */
using ScenarioFactory = std::function<std::vector<std::function<void()>>(uint64_t seed)>;

/**
 * Runs `schedules` schedules with seeds baseSeed, baseSeed + 1, ...: builds the scenario, runs it, then calls `check` (which may throw to
 * report a violated invariant). Stops at the first schedule that failed, deadlocked or whose check threw, and returns its result so the test
 * can print the seed. Returns the last result if all passed. With optionsTemplate.maxSteps == 0, k is calibrated from the previous schedule.
 */
ScheduleResult explore(uint32_t schedules, uint64_t baseSeed, const ScenarioFactory& scenario, const std::function<void()>& check = {},
	const Options& optionsTemplate = {});

/**
 * Number of schedules to run: the AION_PCT_SCHEDULES environment variable if set (nightly runs use 100000, design §12.4), otherwise
 * `defaultSchedules` (keep unit test defaults small so the suite stays fast).
 */
uint32_t schedulesFromEnvironment(uint32_t defaultSchedules);

/** Index of the calling thread in the running schedule, or -1 if the thread is not controlled by a PctScheduler. */
int32_t controlledThreadIndex() noexcept;

} // namespace aion::gameserver::runtime::pct
