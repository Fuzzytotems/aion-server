#include "aion/gameserver/runtime/sync/Semaphore.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/ThreadContext.h"
#include "aion/gameserver/runtime/base/YieldPoint.h"
#include "aion/gameserver/runtime/sync/BlockingRegion.h"
#include "aion/gameserver/runtime/sync/detail/Futex.h"

// permits_ is the futex word. A blocking acquirer increments waiters_ before loading the word it waits on; release publishes the new count
// before loading waiters_, so a release either sees the waiter (and wakes all waiters) or the waiter's futexWait sees the changed word.
// All waiters are woken because they may need different permit counts.

namespace aion::gameserver::runtime {

namespace {

void checkPermits(int32_t permits) {
	if (permits < 0)
		throw IllegalArgumentException("permits < 0");
}

void checkNoLeafHeld() {
	if (ThreadContext::current().heldLeafCount != 0) [[unlikely]]
		throw IllegalStateException("Acquiring a Semaphore while holding a leaf mutex (runtime bug, design §4.1)");
}

int32_t toSigned(uint32_t bits) noexcept {
	return static_cast<int32_t>(bits);
}

uint32_t toBits(int32_t value) noexcept {
	return static_cast<uint32_t>(value);
}

} // namespace

Semaphore::Semaphore(int32_t permits, bool fair) noexcept : permits_(toBits(permits)), fair_(fair) {
}

Semaphore::Semaphore(const LockClass& lockClass, int32_t permits, bool fair) noexcept
	: permits_(toBits(permits)), fair_(fair), lockClass_(&lockClass) {
}

bool Semaphore::tryTake(int32_t permits) noexcept {
	uint32_t current = permits_.load(std::memory_order_acquire);
	while (toSigned(current) >= permits) {
		if (permits_.compare_exchange_weak(current, toBits(toSigned(current) - permits), std::memory_order_acquire))
			return true;
	}
	return false;
}

bool Semaphore::acquireBlocking(int32_t permits, int64_t deadlineNanos) {
	BlockingRegion region("Semaphore.acquire");
	ThreadContext& context = ThreadContext::current();
	static const LockClass& fallbackClass = LockClass::named("Semaphore");
	const LockClass& lockClass = lockClass_ != nullptr ? *lockClass_ : fallbackClass;
	waiters_.fetch_add(1, std::memory_order_acquire);
	context.setWait(lockClass.name().c_str(), reinterpret_cast<uintptr_t>(this), 0, commons::utils::nanoTime());
	bool acquired = false;
	for (;;) {
		AION_YIELD_POINT("Semaphore::acquire");
		uint32_t current = permits_.load(std::memory_order_acquire);
		if (toSigned(current) >= permits) {
			if (permits_.compare_exchange_weak(current, toBits(toSigned(current) - permits), std::memory_order_acquire)) {
				acquired = true;
				break;
			}
			continue;
		}
		int64_t timeout = -1;
		if (deadlineNanos >= 0) {
			timeout = deadlineNanos - commons::utils::nanoTime();
			if (timeout <= 0)
				break;
		}
		AION_PCT_BLOCKING_BEGIN("Semaphore::acquire");
		detail::futexWait(permits_, current, timeout);
		AION_PCT_BLOCKING_END();
	}
	context.clearWait();
	waiters_.fetch_sub(1, std::memory_order_acquire);
	if (!acquired && toSigned(permits_.load(std::memory_order_acquire)) > 0 && waiters_.load(std::memory_order_acquire) != 0)
		detail::futexWakeAll(permits_); // fair mode: waiters may have deferred to this thread
	return acquired;
}

void Semaphore::acquire() {
	acquire(1);
}

void Semaphore::acquire(int32_t permits) {
	checkPermits(permits);
	checkNoLeafHeld();
	AION_YIELD_POINT("Semaphore::acquire");
	if ((!fair_ || waiters_.load(std::memory_order_acquire) == 0) && tryTake(permits))
		return;
	(void)acquireBlocking(permits, -1);
}

void Semaphore::acquireUninterruptibly() {
	acquire(1);
}

void Semaphore::acquireUninterruptibly(int32_t permits) {
	acquire(permits);
}

bool Semaphore::tryAcquire() {
	return tryAcquire(1);
}

bool Semaphore::tryAcquire(int32_t permits) {
	checkPermits(permits);
	AION_YIELD_POINT("Semaphore::tryAcquire");
	return tryTake(permits);
}

bool Semaphore::tryAcquire(std::chrono::nanoseconds timeout) {
	return tryAcquire(1, timeout);
}

bool Semaphore::tryAcquire(int32_t permits, std::chrono::nanoseconds timeout) {
	checkPermits(permits);
	checkNoLeafHeld();
	AION_YIELD_POINT("Semaphore::tryAcquire");
	if ((!fair_ || waiters_.load(std::memory_order_acquire) == 0) && tryTake(permits))
		return true;
	if (timeout.count() <= 0)
		return false;
	return acquireBlocking(permits, commons::utils::nanoTime() + timeout.count());
}

void Semaphore::release() {
	release(1);
}

void Semaphore::release(int32_t permits) {
	checkPermits(permits);
	AION_YIELD_POINT("Semaphore::release");
	uint32_t current = permits_.load(std::memory_order_acquire);
	while (!permits_.compare_exchange_weak(current, toBits(static_cast<int32_t>(static_cast<uint32_t>(current) + static_cast<uint32_t>(permits))),
		std::memory_order_acquire)) {
	}
	if (waiters_.load(std::memory_order_acquire) != 0)
		detail::futexWakeAll(permits_);
}

int32_t Semaphore::availablePermits() const noexcept {
	return toSigned(permits_.load(std::memory_order_acquire));
}

int32_t Semaphore::drainPermits() noexcept {
	uint32_t current = permits_.load(std::memory_order_acquire);
	while (current != 0 && !permits_.compare_exchange_weak(current, 0, std::memory_order_acquire)) {
	}
	return toSigned(current);
}

} // namespace aion::gameserver::runtime
