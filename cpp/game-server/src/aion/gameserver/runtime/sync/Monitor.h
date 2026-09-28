#pragma once

#include <atomic>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <typeinfo>

#include "aion/gameserver/runtime/sync/LockClass.h"

namespace aion::gameserver::runtime {

class ThreadContext;

/**
 * Game-level reentrant lock: Java `synchronized` and `ReentrantLock` (design §3.4, §4.1 family 1, rank 0).
 *
 * Semantics
 * - Reentrant: the owning thread may lock it again; it is released when unlock() was called as often as lock().
 * - Monitors nest arbitrarily and code holding them may call out (callbacks, services, DAOs, scheduling), exactly as in Java.
 * - Not strictly fair (like a default ReentrantLock and like Java monitors): lock() may barge past waiters, but a waiter that has waited
 *   longer than about 1 ms switches the Monitor into starvation mode, in which only registered waiters may acquire (eventual fairness, like
 *   Go's sync.Mutex). tryLock() and tryLock(timeout) barge like Java's non-fair ReentrantLock.tryLock. No wait/notify/conditions: the Java
 *   game server uses none (checked with grep).
 * - Uncontended lock/unlock is a CAS each plus the held-lock record (ThreadContext::heldLocks, all builds, used by the watchdog).
 * - Blocking waits record {thread id, lock class name, owner thread id, wait start} in the thread's ThreadContext for the watchdog's wait
 *   graph (design §4.3) and are bracketed with AION_PCT_BLOCKING_BEGIN/END. The watchdog never dereferences the Monitor.
 * - Every acquisition has an AION_YIELD_POINT (PCT tests, design §12.4).
 *
 * Checks
 * - All builds (C6/C14): lock()/tryLock() throw IllegalStateException when the calling thread holds a leaf mutex (RankedMutex).
 * - Checked builds (C6): acquisitions and releases are reported to the LockOrderValidator with the lock class (the Monitor's static class,
 *   the dynamic class passed by monitorOf/SYNCHRONIZED, or LockClass::anonymousMonitor()). Inversions are reported, never thrown (D5).
 *   Reentrant acquisitions are not reported. tryLock() records no edges (it cannot block); tryLock(timeout) with a positive timeout records
 *   them before its first attempt, contended or not.
 *
 * Size: at most 32 bytes (embedded in every RefCounted, design §2.5 "+40 B per object" budget). Not copyable or movable.
 * Thread-safety: all members are thread-safe. Destroying a Monitor that is held or waited for is undefined (RefCounted objects are only
 * destroyed by the Reclaimer when unreachable).
 */
class Monitor {
public:
	/** Anonymous Monitor: object monitors get their class from monitorOf (dynamic type); standalone use reports "Monitor". */
	constexpr Monitor() noexcept = default;
	/** Monitor with a static lock class, e.g. `Monitor teamLock{AION_LOCK_CLASS(GeneralTeam::teamLock)};` */
	explicit constexpr Monitor(const LockClass& lockClass) noexcept : lockClass_(&lockClass) {}

	Monitor(const Monitor&) = delete;
	Monitor& operator=(const Monitor&) = delete;

	/**
	 * Java: ReentrantLock.lock() / monitorenter. Blocks until acquired.
	 * @throws IllegalStateException if the calling thread holds a leaf mutex (all builds)
	 */
	void lock();

	/** Like lock(), attributing the acquisition to `dynamicClass` when this Monitor has no static class (object monitors). */
	void lock(const LockClass& dynamicClass);

	/**
	 * Java: ReentrantLock.tryLock(). Acquires only if free or already held by the caller; never blocks.
	 * @throws IllegalStateException if the calling thread holds a leaf mutex (all builds)
	 */
	bool tryLock();
	/** Like tryLock(), attributing the acquisition to `dynamicClass` when this Monitor has no static class. */
	bool tryLock(const LockClass& dynamicClass);

	/** Java: ReentrantLock.tryLock(timeout, unit). The wait is recorded like lock(); edges are recorded like lock() when timeout > 0. */
	bool tryLock(std::chrono::nanoseconds timeout);
	/** Like tryLock(timeout), attributing the acquisition to `dynamicClass` when this Monitor has no static class. */
	bool tryLock(std::chrono::nanoseconds timeout, const LockClass& dynamicClass);

	/**
	 * Java: ReentrantLock.unlock() / monitorexit.
	 * @throws IllegalMonitorStateException if the calling thread does not hold the Monitor
	 */
	void unlock();

	/** Java: ReentrantLock.isHeldByCurrentThread() */
	bool isHeldByCurrentThread() const noexcept;
	/** Java: ReentrantLock.getHoldCount() - 0 if the calling thread does not hold it */
	int32_t getHoldCount() const noexcept;
	/** Java: ReentrantLock.isLocked() - a racy snapshot */
	bool isLocked() const noexcept;

	/** The static lock class, or nullptr for anonymous Monitors. */
	const LockClass* staticLockClass() const noexcept { return lockClass_; }

	/** Lockable alias (lock()/unlock() already match BasicLockable), so std::scoped_lock and std::unique_lock work. */
	bool try_lock() { return tryLock(); }

private:
	/** CAS LOCKED into the word if free (and, unless `barge`, not in starvation mode). */
	bool tryAcquireWord(bool barge) noexcept;
	/** Spin, register as waiter and block until acquired or `deadlineNanos` (nanoTime; < 0 = no deadline). @return true if acquired */
	bool acquireSlow(ThreadContext& context, const LockClass& lockClass, int64_t deadlineNanos);

	/** ThreadContext::threadId() of the owner, 0 when free */
	std::atomic<uint64_t> owner_{0};
	/** lock word: bit 0 LOCKED, bit 1 STARVING, bits 2..31 registered waiters; futex protocol, see Monitor.cpp */
	std::atomic<uint32_t> state_{0};
	/** recursion count; written only by the owner */
	uint32_t holdCount_ = 0;
	const LockClass* lockClass_ = nullptr;
};

/** A Monitor together with the lock class to report for it (static class if the Monitor has one, else the object's dynamic type). */
struct MonitorHandle {
	Monitor& monitor;
	const LockClass& lockClass;
};

/** Anything whose Java object monitor is reachable through `monitor()`: RefCounted, Immortal, OwnedPart, collection and Atomic* shims. */
template <class T>
concept HasMonitor = requires(const T& object) {
	{ object.monitor() } -> std::same_as<Monitor&>;
};

/** monitorOf(Monitor): a standalone Monitor (Java ReentrantLock or `synchronized (lockObject)` on a dedicated lock). */
inline MonitorHandle monitorOf(Monitor& monitor) {
	return {monitor, monitor.staticLockClass() != nullptr ? *monitor.staticLockClass() : LockClass::anonymousMonitor()};
}

/**
 * monitorOf(object): the object's Java monitor (design §3.4). Lock class: the member Monitor's static class if it has one (shims, Atomic*),
 * otherwise the dynamic type of `object` (`SYNCHRONIZED(*this)` uses the most derived class). For pointers, dereference first:
 * `SYNCHRONIZED(*player)`.
 */
template <HasMonitor T>
MonitorHandle monitorOf(const T& object) {
	Monitor& monitor = object.monitor();
	return {monitor, monitor.staticLockClass() != nullptr ? *monitor.staticLockClass() : LockClass::ofType(typeid(object))};
}

/** RAII guard used by SYNCHRONIZED: locks in the constructor, unlocks in the destructor. */
class [[nodiscard]] MonitorGuard {
public:
	explicit MonitorGuard(MonitorHandle handle) : monitor(handle.monitor) { monitor.lock(handle.lockClass); }
	~MonitorGuard() { monitor.unlock(); }
	MonitorGuard(const MonitorGuard&) = delete;
	MonitorGuard& operator=(const MonitorGuard&) = delete;

private:
	Monitor& monitor;
};

} // namespace aion::gameserver::runtime

#define AION_SYNC_CONCAT_INNER(a, b) a##b
#define AION_SYNC_CONCAT(a, b) AION_SYNC_CONCAT_INNER(a, b)

/**
 * Java `synchronized (x) { ... }` → `SYNCHRONIZED(x) { ... }` (design §3.4). The lock is held for exactly the following statement or block.
 * A Java synchronized method wraps its whole body: `void f() { SYNCHRONIZED(*this) { ...body... } }`.
 * `x` may be a Monitor, or any object with monitor() (RefCounted, Immortal, OwnedPart, shims, Atomic*). `SYNCHRONIZED(x);` with an empty
 * statement is a bug (the lock is released immediately); lint L7 rejects it.
 * The expansion is `if (MonitorGuard guard{monitorOf(x)}; false) {} else <statement>`, so `return` inside the block needs no trailing return
 * and a following `else` binds correctly.
 */
#define SYNCHRONIZED(x)                                                                                                                             \
	if (::aion::gameserver::runtime::MonitorGuard AION_SYNC_CONCAT(aionSynchronizedGuard, __COUNTER__){::aion::gameserver::runtime::monitorOf(x)};   \
	    false) {                                                                                                                                      \
	} else
