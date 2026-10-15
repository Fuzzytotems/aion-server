#pragma once

#include <atomic>
#include <cstdint>
#include <type_traits>

namespace aion::gameserver::runtime::detail {

/**
 * Minimal futex (address wait) used by the game-level locks of the sync layer (Monitor, LeafMutex, Semaphore). std::atomic::wait has no
 * timeout, but Monitor::tryLock(timeout) and Semaphore::tryAcquire(timeout) need one without spinning.
 *
 * - Windows: WaitOnAddress / WakeByAddressSingle / WakeByAddressAll (Synchronization.lib, linked by Futex.cpp).
 * - Linux: the futex syscall (FUTEX_WAIT_PRIVATE / FUTEX_WAKE_PRIVATE).
 * - Other platforms: std::atomic::wait/notify for untimed waits, sleep polling (at most 1 ms per round) for timed waits.
 *
 * futexWait blocks only if `word` still holds `expected` when the kernel registers the waiter (no lost wakeups for writers that change the
 * word before waking). Spurious wakeups are possible: callers re-check their condition in a loop. The functions do not call the PCT hooks;
 * callers bracket them with AION_PCT_BLOCKING_BEGIN/END.
 */

/** Waits while `word == expected`. `timeoutNanos < 0` waits without a timeout. @return false if the timeout elapsed, true otherwise */
bool futexWait(std::atomic<uint32_t>& word, uint32_t expected, int64_t timeoutNanos = -1) noexcept;
/** Wakes at most one thread waiting on `word`. */
void futexWakeOne(std::atomic<uint32_t>& word) noexcept;
/** Wakes every thread waiting on `word`. */
void futexWakeAll(std::atomic<uint32_t>& word) noexcept;

} // namespace aion::gameserver::runtime::detail
