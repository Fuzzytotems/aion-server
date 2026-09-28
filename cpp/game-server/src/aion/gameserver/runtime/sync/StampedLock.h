#pragma once

#include <atomic>
#include <cstdint>

#include "aion/gameserver/runtime/sync/LockClass.h"

namespace aion::gameserver::runtime {

/**
 * Java: java.util.concurrent.locks.StampedLock (design §3.4). Used by EffectController (readLock/unlockRead, writeLock/unlockWrite).
 *
 * Semantics as Java: not reentrant (a thread taking writeLock while holding readLock of the same lock deadlocks, as in Java; checked builds
 * report it as a SAME_CLASS_NESTING "self-deadlock" warning before blocking), stamps are non-zero for successful acquisitions,
 * tryOptimisticRead returns 0 while write-locked, validate(stamp) is true if no write lock was acquired since the stamp was obtained.
 * Readers do not wait for queued writers (a thread may take readLock again while holding one, which EffectController's nested readers need);
 * a continuous stream of overlapping readers can therefore delay a writer. There are no conversions (tryConvertToWriteLock etc.): unused by
 * the Java server.
 *
 * It is a game-level lock (rank 0): acquisitions are validated by the LockOrderValidator (checked builds; lock class = the static class or
 * "StampedLock"), throw IllegalStateException when a leaf mutex is held (all builds), are recorded as held for the watchdog (all builds), record
 * watchdog wait records while blocked and have PCT yield points. Lock-order bookkeeping assumes the stamp is released on the acquiring thread
 * (a release on another thread leaves the acquirer's held-lock record behind; diagnostics only).
 * Thread-safety: all members are thread-safe. Unlocking with a stamp that does not match throws IllegalMonitorStateException (Java).
 */
class StampedLock {
public:
	StampedLock() noexcept = default;
	explicit StampedLock(const LockClass& lockClass) noexcept : lockClass_(&lockClass) {}
	StampedLock(const StampedLock&) = delete;
	StampedLock& operator=(const StampedLock&) = delete;

	/** Blocks until the exclusive lock is acquired. @return a non-zero stamp */
	int64_t writeLock();
	/** @return a stamp, or 0 if not immediately available */
	int64_t tryWriteLock();
	/** Blocks until a shared lock is acquired. @return a non-zero stamp */
	int64_t readLock();
	/** @return a stamp, or 0 if not immediately available */
	int64_t tryReadLock();
	/** @return a stamp for later validate(), or 0 if write-locked */
	int64_t tryOptimisticRead() noexcept;
	/** @return true if no write lock has been acquired since `stamp` was issued (false for 0) */
	bool validate(int64_t stamp) const noexcept;
	/** @throws IllegalMonitorStateException if the stamp does not match the current write lock */
	void unlockWrite(int64_t stamp);
	/** @throws IllegalMonitorStateException if the stamp does not match a held read lock */
	void unlockRead(int64_t stamp);
	/** Releases the lock corresponding to the stamp (read or write). */
	void unlock(int64_t stamp);

	bool isWriteLocked() const noexcept;
	bool isReadLocked() const noexcept;
	int32_t getReadLockCount() const noexcept;

	const LockClass* staticLockClass() const noexcept { return lockClass_; }

private:
	const LockClass& effectiveClass() const noexcept;
	int64_t tryWriteLockRaw() noexcept;
	int64_t tryReadLockRaw() noexcept;

	/** bits 0..31 reader count, bits 32..61 version (incremented by every write lock), bit 62 writer; waited with std::atomic::wait */
	std::atomic<uint64_t> state_{0};
	std::atomic<uint32_t> waiters_{0};
	const LockClass* lockClass_ = nullptr;
};

} // namespace aion::gameserver::runtime
