#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::runtime {

/**
 * The machinery behind ThreadPoolManager (design §7.1 `installBackend`, §7.7): real thread pools in production (ThreadPoolBackend, created
 * by ThreadPoolManager from its Config), DeterministicExecutor in tests, or a single-thread backend in `gameserver.debug.single_executor`.
 *
 * Contract for implementations
 * - execute(): runs `task` as soon as possible on the given pool (INSTANT: bounded queue of Config::instantQueueCapacity with the Java
 *   rejection policy of CONVENTIONS.md (AionRejectedExecutionHandler): warn, then run on a new thread if the caller's Java priority is above
 *   normal, else on the caller; LONG_RUNNING: cached threads, 60 s idle).
 * - schedule(): the SCHEDULED pool's (due, sequence) heap behind a SCHEDULER leaf mutex; cancelled shells are purged when they exceed 50%.
 * - Every run goes through Future::runFromExecutor (TaskScope, stats, exceptions, re-arming); RESCHEDULE results are pushed back into the heap.
 * - Tasks submitted after shutdown() are dropped like Java's AionRejectedExecutionHandler does for a shut down executor (it returns silently);
 *   unlike Java the dropped task is cancelled, so its captures are released and a get() on it throws CancellationException instead of
 *   blocking forever.
 * - Worker threads name themselves like Java ("ScheduledPool-1", "InstantPool-1", "LongRunning-1") and refresh their ThreadContext name.
 * Thread-safety: all members are thread-safe.
 */
class ExecutorBackend {
public:
	virtual ~ExecutorBackend() = default;

	virtual const Clock& clock() const noexcept = 0;

	/** Runs the task on `pool` (INSTANT or LONG_RUNNING; SCHEDULED is treated as INSTANT). After shutdown the task is cancelled and dropped. */
	virtual void execute(PoolKind pool, FutureRef task) = 0;
	/** Adds the task to the scheduled pool at task->getDueTime(). After shutdown the task is cancelled and dropped. */
	virtual void schedule(FutureRef task) = 0;

	/**
	 * Java ThreadPoolManager.shutdown: drops delayed tasks (setExecuteExistingDelayedTasksAfterShutdownPolicy(false)), lets running and queued
	 * instant/long-running tasks finish for up to `awaitTermination`, then returns.
	 * @return true if all pools terminated in time
	 */
	virtual bool shutdown(std::chrono::milliseconds awaitTermination) = 0;
	virtual bool isShutdown() const noexcept = 0;

	/**
	 * Takes the backend out of service for ThreadPoolManager::installBackend, without destroying it: later submissions are cancelled and
	 * dropped, pending timers are cancelled (captures released), and every worker thread is joined (running tasks finish; queued tasks follow
	 * the implementation's destructor semantics). Afterwards the object stays valid forever for callers that loaded it lock-free before the
	 * swap (clock(), execute/schedule, pendingTasks, getStats all remain callable). Idempotent. Must not be called from one of the backend's
	 * own threads (self-join). The destructor performs the same steps for backends that are destroyed directly.
	 */
	virtual void retire() noexcept = 0;

	/** true if the calling thread is one of this backend's executor threads (single_executor helping get(), design §1.5) */
	virtual bool isExecutorThread() const noexcept = 0;
	/**
	 * Runs one due or queued task on the calling executor thread if there is one (helping get() in single_executor mode).
	 * @return false if nothing was runnable
	 */
	virtual bool runOneTask() = 0;

	/** Result of helpWhileWaiting. */
	enum class HelpResult : uint8_t {
		/** this backend does not help on the calling thread: the caller blocks normally */
		NOT_HELPING,
		/** the task is done */
		DONE,
		/** the deadline passed first */
		TIMED_OUT,
	};
	/**
	 * Helping Future::get (design §1.5): if the calling thread is an executor thread of a backend that runs every pool on one thread
	 * (single_executor, DeterministicExecutor), runs other due or queued tasks until `task` is done or `deadline` (on clock(); time_point::max()
	 * for no timeout) passes. Backends with real worker pools return NOT_HELPING (the default), since running unrelated tasks inside a task
	 * would change lock nesting and scopes.
	 */
	virtual HelpResult helpWhileWaiting(Future& task, std::chrono::steady_clock::time_point deadline) {
		(void)task;
		(void)deadline;
		return HelpResult::NOT_HELPING;
	}

	/** Java getStats() lines (pool sizes, active, completed, queued counts). */
	virtual std::vector<std::string> getStats() const = 0;
	/** pending (not yet started) tasks, for `//debug tasks` and tasksPinning */
	virtual std::vector<FutureRef> pendingTasks() const = 0;
};

} // namespace aion::gameserver::runtime
