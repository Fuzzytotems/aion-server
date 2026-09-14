#pragma once

#include <chrono>
#include <cstddef>
#include <concepts>
#include <cstdint>
#include <memory>
#include <source_location>
#include <string>
#include <utility>
#include <vector>

#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/sched/ExecutorBackend.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/runtime/sched/Pin.h"
#include "aion/gameserver/runtime/sched/PinnedCallback.h"
#include "aion/gameserver/runtime/sched/TaskConcepts.h"
#include "aion/gameserver/runtime/sched/TimeUnit.h"

namespace aion::gameserver::utils {

// The design declares these in aion::gameserver::utils (§7.1); they live in the runtime kernel and are re-exported here.
using runtime::bindTask;
using runtime::Future;
using runtime::FutureRef;
using runtime::Pin;
using runtime::PinnedCallback;
using runtime::TaskArg;
using runtime::TaskStruct;
using runtime::TimeUnit;
using runtime::UnpinnedPeriodicTask;
using runtime::UnpinnedTask;

/**
 * Java: com.aionemu.gameserver.utils.ThreadPoolManager (design §1.1, §7.1-§7.3). Scheduled, instant and long-running pools with Java names.
 *
 * Task forms (design §7.1, §7.3; `this`/`&name` captures of pinned lambdas must be in the pin list, lint L5):
 * - pinned:      schedule(Pin, invocable, delay[, unit]), schedule({this, &effect}, [this, &effect] {...}, delay)
 * - unpinned:    schedule(UnpinnedTask, delay[, unit]) - captureless lambda, TaskStruct with TaskArg members, or bindTask(fn, args...)
 * - member:      schedule(this, &X::method, delay) - pins `this` (or its owner for parts)
 * scheduleAtFixedRate has the same forms (body invocable without arguments or with Future&, delay and period in ms); execute /
 * executeLongRunning / submit / submitLongRunning have the pinned and unpinned forms. Every form records the call site (std::source_location).
 *
 * Semantics (design §7.2, ThreadPoolManager.java)
 * - The pools are created by the first getInstance() (Java: SingletonHolder), unless a backend was installed before (tests). Pool sizes:
 *   instant = max(4, BASE_THREAD_POOL_SIZE or cores), scheduled = max(4, SCHEDULED_THREAD_POOL_SIZE or cores), long running = cached (60 s
 *   idle). Instant queue 100,000 with the Java rejection policy (CONVENTIONS.md: warn, then a new thread above normal priority, else the
 *   caller). See runtime::ThreadPoolBackend.
 * - Delays: negative delays count as 0 (Java triggerTime); delays are measured on the backend's clock (ManualClock in tests).
 * - Bodies run in a TaskScope with their call site, through RunnableWrapper semantics: execute/schedule/scheduleAtFixedRate log exceptions and
 *   warn past MAXIMUM_RUNTIME_IN_MILLISEC_WITHOUT_WARNING; submit/submitLongRunning capture exceptions for Future::get.
 * - Fixed rate: next = previousDue + period, never overlapping; more than `coalesceAfterPeriods` periods (and at least 2 s) behind → run once and
 *   realign (deviation 6).
 * - Cancelled tasks release their captures immediately (deviation 5).
 * - Debug modes (design §1.6): Config::singleExecutor runs every pool on one thread (runtime::SingleExecutorBackend) and makes
 *   ForkJoinPool::commonPool() serial; get() on that thread helps (design §1.5). Config::serialMovement is read by the movement code.
 * - shutdown(): Java sequence and log lines; delayed tasks are cancelled and dropped, running and queued instant/long-running tasks get 5 s.
 *   Tasks submitted afterwards are cancelled and dropped (Java's AionRejectedExecutionHandler drops them silently; the cancellation additionally
 *   releases their captures and makes get() throw instead of blocking forever). The final CleanerDrain of design §11 step 6 is run by its
 *   owner (services) after this call.
 * - A backend installed with installBackend (tests: DeterministicExecutor) replaces the real pools; install it before the first task. Replaced
 *   backends are retired and kept alive, never destroyed (see installBackend).
 *
 * Exceptions: IllegalArgumentException for a fixed-rate period <= 0 (Java). Scheduling methods never block and never take a game-level lock
 * (only a SCHEDULER leaf mutex for the queue operation), so they are legal inside Monitors and compute callbacks; they must not be called
 * while a leaf mutex of rank >= SCHEDULER is held (runtime internals only).
 * Thread-safety: all members are thread-safe.
 */
class ThreadPoolManager final {
public:
	/** Configuration (Java ThreadConfig + gameserver.scheduler.* / gameserver.debug.*). Applied by configure() before the first getInstance(). */
	struct Config {
		/** ThreadConfig.BASE_THREAD_POOL_SIZE (0 = number of cores) */
		int32_t baseThreadPoolSize = 0;
		/** ThreadConfig.SCHEDULED_THREAD_POOL_SIZE (0 = number of cores) */
		int32_t scheduledThreadPoolSize = 0;
		/** ThreadConfig.USE_PRIORITIES (instant pool threads at priority 7) */
		bool usePriorities = false;
		/** ThreadConfig.MAXIMUM_RUNTIME_IN_MILLISEC_WITHOUT_WARNING */
		int64_t maximumRuntimeInMillisecWithoutWarning = 5000;
		int32_t instantQueueCapacity = 100'000;
		/** gameserver.scheduler.coalesce_after (periods; <= 0 disables coalescing). Read at run time, so configure() may change it later. */
		int32_t coalesceAfterPeriods = 10;
		std::chrono::milliseconds coalesceMinimumLag{2000};
		/** gameserver.debug.single_executor */
		bool singleExecutor = false;
		/** gameserver.debug.serial_movement (applied to ForkJoinPool::commonPool by the movement code) */
		bool serialMovement = false;
	};

	static ThreadPoolManager& getInstance();

	/**
	 * Sets the configuration. Pool sizes, priorities, the queue capacity and singleExecutor apply when the default pools are created; the
	 * slow-task threshold and coalescing parameters apply to every later run, also on an installed backend.
	 * @throws IllegalStateException if the default pools were already created
	 */
	static void configure(const Config& config);

	/**
	 * Replaces the pools with `backend` (tests: DeterministicExecutor; debug: single executor). nullptr restores the default: pools are created
	 * again from the configuration on next use. Clears the shutdown state.
	 * Order: the previous backend is first retired while it is still installed (ExecutorBackend::retire: its pending timers are cancelled, later
	 * submissions to it are cancelled and dropped, its threads are joined, so running tasks finish first), and only then is the replacement
	 * published. Tasks and hooks racing with the call therefore see either the old (shut down) backend or the new one, never a missing backend
	 * that would lazily create the default pools. The previous backend object is kept alive until process exit (never destroyed), so lock-free
	 * readers that loaded it (backend(), installedBackend(), clock(), Future::getDelay/get, Reclaimer post-scan hooks) stay memory-safe.
	 * Calls are serialized. Must not be called from a thread of the installed backend (it would join itself); submissions made concurrently
	 * may be cancelled (test and startup use).
	 */
	static void installBackend(std::unique_ptr<runtime::ExecutorBackend> backend);

	/**
	 * The installed backend (explicitly installed or the lazily created default pools), or nullptr; never creates the pools. The pointer stays
	 * valid for the rest of the process (see installBackend). For kernel services running outside the game's task flow (Reclaimer post-scan
	 * hooks: CleanerQueue, LeakCensus), which must not create pools after a test teardown.
	 */
	static runtime::ExecutorBackend* installedBackend() noexcept;
	/** The installed backend's clock (ManualClock under a DeterministicExecutor), or SystemClock if none is installed; never creates the pools. */
	static const runtime::Clock& clock() noexcept;

	/**
	 * submit(task) on the installed backend if there is one, else nothing (returns nullptr); never creates the pools. After shutdown or during
	 * an installBackend the task is cancelled and dropped like any submission. Kernel services only (post-scan hooks).
	 */
	template <UnpinnedTask F>
	static FutureRef submitIfInstalled(F&& task, std::source_location where = std::source_location::current()) {
		return submitToInstalled(runtime::PoolKind::INSTANT, Pin(), oneShot(std::forward<F>(task)), where, runtime::TaskKind::INSTANT, false);
	}
	/** execute(pin, task) on the installed backend if there is one (see submitIfInstalled). @return false if no backend is installed */
	template <std::invocable F>
	static bool executeIfInstalled(Pin pin, F&& task, std::source_location where = std::source_location::current()) {
		return submitToInstalled(runtime::PoolKind::INSTANT, std::move(pin), oneShot(std::forward<F>(task)), where, runtime::TaskKind::INSTANT, true) !=
			nullptr;
	}

	// ------------------------------------------------------------------------------------------------------------------------- schedule

	template <std::invocable F>
	FutureRef schedule(Pin pin, F&& task, int64_t delay, TimeUnit unit = TimeUnit::MILLISECONDS,
		std::source_location where = std::source_location::current()) {
		return submitScheduled(std::move(pin), oneShot(std::forward<F>(task)), delay, unit, 0, where, runtime::TaskKind::SCHEDULED);
	}

	template <UnpinnedTask F>
	FutureRef schedule(F&& task, int64_t delay, TimeUnit unit = TimeUnit::MILLISECONDS, std::source_location where = std::source_location::current()) {
		return submitScheduled(Pin(), oneShot(std::forward<F>(task)), delay, unit, 0, where, runtime::TaskKind::SCHEDULED);
	}

	template <runtime::Pinnable Self>
	FutureRef schedule(Self* self, void (Self::*method)(), int64_t delay, std::source_location where = std::source_location::current()) {
		return submitScheduled(Pin(self), oneShot([self, method] { (self->*method)(); }), delay, TimeUnit::MILLISECONDS, 0, where,
			runtime::TaskKind::SCHEDULED);
	}

	template <runtime::Pinnable Self>
	FutureRef schedule(const Self* self, void (Self::*method)() const, int64_t delay, std::source_location where = std::source_location::current()) {
		return submitScheduled(Pin(self), oneShot([self, method] { (self->*method)(); }), delay, TimeUnit::MILLISECONDS, 0, where,
			runtime::TaskKind::SCHEDULED);
	}

	// ------------------------------------------------------------------------------------------------------------- scheduleAtFixedRate

	/** body: invocable without arguments or with Future& */
	template <class F>
		requires std::invocable<F&> || std::invocable<F&, Future&>
	FutureRef scheduleAtFixedRate(Pin pin, F&& task, int64_t delay, int64_t period, std::source_location where = std::source_location::current()) {
		checkPeriod(period);
		return submitScheduled(std::move(pin), periodic(std::forward<F>(task)), delay, TimeUnit::MILLISECONDS, period, where,
			runtime::TaskKind::SCHEDULED);
	}

	template <class F>
		requires UnpinnedTask<F> || UnpinnedPeriodicTask<F>
	FutureRef scheduleAtFixedRate(F&& task, int64_t delay, int64_t period, std::source_location where = std::source_location::current()) {
		checkPeriod(period);
		return submitScheduled(Pin(), periodic(std::forward<F>(task)), delay, TimeUnit::MILLISECONDS, period, where, runtime::TaskKind::SCHEDULED);
	}

	template <runtime::Pinnable Self>
	FutureRef scheduleAtFixedRate(Self* self, void (Self::*method)(), int64_t delay, int64_t period,
		std::source_location where = std::source_location::current()) {
		checkPeriod(period);
		return submitScheduled(Pin(self), periodic([self, method] { (self->*method)(); }), delay, TimeUnit::MILLISECONDS, period, where,
			runtime::TaskKind::SCHEDULED);
	}

	// ----------------------------------------------------------------------------------------------------------------- instant pool

	template <std::invocable F>
	void execute(Pin pin, F&& task, std::source_location where = std::source_location::current()) {
		(void)submitNow(runtime::PoolKind::INSTANT, std::move(pin), oneShot(std::forward<F>(task)), where, runtime::TaskKind::INSTANT, true);
	}
	template <UnpinnedTask F>
	void execute(F&& task, std::source_location where = std::source_location::current()) {
		(void)submitNow(runtime::PoolKind::INSTANT, Pin(), oneShot(std::forward<F>(task)), where, runtime::TaskKind::INSTANT, true);
	}
	template <std::invocable F>
	FutureRef submit(Pin pin, F&& task, std::source_location where = std::source_location::current()) {
		return submitNow(runtime::PoolKind::INSTANT, std::move(pin), oneShot(std::forward<F>(task)), where, runtime::TaskKind::INSTANT, false);
	}
	template <UnpinnedTask F>
	FutureRef submit(F&& task, std::source_location where = std::source_location::current()) {
		return submitNow(runtime::PoolKind::INSTANT, Pin(), oneShot(std::forward<F>(task)), where, runtime::TaskKind::INSTANT, false);
	}

	// ------------------------------------------------------------------------------------------------------------ long-running pool

	template <std::invocable F>
	void executeLongRunning(Pin pin, F&& task, std::source_location where = std::source_location::current()) {
		(void)submitNow(runtime::PoolKind::LONG_RUNNING, std::move(pin), oneShot(std::forward<F>(task)), where, runtime::TaskKind::LONG_RUNNING, true);
	}
	template <UnpinnedTask F>
	void executeLongRunning(F&& task, std::source_location where = std::source_location::current()) {
		(void)submitNow(runtime::PoolKind::LONG_RUNNING, Pin(), oneShot(std::forward<F>(task)), where, runtime::TaskKind::LONG_RUNNING, true);
	}
	template <std::invocable F>
	FutureRef submitLongRunning(Pin pin, F&& task, std::source_location where = std::source_location::current()) {
		return submitNow(runtime::PoolKind::LONG_RUNNING, std::move(pin), oneShot(std::forward<F>(task)), where, runtime::TaskKind::LONG_RUNNING,
			false);
	}
	template <UnpinnedTask F>
	FutureRef submitLongRunning(F&& task, std::source_location where = std::source_location::current()) {
		return submitNow(runtime::PoolKind::LONG_RUNNING, Pin(), oneShot(std::forward<F>(task)), where, runtime::TaskKind::LONG_RUNNING, false);
	}

	// ----------------------------------------------------------------------------------------------------------------- introspection

	/** Call sites of pending tasks whose Pin retains `owner` (`//tasks <target>`, LeakCensus, design §5.4, §7.8). */
	std::vector<runtime::TaskInfo> tasksPinning(const runtime::RefCounted& owner) const;
	/**
	 * `//debug tasks` (design §7.8): pending tasks grouped by call site, most frequent first, one line per site
	 * ("<count> x <kind> task <file>:<line> (<function>) [periodic]"), at most `maxLines` lines plus a total line.
	 */
	std::vector<std::string> getPendingTaskSummary(size_t maxLines = 50) const;
	/** Java getStats() */
	std::vector<std::string> getStats() const;
	/** Java shutdown() */
	void shutdown();
	bool isShutdown() const noexcept;
	/** the active backend (never null after getInstance) */
	runtime::ExecutorBackend& backend() const noexcept;
	const Config& getConfig() const noexcept;

private:
	ThreadPoolManager();

	/** Java ScheduledThreadPoolExecutor.scheduleAtFixedRate: period <= 0 → IllegalArgumentException */
	static void checkPeriod(int64_t period);

	template <class F>
	static Future::Body oneShot(F&& task) {
		return Future::Body([task = std::forward<F>(task)](Future&) mutable { task(); });
	}
	template <class F>
	static Future::Body periodic(F&& task) {
		if constexpr (std::invocable<std::decay_t<F>&, Future&>)
			return Future::Body([task = std::forward<F>(task)](Future& future) mutable { task(future); });
		else
			return Future::Body([task = std::forward<F>(task)](Future&) mutable { task(); });
	}

	FutureRef submitScheduled(Pin pin, Future::Body body, int64_t delay, TimeUnit unit, int64_t periodMillis, const std::source_location& where,
		const char* kind);
	FutureRef submitNow(runtime::PoolKind pool, Pin pin, Future::Body body, const std::source_location& where, const char* kind, bool logExceptions);
	static FutureRef submitToInstalled(runtime::PoolKind pool, Pin pin, Future::Body body, const std::source_location& where, const char* kind,
		bool logExceptions);
};

} // namespace aion::gameserver::utils
