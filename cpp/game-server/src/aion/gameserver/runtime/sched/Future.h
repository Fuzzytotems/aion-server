#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <exception>
#include <functional>
#include <source_location>
#include <utility>

#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/sched/Pin.h"
#include "aion/gameserver/runtime/sched/TimeUnit.h"

namespace aion::gameserver::runtime {

/** Where a task runs (design §1.1). */
enum class PoolKind : uint8_t {
	/** ScheduledPool: schedule / scheduleAtFixedRate bodies */
	SCHEDULED,
	/** InstantPool: execute / submit, CleanerDrain, SerialExecutor turns */
	INSTANT,
	/** LongRunning: executeLongRunning / submitLongRunning */
	LONG_RUNNING,
	/** not bound to an executor: Future::deferred (run by whoever calls run()) */
	NONE,
};

/**
 * A scheduled or submitted task (design §7.1, §7.2): Java ScheduledFuture/FutureTask. Always held as FutureRef (`Field<FutureRef>` in
 * shared classes, design §7.4).
 *
 * State machine: PENDING → RUNNING → {PENDING (periodic, re-armed) | DONE | FAILED}; PENDING|RUNNING → CANCELLED.
 * - Ownership of the callable (body) and the Pin follows the state machine: the thread that moved the state PENDING → RUNNING owns them until it
 *   leaves RUNNING; a canceller that moved PENDING → CANCELLED owns them afterwards. No other thread touches them, so the callable is never
 *   destroyed while it runs (design §12.1).
 * - cancel(): lock-free CAS. From PENDING the callable and the Pin are released immediately on the cancelling thread (deviation 5); from
 *   RUNNING the body finishes (no interrupts, `mayInterruptIfRunning` is ignored) and is released by the running thread afterwards; a periodic
 *   task is not re-armed. Returns false if already DONE/FAILED/CANCELLED (Java). Wakes get(). Safe from inside the task's own body
 *   (self-cancel) and from any thread; never blocks and never takes a game-level lock, so it is legal inside compute callbacks and Monitors
 *   (design §4.1). Waking a blocked get() briefly takes an internal parking mutex, only if a waiter exists; nothing runs under it.
 * - One-shot tasks release their callable and Pin before they report DONE/FAILED, so get() returning implies the captures are gone.
 * - run() / runNowIfPending(): Java FutureTask.run() on the calling thread (teleport: `task->run(); task->get();`, design §1.5): runs the body
 *   if PENDING, inside a TaskScope with the task's TaskInfo (nested when the caller already has a scope); no-op otherwise. A scheduled task
 *   run this way is later dropped by its pool (Java: the queued ScheduledFutureTask finds its state completed). A periodic task run this way
 *   runs once and stays PENDING with its due time unchanged; if its pool reached the due time while run() was executing the body, the task is
 *   handed back to ThreadPoolManager's installed backend afterwards (same due time), so it keeps running periodically (never silently dropped;
 *   if the hand-back throws, the task is cancelled and the exception propagates from run()).
 * - get(): waits for DONE/FAILED/CANCELLED inside a BlockingRegion("Future.get"). Throws CancellationException if cancelled and
 *   ExecutionException (cause = the body's exception) if FAILED. On a periodic task it returns only by cancellation (CancellationException),
 *   like Java.
 * - Helping get() (design §1.5): on an executor thread of a helping backend (single_executor mode, DeterministicExecutor) get() runs other tasks
 *   through ExecutorBackend::helpWhileWaiting until this one is done or the deadline passes (DeterministicExecutor: the ManualClock is advanced
 *   to the next due time, so a timed get() is deterministic).
 * - get(timeout, unit): additionally TimeoutException.
 * - getDelay(): time until the (next) due time of a SCHEDULED task, negative when overdue, measured on the scheduler clock (the installed
 *   backend's clock, e.g. a ManualClock); still valid after cancel (it keeps the last due time). 0 for tasks of other pools.
 * - Exceptions thrown by execute/schedule/scheduleAtFixedRate bodies are logged as in ThreadPoolManager.java:51-79 (RunnableWrapper, with the
 *   task's call site); a periodic task keeps running after an exception (Java's ScheduledThreadPoolExecutor would stop it; the Java server
 *   wraps bodies so they never throw). submit/submitLongRunning/deferred bodies store the exception (FAILED, get() throws ExecutionException).
 * - Slow-task warnings ("<call site> - execution time: Nms") past the pool's threshold, and RunnableStatsManager records (class Future, method =
 *   call site) when commons.runnablestats.enable is set, as commons ExecuteWrapper does.
 *
 * Thread-safety: all public members are thread-safe.
 */
class Future final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	enum class State : uint8_t { PENDING, RUNNING, DONE, FAILED, CANCELLED };

	/** Type-erased body. One-shot bodies ignore the Future& argument; periodic bodies may use it (e.g. to cancel themselves). */
	using Body = std::move_only_function<void(Future&)>;

	/** Scheduling parameters set by the ThreadPoolManager/backend. */
	struct Schedule {
		PoolKind pool = PoolKind::NONE;
		/** first due time (ignored for PoolKind::INSTANT/LONG_RUNNING/NONE) */
		std::chrono::steady_clock::time_point due{};
		/** fixed-rate period, zero for one-shot tasks */
		std::chrono::nanoseconds period{0};
		/** tie-breaker for equal due times, assigned by the scheduler (FIFO) */
		uint64_t sequence = 0;
		/**
		 * true (execute, schedule, scheduleAtFixedRate): exceptions of the body are logged (RunnableWrapper(catchAndLog = true)) and the task
		 * ends DONE; false (submit, submitLongRunning, deferred): exceptions are stored, the task ends FAILED and get() throws ExecutionException
		 */
		bool logExceptions = true;
	};

	/** @return true if this call cancelled the task */
	bool cancel(bool mayInterruptIfRunning = false) noexcept;
	bool isCancelled() const noexcept;
	/** DONE, FAILED or CANCELLED */
	bool isDone() const noexcept;
	bool isPeriodic() const noexcept;
	/** remaining delay until the (next) due time in `unit`, negative if overdue */
	int64_t getDelay(TimeUnit unit = TimeUnit::MILLISECONDS) const noexcept;

	/** Java FutureTask.run(): runs the body on the calling thread if PENDING (see runNowIfPending). */
	void run();
	/**
	 * Runs the body on the calling thread if the task is PENDING (Java FutureTask.run, CM_TELEPORT_ANIMATION_DONE.java:36-41). Exceptions follow
	 * the task's rules (logged for execute/schedule forms, stored for submit/deferred). @return true if this call ran the body
	 */
	bool runNowIfPending();
	/** @throws CancellationException, ExecutionException */
	void get();
	/** @throws CancellationException, ExecutionException, TimeoutException */
	void get(int64_t timeout, TimeUnit unit);

	/**
	 * A task not bound to any executor, run by an explicit run() (CM_TELEPORT_ANIMATION_DONE.java:36-41). The Pin keeps its owners alive until
	 * the task finished or was cancelled.
	 */
	template <std::invocable F>
	static Ref<Future> deferred(Pin pin, F&& task, std::source_location where = std::source_location::current()) {
		Schedule schedule;
		schedule.logExceptions = false;
		return create(std::move(pin), Body([task = std::forward<F>(task)](Future&) mutable { task(); }), TaskInfo{where, TaskKind::UNKNOWN}, schedule);
	}

	State getState() const noexcept;
	const TaskInfo& getTaskInfo() const noexcept { return info_; }
	/** true if the task's Pin retains `owner` (ThreadPoolManager::tasksPinning, LeakCensus) */
	bool pins(const RefCounted& owner) const noexcept;
	/**
	 * Snapshot of the RefCounted owners the task's Pin retains (identity only, never dereference; nullptr slots are unused, all nullptr once the
	 * task finished or was cancelled). LeakCensus uses it for the "all pins removed from the world" stale-pin rule (design §5.4, C13).
	 */
	std::array<const RefCounted*, Pin::MAX_OWNERS> pinnedOwners() const noexcept;

	// ---------------------------------------------------------------------------------------------------- executor backend interface
	// Used by ThreadPoolManager, ExecutorBackend implementations, SerialExecutor and ForkJoinPool only.

	/** Creates a PENDING task. */
	static Ref<Future> create(Pin pin, Body body, const TaskInfo& info, const Schedule& schedule);

	enum class RunOutcome : uint8_t {
		/** finished (DONE/FAILED) or found cancelled: drop it */
		FINISHED,
		/** periodic task re-armed: getDueTime() holds the next due time (after coalescing, design §1.4) */
		RESCHEDULE,
		/**
		 * not runnable (already running elsewhere or not PENDING): drop it. A periodic task found RUNNING (an explicit run()) is re-armed by that
		 * run() when it finishes, or reported as RESCHEDULE if the run finished before this call returned.
		 */
		NOT_RUN,
	};
	/**
	 * Runs the body once on an executor thread: TaskScope(TaskInfo), ExecuteWrapper semantics (RunnableStatsManager, slow-task warning past
	 * `maxRuntimeWithoutWarningMillis`, exceptions logged and never propagated), then for periodic tasks computes the next due time:
	 * next = previousDue + period, coalesced to now + period when more than `coalesceAfterPeriods` periods (and at least `coalesceMinimumLag`)
	 * behind. noexcept: never lets an exception escape to the pool.
	 */
	RunOutcome runFromExecutor(std::chrono::steady_clock::time_point now, int64_t maxRuntimeWithoutWarningMillis, int32_t coalesceAfterPeriods,
		std::chrono::milliseconds coalesceMinimumLag) noexcept;

	std::chrono::steady_clock::time_point getDueTime() const noexcept;
	std::chrono::nanoseconds getPeriod() const noexcept { return period_; }
	uint64_t getSequence() const noexcept { return sequence_.load(std::memory_order_acquire); }
	void setSequence(uint64_t sequence) noexcept { sequence_.store(sequence, std::memory_order_release); }
	PoolKind getPool() const noexcept { return pool_; }

protected:
	Future(Pin pin, Body body, const TaskInfo& info, const Schedule& schedule);
	~Future() override;

private:
	/** runs the body of a task this thread moved to RUNNING, inside TaskScope(info_); @return true if the body failed with a stored exception */
	bool invokeBody(int64_t maxRuntimeWithoutWarningMillis) noexcept;
	/** leaves RUNNING after invokeBody (re-arming periodic tasks) and wakes waiters */
	RunOutcome finishRun(bool failed, bool rearm, std::chrono::steady_clock::time_point now, int32_t coalesceAfterPeriods,
		std::chrono::milliseconds coalesceMinimumLag) noexcept;
	/** releases the callable and the Pin; called only by the thread that owns them (see the state machine) */
	void releaseCaptures() noexcept;
	/** wakes get() waiters after a transition to a done state */
	void wakeWaiters() noexcept;
	/** waits (or helps) until done; @return false on timeout. `timeoutNanos` < 0 means no timeout */
	bool awaitDone(int64_t timeoutNanos);
	/** throws CancellationException / ExecutionException for CANCELLED / FAILED */
	void reportOutcome() const;

	const TaskInfo info_;
	const PoolKind pool_;
	const std::chrono::nanoseconds period_;
	const bool logExceptions_;
	std::atomic<State> state_{State::PENDING};
	std::atomic<int64_t> dueNanos_{0};
	std::atomic<uint64_t> sequence_{0};
	/** owned by the state machine: accessed only by the thread that moved the state to RUNNING, or by the canceller after PENDING → CANCELLED */
	Body body_;
	Pin pin_;
	/** copies of the Pin's owners for pins(), readable by any thread; cleared before the Pin is released (identity comparison only) */
	std::array<std::atomic<const RefCounted*>, Pin::MAX_OWNERS> pinnedOwners_{};
	/** written by the running thread before the FAILED transition, read by get() after observing FAILED */
	std::exception_ptr failure_;
	/** number of threads blocked in get() (gates the parking notification in wakeWaiters) */
	std::atomic<uint32_t> waiters_{0};
	/** a pool popped this periodic task while an explicit run() owned it: whoever wins the exchange re-arms it (see runFromExecutor) */
	std::atomic<bool> rearmRequested_{false};
};

using FutureRef = Ref<Future>;

} // namespace aion::gameserver::runtime
