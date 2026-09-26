#pragma once

#include <atomic>

#include "aion/gameserver/runtime/base/Checked.h"

/**
 * Yield points for systematic concurrency testing (design §12.4: PCT kernel tests).
 *
 * Every atomic step of a kernel protocol (a load, store or CAS of shared state in retain/release, the Reclaimer scan, lazy epoch publication,
 * Field<Ref> exchange, Monitor acquisition, ConcurrentHashMap node table reads, Future state transitions...) is preceded by
 * AION_YIELD_POINT("site"). A test scheduler (runtime/lifetime/Pct.h) installs PctHooks and serializes the participating threads, choosing
 * at each yield point which thread continues. Without installed hooks a yield point costs one predicted branch on an atomic load.
 *
 * Code that is about to block (Monitor/RankedMutex slow path, Semaphore, Future::get, condition waits) must bracket the blocking wait with
 * AION_PCT_BLOCKING_BEGIN/END, so that the scheduler can run another thread instead of deadlocking.
 *
 * AION_PCT defaults to AION_CHECKED; define AION_PCT=0/1 explicitly to override. With AION_PCT=0 all macros expand to nothing.
 */
#if !defined(AION_PCT)
#define AION_PCT AION_CHECKED
#endif

namespace aion::gameserver::runtime::pct {

/** Callbacks of the active PCT scheduler. Every function must be thread-safe and must not throw. */
struct PctHooks {
	/** a controlled thread reached a yield point; the scheduler may suspend it here */
	void (*yield)(const char* site) noexcept = nullptr;
	/** the calling thread is about to block in the OS (it releases its turn) */
	void (*beforeBlocking)(const char* site) noexcept = nullptr;
	/** the calling thread returned from a blocking wait (it waits for its next turn) */
	void (*afterBlocking)() noexcept = nullptr;
};

/** Pointer to the installed hooks or nullptr. Written by the PCT scheduler only (installHooks), read lock-free by yield points. */
extern std::atomic<const PctHooks*> activeHooks;

/** Installs hooks (nullptr uninstalls). The hooks object must outlive every thread that might still hit a yield point. */
inline void installHooks(const PctHooks* hooks) noexcept {
	activeHooks.store(hooks, std::memory_order_release);
}

inline void yieldPoint(const char* site) noexcept {
	if (const PctHooks* hooks = activeHooks.load(std::memory_order_relaxed); hooks != nullptr && hooks->yield != nullptr) [[unlikely]]
		hooks->yield(site);
}

inline void blockingBegin(const char* site) noexcept {
	if (const PctHooks* hooks = activeHooks.load(std::memory_order_relaxed); hooks != nullptr && hooks->beforeBlocking != nullptr) [[unlikely]]
		hooks->beforeBlocking(site);
}

inline void blockingEnd() noexcept {
	if (const PctHooks* hooks = activeHooks.load(std::memory_order_relaxed); hooks != nullptr && hooks->afterBlocking != nullptr) [[unlikely]]
		hooks->afterBlocking();
}

} // namespace aion::gameserver::runtime::pct

#if AION_PCT
#define AION_YIELD_POINT(site) ::aion::gameserver::runtime::pct::yieldPoint(site)
#define AION_PCT_BLOCKING_BEGIN(site) ::aion::gameserver::runtime::pct::blockingBegin(site)
#define AION_PCT_BLOCKING_END() ::aion::gameserver::runtime::pct::blockingEnd()
#else
#define AION_YIELD_POINT(site) ((void)0)
#define AION_PCT_BLOCKING_BEGIN(site) ((void)0)
#define AION_PCT_BLOCKING_END() ((void)0)
#endif
