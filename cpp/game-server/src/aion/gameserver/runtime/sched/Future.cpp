#include "aion/gameserver/runtime/sched/Future.h"

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <string>

#include "aion/commons/configs/CommonsConfig.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/commons/utils/concurrent/RunnableStatsManager.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/YieldPoint.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/ExecutorBackend.h"
#include "aion/gameserver/runtime/sched/detail/SchedRuntime.h"
#include "aion/gameserver/runtime/sync/BlockingRegion.h"

namespace aion::gameserver::runtime {

namespace {

/** Java: the ExecuteWrapper logger (RunnableWrapper runs bodies through ExecuteWrapper). */
const commons::logging::Logger& executeWrapperLog() {
	static const auto* instance =
		new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.commons.utils.concurrent.ExecuteWrapper"));
	return *instance;
}

/**
 * Parking lot for blocked get() calls: a fixed table of mutex/condition variable stripes indexed by the Future's address. Waiters register in
 * Future::waiters_ before checking the state under the stripe mutex; wakers store the state first and only then read waiters_, so at least one
 * of the two sees the other (no lost wake-up). Nothing but the state check and the wait happens under a stripe mutex.
 */
struct ParkingStripe {
	std::mutex mutex;
	std::condition_variable condition;
};

constexpr size_t PARKING_STRIPES = 64;

ParkingStripe& parkingFor(const Future* future) noexcept {
	static auto* stripes = new ParkingStripe[PARKING_STRIPES];
	auto address = reinterpret_cast<uintptr_t>(future);
	return stripes[(address >> 6 ^ address >> 12) % PARKING_STRIPES];
}

int64_t toNanosSinceEpoch(std::chrono::steady_clock::time_point time) noexcept {
	return std::chrono::duration_cast<std::chrono::nanoseconds>(time.time_since_epoch()).count();
}

} // namespace

Future::Future(Pin pin, Body body, const TaskInfo& info, const Schedule& schedule)
	: info_(info), pool_(schedule.pool), period_(schedule.period), logExceptions_(schedule.logExceptions),
		dueNanos_(toNanosSinceEpoch(schedule.due)), sequence_(schedule.sequence), body_(std::move(body)), pin_(std::move(pin)) {
	for (size_t i = 0; i < pin_.size(); ++i)
		pinnedOwners_[i].store(pin_.owner(i), std::memory_order_release);
}

Future::~Future() = default;

Ref<Future> Future::create(Pin pin, Body body, const TaskInfo& info, const Schedule& schedule) {
	if (schedule.period.count() < 0)
		throw IllegalArgumentException("Future period must not be negative");
	if (!body)
		throw NullPointerException("Future body is empty");
	return makeRef<Future>(std::move(pin), std::move(body), info, schedule);
}

// ------------------------------------------------------------------------------------------------------------------------ state machine

bool Future::cancel(bool) noexcept {
	for (;;) {
		AION_YIELD_POINT("Future::cancel.load");
		State state = state_.load(std::memory_order_acquire);
		if (state == State::PENDING) {
			AION_YIELD_POINT("Future::cancel.casPending");
			if (state_.compare_exchange_strong(state, State::CANCELLED, std::memory_order_acq_rel)) {
				releaseCaptures(); // this thread owns the callable now (deviation 5: captures released immediately)
				wakeWaiters();
				return true;
			}
		} else if (state == State::RUNNING) {
			AION_YIELD_POINT("Future::cancel.casRunning");
			if (state_.compare_exchange_strong(state, State::CANCELLED, std::memory_order_acq_rel)) {
				wakeWaiters(); // the running thread releases the callable when the body returns
				return true;
			}
		} else {
			return false;
		}
	}
}

bool Future::isCancelled() const noexcept {
	return state_.load(std::memory_order_acquire) == State::CANCELLED;
}

bool Future::isDone() const noexcept {
	State state = state_.load(std::memory_order_acquire);
	return state == State::DONE || state == State::FAILED || state == State::CANCELLED;
}

bool Future::isPeriodic() const noexcept {
	return period_.count() > 0;
}

int64_t Future::getDelay(TimeUnit unit) const noexcept {
	if (pool_ != PoolKind::SCHEDULED)
		return 0;
	int64_t now = toNanosSinceEpoch(sched_detail::schedulerClock().now());
	return fromNanos(dueNanos_.load(std::memory_order_acquire) - now, unit); // due and now are non-negative steady times: no overflow
}

Future::State Future::getState() const noexcept {
	return state_.load(std::memory_order_acquire);
}

bool Future::pins(const RefCounted& owner) const noexcept {
	for (const auto& pinned : pinnedOwners_)
		if (pinned.load(std::memory_order_acquire) == &owner)
			return true;
	return false;
}

std::array<const RefCounted*, Pin::MAX_OWNERS> Future::pinnedOwners() const noexcept {
	std::array<const RefCounted*, Pin::MAX_OWNERS> owners{};
	for (size_t i = 0; i < owners.size(); ++i)
		owners[i] = pinnedOwners_[i].load(std::memory_order_acquire);
	return owners;
}

std::chrono::steady_clock::time_point Future::getDueTime() const noexcept {
	return std::chrono::steady_clock::time_point(std::chrono::nanoseconds(dueNanos_.load(std::memory_order_acquire)));
}

void Future::run() {
	(void)runNowIfPending();
}

bool Future::runNowIfPending() {
	AION_YIELD_POINT("Future::run.claim");
	State expected = State::PENDING;
	if (!state_.compare_exchange_strong(expected, State::RUNNING, std::memory_order_acq_rel))
		return false;
	bool failed = invokeBody(sched_detail::maximumRuntimeFor(pool_));
	(void)finishRun(failed, false, {}, 0, std::chrono::milliseconds(0));
	if (isPeriodic()) {
		// the scheduled pool popped this task while this thread ran it and asked for a re-arm (runFromExecutor): hand it back with its due time
		// unchanged, as if run() had not raced with the pool. Dekker handshake with runFromExecutor: this thread stored PENDING (above) before
		// reading the request, the pool stored the request before re-reading the state, and exactly one side wins the exchange.
		AION_YIELD_POINT("Future::run.checkRearm");
		if (rearmRequested_.exchange(false, std::memory_order_acq_rel) && !isDone()) {
			try {
				if (ExecutorBackend* backend = sched_detail::installedBackend())
					backend->schedule(Ref<Future>(*this));
			} catch (...) {
				(void)cancel(false); // the task cannot be re-armed: make that visible (isDone) instead of silently never running again
				throw;
			}
		}
	}
	return true;
}

Future::RunOutcome Future::runFromExecutor(std::chrono::steady_clock::time_point now, int64_t maxRuntimeWithoutWarningMillis,
	int32_t coalesceAfterPeriods, std::chrono::milliseconds coalesceMinimumLag) noexcept {
	AION_YIELD_POINT("Future::runFromExecutor.claim");
	State expected = State::PENDING;
	if (!state_.compare_exchange_strong(expected, State::RUNNING, std::memory_order_acq_rel)) {
		if (expected == State::CANCELLED)
			return RunOutcome::FINISHED;
		if (isPeriodic() && expected == State::RUNNING) {
			// an explicit run() owns the task right now and the pool has just popped its only heap entry: request a re-arm, so the periodic task is
			// not silently dropped (review finding). See runNowIfPending for the handshake.
			rearmRequested_.store(true, std::memory_order_release);
			AION_YIELD_POINT("Future::runFromExecutor.recheck");
			if (state_.load(std::memory_order_acquire) == State::PENDING && rearmRequested_.exchange(false, std::memory_order_acq_rel))
				return RunOutcome::RESCHEDULE; // run() finished meanwhile: the backend re-pushes with the unchanged due time
		}
		return RunOutcome::NOT_RUN;
	}
	bool failed = invokeBody(maxRuntimeWithoutWarningMillis);
	return finishRun(failed, true, now, coalesceAfterPeriods, coalesceMinimumLag);
}

bool Future::invokeBody(int64_t maxRuntimeWithoutWarningMillis) noexcept {
	bool failed = false;
	TaskScope scope(info_);
	int64_t begin = commons::utils::nanoTime();
	try {
		body_(*this);
	} catch (...) {
		if (logExceptions_) {
			try {
				executeWrapperLog().errorCurrentException("Exception in a Runnable execution: " + sched_detail::describeTask(info_));
			} catch (...) {
				// logging failed (e.g. out of memory): nothing else to do, the pool thread must survive
			}
		} else {
			failure_ = std::current_exception();
			failed = true;
		}
	}
	int64_t durationNanos = commons::utils::nanoTime() - begin;
	try {
		if (commons::configs::CommonsConfig::RUNNABLESTATS_ENABLE.load(std::memory_order_relaxed))
			commons::utils::concurrent::RunnableStatsManager::handleStats(typeid(Future), sched_detail::describeTask(info_), durationNanos);
		int64_t durationMillis = durationNanos / 1'000'000;
		if (durationMillis > maxRuntimeWithoutWarningMillis)
			executeWrapperLog().warn("{} - execution time: {}ms", sched_detail::describeTask(info_), durationMillis);
	} catch (...) {
		// statistics and warnings are best effort
	}
	return failed;
}

Future::RunOutcome Future::finishRun(bool failed, bool rearm, std::chrono::steady_clock::time_point now, int32_t coalesceAfterPeriods,
	std::chrono::milliseconds coalesceMinimumLag) noexcept {
	if (isPeriodic() && !failed) {
		if (rearm) {
			// fixed rate: next = previous due + period; coalesced when far behind (design §1.4, deviation 6)
			int64_t period = period_.count();
			int64_t next = dueNanos_.load(std::memory_order_acquire) + period;
			int64_t nowNanos = toNanosSinceEpoch(now);
			int64_t behind = nowNanos - next;
			// "more than N periods behind" = behind > N * period, compared through quotient and remainder so large periods cannot overflow
			if (coalesceAfterPeriods > 0 && behind > 0 && (behind / period > coalesceAfterPeriods || (behind / period == coalesceAfterPeriods && behind % period != 0)) &&
				behind >= std::chrono::duration_cast<std::chrono::nanoseconds>(coalesceMinimumLag).count())
				next = nowNanos + period;
			dueNanos_.store(next, std::memory_order_release);
		}
		AION_YIELD_POINT("Future::finish.rearm");
		State expected = State::RUNNING;
		if (state_.compare_exchange_strong(expected, State::PENDING, std::memory_order_acq_rel))
			return rearm ? RunOutcome::RESCHEDULE : RunOutcome::FINISHED;
		// cancelled while running: this thread still owns the callable
		releaseCaptures();
		wakeWaiters();
		return RunOutcome::FINISHED;
	}
	releaseCaptures(); // before the final state: get() returning implies the captures are released
	AION_YIELD_POINT("Future::finish.cas");
	State expected = State::RUNNING;
	(void)state_.compare_exchange_strong(expected, failed ? State::FAILED : State::DONE, std::memory_order_acq_rel); // fails only if cancelled
	wakeWaiters();
	return RunOutcome::FINISHED;
}

void Future::releaseCaptures() noexcept {
	for (auto& pinned : pinnedOwners_)
		pinned.store(nullptr, std::memory_order_release);
	{
		Body released = std::move(body_);
		body_ = nullptr;
	}
	pin_.reset();
}

void Future::wakeWaiters() noexcept {
	AION_YIELD_POINT("Future::wakeWaiters");
	if (waiters_.load(std::memory_order_acquire) == 0)
		return;
	ParkingStripe& stripe = parkingFor(this);
	{
		// synchronizes with a waiter between its state check and its wait; a waiter holds the stripe mutex only for the check (no yield point
		// and no blocking hook inside), so this short OS-level wait is invisible to and harmless for the PCT scheduler
		std::lock_guard lock(stripe.mutex);
	}
	stripe.condition.notify_all();
}

// ------------------------------------------------------------------------------------------------------------------------------ get

void Future::get() {
	(void)awaitDone(-1);
	reportOutcome();
}

void Future::get(int64_t timeout, TimeUnit unit) {
	if (!awaitDone(std::max<int64_t>(0, toNanos(timeout, unit))))
		throw TimeoutException("Future.get timed out: " + sched_detail::describeTask(info_));
	reportOutcome();
}

bool Future::awaitDone(int64_t timeoutNanos) {
	if (isDone())
		return true;
	BlockingRegion region("Future.get");
	if (ExecutorBackend* backend = sched_detail::installedBackend()) {
		auto deadline = timeoutNanos < 0 ? std::chrono::steady_clock::time_point::max()
																		 : sched_detail::saturatingAdd(backend->clock().now(), timeoutNanos);
		ExecutorBackend::HelpResult help = backend->helpWhileWaiting(*this, deadline);
		if (help != ExecutorBackend::HelpResult::NOT_HELPING)
			return help == ExecutorBackend::HelpResult::DONE;
	}
	auto deadline = timeoutNanos < 0 ? std::chrono::steady_clock::time_point::max()
																	 : sched_detail::saturatingAdd(std::chrono::steady_clock::now(), timeoutNanos);
	ParkingStripe& stripe = parkingFor(this);
	waiters_.fetch_add(1, std::memory_order_acq_rel);
	bool done = false;
	for (;;) {
		AION_YIELD_POINT("Future::get.check"); // never inside the stripe mutex: PCT must not suspend a thread holding an OS mutex
		{
			std::unique_lock lock(stripe.mutex);
			if (isDone()) {
				done = true;
				break;
			}
			if (std::chrono::steady_clock::now() >= deadline)
				break;
			AION_PCT_BLOCKING_BEGIN("Future::get");
			if (deadline == std::chrono::steady_clock::time_point::max())
				stripe.condition.wait(lock);
			else
				stripe.condition.wait_until(lock, deadline);
		}
		AION_PCT_BLOCKING_END(); // outside the stripe mutex: the PCT scheduler may suspend this thread here
	}
	waiters_.fetch_sub(1, std::memory_order_acq_rel);
	return done;
}

void Future::reportOutcome() const {
	switch (state_.load(std::memory_order_acquire)) {
		case State::CANCELLED:
			throw CancellationException("Task was cancelled: " + sched_detail::describeTask(info_));
		case State::FAILED:
			throw ExecutionException("Task failed: " + sched_detail::describeTask(info_), failure_);
		default:
			return;
	}
}

} // namespace aion::gameserver::runtime
