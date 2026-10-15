#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

#include "aion/gameserver/runtime/sync/LockClass.h"

namespace aion::gameserver::runtime {

/**
 * Java: java.util.concurrent.Semaphore (design §3.4). Used by AbstractCronTask (acquireUninterruptibly/release around a cron body).
 *
 * Semantics as Java: permits may go above the initial count through release() and start negative; acquire blocks until enough permits are
 * available; there are no interrupts, so acquire() and acquireUninterruptibly() are equivalent. drainPermits() returns the available
 * permits (a negative count is returned and reset to 0, as in JDK 9+).
 * Fairness: a non-fair semaphore lets any acquirer take available permits. A fair semaphore does not let new blocking acquirers
 * (acquire, tryAcquire(timeout)) take permits while other threads wait; the waiters are then served in the OS wake order (approximately
 * FIFO), not in strict arrival order. The untimed tryAcquire() barges on fair semaphores too (Java).
 *
 * Every blocking wait runs inside a BlockingRegion("Semaphore.acquire") (design §1.2), records a watchdog wait record (lock class = the static
 * class or "Semaphore", no owner) and brackets the OS wait with AION_PCT_BLOCKING_BEGIN/END. A permit is not a lock: holding one records no
 * lock-order edges, but blocking in acquire while holding a Monitor is counted as BLOCKING_UNDER_MONITOR, and acquiring while a leaf mutex is
 * held throws IllegalStateException (all builds).
 * Thread-safety: all members are thread-safe.
 */
class Semaphore {
public:
	explicit Semaphore(int32_t permits, bool fair = false) noexcept;
	Semaphore(const LockClass& lockClass, int32_t permits, bool fair = false) noexcept;
	Semaphore(const Semaphore&) = delete;
	Semaphore& operator=(const Semaphore&) = delete;

	/** Blocks until a permit is available. @throws IllegalArgumentException if permits < 0 (for the multi-permit overload) */
	void acquire();
	void acquire(int32_t permits);
	void acquireUninterruptibly();
	void acquireUninterruptibly(int32_t permits);
	bool tryAcquire();
	bool tryAcquire(int32_t permits);
	bool tryAcquire(std::chrono::nanoseconds timeout);
	bool tryAcquire(int32_t permits, std::chrono::nanoseconds timeout);
	void release();
	void release(int32_t permits);
	int32_t availablePermits() const noexcept;
	/** Acquires and returns all immediately available permits. */
	int32_t drainPermits() noexcept;
	bool isFair() const noexcept { return fair_; }

private:
	bool tryTake(int32_t permits) noexcept;
	bool acquireBlocking(int32_t permits, int64_t deadlineNanos);

	/** the signed permit count stored as its two's complement bits (futex word) */
	std::atomic<uint32_t> permits_;
	std::atomic<uint32_t> waiters_{0};
	const bool fair_;
	const LockClass* lockClass_ = nullptr;
};

} // namespace aion::gameserver::runtime
