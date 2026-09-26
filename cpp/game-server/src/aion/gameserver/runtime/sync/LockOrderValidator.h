#pragma once

#include <cstddef>
#include <cstdint>
#include <source_location>
#include <string>
#include <vector>

#include "aion/gameserver/runtime/sync/LockClass.h"

namespace aion::gameserver::runtime {

/**
 * Lockdep-style lock-order validator for game-level locks (design §4.2, C6). Edge recording and reports are active in checked builds only;
 * in release builds beforeAcquire/onBlocking are no-ops and getReports() is empty. The held-lock bookkeeping of afterAcquire/afterRelease
 * (ThreadContext::heldLocks) runs in ALL builds, because the watchdog (kept in release builds, C14) uses it to verify wait-graph edges.
 *
 * - Each thread keeps its stack of held lock classes. Acquiring class B while holding class A records the edge A→B with the acquisition
 *   stack (std::stacktrace) of its first occurrence; when an edge is new, a DFS over the class graph looks for a path B→...→A.
 * - A cycle is reported once per new edge as `log.error` with both stacks: the stack of the acquisition that closed the cycle and the stored
 *   first-occurrence stacks of every edge on the reverse path (CYCLE). Nothing is thrown and execution continues (D5). The closing edge is
 *   still added to the graph, so later edges that close further cycles are reported too.
 * - Nesting two different locks of the same class (Player A then Player B) is a warning (SAME_CLASS_NESTING), reported once per class.
 *   Reentrant acquisition of a lock already held by the thread (the same Monitor, e.g. stripe #3 twice) is not nesting: Monitors do not call
 *   the hooks for reentrant acquisitions. For non-reentrant locks (StampedLock) acquiring the same lock again is reported as
 *   SAME_CLASS_NESTING ("self-deadlock") unless both the held and the new acquisition are shared (read) acquisitions.
 * - Blocking while holding a game-level lock (BlockingRegion: Future::get, DB calls, Semaphore) is counted per site
 *   (BLOCKING_UNDER_MONITOR, warning once per site; listed by `//debug locks`).
 * - A site marked `// lockdep: <reason>` opens a LockdepSuppression: acquisitions inside it record no edges and produce no reports (the locks
 *   are still recorded as held).
 * - tryLock() (non-blocking) acquisitions record no edges (they cannot deadlock) but are pushed as held, so later acquisitions record edges
 *   from them. Timed acquisitions record edges.
 * - failOnReport (tests): every new CYCLE report additionally increments failureCount(); tests/runtime/sync/LockdepTestSupport.h fails the
 *   running test when it grows.
 *
 * Costs (checked builds): an acquisition while holding other game-level locks looks up each (held class, new class) edge in a thread-local
 * cache; only cache misses take the graph mutex. Report occurrence counts are therefore approximate (counted on cache misses).
 *
 * Thread-safety: all members are thread-safe. The graph is protected by a leaf mutex (LockRank::STATS); stack symbolization and report
 * logging happen after it is released. Hooks called while the thread holds a leaf mutex of rank >= STATS do nothing (runtime bug elsewhere).
 */
class LockOrderValidator {
public:
	enum class ReportKind : uint8_t { CYCLE, SAME_CLASS_NESTING, BLOCKING_UNDER_MONITOR };

	struct Report {
		ReportKind kind = ReportKind::CYCLE;
		/** for CYCLE: the edge that closed the cycle (held → acquired); for SAME_CLASS_NESTING both are the class; for blocking: held class */
		std::string heldLockClass;
		std::string acquiredLockClass;
		/** full human-readable report as logged */
		std::string text;
		/** stack of the new acquisition and stack recorded with the first occurrence of the reverse edge (CYCLE only) */
		std::string acquisitionStack;
		std::string reverseEdgeStack;
		/** number of times this report fired (reports are deduplicated) */
		uint64_t occurrences = 0;
	};

	static LockOrderValidator& getInstance();

	/** true in checked builds unless disabled with setEnabled(false) */
	bool isEnabled() const noexcept;
	void setEnabled(bool enabled) noexcept;

	/**
	 * Called by game-level locks before a (possibly blocking) acquisition by the calling thread, after reentrancy was excluded. Records edges
	 * from every held lock class to `lockClass` and reports cycles/same-class nesting. Never throws (allocation failures are swallowed).
	 * `shared`: the acquisition is a shared (read) acquisition of a non-reentrant lock (StampedLock::readLock).
	 */
	void beforeAcquire(const LockClass& lockClass, uintptr_t lockId, bool shared = false) noexcept;
	/** Called after the lock was acquired: pushes it onto the thread's held-lock stack (all builds). */
	void afterAcquire(const LockClass& lockClass, uintptr_t lockId, uint8_t rank = 0, bool shared = false) noexcept;
	/** Called when the lock is fully released (hold count reached 0): removes it from the thread's held-lock stack (not necessarily LIFO). */
	void afterRelease(uintptr_t lockId) noexcept;
	/** Called by BlockingRegion: counts blocking while the thread holds game-level locks. */
	void onBlocking(const char* what, const std::source_location& where) noexcept;

	/** Deduplicated reports so far, in the order they were first reported. */
	std::vector<Report> getReports() const;
	size_t reportCount(ReportKind kind) const;
	/** Number of CYCLE reports since the last clear while failOnReport was set. */
	uint64_t failureCount() const noexcept;
	/** Forgets reports, failure counts and blocking counters (not the edge graph unless resetGraph). */
	void clearReports(bool resetGraph = false);

	void setFailOnReport(bool failOnReport) noexcept;
	bool isFailOnReport() const noexcept;

	/** `//debug locks`: edge count, classes, reports and blocking-under-monitor sites as display lines. */
	std::vector<std::string> describe() const;

private:
	LockOrderValidator() = default;
};

/**
 * RAII suppression for a site that the lock-order validator must ignore; the reason is mandatory and mirrors the `// lockdep: <reason>` marker.
 * Thread-local, nestable.
 */
class LockdepSuppression {
public:
	explicit LockdepSuppression(const char* reason) noexcept;
	~LockdepSuppression();
	LockdepSuppression(const LockdepSuppression&) = delete;
	LockdepSuppression& operator=(const LockdepSuppression&) = delete;
};

} // namespace aion::gameserver::runtime
