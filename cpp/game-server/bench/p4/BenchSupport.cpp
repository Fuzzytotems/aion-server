// PROTOTYPE BENCHMARK CODE (design §19 P4). See BenchSupport.h.

#include "p4/BenchSupport.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>

#if defined(_WIN32)
#include <malloc.h>
#elif defined(__linux__)
#include <malloc.h>
#endif

#include "aion/gameserver/runtime/base/ThreadContext.h"
#include "aion/gameserver/runtime/base/YieldPoint.h"

namespace aion::gameserver::bench {

namespace {

constexpr size_t MAX_SLOTS = 512;
constexpr size_t SITE_TABLE = 128;

/** Counters of one thread. Every field is written by its owning thread only (relaxed load + store); the overflow slot uses fetch_add. */
struct ThreadSlot {
	std::atomic<uint64_t> allocations{0};
	std::atomic<uint64_t> bytesAllocated{0};
	std::atomic<uint64_t> frees{0};
	std::atomic<uint64_t> bytesFreed{0};
	std::array<std::atomic<uint64_t>, static_cast<size_t>(Event::COUNT)> events{};
	std::array<std::atomic<const char*>, SITE_TABLE> siteKeys{};
	std::array<std::atomic<uint64_t>, SITE_TABLE> siteCounts{};
	std::array<std::atomic<const char*>, SITE_TABLE> waitKeys{};
	std::array<std::atomic<uint64_t>, SITE_TABLE> waitCounts{};
	std::atomic<uint64_t> droppedSites{0};
	bool shared = false;
};

// Plain static array: zero-initialized, never destroyed before other static destructors that might still allocate (atomics have trivial
// destructors), usable from operator new before main.
ThreadSlot slots[MAX_SLOTS];
std::atomic<size_t> slotsUsed{0};
thread_local ThreadSlot* currentSlot = nullptr;

ThreadSlot& slot() noexcept {
	ThreadSlot* s = currentSlot;
	if (s == nullptr) [[unlikely]] {
		size_t index = slotsUsed.fetch_add(1, std::memory_order_acq_rel);
		if (index >= MAX_SLOTS - 1) {
			slotsUsed.store(MAX_SLOTS, std::memory_order_release);
			s = &slots[MAX_SLOTS - 1];
			s->shared = true;
		} else {
			s = &slots[index];
		}
		currentSlot = s;
	}
	return *s;
}

inline void bump(const ThreadSlot& owner, std::atomic<uint64_t>& counter, uint64_t amount) noexcept {
	if (owner.shared) [[unlikely]]
		counter.fetch_add(amount, std::memory_order_relaxed);
	else
		counter.store(counter.load(std::memory_order_relaxed) + amount, std::memory_order_relaxed);
}

size_t slotCount() noexcept {
	return std::min(slotsUsed.load(std::memory_order_acquire), MAX_SLOTS);
}

// ----------------------------------------------------------------------------------------------------------------------- site counting

std::atomic<bool> siteCountingActive{false};

void countKey(ThreadSlot& s, std::array<std::atomic<const char*>, SITE_TABLE>& keys, std::array<std::atomic<uint64_t>, SITE_TABLE>& counts,
	const char* key) noexcept {
	size_t hash = (reinterpret_cast<size_t>(key) >> 3) * 0x9E3779B97F4A7C15ULL;
	for (size_t probe = 0; probe < SITE_TABLE; ++probe) {
		size_t i = (hash + probe) & (SITE_TABLE - 1);
		const char* current = keys[i].load(std::memory_order_relaxed);
		if (current == key) {
			bump(s, counts[i], 1);
			return;
		}
		if (current == nullptr) {
			if (s.shared) {
				const char* expected = nullptr;
				if (!keys[i].compare_exchange_strong(expected, key) && expected != key)
					continue;
			} else {
				keys[i].store(key, std::memory_order_relaxed);
			}
			bump(s, counts[i], 1);
			return;
		}
	}
	bump(s, s.droppedSites, 1);
}

void onYield(const char* site) noexcept {
	if (!siteCountingActive.load(std::memory_order_relaxed) || site == nullptr)
		return;
	ThreadSlot& s = slot();
	countKey(s, s.siteKeys, s.siteCounts, site);
}

void onBeforeBlocking(const char* site) noexcept {
	if (!siteCountingActive.load(std::memory_order_relaxed))
		return;
	const char* key = site != nullptr ? site : "(unknown)";
	if (runtime::ThreadContext* context = runtime::ThreadContext::currentIfRegistered()) {
		runtime::ThreadContext::WaitSnapshot wait = context->wait();
		if (wait.waiting && wait.lockClassName != nullptr)
			key = wait.lockClassName;
	}
	ThreadSlot& s = slot();
	countKey(s, s.waitKeys, s.waitCounts, key);
}

void onAfterBlocking() noexcept {}

const runtime::pct::PctHooks countingHooks{&onYield, &onBeforeBlocking, &onAfterBlocking};

template <size_t N>
std::vector<SiteCount> aggregate(std::array<std::atomic<const char*>, N> ThreadSlot::*keys, std::array<std::atomic<uint64_t>, N> ThreadSlot::*counts) {
	std::map<std::string, uint64_t> byName; // different string literal addresses of one site name are merged
	for (size_t i = 0; i < slotCount(); ++i) {
		ThreadSlot& s = slots[i];
		for (size_t j = 0; j < N; ++j) {
			const char* key = (s.*keys)[j].load(std::memory_order_acquire);
			uint64_t count = (s.*counts)[j].load(std::memory_order_acquire);
			if (key != nullptr && count != 0)
				byName[key] += count;
		}
	}
	std::vector<SiteCount> result;
	for (auto& [name, count] : byName)
		result.push_back({name, count});
	std::ranges::sort(result, [](const SiteCount& a, const SiteCount& b) { return a.count > b.count; });
	return result;
}

// ----------------------------------------------------------------------------------------------------------------------- allocation

#if !defined(AION_ASAN)
constexpr bool COUNT_ALLOCATIONS = true;

size_t usableSize(void* p) noexcept {
#if defined(_WIN32)
	return _msize(p);
#elif defined(__linux__)
	return malloc_usable_size(p);
#else
	(void)p;
	return 0;
#endif
}

size_t usableSizeAligned(void* p, size_t alignment) noexcept {
#if defined(_WIN32)
	return _aligned_msize(p, alignment, 0);
#elif defined(__linux__)
	(void)alignment;
	return malloc_usable_size(p);
#else
	(void)p;
	(void)alignment;
	return 0;
#endif
}

void recordAllocation(size_t bytes) noexcept {
	ThreadSlot& s = slot();
	bump(s, s.allocations, 1);
	bump(s, s.bytesAllocated, bytes);
}

void recordFree(size_t bytes) noexcept {
	ThreadSlot& s = slot();
	bump(s, s.frees, 1);
	bump(s, s.bytesFreed, bytes);
}
#else
constexpr bool COUNT_ALLOCATIONS = false;
#endif

} // namespace

#if !defined(AION_ASAN)
namespace detail {

void* countedAllocate(size_t size) noexcept {
	void* p = std::malloc(size == 0 ? 1 : size);
	if (p != nullptr)
		recordAllocation(usableSize(p));
	return p;
}

void countedFree(void* p) noexcept {
	if (p == nullptr)
		return;
	recordFree(usableSize(p));
	std::free(p);
}

void* countedAllocateAligned(size_t size, size_t alignment) noexcept {
	if (size == 0)
		size = 1;
#if defined(_WIN32)
	void* p = _aligned_malloc(size, alignment);
#else
	void* p = std::aligned_alloc(alignment, (size + alignment - 1) / alignment * alignment);
#endif
	if (p != nullptr)
		recordAllocation(usableSizeAligned(p, alignment));
	return p;
}

void countedFreeAligned(void* p, size_t alignment) noexcept {
	if (p == nullptr)
		return;
	recordFree(usableSizeAligned(p, alignment));
#if defined(_WIN32)
	_aligned_free(p);
#else
	std::free(p);
#endif
}

} // namespace detail
#endif

const char* eventName(Event event) noexcept {
	switch (event) {
		case Event::PACKETS_SERIALIZED: return "packets serialized";
		case Event::PACKET_BYTES_SERIALIZED: return "packet bytes serialized";
		case Event::PACKETS_ENQUEUED: return "packets enqueued";
		case Event::PAIR_ADD_REGION_SCAN: return "addPair pattern 1 (region scan)";
		case Event::PAIR_ADD_PLAYER_FLAGS: return "addPair pattern 2 (player -> flags)";
		case Event::PAIR_ADD_FLAG_UPDATE: return "addPair pattern 3 (flag -> players)";
		case Event::PAIR_ADD_HANDSHAKE_UNDONE: return "addPair undone by spawn handshake";
		case Event::KNOWN_FORGET: return "known pairs forgotten (out of range)";
		case Event::MOVE_STEPS: return "move steps";
		case Event::WALKER_READDS: return "walker re-adds";
		case Event::MOVER_DESPAWNED_IN_TICK: return "movers found despawned in tick";
		case Event::OBSERVER_FASTPATH_EMPTY: return "observer notifications (isEmpty fast path)";
		case Event::OBSERVER_NOTIFIED: return "observer notifications (delivered)";
		case Event::REGION_CHANGES: return "map region changes";
		case Event::REGION_ACTIVATIONS: return "map region activations";
		case Event::KNOWNLIST_UPDATES: return "known list updates";
		case Event::SPAWNS: return "spawns";
		case Event::DESPAWNS: return "despawns";
		case Event::SHELLS_CREATED: return "object shells created";
		case Event::SHELLS_DESTROYED: return "object shells destroyed";
		case Event::KNOWN_OBJECTS_CREATED: return "KnownObjects created";
		case Event::KNOWN_OBJECTS_DESTROYED: return "KnownObjects destroyed";
		case Event::TASK_EXCEPTIONS: return "task exceptions";
		default: break;
	}
	if (static_cast<uint32_t>(event) >= PHASE_NANOS_BASE && event != Event::COUNT)
		return "phase counter";
	return "?";
}

bool phaseProfiling = false;

const char* phaseName(Phase phase) noexcept {
	switch (phase) {
		case Phase::MOVE_ELEMENT: return "move element (whole parallelForEach body)";
		case Phase::UPDATE_POSITION: return "  World.updatePosition";
		case Phase::REGION_LOOKUP: return "    WorldMapInstance.getRegion (all callers)";
		case Phase::BROADCAST: return "  broadcastPacket(SM_MOVE)";
		case Phase::BROADCAST_ITERATE: return "    KnownList.forEachPlayer (iteration, instanceof, sends)";
		case Phase::SERIALIZE: return "    serialize (once per broadcast, all callers)";
		case Phase::ENQUEUE: return "    connection enqueue (seq-ordered, leaf mutex, all callers)";
		case Phase::OBSERVERS: return "  ObserveController.notifyMoveObservers";
		case Phase::KNOWNLIST_UPDATE: return "  KnownList.update (all callers)";
		case Phase::SEE_PACKET: return "  Player see/notSee packets (all callers)";
		case Phase::KNOWN_FORGET: return "    KnownList.forgetObjectsOrUpdateVisibility";
		case Phase::REGION_SCAN: return "    KnownList.findVisibleObjects region scan";
		case Phase::PLAYER_FLAG_SCAN: return "    KnownList.findVisibleObjects player flag scan (forEachNpc of the instance)";
		case Phase::ADD_PAIR: return "      KnownList.addPair (all callers)";
		case Phase::COUNT: break;
	}
	return "?";
}

void countEvent(Event event, uint64_t amount) noexcept {
	ThreadSlot& s = slot();
	bump(s, s.events[static_cast<size_t>(event)], amount);
}

uint64_t eventTotal(Event event) noexcept {
	uint64_t total = 0;
	for (size_t i = 0; i < slotCount(); ++i)
		total += slots[i].events[static_cast<size_t>(event)].load(std::memory_order_acquire);
	return total;
}

bool allocationCountingEnabled() noexcept {
	return COUNT_ALLOCATIONS;
}

AllocationTotals allocationTotals() noexcept {
	AllocationTotals totals;
	for (size_t i = 0; i < slotCount(); ++i) {
		totals.allocations += slots[i].allocations.load(std::memory_order_acquire);
		totals.bytesAllocated += slots[i].bytesAllocated.load(std::memory_order_acquire);
		totals.frees += slots[i].frees.load(std::memory_order_acquire);
		totals.bytesFreed += slots[i].bytesFreed.load(std::memory_order_acquire);
	}
	return totals;
}

bool SiteCounting::available() noexcept {
#if AION_PCT
	return true;
#else
	return false;
#endif
}

void SiteCounting::begin() noexcept {
	for (size_t i = 0; i < slotCount(); ++i) {
		for (auto& c : slots[i].siteCounts)
			c.store(0, std::memory_order_relaxed);
		for (auto& c : slots[i].waitCounts)
			c.store(0, std::memory_order_relaxed);
		slots[i].droppedSites.store(0, std::memory_order_relaxed);
	}
	siteCountingActive.store(true, std::memory_order_release);
	runtime::pct::installHooks(&countingHooks);
}

void SiteCounting::end() noexcept {
	siteCountingActive.store(false, std::memory_order_release);
	runtime::pct::installHooks(nullptr);
}

std::vector<SiteCount> SiteCounting::yieldSites() {
	return aggregate(&ThreadSlot::siteKeys, &ThreadSlot::siteCounts);
}

std::vector<SiteCount> SiteCounting::blockingWaits() {
	return aggregate(&ThreadSlot::waitKeys, &ThreadSlot::waitCounts);
}

uint64_t SiteCounting::droppedSites() noexcept {
	uint64_t total = 0;
	for (size_t i = 0; i < slotCount(); ++i)
		total += slots[i].droppedSites.load(std::memory_order_acquire);
	return total;
}

// ------------------------------------------------------------------------------------------------------------------------- statistics

double LatencyStats::percentileMillis(double p) const {
	if (samples_.empty())
		return 0;
	std::vector<int64_t> sorted = samples_;
	std::ranges::sort(sorted);
	size_t rank = static_cast<size_t>(std::ceil(p / 100.0 * static_cast<double>(sorted.size())));
	rank = std::clamp<size_t>(rank, 1, sorted.size());
	return static_cast<double>(sorted[rank - 1]) / 1e6;
}

double LatencyStats::maxMillis() const {
	return samples_.empty() ? 0 : static_cast<double>(*std::ranges::max_element(samples_)) / 1e6;
}

double LatencyStats::meanMillis() const {
	if (samples_.empty())
		return 0;
	long double sum = 0;
	for (int64_t s : samples_)
		sum += static_cast<long double>(s);
	return static_cast<double>(sum / static_cast<long double>(samples_.size()) / 1e6L);
}

void MillisHistogram::add(int64_t nanos) noexcept {
	int64_t millis = nanos <= 0 ? 0 : nanos / 1'000'000;
	size_t bucket = static_cast<size_t>(std::min<int64_t>(millis, static_cast<int64_t>(BUCKETS - 1)));
	buckets_[bucket].fetch_add(1, std::memory_order_relaxed);
}

uint64_t MillisHistogram::count() const noexcept {
	uint64_t total = 0;
	for (const auto& bucket : buckets_)
		total += bucket.load(std::memory_order_relaxed);
	return total;
}

int64_t MillisHistogram::percentileMillis(double p) const noexcept {
	uint64_t total = count();
	if (total == 0)
		return 0;
	uint64_t rank = static_cast<uint64_t>(std::ceil(p / 100.0 * static_cast<double>(total)));
	rank = std::max<uint64_t>(rank, 1);
	uint64_t seen = 0;
	for (size_t i = 0; i < BUCKETS; ++i) {
		seen += buckets_[i].load(std::memory_order_relaxed);
		if (seen >= rank)
			return static_cast<int64_t>(i) + 1;
	}
	return static_cast<int64_t>(BUCKETS);
}

int64_t MillisHistogram::maxMillis() const noexcept {
	for (size_t i = BUCKETS; i-- > 0;) {
		if (buckets_[i].load(std::memory_order_relaxed) != 0)
			return static_cast<int64_t>(i) + 1;
	}
	return 0;
}

void MillisHistogram::reset() noexcept {
	for (auto& bucket : buckets_)
		bucket.store(0, std::memory_order_relaxed);
}

int64_t nowNanos() noexcept {
	return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

} // namespace aion::gameserver::bench

// ----------------------------------------------------------------------------------------------------------------- global operator new
// Replaced for the whole benchmark executable (allocation counts and live bytes). Not in ASan builds.
#if !defined(AION_ASAN)

using aion::gameserver::bench::detail::countedAllocate;
using aion::gameserver::bench::detail::countedAllocateAligned;
using aion::gameserver::bench::detail::countedFree;
using aion::gameserver::bench::detail::countedFreeAligned;

void* operator new(std::size_t size) {
	if (void* p = countedAllocate(size))
		return p;
	throw std::bad_alloc();
}
void* operator new[](std::size_t size) {
	if (void* p = countedAllocate(size))
		return p;
	throw std::bad_alloc();
}
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
	return countedAllocate(size);
}
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
	return countedAllocate(size);
}
void* operator new(std::size_t size, std::align_val_t alignment) {
	if (void* p = countedAllocateAligned(size, static_cast<size_t>(alignment)))
		return p;
	throw std::bad_alloc();
}
void* operator new[](std::size_t size, std::align_val_t alignment) {
	if (void* p = countedAllocateAligned(size, static_cast<size_t>(alignment)))
		return p;
	throw std::bad_alloc();
}
void* operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept {
	return countedAllocateAligned(size, static_cast<size_t>(alignment));
}
void* operator new[](std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept {
	return countedAllocateAligned(size, static_cast<size_t>(alignment));
}

void operator delete(void* p) noexcept {
	countedFree(p);
}
void operator delete[](void* p) noexcept {
	countedFree(p);
}
void operator delete(void* p, std::size_t) noexcept {
	countedFree(p);
}
void operator delete[](void* p, std::size_t) noexcept {
	countedFree(p);
}
void operator delete(void* p, const std::nothrow_t&) noexcept {
	countedFree(p);
}
void operator delete[](void* p, const std::nothrow_t&) noexcept {
	countedFree(p);
}
void operator delete(void* p, std::align_val_t alignment) noexcept {
	countedFreeAligned(p, static_cast<size_t>(alignment));
}
void operator delete[](void* p, std::align_val_t alignment) noexcept {
	countedFreeAligned(p, static_cast<size_t>(alignment));
}
void operator delete(void* p, std::size_t, std::align_val_t alignment) noexcept {
	countedFreeAligned(p, static_cast<size_t>(alignment));
}
void operator delete[](void* p, std::size_t, std::align_val_t alignment) noexcept {
	countedFreeAligned(p, static_cast<size_t>(alignment));
}
void operator delete(void* p, std::align_val_t alignment, const std::nothrow_t&) noexcept {
	countedFreeAligned(p, static_cast<size_t>(alignment));
}
void operator delete[](void* p, std::align_val_t alignment, const std::nothrow_t&) noexcept {
	countedFreeAligned(p, static_cast<size_t>(alignment));
}

#endif
