#include "aion/gameserver/runtime/sync/Monitor.h"

#include <thread>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/ThreadContext.h"
#include "aion/gameserver/runtime/base/YieldPoint.h"
#include "aion/gameserver/runtime/sync/LockOrderValidator.h"
#include "aion/gameserver/runtime/sync/detail/Futex.h"

// Lock word protocol (state_, a futex word)
//   bit 0      LOCKED
//   bit 1      STARVING: a registered waiter has waited longer than STARVATION_NANOS; unregistered threads must not barge
//   bits 2..31 number of registered waiters (threads between registration and deregistration in the slow path)
//
// Fast path: CAS from a word without LOCKED and STARVING to word | LOCKED.
// Slow path: spin briefly, then register as a waiter (count + 1) and loop: if the lock is free, CAS it to LOCKED while deregistering
// (registered waiters may take the lock even in starvation mode); otherwise mark STARVING after STARVATION_NANOS and futex-wait on the
// observed word. unlock clears LOCKED and wakes one thread if the count is non-zero. A waiter that gives up (timeout) deregisters and passes a
// possibly consumed wakeup on when the lock is free and other waiters remain. Starvation mode ends when the acquiring waiter was the last one
// or did not wait long (Go's sync.Mutex rule), so bursts of contention do not permanently disable barging.
//
// This gives "eventual fairness": a waiter can be overtaken by barging threads for at most about STARVATION_NANOS (plus scheduling latency),
// after which waiters are served in the OS wake order (approximately FIFO for WaitOnAddress/futex).

namespace aion::gameserver::runtime {

namespace {

constexpr uint32_t LOCKED = 1;
constexpr uint32_t STARVING = 2;
constexpr uint32_t WAITER = 4;
constexpr int64_t STARVATION_NANOS = 1'000'000;
constexpr int SPIN_ROUNDS = 32;

void checkNoLeafHeld(const ThreadContext& context) {
	if (context.heldLeafCount != 0) [[unlikely]]
		throw IllegalStateException("Acquiring a Monitor while holding a leaf mutex (runtime bug, design §4.1)");
}

inline uintptr_t lockIdOf(const Monitor* monitor) noexcept {
	return reinterpret_cast<uintptr_t>(monitor);
}

inline void cpuRelax() noexcept {
	std::this_thread::yield();
}

} // namespace

bool Monitor::tryAcquireWord(bool barge) noexcept {
	uint32_t word = state_.load(std::memory_order_acquire);
	uint32_t blocked = barge ? LOCKED : (LOCKED | STARVING);
	while ((word & blocked) == 0) {
		if (state_.compare_exchange_weak(word, word | LOCKED, std::memory_order_acquire))
			return true;
	}
	return false;
}

bool Monitor::acquireSlow(ThreadContext& context, const LockClass& lockClass, int64_t deadlineNanos) {
	for (int round = 0; round < SPIN_ROUNDS; ++round) {
		if (tryAcquireWord(false))
			return true;
		if ((state_.load(std::memory_order_relaxed) >> 2) != 0)
			break; // others already wait: spinning would only compete with them
		cpuRelax();
	}

	// register as a waiter
	uint32_t word = state_.load(std::memory_order_acquire);
	for (;;) {
		if ((word & (LOCKED | STARVING)) == 0) {
			if (state_.compare_exchange_weak(word, word | LOCKED, std::memory_order_acquire))
				return true;
			continue;
		}
		if (state_.compare_exchange_weak(word, word + WAITER, std::memory_order_acquire))
			break;
	}

	int64_t waitStart = commons::utils::nanoTime();
	context.setWait(lockClass.name().c_str(), lockIdOf(this), owner_.load(std::memory_order_acquire), waitStart);
	bool acquired = false;
	for (;;) {
		word = state_.load(std::memory_order_acquire);
		int64_t now = commons::utils::nanoTime();
		if ((word & LOCKED) == 0) {
			uint32_t next = (word | LOCKED) - WAITER;
			if ((next >> 2) == 0 || now - waitStart < STARVATION_NANOS)
				next &= ~STARVING;
			if (state_.compare_exchange_weak(word, next, std::memory_order_acquire)) {
				acquired = true;
				break;
			}
			continue;
		}
		if ((word & STARVING) == 0 && now - waitStart >= STARVATION_NANOS) {
			state_.compare_exchange_weak(word, word | STARVING, std::memory_order_acquire);
			continue;
		}
		int64_t timeout = -1;
		if (deadlineNanos >= 0) {
			timeout = deadlineNanos - now;
			if (timeout <= 0)
				break;
		}
		AION_PCT_BLOCKING_BEGIN("Monitor::lock");
		detail::futexWait(state_, word, timeout);
		AION_PCT_BLOCKING_END();
	}
	context.clearWait();
	if (acquired)
		return true;

	// timed out: deregister; hand on a wakeup this waiter may have consumed
	word = state_.load(std::memory_order_acquire);
	uint32_t next;
	do {
		next = word - WAITER;
		if ((next >> 2) == 0)
			next &= ~STARVING;
	} while (!state_.compare_exchange_weak(word, next, std::memory_order_acquire));
	if ((next & LOCKED) == 0 && (next >> 2) != 0)
		detail::futexWakeOne(state_);
	return false;
}

void Monitor::lock() {
	lock(lockClass_ != nullptr ? *lockClass_ : LockClass::anonymousMonitor());
}

void Monitor::lock(const LockClass& dynamicClass) {
	ThreadContext& context = ThreadContext::current();
	uint64_t self = context.threadId();
	if (owner_.load(std::memory_order_relaxed) == self) {
		++holdCount_;
		return;
	}
	checkNoLeafHeld(context);
	const LockClass& lockClass = lockClass_ != nullptr ? *lockClass_ : dynamicClass;
	LockOrderValidator& validator = LockOrderValidator::getInstance();
	if (CHECKED)
		validator.beforeAcquire(lockClass, lockIdOf(this));
	AION_YIELD_POINT("Monitor::lock");
	if (!tryAcquireWord(false))
		(void)acquireSlow(context, lockClass, -1);
	owner_.store(self, std::memory_order_release);
	holdCount_ = 1;
	validator.afterAcquire(lockClass, lockIdOf(this));
}

bool Monitor::tryLock() {
	return tryLock(lockClass_ != nullptr ? *lockClass_ : LockClass::anonymousMonitor());
}

bool Monitor::tryLock(const LockClass& dynamicClass) {
	ThreadContext& context = ThreadContext::current();
	uint64_t self = context.threadId();
	if (owner_.load(std::memory_order_relaxed) == self) {
		++holdCount_;
		return true;
	}
	checkNoLeafHeld(context);
	AION_YIELD_POINT("Monitor::tryLock");
	if (!tryAcquireWord(true))
		return false;
	owner_.store(self, std::memory_order_release);
	holdCount_ = 1;
	LockOrderValidator::getInstance().afterAcquire(lockClass_ != nullptr ? *lockClass_ : dynamicClass, lockIdOf(this));
	return true;
}

bool Monitor::tryLock(std::chrono::nanoseconds timeout) {
	return tryLock(timeout, lockClass_ != nullptr ? *lockClass_ : LockClass::anonymousMonitor());
}

bool Monitor::tryLock(std::chrono::nanoseconds timeout, const LockClass& dynamicClass) {
	ThreadContext& context = ThreadContext::current();
	uint64_t self = context.threadId();
	if (owner_.load(std::memory_order_relaxed) == self) {
		++holdCount_;
		return true;
	}
	checkNoLeafHeld(context);
	const LockClass& lockClass = lockClass_ != nullptr ? *lockClass_ : dynamicClass;
	LockOrderValidator& validator = LockOrderValidator::getInstance();
	// A timed acquisition can block, so its edges are recorded whether or not this attempt happens to be contended (review fix: recording them
	// only after a failed first attempt made lockdep's findings depend on timing).
	if (CHECKED && timeout.count() > 0)
		validator.beforeAcquire(lockClass, lockIdOf(this));
	AION_YIELD_POINT("Monitor::tryLock");
	if (!tryAcquireWord(true)) {
		if (timeout.count() <= 0)
			return false;
		if (!acquireSlow(context, lockClass, commons::utils::nanoTime() + timeout.count()))
			return false;
	}
	owner_.store(self, std::memory_order_release);
	holdCount_ = 1;
	validator.afterAcquire(lockClass, lockIdOf(this));
	return true;
}

void Monitor::unlock() {
	ThreadContext* context = ThreadContext::currentIfRegistered();
	if (context == nullptr || owner_.load(std::memory_order_relaxed) != context->threadId()) [[unlikely]]
		throw IllegalMonitorStateException("Monitor::unlock by a thread that does not hold the monitor");
	if (--holdCount_ > 0)
		return;
	owner_.store(0, std::memory_order_release);
	LockOrderValidator::getInstance().afterRelease(lockIdOf(this));
	AION_YIELD_POINT("Monitor::unlock");
	uint32_t word = state_.load(std::memory_order_acquire);
	while (!state_.compare_exchange_weak(word, word & ~LOCKED, std::memory_order_acquire)) {
	}
	if ((word >> 2) != 0)
		detail::futexWakeOne(state_);
}

bool Monitor::isHeldByCurrentThread() const noexcept {
	ThreadContext* context = ThreadContext::currentIfRegistered();
	return context != nullptr && owner_.load(std::memory_order_relaxed) == context->threadId();
}

int32_t Monitor::getHoldCount() const noexcept {
	return isHeldByCurrentThread() ? static_cast<int32_t>(holdCount_) : 0;
}

bool Monitor::isLocked() const noexcept {
	return (state_.load(std::memory_order_relaxed) & LOCKED) != 0;
}

} // namespace aion::gameserver::runtime
