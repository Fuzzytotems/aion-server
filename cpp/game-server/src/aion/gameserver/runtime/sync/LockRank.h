#pragma once

#include <cstdint>
#include <string_view>

namespace aion::gameserver::runtime {

/**
 * Ranks of leaf mutexes (design §4.1, family 2). Game-level Monitors are rank 0 and are not listed here.
 *
 * Rules, enforced in checked AND release builds (C6, C14) by RankedMutex:
 * - acquiring a leaf mutex of rank r requires every leaf mutex held by the thread to have a rank < r;
 * - acquiring any game-level lock (Monitor, collection or stripe Monitor, StampedLock, Semaphore) while a leaf mutex is held throws
 *   IllegalStateException.
 * Both can only fire on a runtime bug: leaf mutexes are internal to runtime/ and network/, never run user callbacks, never take a Monitor
 * and never block on IO (lint L18).
 */
enum class LockRank : uint8_t {
	/** ConcurrentHashMap node-table resizing (writers already hold the stripe Monitor) and the ZoneName intern table */
	CONTAINER_SLOT = 10,
	/** AionConnection send queue */
	CONNECTION_QUEUE = 20,
	/** ScheduledPool heap, pool queues, pin index */
	SCHEDULER = 30,
	/** IDFactory bit set and release FIFO */
	IDFACTORY = 40,
	/** Reclaimer retire queues */
	RECLAIMER = 50,
	/** statistics tables (RunnableStatsManager, reclaimer stats, lockdep graph) */
	STATS = 60,
	/** logging sinks */
	LOGGING = 90,
};

constexpr std::string_view lockRankName(LockRank rank) noexcept {
	switch (rank) {
		case LockRank::CONTAINER_SLOT:
			return "CONTAINER_SLOT";
		case LockRank::CONNECTION_QUEUE:
			return "CONNECTION_QUEUE";
		case LockRank::SCHEDULER:
			return "SCHEDULER";
		case LockRank::IDFACTORY:
			return "IDFACTORY";
		case LockRank::RECLAIMER:
			return "RECLAIMER";
		case LockRank::STATS:
			return "STATS";
		case LockRank::LOGGING:
			return "LOGGING";
	}
	return "?";
}

} // namespace aion::gameserver::runtime
