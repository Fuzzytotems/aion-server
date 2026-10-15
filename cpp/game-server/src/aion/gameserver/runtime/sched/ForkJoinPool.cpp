#include "aion/gameserver/runtime/sched/ForkJoinPool.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <exception>
#include <list>
#include <mutex>
#include <optional>
#include <string_view>
#include <thread>
#include <vector>

#include "aion/commons/utils/concurrent/PriorityThreadFactory.h"
#include "aion/gameserver/runtime/base/ThreadContext.h"
#include "aion/gameserver/runtime/base/YieldPoint.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sync/BlockingRegion.h"
#include "aion/gameserver/runtime/sync/RankedMutex.h"

namespace aion::gameserver::runtime {

namespace {

using SchedulerLock = std::unique_lock<RankedMutex<LockRank::SCHEDULER>>;

/**
 * One parallelForEach call. Lives on the caller's stack; helpers only touch it while registered as participants, and the caller returns only
 * when every index is claimed and no participant is left, so the job outlives every access.
 */
struct Job {
	size_t count = 0;
	void (*body)(void* context, size_t index) = nullptr;
	void* context = nullptr;
	bool perElement = false;
	TaskInfo info{};
	uint64_t callerScopeId = 0;
	/** next unclaimed index (may run past count by the number of participants) */
	std::atomic<size_t> next{0};
	/** helpers currently working on the job (under the pool mutex) */
	uint32_t participants = 0;
	std::atomic<bool> failed{false};
	/** written once by the first failing element, read by the caller after all participants left */
	std::exception_ptr failure;

	bool allClaimed() const noexcept { return next.load(std::memory_order_acquire) >= count; }

	void runIndex(size_t index) noexcept {
		try {
			body(context, index);
		} catch (...) {
			if (!failed.exchange(true, std::memory_order_acq_rel))
				failure = std::current_exception();
		}
	}

	/** claims and runs indices until none is left; each PER_ELEMENT element in its own outermost scope */
	void work() noexcept {
		for (;;) {
			AION_YIELD_POINT("ForkJoinPool::claim");
			size_t index = next.fetch_add(1, std::memory_order_acq_rel);
			if (index >= count)
				return;
			if (perElement) {
				TaskScope scope(info);
				runIndex(index);
			} else {
				runIndex(index);
			}
		}
	}
};

/** thread-local marker of the pool's helper threads (nested parallelForEach calls run serially) */
thread_local bool isHelperThread = false;

struct PoolState {
	RankedMutex<LockRank::SCHEDULER> mutex;
	std::condition_variable_any workAvailable;
	std::condition_variable_any jobProgress;
	std::list<Job*> jobs;
	std::vector<std::jthread> helpers;
	bool started = false;
	bool stopping = false;
	std::atomic<bool> serial{false};
	std::atomic<bool> shutdown{false};

	static int32_t parallelism() noexcept {
		unsigned cores = std::thread::hardware_concurrency();
		return cores > 1 ? static_cast<int32_t>(cores - 1) : 1;
	}

	/** under the mutex: starts the helpers on first use */
	void ensureStarted(SchedulerLock& lock) {
		(void)lock;
		if (started)
			return;
		started = true;
		commons::utils::concurrent::PriorityThreadFactory factory("ForkJoin", commons::utils::concurrent::NORM_PRIORITY);
		for (int32_t i = 0; i < parallelism(); ++i)
			helpers.push_back(factory.newThread([this] { helperLoop(); }));
	}

	void helperLoop() {
		isHelperThread = true;
		ThreadContext::current().refreshName();
		SchedulerLock lock(mutex);
		for (;;) {
			auto available = std::find_if(jobs.begin(), jobs.end(), [](Job* job) { return !job->allClaimed(); });
			if (available == jobs.end()) {
				if (stopping)
					return;
				AION_PCT_BLOCKING_BEGIN("ForkJoinPool::idle");
				workAvailable.wait(lock);
				AION_PCT_BLOCKING_END();
				continue;
			}
			Job* job = *available;
			++job->participants;
			lock.unlock();
			if (job->perElement) {
				job->work();
			} else {
				TaskScope scope(job->info, job->callerScopeId); // JOIN: the helper adopts the submitter's scope id
				job->work();
			}
			lock.lock();
			--job->participants;
			if (job->allClaimed())
				jobs.remove(job);
			jobProgress.notify_all();
		}
	}
};

PoolState& poolState() {
	static auto* state = new PoolState();
	return *state;
}

} // namespace

ForkJoinPool& ForkJoinPool::commonPool() {
	static auto* pool = new ForkJoinPool();
	return *pool;
}

int32_t ForkJoinPool::getParallelism() const noexcept {
	return PoolState::parallelism();
}

void ForkJoinPool::setSerial(bool serial) noexcept {
	poolState().serial.store(serial, std::memory_order_release);
}

bool ForkJoinPool::isParallelFromCurrentThread() const noexcept {
	return !isSerial() && !isHelperThread && !poolState().shutdown.load(std::memory_order_acquire);
}

bool ForkJoinPool::isSerial() const noexcept {
	return poolState().serial.load(std::memory_order_acquire);
}

void ForkJoinPool::shutdown() {
	PoolState& state = poolState();
	std::vector<std::jthread> helpers;
	{
		SchedulerLock lock(state.mutex);
		state.shutdown.store(true, std::memory_order_release);
		state.stopping = true;
		helpers.swap(state.helpers);
		state.workAvailable.notify_all();
	}
	helpers.clear(); // joins; helpers finish the jobs they joined, callers finish the rest themselves
}

void ForkJoinPool::runIndexed(size_t count, void (*body)(void* context, size_t index), void* context, bool perElement, const TaskInfo& requested) {
	if (count == 0)
		return;
	// The elements of a startup or shutdown phase keep the phase's task kind (M4 gate): the watchdog exempts those phases from STALL dumps
	// (Watchdog::Config::stallExemptKinds, "DataManager/Geo startup phases"), and one element of a startup load can run longer than the 60 s
	// stall limit (a terrain PNG decoded by GeoWorldLoader in a Debug build). Other callers' elements run as TaskKind::FORK_JOIN.
	TaskInfo info = requested;
	if (TaskScope::active()) {
		std::string_view callerKind = TaskScope::currentTaskInfo().kind;
		if (callerKind == TaskKind::STARTUP || callerKind == TaskKind::SHUTDOWN)
			info.kind = callerKind == TaskKind::STARTUP ? TaskKind::STARTUP : TaskKind::SHUTDOWN;
	}
	PoolState& state = poolState();
	auto runSerially = [&](Job& job) {
		for (size_t index = job.next.fetch_add(1, std::memory_order_acq_rel); index < count; index = job.next.fetch_add(1, std::memory_order_acq_rel)) {
			TaskScope scope(info); // nested in the caller's task (or outermost for a caller without a scope)
			job.runIndex(index);
		}
	};

	Job job;
	job.count = count;
	job.body = body;
	job.context = context;
	job.perElement = perElement;
	job.info = info;
	bool parallel = count >= 2 && !isSerial() && !isHelperThread && !state.shutdown.load(std::memory_order_acquire);
	std::optional<TaskScope> ownScope;
	if (parallel) {
		if (!perElement) {
			if (!TaskScope::active())
				ownScope.emplace(info); // JOIN needs a scope id for the helpers to adopt
			// JOIN: publish the caller BEFORE any helper can load. The caller stays published until its outermost scope ends, so every object a
			// helper loads during the job (unlinked at the earliest after the dispatch, hence stamped >= the caller's epoch) outlives the call,
			// also after that helper left the job and unpublished: Ptrs handed from a helper to the caller or to another helper stay valid,
			// exactly as the shared scope id (C1) promises.
			TaskScope::ensurePublished();
		}
		job.callerScopeId = TaskScope::currentScopeId();
		SchedulerLock lock(state.mutex);
		if (state.stopping) {
			parallel = false;
		} else {
			state.ensureStarted(lock);
			state.jobs.push_back(&job);
			state.workAvailable.notify_all();
		}
	}
	if (!parallel) {
		runSerially(job);
		if (job.failed.load(std::memory_order_acquire))
			std::rethrow_exception(job.failure);
		return;
	}

	if (!perElement) {
		// the caller works too, in a NESTED scope (like runSerially): quiescentPoint() inside an element is a warned no-op on the caller as on
		// the helpers, so the caller never unpublishes or changes its scope id while helpers use Ptrs its publication protects
		TaskScope nested(info);
		job.work();
	}
	{
		BlockingRegion region("ForkJoinPool.parallelForEach");
		SchedulerLock lock(state.mutex);
		while (!job.allClaimed() || job.participants != 0) {
			if (state.stopping && job.participants == 0 && !job.allClaimed()) {
				// helpers stopped before claiming everything: finish on the caller
				lock.unlock();
				if (perElement) {
					runSerially(job);
				} else {
					TaskScope nested(info);
					job.work();
				}
				lock.lock();
				continue;
			}
			AION_PCT_BLOCKING_BEGIN("ForkJoinPool::join");
			state.jobProgress.wait(lock);
			AION_PCT_BLOCKING_END();
		}
		state.jobs.remove(&job);
	}
	if (job.failed.load(std::memory_order_acquire))
		std::rethrow_exception(job.failure);
}

} // namespace aion::gameserver::runtime
