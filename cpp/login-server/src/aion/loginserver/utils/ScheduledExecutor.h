#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace aion::loginserver::utils {

/**
 * A single thread executing periodic tasks at a fixed rate (Java: Executors.newSingleThreadScheduledExecutor() with scheduleAtFixedRate, used by
 * GsConnection.PINGPONG_EXECUTOR and PlayerTransferService).
 * <p>
 * Semantics follow Java's ScheduledThreadPoolExecutor with its default policies:
 * <ul>
 * <li>A task first runs after initialDelay, then at initialDelay + n * period. If a run takes longer than the period, the following runs start
 * late but never concurrently (they catch up one after another).</li>
 * <li>cancel() prevents further runs; a run in progress completes.</li>
 * <li>shutdown() cancels all periodic tasks, rejects new ones and lets the thread exit after the task it is running (if any).</li>
 * </ul>
 * Deviation: an exception escaping a task is logged (Java silently suppresses the following runs); the task is not run again, like in Java.
 * <p>
 * Thread safe. The destructor shuts the executor down and joins its thread (it must not be destroyed by one of its own tasks).
 */
class ScheduledExecutor {
public:
	/** Handle of a scheduled task (Java: ScheduledFuture). */
	class ScheduledFuture {
	public:
		/** Java: cancel(false) - the task will not run again; a run in progress is not interrupted. */
		void cancel() noexcept { cancelled = true; }
		bool isCancelled() const noexcept { return cancelled; }

	private:
		friend class ScheduledExecutor;
		std::atomic<bool> cancelled = false;
	};

	/** @param threadName name of the executor thread (shown in log lines) */
	explicit ScheduledExecutor(std::string threadName);
	~ScheduledExecutor();

	ScheduledExecutor(const ScheduledExecutor&) = delete;
	ScheduledExecutor& operator=(const ScheduledExecutor&) = delete;

	/**
	 * Java: scheduleAtFixedRate(task, initialDelay, period, unit)
	 *
	 * @throws commons::utils::IllegalStateException if the executor was shut down (Java: RejectedExecutionException)
	 * @throws commons::utils::IllegalArgumentException if period is not positive or the task is empty
	 */
	std::shared_ptr<ScheduledFuture> scheduleAtFixedRate(std::function<void()> task, std::chrono::milliseconds initialDelay,
		std::chrono::milliseconds period);

	/** Like scheduleAtFixedRate(task, ...), but the task gets its own future, e.g. to cancel itself. */
	std::shared_ptr<ScheduledFuture> scheduleAtFixedRate(std::function<void(ScheduledFuture& future)> task, std::chrono::milliseconds initialDelay,
		std::chrono::milliseconds period);

	/** Java: shutdown() - cancels all periodic tasks; the thread ends after its current run. Calling it again does nothing. */
	void shutdown();

	/** Java: isTerminated() - true once the executor was shut down and its thread has finished. */
	bool isTerminated() const;

	/**
	 * Java: awaitTermination(timeout) - waits until the thread has finished after a shutdown.
	 * @return true if terminated, false if the timeout elapsed first
	 */
	bool awaitTermination(std::chrono::milliseconds timeout);

private:
	struct Entry {
		std::function<void(ScheduledFuture& future)> task;
		std::chrono::steady_clock::time_point nextRun;
		std::chrono::milliseconds period;
		std::shared_ptr<ScheduledFuture> future;
	};

	void run();

	mutable std::mutex mutex;
	std::condition_variable changed;
	std::condition_variable terminatedCondition;
	std::vector<Entry> entries; // guarded by mutex
	bool shutDown = false; // guarded by mutex
	bool terminated = false; // guarded by mutex
	const std::string threadName;
	std::thread thread;
};

} // namespace aion::loginserver::utils
