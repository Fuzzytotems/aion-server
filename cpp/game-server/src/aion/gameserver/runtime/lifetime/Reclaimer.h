#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"

namespace aion::gameserver::runtime {

/**
 * The epoch Reclaimer, replacing Java's GC (design §1.1, §2.4-§2.7). One thread ("Reclaimer") runs a scan every `period` or when the
 * backlog exceeds `wakeBacklog`.
 *
 * Protocol (design §2.4)
 * - State: global epoch E; per thread the published epoch or IDLE (ThreadContext::publishedEpoch); per object count, queued, retireEpoch.
 * - Retire: RefCounted::release pushes an object after its 1 → 0 transition and `!queued.exchange(true)`; retirePart/retireNode stamp E at the
 *   call (the caller has already unlinked the part/node). Entries go to the calling thread's retire list, which is pushed to a lock-free
 *   incoming stack at outermost TaskScope exit, at quiescentPoint(), when it holds 256 entries, immediately when the thread is outside any
 *   TaskScope, and at thread exit (design §2.4 "flush the thread-local retire list"). Entries not flushed yet are invisible to scans and to
 *   Stats::backlog.
 * - Scan: (1) E.fetch_add(1); (2) m = min(E, every published epoch), computed before reading any object; (3) the incoming stack is taken and
 *   for each queued object: if count == 0 && retireEpoch < m, destroy; else if count > 0, clear `queued`, then if count == 0 &&
 *   !queued.exchange(true) keep it (else drop it: a concurrent last release pushed it again or it is alive); else keep; (4) nodes and parts
 *   without a count are destroyed when their stamp < m; a retired OwnedPart additionally needs partRefs == 0 and uses the later of its
 *   retirement and last-release stamps (review correction of design §2.3, see OwnedPart).
 * - An allocation failure inside a scan terminates (C5): a half-classified queue could destroy an object twice.
 * - Destruction happens only on the scanning thread (the Reclaimer thread or the caller of reclaimNow) inside a destructor context:
 *   dereferencing a Ref, Ptr or loading a pointer there terminates in checked builds (C8). Cascading releases in destructors are stamped with
 *   the current epoch and wait one scan per level.
 * - Every atomic step has an AION_YIELD_POINT for PCT tests ("Reclaimer::scan:advance", ":readPublished", ":take", ":readCount",
 *   ":readStamp", ":clearQueued", ":recheck", ":requeue", ":destroy", "Reclaimer::flush", "Reclaimer::flush:published", ":readPublishedTask", ":readPartRefs", ":readPartStamp", "Reclaimer::retirePart:stamp",
 *   "Reclaimer::retireNode:stamp").
 *
 * Invariants (C5, all builds): an object is destroyed only with count == 0. Checked builds additionally verify the cookie of every queued
 * object (a destroyed object queued twice terminates) and the allocation header. Checked builds poison destroyed objects and keep them in a
 * delayed-free FIFO of `delayedFreeBytes` (C3; disabled in ASan builds, whose quarantine does the same). Retired parts are never freed while a
 * task that could borrow them is active (C12, by the stamp rule).
 * start() registers a watchdog probe: lag > lagWarning → warning (once per oldest epoch); backlog > backlogDumpObjects or > backlogDumpBytes →
 * Watchdog dump naming the oldest publishing thread (once, re-armed when the backlog falls below half), design §2.6.
 *
 * Scans, post-scan hooks and the destroy observer are serialized by an internal scan mutex (a plain mutex, not a leaf RankedMutex, because
 * hooks may take Monitors); reclaimNow() therefore must not be called while holding a Monitor that a hook takes, and hooks, destructors and
 * the destroy observer must not call reclaimNow(), drain(), start() or stop(). A controlled PCT thread that finds the mutex taken brackets
 * the wait with AION_PCT_BLOCKING_BEGIN/END. removePostScanHook() and setDestroyObserver() wait for a running scan (and its hooks) on another
 * thread, so the same rule applies to them: do not call them while holding a Monitor or mutex that a hook or the observer takes.
 * Thread-safety: retire*, stats, currentEpoch and hook registration are thread-safe. start/stop are not reentrant (call from main/shutdown).
 */
class Reclaimer {
public:
	struct Config {
		/** gameserver.runtime.reclaim_period_ms */
		std::chrono::milliseconds period{20};
		/** the thread is woken early when this many flushed entries are queued */
		size_t wakeBacklog = 8192;
		/** gameserver.runtime.backlog_dump_objects */
		size_t backlogDumpObjects = 1'000'000;
		size_t backlogDumpBytes = size_t{256} * 1024 * 1024;
		std::chrono::seconds lagWarning{10};
		/** checked builds (C3): delayed-free FIFO size in bytes (0 disables; ignored in ASan builds) */
		size_t delayedFreeBytes = size_t{64} * 1024 * 1024;
	};

	struct Stats {
		/** current global epoch E */
		uint64_t epoch = 0;
		/** m of the last scan */
		uint64_t minActive = 0;
		/** flushed objects, parts and nodes waiting for reclamation */
		uint64_t backlog = 0;
		/** approximate bytes of the backlog (objects: exact size in checked builds, sizeof(RefCounted) otherwise; nodes: retiredBytes()) */
		uint64_t backlogBytes = 0;
		uint64_t destroyedTotal = 0;
		uint64_t scans = 0;
		/** at the last scan: time since E became the oldest published epoch m (0 if no thread published an epoch older than E) */
		std::chrono::milliseconds lag{0};
		/**
	 * at the last scan: TaskInfo of the task that published the oldest epoch, read together with the epoch (default if none, or if that task
	 * ended while the scan read it)
	 */
		TaskInfo oldestPublishedTask{};
		/** at the last scan: ThreadContext::threadId() of that thread, 0 if none */
		uint64_t oldestPublishedThreadId = 0;
	};

	using ScanHook = std::function<void()>;

	static Reclaimer& getInstance();

	/** Starts the Reclaimer thread with the given period (design signature). */
	void start(std::chrono::milliseconds period);
	/** Starts the Reclaimer thread (if it is running, only the configuration is updated). */
	void start(const Config& config);
	/** Stops and joins the thread (remaining objects stay queued; reclaimNow can still free them). Idempotent. */
	void stop();
	bool isRunning() const noexcept;

	/** Applies a configuration without starting the thread (tests, DeterministicExecutor). */
	void configure(const Config& config);
	Config getConfig() const;

	/**
	 * Called by RefCounted::release after its successful 1 → 0 transition and `!queued.exchange(true)`. Appends to the calling thread's retire
	 * list (see above). noexcept; legal on any thread.
	 */
	static void retire(const RefCounted& object) noexcept;

	/**
	 * Retires a part replaced in a PartSlot<P, RECLAIMER> or PartMap (design §2.3, RR-19): holds a Ref to `owner`, stamps the current epoch and
	 * destroys the part once no task that could have borrowed it is active and, for an OwnedPart, no Ref to the part remains; then releases
	 * the owner. The caller must have unlinked the part before the call. A null part is ignored. noexcept; legal on any thread.
	 */
	static void retirePart(const RefCounted& owner, std::unique_ptr<OwnedPartBase> part) noexcept;

	/**
	 * Retires runtime-internal memory without a reference count (ConcurrentHashMap node tables, CopyOnWriteArrayList arrays, Field<std::string>
	 * boxes): stamps the current epoch and destroys the node once no task that could have loaded it is active. The caller must have unlinked
	 * the node before the call. A null node is ignored. noexcept; legal on any thread.
	 */
	static void retireNode(std::unique_ptr<RetiredNode> node) noexcept;

	/**
	 * Runs one full scan synchronously on the calling thread (DeterministicExecutor, tests, shutdown), after flushing the calling thread's retire
	 * list. The calling thread's own published epoch is respected like any other. Runs post-scan hooks. Serialized with the Reclaimer thread's
	 * scans.
	 */
	void reclaimNow();

	/**
	 * Repeats reclaimNow() until the backlog is empty, at most `maxScans` times. Retired parts that a Ref still holds and objects borrowed by a
	 * published task stay in the backlog. @return true if the backlog is empty
	 */
	bool drain(uint32_t maxScans = 64);

	Stats stats() const;

	/** the current global epoch E (lock-free) */
	static uint64_t currentEpoch() noexcept;

	/** true while the calling thread runs destructors for the Reclaimer (C8) */
	static bool inDestructorContext() noexcept;

	/**
	 * Hooks run after each scan on the scanning thread, outside the destructor context and inside a TaskScope of kind RECLAIMER (LeakCensus scan,
	 * posting the CleanerDrain to the instant pool, design §5.4, §6). Hooks must not block and must not throw (exceptions are logged).
	 * @return id for removePostScanHook
	 */
	uint64_t addPostScanHook(std::string name, ScanHook hook);
	/**
	 * Removes a hook. When called from another thread than the one scanning, returns only after a scan that is running (with its hooks) has
	 * finished, so the hook is neither running nor started again afterwards. From the scanning thread (inside a hook) it returns at once.
	 */
	void removePostScanHook(uint64_t hookId);

	/**
	 * Observer called on the scanning thread for every RefCounted object immediately before it is destroyed (serialized with scans and
	 * post-scan hooks). LeakCensus uses it to erase the object's census entry before the memory is freed, so its raw pointers never dangle
	 * (design §5.4, RR-18). Must be noexcept, must not dereference anything but the object's count, must not block. nullptr removes it.
	 * There is a single slot, owned by LeakCensus (a second user would replace the census observer). Like removePostScanHook, returns only after
	 * a running scan on another thread has finished, so the previous observer is not called any more.
	 */
	using DestroyObserver = void (*)(const RefCounted& object) noexcept;
	void setDestroyObserver(DestroyObserver observer) noexcept;

private:
	Reclaimer() = default;
};

} // namespace aion::gameserver::runtime
