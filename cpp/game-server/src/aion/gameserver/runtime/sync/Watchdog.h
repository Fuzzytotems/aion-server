#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/base/ThreadContext.h"

namespace aion::gameserver::runtime {

/**
 * The watchdog thread (design §1.1, §2.6, §4.3, C7), replacing Java's DeadLockDetector (deviation 17). Kept in release builds (C14).
 *
 * Every `period` it snapshots all ThreadContexts (never dereferencing lock objects or game objects, RR-15/RR-18) and
 * - builds the wait graph: a thread waiting for lock L (its wait record) has an edge to every other thread whose held-lock records contain L
 *   (Monitor, StampedLock, leaf mutexes record held locks in all builds); the owner thread id copied at wait start is only used when the
 *   holder's records overflowed. A cycle (including a thread waiting for a non-reentrant lock it holds itself) that is present with the same
 *   (thread, lock) pairs in two consecutive checks is reported as DEADLOCK. Requiring two observations filters cycles that only appear
 *   because the per-thread records were read at slightly different times;
 * - reports a task running longer than `slowTaskWarning` once as SLOW_TASK (a warning without a thread dump), and a task without progress
 *   for longer than `stall` once as STALL with a dump. A task run is identified by thread id and start time. A quiescentPoint() (new scope id,
 *   same start time) counts as progress: it restarts the stall clock, so a long QuiescentScope loop (PeriodicSaveService, shutdown saves,
 *   FixPath) is dumped only if one step makes no progress for `stall`, and never re-fires on every check. The slow-task clock covers the
 *   whole run and fires once. Kinds in `slowTaskExemptKinds` / `stallExemptKinds` are exempt (Java: the long-running pool never warns;
 *   startup and main-thread phases are expected to take long);
 * - runs the registered probes (the Reclaimer registers one for reclamation lag > 10 s and backlog high-water marks, design §2.6).
 *
 * A dump contains every thread's name, id, TaskInfo and running time, held lock classes, BlockingRegion, wait record and published epoch.
 * All tasks that stall in one check are reported together in one STALL dump (a parallel load used to produce one dump per thread).
 * Stack traces of other threads are not portable; on Windows DEADLOCK and STALL dumps additionally write a minidump (thread information and
 * every thread's stack, into `minidumpDirectory`) through MinidumpWriter: from a process snapshot, preferably by a helper process with a
 * timeout, so no live thread is suspended while DbgHelp works. Rate limit: at most one minidump per check (later dumps of the same check name
 * it), none within `minidumpMinInterval` after the previous one and at most `maxMinidumps` per run; DEADLOCK dumps are exempt from the
 * interval and have their own budget of `maxMinidumps` (a cycle is dumped once and may be followed by the restart). The text dump is always
 * logged, before the minidump is written, followed by a line naming the minidump or why none was written. Set writeMinidump = false to
 * disable. On other platforms no minidump is written (TODO: write a core file with gcore or a signal-based
 * backtrace collector).
 * After a dump the server keeps running (D5); only if `restartOnDeadlock` is set does a DEADLOCK exit with ExitCode::RESTART
 * (std::quick_exit, so at_quick_exit handlers flush the logs). The watchdog thread skips its checks while a debugger is attached
 * (IsDebuggerPresent on Windows), unless `evenUnderDebugger`; checkNow() always runs.
 *
 * Thread-safety: all members are thread-safe. Checks are serialized (the watchdog thread and checkNow callers never run one concurrently).
 * Probes and listeners run on the checking thread (the watchdog thread or the caller of checkNow), outside every internal leaf mutex; they
 * must not block for long and must not call checkNow (dump is allowed).
 */
class Watchdog {
public:
	enum class Reason : uint8_t { DEADLOCK, STALL, SLOW_TASK, RECLAIM_LAG, BACKLOG, MANUAL };

	struct Config {
		std::chrono::milliseconds period{1000};
		/** Java ThreadConfig.MAXIMUM_RUNTIME_IN_MILLISEC_WITHOUT_WARNING */
		std::chrono::milliseconds slowTaskWarning{5000};
		/** gameserver.watchdog.stall_seconds (milliseconds here, so tests can use short stalls; seconds convert implicitly) */
		std::chrono::milliseconds stall{std::chrono::seconds(60)};
		/**
		 * TaskInfo kinds without SLOW_TASK warnings: Java uses Long.MAX_VALUE for the long-running pool and has no warning for the main thread,
		 * startup/shutdown phases or ForkJoin elements.
		 */
		std::vector<std::string> slowTaskExemptKinds{TaskKind::LONG_RUNNING, TaskKind::MAIN, TaskKind::STARTUP, TaskKind::SHUTDOWN, TaskKind::FORK_JOIN};
		/** TaskInfo kinds without STALL dumps (geo builds, NN training, DataManager/Geo startup phases, design §2.6); deadlocks are still reported */
		std::vector<std::string> stallExemptKinds{TaskKind::LONG_RUNNING, TaskKind::MAIN, TaskKind::STARTUP};
		/** gameserver.watchdog.restart_on_deadlock */
		bool restartOnDeadlock = false;
		/** write a minidump with DEADLOCK/STALL dumps (Windows) */
		bool writeMinidump = true;
		/** directory for minidump files (created on demand; UTF-8) */
		std::string minidumpDirectory = "log/dumps";
		bool evenUnderDebugger = false;
		/**
		 * The process that writes minidumps (MinidumpWriter): empty = the running executable if its main registered the helper mode
		 * (MinidumpWriter::runIfRequested), otherwise the in-process snapshot writer; a path (UTF-8) = that executable with `--write-minidump`.
		 */
		std::string minidumpHelperExecutable;
		/** longest time a minidump may take; a helper process is terminated afterwards, an in-process writer abandoned */
		std::chrono::milliseconds minidumpTimeout{30'000};
		/**
		 * rate limit: at most one minidump per check, and none within this interval after the previous one (0 = only one per check); DEADLOCK
		 * minidumps are not delayed by it
		 */
		std::chrono::milliseconds minidumpMinInterval{60'000};
		/** at most this many STALL minidumps per process run, and separately this many DEADLOCK minidumps (0 = unlimited) */
		int32_t maxMinidumps = 20;
	};

	/** One thread in a dump; copied from its ThreadContext. */
	struct ThreadSnapshot {
		uint64_t threadId = 0;
		const char* threadName = "";
		ThreadContext::TaskSnapshot task;
		ThreadContext::BlockingSnapshot blocking;
		ThreadContext::WaitSnapshot wait;
		std::vector<const char*> heldLockClasses;
		/** lock identities parallel to heldLockClasses (addresses, never dereferenced) */
		std::vector<uintptr_t> heldLockIds;
		/** number of held locks, may exceed the recorded ones (ThreadContext::MAX_RECORDED_LOCKS) */
		uint32_t heldLockCount = 0;
		uint64_t publishedEpoch = EPOCH_IDLE;
	};

	struct DumpReport {
		Reason reason = Reason::MANUAL;
		std::string summary;
		/** threads involved (cycle members, stalled thread); empty for probe dumps */
		std::vector<uint64_t> threadIds;
		std::vector<ThreadSnapshot> threads;
		/** full text as logged */
		std::string text;
		/** path of the minidump written with this report, empty if none */
		std::string minidumpPath;
	};

	/** Called on each check with the current snapshot; may call dump() to report a problem. */
	using Probe = std::function<void(Watchdog& watchdog, const std::vector<ThreadSnapshot>& threads)>;
	using DumpListener = std::function<void(const DumpReport& report)>;

	static Watchdog& getInstance();

	/** Starts the watchdog thread (idempotent: a second start only updates the config). */
	void start(const Config& config);
	/** Stops and joins the watchdog thread. Idempotent. Must not be called from a probe or listener. */
	void stop();
	bool isRunning() const noexcept;
	Config getConfig() const;
	/** Updates the config without starting the thread (tests using checkNow). */
	void configure(const Config& config);

	/** Runs one check synchronously on the calling thread (tests, `//debug`), regardless of start()/debugger state. */
	void checkNow();

	/** Registers a probe; returns an id for removeProbe. */
	uint64_t addProbe(std::string name, Probe probe);
	void removeProbe(uint64_t probeId);

	/** Listeners see every dump (tests use them to observe detections). Returns an id for removeDumpListener. */
	uint64_t addDumpListener(DumpListener listener);
	void removeDumpListener(uint64_t listenerId);

	/**
	 * Logs a dump of all threads with the given reason; used by probes and `//debug`. Deduplication is the caller's responsibility. For DEADLOCK
	 * and STALL a minidump is written if configured.
	 */
	DumpReport dump(Reason reason, std::string_view summary, std::vector<uint64_t> involvedThreadIds = {});

	/** Snapshot of all registered threads. */
	std::vector<ThreadSnapshot> snapshotThreads() const;

	/** Display name of a reason ("DEADLOCK", ...). */
	static const char* reasonName(Reason reason) noexcept;

private:
	Watchdog() = default;
};

} // namespace aion::gameserver::runtime
