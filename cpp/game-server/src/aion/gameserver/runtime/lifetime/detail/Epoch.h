#pragma once

#include <atomic>
#include <cstdint>

// Internal to the lifetime core: the global epoch E (design §2.4) and the per-thread retire list. Not part of the public API; tests of the
// lifetime core may use the `testing` helpers.

namespace aion::gameserver::runtime::detail {

/** Global epoch E; starts at 1 so that retireEpoch 0 means "never stamped". Advanced only by Reclaimer scans (and testing::advanceEpoch). */
extern std::atomic<uint64_t> globalEpoch;

/**
 * Pushes the calling thread's retire list (objects, parts and nodes retired since the last flush) to the Reclaimer's lock-free incoming stack.
 * Called at outermost TaskScope exit, by quiescentPoint(), when the list is full, by retire*() outside any TaskScope, at the start and end of
 * every scan, and at thread exit. noexcept; cheap when the list is empty.
 */
void flushThreadRetireList() noexcept;

/** Number of entries in the calling thread's retire list that were not flushed yet (tests). */
size_t unflushedRetireCount() noexcept;

namespace testing {

/** Advances E by one without scanning (legal at any time: E is only required to be monotone). Directed interleaving tests use it. */
void advanceEpoch() noexcept;

/** Number of C16 warnings logged by quiescentPoint() no-op calls so far (once per call site). */
uint64_t quiescentNoOpWarnings() noexcept;

} // namespace testing

} // namespace aion::gameserver::runtime::detail
