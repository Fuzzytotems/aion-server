#include "aion/gameserver/runtime/sync/RankedMutex.h"

#include <string>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/runtime/base/Checked.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/ThreadContext.h"
#include "aion/gameserver/runtime/base/YieldPoint.h"
#include "aion/gameserver/runtime/sync/detail/Futex.h"
#include "aion/gameserver/runtime/sync/detail/HeldLocks.h"

// Lock word: 0 free, 1 locked, 2 locked with (possible) waiters (the classic three-state futex mutex). Leaf mutexes guard short internal
// critical sections, so there is no starvation handling. Waits are recorded for the watchdog with the rank name as lock class; held leaf
// mutexes are recorded in ThreadContext::heldLocks (rank != 0) so the watchdog can resolve wait-graph edges through them (all builds).

namespace aion::gameserver::runtime {

namespace {

void checkRank(const ThreadContext& context, LockRank rank) {
	if (context.heldLeafCount == 0)
		return;
	uint8_t innermost = context.heldLeafRanks[context.heldLeafCount - 1];
	if (innermost >= static_cast<uint8_t>(rank)) [[unlikely]]
		throw IllegalStateException("Leaf mutex rank violation: acquiring " + std::string(lockRankName(rank)) + " while holding rank " +
			std::to_string(innermost) + " (runtime bug, design §4.1)");
	if (context.heldLeafCount >= ThreadContext::MAX_LEAF_LOCKS) [[unlikely]]
		throw IllegalStateException("Leaf mutex nesting too deep");
}

void recordAcquired(ThreadContext& context, const LeafMutex* mutex, LockRank rank) noexcept {
	context.heldLeafRanks[context.heldLeafCount++] = static_cast<uint8_t>(rank);
	detail::pushHeldLock(context, lockRankName(rank).data(), reinterpret_cast<uintptr_t>(mutex), static_cast<uint8_t>(rank));
}

} // namespace

void LeafMutex::lock() {
	ThreadContext& context = ThreadContext::current();
	checkRank(context, rank_);
	AION_YIELD_POINT("LeafMutex::lock");
	uint32_t expected = 0;
	if (!state_.compare_exchange_strong(expected, 1, std::memory_order_acquire)) {
		context.setWait(lockRankName(rank_).data(), reinterpret_cast<uintptr_t>(this), owner_.load(std::memory_order_relaxed),
			commons::utils::nanoTime());
		if (expected != 2)
			expected = state_.exchange(2, std::memory_order_acquire);
		while (expected != 0) {
			AION_PCT_BLOCKING_BEGIN("LeafMutex::lock");
			detail::futexWait(state_, 2);
			AION_PCT_BLOCKING_END();
			expected = state_.exchange(2, std::memory_order_acquire);
		}
		context.clearWait();
	}
	owner_.store(context.threadId(), std::memory_order_relaxed);
	recordAcquired(context, this, rank_);
}

bool LeafMutex::try_lock() {
	ThreadContext& context = ThreadContext::current();
	checkRank(context, rank_);
	AION_YIELD_POINT("LeafMutex::try_lock");
	uint32_t expected = 0;
	if (!state_.compare_exchange_strong(expected, 1, std::memory_order_acquire))
		return false;
	owner_.store(context.threadId(), std::memory_order_relaxed);
	recordAcquired(context, this, rank_);
	return true;
}

void LeafMutex::unlock() noexcept {
	ThreadContext& context = ThreadContext::current();
	AION_CHECK("C6", context.heldLeafCount > 0 && owner_.load(std::memory_order_relaxed) == context.threadId(),
		"LeafMutex::unlock by a thread that does not hold it");
	// kernel code releases innermost-first (scoped locks); tolerate out-of-order unlocks by removing the innermost entry of this rank
	for (uint32_t i = context.heldLeafCount; i-- > 0;) {
		if (context.heldLeafRanks[i] == static_cast<uint8_t>(rank_)) {
			for (uint32_t j = i; j + 1 < context.heldLeafCount; ++j)
				context.heldLeafRanks[j] = context.heldLeafRanks[j + 1];
			--context.heldLeafCount;
			break;
		}
	}
	detail::popHeldLock(context, reinterpret_cast<uintptr_t>(this));
	owner_.store(0, std::memory_order_relaxed);
	AION_YIELD_POINT("LeafMutex::unlock");
	if (state_.exchange(0, std::memory_order_acquire) == 2)
		detail::futexWakeOne(state_);
}

} // namespace aion::gameserver::runtime
