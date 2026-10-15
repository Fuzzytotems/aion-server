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
 * The epoch Reclaimer, replacing Java's GC (design §1.1, §2.4-§2.7). One thread ("Reclaimer") starts a scan every `period`, when the backlog
 * exceeds `wakeBacklog`, and at once after a scan that ended at its work budget with eligible entries left.
 *
 * Protocol (design §2.4 as corrected in §21)
 * - State: global epoch E; per thread the published epoch or IDLE (ThreadContext::publishedEpoch); per object count, queued, retireEpoch.
 * - Retire: RefCounted::release pushes an object after its 1 → 0 transition and `!queued.exchange(true)`; retirePart/retireNode stamp E at the
 *   call (the caller has already unlinked the part/node). Entries go to the calling thread's retire list, which is pushed to a lock-free
 *   incoming stack at outermost TaskScope exit, at quiescentPoint(), when it holds 256 entries, immediately when the thread is outside any
 *   TaskScope, and at thread exit (design §2.4 "flush the thread-local retire list"). Entries not flushed yet are invisible to scans and to
 *   Stats::backlog.
 * - Limbo: every flushed entry waits in a bucket keyed by a lower bound of the stamp its destruction rule compares with m (its "key"): an
 *   object's retireEpoch as read by retire() (or by the scan that kept it), a part's or node's retire stamp, a retired OwnedPart's
 *   max(retire stamp, part stamp read by the scan that kept it).
 * - Scan: (1) E.fetch_add(1); (2) m = min(E, every published epoch), computed before reading any object; (3) the incoming stack is taken and
 *   its entries are filed under their keys (no object is read); (4) the buckets with a key below m are visited oldest first, entry by entry:
 *   for an object, if count == 0 && retireEpoch < m, destroy; else if count > 0, clear `queued`, then if count == 0 && !queued.exchange(true)
 *   keep it (else drop it: a concurrent last release pushed it again or it is alive); else keep; nodes and parts without a count are
 *   destroyed (their key is their stamp); a retired OwnedPart additionally needs partRefs == 0 and uses the later of its retirement and
 *   last-release stamps (review correction of design §2.3, see OwnedPart). Kept entries are filed again under the stamp just read (re-bucketed),
 *   after the visit, so one scan examines an entry at most once. Destruction runs after every 64 classified entries.
 * - Held parts: a retired OwnedPart found held by a Ref with a key below m cannot make progress until the Ref is released, so it is not filed
 *   back into the limbo (where it would be taken first by every scan and starve bounded scans) but appended to a held-part queue. The queue is
 *   visited round robin after the limbo, and every second scan starts with a share of min(64, max(1, budget entries / 2)) of it; an unbounded
 *   scan visits all of it. Entries found released, or with a key >= the scan's m, go back to the limbo. Stats::retiredPartsHeld reports its
 *   size.
 * - Work: a scan examines only entries with a key below its m, so entries kept alive by a pinned epoch (a task blocked while holding borrows)
 *   are not walked again by every scan, and nothing is copied per scan. Scans of the Reclaimer thread stop after `scanTimeBudget` /
 *   `scanEntryBudget` (checked after each chunk) and the next scan follows without waiting while it made progress on the limbo, so a large
 *   eligible backlog is destroyed in slices. Threads waiting for the scan lock (reclaimNow, removePostScanHook, setDestroyObserver) are let in
 *   before a continued scan (for at most one period). reclaimNow() and drain() run unbounded scans.
 * - Epoch progress: E advances once per scan. The gap between two advances is the waiting time (period, or none for a continued scan) plus the
 *   scan: filing the incoming entries (O(incoming), not budgeted), the classification and destruction of the chunks until the budget check
 *   fails (so at most the budget plus one chunk of 64 entries, whose destructors and destroy observer calls are not divisible), re-filing the
 *   kept entries (O(kept)), the post-scan hooks, and the wait for scan lock waiters. Heavy destructors (large node tables) and slow hooks
 *   lengthen it beyond the budget.
 * - Every atomic step has an AION_YIELD_POINT for PCT tests ("Reclaimer::scan:advance", ":readPublished", ":take", ":readCount",
 *   ":readStamp", ":clearQueued", ":recheck", ":requeue", ":destroy", "Reclaimer::flush", "Reclaimer::flush:published", ":readPublishedTask",
 *   ":readPartRefs", ":readPartStamp", "Reclaimer::retirePart:stamp", "Reclaimer::retireNode:stamp").
 *
 * Safety argument of the limbo (extends the §2.4 sketch, which shows that an entry satisfying the destruction rule with an m computed before
 * its reads is unreachable)
 * - Every stamp only grows (release, OwnedPart::release and retirePart store max(stamp, E)), so an entry's key stays a lower bound of its
 *   stamp. A scan that skips a bucket with key >= m skips only entries whose stamp is >= m: the rule would not destroy them either.
 * - Skipping a resurrected object (count > 0) postpones its drop. That is safe because its `queued` stays set, so its next last release does
 *   not push a second entry, and the entry that stays in the limbo is visited once m exceeds its key (the release that brings the count back
 *   to 0 stamps >= the key).
 * - The held-part queue changes only when an entry is examined, never what is destroyed: a queued part is classified by the same rule (refs,
 *   then max(retire stamp, part stamp) < m, read after m). Postponing its examination is safe for the same reason as skipping a bucket.
 * - Destroy decisions are unchanged: count and stamp are read after this scan's m, entry by entry. A decision stays valid until the entry is
 *   destroyed later in the same scan (after its chunk): an unreachable object cannot become reachable, and releases by destructors of earlier
 *   chunks stamp the current E >= m. A budget splits the visit between entries, never inside one entry's classification; the next scan
 *   computes a new m before it reads anything.
 * - Classification does not allocate (chunk buffers are preallocated, the re-bucket list is reserved per chunk); an allocation failure
 *   elsewhere in a scan terminates (C5).
 * - Destruction happens only on the scanning thread (the Reclaimer thread or the caller of reclaimNow) inside a destructor context:
 *   dereferencing a Ref, Ptr or loading a pointer there terminates in checked builds (C8). Cascading releases in destructors are stamped with
 *   the current epoch and wait one scan per level.
 *
 * Invariants (C5, all builds): an object is destroyed only with count == 0. Checked builds additionally verify that no examined object was
 * destroyed earlier in the same scan (a per-scan set of destroyed objects, checked before the entry's memory is read) and the cookie of every
 * examined object (a destroyed object queued twice in different scans terminates thanks to the poisoned delayed-free memory or ASan's
 * quarantine), and the allocation header. Checked builds poison destroyed objects and keep them in a delayed-free FIFO of `delayedFreeBytes`
 * (C3; disabled in ASan builds, whose quarantine does the same). Retired parts are never freed while a task that could borrow them is active
 * (C12, by the stamp rule).
 * Costs: filing is O(1) per entry (one map lookup per run of equal keys); a scan costs O(published threads + incoming entries + examined
 * entries + buckets with a key below m). Retired OwnedParts that Refs hold cost an unbounded scan O(held parts) and a bounded scan at most its
 * budget; checked builds add O(objects destroyed by the scan) for the destroyed set.
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
		/**
		 * Reclaimer thread: a scan stops examining entries once it has run this long (checked after every 64 entries, so a scan examines at
		 * least one chunk) and the next scan starts at once; 0 = unlimited. reclaimNow() ignores it.
		 */
		std::chrono::microseconds scanTimeBudget{10'000};
		/** Reclaimer thread: at most this many entries examined per scan (0 = unlimited). reclaimNow() ignores it. */
		size_t scanEntryBudget = 0;
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
		/** entries examined (classified) by all scans; a scan examines only entries whose limbo key is below its m */
		uint64_t examinedTotal = 0;
		/**
		 * scans that ended at their budget with limbo entries below their m left (the Reclaimer thread continues without waiting while such scans
		 * make progress; held retired parts are not counted)
		 */
		uint64_t budgetExhaustedScans = 0;
		/** retired OwnedParts that the last scan found held by a Ref (part of the backlog; a leaked Ref<Part> keeps its part and owner here) */
		uint64_t retiredPartsHeld = 0;
		/** entries examined by the last scan */
		uint64_t lastScanExamined = 0;
		/** objects, parts and nodes destroyed by the last scan */
		uint64_t lastScanDestroyed = 0;
		/** duration of the last scan (from the epoch advance to the end of destruction; hooks excluded) */
		std::chrono::microseconds lastScanDuration{0};
		/** at the last scan: time since E became the oldest published epoch m (0 if no thread published an epoch older than E) */
		std::chrono::milliseconds lag{0};
		/**
		 * at the last scan: TaskInfo of the task that published the oldest epoch, read together with the epoch (default if none, or if that
		 * task ended while the scan read it)
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
	 * list: every entry eligible at this scan is examined, whatever the budget. The calling thread's own published epoch is respected like any
	 * other. Runs post-scan hooks. Serialized with the Reclaimer thread's scans.
	 */
	void reclaimNow();

	/**
	 * Repeats reclaimNow() until the backlog is empty, at most `maxScans` times. Retired parts that a Ref still holds and objects borrowed by a
	 * published task stay in the backlog. @return true if the backlog is empty
	 */
	bool drain(uint32_t maxScans = 64);

	Stats stats() const;

	/**
	 * Backpressure for bulk producers of short-lived objects (C++ only; used by the parallel startup loops of World creation): waits while
	 * Stats::backlog exceeds `objects`, sleeping in 1 ms steps inside a BlockingRegion, for at most `timeout`. Returns at once when the Reclaimer
	 * thread is not running (nobody would reclaim) or when the calling thread has published an epoch in its task scope (it could hold up the
	 * very reclamation it waits for); call it before the task borrows anything. Lock-free reads only; legal on any thread outside Monitors.
	 * @return true if the backlog was at most `objects` when it returned
	 */
	bool awaitBacklogBelow(uint64_t objects, std::chrono::milliseconds timeout = std::chrono::seconds(10));

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
