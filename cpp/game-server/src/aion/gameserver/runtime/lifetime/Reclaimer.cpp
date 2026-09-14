// The epoch Reclaimer: per-thread retire lists, the lock-free incoming stack, the scan of design §2.4, destruction with C3/C5/C8 checks,
// statistics, hooks and the Reclaimer thread. The release side of the protocol lives in RefCounted.cpp, publication in TaskScope.cpp.

#include "aion/gameserver/runtime/lifetime/Reclaimer.h"

#include <algorithm>
#include <array>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <optional>
#include <thread>
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
	/** PART/NODE: E at retire time (COUNTED_PART: also stamped into the part) */
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
constexpr size_t EPOCH_START_RING = 1024;

struct ReclaimerState {
	// ------------------------------------------------------------------------------------------------ incoming (lock-free, any thread)
	std::atomic<RetireBatch*> incoming{nullptr};
	std::atomic<uint64_t> incomingCount{0};
	std::atomic<uint64_t> incomingBytes{0};

	// ------------------------------------------------------------------------------------------------ scanner (guarded by scanMutex)
	std::mutex scanMutex;
	/** the thread holding scanMutex (default id when none): hook/observer removal from that thread must not wait for itself */
	std::atomic<std::thread::id> scanningThread{};
	std::vector<RetiredItem> queue;
	/** scan scratch vectors, reused across scans (their capacity follows the largest queue) */
	std::vector<RetiredItem> scratchKeep;
	std::vector<const RefCounted*> scratchDestroyObjects;
	std::vector<RetiredItem> scratchDestroyOthers;
	std::vector<const RefCounted*> scratchSorted;
	std::deque<std::pair<void*, size_t>> delayedFree;
	size_t delayedFreeUsed = 0;
	std::array<std::pair<uint64_t, int64_t>, EPOCH_START_RING> epochStarts{};
	size_t epochStartsNext = 0;
	size_t epochStartsSize = 0;

	// ------------------------------------------------------------------------------------------------ statistics
	std::atomic<uint64_t> scannerBacklog{0};
	std::atomic<uint64_t> scannerBacklogBytes{0};
	std::atomic<uint64_t> destroyed{0};
	std::atomic<uint64_t> scans{0};
	mutable std::mutex statsMutex; // leaf: no callbacks under it
	uint64_t minActive = 0;
	std::chrono::milliseconds lag{0};
	TaskInfo oldestTask{};
	uint64_t oldestThreadId = 0;

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

// ------------------------------------------------------------------------------------------------------------------------ scan

enum class Decision { DESTROY, KEEP, DROP };

/** Ensures capacity for `size` elements with geometric growth (plain reserve would reallocate on every scan that adds a few entries). */
template <class T>
void reserveGeometric(std::vector<T>& vector, size_t size) {
	if (size > vector.capacity())
		vector.reserve(std::max(size, vector.capacity() * 2));
}

struct ObservedObject {
	uint32_t count;
	uint64_t stamp;
};

/** Design §2.4 step 3 for one queued object. `observed` is only used by Mutation::SCAN_MIN_AFTER_OBJECTS. */
Decision decideObject(const RefCounted& object, uint64_t m, const std::optional<ObservedObject>& observed) noexcept {
#if AION_CHECKED
	AION_CHECK_ALWAYS("C5", RefCountedAccess::cookie(object).load(std::memory_order_acquire) == RefCounted::Cookie::ALIVE,
		"Reclaimer invariant: a queued object was already destroyed (queued twice)");
#endif
	std::atomic<uint32_t>& count = RefCountedAccess::count(object);
	std::atomic<bool>& queued = RefCountedAccess::queued(object);
	uint32_t current;
	if (observed) {
		current = observed->count;
	} else {
		AION_YIELD_POINT("Reclaimer::scan:readCount");
		current = count.load(std::memory_order_acquire);
	}
	if (current == 0) {
		uint64_t stamp;
		if (observed) {
			stamp = observed->stamp;
		} else {
			AION_YIELD_POINT("Reclaimer::scan:readStamp");
			stamp = RefCountedAccess::retireEpoch(object).load(std::memory_order_acquire);
		}
		return stamp < m ? Decision::DESTROY : Decision::KEEP;
	}
	if (isMutated(Mutation::SCAN_KEEP_WITHOUT_CLEAR))
		return Decision::DROP;
	AION_YIELD_POINT("Reclaimer::scan:clearQueued");
	queued.store(false, std::memory_order_release);
	if (isMutated(Mutation::SCAN_DROP_WITHOUT_RECHECK))
		return Decision::DROP;
	AION_YIELD_POINT("Reclaimer::scan:recheck");
	if (count.load(std::memory_order_acquire) == 0) {
		if (isMutated(Mutation::SCAN_REQUEUE_WITHOUT_EXCHANGE))
			return Decision::KEEP;
		AION_YIELD_POINT("Reclaimer::scan:requeue");
		if (!queued.exchange(true, std::memory_order_acq_rel))
			return Decision::KEEP;
	}
	return Decision::DROP;
}

/**
 * Design §2.4 step 4 for a retired OwnedPart, extended by the part's own reference count (see OwnedPart): destroyed only when no Ref holds the
 * part and the later of its retirement and its last release is older than m. The count is read before the stamp; every release stores its
 * stamp before its decrement, so a zero count implies the stamp of the release that reached it is visible.
 */
bool decideCountedPart(const RetiredItem& item, uint64_t m) noexcept {
	const OwnedPart& part = *item.countedPart();
	AION_YIELD_POINT("Reclaimer::scan:readPartRefs");
	if (!isMutated(Mutation::PART_REFS_IGNORED) && detail::OwnedPartAccess::refs(part).load(std::memory_order_acquire) != 0)
		return false;
	AION_YIELD_POINT("Reclaimer::scan:readPartStamp");
	uint64_t stamp = std::max(item.epoch, detail::OwnedPartAccess::stamp(part).load(std::memory_order_acquire));
	if (isMutated(Mutation::PART_REFS_IGNORED))
		stamp = item.epoch; // the design's literal rule: the retirement stamp alone
	return stamp < m;
}

void recordEpochStart(ReclaimerState& s, uint64_t epoch, int64_t now) noexcept {
	s.epochStarts[s.epochStartsNext] = {epoch, now};
	s.epochStartsNext = (s.epochStartsNext + 1) % EPOCH_START_RING;
	s.epochStartsSize = std::min(s.epochStartsSize + 1, EPOCH_START_RING);
}

/** Time at which E became `epoch` (or the oldest known start if the ring no longer covers it). */
int64_t epochStartNanos(const ReclaimerState& s, uint64_t epoch) noexcept {
	int64_t oldest = INT64_MAX;
	uint64_t oldestEpoch = UINT64_MAX;
	for (size_t i = 0; i < s.epochStartsSize; ++i) {
		auto [knownEpoch, nanos] = s.epochStarts[i];
		if (knownEpoch == epoch)
			return nanos;
		if (knownEpoch < oldestEpoch) {
			oldestEpoch = knownEpoch;
			oldest = nanos;
		}
	}
	return oldest;
}

/** The scan's steps (1)-(4); may throw std::bad_alloc (see scan()). */
void scanSteps(ReclaimerState& s) {
	const int64_t now = commons::utils::nanoTime();

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

	// take the incoming stack. Everything that allocates happens before the first object is classified: decideObject has side effects on
	// `queued`, so an allocation failure in the middle of the classification could queue an object twice.
	AION_YIELD_POINT("Reclaimer::scan:take");
	RetireBatch* batch = s.incoming.exchange(nullptr, std::memory_order_acq_rel);
	size_t incomingItems = 0;
	for (RetireBatch* counted = batch; counted != nullptr; counted = counted->next)
		incomingItems += counted->items.size();
	reserveGeometric(s.queue, s.queue.size() + incomingItems);
	while (batch != nullptr) {
		s.incomingCount.fetch_sub(batch->items.size(), std::memory_order_acq_rel);
		s.incomingBytes.fetch_sub(batch->bytes, std::memory_order_relaxed);
		s.queue.insert(s.queue.end(), batch->items.begin(), batch->items.end()); // no reallocation: reserved
		RetireBatch* next = batch->next;
		delete batch;
		batch = next;
	}

	std::vector<std::optional<ObservedObject>> observed;
	if (!minFirst) {
		observed.resize(s.queue.size());
		for (size_t i = 0; i < s.queue.size(); ++i) {
			if (s.queue[i].kind() != OBJECT)
				continue;
			const RefCounted& object = *s.queue[i].object();
			AION_YIELD_POINT("Reclaimer::scan:readCount");
			uint32_t count = RefCountedAccess::count(object).load(std::memory_order_acquire);
			AION_YIELD_POINT("Reclaimer::scan:readStamp");
			observed[i] = ObservedObject{count, RefCountedAccess::retireEpoch(object).load(std::memory_order_acquire)};
			if (count != 0)
				observed[i].reset(); // only the zero-count decision is taken on stale values
		}
		computeMin();
	}

	// (3), (4) classify (no allocation from here on: the vectors are reserved for the whole queue)
	std::vector<RetiredItem>& keep = s.scratchKeep;
	std::vector<const RefCounted*>& destroyObjects = s.scratchDestroyObjects;
	std::vector<RetiredItem>& destroyOthers = s.scratchDestroyOthers;
	std::vector<const RefCounted*>& sorted = s.scratchSorted;
	keep.clear();
	destroyObjects.clear();
	destroyOthers.clear();
	sorted.clear();
	reserveGeometric(keep, s.queue.size());
	reserveGeometric(destroyObjects, s.queue.size());
	reserveGeometric(destroyOthers, s.queue.size());
	if (CHECKED)
		reserveGeometric(sorted, s.queue.size());
	uint64_t keepBytes = 0;
	for (size_t i = 0; i < s.queue.size(); ++i) {
		const RetiredItem& item = s.queue[i];
		bool destroy;
		bool keepItem;
		if (item.kind() == OBJECT) {
			Decision decision = decideObject(*item.object(), m, observed.empty() ? std::nullopt : observed[i]);
			destroy = decision == Decision::DESTROY;
			keepItem = decision == Decision::KEEP;
		} else if (item.kind() == COUNTED_PART) {
			destroy = decideCountedPart(item, m);
			keepItem = !destroy;
		} else {
			destroy = item.epoch < m;
			keepItem = !destroy;
		}
		if (destroy) {
			if (item.kind() == OBJECT)
				destroyObjects.push_back(item.object());
			else
				destroyOthers.push_back(item);
		} else if (keepItem) {
			keep.push_back(item);
			keepBytes += item.bytes;
		}
	}
	s.queue.swap(keep); // `keep` (the scratch vector) now holds the previous queue; it is cleared by the next scan
	if (CHECKED && destroyObjects.size() > 1) {
		sorted.assign(destroyObjects.begin(), destroyObjects.end());
		std::ranges::sort(sorted);
		AION_CHECK("C5", std::ranges::adjacent_find(sorted) == sorted.end(), "Reclaimer invariant: an object was queued twice (double retire)");
	}
	s.scannerBacklog.store(s.queue.size(), std::memory_order_relaxed);
	s.scannerBacklogBytes.store(keepBytes, std::memory_order_relaxed);

	// destroy
	{
		DestructorContext context;
		Reclaimer::DestroyObserver observer = s.destroyObserver.load(std::memory_order_acquire);
		for (const RefCounted* object : destroyObjects) {
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
		}
		for (const RetiredItem& item : destroyOthers) {
			AION_YIELD_POINT("Reclaimer::scan:destroy");
			if (item.kind() == COUNTED_PART)
				delete static_cast<OwnedPartBase*>(item.countedPart()); // OwnedPart destructors are protected; OwnedPartBase has a public virtual one
			else if (item.kind() == PART)
				delete item.part();
			else
				delete item.node();
		}
	}
	for (const RetiredItem& item : destroyOthers) {
		if (item.kind() == PART || item.kind() == COUNTED_PART)
			item.owner->release();
	}
	s.destroyed.fetch_add(destroyObjects.size() + destroyOthers.size(), std::memory_order_relaxed);
	releaseDelayedFree(s, s.delayedFreeLimit.load(std::memory_order_relaxed));

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
	}
	s.scans.fetch_add(1, std::memory_order_relaxed);
}

/**
 * One scan. The only exceptions scanSteps can throw are allocation failures (and std::system_error from the stats mutex); a scan interrupted
 * half-way would leave the queue inconsistent, so they terminate like the other out-of-memory paths of the Reclaimer (C5).
 */
void scan(ReclaimerState& s) noexcept {
	try {
		scanSteps(s);
	} catch (...) {
		checkFailed("C5", "Reclaimer: exception (out of memory) during a scan");
	}
}

/** Holds scanMutex and records the scanning thread (PCT: a contended wait is a blocking bracket). */
class ScanLock {
public:
	explicit ScanLock(ReclaimerState& s) : state_(s), lock_(s.scanMutex, std::try_to_lock) {
		if (!lock_.owns_lock()) {
			AION_PCT_BLOCKING_BEGIN("Reclaimer::reclaimNow");
			lock_.lock();
			AION_PCT_BLOCKING_END();
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
			AION_PCT_BLOCKING_BEGIN("Reclaimer::waitForRunningScan");
			lock.lock();
			AION_PCT_BLOCKING_END();
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

// ------------------------------------------------------------------------------------------------------------------------ thread and probe

void threadMain() {
	ReclaimerState& s = state();
	commons::utils::concurrent::setCurrentThreadName("Reclaimer");
	ThreadContext::current().refreshName();
	while (!s.stopRequested.load(std::memory_order_acquire)) {
		std::chrono::milliseconds period;
		{
			std::scoped_lock lock(s.configMutex);
			period = s.config.period;
		}
		{
			std::unique_lock lock(s.threadMutex);
			s.wakeCondition.wait_for(lock, period, [&] { return s.stopRequested.load() || s.wakeRequested.load(); });
		}
		s.wakeRequested.store(false, std::memory_order_release);
		if (s.stopRequested.load(std::memory_order_acquire))
			break;
		try {
			Reclaimer::getInstance().reclaimNow();
		} catch (...) {
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
	pushItem(RetiredItem{reinterpret_cast<uintptr_t>(&object) | OBJECT, nullptr, 0, objectBytes(object)});
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
	ReclaimerState& s = state();
	detail::flushThreadRetireList();
	ScanLock lock(s);
	scan(s);
	detail::flushThreadRetireList(); // cascades released by destructors
	runHooks(s);
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
	std::scoped_lock lock(s.statsMutex);
	stats.minActive = s.minActive;
	stats.lag = s.lag;
	stats.oldestPublishedTask = s.oldestTask;
	stats.oldestPublishedThreadId = s.oldestThreadId;
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
