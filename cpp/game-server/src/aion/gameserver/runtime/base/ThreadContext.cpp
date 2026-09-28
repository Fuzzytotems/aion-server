#include "aion/gameserver/runtime/base/ThreadContext.h"

#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>

#include "aion/commons/utils/concurrent/ThreadName.h"

namespace aion::gameserver::runtime {

std::atomic<ThreadContext*> ThreadContext::head{nullptr};

namespace {

std::atomic<uint64_t> nextThreadId{1};
std::atomic<uint32_t> liveThreads{0};

/** Interned thread names (never freed, so ThreadContext::threadName() pointers stay valid). Only touched on registration/refresh. */
const char* internName(const std::string& name) {
	// leaked on purpose (usable during static destruction); plain std::mutex: base sits below the lock ranks and runs no callbacks under it
	static auto* mutex = new std::mutex();
	static auto* names = new std::unordered_set<std::string>();
	std::scoped_lock lock(*mutex);
	return names->insert(name).first->c_str();
}

} // namespace

struct ThreadContextAccess {
	static ThreadContext* acquire() {
		// reuse a record of an exited thread
		for (ThreadContext* context = ThreadContext::head.load(std::memory_order_acquire); context != nullptr; context = context->next) {
			bool expected = false;
			if (!context->inUse.load(std::memory_order_relaxed) && context->inUse.compare_exchange_strong(expected, true))
				return context;
		}
		auto* context = new ThreadContext();
		context->inUse.store(true, std::memory_order_relaxed);
		ThreadContext* oldHead = ThreadContext::head.load(std::memory_order_acquire);
		do {
			context->next = oldHead;
		} while (!ThreadContext::head.compare_exchange_weak(oldHead, context, std::memory_order_acq_rel));
		return context;
	}

	static void reset(ThreadContext& context) noexcept {
		context.publishedEpoch.store(EPOCH_IDLE, std::memory_order_release);
		context.scopeDepth = 0;
		context.scopeId.store(0, std::memory_order_release);
		context.joinedHelper = false;
		context.quiescentScopeDepth = 0;
		context.destructorContextDepth = 0;
		context.clearTask();
		context.blockingDepth = 0;
		context.clearBlocking();
		context.clearWait();
		context.heldLockCount.store(0, std::memory_order_release);
		context.heldLeafCount = 0;
		context.lockdepSuppressionDepth = 0;
	}

	static void assignId(ThreadContext& context) noexcept {
		context.id.store(nextThreadId.fetch_add(1, std::memory_order_relaxed), std::memory_order_release);
	}

	static void release(ThreadContext& context) noexcept {
		reset(context);
		context.inUse.store(false, std::memory_order_release);
	}
};

namespace {

/** Owns the calling thread's record; releases it for reuse when the thread exits. */
struct ThreadContextHolder {
	ThreadContext* context = nullptr;

	ThreadContext& get() noexcept {
		if (context == nullptr) [[unlikely]] {
			context = ThreadContextAccess::acquire();
			ThreadContextAccess::reset(*context);
			ThreadContextAccess::assignId(*context);
			context->refreshName();
			liveThreads.fetch_add(1, std::memory_order_relaxed);
		}
		return *context;
	}

	~ThreadContextHolder() {
		if (context != nullptr) {
			ThreadContextAccess::release(*context);
			liveThreads.fetch_sub(1, std::memory_order_relaxed);
			context = nullptr; // a later thread_local destructor that uses the kernel registers a fresh record (leaked, bounded)
		}
	}
};

thread_local ThreadContextHolder holder;

} // namespace

ThreadContext& ThreadContext::current() noexcept {
	return holder.get();
}

ThreadContext* ThreadContext::currentIfRegistered() noexcept {
	return holder.context;
}

uint32_t ThreadContext::liveThreadCount() noexcept {
	return liveThreads.load(std::memory_order_relaxed);
}

void ThreadContext::refreshName() {
	name.store(internName(commons::utils::concurrent::getCurrentThreadName()), std::memory_order_release);
}

void ThreadContext::setTask(const TaskInfo& info, uint64_t currentScopeId, int64_t startNanos) noexcept {
	taskSeq.fetch_add(1, std::memory_order_release);
	taskInfo.store(info, std::memory_order_release);
	taskScopeId.store(currentScopeId, std::memory_order_release);
	taskStartNanos.store(startNanos, std::memory_order_release);
	taskActive.store(true, std::memory_order_release);
	taskSeq.fetch_add(1, std::memory_order_release);
}

void ThreadContext::clearTask() noexcept {
	taskSeq.fetch_add(1, std::memory_order_release);
	taskActive.store(false, std::memory_order_release);
	taskSeq.fetch_add(1, std::memory_order_release);
}

ThreadContext::TaskSnapshot ThreadContext::task() const noexcept {
	TaskSnapshot snapshot;
	for (;;) {
		uint32_t before = taskSeq.load(std::memory_order_acquire);
		if ((before & 1) == 0) {
			snapshot.active = taskActive.load(std::memory_order_acquire);
			snapshot.info = taskInfo.load(std::memory_order_acquire);
			snapshot.scopeId = taskScopeId.load(std::memory_order_acquire);
			snapshot.startNanos = taskStartNanos.load(std::memory_order_acquire);
			if (taskSeq.load(std::memory_order_acquire) == before)
				return snapshot;
		}
		std::this_thread::yield();
	}
}

void ThreadContext::setBlocking(const char* what, const std::source_location& where, int64_t sinceNanos) noexcept {
	blockingSeq.fetch_add(1, std::memory_order_release);
	blockingWhat.store(what, std::memory_order_release);
	blockingWhere.store(where, std::memory_order_release);
	blockingSince.store(sinceNanos, std::memory_order_release);
	blockingActive.store(true, std::memory_order_release);
	blockingSeq.fetch_add(1, std::memory_order_release);
}

void ThreadContext::clearBlocking() noexcept {
	blockingSeq.fetch_add(1, std::memory_order_release);
	blockingActive.store(false, std::memory_order_release);
	blockingSeq.fetch_add(1, std::memory_order_release);
}

ThreadContext::BlockingSnapshot ThreadContext::blocking() const noexcept {
	BlockingSnapshot snapshot;
	for (;;) {
		uint32_t before = blockingSeq.load(std::memory_order_acquire);
		if ((before & 1) == 0) {
			snapshot.active = blockingActive.load(std::memory_order_acquire);
			snapshot.what = blockingWhat.load(std::memory_order_acquire);
			snapshot.where = blockingWhere.load(std::memory_order_acquire);
			snapshot.sinceNanos = blockingSince.load(std::memory_order_acquire);
			if (blockingSeq.load(std::memory_order_acquire) == before)
				return snapshot;
		}
		std::this_thread::yield();
	}
}

void ThreadContext::setWait(const char* lockClassName, uintptr_t lockId, uint64_t ownerThreadId, int64_t startNanos) noexcept {
	waitSeq.fetch_add(1, std::memory_order_release);
	waitLockClassName.store(lockClassName, std::memory_order_release);
	waitLockId.store(lockId, std::memory_order_release);
	waitOwnerThreadId.store(ownerThreadId, std::memory_order_release);
	waitStartNanos.store(startNanos, std::memory_order_release);
	waitActive.store(true, std::memory_order_release);
	waitSeq.fetch_add(1, std::memory_order_release);
}

void ThreadContext::clearWait() noexcept {
	waitSeq.fetch_add(1, std::memory_order_release);
	waitActive.store(false, std::memory_order_release);
	waitSeq.fetch_add(1, std::memory_order_release);
}

ThreadContext::WaitSnapshot ThreadContext::wait() const noexcept {
	WaitSnapshot snapshot;
	for (;;) {
		uint32_t before = waitSeq.load(std::memory_order_acquire);
		if ((before & 1) == 0) {
			snapshot.waiting = waitActive.load(std::memory_order_acquire);
			snapshot.lockClassName = waitLockClassName.load(std::memory_order_acquire);
			snapshot.lockId = waitLockId.load(std::memory_order_acquire);
			snapshot.ownerThreadId = waitOwnerThreadId.load(std::memory_order_acquire);
			snapshot.waitStartNanos = waitStartNanos.load(std::memory_order_acquire);
			if (waitSeq.load(std::memory_order_acquire) == before)
				return snapshot;
		}
		std::this_thread::yield();
	}
}

} // namespace aion::gameserver::runtime
