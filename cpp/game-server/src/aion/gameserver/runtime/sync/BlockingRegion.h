#pragma once

#include <source_location>

namespace aion::gameserver::runtime {

/**
 * RAII around a blocking wait (design §1.2, §2.6): Future::get, Semaphore::acquire, DB getConnection/execute, NioServer::openSocket,
 * sleeping helpers.
 *
 * - Records `what` and the call site in the thread's ThreadContext (outermost region wins when nested) for the watchdog: stall dumps and
 *   reclamation-lag dumps name the current BlockingRegion (design §2.6, §4.3, §7.8).
 * - Checked builds: if the thread holds any game-level lock, the LockOrderValidator counts a "blocking under a Monitor" warning with the
 *   site (design §4.2, `//debug locks`).
 * - Does NOT unpublish the thread's epoch: borrows stay valid across the wait (design §1.2).
 * - Does not itself notify the PCT scheduler; the primitive that actually blocks brackets the OS wait with AION_PCT_BLOCKING_BEGIN/END.
 *
 * `what` must have static storage duration (string literal). Usable on any thread (registers the ThreadContext lazily). Must be destroyed on
 * the thread that created it.
 */
class BlockingRegion {
public:
	explicit BlockingRegion(const char* what, std::source_location where = std::source_location::current()) noexcept;
	~BlockingRegion();
	BlockingRegion(const BlockingRegion&) = delete;
	BlockingRegion& operator=(const BlockingRegion&) = delete;
};

} // namespace aion::gameserver::runtime
