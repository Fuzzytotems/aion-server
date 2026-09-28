#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/ExecutorBackend.h"

namespace aion::gameserver::runtime {

/**
 * The production ExecutorBackend: Java ThreadPoolManager's three pools (design §1.1, §7.2). Created by ThreadPoolManager from its Config; tests
 * may create one directly and install it.
 *
 * - ScheduledPool ("ScheduledPool-n", `scheduledThreads` threads, normal priority): one (due, sequence) TimerHeap behind a SCHEDULER leaf
 *   mutex. One worker (the leader) sleeps until the earliest due time, the others wait untimed (leader/follower, like Java's DelayedWorkQueue),
 *   so a due time wakes one thread. Periodic tasks are re-armed after they ran (never overlapping), with coalescing (design §1.4). Cancelled
 *   shells are purged when they exceed 50% of the heap.
 * - InstantPool ("InstantPool-n", `instantThreads` threads, Java priority 7 when usePriorities): FIFO queue bounded by `instantQueueCapacity`.
 *   A full queue applies AionRejectedExecutionHandler: warn with a RejectedExecutionException, then run the task on a new thread ("Thread-n") if
 *   the caller's Java priority is above normal, else on the calling thread.
 * - LongRunning ("LongRunning-n"): Java Executors.newCachedThreadPool: an idle thread takes the task, otherwise a new thread starts; threads end
 *   after `longRunningKeepAlive` idle. No slow-task warnings.
 * - Every run goes through Future::runFromExecutor (TaskScope, RunnableWrapper semantics, re-arming). The slow-task threshold and coalescing
 *   parameters are read from ThreadPoolManager's configuration at run time.
 * - shutdown(await): Java ThreadPoolManager.shutdown: delayed and periodic tasks are cancelled and dropped
 *   (setExecuteExistingDelayedTasksAfterShutdownPolicy(false)); running tasks and queued instant/long-running tasks finish; waits up to `await`
 *   for all threads to end. Later submissions are cancelled and dropped. The destructor shuts down and joins every thread (it waits for running
 *   tasks without a time limit; never destroy the backend from one of its own threads).
 * Thread-safety: all members are thread-safe.
 */
class ThreadPoolBackend final : public ExecutorBackend {
public:
	struct Options {
		int32_t instantThreads = 4;
		int32_t scheduledThreads = 4;
		/** Java priority of instant pool threads (ThreadConfig.USE_PRIORITIES ? 7 : NORM_PRIORITY) */
		int32_t instantThreadPriority = 5;
		size_t instantQueueCapacity = 100'000;
		std::chrono::milliseconds longRunningKeepAlive{60'000};
	};

	explicit ThreadPoolBackend(const Options& options);
	~ThreadPoolBackend() override;
	ThreadPoolBackend(const ThreadPoolBackend&) = delete;
	ThreadPoolBackend& operator=(const ThreadPoolBackend&) = delete;

	const Clock& clock() const noexcept override;
	void execute(PoolKind pool, FutureRef task) override;
	void schedule(FutureRef task) override;
	bool shutdown(std::chrono::milliseconds awaitTermination) override;
	bool isShutdown() const noexcept override;
	/** shutdown(0) and join every thread (waits for running and queued tasks without a time limit); the destructor does the same */
	void retire() noexcept override;
	bool isExecutorThread() const noexcept override;
	/** always false: worker pools do not help get() */
	bool runOneTask() override;
	std::vector<std::string> getStats() const override;
	std::vector<FutureRef> pendingTasks() const override;

	/** pool sizes as created (Java getPoolSize) */
	int32_t getInstantPoolSize() const noexcept;
	int32_t getScheduledPoolSize() const noexcept;
	/** threads currently alive in the long running pool */
	int32_t getLongRunningPoolSize() const noexcept;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

/**
 * `gameserver.debug.single_executor` backend (design §1.6): one thread ("SingleExecutor") runs every pool. Due timers run in (due, sequence)
 * order before queued instant/long-running tasks. Future::get on that thread helps: it runs other due or queued tasks until the awaited task is
 * done or its timeout passes (design §1.5); when nothing is runnable it sleeps until the next due time, re-checking at least every 10 ms (a
 * canceller on another thread, e.g. an IO thread, does not notify the executor).
 * The instant queue keeps its capacity and rejection policy (the executor thread has normal priority, so rejected tasks run on the caller).
 * shutdown and destruction as ThreadPoolBackend.
 */
class SingleExecutorBackend final : public ExecutorBackend {
public:
	explicit SingleExecutorBackend(size_t instantQueueCapacity = 100'000);
	~SingleExecutorBackend() override;
	SingleExecutorBackend(const SingleExecutorBackend&) = delete;
	SingleExecutorBackend& operator=(const SingleExecutorBackend&) = delete;

	const Clock& clock() const noexcept override;
	void execute(PoolKind pool, FutureRef task) override;
	void schedule(FutureRef task) override;
	bool shutdown(std::chrono::milliseconds awaitTermination) override;
	bool isShutdown() const noexcept override;
	/** shutdown(0) and join the executor thread (queued tasks run first); the destructor does the same */
	void retire() noexcept override;
	bool isExecutorThread() const noexcept override;
	bool runOneTask() override;
	HelpResult helpWhileWaiting(Future& task, std::chrono::steady_clock::time_point deadline) override;
	std::vector<std::string> getStats() const override;
	std::vector<FutureRef> pendingTasks() const override;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

} // namespace aion::gameserver::runtime
