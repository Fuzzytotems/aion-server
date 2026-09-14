#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"

#include <deque>
#include <format>
#include <thread>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/sched/detail/SchedRuntime.h"
#include "aion/gameserver/runtime/sched/detail/TimerHeap.h"
#include "aion/gameserver/runtime/sync/RankedMutex.h"

namespace aion::gameserver::runtime {

namespace {

using SteadyTime = std::chrono::steady_clock::time_point;
using SchedulerLock = std::unique_lock<RankedMutex<LockRank::SCHEDULER>>;

const commons::logging::Logger& log() {
	static const auto* instance =
		new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.runtime.DeterministicExecutor"));
	return *instance;
}

/** incremented by every DeterministicExecutor that seeds the calling thread's Rnd; a destructor restores only if no later executor seeded */
thread_local uint64_t rndSeedToken = 0;

} // namespace

struct DeterministicExecutor::State {
	explicit State(uint64_t seed) : savedGenerator(commons::utils::Rnd::generator()), creator(std::this_thread::get_id()), seedToken(++rndSeedToken) {
		commons::utils::Rnd::seedCurrentThreadForTests(seed);
	}

	mutable RankedMutex<LockRank::SCHEDULER> mutex;
	sched_detail::TimerHeap heap;
	std::deque<std::pair<PoolKind, FutureRef>> queue;
	bool shutdown = false;
	/** retire() ran (installBackend replaced the executor, or the destructor) */
	bool retired = false;
	std::atomic<bool> shutdownFlag{false};
	const commons::utils::Rnd::Xoshiro256PlusPlus savedGenerator;
	const std::thread::id creator;
	const uint64_t seedToken;
	uint64_t tasksRun = 0;
	uint64_t submitted = 0;
};

DeterministicExecutor::DeterministicExecutor(ManualClock& clock, uint64_t rndSeed)
	: clock_(clock), rndSeed_(rndSeed), state_(std::make_unique<State>(rndSeed)) {
}

DeterministicExecutor::~DeterministicExecutor() {
	retire();
}

void DeterministicExecutor::retire() noexcept {
	std::vector<FutureRef> pending;
	{
		SchedulerLock lock(state_->mutex);
		if (state_->retired)
			return;
		state_->retired = true;
		state_->shutdown = true;
		state_->shutdownFlag.store(true, std::memory_order_release);
		state_->heap.clear(pending);
		for (auto& [pool, task] : state_->queue)
			pending.push_back(std::move(task));
		state_->queue.clear();
	}
	for (FutureRef& task : pending)
		(void)task->cancel(false);
	pending.clear();
	if (std::this_thread::get_id() == state_->creator && rndSeedToken == state_->seedToken) // replaced executors do not undo a newer seed
		commons::utils::Rnd::generator() = state_->savedGenerator;
}

void DeterministicExecutor::execute(PoolKind pool, FutureRef task) {
	if (!task)
		throw NullPointerException("task");
	{
		SchedulerLock lock(state_->mutex);
		if (!state_->shutdown) {
			state_->queue.emplace_back(pool == PoolKind::LONG_RUNNING ? PoolKind::LONG_RUNNING : PoolKind::INSTANT, std::move(task));
			++state_->submitted;
			return;
		}
	}
	(void)task->cancel(false);
}

void DeterministicExecutor::schedule(FutureRef task) {
	if (!task)
		throw NullPointerException("task");
	std::vector<FutureRef> released;
	{
		SchedulerLock lock(state_->mutex);
		if (!state_->shutdown) {
			state_->heap.push(std::move(task));
			++state_->submitted;
			state_->heap.maybePurge(released);
		}
	}
	if (task)
		(void)task->cancel(false);
}

bool DeterministicExecutor::runOneTask() {
	FutureRef task;
	PoolKind pool = PoolKind::INSTANT;
	std::vector<FutureRef> released;
	{
		SchedulerLock lock(state_->mutex);
		state_->heap.dropCancelledTop(released);
		if (!state_->heap.empty() && state_->heap.topDue() <= clock_.now()) {
			task = state_->heap.pop();
			pool = PoolKind::SCHEDULED;
		} else if (!state_->queue.empty()) {
			pool = state_->queue.front().first;
			task = std::move(state_->queue.front().second);
			state_->queue.pop_front();
		}
	}
	released.clear();
	if (!task)
		return false;
	Future::RunOutcome outcome = task->runFromExecutor(clock_.now(), sched_detail::maximumRuntimeFor(pool), sched_detail::coalesceAfterPeriods(),
		sched_detail::coalesceMinimumLag());
	if (outcome == Future::RunOutcome::RESCHEDULE) {
		{
			SchedulerLock lock(state_->mutex);
			if (!state_->shutdown)
				state_->heap.push(std::move(task));
		}
		if (task)
			(void)task->cancel(false);
	}
	{
		SchedulerLock lock(state_->mutex);
		++state_->tasksRun;
	}
	task.reset();
	Reclaimer::getInstance().reclaimNow();
	return true;
}

size_t DeterministicExecutor::runReady() {
	size_t count = 0;
	while (runOneTask())
		++count;
	return count;
}

std::optional<SteadyTime> DeterministicExecutor::nextDueTime() const {
	std::vector<FutureRef> released;
	std::optional<SteadyTime> due;
	{
		SchedulerLock lock(state_->mutex);
		state_->heap.dropCancelledTop(released);
		if (!state_->heap.empty())
			due = state_->heap.topDue();
	}
	return due;
}

size_t DeterministicExecutor::advance(std::chrono::milliseconds dt) {
	SteadyTime target = sched_detail::saturatingAdd(clock_.now(), std::chrono::duration_cast<std::chrono::nanoseconds>(dt).count());
	size_t count = runReady();
	for (;;) {
		std::optional<SteadyTime> next = nextDueTime();
		if (!next || *next > target)
			break;
		if (*next > clock_.now())
			clock_.advance(*next - clock_.now());
		count += runReady();
	}
	if (clock_.now() < target)
		clock_.advance(target - clock_.now());
	count += runReady();
	return count;
}

ExecutorBackend::HelpResult DeterministicExecutor::helpWhileWaiting(Future& task, SteadyTime deadline) {
	if (!isExecutorThread())
		return HelpResult::NOT_HELPING;
	auto pollingSince = std::chrono::steady_clock::time_point::min();
	bool warned = false;
	for (;;) {
		if (task.isDone())
			return HelpResult::DONE;
		if (runOneTask())
			continue;
		SteadyTime now = clock_.now();
		if (now >= deadline)
			return HelpResult::TIMED_OUT;
		std::optional<SteadyTime> next = nextDueTime();
		if (next && *next <= deadline) {
			if (*next > now)
				clock_.advance(*next - now); // the blocked thread lets simulated time pass
			continue;
		}
		if (deadline != SteadyTime::max()) {
			clock_.advance(deadline - now);
			continue;
		}
		// untimed get() and nothing scheduled: only another thread can complete the task
		auto realNow = std::chrono::steady_clock::now();
		if (pollingSince == SteadyTime::min())
			pollingSince = realNow;
		if (!warned && realNow - pollingSince > std::chrono::seconds(5)) {
			warned = true;
			log().warn("Future.get() on the DeterministicExecutor thread is waiting for {} with no runnable or scheduled task",
				sched_detail::describeTask(task.getTaskInfo()));
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

bool DeterministicExecutor::shutdown(std::chrono::milliseconds) {
	std::vector<FutureRef> dropped;
	std::deque<std::pair<PoolKind, FutureRef>> queued;
	{
		SchedulerLock lock(state_->mutex);
		state_->shutdown = true;
		state_->shutdownFlag.store(true, std::memory_order_release);
		state_->heap.clear(dropped);
		queued.swap(state_->queue);
	}
	for (FutureRef& task : dropped)
		(void)task->cancel(false);
	dropped.clear();
	for (auto& [pool, task] : queued) {
		(void)task->runFromExecutor(clock_.now(), sched_detail::maximumRuntimeFor(pool), sched_detail::coalesceAfterPeriods(),
			sched_detail::coalesceMinimumLag());
		task.reset();
		Reclaimer::getInstance().reclaimNow();
	}
	return true;
}

bool DeterministicExecutor::isShutdown() const noexcept {
	return state_->shutdownFlag.load(std::memory_order_acquire);
}

bool DeterministicExecutor::isExecutorThread() const noexcept {
	return std::this_thread::get_id() == state_->creator;
}

size_t DeterministicExecutor::pendingTaskCount() const {
	size_t count = 0;
	SchedulerLock lock(state_->mutex);
	state_->heap.forEach([&](const FutureRef& task) {
		if (!task->isDone())
			++count;
	});
	for (const auto& [pool, task] : state_->queue)
		if (!task->isDone())
			++count;
	return count;
}

std::vector<std::string> DeterministicExecutor::getStats() const {
	SchedulerLock lock(state_->mutex);
	return {"", "Deterministic executor:", "=================================================",
		std::format("\tnow (ns): ............ {}", clock_.now().time_since_epoch().count()),
		std::format("\tgetCompletedTaskCount: {}", state_->tasksRun), std::format("\tgetQueuedTaskCount: .. {}", state_->heap.size() + state_->queue.size()),
		std::format("\tgetTaskCount: ........ {}", state_->submitted)};
}

std::vector<FutureRef> DeterministicExecutor::pendingTasks() const {
	std::vector<FutureRef> tasks;
	SchedulerLock lock(state_->mutex);
	state_->heap.forEach([&](const FutureRef& task) {
		if (!task->isDone())
			tasks.push_back(task);
	});
	for (const auto& [pool, task] : state_->queue)
		if (!task->isDone())
			tasks.push_back(task);
	return tasks;
}

} // namespace aion::gameserver::runtime
