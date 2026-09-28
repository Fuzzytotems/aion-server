#pragma once

#include <atomic>
#include <cstdint>

#include "aion/gameserver/runtime/sync/LockRank.h"

namespace aion::gameserver::runtime {

/**
 * Leaf mutex of the runtime internals (design §4.1 family 2). Non-reentrant, not fair, short critical sections only.
 *
 * Rules (all builds, C6/C14): acquiring rank r requires every held leaf mutex to have rank < r, otherwise lock() throws IllegalStateException
 * before blocking. While any leaf mutex is held, acquiring a game-level lock (Monitor, StampedLock, Semaphore) throws IllegalStateException.
 * Code holding a leaf mutex never runs user callbacks, never takes a Monitor, never blocks on IO and never allocates Refs whose release could
 * run user code (lint L18). Waits are recorded in the ThreadContext for the watchdog (the class name reported is lockRankName(rank)), and held
 * leaf mutexes are recorded in ThreadContext::heldLocks with their rank (all builds), so the watchdog resolves wait-graph edges through them.
 * Leaf mutexes are not validated by the lock-order validator: the rank rule already orders them. Kernel code logs only after releasing its
 * leaf mutexes (commons logging uses spdlog's own mutexes, so LOGGING is not enforced for logging calls).
 *
 * Satisfies BasicLockable/Lockable (lock, unlock, try_lock) for std::scoped_lock/std::unique_lock.
 * Thread-safety: all members are thread-safe; unlock by a thread that does not hold the mutex is undefined (checked builds terminate, C6).
 */
class LeafMutex {
public:
	explicit constexpr LeafMutex(LockRank rank) noexcept : rank_(rank) {}
	LeafMutex(const LeafMutex&) = delete;
	LeafMutex& operator=(const LeafMutex&) = delete;

	/** @throws IllegalStateException on a rank violation (all builds) */
	void lock();
	/** @throws IllegalStateException on a rank violation (all builds) */
	bool try_lock();
	void unlock() noexcept;

	LockRank rank() const noexcept { return rank_; }

private:
	std::atomic<uint32_t> state_{0}; // futex protocol: 0 free, 1 locked, 2 locked with waiters
	std::atomic<uint64_t> owner_{0}; // diagnostics: ThreadContext::threadId() of the holder
	const LockRank rank_;
};

/** A LeafMutex whose rank is part of its type: `RankedMutex<LockRank::SCHEDULER> heapMutex;` */
template <LockRank R>
class RankedMutex : public LeafMutex {
public:
	constexpr RankedMutex() noexcept : LeafMutex(R) {}
	static constexpr LockRank RANK = R;
};

} // namespace aion::gameserver::runtime
