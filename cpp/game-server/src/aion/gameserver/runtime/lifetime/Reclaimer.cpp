// The epoch Reclaimer: per-thread retire lists, the lock-free incoming stack, the epoch-bucketed limbo, the scan of design §2.4 (bounded and
// resumable on the Reclaimer thread), destruction with C3/C5/C8 checks, statistics, hooks and the Reclaimer thread. The release side of the
// protocol lives in RefCounted.cpp, publication in TaskScope.cpp.

#include "aion/gameserver/runtime/lifetime/Reclaimer.h"

#include <algorithm>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/commons/utils/concurrent/ThreadName.h"
#include "aion/gameserver/runtime/base/Checked.h"
#include "aion/gameserver/runtime/base/ThreadContext.h"
#include "aion/gameserver/runtime/base/YieldPoint.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/lifetime/detail/Epoch.h"
#include "aion/gameserver/runtime/lifetime/detail/Mutations.h"
#include "aion/gameserver/runtime/lifetime/detail/RefCountedAccess.h"
#include "aion/gameserver/runtime/sync/Watchdog.h"

#if !defined(AION_ASAN)
#define AION_ASAN 0
#endif

namespace aion::gameserver::runtime {

namespace detail {

struct OwnedPartAccess {
	static std::atomic<uint32_t>& refs(const OwnedPart& part) noexcept { return part.partRefs_; }
	static std::atomic<uint64_t>& stamp(const OwnedPart& part) noexcept { return part.partStamp_; }
};

} // namespace detail

namespace {

using detail::isMutated;
using detail::Mutation;
using detail::RefCountedAccess;

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.runtime.Reclaimer"));
	return *logger;
}

// ------------------------------------------------------------------------------------------------------------------------ retired entries

enum ItemKind : uintptr_t { OBJECT = 0, PART = 1, NODE = 2, COUNTED_PART = 3 };

/** One retired entry. The pointer's low two bits hold the kind (every retired type is polymorphic, so its alignment is at least 8). */
struct RetiredItem {
	uintptr_t tagged = 0;
	/** PART/COUNTED_PART: the retained owner */
	const RefCounted* owner = nullptr;
	/**
	 * PART/NODE: E at retire time. COUNTED_PART: E at retire time (also stamped into the part). OBJECT: the object's retireEpoch observed when
	 * the entry was made (a lower bound of its stamp, used only as the limbo key).
	 */
	uint64_t epoch = 0;
	uint64_t bytes = 0;

	ItemKind kind() const noexcept { return static_cast<ItemKind>(tagged & 3); }
	const RefCounted* object() const noexcept { return reinterpret_cast<const RefCounted*>(tagged & ~uintptr_t{3}); }
	/** PART: a part without its own reference count (derives OwnedPartBase only) */
	OwnedPartBase* part() const noexcept { return reinterpret_cast<OwnedPartBase*>(tagged & ~uintptr_t{3}); }
	/** COUNTED_PART: an OwnedPart, kept while Refs to the part exist */
	OwnedPart* countedPart() const noexcept { return reinterpret_cast<OwnedPart*>(tagged & ~uintptr_t{3}); }
	RetiredNode* node() const noexcept { return reinterpret_cast<RetiredNode*>(tagged & ~uintptr_t{3}); }
};

struct RetireBatch {
	RetireBatch* next = nullptr;
	std::vector<RetiredItem> items;
	uint64_t bytes = 0;
};

constexpr size_t FLUSH_THRESHOLD = 256;
/** epoch start times kept for Stats::lag: 65536 epochs, more than 20 minutes at the default period */
constexpr size_t EPOCH_START_RING = size_t{1} << 16;
/** entries classified before the chunk is destroyed (bounded scans check their budget after every chunk) */
constexpr size_t CHUNK = 64;
/** emptied limbo buckets kept for reuse (their capacity avoids an allocation per epoch) */
constexpr size_t MAX_SPARE_BUCKETS = 64;
constexpr size_t MAX_SPARE_BUCKET_CAPACITY = 16384;

/** Entries whose limbo key is the same epoch (see ReclaimerState::limbo). */
using Bucket = std::vector<RetiredItem>;

struct ScanBudget {
	uint64_t entries = UINT64_MAX;
	int64_t nanos = INT64_MAX;
};

/** An entry kept by the running scan: filed again after the visit, into the limbo under `key` or (held) at the end of the held-part queue. */
struct Refiled {
	uint64_t key;
	RetiredItem item;
	bool held;
};

struct ReclaimerState {
	// ------------------------------------------------------------------------------------------------ incoming (lock-free, any thread)
	std::atomic<RetireBatch*> incoming{nullptr};
	std::atomic<uint64_t> incomingCount{0};
	std::atomic<uint64_t> incomingBytes{0};

	// ------------------------------------------------------------------------------------------------ scanner (guarded by scanMutex)
	std::mutex scanMutex;
	/** the thread holding scanMutex (default id when none): hook/observer removal from that thread must not wait for itself */
	std::atomic<std::thread::id> scanningThread{};
	/**
	 * The limbo: every flushed entry, filed under a key that is a lower bound of the stamp its destruction rule compares with m (see
	 * Reclaimer.h). Stamps only grow, so a scan visits only the buckets whose key is below its m; the rest cannot be destroyed by it.
	 */
	std::map<uint64_t, Bucket> limbo;
	uint64_t limboCount = 0;
	uint64_t limboBytes = 0;
	std::vector<Bucket> spareBuckets;
	/**
	 * Retired OwnedParts found held by a Ref with a key below the finding scan's m (review fix: re-filed into the oldest bucket, they were taken
	 * first by every scan and starved bounded scans). They are visited round robin (taken from the front, kept ones appended after the visit),
	 * after the limbo, and first on every second scan with a share of the budget. Counted in limboCount.
	 */
	std::deque<RetiredItem> heldParts;
	uint64_t scanSequence = 0;
	/** entries kept by the running scan with their new key; filed after its visit loop (a scan never visits an entry twice) */
	std::vector<Refiled> refile;
	/** the chunk being destroyed (capacity CHUNK, never reallocated while entries are classified) */
	std::vector<RetiredItem> chunkObjects;
	std::vector<RetiredItem> chunkOthers;
#if AION_CHECKED
	/**
	 * Objects destroyed by the running scan (open addressing, 0 = free slot; grown between chunks, never while an entry is classified). Review
	 * fix: the C5 double-retire check covered one chunk; with it, a second entry of an object destroyed earlier in the same scan is detected
	 * before its memory is read, even if that memory was freed and reused.
	 */
	std::vector<uintptr_t> destroyedSet;
	size_t destroyedSetCount = 0;
#endif
	std::deque<std::pair<void*, size_t>> delayedFree;
	size_t delayedFreeUsed = 0;
	/** (epoch, nanoTime when E became it), appended by scans in increasing epoch order */
	std::vector<std::pair<uint64_t, int64_t>> epochStarts;
	size_t epochStartsNext = 0;
	size_t epochStartsSize = 0;

	// ------------------------------------------------------------------------------------------------ statistics
	std::atomic<uint64_t> scannerBacklog{0};
	std::atomic<uint64_t> scannerBacklogBytes{0};
	std::atomic<uint64_t> destroyed{0};
	std::atomic<uint64_t> scans{0};
	std::atomic<uint64_t> examinedTotal{0};
	std::atomic<uint64_t> budgetExhaustedScans{0};
	std::atomic<uint64_t> heldPartsCount{0};
	/** threads blocked on scanMutex (ScanLock, waitForRunningScan): a continued scan lets them in first (std::mutex is not fair) */
	std::atomic<uint32_t> scanWaiters{0};
	mutable std::mutex statsMutex; // leaf: no callbacks under it
	uint64_t minActive = 0;
	std::chrono::milliseconds lag{0};
	TaskInfo oldestTask{};
	uint64_t oldestThreadId = 0;
	uint64_t lastScanExamined = 0;
	uint64_t lastScanDestroyed = 0;
	std::chrono::microseconds lastScanDuration{0};

	// ------------------------------------------------------------------------------------------------ configuration and hooks
	mutable std::mutex configMutex; // leaf
	Reclaimer::Config config;
	std::atomic<uint64_t> wakeBacklog{8192};
	std::atomic<size_t> delayedFreeLimit{size_t{64} * 1024 * 1024};
	std::atomic<Reclaimer::DestroyObserver> destroyObserver{nullptr};
	std::mutex hooksMutex; // leaf: hooks are copied out before they run
	uint64_t nextHookId = 1;
	std::vector<std::pair<uint64_t, std::pair<std::string, std::shared_ptr<Reclaimer::ScanHook>>>> hooks;

	// ------------------------------------------------------------------------------------------------ thread
	std::mutex threadMutex;
	std::condition_variable wakeCondition;
	std::thread thread;
	std::atomic<bool> running{false};
	std::atomic<bool> stopRequested{false};
	std::atomic<bool> wakeRequested{false};
	uint64_t probeId = 0;
	uint64_t lagWarnedEpoch = 0;
	bool backlogDumped = false;

	ReclaimerState() {
		spareBuckets.reserve(MAX_SPARE_BUCKETS);
		refile.reserve(CHUNK);
		chunkObjects.reserve(CHUNK);
		chunkOthers.reserve(CHUNK);
		epochStarts.resize(EPOCH_START_RING);
		epochStarts[0] = {1, commons::utils::nanoTime()};
		epochStartsNext = 1;
		epochStartsSize = 1;
	}
};

ReclaimerState& state() {
	static auto* instance = new ReclaimerState(); // leaked: usable from thread exit and static destruction
	return *instance;
}

void wake(ReclaimerState& s) noexcept {
	if (s.running.load(std::memory_order_relaxed) && !s.wakeRequested.exchange(true, std::memory_order_acq_rel))
		s.wakeCondition.notify_one();
}

/**
 * Publishes a batch on the incoming stack. The counters are added BEFORE the batch becomes visible and the batch is never touched after the
 * successful CAS: from that moment a concurrent scan may take it, subtract its sizes and delete it (review fix: reading the sizes after the
 * CAS was a heap use-after-free and could wrap the backlog counters below zero).
 */
void pushBatch(RetireBatch* batch) noexcept {
	ReclaimerState& s = state();
	const uint64_t items = batch->items.size();
	const uint64_t bytes = batch->bytes;
	const uint64_t count = s.incomingCount.fetch_add(items, std::memory_order_acq_rel) + items;
	s.incomingBytes.fetch_add(bytes, std::memory_order_relaxed);
	AION_YIELD_POINT("Reclaimer::flush");
	RetireBatch* head = s.incoming.load(std::memory_order_acquire);
	do {
		batch->next = head;
	} while (!s.incoming.compare_exchange_weak(head, batch, std::memory_order_acq_rel));
	AION_YIELD_POINT("Reclaimer::flush:published"); // `batch` may already be deleted here
	if (count > s.wakeBacklog.load(std::memory_order_relaxed))
		wake(s);
}

// ------------------------------------------------------------------------------------------------------------------------ thread retire lists

struct ThreadRetireList {
	std::vector<RetiredItem> items;
	uint64_t bytes = 0;

	void flush() noexcept {
		if (items.empty())
			return;
		try {
			auto* batch = new RetireBatch();
			batch->items = std::move(items);
			batch->bytes = std::exchange(bytes, 0);
			items = std::vector<RetiredItem>();
			pushBatch(batch);
		} catch (...) {
			checkFailed("C5", "Reclaimer: out of memory while flushing a retire list");
		}
	}
};

// A trivially destructible thread_local pointer is safe to use at any point of thread exit; the owner flushes the list when the thread exits
// and later retires (thread_local destructors releasing Refs) push single-entry batches.
thread_local ThreadRetireList* threadRetireList = nullptr;
thread_local bool threadRetireListFinished = false;

struct ThreadRetireListOwner {
	void touch() noexcept {}
	~ThreadRetireListOwner() {
		if (threadRetireList != nullptr) {
			threadRetireList->flush();
			delete threadRetireList;
			threadRetireList = nullptr;
		}
		threadRetireListFinished = true;
	}
};
thread_local ThreadRetireListOwner threadRetireListOwner;

void pushItem(const RetiredItem& item) noexcept {
	try {
		// Outside a TaskScope nothing else would flush, so the entry is pushed immediately (this is also the path of thread_local destructors
		// at thread exit). Inside a scope or the destructor context (the scan flushes once at its end) entries are batched.
		ThreadContext* context = ThreadContext::currentIfRegistered();
		bool batched = context != nullptr && (context->scopeDepth > 0 || context->destructorContextDepth > 0);
		ThreadRetireList* list = threadRetireList;
		if (list == nullptr) [[unlikely]] {
			if (!batched || threadRetireListFinished) {
				auto* batch = new RetireBatch();
				batch->items.push_back(item);
				batch->bytes = item.bytes;
				pushBatch(batch);
				return;
			}
			threadRetireListOwner.touch();
			list = threadRetireList = new ThreadRetireList();
			list->items.reserve(64);
		}
		list->items.push_back(item);
		list->bytes += item.bytes;
		if (!batched || list->items.size() >= FLUSH_THRESHOLD)
			list->flush();
	} catch (...) {
		checkFailed("C5", "Reclaimer: out of memory while retiring");
	}
}

uint64_t objectBytes([[maybe_unused]] const RefCounted& object) noexcept {
#if AION_CHECKED
	auto* header = static_cast<const detail::ObjectHeader*>(dynamic_cast<const void*>(&object)) - 1;
	return header->magic == detail::OBJECT_HEADER_MAGIC ? header->size : sizeof(RefCounted);
#else
	return sizeof(RefCounted);
#endif
}

// ------------------------------------------------------------------------------------------------------------------------ destruction

struct DestructorContext {
	ThreadContext& context = ThreadContext::current();
	DestructorContext() noexcept { ++context.destructorContextDepth; }
	~DestructorContext() { --context.destructorContextDepth; }
	DestructorContext(const DestructorContext&) = delete;
	DestructorContext& operator=(const DestructorContext&) = delete;
};

constexpr uint64_t FREED_HEADER_MAGIC = 0xF4EE'D0B1'EC7F'4EED;
constexpr unsigned char POISON = 0xDD;

/** Frees (or poisons and delays) the memory of a destroyed object. Scanner only. */
void freeObject(ReclaimerState& s, void* memory, [[maybe_unused]] const char* cookieAddress) noexcept {
#if AION_CHECKED
	auto* header = static_cast<detail::ObjectHeader*>(memory) - 1;
	AION_CHECK_ALWAYS("C3", header->magic == detail::OBJECT_HEADER_MAGIC, "Reclaimer: corrupted allocation header of a destroyed object");
	size_t size = header->size;
	size_t limit = s.delayedFreeLimit.load(std::memory_order_relaxed);
	if (!AION_ASAN && limit > 0 && size <= limit) {
		std::memset(memory, POISON, size);
		const RefCounted::Cookie dead = RefCounted::Cookie::DEAD; // retain/release on a stale pointer still terminate with C4
		std::memcpy(const_cast<char*>(cookieAddress), &dead, sizeof(dead));
		header->magic = FREED_HEADER_MAGIC;
		try {
			s.delayedFree.emplace_back(header, size);
			s.delayedFreeUsed += size;
			while (s.delayedFreeUsed > limit) {
				auto [oldest, oldestSize] = s.delayedFree.front();
				s.delayedFree.pop_front();
				s.delayedFreeUsed -= oldestSize;
				::operator delete(oldest);
			}
			return;
		} catch (...) {
			// fall through: free immediately
		}
	}
	::operator delete(header);
#else
	::operator delete(memory);
#endif
}

void releaseDelayedFree(ReclaimerState& s, size_t limit) noexcept {
	while (s.delayedFreeUsed > limit && !s.delayedFree.empty()) {
		auto [oldest, oldestSize] = s.delayedFree.front();
		s.delayedFree.pop_front();
		s.delayedFreeUsed -= oldestSize;
		::operator delete(oldest);
	}
}

// ------------------------------------------------------------------------------------------------------------------------ limbo

/** The bucket of `key`, created (from a spare bucket if one is left) if missing. Allocates: never called while an entry is being classified. */
Bucket& bucketFor(ReclaimerState& s, uint64_t key) {
	auto [it, inserted] = s.limbo.try_emplace(key);
	if (inserted && !s.spareBuckets.empty()) {
		it->second.swap(s.spareBuckets.back());
		s.spareBuckets.pop_back();
	}
	return it->second;
}

/** Removes an empty bucket, keeping its vector for reuse. Does not allocate (spareBuckets has its capacity reserved). */
void eraseBucket(ReclaimerState& s, std::map<uint64_t, Bucket>::iterator it) noexcept {
	if (s.spareBuckets.size() < MAX_SPARE_BUCKETS && it->second.capacity() <= MAX_SPARE_BUCKET_CAPACITY)
		s.spareBuckets.push_back(std::move(it->second));
	s.limbo.erase(it);
}

void publishBacklog(ReclaimerState& s) noexcept {
	s.scannerBacklog.store(s.limboCount, std::memory_order_relaxed);
	s.scannerBacklogBytes.store(s.limboBytes, std::memory_order_relaxed);
}

/** Files the taken incoming batches into the limbo under the key each entry carries. Reads no object. */
void fileIncoming(ReclaimerState& s, RetireBatch* batch) {
	while (batch != nullptr) {
		Bucket* bucket = nullptr;
		uint64_t bucketKey = 0;
		for (const RetiredItem& item : batch->items) {
			if (bucket == nullptr || item.epoch != bucketKey) { // entries of one batch mostly share their epoch
				bucket = &bucketFor(s, item.epoch);
				bucketKey = item.epoch;
			}
			bucket->push_back(item);
		}
		s.limboCount += batch->items.size();
		s.limboBytes += batch->bytes;
		// published before the incoming counters drop (review fix): stats() reads incomingCount first, so a racing reader may count the batch
		// twice but never misses it (drain() and teardown checks rely on backlog == 0 meaning empty)
		publishBacklog(s);
		s.incomingCount.fetch_sub(batch->items.size(), std::memory_order_acq_rel);
		s.incomingBytes.fetch_sub(batch->bytes, std::memory_order_relaxed);
		RetireBatch* next = batch->next;
		delete batch;
		batch = next;
	}
}

// ------------------------------------------------------------------------------------------------------------------------ scan

enum class Decision { DESTROY, KEEP, DROP };

struct ObservedObject {
	uint32_t count;
	uint64_t stamp;
};

struct ObjectDecision {
	Decision decision;
	/** KEEP: the new limbo key (the stamp read by this scan, a lower bound of the object's stamp from now on) */
	uint64_t key;
};

/** Design §2.4 step 3 for one queued object. `observed` is only used by Mutation::SCAN_MIN_AFTER_OBJECTS. */
ObjectDecision decideObject(const RefCounted& object, uint64_t m, const ObservedObject* observed) noexcept {
#if AION_CHECKED
	AION_CHECK_ALWAYS("C5", RefCountedAccess::cookie(object).load(std::memory_order_acquire) == RefCounted::Cookie::ALIVE,
		"Reclaimer invariant: a queued object was already destroyed (queued twice)");
#endif
	std::atomic<uint32_t>& count = RefCountedAccess::count(object);
	std::atomic<bool>& queued = RefCountedAccess::queued(object);
	std::atomic<uint64_t>& retireEpoch = RefCountedAccess::retireEpoch(object);
	uint32_t current;
	if (observed != nullptr) {
		current = observed->count;
	} else {
		AION_YIELD_POINT("Reclaimer::scan:readCount");
		current = count.load(std::memory_order_acquire);
	}
	if (current == 0) {
		uint64_t stamp;
		if (observed != nullptr) {
			stamp = observed->stamp;
		} else {
			AION_YIELD_POINT("Reclaimer::scan:readStamp");
			stamp = retireEpoch.load(std::memory_order_acquire);
		}
		return stamp < m ? ObjectDecision{Decision::DESTROY, 0} : ObjectDecision{Decision::KEEP, stamp};
	}
	if (isMutated(Mutation::SCAN_KEEP_WITHOUT_CLEAR))
		return {Decision::DROP, 0};
	AION_YIELD_POINT("Reclaimer::scan:clearQueued");
	queued.store(false, std::memory_order_release);
	if (isMutated(Mutation::SCAN_DROP_WITHOUT_RECHECK))
		return {Decision::DROP, 0};
	AION_YIELD_POINT("Reclaimer::scan:recheck");
	if (count.load(std::memory_order_acquire) == 0) {
		if (isMutated(Mutation::SCAN_REQUEUE_WITHOUT_EXCHANGE))
			return {Decision::KEEP, retireEpoch.load(std::memory_order_acquire)};
		AION_YIELD_POINT("Reclaimer::scan:requeue");
		if (!queued.exchange(true, std::memory_order_acq_rel)) {
			// The release that reached 0 stored its stamp before its decrement, so the stamp read now is a lower bound from here on (a limbo key
			// only: the next visit reads count and stamp again).
			return {Decision::KEEP, retireEpoch.load(std::memory_order_acquire)};
		}
	}
	return {Decision::DROP, 0};
}

struct PartDecision {
	bool destroy;
	/** !destroy: the new limbo key, max(retirement stamp, part stamp read now) */
	uint64_t key;
	/** !destroy: a Ref held the part */
	bool held = false;
};

/**
 * Design §2.4 step 4 for a retired OwnedPart, extended by the part's own reference count (see OwnedPart): destroyed only when no Ref holds the
 * part and the later of its retirement and its last release is older than m. The count is read before the stamp; every release stores its
 * stamp before its decrement, so a zero count implies the stamp of the release that reached it is visible.
 */
PartDecision decideCountedPart(const RetiredItem& item, uint64_t m) noexcept {
	const OwnedPart& part = *item.countedPart();
	std::atomic<uint64_t>& partStamp = detail::OwnedPartAccess::stamp(part);
	AION_YIELD_POINT("Reclaimer::scan:readPartRefs");
	if (!isMutated(Mutation::PART_REFS_IGNORED) && detail::OwnedPartAccess::refs(part).load(std::memory_order_acquire) != 0) {
		// held by a Ref: the eventual release stamps at least the current part stamp (stamps only grow), so this is a lower bound
		return {false, std::max(item.epoch, partStamp.load(std::memory_order_acquire)), true};
	}
	AION_YIELD_POINT("Reclaimer::scan:readPartStamp");
	uint64_t stamp = std::max(item.epoch, partStamp.load(std::memory_order_acquire));
	if (isMutated(Mutation::PART_REFS_IGNORED))
		stamp = item.epoch; // the design's literal rule: the retirement stamp alone
	return {stamp < m, stamp};
}

void recordEpochStart(ReclaimerState& s, uint64_t epoch, int64_t now) noexcept {
	s.epochStarts[s.epochStartsNext] = {epoch, now};
	s.epochStartsNext = (s.epochStartsNext + 1) % EPOCH_START_RING;
	s.epochStartsSize = std::min(s.epochStartsSize + 1, EPOCH_START_RING);
}

/**
 * Time at which E became `epoch`: the start recorded for it, else (an epoch advanced by the test hook, or a stale publication) the start of the
 * latest recorded epoch below it, else the oldest known start. Entries are recorded in increasing epoch order, so this is a binary search.
 */
int64_t epochStartNanos(const ReclaimerState& s, uint64_t epoch) noexcept {
	const size_t first = (s.epochStartsNext + EPOCH_START_RING - s.epochStartsSize) % EPOCH_START_RING;
	auto at = [&](size_t index) -> const std::pair<uint64_t, int64_t>& { return s.epochStarts[(first + index) % EPOCH_START_RING]; };
	size_t low = 0;
	size_t high = s.epochStartsSize; // first index with a recorded epoch > `epoch`
	while (low < high) {
		size_t middle = low + (high - low) / 2;
		if (at(middle).first <= epoch)
			low = middle + 1;
		else
			high = middle;
	}
	if (low == 0)
		return at(0).second;
	size_t found = low - 1;
	while (found > 0 && at(found - 1).first == at(found).first) // SCAN_NO_ADVANCE records one epoch repeatedly: its first start
		--found;
	return at(found).second;
}

#if AION_CHECKED

size_t destroyedSlot(uintptr_t key, size_t mask) noexcept {
	return static_cast<size_t>((static_cast<uint64_t>(key) * 0x9E37'79B9'7F4A'7C15ull) >> 32) & mask;
}

/** true if `key` (an object pointer) was destroyed earlier in the running scan. Does not allocate. */
bool destroyedThisScan(const ReclaimerState& s, uintptr_t key) noexcept {
	if (s.destroyedSetCount == 0)
		return false;
	const size_t mask = s.destroyedSet.size() - 1;
	for (size_t slot = destroyedSlot(key, mask);; slot = (slot + 1) & mask) {
		if (s.destroyedSet[slot] == key)
			return true;
		if (s.destroyedSet[slot] == 0)
			return false;
	}
}

/** @return false if `key` was already in the set. Requires room (growDestroyedSet). */
bool insertDestroyed(ReclaimerState& s, uintptr_t key) noexcept {
	const size_t mask = s.destroyedSet.size() - 1;
	for (size_t slot = destroyedSlot(key, mask);; slot = (slot + 1) & mask) {
		if (s.destroyedSet[slot] == key)
			return false;
		if (s.destroyedSet[slot] == 0) {
			s.destroyedSet[slot] = key;
			++s.destroyedSetCount;
			return true;
		}
	}
}

/** Keeps the load factor at most 1/2 for `adding` more keys. Allocates: between chunks only. */
void growDestroyedSet(ReclaimerState& s, size_t adding) {
	const size_t needed = (s.destroyedSetCount + adding) * 2;
	if (s.destroyedSet.size() >= needed && !s.destroyedSet.empty())
		return;
	size_t capacity = std::max<size_t>(256, s.destroyedSet.size());
	while (capacity < needed)
		capacity *= 2;
	std::vector<uintptr_t> previous(capacity, 0);
	previous.swap(s.destroyedSet);
	s.destroyedSetCount = 0;
	for (uintptr_t key : previous)
		if (key != 0)
			(void)insertDestroyed(s, key);
}

/** Empties the set at the start of a scan; a table much larger than the last scan needed is released. Costs O(the last scan's destruction). */
void resetDestroyedSet(ReclaimerState& s) noexcept {
	if (s.destroyedSetCount == 0)
		return;
	if (s.destroyedSet.size() > 4096 && s.destroyedSet.size() > s.destroyedSetCount * 8)
		std::vector<uintptr_t>().swap(s.destroyedSet);
	else
		std::fill(s.destroyedSet.begin(), s.destroyedSet.end(), uintptr_t{0});
	s.destroyedSetCount = 0;
}

#endif

/**
 * Destroys the classified chunk: objects (after the C5 checks, the destroy observer and the C3 poisoning), then parts and nodes, then releases
 * the owners of the destroyed parts outside the destructor context.
 */
void destroyChunk(ReclaimerState& s) {
#if AION_CHECKED
	// C5 double retire: every object of the chunk joins the scan's destroyed set before any is destroyed (a duplicate in the same chunk, or of an
	// object destroyed by an earlier chunk of this scan, fails here; classifyEntry checks later entries before reading them)
	growDestroyedSet(s, s.chunkObjects.size());
	for (const RetiredItem& item : s.chunkObjects) {
		AION_CHECK_ALWAYS("C5", insertDestroyed(s, item.tagged), "Reclaimer invariant: an object was queued twice (double retire)");
	}
#endif
	uint64_t bytes = 0;
	{
		DestructorContext context;
		Reclaimer::DestroyObserver observer = s.destroyObserver.load(std::memory_order_acquire);
		for (const RetiredItem& item : s.chunkObjects) {
			const RefCounted* object = item.object();
			AION_YIELD_POINT("Reclaimer::scan:destroy");
			AION_CHECK_ALWAYS("C5", RefCountedAccess::count(*object).load(std::memory_order_acquire) == 0,
				"Reclaimer invariant: an object selected for destruction was resurrected (count != 0)");
			if (observer != nullptr)
				observer(*object);
			void* memory = const_cast<void*>(dynamic_cast<const void*>(object));
			const char* cookieAddress = nullptr;
#if AION_CHECKED
			cookieAddress = static_cast<const char*>(static_cast<const void*>(&RefCountedAccess::cookie(*object)));
			RefCountedAccess::cookie(*object).store(RefCounted::Cookie::DEAD, std::memory_order_release);
#endif
			RefCountedAccess::destroy(*object);
			freeObject(s, memory, cookieAddress);
			bytes += item.bytes;
		}
		for (const RetiredItem& item : s.chunkOthers) {
			AION_YIELD_POINT("Reclaimer::scan:destroy");
			if (item.kind() == COUNTED_PART)
				delete static_cast<OwnedPartBase*>(item.countedPart()); // OwnedPart destructors are protected; OwnedPartBase has a public virtual one
			else if (item.kind() == PART)
				delete item.part();
			else
				delete item.node();
			bytes += item.bytes;
		}
	}
	for (const RetiredItem& item : s.chunkOthers) {
		if (item.kind() == PART || item.kind() == COUNTED_PART)
			item.owner->release();
	}
	const uint64_t count = s.chunkObjects.size() + s.chunkOthers.size();
	s.destroyed.fetch_add(count, std::memory_order_relaxed);
	s.limboCount -= count;
	s.limboBytes -= bytes;
	s.chunkObjects.clear();
	s.chunkOthers.clear();
}

/**
 * Classifies one entry of the running scan into the chunk (destroy), `refile` (keep: limbo or held-part queue) or nothing (drop). Does not
 * allocate. @return true if the entry stays in the limbo under a key below m (an object re-queued by the handshake): no progress for it
 */
bool classifyEntry(ReclaimerState& s, const RetiredItem& item, uint64_t m, const std::optional<std::unordered_map<uintptr_t, ObservedObject>>& observed) noexcept {
	switch (item.kind()) {
		case OBJECT: {
#if AION_CHECKED
			AION_CHECK_ALWAYS("C5", !destroyedThisScan(s, item.tagged), "Reclaimer invariant: a queued object was already destroyed (queued twice)");
#endif
			const ObservedObject* seen = nullptr;
			if (observed) {
				auto found = observed->find(item.tagged);
				seen = found != observed->end() ? &found->second : nullptr;
			}
			ObjectDecision decision = decideObject(*item.object(), m, seen);
			if (decision.decision == Decision::DESTROY) {
				s.chunkObjects.push_back(item);
			} else if (decision.decision == Decision::KEEP) {
				s.refile.push_back(Refiled{decision.key, RetiredItem{item.tagged, nullptr, decision.key, item.bytes}, false});
				return decision.key < m;
			} else {
				s.limboCount -= 1;
				s.limboBytes -= item.bytes;
			}
			return false;
		}
		case COUNTED_PART: {
			PartDecision decision = decideCountedPart(item, m);
			if (decision.destroy)
				s.chunkOthers.push_back(item);
			else // held with a key below m: the held-part queue (never the front of the limbo); else the limbo under a key >= m or still held
				s.refile.push_back(Refiled{decision.key, item, decision.held && decision.key < m});
			return false;
		}
		default: // PART, NODE: the key is the stamp
			if (item.epoch < m)
				s.chunkOthers.push_back(item);
			else
				s.refile.push_back(Refiled{item.epoch, item, false});
			return false;
	}
}

size_t chunkSize() noexcept {
#if AION_LIFETIME_MUTATIONS
	if (uint32_t chunk = detail::testing::scanChunkOverride.load(std::memory_order_relaxed); chunk != 0)
		return std::min<size_t>(chunk, CHUNK);
#endif
	return CHUNK;
}

/** Takes up to `limit` entries with a key below m from the limbo (oldest bucket first) and classifies them. @return entries taken */
uint64_t takeFromLimbo(ReclaimerState& s, uint64_t limit, uint64_t m, const std::optional<std::unordered_map<uintptr_t, ObservedObject>>& observed,
	uint64_t& stuck) noexcept {
	uint64_t taken = 0;
	while (taken < limit) {
		auto it = s.limbo.begin();
		if (it == s.limbo.end() || (it->first >= m && !isMutated(Mutation::SCAN_EXAMINE_WHOLE_LIMBO)))
			break;
		Bucket& bucket = it->second;
		if (bucket.empty()) {
			eraseBucket(s, it);
			continue;
		}
		// entries are taken from the back of the oldest bucket; classification appends only to the chunk vectors and `refile`, never to a bucket
		const size_t size = bucket.size();
		const size_t count = static_cast<size_t>(std::min<uint64_t>(limit - taken, size));
		const RetiredItem* entries = bucket.data();
		for (size_t i = 1; i <= count; ++i)
			stuck += classifyEntry(s, entries[size - i], m, observed) ? 1 : 0;
		bucket.resize(size - count);
		taken += count;
	}
	return taken;
}

/** Takes up to `limit` entries from the front of the held-part queue and classifies them. @return entries taken */
uint64_t takeHeldParts(ReclaimerState& s, uint64_t limit, uint64_t m, const std::optional<std::unordered_map<uintptr_t, ObservedObject>>& observed) noexcept {
	uint64_t taken = 0;
	while (taken < limit && !s.heldParts.empty()) {
		const RetiredItem item = s.heldParts.front();
		s.heldParts.pop_front();
		(void)classifyEntry(s, item, m, observed);
		++taken;
	}
	return taken;
}

struct ScanResult {
	uint64_t examined = 0;
	uint64_t destroyed = 0;
	/** the budget ended the scan while limbo entries with a key below m were left (Stats::budgetExhaustedScans) */
	bool budgetExhausted = false;
	/** budgetExhausted, and the next scan should follow without waiting (this one made progress on those entries or did not reach them) */
	bool eligibleLeft = false;
};

/** The scan's steps (1)-(4); may throw std::bad_alloc outside the classification of an entry (see scan()). */
ScanResult scanSteps(ReclaimerState& s, const ScanBudget& budget) {
	const int64_t now = commons::utils::nanoTime();
	const uint64_t destroyedBefore = s.destroyed.load(std::memory_order_relaxed);

	// (1) advance
	AION_YIELD_POINT("Reclaimer::scan:advance");
	const uint64_t epoch =
		isMutated(Mutation::SCAN_NO_ADVANCE) ? detail::globalEpoch.load(std::memory_order_acquire) : detail::globalEpoch.fetch_add(1, std::memory_order_acq_rel) + 1;
	recordEpochStart(s, epoch, now);

	// (2) m = min(E, published), before reading any object. The TaskInfo of the thread holding the oldest epoch is read right here (review fix:
	// reading it after the destruction named whatever task the thread ran by then): the task snapshot belongs to the publishing scope if the
	// published epoch is unchanged after the snapshot, because a scope exit stores IDLE and every later publication stores an epoch >= E > m.
	uint64_t m = epoch;
	TaskInfo oldestTask{};
	uint64_t oldestThreadId = 0;
	auto computeMin = [&] {
		m = epoch;
		oldestTask = TaskInfo{};
		oldestThreadId = 0;
		ThreadContext::forEach([&](const ThreadContext& context) {
			AION_YIELD_POINT("Reclaimer::scan:readPublished");
			uint64_t published = context.publishedEpoch.load(std::memory_order_acquire);
			if (published < m) {
				m = published;
				ThreadContext::TaskSnapshot task = context.task();
				AION_YIELD_POINT("Reclaimer::scan:readPublishedTask");
				bool samePublication = context.publishedEpoch.load(std::memory_order_acquire) == published;
				oldestTask = samePublication && task.active ? task.info : TaskInfo{};
				oldestThreadId = context.threadId();
			}
		});
		if (isMutated(Mutation::SCAN_IGNORE_PUBLISHED))
			m = epoch;
	};
	const bool minFirst = !isMutated(Mutation::SCAN_MIN_AFTER_OBJECTS);
	if (minFirst)
		computeMin();

	// take the incoming stack and file its entries (reads no object)
	AION_YIELD_POINT("Reclaimer::scan:take");
	fileIncoming(s, s.incoming.exchange(nullptr, std::memory_order_acq_rel));

	std::optional<std::unordered_map<uintptr_t, ObservedObject>> observed; // Mutation::SCAN_MIN_AFTER_OBJECTS only
	if (!minFirst) {
		observed.emplace();
		for (const auto& [key, bucket] : s.limbo) {
			if (key >= epoch) // m <= epoch: later buckets are not visited
				break;
			for (const RetiredItem& item : bucket) {
				if (item.kind() != OBJECT)
					continue;
				AION_YIELD_POINT("Reclaimer::scan:readCount");
				uint32_t count = RefCountedAccess::count(*item.object()).load(std::memory_order_acquire);
				AION_YIELD_POINT("Reclaimer::scan:readStamp");
				uint64_t stamp = RefCountedAccess::retireEpoch(*item.object()).load(std::memory_order_acquire);
				if (count == 0) // only the zero-count decision is taken on stale values
					(*observed)[item.tagged] = ObservedObject{count, stamp};
			}
		}
		computeMin();
	}

	// (3), (4) visit the entries with a key below m in chunks: the limbo's buckets oldest first, and the queue of retired parts held by Refs round
	// robin. Classification does not allocate: the chunk vectors have capacity CHUNK and `refile` is reserved for the chunk before it starts, so
	// an allocation failure never interrupts an entry's classification.
	ScanResult result;
	const uint64_t chunk = chunkSize();
	const uint64_t heldAtStart = s.heldParts.size(); // parts moved to the queue by this scan are appended after the visit
	uint64_t heldExamined = 0;
	uint64_t limboExamined = 0;
	uint64_t limboProgress = 0; // limbo entries that left the part below m (destroyed, dropped, re-filed under a key >= m, moved to the queue)
	bool stopped = false;
	auto reserveRefile = [&](uint64_t entries) {
		if (s.refile.size() + entries > s.refile.capacity())
			s.refile.reserve(std::max<size_t>(s.refile.size() + entries, s.refile.capacity() * 2));
	};
	auto finishChunk = [&](uint64_t taken) {
		result.examined += taken;
		destroyChunk(s);
		publishBacklog(s);
	};
	auto outOfBudget = [&] { return result.examined >= budget.entries || commons::utils::nanoTime() - now >= budget.nanos; };
	// every second scan starts with a share of the budget for the held parts, so a steady eligible inflow that uses whole budgets cannot starve
	// them (a part whose Ref was released is destroyed by the Reclaimer thread); the share is not cut by the time budget
	if ((s.scanSequence++ & 1) != 0 && heldAtStart > 0) {
		const uint64_t share = std::min<uint64_t>({heldAtStart, chunk, std::max<uint64_t>(1, budget.entries / 2)});
		reserveRefile(share);
		const uint64_t taken = takeHeldParts(s, share, m, observed);
		heldExamined += taken;
		finishChunk(taken);
		stopped = result.examined >= budget.entries;
	}
	while (!stopped) {
		const uint64_t limit = std::min<uint64_t>(chunk, budget.entries - result.examined);
		reserveRefile(limit);
		uint64_t stuck = 0;
		const uint64_t taken = takeFromLimbo(s, limit, m, observed, stuck);
		if (taken == 0)
			break;
		limboExamined += taken;
		limboProgress += taken - stuck;
		finishChunk(taken);
		stopped = outOfBudget();
	}
	while (!stopped && heldExamined < heldAtStart) {
		const uint64_t limit = std::min<uint64_t>({chunk, budget.entries - result.examined, heldAtStart - heldExamined});
		reserveRefile(limit);
		const uint64_t taken = takeHeldParts(s, limit, m, observed);
		if (taken == 0)
			break;
		heldExamined += taken;
		finishChunk(taken);
		stopped = outOfBudget();
	}
	if (stopped) {
		auto it = s.limbo.begin();
		while (it != s.limbo.end() && it->first < m && it->second.empty()) {
			eraseBucket(s, it);
			it = s.limbo.begin();
		}
		const bool limboLeft = it != s.limbo.end() && it->first < m;
		result.budgetExhausted = limboLeft;
		// continued at once while the limbo part below m shrinks (or was not reached): entries that stay below m (objects re-queued by the
		// handshake) and held parts are not a reason to spin
		result.eligibleLeft = limboLeft && (limboProgress > 0 || limboExamined == 0);
	}
	for (const Refiled& entry : s.refile) {
		if (entry.held)
			s.heldParts.push_back(entry.item);
		else
			bucketFor(s, entry.key).push_back(entry.item);
	}
	s.refile.clear();
	if (s.refile.capacity() > MAX_SPARE_BUCKET_CAPACITY) { // after an unusual scan: do not keep its peak
		s.refile = std::vector<Refiled>();
		s.refile.reserve(CHUNK);
	}
	s.heldPartsCount.store(s.heldParts.size(), std::memory_order_relaxed);
#if AION_CHECKED
	resetDestroyedSet(s);
#endif
	publishBacklog(s);
	releaseDelayedFree(s, s.delayedFreeLimit.load(std::memory_order_relaxed));
	result.destroyed = s.destroyed.load(std::memory_order_relaxed) - destroyedBefore;

	// statistics
	std::chrono::milliseconds lag{0};
	if (m < epoch)
		lag = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::nanoseconds(std::max<int64_t>(0, now - epochStartNanos(s, m))));
	{
		std::scoped_lock lock(s.statsMutex);
		s.minActive = m;
		s.lag = lag;
		s.oldestTask = oldestTask;
		s.oldestThreadId = oldestThreadId;
		s.lastScanExamined = result.examined;
		s.lastScanDestroyed = result.destroyed;
		s.lastScanDuration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::nanoseconds(commons::utils::nanoTime() - now));
	}
	s.examinedTotal.fetch_add(result.examined, std::memory_order_relaxed);
	if (result.budgetExhausted)
		s.budgetExhaustedScans.fetch_add(1, std::memory_order_relaxed);
	s.scans.fetch_add(1, std::memory_order_relaxed);
	return result;
}

/**
 * One scan. The only exceptions scanSteps can throw are allocation failures (and std::system_error from the stats mutex); a scan interrupted
 * half-way would leave the limbo counters inconsistent, so they terminate like the other out-of-memory paths of the Reclaimer (C5).
 */
ScanResult scan(ReclaimerState& s, const ScanBudget& budget) noexcept {
	try {
		return scanSteps(s, budget);
	} catch (...) {
		checkFailed("C5", "Reclaimer: exception (out of memory) during a scan");
	}
}

/** Holds scanMutex and records the scanning thread (PCT: a contended wait is a blocking bracket). */
class ScanLock {
public:
	explicit ScanLock(ReclaimerState& s) : state_(s), lock_(s.scanMutex, std::try_to_lock) {
		if (!lock_.owns_lock()) {
			s.scanWaiters.fetch_add(1, std::memory_order_acq_rel);
			AION_PCT_BLOCKING_BEGIN("Reclaimer::reclaimNow");
			try {
				lock_.lock();
			} catch (...) {
				s.scanWaiters.fetch_sub(1, std::memory_order_acq_rel);
				throw;
			}
			AION_PCT_BLOCKING_END();
			s.scanWaiters.fetch_sub(1, std::memory_order_acq_rel);
		}
		state_.scanningThread.store(std::this_thread::get_id(), std::memory_order_release);
	}
	~ScanLock() { state_.scanningThread.store(std::thread::id(), std::memory_order_release); }
	ScanLock(const ScanLock&) = delete;
	ScanLock& operator=(const ScanLock&) = delete;

private:
	ReclaimerState& state_;
	std::unique_lock<std::mutex> lock_;
};

/**
 * Barrier for hook and observer removal (review fix: removal returned while a copy of the hook was still running on the scanning thread):
 * waits until a scan, its destroy observer calls and its post-scan hooks that may have started before the caller's change have finished.
 * A call from the scanning thread itself (a hook removing itself or another hook) returns immediately.
 */
void waitForRunningScan(ReclaimerState& s) noexcept {
	if (s.scanningThread.load(std::memory_order_acquire) == std::this_thread::get_id())
		return;
	try {
		std::unique_lock lock(s.scanMutex, std::try_to_lock);
		if (!lock.owns_lock()) {
			s.scanWaiters.fetch_add(1, std::memory_order_acq_rel);
			AION_PCT_BLOCKING_BEGIN("Reclaimer::waitForRunningScan");
			lock.lock(); // std::system_error terminates below
			AION_PCT_BLOCKING_END();
			s.scanWaiters.fetch_sub(1, std::memory_order_acq_rel);
		}
	} catch (...) {
		checkFailed("C5", "Reclaimer: waiting for a running scan failed (std::mutex)");
	}
}

void runHooks(ReclaimerState& s) {
	std::vector<std::pair<std::string, std::shared_ptr<Reclaimer::ScanHook>>> hooks;
	{
		std::scoped_lock lock(s.hooksMutex);
		for (auto& [id, hook] : s.hooks)
			hooks.push_back(hook);
	}
	if (hooks.empty())
		return;
	TaskScope scope(AION_TASK_INFO(TaskKind::RECLAIMER));
	for (auto& [name, hook] : hooks) {
		try {
			(*hook)();
		} catch (...) {
			log().errorCurrentException("Post-scan hook " + name + " failed");
		}
	}
}

/** flush, scan, flush the cascades released by destructors, hooks (all under the scan lock) */
ScanResult runScan(ReclaimerState& s, const ScanBudget& budget) {
	detail::flushThreadRetireList();
	ScanLock lock(s);
	ScanResult result = scan(s, budget);
	detail::flushThreadRetireList(); // cascades released by destructors
	runHooks(s);
	return result;
}

// ------------------------------------------------------------------------------------------------------------------------ thread and probe

void threadMain() {
	ReclaimerState& s = state();
	commons::utils::concurrent::setCurrentThreadName("Reclaimer");
	ThreadContext::current().refreshName();
	using Clock = std::chrono::steady_clock;
	Clock::time_point nextScan = Clock::now() + Reclaimer::getInstance().getConfig().period;
	bool eligibleLeft = false;
	while (!s.stopRequested.load(std::memory_order_acquire)) {
		Reclaimer::Config config;
		{
			std::scoped_lock lock(s.configMutex);
			config = s.config;
		}
		if (!eligibleLeft) { // a scan that ran out of budget is continued at once; otherwise scans start every period (fixed rate)
			std::unique_lock lock(s.threadMutex);
			s.wakeCondition.wait_until(lock, nextScan, [&] { return s.stopRequested.load() || s.wakeRequested.load(); });
		} else if (s.scanWaiters.load(std::memory_order_acquire) != 0) {
			// Review fix: std::mutex (an SRWLOCK) is not fair, so re-locking at once could keep a thread waiting in reclaimNow, removePostScanHook or
			// setDestroyObserver for a whole drain. A waiter decrements the count once it holds the lock; after at most one period the scan goes on.
			const Clock::time_point giveUp = Clock::now() + config.period;
			for (uint32_t spins = 0; s.scanWaiters.load(std::memory_order_acquire) != 0 && !s.stopRequested.load(std::memory_order_acquire) &&
									 Clock::now() < giveUp;
				 ++spins) {
				if (spins < 64)
					std::this_thread::yield();
				else
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
		s.wakeRequested.store(false, std::memory_order_release);
		if (s.stopRequested.load(std::memory_order_acquire))
			break;
		nextScan = Clock::now() + config.period;
		ScanBudget budget;
		budget.entries = config.scanEntryBudget == 0 ? UINT64_MAX : config.scanEntryBudget;
		budget.nanos = config.scanTimeBudget.count() <= 0 ? INT64_MAX : std::chrono::duration_cast<std::chrono::nanoseconds>(config.scanTimeBudget).count();
		try {
			eligibleLeft = runScan(s, budget).eligibleLeft;
		} catch (...) {
			eligibleLeft = false;
			log().errorCurrentException("Reclaimer scan failed");
		}
	}
}

void probe(Watchdog& watchdog) {
	ReclaimerState& s = state();
	Reclaimer::Stats stats = Reclaimer::getInstance().stats();
	Reclaimer::Config config = Reclaimer::getInstance().getConfig();
	if (stats.lag > config.lagWarning && stats.minActive != s.lagWarnedEpoch) {
		s.lagWarnedEpoch = stats.minActive;
		log().warn("Reclamation lag {} ms: thread {} ({} at {}:{}) holds epoch {} (E = {}, backlog {} objects, {} bytes)", stats.lag.count(),
			stats.oldestPublishedThreadId, stats.oldestPublishedTask.kind, stats.oldestPublishedTask.where.file_name(),
			stats.oldestPublishedTask.where.line(), stats.minActive, stats.epoch, stats.backlog, stats.backlogBytes);
	}
	bool high = stats.backlog > config.backlogDumpObjects || stats.backlogBytes > config.backlogDumpBytes;
	if (high && !s.backlogDumped) {
		s.backlogDumped = true;
		std::vector<uint64_t> involved;
		if (stats.oldestPublishedThreadId != 0)
			involved.push_back(stats.oldestPublishedThreadId);
		watchdog.dump(Watchdog::Reason::BACKLOG,
			"Reclaimer backlog " + std::to_string(stats.backlog) + " objects / " + std::to_string(stats.backlogBytes) + " bytes, lag " +
				std::to_string(stats.lag.count()) + " ms, oldest publishing thread " + std::to_string(stats.oldestPublishedThreadId),
			std::move(involved));
	} else if (stats.backlog < config.backlogDumpObjects / 2 && stats.backlogBytes < config.backlogDumpBytes / 2) {
		s.backlogDumped = false;
	}
}

} // namespace

// ---------------------------------------------------------------------------------------------------------------------------- detail

namespace detail {

#if AION_LIFETIME_MUTATIONS
namespace testing {
std::atomic<uint32_t> scanChunkOverride{0};
std::atomic<uint64_t> reclaimNowEntryBudget{0};
} // namespace testing
#endif

void flushThreadRetireList() noexcept {
	if (threadRetireList != nullptr)
		threadRetireList->flush();
}

size_t unflushedRetireCount() noexcept {
	return threadRetireList != nullptr ? threadRetireList->items.size() : 0;
}

} // namespace detail

// ---------------------------------------------------------------------------------------------------------------------------- Reclaimer

Reclaimer& Reclaimer::getInstance() {
	static auto* instance = new Reclaimer();
	return *instance;
}

void Reclaimer::start(std::chrono::milliseconds period) {
	Config config = getConfig();
	config.period = period;
	start(config);
}

void Reclaimer::start(const Config& config) {
	ReclaimerState& s = state();
	configure(config);
	if (s.running.load(std::memory_order_acquire))
		return;
	s.stopRequested.store(false, std::memory_order_release);
	s.wakeRequested.store(false, std::memory_order_release);
	s.lagWarnedEpoch = 0; // the probe's deduplication starts afresh with every start (it is removed by stop)
	s.backlogDumped = false;
	s.running.store(true, std::memory_order_release);
	s.thread = std::thread(threadMain);
	s.probeId = Watchdog::getInstance().addProbe("reclaimer", [](Watchdog& watchdog, const std::vector<Watchdog::ThreadSnapshot>&) { probe(watchdog); });
}

void Reclaimer::stop() {
	ReclaimerState& s = state();
	if (!s.running.load(std::memory_order_acquire))
		return;
	{
		std::scoped_lock lock(s.threadMutex);
		s.stopRequested.store(true, std::memory_order_release);
	}
	s.wakeCondition.notify_all();
	if (s.thread.joinable())
		s.thread.join();
	s.running.store(false, std::memory_order_release);
	if (s.probeId != 0) {
		Watchdog::getInstance().removeProbe(s.probeId);
		s.probeId = 0;
	}
}

bool Reclaimer::isRunning() const noexcept {
	return state().running.load(std::memory_order_acquire);
}

void Reclaimer::configure(const Config& config) {
	ReclaimerState& s = state();
	std::scoped_lock lock(s.configMutex);
	s.config = config;
	s.wakeBacklog.store(config.wakeBacklog, std::memory_order_relaxed);
	s.delayedFreeLimit.store(config.delayedFreeBytes, std::memory_order_relaxed);
}

Reclaimer::Config Reclaimer::getConfig() const {
	ReclaimerState& s = state();
	std::scoped_lock lock(s.configMutex);
	return s.config;
}

void Reclaimer::retire(const RefCounted& object) noexcept {
	// The stamp stored by the release that queued the object (it stamps before its decrement) is the object's limbo key: stamps only grow.
	const uint64_t stamp = RefCountedAccess::retireEpoch(object).load(std::memory_order_acquire);
	pushItem(RetiredItem{reinterpret_cast<uintptr_t>(&object) | OBJECT, nullptr, stamp, objectBytes(object)});
}

void Reclaimer::retirePart(const RefCounted& owner, std::unique_ptr<OwnedPartBase> part) noexcept {
	if (part == nullptr)
		return;
	owner.retain();
	AION_YIELD_POINT("Reclaimer::retirePart:stamp");
	uint64_t epoch = isMutated(Mutation::PART_STAMP_ZERO) ? 0 : detail::globalEpoch.load(std::memory_order_acquire);
	if (const auto* counted = dynamic_cast<const OwnedPart*>(part.get())) {
		std::atomic<uint64_t>& stamp = detail::OwnedPartAccess::stamp(*counted);
		uint64_t stamped = stamp.load(std::memory_order_acquire);
		while (stamped < epoch && !stamp.compare_exchange_weak(stamped, epoch, std::memory_order_acq_rel)) {
		}
		auto* raw = const_cast<OwnedPart*>(counted);
		(void)part.release();
		pushItem(RetiredItem{reinterpret_cast<uintptr_t>(raw) | COUNTED_PART, &owner, epoch, 0});
		return;
	}
	pushItem(RetiredItem{reinterpret_cast<uintptr_t>(part.release()) | PART, &owner, epoch, 0});
}

void Reclaimer::retireNode(std::unique_ptr<RetiredNode> node) noexcept {
	if (node == nullptr)
		return;
	uint64_t bytes = node->retiredBytes();
	AION_YIELD_POINT("Reclaimer::retireNode:stamp");
	uint64_t epoch = isMutated(Mutation::NODE_STAMP_ZERO) ? 0 : detail::globalEpoch.load(std::memory_order_acquire);
	pushItem(RetiredItem{reinterpret_cast<uintptr_t>(node.release()) | NODE, nullptr, epoch, bytes});
}

void Reclaimer::reclaimNow() {
	ScanBudget budget;
#if AION_LIFETIME_MUTATIONS
	if (uint64_t entries = detail::testing::reclaimNowEntryBudget.load(std::memory_order_relaxed); entries != 0)
		budget.entries = entries;
#endif
	(void)runScan(state(), budget);
}

bool Reclaimer::drain(uint32_t maxScans) {
	for (uint32_t i = 0; i < maxScans; ++i) {
		reclaimNow();
		if (stats().backlog == 0 && detail::unflushedRetireCount() == 0)
			return true;
	}
	return stats().backlog == 0;
}

Reclaimer::Stats Reclaimer::stats() const {
	ReclaimerState& s = state();
	Stats stats;
	stats.epoch = detail::globalEpoch.load(std::memory_order_acquire);
	stats.backlog = s.incomingCount.load(std::memory_order_acquire) + s.scannerBacklog.load(std::memory_order_relaxed);
	stats.backlogBytes = s.incomingBytes.load(std::memory_order_relaxed) + s.scannerBacklogBytes.load(std::memory_order_relaxed);
	stats.destroyedTotal = s.destroyed.load(std::memory_order_relaxed);
	stats.scans = s.scans.load(std::memory_order_relaxed);
	stats.examinedTotal = s.examinedTotal.load(std::memory_order_relaxed);
	stats.budgetExhaustedScans = s.budgetExhaustedScans.load(std::memory_order_relaxed);
	stats.retiredPartsHeld = s.heldPartsCount.load(std::memory_order_relaxed);
	std::scoped_lock lock(s.statsMutex);
	stats.minActive = s.minActive;
	stats.lag = s.lag;
	stats.oldestPublishedTask = s.oldestTask;
	stats.oldestPublishedThreadId = s.oldestThreadId;
	stats.lastScanExamined = s.lastScanExamined;
	stats.lastScanDestroyed = s.lastScanDestroyed;
	stats.lastScanDuration = s.lastScanDuration;
	return stats;
}

uint64_t Reclaimer::currentEpoch() noexcept {
	return detail::globalEpoch.load(std::memory_order_acquire);
}

bool Reclaimer::inDestructorContext() noexcept {
	ThreadContext* context = ThreadContext::currentIfRegistered();
	return context != nullptr && context->destructorContextDepth > 0;
}

uint64_t Reclaimer::addPostScanHook(std::string name, ScanHook hook) {
	ReclaimerState& s = state();
	std::scoped_lock lock(s.hooksMutex);
	uint64_t id = s.nextHookId++;
	s.hooks.emplace_back(id, std::make_pair(std::move(name), std::make_shared<ScanHook>(std::move(hook))));
	return id;
}

void Reclaimer::removePostScanHook(uint64_t hookId) {
	ReclaimerState& s = state();
	{
		std::scoped_lock lock(s.hooksMutex);
		std::erase_if(s.hooks, [hookId](const auto& entry) { return entry.first == hookId; });
	}
	waitForRunningScan(s);
}

void Reclaimer::setDestroyObserver(DestroyObserver observer) noexcept {
	ReclaimerState& s = state();
	s.destroyObserver.store(observer, std::memory_order_release);
	waitForRunningScan(s);
}

} // namespace aion::gameserver::runtime
