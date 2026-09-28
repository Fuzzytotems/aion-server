#include "aion/gameserver/runtime/sync/StampedLock.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/runtime/base/Checked.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/ThreadContext.h"
#include "aion/gameserver/runtime/base/YieldPoint.h"
#include "aion/gameserver/runtime/sync/LockOrderValidator.h"

// state_: bits 0..31 reader count, bits 32..61 version (incremented by every write lock), bit 62 writer.
// Stamps: version bits | tag (1 = read, 2 = write, 3 = optimistic); never 0 because version 0 is represented as 1.
// Blocking uses std::atomic<uint64_t>::wait (no timed operations exist). Lost wakeups are excluded because a waiter increments waiters_
// after loading the word it waits on, and unlockRead loads waiters_ after publishing its decrement (sequentially consistent atomics);
// unlockWrite always notifies.

namespace aion::gameserver::runtime {

namespace {

constexpr uint64_t READERS = 0xFFFF'FFFFull;
constexpr uint64_t VERSION_UNIT = 1ull << 32;
constexpr uint64_t VERSION_MASK = ((1ull << 62) - 1) & ~READERS;
constexpr uint64_t WRITER = 1ull << 62;
constexpr int64_t TAG_READ = 1;
constexpr int64_t TAG_WRITE = 2;
constexpr int64_t TAG_OPTIMISTIC = 3;

uint64_t version(uint64_t state) noexcept {
	uint64_t v = state & VERSION_MASK;
	return v == 0 ? VERSION_UNIT : v; // version 0 is represented as 1 so stamps are never 0
}

ThreadContext& checkNoLeafHeld() {
	ThreadContext& context = ThreadContext::current();
	if (context.heldLeafCount != 0) [[unlikely]]
		throw IllegalStateException("Acquiring a StampedLock while holding a leaf mutex (runtime bug, design §4.1)");
	return context;
}

} // namespace

const LockClass& StampedLock::effectiveClass() const noexcept {
	if (lockClass_ != nullptr)
		return *lockClass_;
	static const LockClass& fallback = LockClass::named("StampedLock");
	return fallback;
}

int64_t StampedLock::tryWriteLockRaw() noexcept {
	uint64_t s = state_.load(std::memory_order_acquire);
	if ((s & WRITER) != 0 || (s & READERS) != 0)
		return 0;
	uint64_t next = ((version(s) + VERSION_UNIT) & VERSION_MASK) | WRITER;
	if ((next & VERSION_MASK) == 0)
		next |= VERSION_UNIT;
	if (!state_.compare_exchange_strong(s, next, std::memory_order_acquire))
		return 0;
	return static_cast<int64_t>(version(next)) | TAG_WRITE;
}

int64_t StampedLock::tryReadLockRaw() noexcept {
	uint64_t s = state_.load(std::memory_order_acquire);
	while ((s & WRITER) == 0 && (s & READERS) != READERS) {
		if (state_.compare_exchange_weak(s, s + 1, std::memory_order_acquire))
			return static_cast<int64_t>(version(s)) | TAG_READ;
	}
	return 0;
}

int64_t StampedLock::writeLock() {
	ThreadContext& context = checkNoLeafHeld();
	const LockClass& lockClass = effectiveClass();
	LockOrderValidator& validator = LockOrderValidator::getInstance();
	if (CHECKED)
		validator.beforeAcquire(lockClass, reinterpret_cast<uintptr_t>(this), false);
	bool waiting = false;
	int64_t stamp;
	for (;;) {
		AION_YIELD_POINT("StampedLock::writeLock");
		stamp = tryWriteLockRaw();
		if (stamp != 0)
			break;
		uint64_t s = state_.load(std::memory_order_acquire);
		if ((s & (WRITER | READERS)) != 0) {
			if (!waiting) {
				waiting = true;
				context.setWait(lockClass.name().c_str(), reinterpret_cast<uintptr_t>(this), 0, commons::utils::nanoTime());
			}
			waiters_.fetch_add(1, std::memory_order_acquire);
			AION_PCT_BLOCKING_BEGIN("StampedLock::writeLock");
			state_.wait(s, std::memory_order_acquire);
			AION_PCT_BLOCKING_END();
			waiters_.fetch_sub(1, std::memory_order_acquire);
		}
	}
	if (waiting)
		context.clearWait();
	validator.afterAcquire(lockClass, reinterpret_cast<uintptr_t>(this), 0, false);
	return stamp;
}

int64_t StampedLock::tryWriteLock() {
	checkNoLeafHeld();
	AION_YIELD_POINT("StampedLock::tryWriteLock");
	int64_t stamp = tryWriteLockRaw();
	if (stamp != 0)
		LockOrderValidator::getInstance().afterAcquire(effectiveClass(), reinterpret_cast<uintptr_t>(this), 0, false);
	return stamp;
}

int64_t StampedLock::readLock() {
	ThreadContext& context = checkNoLeafHeld();
	const LockClass& lockClass = effectiveClass();
	LockOrderValidator& validator = LockOrderValidator::getInstance();
	if (CHECKED)
		validator.beforeAcquire(lockClass, reinterpret_cast<uintptr_t>(this), true);
	bool waiting = false;
	int64_t stamp;
	for (;;) {
		AION_YIELD_POINT("StampedLock::readLock");
		stamp = tryReadLockRaw();
		if (stamp != 0)
			break;
		uint64_t s = state_.load(std::memory_order_acquire);
		if ((s & WRITER) != 0 || (s & READERS) == READERS) {
			if (!waiting) {
				waiting = true;
				context.setWait(lockClass.name().c_str(), reinterpret_cast<uintptr_t>(this), 0, commons::utils::nanoTime());
			}
			waiters_.fetch_add(1, std::memory_order_acquire);
			AION_PCT_BLOCKING_BEGIN("StampedLock::readLock");
			state_.wait(s, std::memory_order_acquire);
			AION_PCT_BLOCKING_END();
			waiters_.fetch_sub(1, std::memory_order_acquire);
		}
	}
	if (waiting)
		context.clearWait();
	validator.afterAcquire(lockClass, reinterpret_cast<uintptr_t>(this), 0, true);
	return stamp;
}

int64_t StampedLock::tryReadLock() {
	checkNoLeafHeld();
	AION_YIELD_POINT("StampedLock::tryReadLock");
	int64_t stamp = tryReadLockRaw();
	if (stamp != 0)
		LockOrderValidator::getInstance().afterAcquire(effectiveClass(), reinterpret_cast<uintptr_t>(this), 0, true);
	return stamp;
}

int64_t StampedLock::tryOptimisticRead() noexcept {
	AION_YIELD_POINT("StampedLock::tryOptimisticRead");
	uint64_t s = state_.load(std::memory_order_acquire);
	return (s & WRITER) != 0 ? 0 : static_cast<int64_t>(version(s)) | TAG_OPTIMISTIC;
}

bool StampedLock::validate(int64_t stamp) const noexcept {
	if (stamp == 0)
		return false;
	AION_YIELD_POINT("StampedLock::validate");
	uint64_t s = state_.load(std::memory_order_acquire);
	uint64_t stampVersion = static_cast<uint64_t>(stamp) & VERSION_MASK;
	if ((stamp & 3) == TAG_WRITE)
		return (s & WRITER) != 0 && version(s) == stampVersion;
	return (s & WRITER) == 0 && version(s) == stampVersion;
}

void StampedLock::unlockWrite(int64_t stamp) {
	uint64_t s = state_.load(std::memory_order_acquire);
	if ((stamp & 3) != TAG_WRITE || (s & WRITER) == 0 || version(s) != (static_cast<uint64_t>(stamp) & VERSION_MASK))
		throw IllegalMonitorStateException("StampedLock::unlockWrite with a stamp that does not match");
	LockOrderValidator::getInstance().afterRelease(reinterpret_cast<uintptr_t>(this));
	AION_YIELD_POINT("StampedLock::unlockWrite");
	// while write-locked nobody else modifies the word (reader and writer CASes fail on WRITER)
	state_.store(s & ~WRITER, std::memory_order_release);
	state_.notify_all();
}

void StampedLock::unlockRead(int64_t stamp) {
	if ((stamp & 3) != TAG_READ)
		throw IllegalMonitorStateException("StampedLock::unlockRead with a stamp that does not match");
	AION_YIELD_POINT("StampedLock::unlockRead");
	uint64_t s = state_.load(std::memory_order_acquire);
	for (;;) {
		if ((s & READERS) == 0 || (s & WRITER) != 0 || version(s) != (static_cast<uint64_t>(stamp) & VERSION_MASK))
			throw IllegalMonitorStateException("StampedLock::unlockRead with a stamp that does not match");
		if (state_.compare_exchange_weak(s, s - 1, std::memory_order_acquire))
			break;
	}
	LockOrderValidator::getInstance().afterRelease(reinterpret_cast<uintptr_t>(this));
	if (((s - 1) & READERS) == 0 && waiters_.load(std::memory_order_acquire) != 0)
		state_.notify_all();
}

void StampedLock::unlock(int64_t stamp) {
	if ((stamp & 3) == TAG_WRITE)
		unlockWrite(stamp);
	else
		unlockRead(stamp);
}

bool StampedLock::isWriteLocked() const noexcept {
	return (state_.load(std::memory_order_acquire) & WRITER) != 0;
}

bool StampedLock::isReadLocked() const noexcept {
	return (state_.load(std::memory_order_acquire) & READERS) != 0;
}

int32_t StampedLock::getReadLockCount() const noexcept {
	return static_cast<int32_t>(state_.load(std::memory_order_acquire) & READERS);
}

} // namespace aion::gameserver::runtime
