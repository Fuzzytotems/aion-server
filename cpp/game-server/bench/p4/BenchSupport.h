#pragma once

// =====================================================================================================================================
// PROTOTYPE BENCHMARK CODE (design runtime-architecture.md §19 P4). Not part of the game server; never include from src/.
// =====================================================================================================================================
//
// Measurement support for the P4 movement / known-list microbenchmark:
// - Allocation counting: this executable replaces the global operator new/delete (BenchSupport.cpp). Every allocation is counted in a
//   per-thread slot (single writer, no lock prefix on the hot path), with the usable size reported by the C runtime (_msize /
//   malloc_usable_size), so live bytes are exact at quiescence. Disabled in ASan builds (ASan owns the allocator).
// - Event counters: model code counts events (packets serialized, pair adds, ...) in the same per-thread slots, so counting does not add
//   cache-line contention to the measured hot paths.
// - Site counting (checked builds only, AION_PCT=1): installs PCT hooks (runtime/base/YieldPoint.h) that count every kernel yield point by
//   site (refcount traffic: "RefCounted::retain", "RefCounted::release:cas"; Monitor/LeafMutex acquisitions; read barriers) and every
//   blocking wait by the lock class recorded in the thread's wait record (contention per shim). Hooks slow the process down, so ticks
//   measured while counting are reported separately and excluded from the latency statistics.
// Thread-safety: all functions are thread-safe. Totals are racy snapshots while threads run and exact once they are quiescent.

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace aion::gameserver::bench {

/** Events counted by the prototype model (per-thread counters, summed by eventTotal). */
enum class Event : uint32_t {
	PACKETS_SERIALIZED,
	PACKET_BYTES_SERIALIZED,
	PACKETS_ENQUEUED,
	PAIR_ADD_REGION_SCAN,        // pattern 1: KnownList.findVisibleObjects region scan (KnownList.java:175-178)
	PAIR_ADD_PLAYER_FLAGS,       // pattern 2: player update adds flag npcs of the instance (KnownList.java:192-193)
	PAIR_ADD_FLAG_UPDATE,        // pattern 3: FlagKnownList.update adds players of the instance (FlagKnownList.java:18-21)
	PAIR_ADD_HANDSHAKE_UNDONE,   // addPair found an unspawned side after adding and removed both entries (design §5.3)
	KNOWN_FORGET,
	MOVE_STEPS,
	WALKER_READDS,
	MOVER_DESPAWNED_IN_TICK,
	OBSERVER_FASTPATH_EMPTY,
	OBSERVER_NOTIFIED,
	REGION_CHANGES,
	REGION_ACTIVATIONS,
	KNOWNLIST_UPDATES,
	SPAWNS,
	DESPAWNS,
	SHELLS_CREATED,
	SHELLS_DESTROYED,
	KNOWN_OBJECTS_CREATED,
	KNOWN_OBJECTS_DESTROYED,
	TASK_EXCEPTIONS,
	// phase profiler (--profile): inclusive nanoseconds and calls per phase, in Phase order
	PHASE_MOVE_ELEMENT_NANOS,
	PHASE_UPDATE_POSITION_NANOS,
	PHASE_REGION_LOOKUP_NANOS,
	PHASE_BROADCAST_NANOS,
	PHASE_BROADCAST_ITERATE_NANOS,
	PHASE_SERIALIZE_NANOS,
	PHASE_ENQUEUE_NANOS,
	PHASE_OBSERVERS_NANOS,
	PHASE_KNOWNLIST_UPDATE_NANOS,
	PHASE_SEE_PACKET_NANOS,
	PHASE_KNOWN_FORGET_NANOS,
	PHASE_REGION_SCAN_NANOS,
	PHASE_PLAYER_FLAG_SCAN_NANOS,
	PHASE_ADD_PAIR_NANOS,
	PHASE_MOVE_ELEMENT_CALLS,
	PHASE_UPDATE_POSITION_CALLS,
	PHASE_REGION_LOOKUP_CALLS,
	PHASE_BROADCAST_CALLS,
	PHASE_BROADCAST_ITERATE_CALLS,
	PHASE_SERIALIZE_CALLS,
	PHASE_ENQUEUE_CALLS,
	PHASE_OBSERVERS_CALLS,
	PHASE_KNOWNLIST_UPDATE_CALLS,
	PHASE_SEE_PACKET_CALLS,
	PHASE_KNOWN_FORGET_CALLS,
	PHASE_REGION_SCAN_CALLS,
	PHASE_PLAYER_FLAG_SCAN_CALLS,
	PHASE_ADD_PAIR_CALLS,
	COUNT
};

/** Phases of the opt-in phase profiler; each maps to a *_NANOS and a *_CALLS event. */
enum class Phase : uint32_t {
	MOVE_ELEMENT,
	UPDATE_POSITION,
	REGION_LOOKUP,
	BROADCAST,
	BROADCAST_ITERATE,
	SERIALIZE,
	ENQUEUE,
	OBSERVERS,
	KNOWNLIST_UPDATE,
	SEE_PACKET,
	KNOWN_FORGET,
	REGION_SCAN,
	PLAYER_FLAG_SCAN,
	ADD_PAIR,
	COUNT
};
const char* phaseName(Phase phase) noexcept;
inline constexpr uint32_t PHASE_NANOS_BASE = static_cast<uint32_t>(Event::PHASE_MOVE_ELEMENT_NANOS);
inline constexpr uint32_t PHASE_CALLS_BASE = static_cast<uint32_t>(Event::PHASE_MOVE_ELEMENT_CALLS);

/** set once before the scenario starts (--profile); timers cost one predicted branch when disabled */
extern bool phaseProfiling;
const char* eventName(Event event) noexcept;

void countEvent(Event event, uint64_t amount = 1) noexcept;
uint64_t eventTotal(Event event) noexcept;

struct AllocationTotals {
	uint64_t allocations = 0;
	uint64_t bytesAllocated = 0;
	uint64_t frees = 0;
	uint64_t bytesFreed = 0;
	int64_t liveBytes() const noexcept { return static_cast<int64_t>(bytesAllocated) - static_cast<int64_t>(bytesFreed); }
};
/** false in ASan builds (the global operator new is not replaced) */
bool allocationCountingEnabled() noexcept;
AllocationTotals allocationTotals() noexcept;

struct SiteCount {
	std::string site;
	uint64_t count = 0;
};

/**
 * Kernel yield-point and blocking-wait counting through the PCT hooks (checked builds). begin() resets the counters and installs the hooks,
 * end() uninstalls them. Must not be used while a PCT scheduler is active.
 */
class SiteCounting {
public:
	/** true if kernel yield points are compiled in (AION_PCT) */
	static bool available() noexcept;
	static void begin() noexcept;
	static void end() noexcept;
	/** yield points by site name, descending */
	static std::vector<SiteCount> yieldSites();
	/** blocking waits by lock class name (or blocking site when the thread has no lock wait record), descending */
	static std::vector<SiteCount> blockingWaits();
	/** entries that did not fit into a thread's fixed site table (should be 0) */
	static uint64_t droppedSites() noexcept;
};

/** Sorted-sample latency statistics (nanoseconds in, milliseconds out). Not thread-safe. */
class LatencyStats {
public:
	void add(int64_t nanos) { samples_.push_back(nanos); }
	size_t count() const noexcept { return samples_.size(); }
	/** p in [0, 100]; 0 if empty (nearest-rank) */
	double percentileMillis(double p) const;
	double maxMillis() const;
	double meanMillis() const;

private:
	std::vector<int64_t> samples_;
};

/** Lock-free histogram with 1 ms buckets up to 10 s (noexcept, usable from release-only destructors). */
class MillisHistogram {
public:
	static constexpr size_t BUCKETS = 10'001;
	void add(int64_t nanos) noexcept;
	uint64_t count() const noexcept;
	/** upper bound (ms) of the bucket containing the p-th percentile; 0 if empty */
	int64_t percentileMillis(double p) const noexcept;
	int64_t maxMillis() const noexcept;
	void reset() noexcept;

private:
	std::array<std::atomic<uint64_t>, BUCKETS> buckets_{};
};

/** monotonic nanoseconds */
int64_t nowNanos() noexcept;

/** RAII inclusive phase timer (no-op unless phaseProfiling). Nested phases overlap: times are inclusive. */
class PhaseTimer {
public:
	explicit PhaseTimer(Phase phase) noexcept : phase_(phase), start_(phaseProfiling ? nowNanos() : 0) {}
	~PhaseTimer() {
		if (start_ != 0) {
			countEvent(static_cast<Event>(PHASE_NANOS_BASE + static_cast<uint32_t>(phase_)), static_cast<uint64_t>(nowNanos() - start_));
			countEvent(static_cast<Event>(PHASE_CALLS_BASE + static_cast<uint32_t>(phase_)));
		}
	}
	PhaseTimer(const PhaseTimer&) = delete;
	PhaseTimer& operator=(const PhaseTimer&) = delete;

private:
	Phase phase_;
	int64_t start_;
};

} // namespace aion::gameserver::bench
