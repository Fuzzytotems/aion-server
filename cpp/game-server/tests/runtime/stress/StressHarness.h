#pragma once

// Kernel stress harness v0 (design §12.4 item 2 at kernel level, §19 P2).
//
// 2 x cores worker threads run short tasks (each in its own TaskScope) that store, drop, borrow and resurrect Refs, replace parts, set Fields,
// mutate and iterate the collection shims (including ConcurrentHashMap/HashMap compute callbacks that take Monitors and nest map operations),
// schedule and cancel Futures on the real pools, and run borrow-free long tasks and blocked tasks, with fault injection (exceptions thrown in
// task bodies, compute callbacks, SYNCHRONIZED blocks, predicates and comparators). Borrows taken early in a task are re-verified later in the
// same task while other threads drop the objects, so every premature free shows up as an ASan report or a canary mismatch (checked builds
// poison freed objects, C3).
//
// Periodic checks (checker thread): canaries and key consistency of a sample of the world map, Reclaimer progress (scans advance), lag and
// backlog maxima, lockdep cycle reports, watchdog DEADLOCK/STALL dumps. A sampler thread verifies that borrow-free tasks never hold a published
// epoch (a seqlock-bracketed read of the task's ThreadContext::publishedEpoch) and records lag/backlog/memory samples.
// End checks after teardown: zero live objects, parts and hubs, empty Reclaimer backlog, LeakCensus tracked count zero and no zombie cuts,
// every CleanerQueue push drained, no unexpected exception, no canary failure, no lockdep cycle, no watchdog deadlock or stall, the process
// heap back near its baseline (ASan builds: reported only).
//
// A supervisor thread terminates the process with a diagnostic dump (worker operations, watchdog thread dump) if the run or the teardown
// exceeds its deadline, so a deadlock never hangs the test run.
//
// Environment (read by StressConfig::fromEnvironment; test-only, the runtime getenv ban applies to game code):
//   AION_STRESS_SECONDS   duration of the long run; the long test is skipped when unset
//   AION_STRESS_THREADS   worker threads (default 2 x std::thread::hardware_concurrency())
//   AION_STRESS_SEED      base seed (default: time based, printed)
//   AION_STRESS_FAULTS    fault injection rate in permille of the injection sites (default 5)
//   AION_STRESS_REPORT    file the final report is appended to
//   AION_STRESS_VERBOSE   1: progress line every 10 s
//   AION_STRESS_KNOWN_ISSUES 1: also inject faults at sites with known, reported kernel issues (see StressReproducerTest.cpp; currently none)
//   AION_STRESS_MUTATION  harness self-test: a lifetime detail::Mutation value active during the run phase (the run must fail)

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace aion::gameserver::runtime::stress {

struct StressConfig {
	std::chrono::milliseconds duration{3000};
	/** 0 = 2 x hardware_concurrency */
	int32_t workerThreads = 0;
	/** 0 = time based */
	uint64_t seed = 0;
	/** exceptions injected at a fault site with this probability (permille) */
	int32_t faultPermille = 5;
	/** Field<Ref> slots of the hub */
	int32_t slots = 4096;
	/** key space of the world map (keys are id % worldKeys) */
	int32_t worldKeys = 8192;
	int32_t arrayLength = 1024;
	/** caps of the plain shared shims */
	int32_t listCap = 2048;
	int32_t cowCap = 256;
	int32_t partListCap = 4096;
	/** throttles */
	int32_t maxInflightFutures = 20'000;
	int32_t maxConcurrentLongTasks = 8;
	std::chrono::milliseconds longTaskMax{300};
	std::chrono::milliseconds blockedHoldMax{20};
	/** periods of the checker and the sampler */
	std::chrono::milliseconds checkPeriod{1000};
	std::chrono::milliseconds samplePeriod{10};
	/** time allowed for joining workers after the run and for the teardown (supervisor deadlines) */
	std::chrono::milliseconds joinTimeout{60'000};
	std::chrono::milliseconds teardownTimeout{180'000};
	/** pools of the run (ThreadPoolBackend); 0 = hardware_concurrency */
	int32_t instantThreads = 0;
	int32_t scheduledThreads = 4;
	bool startWatchdog = true;
	/** also inject faults at sites with a known, reported kernel issue (a throwing ArrayList::sort comparator, StressReproducerTest) */
	bool knownIssueFaults = false;
	/** harness self-test (checked builds): detail::Mutation value switched on during the run phase; 0 = none */
	int32_t mutation = 0;
	bool verbose = false;
	std::string reportPath;

	/** Applies the AION_STRESS_* variables (see the file comment) on top of `defaults`. */
	static StressConfig fromEnvironment(StressConfig defaults);
	/** duration from AION_STRESS_SECONDS, 0 if unset */
	static std::chrono::seconds requestedLongRunDuration();
};

struct StressResult {
	bool passed = false;
	/** pass criteria that failed (empty when passed) */
	std::vector<std::string> failures;
	/** noteworthy observations that do not fail the run */
	std::vector<std::string> warnings;
	/** the full human-readable report (configuration, throughput, reclamation lag, memory, checks) */
	std::string report;
};

/** Exit code of the supervisor when a phase exceeds its deadline (distinguishes a hang from a detected failure in death tests). */
inline constexpr int SUPERVISOR_EXIT_CODE = 4;

/** Runs one stress run (starts and stops the Reclaimer thread, the watchdog and the pools). Not reentrant. */
StressResult runKernelStress(const StressConfig& config);

/** Process memory (0 where the platform provides no value). */
struct MemorySample {
	/** committed private bytes of the process */
	uint64_t privateBytes = 0;
	uint64_t workingSet = 0;
	/** bytes allocated from the process heap and in use (Windows HeapSummary), the closest measure of live heap memory */
	uint64_t heapAllocated = 0;
};
MemorySample sampleProcessMemory();

} // namespace aion::gameserver::runtime::stress
