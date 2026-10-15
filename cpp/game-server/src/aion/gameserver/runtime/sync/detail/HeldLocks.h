#pragma once

#include <atomic>
#include <cstdint>

#include "aion/gameserver/runtime/base/ThreadContext.h"

namespace aion::gameserver::runtime::detail {

/**
 * Held-lock records in ThreadContext::heldLocks (owner writes, any thread reads). Maintained in ALL builds for every game-level lock (Monitor,
 * StampedLock) and leaf mutex, because the watchdog (kept in release builds, C14) verifies wait-graph edges against them: a thread waiting for
 * lock L has an edge to the thread whose records contain L (design §4.3). Only the first MAX_RECORDED_LOCKS locks are recorded; deeper
 * nesting is counted.
 *
 * Write order keeps concurrent readers safe (they tolerate stale entries): push writes the entry, then publishes the count; pop shifts the
 * entries above the removed one down, then publishes the count.
 */
inline void pushHeldLock(ThreadContext& context, const char* lockClassName, uintptr_t lockId, uint8_t rank) noexcept {
	uint32_t count = context.heldLockCount.load(std::memory_order_relaxed);
	if (count < ThreadContext::MAX_RECORDED_LOCKS) {
		ThreadContext::HeldLock& entry = context.heldLocks[count];
		entry.lockClassName.store(lockClassName, std::memory_order_release);
		entry.lockId.store(lockId, std::memory_order_release);
		entry.rank.store(rank, std::memory_order_release);
	}
	context.heldLockCount.store(count + 1, std::memory_order_release);
}

/**
 * Removes the innermost record of `lockId` (releases need not be LIFO). If the lock is not recorded on this thread (released by another
 * thread, e.g. a StampedLock read stamp handed over) nothing is removed, unless the count exceeds the recorded entries (the lock was among the
 * unrecorded deep ones). @return true if a record was removed or the count was decremented
 */
inline bool popHeldLock(ThreadContext& context, uintptr_t lockId) noexcept {
	uint32_t count = context.heldLockCount.load(std::memory_order_relaxed);
	if (count == 0)
		return false;
	uint32_t recorded = count < ThreadContext::MAX_RECORDED_LOCKS ? count : ThreadContext::MAX_RECORDED_LOCKS;
	for (uint32_t i = recorded; i-- > 0;) {
		if (context.heldLocks[i].lockId.load(std::memory_order_relaxed) != lockId)
			continue;
		// with more than MAX_RECORDED_LOCKS held, shifting leaves the last recorded slot stale until the deep locks are released; readers only
		// look at the first min(count, MAX_RECORDED_LOCKS) entries, so diagnostics become coarser but never wrong about lock identity
		for (uint32_t j = i; j + 1 < recorded; ++j) {
			ThreadContext::HeldLock& to = context.heldLocks[j];
			const ThreadContext::HeldLock& from = context.heldLocks[j + 1];
			to.lockClassName.store(from.lockClassName.load(std::memory_order_relaxed), std::memory_order_release);
			to.lockId.store(from.lockId.load(std::memory_order_relaxed), std::memory_order_release);
			to.rank.store(from.rank.load(std::memory_order_relaxed), std::memory_order_release);
		}
		context.heldLockCount.store(count - 1, std::memory_order_release);
		return true;
	}
	if (count > ThreadContext::MAX_RECORDED_LOCKS) {
		context.heldLockCount.store(count - 1, std::memory_order_release);
		return true;
	}
	return false;
}

} // namespace aion::gameserver::runtime::detail
