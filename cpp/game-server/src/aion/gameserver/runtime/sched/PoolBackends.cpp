#include "aion/gameserver/runtime/sched/PoolBackends.h"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <format>
#include <list>
#include <mutex>
#include <thread>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/concurrent/PriorityThreadFactory.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/ThreadContext.h"
#include "aion/gameserver/runtime/base/YieldPoint.h"
#include "aion/gameserver/runtime/sched/detail/SchedRuntime.h"
#include "aion/gameserver/runtime/sched/detail/TimerHeap.h"
#include "aion/gameserver/runtime/sync/RankedMutex.h"

namespace aion::gameserver::runtime {

namespace {

using commons::utils::concurrent::NORM_PRIORITY;
using commons::utils::concurrent::PriorityThreadFactory;
using SteadyTime = std::chrono::steady_clock::time_point;
using SchedulerMutex = RankedMutex<LockRank::SCHEDULER>;
using SchedulerLock = std::unique_lock<SchedulerMutex>;

/** the backend whose worker thread is the calling thread (isExecutorThread) */
thread_local const ExecutorBackend* currentExecutor = nullptr;

const commons::logging::Logger& rejectionLog() {
	static const auto* instance =
		new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.commons.utils.concurrent.AionRejectedExecutionHandler"));
	return *instance;
}

void cancelAll(std::vector<FutureRef>& tasks) noexcept {
	for (FutureRef& task : tasks)
		(void)task->cancel(false);
	tasks.clear();
}

/** condition wait bracketed for the PCT scheduler (the leaf mutex is re-acquired before AION_PCT_BLOCKING_END) */
void waitOn(std::condition_variable_any& condition, SchedulerLock& lock) {
	AION_PCT_BLOCKING_BEGIN("sched.wait");
	condition.wait(lock);
	AION_PCT_BLOCKING_END();
}

void waitOnUntil(std::condition_variable_any& condition, SchedulerLock& lock, SteadyTime deadline) {
	AION_PCT_BLOCKING_BEGIN("sched.waitUntil");
	condition.wait_until(lock, deadline);
	AION_PCT_BLOCKING_END();
}

Future::RunOutcome runOnThisThread(const FutureRef& task, PoolKind pool) noexcept {
	return task->runFromExecutor(std::chrono::steady_clock::now(), sched_detail::maximumRuntimeFor(pool), sched_detail::coalesceAfterPeriods(),
		sched_detail::coalesceMinimumLag());
}

/** Java ThreadPoolExecutor statistics of one pool */
struct PoolCounters {
	std::atomic<int32_t> active{0};
	std::atomic<int32_t> poolSize{0};
	std::atomic<int32_t> largestPoolSize{0};
	std::atomic<uint64_t> completed{0};
	std::atomic<uint64_t> submitted{0};

	void threadStarted() noexcept {
		int32_t size = poolSize.fetch_add(1, std::memory_order_acq_rel) + 1;
		int32_t largest = largestPoolSize.load(std::memory_order_acquire);
		while (size > largest && !largestPoolSize.compare_exchange_weak(largest, size, std::memory_order_acq_rel)) {
		}
	}
};

void appendPoolStats(std::vector<std::string>& lines, const char* title, const PoolCounters& counters, int32_t corePoolSize, int32_t maximumPoolSize,
	size_t queued) {
	lines.emplace_back("");
	lines.emplace_back(title);
	lines.emplace_back("=================================================");
	lines.push_back(std::format("\tgetActiveCount: ...... {}", counters.active.load()));
	lines.push_back(std::format("\tgetCorePoolSize: ..... {}", corePoolSize));
	lines.push_back(std::format("\tgetPoolSize: ......... {}", counters.poolSize.load()));
	lines.push_back(std::format("\tgetLargestPoolSize: .. {}", counters.largestPoolSize.load()));
	lines.push_back(std::format("\tgetMaximumPoolSize: .. {}", maximumPoolSize));
	lines.push_back(std::format("\tgetCompletedTaskCount: {}", counters.completed.load()));
	lines.push_back(std::format("\tgetQueuedTaskCount: .. {}", queued));
	lines.push_back(std::format("\tgetTaskCount: ........ {}", counters.submitted.load()));
}

} // namespace

// ============================================================================================================================== ThreadPoolBackend

struct ThreadPoolBackend::Impl {
	explicit Impl(ThreadPoolBackend& owner, const Options& options) : owner(owner), options(options) {}

	ThreadPoolBackend& owner;
	const Options options;
	std::atomic<bool> shutdown{false};

	// ------------------------------------------------------------------ scheduled pool
	mutable SchedulerMutex scheduledMutex;
	std::condition_variable_any scheduledCondition;
	sched_detail::TimerHeap heap;
	/** token of the worker sleeping until the top's due time, 0 if none (leader/follower) */
	uint64_t leaderToken = 0;
	uint64_t nextLeaderToken = 1;
	bool scheduledStopping = false;
	std::vector<std::jthread> scheduledThreads;
	PoolCounters scheduledCounters;

	// ------------------------------------------------------------------ instant pool
	mutable SchedulerMutex instantMutex;
	std::condition_variable_any instantCondition;
	std::deque<FutureRef> instantQueue;
	bool instantStopping = false;
	std::vector<std::jthread> instantThreads;
	PoolCounters instantCounters;

	// ------------------------------------------------------------------ long running pool and rejection threads
	mutable SchedulerMutex longMutex;
	std::condition_variable_any longCondition;
	std::deque<FutureRef> longQueue;
	int32_t longIdle = 0;
	bool longStopping = false;
	/** long running workers and rejection threads; finished ones are joined (reaped) by later spawns and by the destructor */
	std::list<std::jthread> dynamicThreads;
	std::vector<std::thread::id> exitedThreads;
	PoolCounters longCounters;
	PriorityThreadFactory longRunningFactory{"LongRunning", NORM_PRIORITY};
	PriorityThreadFactory rejectionFactory{"Thread", NORM_PRIORITY};

	// ------------------------------------------------------------------ termination
	std::mutex terminationMutex;
	std::condition_variable terminationCondition;
	int32_t liveThreads = 0;

	void threadCreated() {
		std::lock_guard lock(terminationMutex);
		++liveThreads;
	}

	void threadExited() noexcept {
		{
			std::lock_guard lock(terminationMutex);
			--liveThreads;
		}
		terminationCondition.notify_all();
	}

	/** starts a thread counted in liveThreads (the count is undone if the thread cannot be started) */
	template <class Container, class F>
	void spawn(Container& threads, PriorityThreadFactory& factory, F&& body) {
		threadCreated();
		try {
			threads.push_back(factory.newThread(std::forward<F>(body)));
		} catch (...) {
			threadExited();
			throw;
		}
	}

	void start() {
		PriorityThreadFactory scheduledFactory("ScheduledPool", NORM_PRIORITY);
		for (int32_t i = 0; i < options.scheduledThreads; ++i)
			spawn(scheduledThreads, scheduledFactory, [this] { scheduledWorker(); });
		PriorityThreadFactory instantFactory("InstantPool", options.instantThreadPriority);
		for (int32_t i = 0; i < options.instantThreads; ++i)
			spawn(instantThreads, instantFactory, [this] { instantWorker(); });
	}

	/** worker thread start: the factory already named the thread ("ScheduledPool-1"), the ThreadContext copies the name */
	void enterWorker(PoolCounters& counters, int32_t priority) {
		currentExecutor = &owner;
		sched_detail::setCurrentThreadJavaPriority(priority);
		ThreadContext::current().refreshName();
		counters.threadStarted();
	}

	// ------------------------------------------------------------------ scheduled pool

	void schedule(FutureRef task) {
		std::vector<FutureRef> released;
		{
			SchedulerLock lock(scheduledMutex);
			if (!scheduledStopping) {
				bool newTop = heap.empty() || task->getDueTime() < heap.topDue();
				heap.push(std::move(task));
				scheduledCounters.submitted.fetch_add(1, std::memory_order_relaxed);
				heap.maybePurge(released);
				if (newTop) {
					leaderToken = 0; // the current leader sleeps too long: let a follower take the new top
					scheduledCondition.notify_one();
				}
			}
		}
		if (task) // not queued: shut down
			(void)task->cancel(false);
		released.clear();
	}

	void scheduledWorker() {
		enterWorker(scheduledCounters, NORM_PRIORITY);
		std::vector<FutureRef> released;
		SchedulerLock lock(scheduledMutex);
		for (;;) {
			if (scheduledStopping)
				break;
			heap.dropCancelledTop(released);
			if (!released.empty()) {
				lock.unlock();
				released.clear();
				lock.lock();
				continue;
			}
			if (heap.empty()) {
				waitOn(scheduledCondition, lock);
				continue;
			}
			SteadyTime now = std::chrono::steady_clock::now();
			if (heap.topDue() <= now) {
				FutureRef task = heap.pop();
				if (!heap.empty())
					scheduledCondition.notify_one(); // a follower becomes the next leader
				lock.unlock();
				scheduledCounters.active.fetch_add(1, std::memory_order_relaxed);
				Future::RunOutcome outcome = task->runFromExecutor(now, sched_detail::maximumRuntimeFor(PoolKind::SCHEDULED),
					sched_detail::coalesceAfterPeriods(), sched_detail::coalesceMinimumLag());
				scheduledCounters.active.fetch_sub(1, std::memory_order_relaxed);
				if (outcome != Future::RunOutcome::NOT_RUN)
					scheduledCounters.completed.fetch_add(1, std::memory_order_relaxed);
				if (outcome == Future::RunOutcome::RESCHEDULE) {
					lock.lock();
					if (!scheduledStopping) {
						if (heap.empty() || task->getDueTime() < heap.topDue())
							leaderToken = 0;
						heap.push(std::move(task));
					}
					lock.unlock();
					if (task)
						(void)task->cancel(false); // shut down while running: periodic task dropped
				}
				task.reset();
				lock.lock();
				continue;
			}
			if (leaderToken != 0) {
				waitOn(scheduledCondition, lock);
				continue;
			}
			uint64_t token = nextLeaderToken++;
			leaderToken = token;
			waitOnUntil(scheduledCondition, lock, heap.topDue());
			if (leaderToken == token)
				leaderToken = 0;
		}
		lock.unlock();
		scheduledCounters.poolSize.fetch_sub(1, std::memory_order_relaxed);
		threadExited();
	}

	// ------------------------------------------------------------------ instant pool

	void executeInstant(FutureRef task) {
		{
			SchedulerLock lock(instantMutex);
			if (instantStopping) {
				lock.unlock();
				(void)task->cancel(false);
				return;
			}
			if (instantQueue.size() < options.instantQueueCapacity) {
				instantQueue.push_back(std::move(task));
				instantCounters.submitted.fetch_add(1, std::memory_order_relaxed);
				instantCondition.notify_one();
				return;
			}
		}
		reject(std::move(task));
	}

	/** Java AionRejectedExecutionHandler.rejectedExecution */
	void reject(FutureRef task) {
		rejectionLog().warn("{} from InstantPool", sched_detail::describeTask(task->getTaskInfo()),
			RejectedExecutionException("Instant pool queue is full"));
		if (sched_detail::currentThreadJavaPriority() > NORM_PRIORITY) {
			int32_t priority = sched_detail::currentThreadJavaPriority();
			SchedulerLock lock(longMutex);
			spawn(dynamicThreads, rejectionFactory, [this, task = std::move(task), priority]() mutable {
				currentExecutor = &owner;
				sched_detail::setCurrentThreadJavaPriority(priority);
				ThreadContext::current().refreshName();
				(void)runOnThisThread(task, PoolKind::INSTANT);
				task.reset();
				dynamicThreadExited();
			});
		} else {
			(void)runOnThisThread(task, PoolKind::INSTANT);
		}
	}

	void instantWorker() {
		enterWorker(instantCounters, options.instantThreadPriority);
		SchedulerLock lock(instantMutex);
		for (;;) {
			while (instantQueue.empty() && !instantStopping)
				waitOn(instantCondition, lock);
			if (instantQueue.empty())
				break; // stopping and drained
			FutureRef task = std::move(instantQueue.front());
			instantQueue.pop_front();
			lock.unlock();
			instantCounters.active.fetch_add(1, std::memory_order_relaxed);
			if (runOnThisThread(task, PoolKind::INSTANT) != Future::RunOutcome::NOT_RUN)
				instantCounters.completed.fetch_add(1, std::memory_order_relaxed);
			instantCounters.active.fetch_sub(1, std::memory_order_relaxed);
			task.reset();
			lock.lock();
		}
		lock.unlock();
		instantCounters.poolSize.fetch_sub(1, std::memory_order_relaxed);
		threadExited();
	}

	// ------------------------------------------------------------------ long running pool

	void executeLongRunning(FutureRef task) {
		std::list<std::jthread> reaped;
		{
			SchedulerLock lock(longMutex);
			if (!longStopping) {
				longQueue.push_back(std::move(task));
				longCounters.submitted.fetch_add(1, std::memory_order_relaxed);
				if (static_cast<size_t>(longIdle) >= longQueue.size()) {
					longCondition.notify_one();
				} else {
					spawn(dynamicThreads, longRunningFactory, [this] { longRunningWorker(); });
				}
				reapExited(reaped);
			}
		}
		if (task)
			(void)task->cancel(false);
		reaped.clear(); // joins threads that already returned from their body
	}

	/** under longMutex: moves finished threads into `reaped` (joined by the caller after unlocking) */
	void reapExited(std::list<std::jthread>& reaped) {
		if (exitedThreads.empty())
			return;
		for (auto it = dynamicThreads.begin(); it != dynamicThreads.end();) {
			if (std::find(exitedThreads.begin(), exitedThreads.end(), it->get_id()) != exitedThreads.end()) {
				auto next = std::next(it);
				reaped.splice(reaped.end(), dynamicThreads, it);
				it = next;
			} else {
				++it;
			}
		}
		exitedThreads.clear();
	}

	void dynamicThreadExited() noexcept {
		{
			SchedulerLock lock(longMutex);
			exitedThreads.push_back(std::this_thread::get_id());
		}
		threadExited();
	}

	void longRunningWorker() {
		enterWorker(longCounters, NORM_PRIORITY);
		SchedulerLock lock(longMutex);
		for (;;) {
			if (!longQueue.empty()) {
				FutureRef task = std::move(longQueue.front());
				longQueue.pop_front();
				lock.unlock();
				longCounters.active.fetch_add(1, std::memory_order_relaxed);
				if (runOnThisThread(task, PoolKind::LONG_RUNNING) != Future::RunOutcome::NOT_RUN)
					longCounters.completed.fetch_add(1, std::memory_order_relaxed);
				longCounters.active.fetch_sub(1, std::memory_order_relaxed);
				task.reset();
				lock.lock();
				continue;
			}
			if (longStopping)
				break;
			++longIdle;
			AION_PCT_BLOCKING_BEGIN("LongRunning.idle");
			bool woken = longCondition.wait_for(lock, options.longRunningKeepAlive, [this] { return !longQueue.empty() || longStopping; });
			AION_PCT_BLOCKING_END();
			--longIdle;
			if (!woken)
				break; // idle for keepAlive
		}
		exitedThreads.push_back(std::this_thread::get_id());
		lock.unlock();
		longCounters.poolSize.fetch_sub(1, std::memory_order_relaxed);
		threadExited();
	}

	// ------------------------------------------------------------------ shutdown

	bool shutdownPools(std::chrono::milliseconds awaitTermination) {
		shutdown.store(true, std::memory_order_release);
		std::vector<FutureRef> dropped;
		{
			SchedulerLock lock(scheduledMutex);
			scheduledStopping = true;
			heap.clear(dropped);
			scheduledCondition.notify_all();
		}
		cancelAll(dropped);
		{
			SchedulerLock lock(instantMutex);
			instantStopping = true;
			instantCondition.notify_all();
		}
		{
			SchedulerLock lock(longMutex);
			longStopping = true;
			longCondition.notify_all();
		}
		if (currentExecutor == &owner)
			return false; // a pool thread cannot wait for its own pool
		std::unique_lock lock(terminationMutex);
		return terminationCondition.wait_for(lock, awaitTermination, [this] { return liveThreads == 0; });
	}

	/** joins every thread (idempotent: joined threads are skipped) */
	void joinAll() {
		for (std::jthread& thread : scheduledThreads)
			if (thread.joinable())
				thread.join();
		for (std::jthread& thread : instantThreads)
			if (thread.joinable())
				thread.join();
		std::unique_lock lock(terminationMutex);
		terminationCondition.wait(lock, [this] { return liveThreads == 0; });
		lock.unlock();
		std::list<std::jthread> threads;
		{
			SchedulerLock longLock(longMutex);
			threads.swap(dynamicThreads);
			exitedThreads.clear();
		}
		threads.clear();
	}
};

ThreadPoolBackend::ThreadPoolBackend(const Options& options) : impl_(std::make_unique<Impl>(*this, options)) {
	if (options.instantThreads < 1 || options.scheduledThreads < 1 || options.instantQueueCapacity < 1)
		throw IllegalArgumentException("ThreadPoolBackend needs at least one instant and one scheduled thread and a non-empty instant queue");
	try {
		impl_->start();
	} catch (...) {
		(void)impl_->shutdownPools(std::chrono::milliseconds(0));
		impl_->joinAll();
		throw;
	}
}

ThreadPoolBackend::~ThreadPoolBackend() {
	retire();
}

void ThreadPoolBackend::retire() noexcept {
	(void)impl_->shutdownPools(std::chrono::milliseconds(0));
	impl_->joinAll(); // running and queued instant/long-running tasks finish; later submissions are cancelled and dropped
}

const Clock& ThreadPoolBackend::clock() const noexcept {
	return SystemClock::getInstance();
}

void ThreadPoolBackend::execute(PoolKind pool, FutureRef task) {
	if (!task)
		throw NullPointerException("task");
	if (pool == PoolKind::LONG_RUNNING)
		impl_->executeLongRunning(std::move(task));
	else
		impl_->executeInstant(std::move(task));
}

void ThreadPoolBackend::schedule(FutureRef task) {
	if (!task)
		throw NullPointerException("task");
	impl_->schedule(std::move(task));
}

bool ThreadPoolBackend::shutdown(std::chrono::milliseconds awaitTermination) {
	return impl_->shutdownPools(awaitTermination);
}

bool ThreadPoolBackend::isShutdown() const noexcept {
	return impl_->shutdown.load(std::memory_order_acquire);
}

bool ThreadPoolBackend::isExecutorThread() const noexcept {
	return currentExecutor == this;
}

bool ThreadPoolBackend::runOneTask() {
	return false;
}

std::vector<std::string> ThreadPoolBackend::getStats() const {
	size_t scheduledQueued = 0;
	size_t instantQueued = 0;
	size_t longQueued = 0;
	{
		SchedulerLock lock(impl_->scheduledMutex);
		scheduledQueued = impl_->heap.size();
	}
	{
		SchedulerLock lock(impl_->instantMutex);
		instantQueued = impl_->instantQueue.size();
	}
	{
		SchedulerLock lock(impl_->longMutex);
		longQueued = impl_->longQueue.size();
	}
	std::vector<std::string> lines;
	appendPoolStats(lines, "Scheduled pool:", impl_->scheduledCounters, impl_->options.scheduledThreads, INT32_MAX, scheduledQueued);
	appendPoolStats(lines, "Instant pool:", impl_->instantCounters, impl_->options.instantThreads, impl_->options.instantThreads, instantQueued);
	appendPoolStats(lines, "Long running pool:", impl_->longCounters, 0, INT32_MAX, longQueued);
	return lines;
}

std::vector<FutureRef> ThreadPoolBackend::pendingTasks() const {
	std::vector<FutureRef> tasks;
	{
		SchedulerLock lock(impl_->scheduledMutex);
		impl_->heap.forEach([&](const FutureRef& task) {
			if (!task->isDone())
				tasks.push_back(task);
		});
	}
	{
		SchedulerLock lock(impl_->instantMutex);
		for (const FutureRef& task : impl_->instantQueue)
			if (!task->isDone())
				tasks.push_back(task);
	}
	{
		SchedulerLock lock(impl_->longMutex);
		for (const FutureRef& task : impl_->longQueue)
			if (!task->isDone())
				tasks.push_back(task);
	}
	return tasks;
}

int32_t ThreadPoolBackend::getInstantPoolSize() const noexcept {
	return impl_->options.instantThreads;
}

int32_t ThreadPoolBackend::getScheduledPoolSize() const noexcept {
	return impl_->options.scheduledThreads;
}

int32_t ThreadPoolBackend::getLongRunningPoolSize() const noexcept {
	return impl_->longCounters.poolSize.load(std::memory_order_acquire);
}

// ========================================================================================================================= SingleExecutorBackend

struct SingleExecutorBackend::Impl {
	explicit Impl(SingleExecutorBackend& owner, size_t capacity) : owner(owner), capacity(capacity) {}

	SingleExecutorBackend& owner;
	const size_t capacity;
	std::atomic<bool> shutdown{false};
	mutable SchedulerMutex mutex;
	std::condition_variable_any condition;
	sched_detail::TimerHeap heap;
	std::deque<std::pair<PoolKind, FutureRef>> queue;
	bool stopping = false;
	bool exited = false;
	std::jthread thread;
	PoolCounters counters;

	/** under the lock: takes the next runnable task (due timer first, then the queue) */
	bool takeNext(SchedulerLock& lock, FutureRef& task, PoolKind& pool, std::vector<FutureRef>& released) {
		(void)lock;
		heap.dropCancelledTop(released);
		if (!heap.empty() && heap.topDue() <= std::chrono::steady_clock::now()) {
			task = heap.pop();
			pool = PoolKind::SCHEDULED;
			return true;
		}
		if (!queue.empty()) {
			pool = queue.front().first;
			task = std::move(queue.front().second);
			queue.pop_front();
			return true;
		}
		return false;
	}

	/** runs a taken task on the calling thread (lock not held) */
	void run(FutureRef task, PoolKind pool) {
		counters.active.fetch_add(1, std::memory_order_relaxed);
		Future::RunOutcome outcome = runOnThisThread(task, pool);
		counters.active.fetch_sub(1, std::memory_order_relaxed);
		if (outcome != Future::RunOutcome::NOT_RUN)
			counters.completed.fetch_add(1, std::memory_order_relaxed);
		if (outcome == Future::RunOutcome::RESCHEDULE) {
			{
				SchedulerLock lock(mutex);
				if (!stopping) {
					heap.push(std::move(task));
					condition.notify_all();
				}
			}
			if (task)
				(void)task->cancel(false); // shut down while running: periodic task dropped
		}
	}

	/** runs the next due timer or queued task on the calling thread; @return false if nothing was runnable */
	bool runOne() {
		FutureRef task;
		PoolKind pool = PoolKind::INSTANT;
		std::vector<FutureRef> released; // cancelled shells popped from the top, released after unlocking
		bool taken = false;
		{
			SchedulerLock lock(mutex);
			taken = takeNext(lock, task, pool, released);
		}
		released.clear();
		if (!taken)
			return false;
		run(std::move(task), pool);
		return true;
	}

	void worker() {
		currentExecutor = &owner;
		sched_detail::nameCurrentThread("SingleExecutor");
		counters.threadStarted();
		for (;;) {
			if (runOne())
				continue;
			SchedulerLock lock(mutex);
			if (stopping && queue.empty())
				break;
			if (!queue.empty() || (!heap.empty() && heap.topDue() <= std::chrono::steady_clock::now()))
				continue;
			if (heap.empty())
				waitOn(condition, lock);
			else
				waitOnUntil(condition, lock, heap.topDue());
		}
		SchedulerLock lock(mutex);
		exited = true;
		condition.notify_all();
	}
};

SingleExecutorBackend::SingleExecutorBackend(size_t instantQueueCapacity) : impl_(std::make_unique<Impl>(*this, instantQueueCapacity)) {
	if (instantQueueCapacity < 1)
		throw IllegalArgumentException("instantQueueCapacity must be positive");
	impl_->thread = std::jthread([this] { impl_->worker(); });
}

SingleExecutorBackend::~SingleExecutorBackend() {
	retire();
}

void SingleExecutorBackend::retire() noexcept {
	(void)shutdown(std::chrono::milliseconds(0));
	if (impl_->thread.joinable())
		impl_->thread.join();
}

const Clock& SingleExecutorBackend::clock() const noexcept {
	return SystemClock::getInstance();
}

void SingleExecutorBackend::execute(PoolKind pool, FutureRef task) {
	if (!task)
		throw NullPointerException("task");
	{
		SchedulerLock lock(impl_->mutex);
		if (impl_->stopping) {
			lock.unlock();
			(void)task->cancel(false);
			return;
		}
		if (pool == PoolKind::LONG_RUNNING || impl_->queue.size() < impl_->capacity) {
			impl_->queue.emplace_back(pool == PoolKind::LONG_RUNNING ? PoolKind::LONG_RUNNING : PoolKind::INSTANT, std::move(task));
			impl_->counters.submitted.fetch_add(1, std::memory_order_relaxed);
			impl_->condition.notify_all();
			return;
		}
	}
	rejectionLog().warn("{} from SingleExecutor", sched_detail::describeTask(task->getTaskInfo()), RejectedExecutionException("Instant queue is full"));
	(void)runOnThisThread(task, PoolKind::INSTANT);
}

void SingleExecutorBackend::schedule(FutureRef task) {
	if (!task)
		throw NullPointerException("task");
	std::vector<FutureRef> released;
	{
		SchedulerLock lock(impl_->mutex);
		if (!impl_->stopping) {
			impl_->heap.push(std::move(task));
			impl_->counters.submitted.fetch_add(1, std::memory_order_relaxed);
			impl_->heap.maybePurge(released);
			impl_->condition.notify_all();
		}
	}
	if (task)
		(void)task->cancel(false);
}

bool SingleExecutorBackend::shutdown(std::chrono::milliseconds awaitTermination) {
	impl_->shutdown.store(true, std::memory_order_release);
	std::vector<FutureRef> dropped;
	{
		SchedulerLock lock(impl_->mutex);
		impl_->stopping = true;
		impl_->heap.clear(dropped);
		impl_->condition.notify_all();
	}
	cancelAll(dropped);
	if (isExecutorThread())
		return false;
	SchedulerLock lock(impl_->mutex);
	return impl_->condition.wait_for(lock, awaitTermination, [this] { return impl_->exited; });
}

bool SingleExecutorBackend::isShutdown() const noexcept {
	return impl_->shutdown.load(std::memory_order_acquire);
}

bool SingleExecutorBackend::isExecutorThread() const noexcept {
	return currentExecutor == this;
}

bool SingleExecutorBackend::runOneTask() {
	return impl_->runOne();
}

ExecutorBackend::HelpResult SingleExecutorBackend::helpWhileWaiting(Future& task, SteadyTime deadline) {
	if (!isExecutorThread())
		return HelpResult::NOT_HELPING;
	for (;;) {
		if (task.isDone())
			return HelpResult::DONE;
		if (impl_->runOne())
			continue;
		SteadyTime now = std::chrono::steady_clock::now();
		if (now >= deadline)
			return task.isDone() ? HelpResult::DONE : HelpResult::TIMED_OUT;
		SchedulerLock lock(impl_->mutex);
		SteadyTime wakeAt = std::min(deadline, now + std::chrono::milliseconds(10));
		if (!impl_->heap.empty())
			wakeAt = std::min(wakeAt, impl_->heap.topDue());
		if (impl_->queue.empty())
			waitOnUntil(impl_->condition, lock, wakeAt);
	}
}

std::vector<std::string> SingleExecutorBackend::getStats() const {
	size_t queued = 0;
	{
		SchedulerLock lock(impl_->mutex);
		queued = impl_->heap.size() + impl_->queue.size();
	}
	std::vector<std::string> lines;
	appendPoolStats(lines, "Single executor (gameserver.debug.single_executor):", impl_->counters, 1, 1, queued);
	return lines;
}

std::vector<FutureRef> SingleExecutorBackend::pendingTasks() const {
	std::vector<FutureRef> tasks;
	SchedulerLock lock(impl_->mutex);
	impl_->heap.forEach([&](const FutureRef& task) {
		if (!task->isDone())
			tasks.push_back(task);
	});
	for (const auto& [pool, task] : impl_->queue)
		if (!task->isDone())
			tasks.push_back(task);
	return tasks;
}

} // namespace aion::gameserver::runtime
