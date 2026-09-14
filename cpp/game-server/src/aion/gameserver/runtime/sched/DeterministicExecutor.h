#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/ExecutorBackend.h"

namespace aion::gameserver::runtime {

/**
 * Single-threaded, fully deterministic ExecutorBackend for tests (design §7.7): GameServerHarness, handler conformance, scheduler tests.
 * Install with `ThreadPoolManager::installBackend(std::make_unique<DeterministicExecutor>(clock, seed))` and keep a raw pointer to drive it.
 *
 * - Nothing runs on its own: runReady() runs, on the calling thread, one task at a time until nothing is runnable: a due timer if there is one
 *   (in (due, sequence) order), otherwise the oldest queued INSTANT/LONG_RUNNING task (submission order). Tasks submitted while runReady() runs
 *   are executed in the same call once they are due. After each task the task's Refs are dropped and Reclaimer::reclaimNow() runs, so
 *   destruction is exact and ASan reports use-after-free at the first bad access. (A CleanerDrain is an ordinary instant task posted by a
 *   Reclaimer post-scan hook, so it runs in the same runReady() call.)
 * - advance(dt) moves the ManualClock forward to each intermediate due time (in order), running runReady() at each, then to now + dt. A periodic
 *   task therefore runs at every period in between; to test coalescing, jump the clock with ManualClock::advance and call runReady().
 * - Periodic tasks, exceptions, slow-task thresholds and coalescing follow ThreadPoolManager's configuration exactly like the real pools.
 * - The rndSeed seeds the calling thread's commons Rnd generator (Rnd::generator() = Xoshiro256PlusPlus(seed)); the previous generator state is
 *   restored by retire() (installBackend) or the destructor when it runs on the same thread and no later executor seeded that thread since
 *   (the new executor is created before installBackend retires the old one).
 * - isExecutorThread() is true on the thread that created the executor. Future::get() there helps (design §1.5): it runs ready tasks, and when
 *   nothing is ready it advances the ManualClock to the next due time (a blocked thread lets time pass); a timed get() whose deadline comes
 *   first advances the clock to the deadline and throws TimeoutException. An untimed get() with no timer and no queued task can only be
 *   completed by another thread; it then polls in real time and logs a warning after 5 s.
 * - shutdown(): cancels timers, runs the queued tasks on the calling thread, later submissions are cancelled. retire() (called by
 *   ThreadPoolManager::installBackend when the executor is replaced) and the destructor cancel every pending task (releasing captures).
 * Thread-safety: submissions (execute/schedule) and queries are thread-safe; runReady/advance/runOneTask must be called from one thread.
 */
class DeterministicExecutor final : public ExecutorBackend {
public:
	DeterministicExecutor(ManualClock& clock, uint64_t rndSeed);
	~DeterministicExecutor() override;
	DeterministicExecutor(const DeterministicExecutor&) = delete;
	DeterministicExecutor& operator=(const DeterministicExecutor&) = delete;

	/** Runs everything that is runnable now (see above). @return number of tasks run */
	size_t runReady();
	/** Advances the clock by dt, running due tasks at their due times. @return number of tasks run */
	size_t advance(std::chrono::milliseconds dt);
	/** Tasks queued or scheduled but not yet run (cancelled shells excluded). */
	size_t pendingTaskCount() const;
	/** design name */
	size_t pendingTasksCount() const { return pendingTaskCount(); }
	/** due time of the earliest pending timer, if any */
	std::optional<std::chrono::steady_clock::time_point> nextDueTime() const;
	ManualClock& manualClock() noexcept { return clock_; }
	uint64_t getRndSeed() const noexcept { return rndSeed_; }

	const Clock& clock() const noexcept override { return clock_; }
	void execute(PoolKind pool, FutureRef task) override;
	void schedule(FutureRef task) override;
	bool shutdown(std::chrono::milliseconds awaitTermination) override;
	bool isShutdown() const noexcept override;
	/** cancels every pending task (captures released; queued tasks do NOT run) and restores the Rnd generator as described above */
	void retire() noexcept override;
	bool isExecutorThread() const noexcept override;
	bool runOneTask() override;
	HelpResult helpWhileWaiting(Future& task, std::chrono::steady_clock::time_point deadline) override;
	std::vector<std::string> getStats() const override;
	std::vector<FutureRef> pendingTasks() const override;

private:
	struct State;

	ManualClock& clock_;
	const uint64_t rndSeed_;
	std::unique_ptr<State> state_;
};

} // namespace aion::gameserver::runtime
