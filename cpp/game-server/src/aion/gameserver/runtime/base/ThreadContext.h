#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <source_location>

#include "aion/gameserver/runtime/base/TaskInfo.h"

namespace aion::gameserver::runtime {

/** Published-epoch value of a thread that holds no borrows (design §2.4 "IDLE"). */
inline constexpr uint64_t EPOCH_IDLE = std::numeric_limits<uint64_t>::max();

/**
 * Per-thread kernel state, one record per thread that ever touched the kernel (design §1.2, §2.4, §4.3).
 *
 * Records are registered lazily by ThreadContext::current() on first use, never freed (a record is recycled after its thread exits), and
 * linked into a global lock-free list, so any thread (Reclaimer scan, watchdog, introspection) may read any record at any time without
 * lifetime concerns. This is the only data shared between the lifetime core (epochs, scopes) and the sync layer (held locks, waits, blocking
 * regions) without an upward dependency: base owns the layout, each area owns the semantics of its fields.
 *
 * Field access rules:
 * - "owner" fields are read and written only by the thread that owns the record.
 * - "shared" fields are atomics written by the owner and read by other threads; readers must tolerate values that change between two loads
 *   (they are diagnostics, except publishedEpoch whose protocol is design §2.4).
 * - Strings referenced by pointer (lock class names, TaskInfo file/function names, blocking region names) must have static storage duration
 *   or belong to immortal objects (interned LockClass names), so a reader never dereferences freed memory (RR-18).
 */
class ThreadContext {
public:
	/** Maximum number of Monitors/leaf mutexes recorded as held (deeper nesting is counted but not recorded). */
	static constexpr uint32_t MAX_RECORDED_LOCKS = 32;
	/** Maximum nesting depth of held leaf mutexes (RankedMutex). */
	static constexpr uint32_t MAX_LEAF_LOCKS = 16;

	/** A lock recorded as held by this thread, for lock-order validation and watchdog dumps. */
	struct HeldLock {
		/** interned lock class name (immortal) */
		std::atomic<const char*> lockClassName{nullptr};
		/** identity of the lock object (its address); never dereferenced by other threads */
		std::atomic<uintptr_t> lockId{0};
		/** 0 for game-level Monitors (rank 0), otherwise the LockRank of the leaf mutex */
		std::atomic<uint8_t> rank{0};
	};

	/** Consistent copy of the task currently running on a thread. */
	struct TaskSnapshot {
		bool active = false;
		TaskInfo info{};
		uint64_t scopeId = 0;
		int64_t startNanos = 0;
	};

	/** Consistent copy of the blocking region a thread is in. */
	struct BlockingSnapshot {
		bool active = false;
		const char* what = nullptr;
		std::source_location where{};
		int64_t sinceNanos = 0;
	};

	/** Consistent copy of the lock wait a thread is blocked in (design §4.3). */
	struct WaitSnapshot {
		bool waiting = false;
		const char* lockClassName = nullptr;
		uintptr_t lockId = 0;
		/** ThreadContext::threadId() of the owner at wait start, 0 if unknown/none */
		uint64_t ownerThreadId = 0;
		int64_t waitStartNanos = 0;
	};

	ThreadContext(const ThreadContext&) = delete;
	ThreadContext& operator=(const ThreadContext&) = delete;

	/** The calling thread's record, registered on first use. Never fails (allocation failure terminates). */
	static ThreadContext& current() noexcept;

	/** The calling thread's record if it has one, else nullptr. Does not register. */
	static ThreadContext* currentIfRegistered() noexcept;

	/**
	 * Visits every record that currently belongs to a live thread. Safe to call from any thread at any time; records registered or released
	 * concurrently may or may not be visited. The visitor must not register new threads.
	 */
	template <class F>
	static void forEach(F&& visitor) {
		for (ThreadContext* context = head.load(std::memory_order_acquire); context != nullptr; context = context->next)
			if (context->inUse.load(std::memory_order_acquire))
				visitor(static_cast<const ThreadContext&>(*context));
	}

	/** Number of records currently owned by live threads. */
	static uint32_t liveThreadCount() noexcept;

	/** Unique id of the thread that owns the record (never 0, never reused within the process). */
	uint64_t threadId() const noexcept { return id.load(std::memory_order_acquire); }

	/**
	 * Thread name captured at registration or by refreshName() (commons getCurrentThreadName). Any thread; the returned string is never freed
	 * (names are interned, so a racing refresh cannot invalidate it).
	 */
	const char* threadName() const noexcept { return name.load(std::memory_order_acquire); }

	/** owner: re-reads the thread name (call after setCurrentThreadName; pools do this when they start a worker). */
	void refreshName();

	// ----------------------------------------------------------------------------------------------------------- lifetime core (§2.4, §2.6)

	/** shared: the epoch published by the first pointer load of the current scope, or EPOCH_IDLE (design §2.4). */
	std::atomic<uint64_t> publishedEpoch{EPOCH_IDLE};
	/** owner: TaskScope nesting depth */
	uint32_t scopeDepth = 0;
	/** shared: id of the current outermost scope (0 = none); Ptr stamps compare against it (C1) */
	std::atomic<uint64_t> scopeId{0};
	/** owner: true while this thread runs a joined ForkJoin helper scope (quiescentPoint is a no-op, design §2.6) */
	bool joinedHelper = false;
	/** owner: number of open QuiescentScopes */
	uint32_t quiescentScopeDepth = 0;
	/** owner: >0 while the Reclaimer runs destructors on this thread (C8: dereferences terminate) */
	uint32_t destructorContextDepth = 0;

	/** owner: publishes the running task for other threads (seqlock write). */
	void setTask(const TaskInfo& info, uint64_t currentScopeId, int64_t startNanos) noexcept;
	/** owner: clears the running task. */
	void clearTask() noexcept;
	/** any thread: consistent snapshot of the running task. */
	TaskSnapshot task() const noexcept;

	// ----------------------------------------------------------------------------------------------------------- sync layer (§1.2, §4.2, §4.3)

	/** owner: enters/leaves a blocking region (nesting keeps the outermost). */
	void setBlocking(const char* what, const std::source_location& where, int64_t sinceNanos) noexcept;
	void clearBlocking() noexcept;
	/** any thread */
	BlockingSnapshot blocking() const noexcept;
	/** owner: nesting depth of BlockingRegions */
	uint32_t blockingDepth = 0;

	/** owner: records the lock this thread is about to wait for (design §4.3; the lock object is never dereferenced by readers). */
	void setWait(const char* lockClassName, uintptr_t lockId, uint64_t ownerThreadId, int64_t waitStartNanos) noexcept;
	void clearWait() noexcept;
	/** any thread */
	WaitSnapshot wait() const noexcept;

	/** owner writes, any thread reads: locks held in acquisition order; entries >= heldLockCount are stale */
	std::array<HeldLock, MAX_RECORDED_LOCKS> heldLocks{};
	/** shared: number of held locks (may exceed MAX_RECORDED_LOCKS; only the first MAX_RECORDED_LOCKS are recorded) */
	std::atomic<uint32_t> heldLockCount{0};

	/** owner: ranks of held leaf mutexes, innermost last (C6) */
	std::array<uint8_t, MAX_LEAF_LOCKS> heldLeafRanks{};
	/** owner: number of held leaf mutexes */
	uint32_t heldLeafCount = 0;

	/** owner: suppression depth for the lock-order validator (`// lockdep: <reason>` sites) */
	uint32_t lockdepSuppressionDepth = 0;

private:
	ThreadContext() = default;
	~ThreadContext() = default;

	friend struct ThreadContextAccess;

	static std::atomic<ThreadContext*> head;

	ThreadContext* next = nullptr;
	std::atomic<bool> inUse{false};
	std::atomic<uint64_t> id{0};
	std::atomic<const char*> name{""};

	// task seqlock: odd while being written
	std::atomic<uint32_t> taskSeq{0};
	std::atomic<bool> taskActive{false};
	std::atomic<TaskInfo> taskInfo{TaskInfo{}}; // trivially copyable; not lock-free (MSVC uses an internal spinlock)
	std::atomic<uint64_t> taskScopeId{0};
	std::atomic<int64_t> taskStartNanos{0};

	std::atomic<uint32_t> blockingSeq{0};
	std::atomic<bool> blockingActive{false};
	std::atomic<const char*> blockingWhat{nullptr};
	std::atomic<std::source_location> blockingWhere{std::source_location{}};
	std::atomic<int64_t> blockingSince{0};

	std::atomic<uint32_t> waitSeq{0};
	std::atomic<bool> waitActive{false};
	std::atomic<const char*> waitLockClassName{nullptr};
	std::atomic<uintptr_t> waitLockId{0};
	std::atomic<uint64_t> waitOwnerThreadId{0};
	std::atomic<int64_t> waitStartNanos{0};
};

} // namespace aion::gameserver::runtime
