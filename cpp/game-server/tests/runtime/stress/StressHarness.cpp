// Kernel stress harness v0 (design §12.4, §19 P2). See StressHarness.h for what it does and the pass criteria.
//
// Harness rules (so that every failure it reports is a kernel failure, not a harness bug):
// - A worker task publishes its epoch (TaskScope::ensurePublished) before it borrows anything, so a Ptr made from a Ref the task holds stays
//   valid after the Ref is dropped (the drop's release stamps an epoch >= the published one, design §2.4). Borrows never outlive their task.
// - Lock order: buckets stripe -> object monitor -> bucket list monitor; buckets stripe -> world stripe; nested stripe -> inner map stripe;
//   groups/list monitor -> object monitor. Nothing is acquired while an object monitor is held except a bucket list monitor. Stripe nesting on
//   one map only happens for keys of the same stripe (reentrant), never across stripes (that would be a real deadlock, collections report).
// - Retaining references from objects and parts point to lower ranks only (StressObjects.h), so no Ref cycles outlive a task run or cancel.
// - Exceptions logged by the pools (execute/schedule faults) are rate limited to one per LOGGED_FAULT_INTERVAL; faults of submit/deferred tasks,
//   compute callbacks, SYNCHRONIZED blocks, predicates and comparators are not logged and run at the configured rate.

#include "StressHarness.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <format>
#include <fstream>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <source_location>
#include <stdexcept>
#include <string>
#include <thread>
#include <typeinfo>
#include <vector>

#include "StressObjects.h"
#include "aion/gameserver/runtime/base/Finally.h"
#include "aion/gameserver/runtime/base/ThreadContext.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/lifetime/detail/Mutations.h"
#include "aion/gameserver/runtime/sched/ForkJoinPool.h"
#include "aion/gameserver/runtime/sched/PoolBackends.h"
#include "aion/gameserver/runtime/sched/SerialExecutor.h"
#include "aion/gameserver/runtime/services/CleanerQueue.h"
#include "aion/gameserver/runtime/services/LeakCensus.h"
#include "aion/gameserver/runtime/sync/BlockingRegion.h"
#include "aion/gameserver/runtime/sync/LockOrderValidator.h"
#include "aion/gameserver/runtime/sync/Watchdog.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::runtime::stress {

namespace {

using utils::ThreadPoolManager;
using SteadyClock = std::chrono::steady_clock;
using namespace std::chrono_literals;

constexpr const char* WORKER_KIND = "stress-worker";
constexpr const char* BORROW_FREE_KIND = "stress-borrow-free";
constexpr const char* CHECKER_KIND = "stress-checker";
constexpr std::chrono::milliseconds LOGGED_FAULT_INTERVAL{2000};
constexpr size_t MAX_RECORDED_MESSAGES = 64;
constexpr size_t MAX_BORROWS = 8;
constexpr int32_t MAX_POOL_BORROW_FREE_SLOTS = 64;

int64_t nowNanos() noexcept {
	return std::chrono::duration_cast<std::chrono::nanoseconds>(SteadyClock::now().time_since_epoch()).count();
}

/** Exception thrown at fault injection sites. */
class FaultInjected : public std::runtime_error {
public:
	using std::runtime_error::runtime_error;
};

/** xoshiro-style fast per-thread generator (splitmix64 seeding). */
class Rng {
public:
	explicit Rng(uint64_t seed) noexcept : state(seed ^ 0x9E3779B97F4A7C15ULL) {}
	uint64_t next() noexcept {
		uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
		z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
		z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
		return z ^ (z >> 31);
	}
	/** uniform in [0, bound) (bound > 0) */
	int32_t below(int32_t bound) noexcept { return static_cast<int32_t>(next() % static_cast<uint64_t>(bound > 0 ? bound : 1)); }
	/** true with probability permille / 1000 */
	bool chance(int32_t permille) noexcept { return below(1000) < permille; }

private:
	uint64_t state;
};

// ----------------------------------------------------------------------------------------------------------------------------- operations

enum class Op : uint8_t {
	CREATE,
	DROP,
	BORROW,
	VERIFY_BORROWS,
	RESURRECT,
	REPLACE_PART,
	SET_FIELDS,
	WORLD_ITERATE,
	LIST_OPS,
	MAP_COMPUTE,
	SET_OPS,
	COW_OPS,
	QUEUE_OPS,
	BUCKET_COMPUTE,
	NESTED_COMPUTE,
	ARRAY_OPS,
	ATOMIC_OPS,
	SCHEDULE,
	CANCEL,
	PERIODIC,
	SUBMIT_GET,
	DEFERRED,
	EXECUTE,
	REMOVE_FROM_WORLD,
	FORK_JOIN,
	SERIAL_EXECUTE,
	LINGER,
	// task kinds (counted once per task)
	TASK_MIXED,
	TASK_QUIESCENT,
	TASK_BLOCKED,
	TASK_BORROW_FREE,
	TASK_LONG_SUBMIT,
	COUNT,
};

constexpr size_t OP_COUNT = static_cast<size_t>(Op::COUNT);
constexpr size_t FIRST_TASK_KIND = static_cast<size_t>(Op::TASK_MIXED);

const char* opName(Op op) noexcept {
	static constexpr std::array<const char*, OP_COUNT> NAMES = {"create", "drop", "borrow", "verifyBorrows", "resurrect", "replacePart", "setFields",
		"worldIterate", "listOps", "mapCompute", "setOps", "cowOps", "queueOps", "bucketCompute", "nestedCompute", "arrayOps", "atomicOps", "schedule",
		"cancel", "periodic", "submitGet", "deferred", "execute", "removeFromWorld", "forkJoin", "serialExecute", "linger", "task:mixed", "task:quiescent", "task:blocked",
		"task:borrowFree", "task:longSubmit"};
	return NAMES[static_cast<size_t>(op)];
}

/** Relative weights of the operations inside a mixed task. */
struct WeightedOp {
	Op op;
	int32_t weight;
};
constexpr std::array<WeightedOp, FIRST_TASK_KIND> OP_WEIGHTS = {{
	{Op::CREATE, 120},
	{Op::DROP, 90},
	{Op::BORROW, 120},
	{Op::VERIFY_BORROWS, 40},
	{Op::RESURRECT, 40},
	{Op::REPLACE_PART, 50},
	{Op::SET_FIELDS, 100},
	{Op::WORLD_ITERATE, 15},
	{Op::LIST_OPS, 40},
	{Op::MAP_COMPUTE, 35},
	{Op::SET_OPS, 25},
	{Op::COW_OPS, 20},
	{Op::QUEUE_OPS, 20},
	{Op::BUCKET_COMPUTE, 45},
	{Op::NESTED_COMPUTE, 25},
	{Op::ARRAY_OPS, 40},
	{Op::ATOMIC_OPS, 25},
	{Op::SCHEDULE, 35},
	{Op::CANCEL, 20},
	{Op::PERIODIC, 8},
	{Op::SUBMIT_GET, 6},
	{Op::DEFERRED, 8},
	{Op::EXECUTE, 10},
	{Op::REMOVE_FROM_WORLD, 20},
	{Op::FORK_JOIN, 2},
	{Op::SERIAL_EXECUTE, 10},
	{Op::LINGER, 4},
}};

// ----------------------------------------------------------------------------------------------------------------------------- run state

struct WorkerState {
	std::atomic<const char*> op{"starting"};
	std::atomic<int64_t> opStartNanos{0};
	std::atomic<ThreadContext*> context{nullptr};
	/** odd while the worker runs a borrow-free task (after its scope was opened, before any load) */
	std::atomic<uint64_t> borrowFreeSeq{0};
	std::atomic<uint64_t> tasks{0};
	std::array<std::atomic<uint64_t>, OP_COUNT> counts{};

	void begin(Op kind) noexcept {
		opStartNanos.store(nowNanos(), std::memory_order_relaxed);
		op.store(opName(kind), std::memory_order_relaxed);
		counts[static_cast<size_t>(kind)].fetch_add(1, std::memory_order_relaxed);
	}
	void end() noexcept { op.store("between ops", std::memory_order_relaxed); }
};

/** Borrow-free long task on a pool thread, visible to the sampler (same seqlock protocol as WorkerState::borrowFreeSeq). */
struct BorrowFreeSlot {
	std::atomic<bool> used{false};
	std::atomic<ThreadContext*> context{nullptr};
	std::atomic<uint64_t> seq{0};
};

/** Thread-safe list of failure/warning messages (all counted, the first MAX_RECORDED_MESSAGES kept). */
class MessageLog {
public:
	void add(std::string message) {
		count.fetch_add(1, std::memory_order_relaxed);
		std::scoped_lock lock(mutex);
		if (messages.size() < MAX_RECORDED_MESSAGES)
			messages.push_back(std::move(message));
	}
	std::vector<std::string> snapshot() const {
		std::scoped_lock lock(mutex);
		return messages;
	}
	uint64_t total() const noexcept { return count.load(); }

private:
	mutable std::mutex mutex;
	std::vector<std::string> messages;
	std::atomic<uint64_t> count{0};
};

struct Metrics {
	std::atomic<uint64_t> resurrections{0};
	std::atomic<uint64_t> quiescentPoints{0};
	std::atomic<uint64_t> staleBorrowsDetected{0};
	std::atomic<uint64_t> futuresScheduled{0};
	std::atomic<uint64_t> futuresRan{0};
	std::atomic<uint64_t> futuresCancelled{0};
	std::atomic<uint64_t> periodicRuns{0};
	std::atomic<uint64_t> getTimeouts{0};
	std::atomic<uint64_t> faultsInjected{0};
	std::atomic<uint64_t> faultsCaught{0};
	std::atomic<uint64_t> recursiveUpdatesDetected{0};
	std::atomic<uint64_t> executionExceptions{0};
	std::atomic<uint64_t> cancellationExceptions{0};
	std::atomic<uint64_t> indexRaces{0};
	std::atomic<uint64_t> longTasks{0};
	std::atomic<uint64_t> blockedAcquires{0};
	std::atomic<uint64_t> borrowFreeSamples{0};
	std::atomic<uint64_t> borrowFreeStatsAttributions{0};
	std::atomic<uint64_t> longTaskStatsAttributions{0};
	std::atomic<uint64_t> throttledSubmissions{0};
};

struct RunState {
	explicit RunState(const StressConfig& config) : config(config) {}

	const StressConfig& config;
	uint64_t seed = 0;
	int32_t workerThreads = 0;
	/** set before the workers start, reset after they are joined; reached only through this const Ref (no read barrier) */
	Ref<StressHub> hub;
	std::atomic<bool> stop{false};
	std::atomic<uint32_t> nextId{1};
	std::atomic<uint64_t> serial{1};
	std::atomic<bool> throttleFutures{false};
	std::atomic<int32_t> longTasksRunning{0};
	std::atomic<int64_t> lastLoggedFaultNanos{0};
	std::vector<std::unique_ptr<WorkerState>> workers;
	std::array<BorrowFreeSlot, MAX_POOL_BORROW_FREE_SLOTS> poolBorrowFree{};
	std::source_location longTaskSite{};
	/** LS/CS-link-like serial executor: tasks must run one at a time in submission order */
	std::unique_ptr<SerialExecutor> serialExecutor;
	std::mutex serialSubmitMutex;
	uint64_t serialSubmitted = 0; // under serialSubmitMutex
	std::atomic<uint64_t> serialLastRun{0};
	std::atomic<bool> serialRunning{false};
	std::atomic<uint64_t> serialRuns{0};
	Metrics metrics;

	MessageLog failures;
	MessageLog warnings;
	std::atomic<uint64_t> canaryFailures{0};
	std::atomic<uint64_t> unexpectedExceptions{0};
	std::atomic<uint64_t> pinViolations{0};

	// sampler results (sampler thread only until it is joined)
	std::vector<int64_t> lagSamplesMillis;
	std::vector<uint64_t> backlogSamples;
	uint64_t maxBacklog = 0;
	uint64_t maxBacklogBytes = 0;
	MemorySample peakMemory{};

	// watchdog dumps
	std::array<std::atomic<uint64_t>, 6> dumpsByReason{};
	MessageLog dumpSummaries;

	StressHub& hubRef() const { return *hub; }

	void fail(std::string message) {
		std::fprintf(stderr, "[stress] FAILURE: %s\n", message.c_str());
		failures.add(std::move(message));
	}
	void canaryFailure(std::string message) {
		canaryFailures.fetch_add(1, std::memory_order_relaxed);
		fail(std::move(message));
		stop.store(true); // keep the evidence: stop the run, then tear down and report
	}
	void unexpected(const char* where, const std::exception& e) {
		unexpectedExceptions.fetch_add(1, std::memory_order_relaxed);
		fail(std::format("unexpected exception in {}: {}: {}", where, typeid(e).name(), e.what()));
	}
	/** true (and counted) if an injected fault should be thrown now; logged faults are rate limited */
	bool injectFault(Rng& rng, bool logged = false) {
		if (!rng.chance(config.faultPermille))
			return false;
		if (logged) {
			int64_t now = nowNanos();
			int64_t last = lastLoggedFaultNanos.load(std::memory_order_relaxed);
			if (now - last < std::chrono::duration_cast<std::chrono::nanoseconds>(LOGGED_FAULT_INTERVAL).count() ||
				!lastLoggedFaultNanos.compare_exchange_strong(last, now))
				return false;
		}
		metrics.faultsInjected.fetch_add(1, std::memory_order_relaxed);
		return true;
	}
	uint64_t nextSerial() noexcept { return serial.fetch_add(1, std::memory_order_relaxed); }
	int32_t newId() noexcept { return static_cast<int32_t>(nextId.fetch_add(1, std::memory_order_relaxed) & 0x7FFFFFFF); }
	int32_t worldKeyOf(const StressObject& object) const noexcept { return object.id % config.worldKeys; }
};

// ------------------------------------------------------------------------------------------------------------------------------ verification

void verifyObject(RunState& run, const StressObject& object, const char* where) {
	if (!object.intact()) [[unlikely]]
		run.canaryFailure(std::format("{}: StressObject canary mismatch at {} (use after free or corruption)", where, static_cast<void*>(
			const_cast<StressObject*>(&object))));
}

void verifyPart(RunState& run, const StressPart& part, const RefCounted& expectedOwner, const char* where) {
	if (!part.intact()) [[unlikely]] {
		run.canaryFailure(std::format("{}: StressPart canary mismatch at {} (use after free or corruption)", where, static_cast<void*>(
			const_cast<StressPart*>(&part))));
		return;
	}
	if (&part.partOwner() != &expectedOwner) [[unlikely]]
		run.fail(std::format("{}: part owner mismatch", where));
}

/** Verifies an object and everything reachable from its members (all loads publish; the caller is inside a task scope). */
void verifyDeep(RunState& run, Ptr<StressObject> object, const char* where) {
	verifyObject(run, *object, where);
	if (run.canaryFailures.load(std::memory_order_relaxed) != 0)
		return;
	(void)object->counter.get();
	(void)object->hits.get();
	(void)object->controller->ticks.get();
	if (!isIntactCheckedString(object->name.get())) [[unlikely]]
		run.canaryFailure(std::format("{}: Field<std::string> checksum mismatch (torn or freed string box)", where));
	if (Ptr<StressObject> link = object->link.get()) {
		verifyObject(run, *link, where);
		if (link->rank >= object->rank) [[unlikely]]
			run.fail(std::format("{}: link rank discipline violated ({} -> {})", where, object->rank, link->rank));
	}
	if (Ptr<StressObject> target = object->target.get()) {
		verifyObject(run, *target, where);
		if (target != object && target->rank >= object->rank) [[unlikely]]
			run.fail(std::format("{}: target rank discipline violated", where));
	}
	if (StressPart* part = object->part.get()) {
		verifyPart(run, *part, *object, where);
		if (Ptr<StressObject> ref = part->ref.get())
			verifyObject(run, *ref, where);
		if (Ptr<StressObject> actor = part->actor.get())
			verifyObject(run, *actor, where);
	}
	if (StressPart* ownerPart = object->ownerPart.get())
		verifyPart(run, *ownerPart, *object, where);
}

/**
 * Borrows kept for the rest of the task (valid until the task ends even if the objects are dropped meanwhile). Besides the object it keeps the
 * raw memory the task loaded through the object - its current part (PartSlot RECLAIMER, retired by replacement) and its name string box
 * (Field<std::string>, retired by assignment) - so epoch-retired parts and nodes freed too early are detected as well.
 */
class Borrows {
public:
	void add(Ptr<StressObject> object, Rng& rng) {
		if (!object)
			return;
		Item item{object, object->part.get(), &object->name.get()};
		if (items.size() < MAX_BORROWS)
			items.push_back(item);
		else
			items[static_cast<size_t>(rng.below(static_cast<int32_t>(MAX_BORROWS)))] = item;
	}
	/** a part of the hub loaded from its PartMap/PartList */
	void addHubPart(const StressPart* part, Rng& rng) {
		if (part == nullptr)
			return;
		if (hubParts.size() < MAX_BORROWS)
			hubParts.push_back(part);
		else
			hubParts[static_cast<size_t>(rng.below(static_cast<int32_t>(MAX_BORROWS)))] = part;
	}
	void verifyAll(RunState& run, const char* where) {
		for (const Item& item : items) {
			verifyDeep(run, item.object, where);
			if (item.part != nullptr && !item.part->intact())
				run.canaryFailure(std::format("{}: a borrowed StressPart (possibly replaced and retired since) was freed during the task", where));
			if (!isIntactCheckedString(*item.name))
				run.canaryFailure(std::format("{}: a borrowed Field<std::string> box (possibly replaced and retired since) was freed during the task", where));
		}
		for (const StressPart* part : hubParts)
			if (!part->intact())
				run.canaryFailure(std::format("{}: a borrowed hub part (PartMap/PartList) was freed during the task", where));
	}

private:
	struct Item {
		Ptr<StressObject> object;
		const StressPart* part;
		const std::string* name;
	};
	std::vector<Item> items;
	std::vector<const StressPart*> hubParts;
};

// ------------------------------------------------------------------------------------------------------------------------------ picking

Ptr<StressObject> pickObject(RunState& run, Rng& rng) {
	StressHub& hub = run.hubRef();
	switch (rng.below(8)) {
		case 0:
		case 1:
		case 2:
			return hub.slots[static_cast<size_t>(rng.below(hub.slotCount))].get();
		case 3:
		case 4:
			return hub.world.get(rng.below(run.config.worldKeys));
		case 5:
			return hub.array->get(rng.below(hub.array->length()));
		case 6:
			return hub.atomicRef.get();
		default: {
			int32_t size = hub.list.size();
			if (size == 0)
				return nullptr;
			try {
				return hub.list.get(rng.below(size));
			} catch (const IndexOutOfBoundsException&) {
				run.metrics.indexRaces.fetch_add(1, std::memory_order_relaxed); // Java semantics: the list shrank concurrently
				return nullptr;
			}
		}
	}
}

/** An object of a rank strictly below `rank` (null if none was found quickly). */
Ptr<StressObject> pickLowerRank(RunState& run, Rng& rng, int32_t rank) {
	if (rank <= 0)
		return nullptr;
	for (int attempt = 0; attempt < 3; ++attempt) {
		Ptr<StressObject> candidate = pickObject(run, rng);
		if (candidate && candidate->rank < rank)
			return candidate;
	}
	return nullptr;
}

void storeInWorld(RunState& run, Ptr<StressObject> object) {
	StressHub& hub = run.hubRef();
	Ptr<StressObject> previous = hub.world.put(run.worldKeyOf(*object), Ref<StressObject>(object));
	LeakCensus::getInstance().onAddedToWorld(*object);
	if (previous && previous != object) {
		verifyObject(run, *previous, "world.put previous");
		LeakCensus::getInstance().onRemovedFromWorld(*previous, "StressObject", previous->id);
	}
}

/** CreatureController.cancelAllTasks: breaks the object -> task -> capture cycle. */
void cancelAllTasks(RunState& run, StressObject& object) {
	for (const auto& entry : object.tasks.entrySet()) {
		if (entry.value->cancel())
			run.metrics.futuresCancelled.fetch_add(1, std::memory_order_relaxed);
	}
	object.tasks.clear();
}

/** A key of `map` guarded by the same stripe Monitor as `key` (for reentrant same-stripe nesting), -1 if none was found. */
template <class V>
int32_t sameStripeKey(const ConcurrentHashMap<int32_t, V>& map, int32_t key, Rng& rng, int32_t keySpace) {
	Monitor& stripe = map.stripeMonitor(key);
	for (int attempt = 0; attempt < 32; ++attempt) {
		int32_t other = rng.below(keySpace);
		if (other != key && &map.stripeMonitor(other) == &stripe)
			return other;
	}
	return -1;
}

// ------------------------------------------------------------------------------------------------------------------------------ operations

void opCreate(RunState& run, Rng& rng, Borrows& borrows) {
	StressHub& hub = run.hubRef();
	Ref<StressObject> object = StressObject::create(run.newId(), rng.below(RANKS));
	object->name = makeCheckedString(object->id, run.nextSerial(), static_cast<size_t>(rng.below(48)));
	if (Ptr<StressObject> lower = pickLowerRank(run, rng, object->rank))
		object->link = lower;
	if (rng.chance(300))
		object->target.set(object.borrow());
	switch (rng.below(8)) {
		case 0:
		case 1:
			storeInWorld(run, object.borrow());
			break;
		case 2:
		case 3:
			hub.slots[static_cast<size_t>(rng.below(hub.slotCount))].set(object);
			break;
		case 4:
			if (hub.list.size() < run.config.listCap)
				hub.list.add(object);
			break;
		case 5:
			(*hub.array)[rng.below(hub.array->length())].set(object);
			break;
		case 6:
			hub.atomicRef.set(object);
			break;
		default:
			if (hub.set.size() < 512)
				hub.set.add(object);
			break;
	}
	borrows.add(object.borrow(), rng);
}

void opDrop(RunState& run, Rng& rng) {
	StressHub& hub = run.hubRef();
	switch (rng.below(8)) {
		case 0:
		case 1:
			hub.slots[static_cast<size_t>(rng.below(hub.slotCount))].set(nullptr);
			break;
		case 2: {
			if (Ptr<StressObject> removed = hub.world.remove(rng.below(run.config.worldKeys))) {
				verifyObject(run, *removed, "world.remove");
				LeakCensus::getInstance().onRemovedFromWorld(*removed, "StressObject", removed->id);
			}
			break;
		}
		case 3: {
			int32_t size = hub.list.size();
			if (size > 0) {
				try {
					Ptr<StressObject> removed = hub.list.removeAt(rng.below(size));
					verifyObject(run, *removed, "list.removeAt");
				} catch (const IndexOutOfBoundsException&) {
					run.metrics.indexRaces.fetch_add(1, std::memory_order_relaxed);
				}
			}
			break;
		}
		case 4:
			(*hub.array)[rng.below(hub.array->length())].set(nullptr);
			break;
		case 5:
			(void)hub.atomicRef.getAndSet(nullptr);
			break;
		case 6:
			if (Ptr<StressObject> object = pickObject(run, rng))
				(void)hub.set.remove(object);
			break;
		default:
			if (Ptr<StressObject> object = pickObject(run, rng))
				object->link = nullptr;
			break;
	}
}

void opBorrow(RunState& run, Rng& rng, Borrows& borrows) {
	if (Ptr<StressObject> object = pickObject(run, rng)) {
		verifyDeep(run, object, "borrow");
		borrows.add(object, rng);
	}
}

/** Takes a borrow, unlinks the (possibly last) reference, then retains the borrowed object again (0 -> 1 is legal) and stores it elsewhere. */
void opResurrect(RunState& run, Rng& rng, Borrows& borrows) {
	StressHub& hub = run.hubRef();
	Ptr<StressObject> borrowed;
	if (rng.chance(500)) {
		Field<Ref<StressObject>>& slot = hub.slots[static_cast<size_t>(rng.below(hub.slotCount))];
		borrowed = slot.get();
		if (!borrowed)
			return;
		(void)slot.compareAndSet(borrowed, Ref<StressObject>()); // may be the last reference: count 0, queued for the Reclaimer
	} else {
		borrowed = hub.world.remove(rng.below(run.config.worldKeys));
		if (!borrowed)
			return;
		LeakCensus::getInstance().onRemovedFromWorld(*borrowed, "StressObject", borrowed->id);
	}
	verifyObject(run, *borrowed, "resurrect after unlink");
	if (borrowed->refCount() == 0)
		run.metrics.resurrections.fetch_add(1, std::memory_order_relaxed);
	Ref<StressObject> again(borrowed); // resurrection
	verifyDeep(run, again.borrow(), "resurrect after retain");
	if (rng.chance(500))
		(void)hub.slots[static_cast<size_t>(rng.below(hub.slotCount))].compareAndSet(nullptr, std::move(again));
	else
		storeInWorld(run, borrowed);
	borrows.add(borrowed, rng);
}

std::unique_ptr<StressPart> newPart(RunState& run, Rng& rng, const RefCounted& owner, int32_t ownerRank, Ptr<StressObject> self) {
	auto part = std::make_unique<StressPart>(owner, ownerRank, static_cast<int64_t>(run.nextSerial()));
	part->value = static_cast<int64_t>(rng.next() & 0xFFFF);
	if (ownerRank >= RANKS) {
		part->ref = pickObject(run, rng); // hub part: any object
		if (rng.chance(500))
			part->actor.set(pickObject(run, rng));
	} else {
		part->ref = pickLowerRank(run, rng, ownerRank);
		if (rng.chance(500) && self)
			part->actor.set(self); // the owner itself: stored tagged, non-retaining
		else
			part->actor.set(pickLowerRank(run, rng, ownerRank));
	}
	return part;
}

void opReplacePart(RunState& run, Rng& rng, Borrows& borrows) {
	StressHub& hub = run.hubRef();
	switch (rng.below(6)) {
		case 0:
		case 1: {
			Ptr<StressObject> object = pickObject(run, rng);
			if (!object)
				return;
			StressPart* before = object->part.get();
			if (before != nullptr)
				verifyPart(run, *before, *object, "part before replace");
			object->part.set(newPart(run, rng, *object, object->rank, object));
			if (before != nullptr)
				verifyPart(run, *before, *object, "retired part still borrowed"); // retired to the Reclaimer, must stay readable
			borrows.add(object, rng);
			break;
		}
		case 2: {
			Ptr<StressObject> object = pickObject(run, rng);
			if (object && object->ownerPartReplacements.get() < 3) {
				++object->ownerPartReplacements;
				object->ownerPart.set(newPart(run, rng, *object, object->rank, object));
			}
			break;
		}
		case 3: {
			int32_t key = rng.below(256);
			Ptr<StressPart> before = hub.partMap.get(key);
			hub.partMap.put(key, newPart(run, rng, hub, RANKS, nullptr));
			if (before) {
				verifyPart(run, *before, hub, "retired PartMap part still borrowed");
				borrows.addHubPart(before.rawPointer(), rng);
			}
			break;
		}
		case 4: {
			int32_t key = rng.below(256);
			Ptr<StressPart> before = hub.partMap.get(key);
			(void)hub.partMap.remove(key);
			if (before) {
				verifyPart(run, *before, hub, "removed PartMap part still borrowed");
				borrows.addHubPart(before.rawPointer(), rng);
				if (Ptr<StressObject> ref = before->ref.get())
					verifyObject(run, *ref, "removed PartMap part ref");
			}
			break;
		}
		default: {
			int32_t size = hub.partList.size();
			if (size < run.config.partListCap && rng.chance(100)) {
				(void)hub.partList.add(newPart(run, rng, hub, RANKS, nullptr));
			} else if (size > 0) {
				Ptr<StressPart> part = hub.partList.get(rng.below(size));
				verifyPart(run, *part, hub, "PartList get");
				part->ref = pickObject(run, rng);
				part->value += 1;
			}
			break;
		}
	}
}

void opSetFields(RunState& run, Rng& rng, Borrows& borrows) {
	Ptr<StressObject> object = pickObject(run, rng);
	if (!object)
		return;
	verifyObject(run, *object, "setFields");
	switch (rng.below(10)) {
		case 0:
			object->counter += 1;
			++object->counter;
			break;
		case 1: {
			int64_t value = object->counter.get();
			(void)object->counter.compareAndSet(value, value + 1);
			break;
		}
		case 2:
			object->name = makeCheckedString(object->id, run.nextSerial(), static_cast<size_t>(rng.below(64)));
			break;
		case 3:
			object->link = pickLowerRank(run, rng, object->rank);
			break;
		case 4: {
			Ptr<StressObject> current = object->link.get();
			Ptr<StressObject> next = pickLowerRank(run, rng, object->rank);
			(void)object->link.compareAndSet(current, Ref<StressObject>(next));
			(void)object->link.exchange(Ref<StressObject>(pickLowerRank(run, rng, object->rank)));
			break;
		}
		case 5:
			if (rng.chance(500))
				object->target.set(object);
			else
				(void)object->target.compareAndSet(object->target.get(), pickLowerRank(run, rng, object->rank));
			break;
		case 6:
			(void)object->hits.incrementAndGet();
			object->controller->ticks += 1;
			break;
		case 7: {
			Ptr<StressObject> lower = pickLowerRank(run, rng, object->rank);
			if (lower) {
				if (object->known.size() >= 8) {
					try {
						(void)object->known.removeAt(0);
					} catch (const IndexOutOfBoundsException&) {
						run.metrics.indexRaces.fetch_add(1, std::memory_order_relaxed);
					}
				}
				object->known.add(Ref<StressObject>(lower));
			}
			for (Ptr<StressObject> known : object->known) {
				verifyObject(run, *known, "known iteration");
				if (known->rank >= object->rank)
					run.fail("known rank discipline violated");
			}
			break;
		}
		case 8:
			if (object->ids.size() < 16)
				object->ids.add(rng.below(1000));
			else
				(void)object->ids.removeIf([bit = rng.below(2)](int32_t value) { return (value & 1) == bit; });
			break;
		default:
			if (Ptr<StressObject> known = pickObject(run, rng))
				(void)object->known.remove(known);
			break;
	}
	borrows.add(object, rng);
}

void opWorldIterate(RunState& run, Rng& rng) {
	StressHub& hub = run.hubRef();
	int32_t limit = 16 + rng.below(64);
	int32_t visited = 0;
	if (rng.chance(500)) {
		for (const auto& entry : hub.world.entrySet()) {
			verifyObject(run, *entry.value, "world entry iteration");
			if (run.worldKeyOf(*entry.value) != entry.key) [[unlikely]]
				run.fail(std::format("world map key mismatch: key {} holds id {}", entry.key, entry.value->id));
			if (++visited >= limit)
				break;
		}
	} else {
		for (Ptr<StressObject> value : hub.world.values()) {
			verifyObject(run, *value, "world value iteration");
			if (++visited >= limit)
				break;
		}
		(void)hub.world.containsKey(rng.below(run.config.worldKeys));
	}
}

void opListOps(RunState& run, Rng& rng) {
	StressHub& hub = run.hubRef();
	switch (rng.below(9)) {
		case 0:
		case 1:
			if (Ptr<StressObject> object = pickObject(run, rng); object && hub.list.size() < run.config.listCap)
				hub.list.add(Ref<StressObject>(object));
			break;
		case 2: {
			int32_t visited = 0;
			for (Ptr<StressObject> object : hub.list) {
				verifyObject(run, *object, "list snapshot iteration");
				if (++visited >= 256)
					break;
			}
			break;
		}
		case 3: {
			bool fault = run.injectFault(rng);
			int32_t pattern = rng.below(64);
			try {
				(void)hub.list.removeIf([&](const Ptr<StressObject>& object) {
					verifyObject(run, *object, "list removeIf predicate");
					if (fault)
						throw FaultInjected("removeIf predicate fault");
					return (object->id & 63) == pattern;
				});
			} catch (const FaultInjected&) {
				if (hub.list.monitor().isHeldByCurrentThread())
					run.fail("list Monitor still held after a throwing removeIf predicate");
				throw;
			}
			break;
		}
		case 4: {
			if (!rng.chance(100))
				break;
			// throwing comparator: the list must keep every element (the ArrayList::sort element loss found by this harness is fixed;
			// StressReproducerTest.ArrayListSortWithThrowingComparatorKeepsAllElements)
			bool fault = run.injectFault(rng);
			int32_t compares = 0;
			try {
				hub.list.sort([&](const Ptr<StressObject>& a, const Ptr<StressObject>& b) -> int32_t {
					int32_t ia = 0;
					int32_t ib = 0;
					SYNCHRONIZED(*a) { // comparator takes Monitors under the collection Monitor (lockdep-tracked)
						ia = a->id;
					}
					SYNCHRONIZED(*b) {
						ib = b->id;
					}
					if (fault && ++compares == 64)
						throw FaultInjected("comparator fault");
					return ia < ib ? -1 : (ia > ib ? 1 : 0);
				});
			} catch (const FaultInjected&) {
				if (hub.list.monitor().isHeldByCurrentThread())
					run.fail("list Monitor still held after a throwing comparator");
				throw;
			}
			break;
		}
		case 5: {
			auto it = hub.list.iterator();
			int32_t visited = 0;
			while (it.hasNext() && visited++ < 32) {
				Ptr<StressObject> object = it.next();
				verifyObject(run, *object, "list JavaIterator");
				if ((object->id & 7) == 0)
					it.remove();
			}
			break;
		}
		case 6:
			if (Ptr<StressObject> object = pickObject(run, rng)) {
				(void)hub.list.contains(object);
				(void)hub.list.indexOf(object);
			}
			break;
		case 7: {
			int32_t size = hub.list.size();
			if (size > 0) {
				try {
					Ptr<StressObject> object = hub.list.get(rng.below(size));
					verifyObject(run, *object, "list get");
					(void)hub.list.set(rng.below(size), Ref<StressObject>(object));
				} catch (const IndexOutOfBoundsException&) {
					run.metrics.indexRaces.fetch_add(1, std::memory_order_relaxed);
				}
			}
			break;
		}
		default: {
			bool fault = run.injectFault(rng);
			try {
				SYNCHRONIZED(hub.list) { // synchronized (list) { ... } with a throw inside
					if (hub.list.size() > 0)
						verifyObject(run, *hub.list.get(0), "list synchronized get");
					if (fault)
						throw FaultInjected("fault inside SYNCHRONIZED(list)");
				}
			} catch (const FaultInjected&) {
				if (hub.list.monitor().isHeldByCurrentThread())
					run.fail("list Monitor still held after an exception left SYNCHRONIZED");
				throw;
			}
			break;
		}
	}
}

void opMapCompute(RunState& run, Rng& rng) {
	StressHub& hub = run.hubRef();
	int32_t key = rng.below(512);
	Ptr<StressObject> candidate = pickObject(run, rng);
	bool fault = run.injectFault(rng);
	bool recursion = rng.chance(20);
	bool remove = rng.chance(100);
	try {
		switch (rng.below(4)) {
			case 0:
			case 1:
				(void)hub.groups.compute(key, [&](const int32_t&, Ptr<StressObject> old) -> Ref<StressObject> {
					if (old)
						verifyObject(run, *old, "groups compute old");
					if (candidate) {
						SYNCHRONIZED(*candidate) { // HashMap Monitor -> object monitor
							candidate->counter += 1;
							if (fault)
								throw FaultInjected("fault inside compute + SYNCHRONIZED");
						}
					}
					if (recursion) {
						try {
							(void)hub.groups.put(key, Ref<StressObject>(old ? old : candidate));
							if (old || candidate)
								run.fail("HashMap same-key write inside compute did not throw");
						} catch (const IllegalStateException&) {
							run.metrics.recursiveUpdatesDetected.fetch_add(1, std::memory_order_relaxed);
						} catch (const NullPointerException&) {
						}
					}
					if (remove)
						return nullptr;
					return Ref<StressObject>(candidate ? candidate : old);
				});
				break;
			case 2:
				(void)hub.groups.computeIfAbsent(key, [&](const int32_t&) -> Ref<StressObject> {
					if (fault)
						throw FaultInjected("computeIfAbsent fault");
					return Ref<StressObject>(candidate);
				});
				break;
			default:
				if (candidate)
					(void)hub.groups.merge(key, Ref<StressObject>(candidate), [&](Ptr<StressObject> old, Ptr<StressObject> given) -> Ref<StressObject> {
						verifyObject(run, *old, "groups merge old");
						if (fault)
							throw FaultInjected("merge fault");
						return Ref<StressObject>(remove ? nullptr : given);
					});
				break;
		}
	} catch (const FaultInjected&) {
		if (hub.groups.monitor().isHeldByCurrentThread() || (candidate && candidate->monitor().isHeldByCurrentThread()))
			run.fail("Monitor still held after a throwing HashMap compute callback");
		throw;
	}
	if (rng.chance(100)) {
		int32_t visited = 0;
		for (Ptr<StressObject> value : hub.groups.values()) {
			verifyObject(run, *value, "groups values");
			if (++visited >= 128)
				break;
		}
	}
}

void opSetOps(RunState& run, Rng& rng) {
	StressHub& hub = run.hubRef();
	Ptr<StressObject> object = pickObject(run, rng);
	switch (rng.below(4)) {
		case 0:
			if (object && hub.set.size() < 512)
				(void)hub.set.add(Ref<StressObject>(object));
			break;
		case 1:
			if (object)
				(void)hub.set.remove(object);
			break;
		case 2:
			if (object)
				(void)hub.set.contains(object);
			break;
		default: {
			int32_t visited = 0;
			for (Ptr<StressObject> element : hub.set) {
				verifyObject(run, *element, "set iteration");
				if (++visited >= 128)
					break;
			}
			break;
		}
	}
}

void opCowOps(RunState& run, Rng& rng) {
	StressHub& hub = run.hubRef();
	switch (rng.below(4)) {
		case 0:
			if (Ptr<StressObject> object = pickObject(run, rng); object && hub.observers.size() < run.config.cowCap)
				(void)hub.observers.add(Ref<StressObject>(object));
			break;
		case 1:
			if (Ptr<StressObject> object = pickObject(run, rng))
				(void)hub.observers.remove(object);
			break;
		case 2: {
			bool fault = run.injectFault(rng);
			int32_t pattern = rng.below(16);
			try {
				(void)hub.observers.removeIf([&](const Ptr<StressObject>& object) {
					if (fault)
						throw FaultInjected("CopyOnWriteArrayList removeIf fault");
					return (object->id & 15) == pattern;
				});
			} catch (const FaultInjected&) {
				if (hub.observers.monitor().isHeldByCurrentThread())
					run.fail("CopyOnWriteArrayList Monitor still held after a throwing predicate");
				throw;
			}
			break;
		}
		default:
			for (Ptr<StressObject> observer : hub.observers) // lock-free in-place iteration
				verifyObject(run, *observer, "observers iteration");
			break;
	}
}

void opQueueOps(RunState& run, Rng& rng) {
	StressHub& hub = run.hubRef();
	switch (rng.below(3)) {
		case 0:
			if (Ptr<StressObject> object = pickObject(run, rng); object && hub.queue.size() < 512)
				(void)hub.queue.offer(Ref<StressObject>(object));
			break;
		case 1:
			if (Ptr<StressObject> polled = hub.queue.poll())
				verifyObject(run, *polled, "queue poll");
			break;
		default:
			if (Ptr<StressObject> peeked = hub.queue.peek())
				verifyObject(run, *peeked, "queue peek");
			break;
	}
}

/** ConcurrentHashMap compute whose callback takes Monitors, nests same-stripe and other-map operations, blocks, recurses and throws (RR-1). */
void opBucketCompute(RunState& run, Rng& rng) {
	StressHub& hub = run.hubRef();
	Ptr<StressObject> candidate = pickObject(run, rng);
	if (!candidate)
		return;
	int32_t key = rng.below(256);
	bool fault = run.injectFault(rng);
	bool nestSameStripe = rng.chance(150);
	bool nestWorld = rng.chance(100);
	bool blockInside = rng.chance(10);
	bool recursion = rng.chance(20);
	bool remove = rng.chance(30);
	int32_t sameStripe = nestSameStripe ? sameStripeKey(hub.buckets, key, rng, 256) : -1;
	try {
		(void)hub.buckets.compute(key, [&](const int32_t& k, Ptr<StressList> existing) -> Ref<StressList> {
			Ref<StressList> list(existing);
			if (!list)
				list = StressList::create(AION_LOCK_CLASS(StressHub::bucketList));
			SYNCHRONIZED(*candidate) { // stripe Monitor -> object monitor
				candidate->counter += 1;
				SYNCHRONIZED(*list) { // -> bucket list monitor
					list->add(Ref<StressObject>(candidate));
					if (list->size() > 32)
						(void)list->removeAt(0);
				}
			}
			if (sameStripe >= 0) { // reentrant stripe Monitor, different key (CreatureGameStats/SpawnsData pattern on one stripe)
				(void)hub.buckets.computeIfPresent(sameStripe, [&](Ptr<StressList> other) -> Ref<StressList> {
					for (Ptr<StressObject> element : *other)
						verifyObject(run, *element, "same-stripe nested bucket");
					return Ref<StressList>(other);
				});
			}
			if (nestWorld) { // buckets stripe -> world stripe (PlayerContainer-like nesting on another map)
				if (hub.world.putIfAbsent(run.worldKeyOf(*candidate), Ref<StressObject>(candidate)) == nullptr)
					LeakCensus::getInstance().onAddedToWorld(*candidate);
			}
			if (blockInside) { // LegionService: DAO call inside computeIfAbsent
				BlockingRegion region("stress DAO inside compute");
				std::this_thread::sleep_for(std::chrono::microseconds(rng.below(500)));
			}
			if (recursion) {
				try {
					(void)hub.buckets.put(k, list);
					run.fail("ConcurrentHashMap same-key write inside compute did not throw");
				} catch (const IllegalStateException&) {
					run.metrics.recursiveUpdatesDetected.fetch_add(1, std::memory_order_relaxed);
				}
			}
			if (fault)
				throw FaultInjected("fault inside ConcurrentHashMap compute");
			return remove ? Ref<StressList>() : list;
		});
	} catch (const FaultInjected&) {
		if (hub.buckets.stripeMonitor(key).isHeldByCurrentThread() || candidate->monitor().isHeldByCurrentThread())
			run.fail("Monitor still held after a throwing ConcurrentHashMap compute callback");
		throw;
	}
	if (Ptr<StressList> list = hub.buckets.get(key)) {
		int32_t visited = 0;
		for (Ptr<StressObject> element : *list) {
			verifyObject(run, *element, "bucket list iteration");
			if (++visited >= 32)
				break;
		}
	}
}

/** SpawnsData.java:83-96: nested compute on inner maps inside an outer compute. */
void opNestedCompute(RunState& run, Rng& rng) {
	StressHub& hub = run.hubRef();
	int32_t outer = rng.below(64);
	int32_t inner = rng.below(64);
	Ptr<StressObject> candidate = pickObject(run, rng);
	bool fault = run.injectFault(rng);
	bool removeInner = rng.chance(200);
	try {
		(void)hub.nested.compute(outer, [&](const int32_t&, Ptr<StressInnerMap> existing) -> Ref<StressInnerMap> {
			Ref<StressInnerMap> map(existing);
			if (!map)
				map = StressInnerMap::create(AION_LOCK_CLASS(StressHub::nestedInner#stripe));
			(void)map->compute(inner, [&](const int32_t&, Ptr<StressObject> old) -> Ref<StressObject> {
				if (old)
					verifyObject(run, *old, "nested inner old");
				if (fault)
					throw FaultInjected("fault inside nested inner compute");
				if (removeInner)
					return nullptr;
				return Ref<StressObject>(candidate ? candidate : old);
			});
			return map;
		});
	} catch (const FaultInjected&) {
		if (hub.nested.stripeMonitor(outer).isHeldByCurrentThread())
			run.fail("outer stripe Monitor still held after a throwing nested compute");
		throw;
	}
	if (Ptr<StressInnerMap> map = hub.nested.get(outer)) {
		for (Ptr<StressObject> value : map->values())
			verifyObject(run, *value, "nested inner values");
	}
}

void opArrayOps(RunState& run, Rng& rng) {
	StressHub& hub = run.hubRef();
	Array<Ref<StressObject>>& array = *hub.array;
	Field<Ref<StressObject>>& slot = array[rng.below(array.length())];
	switch (rng.below(4)) {
		case 0:
			slot.set(pickObject(run, rng));
			break;
		case 1:
			if (Ptr<StressObject> object = slot.get())
				verifyObject(run, *object, "array slot");
			break;
		case 2: {
			Ptr<StressObject> current = slot.get();
			(void)slot.compareAndSet(current, Ref<StressObject>(pickObject(run, rng)));
			break;
		}
		default:
			if (rng.chance(50)) {
				for (Ptr<StressObject> object : array.snapshot())
					if (object)
						verifyObject(run, *object, "array snapshot");
			} else {
				Ref<StressObject> previous = slot.exchange(nullptr);
				if (previous)
					verifyObject(run, *previous, "array exchange");
			}
			break;
	}
}

void opAtomicOps(RunState& run, Rng& rng) {
	StressHub& hub = run.hubRef();
	switch (rng.below(3)) {
		case 0: {
			Ref<StressObject> previous = hub.atomicRef.getAndSet(Ref<StressObject>(pickObject(run, rng)));
			if (previous)
				verifyObject(run, *previous, "atomicRef getAndSet");
			break;
		}
		case 1: {
			Ptr<StressObject> current = hub.atomicRef.get();
			(void)hub.atomicRef.compareAndSet(current, Ref<StressObject>(pickObject(run, rng)));
			break;
		}
		default: {
			Ptr<StressObject> replacement = pickObject(run, rng);
			Ptr<StressObject> updated = hub.atomicRef.updateAndGet([&](Ptr<StressObject> previous) {
				if (previous)
					verifyObject(run, *previous, "atomicRef updateAndGet");
				return replacement;
			});
			if (updated)
				verifyObject(run, *updated, "atomicRef updated");
			break;
		}
	}
}

/** Body of scheduled/executed/submitted tasks: runs on the pools in their TaskScope; loads publish there. */
void taskBody(RunState& run, StressObject& target, bool fault, const char* what) {
	run.metrics.futuresRan.fetch_add(1, std::memory_order_relaxed);
	verifyObject(run, target, what);
	target.counter += 1;
	target.controller->ticks += 1;
	if (Ptr<StressObject> link = target.link.get())
		verifyObject(run, *link, what);
	if (StressPart* part = target.part.get())
		verifyPart(run, *part, target, what);
	if (fault)
		throw FaultInjected(std::string("injected fault in ") + what);
}

void opSchedule(RunState& run, Rng& rng) {
	if (run.throttleFutures.load(std::memory_order_relaxed)) {
		run.metrics.throttledSubmissions.fetch_add(1, std::memory_order_relaxed);
		return;
	}
	Ptr<StressObject> picked = pickObject(run, rng);
	if (!picked)
		return;
	Ref<StressObject> object(picked);
	int32_t taskId = rng.below(4);
	int64_t delay = rng.below(50);
	bool fault = run.injectFault(rng, true);
	ThreadPoolManager& pool = ThreadPoolManager::getInstance();
	// CreatureController.addTask: compute replaces and cancels the previous task and schedules inside the callback (Preview.java:211-226)
	(void)object->tasks.compute(taskId, [&](const int32_t&, Ptr<Future> previous) -> FutureRef {
		if (previous && previous->cancel())
			run.metrics.futuresCancelled.fetch_add(1, std::memory_order_relaxed);
		run.metrics.futuresScheduled.fetch_add(1, std::memory_order_relaxed);
		if (rng.chance(500)) {
			StressController* controller = object->controller.get(); // pin through a part: retains the owner
			return pool.schedule(Pin(controller), [&run, target = object, fault] { taskBody(run, *target, fault, "scheduled task"); }, delay);
		}
		return pool.schedule(Pin(object), [&run, target = object, fault] { taskBody(run, *target, fault, "scheduled task"); }, delay);
	});
}

void opCancel(RunState& run, Rng& rng) {
	Ptr<StressObject> object = pickObject(run, rng);
	if (!object)
		return;
	int32_t taskId = rng.below(6);
	if (Ptr<Future> future = object->tasks.get(taskId)) {
		(void)future->getDelay();
		if (future->cancel()) {
			run.metrics.futuresCancelled.fetch_add(1, std::memory_order_relaxed);
			if (!future->isCancelled())
				run.fail("Future::cancel returned true but isCancelled() is false");
		}
		if (rng.chance(300))
			(void)object->tasks.remove(taskId, future);
	}
}

void opPeriodic(RunState& run, Rng& rng) {
	if (run.throttleFutures.load(std::memory_order_relaxed)) {
		run.metrics.throttledSubmissions.fetch_add(1, std::memory_order_relaxed);
		return;
	}
	Ptr<StressObject> picked = pickObject(run, rng);
	if (!picked)
		return;
	Ref<StressObject> object(picked);
	int32_t taskId = 4 + rng.below(2);
	int64_t delay = rng.below(20);
	int64_t period = 5 + rng.below(45);
	int32_t limit = 2 + rng.below(10);
	bool fault = run.injectFault(rng, true);
	ThreadPoolManager& pool = ThreadPoolManager::getInstance();
	(void)object->tasks.compute(taskId, [&](const int32_t&, Ptr<Future> previous) -> FutureRef {
		if (previous && previous->cancel())
			run.metrics.futuresCancelled.fetch_add(1, std::memory_order_relaxed);
		run.metrics.futuresScheduled.fetch_add(1, std::memory_order_relaxed);
		return pool.scheduleAtFixedRate(
			Pin(object),
			[&run, target = object, fault, limit, runs = 0](Future& self) mutable {
				run.metrics.periodicRuns.fetch_add(1, std::memory_order_relaxed);
				if (++runs >= limit)
					(void)self.cancel(); // bounded: releases the captures (breaks object -> task -> object)
				taskBody(run, *target, fault && runs == 1, "periodic task"); // periodic tasks keep running after an exception
			},
			delay, period);
	});
}

void opSubmitGet(RunState& run, Rng& rng) {
	Ptr<StressObject> picked = pickObject(run, rng);
	if (!picked)
		return;
	Ref<StressObject> object(picked);
	bool fault = run.injectFault(rng);
	ThreadPoolManager& pool = ThreadPoolManager::getInstance();
	run.metrics.futuresScheduled.fetch_add(1, std::memory_order_relaxed);
	FutureRef future = rng.chance(200)
		? pool.submitLongRunning(Pin(object), [&run, target = object, fault] { taskBody(run, *target, fault, "submitted long-running task"); })
		: pool.submit(Pin(object), [&run, target = object, fault] { taskBody(run, *target, fault, "submitted task"); });
	try {
		future->get(250, TimeUnit::MILLISECONDS); // BlockingRegion("Future.get")
		if (fault)
			run.fail("Future::get returned normally for a task that threw");
	} catch (const ExecutionException&) {
		run.metrics.executionExceptions.fetch_add(1, std::memory_order_relaxed);
		if (!fault)
			run.fail("Future::get threw ExecutionException for a task that did not throw");
	} catch (const TimeoutException&) {
		run.metrics.getTimeouts.fetch_add(1, std::memory_order_relaxed);
		if (future->cancel())
			run.metrics.futuresCancelled.fetch_add(1, std::memory_order_relaxed);
	} catch (const CancellationException&) {
		run.metrics.cancellationExceptions.fetch_add(1, std::memory_order_relaxed);
	}
}

void opDeferred(RunState& run, Rng& rng) {
	Ptr<StressObject> picked = pickObject(run, rng);
	if (!picked)
		return;
	Ref<StressObject> object(picked);
	bool fault = run.injectFault(rng);
	FutureRef deferred = Future::deferred(Pin(object), [&run, target = object, fault] { taskBody(run, *target, fault, "deferred task"); });
	if (rng.chance(700)) { // CM_TELEPORT_ANIMATION_DONE: task.run(); task.get();
		deferred->run();
		try {
			deferred->get();
			if (fault)
				run.fail("deferred get returned normally for a task that threw");
		} catch (const ExecutionException&) {
			run.metrics.executionExceptions.fetch_add(1, std::memory_order_relaxed);
			if (!fault)
				run.fail("deferred get threw ExecutionException for a task that did not throw");
		}
	} else {
		if (!deferred->cancel())
			run.fail("cancel of a pending deferred Future returned false");
		try {
			deferred->get();
			run.fail("get of a cancelled Future did not throw");
		} catch (const CancellationException&) {
			run.metrics.cancellationExceptions.fetch_add(1, std::memory_order_relaxed);
		}
	}
}

void opExecute(RunState& run, Rng& rng) {
	if (run.throttleFutures.load(std::memory_order_relaxed)) {
		run.metrics.throttledSubmissions.fetch_add(1, std::memory_order_relaxed);
		return;
	}
	Ptr<StressObject> picked = pickObject(run, rng);
	if (!picked)
		return;
	Ref<StressObject> object(picked);
	bool fault = run.injectFault(rng, true);
	run.metrics.futuresScheduled.fetch_add(1, std::memory_order_relaxed);
	ThreadPoolManager::getInstance().execute(Pin(object), [&run, target = object, fault] { taskBody(run, *target, fault, "executed task"); });
}

void opRemoveFromWorld(RunState& run, Rng& rng) {
	StressHub& hub = run.hubRef();
	Ptr<StressObject> removed = hub.world.remove(rng.below(run.config.worldKeys));
	if (!removed)
		return;
	verifyObject(run, *removed, "removeFromWorld");
	LeakCensus::getInstance().onRemovedFromWorld(*removed, "StressObject", removed->id);
	cancelAllTasks(run, *removed); // onDelete -> cancelAllTasks
}

/** parallelForEach with JOIN (helpers adopt the caller's scope: borrows stay valid) and PER_ELEMENT (each element in its own scope). */
void opForkJoin(RunState& run, Rng& rng, Borrows& borrows) {
	StressHub& hub = run.hubRef();
	bool fault = run.injectFault(rng);
	int32_t limit = 4 + rng.below(28);
	std::atomic<int32_t> processed{0};
	try {
		if (rng.chance(500)) {
			std::vector<Ptr<StressObject>> items;
			for (Ptr<StressObject> value : hub.world.values()) {
				items.push_back(value);
				if (static_cast<int32_t>(items.size()) >= limit)
					break;
			}
			ForkJoinPool::commonPool().parallelForEach(items, [&](const Ptr<StressObject>& item) {
				verifyDeep(run, item, "fork-join JOIN element (caller's borrow on a helper)");
				item->counter += 1;
				if (fault && processed.fetch_add(1) == 1)
					throw FaultInjected("fault in a JOIN fork-join element");
			});
			for (const Ptr<StressObject>& item : items)
				borrows.add(item, rng);
		} else {
			std::vector<Ref<StressObject>> items;
			for (Ptr<StressObject> value : hub.world.values()) {
				items.emplace_back(value);
				if (static_cast<int32_t>(items.size()) >= limit)
					break;
			}
			ForkJoinPool::commonPool().parallelForEach(
				items,
				[&](const Ref<StressObject>& item) {
					verifyDeep(run, item.borrow(), "fork-join PER_ELEMENT element");
					if (fault && processed.fetch_add(1) == 1)
						throw FaultInjected("fault in a PER_ELEMENT fork-join element");
				},
				Isolation::PER_ELEMENT);
		}
	} catch (const FaultInjected&) {
		run.metrics.faultsCaught.fetch_add(1, std::memory_order_relaxed);
	}
}

void serialBody(RunState& run, StressObject& target, uint64_t sequence) {
	if (run.serialRunning.exchange(true))
		run.fail("SerialExecutor ran two tasks at the same time");
	uint64_t last = run.serialLastRun.exchange(sequence);
	if (sequence != last + 1)
		run.fail(std::format("SerialExecutor order violated: task {} ran after task {}", sequence, last));
	run.serialRuns.fetch_add(1, std::memory_order_relaxed);
	verifyObject(run, target, "serial task");
	if (Ptr<StressObject> link = target.link.get())
		verifyObject(run, *link, "serial task link");
	run.serialRunning.store(false);
}

void opSerialExecute(RunState& run, Rng& rng) {
	if (run.throttleFutures.load(std::memory_order_relaxed)) {
		run.metrics.throttledSubmissions.fetch_add(1, std::memory_order_relaxed);
		return;
	}
	Ptr<StressObject> picked = pickObject(run, rng);
	if (!picked)
		return;
	Ref<StressObject> object(picked);
	std::scoped_lock lock(run.serialSubmitMutex); // sequence numbers in submission order
	uint64_t sequence = ++run.serialSubmitted;
	run.serialExecutor->execute(Pin(object), [&run, target = object, sequence] { serialBody(run, *target, sequence); });
}

/**
 * Keeps the task's borrows (objects, parts, string boxes) across Reclaimer scans: sleeps up to 25 ms (longer than the 20 ms scan period) and
 * iterates the world map in place slowly, so memory retired meanwhile by other threads (objects, parts, ConcurrentHashMap nodes and tables)
 * would be freed under the borrow if the epoch protocol let it.
 */
void opLinger(RunState& run, Rng& rng, Borrows& borrows) {
	StressHub& hub = run.hubRef();
	borrows.verifyAll(run, "linger before");
	quiescentPoint(); // no QuiescentScope here: a warned no-op (C16) that must keep every borrow of the task valid (RR-15)
	if (rng.chance(500)) {
		int32_t visited = 0;
		for (const auto& entry : hub.world.entrySet()) {
			verifyObject(run, *entry.value, "slow world iteration");
			std::this_thread::sleep_for(std::chrono::microseconds(500 + rng.below(3000)));
			if (++visited >= 8)
				break;
		}
	} else {
		std::this_thread::sleep_for(std::chrono::milliseconds(1 + rng.below(25)));
	}
	borrows.verifyAll(run, "linger after");
}

void runOp(RunState& run, Op op, Rng& rng, Borrows& borrows) {
	switch (op) {
		case Op::CREATE:
			return opCreate(run, rng, borrows);
		case Op::DROP:
			return opDrop(run, rng);
		case Op::BORROW:
			return opBorrow(run, rng, borrows);
		case Op::VERIFY_BORROWS:
			return borrows.verifyAll(run, "verify borrows");
		case Op::RESURRECT:
			return opResurrect(run, rng, borrows);
		case Op::REPLACE_PART:
			return opReplacePart(run, rng, borrows);
		case Op::SET_FIELDS:
			return opSetFields(run, rng, borrows);
		case Op::WORLD_ITERATE:
			return opWorldIterate(run, rng);
		case Op::LIST_OPS:
			return opListOps(run, rng);
		case Op::MAP_COMPUTE:
			return opMapCompute(run, rng);
		case Op::SET_OPS:
			return opSetOps(run, rng);
		case Op::COW_OPS:
			return opCowOps(run, rng);
		case Op::QUEUE_OPS:
			return opQueueOps(run, rng);
		case Op::BUCKET_COMPUTE:
			return opBucketCompute(run, rng);
		case Op::NESTED_COMPUTE:
			return opNestedCompute(run, rng);
		case Op::ARRAY_OPS:
			return opArrayOps(run, rng);
		case Op::ATOMIC_OPS:
			return opAtomicOps(run, rng);
		case Op::SCHEDULE:
			return opSchedule(run, rng);
		case Op::CANCEL:
			return opCancel(run, rng);
		case Op::PERIODIC:
			return opPeriodic(run, rng);
		case Op::SUBMIT_GET:
			return opSubmitGet(run, rng);
		case Op::DEFERRED:
			return opDeferred(run, rng);
		case Op::EXECUTE:
			return opExecute(run, rng);
		case Op::REMOVE_FROM_WORLD:
			return opRemoveFromWorld(run, rng);
		case Op::FORK_JOIN:
			return opForkJoin(run, rng, borrows);
		case Op::SERIAL_EXECUTE:
			return opSerialExecute(run, rng);
		case Op::LINGER:
			return opLinger(run, rng, borrows);
		default:
			return;
	}
}

Op pickOp(Rng& rng) {
	static const int32_t totalWeight = [] {
		int32_t total = 0;
		for (const WeightedOp& entry : OP_WEIGHTS)
			total += entry.weight;
		return total;
	}();
	int32_t roll = rng.below(totalWeight);
	for (const WeightedOp& entry : OP_WEIGHTS) {
		if (roll < entry.weight)
			return entry.op;
		roll -= entry.weight;
	}
	return Op::BORROW;
}

// ------------------------------------------------------------------------------------------------------------------------------ tasks

void taskMixed(RunState& run, WorkerState& worker, Rng& rng) {
	TaskScope scope(TaskInfo{std::source_location::current(), WORKER_KIND});
	TaskScope::ensurePublished(); // see the harness rules at the top of the file
	Borrows borrows;
	int32_t ops = 1 + rng.below(12);
	for (int32_t i = 0; i < ops && !run.stop.load(std::memory_order_relaxed); ++i) {
		Op op = pickOp(rng);
		worker.begin(op);
		try {
			runOp(run, op, rng, borrows);
		} catch (const FaultInjected&) {
			run.metrics.faultsCaught.fetch_add(1, std::memory_order_relaxed);
		} catch (const std::exception& e) {
			run.unexpected(opName(op), e);
		}
		worker.end();
	}
	borrows.verifyAll(run, "end of task");
}

/** PeriodicSaveService-like loop: QuiescentScope at depth 1 over a vector<Ref> snapshot with a quiescentPoint per element (design §2.6). */
void taskQuiescent(RunState& run, WorkerState& worker, Rng& rng) {
	TaskScope scope(TaskInfo{std::source_location::current(), WORKER_KIND});
	StressHub& hub = run.hubRef();
	std::vector<Ref<StressObject>> snapshot;
	{
		int32_t limit = 16 + rng.below(96);
		for (Ptr<StressObject> value : hub.world.values()) {
			snapshot.emplace_back(value);
			if (static_cast<int32_t>(snapshot.size()) >= limit)
				break;
		}
	}
	QuiescentScope quiescent;
	for (const Ref<StressObject>& object : snapshot) {
		if (run.stop.load(std::memory_order_relaxed))
			break;
		quiescentPoint();
		run.metrics.quiescentPoints.fetch_add(1, std::memory_order_relaxed);
		verifyObject(run, *object, "quiescent loop element");
		object->counter += 1;
		if (Ptr<StressObject> link = object->link.get())
			verifyObject(run, *link, "quiescent loop link");
		if (CHECKED && rng.chance(20)) { // C1: a borrow taken before quiescentPoint must not be usable after it
			Ptr<StressObject> stale(object);
			quiescentPoint();
			try {
				(void)stale->id;
				run.fail("Ptr created before quiescentPoint() was usable after it (C1)");
			} catch (const IllegalStateException&) {
				run.metrics.staleBorrowsDetected.fetch_add(1, std::memory_order_relaxed);
			}
		}
	}
	(void)worker;
}

/** Tasks that block: Semaphore acquire, contended Monitor, timed tryLock; with or without a borrow held across the wait. */
void taskBlocked(RunState& run, WorkerState& worker, Rng& rng) {
	TaskScope scope(TaskInfo{std::source_location::current(), WORKER_KIND});
	StressHub& hub = run.hubRef();
	Ptr<StressObject> held;
	if (rng.chance(400))
		held = pickObject(run, rng); // publishes: this task pins reclamation for the duration of its wait (bounded)
	auto hold = std::chrono::milliseconds(rng.below(static_cast<int32_t>(run.config.blockedHoldMax.count()) + 1));
	try {
		switch (rng.below(3)) {
			case 0: {
				hub.semaphore.acquire();
				run.metrics.blockedAcquires.fetch_add(1, std::memory_order_relaxed);
				auto release = finally([&] { hub.semaphore.release(); });
				std::this_thread::sleep_for(hold);
				if (run.injectFault(rng))
					throw FaultInjected("fault while holding a Semaphore permit");
				break;
			}
			case 1:
				SYNCHRONIZED(hub.contended) {
					run.metrics.blockedAcquires.fetch_add(1, std::memory_order_relaxed);
					std::this_thread::sleep_for(hold / 4);
				}
				break;
			default:
				if (hub.contended.tryLock(std::chrono::milliseconds(rng.below(10)))) {
					auto unlock = finally([&] { hub.contended.unlock(); });
					run.metrics.blockedAcquires.fetch_add(1, std::memory_order_relaxed);
				}
				break;
		}
	} catch (const FaultInjected&) {
		run.metrics.faultsCaught.fetch_add(1, std::memory_order_relaxed);
		if (hub.contended.isHeldByCurrentThread())
			run.fail("contended Monitor still held after an exception");
	}
	if (held)
		verifyDeep(run, held, "borrow held across a blocking wait");
	(void)worker;
}

/** A task that loads no pointer (geo build, NN training, pure DAO task): it must never publish an epoch (design §2.6, RR-4). */
void taskBorrowFree(RunState& run, WorkerState& worker, Rng& rng) {
	TaskScope scope(TaskInfo{std::source_location::current(), BORROW_FREE_KIND});
	StressHub& hub = run.hubRef(); // a retained const Ref: no read barrier
	worker.borrowFreeSeq.fetch_add(1, std::memory_order_release); // odd
	auto end = SteadyClock::now() + std::chrono::milliseconds(1 + rng.below(20));
	uint64_t hash = rng.next();
	while (SteadyClock::now() < end) {
		for (int i = 0; i < 256; ++i)
			hash = (hash ^ (hash >> 31)) * 0x7FB5D329728EA185ULL;
		if (rng.chance(5) && hub.semaphore.tryAcquire(std::chrono::milliseconds(2))) { // a blocking wait without borrows
			hub.semaphore.release();
			run.metrics.blockedAcquires.fetch_add(1, std::memory_order_relaxed);
		}
	}
	(void)hub.slotCount;
	bool published = TaskScope::isPublished();
	worker.borrowFreeSeq.fetch_add(1, std::memory_order_release); // even
	if (published) {
		run.pinViolations.fetch_add(1, std::memory_order_relaxed);
		run.fail("a borrow-free worker task has a published epoch (TaskScope entry or a non-loading operation published)");
	}
}

void longBorrowFreeBody(RunState& run, int64_t durationMillis, bool block) {
	BorrowFreeSlot* slot = nullptr;
	for (BorrowFreeSlot& candidate : run.poolBorrowFree) {
		bool expected = false;
		if (candidate.used.compare_exchange_strong(expected, true)) {
			slot = &candidate;
			break;
		}
	}
	auto releaseSlot = finally([&] {
		if (slot != nullptr)
			slot->used.store(false);
		run.longTasksRunning.fetch_sub(1);
	});
	if (slot != nullptr) {
		slot->context.store(&ThreadContext::current());
		slot->seq.fetch_add(1); // odd
	}
	run.metrics.longTasks.fetch_add(1, std::memory_order_relaxed);
	auto end = SteadyClock::now() + std::chrono::milliseconds(durationMillis);
	if (block) {
		BlockingRegion region("stress long blocking wait");
		while (SteadyClock::now() < end && !run.stop.load(std::memory_order_relaxed))
			std::this_thread::sleep_for(5ms);
	} else {
		uint64_t hash = static_cast<uint64_t>(durationMillis);
		while (SteadyClock::now() < end && !run.stop.load(std::memory_order_relaxed)) {
			for (int i = 0; i < 4096; ++i)
				hash = (hash ^ (hash >> 29)) * 0xBF58476D1CE4E5B9ULL;
		}
	}
	bool published = TaskScope::isPublished();
	if (slot != nullptr)
		slot->seq.fetch_add(1); // even
	if (published) {
		run.pinViolations.fetch_add(1, std::memory_order_relaxed);
		run.fail("a borrow-free long-running pool task has a published epoch (the pool or the Future published for it)");
	}
}

void taskLongSubmit(RunState& run, WorkerState& worker, Rng& rng) {
	(void)worker;
	if (run.longTasksRunning.fetch_add(1) >= run.config.maxConcurrentLongTasks) {
		run.longTasksRunning.fetch_sub(1);
		return;
	}
	int64_t duration = 1 + rng.below(static_cast<int32_t>(run.config.longTaskMax.count()));
	bool block = rng.chance(500);
	try {
		ThreadPoolManager::getInstance().executeLongRunning(Pin(), [&run, duration, block] { longBorrowFreeBody(run, duration, block); }, run.longTaskSite);
	} catch (...) {
		run.longTasksRunning.fetch_sub(1);
		throw;
	}
}

void workerMain(RunState& run, int32_t index) {
	WorkerState& worker = *run.workers[static_cast<size_t>(index)];
	worker.context.store(&ThreadContext::current());
	Rng rng(run.seed + 0x1000193ULL * static_cast<uint64_t>(index + 1));
	while (!run.stop.load(std::memory_order_relaxed)) {
		int32_t roll = rng.below(1000);
		Op kind = roll < 15 ? Op::TASK_QUIESCENT
			: roll < 30    ? Op::TASK_BLOCKED
			: roll < 50    ? Op::TASK_BORROW_FREE
			: roll < 53    ? Op::TASK_LONG_SUBMIT
						   : Op::TASK_MIXED;
		worker.begin(kind);
		try {
			switch (kind) {
				case Op::TASK_QUIESCENT:
					taskQuiescent(run, worker, rng);
					break;
				case Op::TASK_BLOCKED:
					taskBlocked(run, worker, rng);
					break;
				case Op::TASK_BORROW_FREE:
					taskBorrowFree(run, worker, rng);
					break;
				case Op::TASK_LONG_SUBMIT:
					taskLongSubmit(run, worker, rng);
					break;
				default:
					taskMixed(run, worker, rng);
					break;
			}
		} catch (const std::exception& e) {
			run.unexpected(opName(kind), e);
		} catch (...) {
			run.unexpectedExceptions.fetch_add(1);
			run.fail(std::format("unknown exception in {}", opName(kind)));
		}
		worker.end();
		worker.tasks.fetch_add(1, std::memory_order_relaxed);
	}
	worker.op.store("finished");
}

// ------------------------------------------------------------------------------------------------------------------------------ monitors

/** Seqlock-bracketed read: true if the thread was in a borrow-free phase for the whole read and had a published epoch. */
bool publishedDuringBorrowFree(const std::atomic<uint64_t>& seq, const std::atomic<ThreadContext*>& context, uint64_t& epoch) {
	uint64_t before = seq.load(std::memory_order_acquire);
	if ((before & 1) == 0)
		return false;
	ThreadContext* ctx = context.load(std::memory_order_acquire);
	if (ctx == nullptr)
		return false;
	epoch = ctx->publishedEpoch.load(std::memory_order_acquire);
	uint64_t after = seq.load(std::memory_order_acquire);
	return before == after && epoch != EPOCH_IDLE;
}

void samplerMain(RunState& run, std::stop_token stopToken) {
	auto nextStats = SteadyClock::now();
	auto nextMemory = SteadyClock::now();
	while (!stopToken.stop_requested()) {
		for (const auto& worker : run.workers) {
			uint64_t epoch = 0;
			if (publishedDuringBorrowFree(worker->borrowFreeSeq, worker->context, epoch)) {
				run.pinViolations.fetch_add(1);
				run.fail(std::format("sampler: borrow-free worker task holds published epoch {}", epoch));
			}
		}
		for (const BorrowFreeSlot& slot : run.poolBorrowFree) {
			uint64_t epoch = 0;
			if (slot.used.load(std::memory_order_relaxed) && publishedDuringBorrowFree(slot.seq, slot.context, epoch)) {
				run.pinViolations.fetch_add(1);
				run.fail(std::format("sampler: borrow-free long-running pool task holds published epoch {}", epoch));
			}
		}
		run.metrics.borrowFreeSamples.fetch_add(1, std::memory_order_relaxed);
		auto now = SteadyClock::now();
		if (now >= nextStats) {
			nextStats = now + 50ms;
			Reclaimer::Stats stats = Reclaimer::getInstance().stats();
			run.lagSamplesMillis.push_back(stats.lag.count());
			run.backlogSamples.push_back(stats.backlog);
			run.maxBacklog = std::max(run.maxBacklog, stats.backlog);
			run.maxBacklogBytes = std::max(run.maxBacklogBytes, stats.backlogBytes);
			const TaskInfo& oldest = stats.oldestPublishedTask;
			bool longSite = oldest.where.line() == run.longTaskSite.line() && oldest.where.file_name() != nullptr &&
				std::strcmp(oldest.where.file_name(), run.longTaskSite.file_name()) == 0 && std::strcmp(oldest.kind, TaskKind::LONG_RUNNING) == 0;
			if (stats.oldestPublishedThreadId != 0 && oldest.kind == BORROW_FREE_KIND)
				run.metrics.borrowFreeStatsAttributions.fetch_add(1, std::memory_order_relaxed);
			if (stats.oldestPublishedThreadId != 0 && longSite)
				run.metrics.longTaskStatsAttributions.fetch_add(1, std::memory_order_relaxed);
		}
		if (now >= nextMemory) {
			nextMemory = now + 250ms;
			MemorySample memory = sampleProcessMemory();
			run.peakMemory.privateBytes = std::max(run.peakMemory.privateBytes, memory.privateBytes);
			run.peakMemory.workingSet = std::max(run.peakMemory.workingSet, memory.workingSet);
			run.peakMemory.heapAllocated = std::max(run.peakMemory.heapAllocated, memory.heapAllocated);
		}
		std::this_thread::sleep_for(run.config.samplePeriod);
	}
}

void checkerMain(RunState& run, std::stop_token stopToken, SteadyClock::time_point start) {
	Reclaimer::Stats initialStats = Reclaimer::getInstance().stats();
	uint64_t lastScans = initialStats.scans;
	uint64_t lastEpoch = initialStats.epoch;
	uint64_t lastMinActive = initialStats.minActive;
	auto lastScanProgress = SteadyClock::now();
	auto lastEpochProgress = lastScanProgress;
	auto lastMinActiveProgress = lastScanProgress;
	auto nextProgressLine = SteadyClock::now() + 10s;
	uint64_t checks = 0;
	std::mutex sleepMutex;
	std::condition_variable_any sleepCondition;
	while (!stopToken.stop_requested()) {
		{
			std::unique_lock lock(sleepMutex);
			sleepCondition.wait_for(lock, stopToken, run.config.checkPeriod, [] { return false; });
		}
		if (stopToken.stop_requested())
			break;
		++checks;
		try {
			TaskScope scope(TaskInfo{std::source_location::current(), CHECKER_KIND});
			StressHub& hub = run.hubRef();
			int32_t visited = 0;
			for (const auto& entry : hub.world.entrySet()) {
				verifyObject(run, *entry.value, "checker world sample");
				if (run.worldKeyOf(*entry.value) != entry.key)
					run.fail(std::format("checker: world map key {} holds id {}", entry.key, entry.value->id));
				if (++visited >= 512)
					break;
			}
			if (checks % 5 == 0) {
				for (Ptr<StressPart> part : hub.partList.snapshot())
					verifyPart(run, *part, hub, "checker PartList sample");
				for (const auto& entry : hub.partMap.snapshot())
					verifyPart(run, *entry.second, hub, "checker PartMap sample");
			}
		} catch (const std::exception& e) {
			run.unexpected("checker", e);
		}
		Reclaimer::Stats stats = Reclaimer::getInstance().stats();
		auto now = SteadyClock::now();
		if (stats.scans != lastScans) {
			lastScans = stats.scans;
			lastScanProgress = now;
		} else if (now - lastScanProgress > 5s) {
			run.fail("the Reclaimer made no scan for more than 5 s");
			lastScanProgress = now;
		}
		if (stats.epoch != lastEpoch) {
			lastEpoch = stats.epoch;
			lastEpochProgress = now;
		} else if (now - lastEpochProgress > 5s) {
			run.fail(std::format("the global epoch stayed at {} for more than 5 s while scans ran", stats.epoch));
			lastEpochProgress = now;
		}
		if (stats.minActive != lastMinActive) {
			lastMinActive = stats.minActive;
			lastMinActiveProgress = now;
		} else if (now - lastMinActiveProgress > 30s) {
			run.fail(std::format("the oldest published epoch stayed at {} for more than 30 s (a task pins reclamation; oldest thread {})",
				stats.minActive, stats.oldestPublishedThreadId));
			lastMinActiveProgress = now;
		}
		try {
			size_t pending = ThreadPoolManager::getInstance().backend().pendingTasks().size();
			run.throttleFutures.store(pending > static_cast<size_t>(run.config.maxInflightFutures), std::memory_order_relaxed);
			(void)CleanerQueue::drainNow();
		} catch (const std::exception& e) {
			run.unexpected("checker pools/cleaner", e);
		}
		if (run.config.verbose && now >= nextProgressLine) {
			nextProgressLine = now + 10s;
			uint64_t tasks = 0;
			for (const auto& worker : run.workers)
				tasks += worker->tasks.load(std::memory_order_relaxed);
			auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();
			std::printf("[stress] %llds: tasks %llu, live objects %lld, destroyed %llu, backlog %llu (%llu KB), lag %lld ms, epoch %llu, failures %llu\n",
				static_cast<long long>(elapsed), static_cast<unsigned long long>(tasks), static_cast<long long>(stressCounters().liveObjects()),
				static_cast<unsigned long long>(stats.destroyedTotal), static_cast<unsigned long long>(stats.backlog),
				static_cast<unsigned long long>(stats.backlogBytes / 1024), static_cast<long long>(stats.lag.count()),
				static_cast<unsigned long long>(stats.epoch), static_cast<unsigned long long>(run.failures.total()));
			std::fflush(stdout);
		}
	}
}

/** Terminates the process with diagnostics when a phase exceeds its deadline (a deadlock must not hang the test run). */
class Supervisor {
public:
	explicit Supervisor(RunState& run) : run(run), thread([this](std::stop_token stop) { main(stop); }) {}
	~Supervisor() {
		thread.request_stop();
		condition.notify_all();
	}
	Supervisor(const Supervisor&) = delete;
	Supervisor& operator=(const Supervisor&) = delete;

	void setDeadline(const char* phase, SteadyClock::time_point deadline) {
		{
			std::scoped_lock lock(mutex);
			currentPhase = phase;
			currentDeadline = deadline;
		}
		condition.notify_all();
	}

private:
	void main(std::stop_token stop) {
		std::unique_lock lock(mutex);
		while (!stop.stop_requested()) {
			condition.wait_for(lock, stop, 200ms, [] { return false; });
			if (!stop.stop_requested() && SteadyClock::now() >= currentDeadline)
				dumpAndExit(currentPhase);
		}
	}

	[[noreturn]] void dumpAndExit(const char* phase) {
		std::fprintf(stderr, "\n[stress] HANG: phase '%s' exceeded its deadline. Worker states:\n", phase);
		int64_t now = nowNanos();
		for (size_t i = 0; i < run.workers.size(); ++i) {
			const WorkerState& worker = *run.workers[i];
			std::fprintf(stderr, "  worker %zu: op=%s for %lld ms, tasks=%llu\n", i, worker.op.load(),
				static_cast<long long>((now - worker.opStartNanos.load()) / 1'000'000), static_cast<unsigned long long>(worker.tasks.load()));
		}
		try {
			Watchdog::DumpReport report = Watchdog::getInstance().dump(Watchdog::Reason::MANUAL, "stress harness hang");
			std::fprintf(stderr, "%s\n", report.text.c_str());
			for (const std::string& line : LockOrderValidator::getInstance().describe())
				std::fprintf(stderr, "%s\n", line.c_str());
			Reclaimer::Stats stats = Reclaimer::getInstance().stats();
			std::fprintf(stderr, "Reclaimer: epoch %llu, minActive %llu, backlog %llu, lag %lld ms\n", static_cast<unsigned long long>(stats.epoch),
				static_cast<unsigned long long>(stats.minActive), static_cast<unsigned long long>(stats.backlog), static_cast<long long>(stats.lag.count()));
		} catch (...) {
		}
		std::fflush(stderr);
		std::fflush(stdout);
		std::_Exit(SUPERVISOR_EXIT_CODE);
	}

	RunState& run;
	std::mutex mutex;
	std::condition_variable_any condition;
	const char* currentPhase = "setup";
	SteadyClock::time_point currentDeadline = SteadyClock::time_point::max();
	std::jthread thread;
};

// ------------------------------------------------------------------------------------------------------------------------------ teardown

void clearHub(RunState& run) {
	TaskScope scope(TaskInfo{std::source_location::current(), TaskKind::SHUTDOWN});
	StressHub& hub = run.hubRef();
	for (const auto& entry : hub.world.entrySet()) {
		if (Ptr<StressObject> removed = hub.world.remove(entry.key))
			LeakCensus::getInstance().onRemovedFromWorld(*removed, "StressObject", removed->id);
	}
	hub.world.clear();
	hub.buckets.clear();
	hub.nested.clear();
	hub.list.clear();
	hub.groups.clear();
	hub.set.clear();
	hub.observers.clear();
	hub.queue.clear();
	for (int32_t i = 0; i < hub.slotCount; ++i)
		hub.slots[static_cast<size_t>(i)].set(nullptr);
	for (int32_t i = 0; i < hub.array->length(); ++i)
		(*hub.array)[i].set(nullptr);
	hub.atomicRef.set(nullptr);
	for (int32_t key = 0; key < 256; ++key)
		(void)hub.partMap.remove(key);
	for (Ptr<StressPart> part : hub.partList.snapshot()) {
		part->ref = nullptr;
		part->actor.set(nullptr);
	}
}

std::string formatBytes(uint64_t bytes) {
	return std::format("{:.1f} MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
}

int64_t percentile(std::vector<int64_t> values, double fraction) {
	if (values.empty())
		return 0;
	std::sort(values.begin(), values.end());
	auto index = static_cast<size_t>(fraction * static_cast<double>(values.size() - 1));
	return values[index];
}

std::string buildName() {
	std::string name = CHECKED ? "checked" : "unchecked";
#if defined(AION_ASAN) && AION_ASAN
	name += "+ASan";
#endif
#if defined(NDEBUG)
	name += " optimized";
#else
	name += " debug";
#endif
	return name;
}

} // namespace

// ------------------------------------------------------------------------------------------------------------------------------ config

StressConfig StressConfig::fromEnvironment(StressConfig defaults) {
	auto readInt = [](const char* name) -> std::optional<int64_t> {
		const char* value = std::getenv(name); // test-only switch
		if (value == nullptr || *value == '\0')
			return std::nullopt;
		return std::strtoll(value, nullptr, 10);
	};
	if (auto seconds = readInt("AION_STRESS_SECONDS"); seconds && *seconds > 0)
		defaults.duration = std::chrono::seconds(*seconds);
	if (auto threads = readInt("AION_STRESS_THREADS"); threads && *threads > 0)
		defaults.workerThreads = static_cast<int32_t>(*threads);
	if (auto seed = readInt("AION_STRESS_SEED"); seed && *seed != 0)
		defaults.seed = static_cast<uint64_t>(*seed);
	if (auto faults = readInt("AION_STRESS_FAULTS"); faults && *faults >= 0)
		defaults.faultPermille = static_cast<int32_t>(*faults);
	if (auto verbose = readInt("AION_STRESS_VERBOSE"); verbose && *verbose != 0)
		defaults.verbose = true;
	if (auto knownIssues = readInt("AION_STRESS_KNOWN_ISSUES"); knownIssues && *knownIssues != 0)
		defaults.knownIssueFaults = true;
	if (auto mutation = readInt("AION_STRESS_MUTATION"); mutation && *mutation > 0)
		defaults.mutation = static_cast<int32_t>(*mutation);
	if (const char* report = std::getenv("AION_STRESS_REPORT"); report != nullptr && *report != '\0')
		defaults.reportPath = report;
	return defaults;
}

std::chrono::seconds StressConfig::requestedLongRunDuration() {
	const char* value = std::getenv("AION_STRESS_SECONDS");
	if (value == nullptr || *value == '\0')
		return std::chrono::seconds(0);
	return std::chrono::seconds(std::max<long long>(0, std::strtoll(value, nullptr, 10)));
}

// ------------------------------------------------------------------------------------------------------------------------------ run

StressResult runKernelStress(const StressConfig& config) {
	StressResult result;
	RunState run(config);
	auto hardware = static_cast<int32_t>(std::max(1u, std::thread::hardware_concurrency()));
	run.workerThreads = config.workerThreads > 0 ? config.workerThreads : 2 * hardware;
	run.seed = config.seed != 0 ? config.seed : static_cast<uint64_t>(nowNanos());
	run.longTaskSite = std::source_location::current();
	StressCounters& counters = stressCounters();

	std::printf("[stress] kernel stress run: %d workers, %lld ms, seed %llu, faults %d permille, build %s\n", run.workerThreads,
		static_cast<long long>(config.duration.count()), static_cast<unsigned long long>(run.seed), config.faultPermille, buildName().c_str());
	std::fflush(stdout);

	Supervisor supervisor(run);
	supervisor.setDeadline("setup", SteadyClock::now() + 60s);

	if (counters.liveObjects() != 0 || counters.liveParts() != 0 || counters.liveHubs() != 0)
		run.fail(std::format("live stress objects before the run (previous run leaked): objects {}, parts {}, hubs {}", counters.liveObjects(),
			counters.liveParts(), counters.liveHubs()));

	// ---- kernel services
	Reclaimer& reclaimer = Reclaimer::getInstance();
	bool reclaimerWasRunning = reclaimer.isRunning();
	(void)reclaimer.drain();
	Reclaimer::Config reclaimerConfig = reclaimer.getConfig();
	reclaimerConfig.period = 20ms;
	reclaimer.start(reclaimerConfig);

	LockOrderValidator& lockdep = LockOrderValidator::getInstance();
	uint64_t lockdepFailuresBefore = lockdep.failureCount();
	size_t cyclesBefore = lockdep.reportCount(LockOrderValidator::ReportKind::CYCLE);
	size_t nestingBefore = lockdep.reportCount(LockOrderValidator::ReportKind::SAME_CLASS_NESTING);
	size_t blockingBefore = lockdep.reportCount(LockOrderValidator::ReportKind::BLOCKING_UNDER_MONITOR);

	Watchdog& watchdog = Watchdog::getInstance();
	uint64_t dumpListener = watchdog.addDumpListener([&run](const Watchdog::DumpReport& report) {
		auto index = static_cast<size_t>(report.reason);
		if (index < run.dumpsByReason.size())
			run.dumpsByReason[index].fetch_add(1);
		run.dumpSummaries.add(std::format("{}: {}", Watchdog::reasonName(report.reason), report.summary));
	});
	bool watchdogStarted = false;
	if (config.startWatchdog && !watchdog.isRunning()) {
		Watchdog::Config watchdogConfig;
		watchdogConfig.period = 1000ms;
		watchdogConfig.slowTaskWarning = 5000ms;
		watchdogConfig.stall = 60s;
		watchdogConfig.writeMinidump = false;
		watchdog.start(watchdogConfig);
		watchdogStarted = true;
	}

	LeakCensus& census = LeakCensus::getInstance();
	LeakCensus::Config censusBefore = census.getConfig();
	LeakCensus::Config censusConfig = censusBefore;
	// Objects removed from the world stay legitimately referenced by slots and lists, so the census's time-based reports and the zombie breaker
	// are pushed past the run; the pass criterion is trackedCount() == 0 after the teardown.
	auto beyondRun = std::chrono::duration_cast<std::chrono::milliseconds>(config.duration + config.teardownTimeout) + std::chrono::hours(1);
	censusConfig.censusAfter = beyondRun;
	censusConfig.zombieBreakAfter = beyondRun;
	censusConfig.stalePinAfter = beyondRun;
	census.configure(censusConfig);
	census.install();
	counters.cleanerCleaned.store(counters.cleanerPushed.load());
	CleanerQueue::setCleanerAction([](int32_t, const char*) { stressCounters().cleanerCleaned.fetch_add(1, std::memory_order_relaxed); });
	CleanerQueue::install();

	ThreadPoolManager::installBackend(nullptr);
	ThreadPoolManager::Config poolConfig;
	poolConfig.maximumRuntimeInMillisecWithoutWarning = 5000;
	try {
		ThreadPoolManager::configure(poolConfig);
	} catch (const std::exception& e) {
		run.fail(std::format("ThreadPoolManager::configure failed (default pools already created in this process?): {}", e.what()));
	}
	ThreadPoolBackend::Options poolOptions;
	poolOptions.instantThreads = config.instantThreads > 0 ? config.instantThreads : hardware;
	poolOptions.scheduledThreads = config.scheduledThreads;
	ThreadPoolManager::installBackend(std::make_unique<ThreadPoolBackend>(poolOptions));
	run.serialExecutor = std::make_unique<SerialExecutor>("StressLink");

	Reclaimer::Stats statsBefore = reclaimer.stats();
	MemorySample baseline = sampleProcessMemory();

	// ---- run
	run.hub = StressHub::create(config.slots, config.arrayLength);
	for (int32_t i = 0; i < run.workerThreads; ++i)
		run.workers.push_back(std::make_unique<WorkerState>());
	auto start = SteadyClock::now();
	std::vector<std::thread> workers;
	{
		std::jthread sampler([&run](std::stop_token stop) { samplerMain(run, stop); });
		std::jthread checker([&run, start](std::stop_token stop) { checkerMain(run, stop, start); });
		supervisor.setDeadline("run", start + config.duration + config.joinTimeout);
		{
			// harness self-test: a lifetime protocol step switched off for the run phase (checked builds), the run must fail
			std::optional<detail::MutationScope> mutation;
			if (config.mutation != 0)
				mutation.emplace(static_cast<detail::Mutation>(config.mutation));
			for (int32_t i = 0; i < run.workerThreads; ++i)
				workers.emplace_back([&run, i] { workerMain(run, i); });

			while (SteadyClock::now() - start < config.duration && !run.stop.load())
				std::this_thread::sleep_for(20ms);
			run.stop.store(true);
			for (std::thread& worker : workers)
				worker.join();
		}
		auto runEnd = SteadyClock::now();
		double seconds = std::chrono::duration<double>(runEnd - start).count();

		// ---- teardown
		supervisor.setDeadline("teardown", SteadyClock::now() + config.teardownTimeout);
		for (auto waitStart = SteadyClock::now(); run.longTasksRunning.load() > 0 && SteadyClock::now() - waitStart < 10s;)
			std::this_thread::sleep_for(5ms);
		checker.request_stop();
		checker.join();
		run.serialExecutor.reset(); // cancels its queued tasks (safe while one runs)
		CleanerQueue::uninstall(); // no more drains: the teardown drains explicitly below
		reclaimer.reclaimNow();    // serialized with the Reclaimer thread's scans: a hook already running has finished
		ThreadPoolManager::getInstance().shutdown();
		// the shut-down pools stay installed during the drain (the kernel services never create the default pools, see ThreadPoolManager::clock)
		sampler.request_stop();
		sampler.join();

		Reclaimer::Stats statsAtRunEnd = reclaimer.stats();
		clearHub(run);
		run.hub.reset();

		auto drainStart = SteadyClock::now();
		bool drained = false;
		uint64_t drainScans = 0;
		while (SteadyClock::now() - drainStart < config.teardownTimeout - 10s) {
			reclaimer.reclaimNow();
			++drainScans;
			(void)CleanerQueue::drainNow();
			Reclaimer::Stats stats = reclaimer.stats();
			if (counters.liveObjects() == 0 && counters.liveParts() == 0 && counters.liveHubs() == 0 && stats.backlog == 0 &&
				counters.cleanerCleaned.load() == counters.cleanerPushed.load()) {
				drained = true;
				break;
			}
			if (drainScans % 256 == 0)
				std::this_thread::sleep_for(1ms);
		}
		auto drainMillis = std::chrono::duration_cast<std::chrono::milliseconds>(SteadyClock::now() - drainStart).count();
		Reclaimer::Stats statsAfter = reclaimer.stats();
		size_t censusTracked = census.trackedCount();
		uint64_t zombieCuts = census.zombieCutCount();
		size_t censusLeaks = census.getLeaks().size();
		MemorySample finalMemory = sampleProcessMemory();

		// ---- restore the process state
		census.uninstall();
		reclaimer.reclaimNow(); // barrier: a census hook invocation still in flight has finished (the hook also re-checks its installed flag)
		census.configure(censusBefore);
		// installBackend retires the pools (cancels what is left, joins the threads) before it publishes nullptr, so a task still running there
		// (SerialExecutor hand-over) cannot lazily create the default pools (StressReproducerTest.InstallBackendDoesNot...)
		ThreadPoolManager::installBackend(nullptr);
		(void)reclaimer.drain(); // futures the replaced pools still held
		CleanerQueue::setCleanerAction(nullptr);
		if (watchdogStarted)
			watchdog.stop();
		watchdog.removeDumpListener(dumpListener);
		if (!reclaimerWasRunning)
			reclaimer.stop();
		supervisor.setDeadline("done", SteadyClock::time_point::max());

		// ---- evaluation
		if (!drained)
			run.fail(std::format("teardown did not reclaim everything within {} ms: live objects {}, parts {}, hubs {}, backlog {}, cleaner {}/{}",
				drainMillis, counters.liveObjects(), counters.liveParts(), counters.liveHubs(), statsAfter.backlog, counters.cleanerCleaned.load(),
				counters.cleanerPushed.load()));
		if (censusTracked != 0 || censusLeaks != 0)
			run.fail(std::format("LeakCensus not empty after teardown: tracked {}, leak reports {}", censusTracked, censusLeaks));
		if (zombieCuts != 0)
			run.fail(std::format("zombie breaker cut {} edges", zombieCuts));
		uint64_t lockdepFailures = lockdep.failureCount() - lockdepFailuresBefore;
		size_t cycles = lockdep.reportCount(LockOrderValidator::ReportKind::CYCLE) - cyclesBefore;
		if (lockdepFailures != 0 || cycles != 0) {
			std::string texts;
			for (const LockOrderValidator::Report& report : lockdep.getReports())
				if (report.kind == LockOrderValidator::ReportKind::CYCLE)
					texts += "\n    " + report.text;
			run.fail(std::format("lock-order validator reported {} new cycle(s) ({} failures):{}", cycles, lockdepFailures, texts));
		}
		uint64_t deadlockDumps = run.dumpsByReason[static_cast<size_t>(Watchdog::Reason::DEADLOCK)].load();
		uint64_t stallDumps = run.dumpsByReason[static_cast<size_t>(Watchdog::Reason::STALL)].load();
		if (deadlockDumps != 0 || stallDumps != 0)
			run.fail(std::format("watchdog dumps: {} deadlock(s), {} stall(s)", deadlockDumps, stallDumps));
		uint64_t heapGrowth = finalMemory.heapAllocated > baseline.heapAllocated ? finalMemory.heapAllocated - baseline.heapAllocated : 0;
		uint64_t heapSlack = uint64_t{64} * 1024 * 1024 + (CHECKED ? reclaimerConfig.delayedFreeBytes : 0);
		bool asan = false;
#if defined(AION_ASAN) && AION_ASAN
		asan = true;
#endif
		if (heapGrowth > heapSlack) {
			std::string message = std::format("process heap in use did not return to baseline: {} -> {} (+{}, slack {})", formatBytes(baseline.heapAllocated),
				formatBytes(finalMemory.heapAllocated), formatBytes(heapGrowth), formatBytes(heapSlack));
			if (asan)
				run.warnings.add(message + " (ASan allocator: not a pass criterion)");
			else
				run.fail(message);
		}
		if (run.metrics.borrowFreeStatsAttributions.load() != 0 || run.metrics.longTaskStatsAttributions.load() != 0)
			run.warnings.add(std::format("Reclaimer::Stats named a borrow-free task as the oldest publishing task in {} worker / {} long-running samples "
				"(the seqlock-checked sampler found {} real publication(s) by borrow-free tasks)", run.metrics.borrowFreeStatsAttributions.load(),
				run.metrics.longTaskStatsAttributions.load(), run.pinViolations.load()));

		// ---- report
		std::array<uint64_t, OP_COUNT> opTotals{};
		uint64_t tasksTotal = 0;
		for (const auto& worker : run.workers) {
			tasksTotal += worker->tasks.load();
			for (size_t i = 0; i < OP_COUNT; ++i)
				opTotals[i] += worker->counts[i].load();
		}
		uint64_t opsTotal = 0;
		for (size_t i = 0; i < FIRST_TASK_KIND; ++i)
			opsTotal += opTotals[i];
		uint64_t destroyedDuringRun = statsAtRunEnd.destroyedTotal - statsBefore.destroyedTotal;
		uint64_t scansDuringRun = statsAtRunEnd.scans - statsBefore.scans;

		std::string report;
		auto line = [&report](std::string text) {
			report += text;
			report += '\n';
		};
		line(std::format("=== Kernel stress report ({}) ===", buildName()));
		if (config.mutation != 0)
			line(std::format("MUTATION {} was active during the run phase (harness self-test: the run is expected to fail)", config.mutation));
		line(std::format("config: workers {}, duration {:.1f} s, seed {}, faults {} permille, slots {}, world keys {}, instant threads {}, scheduled threads {}",
			run.workerThreads, seconds, run.seed, config.faultPermille, config.slots, config.worldKeys, poolOptions.instantThreads,
			poolOptions.scheduledThreads));
		line(std::format("throughput: {} tasks ({:.0f}/s), {} kernel ops ({:.0f}/s), objects created {} ({:.0f}/s), destroyed during run {} ({:.0f}/s)", tasksTotal,
			static_cast<double>(tasksTotal) / seconds, opsTotal, static_cast<double>(opsTotal) / seconds, counters.objectsCreated.load(),
			static_cast<double>(counters.objectsCreated.load()) / seconds, destroyedDuringRun, static_cast<double>(destroyedDuringRun) / seconds));
		line(std::format("futures: scheduled {}, ran {}, cancelled {}, periodic runs {}, get timeouts {}, throttled submissions {}, serial runs {}/{}",
			run.metrics.futuresScheduled.load(), run.metrics.futuresRan.load(), run.metrics.futuresCancelled.load(), run.metrics.periodicRuns.load(),
			run.metrics.getTimeouts.load(), run.metrics.throttledSubmissions.load(), run.serialRuns.load(), run.serialSubmitted));
		line(std::format("lifetime: resurrections {}, quiescent points {}, stale borrows detected (C1) {}, parts created {}, long borrow-free pool tasks {}, "
			"blocked acquires {}, borrow-free samples {}", run.metrics.resurrections.load(), run.metrics.quiescentPoints.load(),
			run.metrics.staleBorrowsDetected.load(), counters.partsCreated.load(), run.metrics.longTasks.load(), run.metrics.blockedAcquires.load(),
			run.metrics.borrowFreeSamples.load()));
		line(std::format("faults: injected {}, caught by the harness {}, ExecutionException {}, CancellationException {}, recursive updates detected {}, "
			"index races (Java IOOBE) {}", run.metrics.faultsInjected.load(), run.metrics.faultsCaught.load(), run.metrics.executionExceptions.load(),
			run.metrics.cancellationExceptions.load(), run.metrics.recursiveUpdatesDetected.load(), run.metrics.indexRaces.load()));
		line(std::format("reclamation: scans during run {} ({:.1f}/s), lag p50 {} ms, p99 {} ms, max {} ms, backlog max {} (avg {:.0f}), backlog bytes max {}",
			scansDuringRun, static_cast<double>(scansDuringRun) / seconds, percentile(run.lagSamplesMillis, 0.5), percentile(run.lagSamplesMillis, 0.99),
			run.lagSamplesMillis.empty() ? 0 : *std::max_element(run.lagSamplesMillis.begin(), run.lagSamplesMillis.end()), run.maxBacklog,
			run.backlogSamples.empty() ? 0.0
									   : static_cast<double>(std::accumulate(run.backlogSamples.begin(), run.backlogSamples.end(), uint64_t{0})) /
					static_cast<double>(run.backlogSamples.size()),
			formatBytes(run.maxBacklogBytes)));
		line(std::format("teardown: drain {} ms, {} scans, destroyed total {}, backlog {}, census tracked {}, zombie cuts {}, cleaner {}/{}", drainMillis,
			drainScans, statsAfter.destroyedTotal - statsBefore.destroyedTotal, statsAfter.backlog, censusTracked, zombieCuts,
			counters.cleanerCleaned.load(), counters.cleanerPushed.load()));
		line(std::format("memory: baseline private {} / working set {} / heap {}; peak {} / {} / {}; final {} / {} / {}", formatBytes(baseline.privateBytes),
			formatBytes(baseline.workingSet), formatBytes(baseline.heapAllocated), formatBytes(run.peakMemory.privateBytes),
			formatBytes(run.peakMemory.workingSet), formatBytes(run.peakMemory.heapAllocated), formatBytes(finalMemory.privateBytes),
			formatBytes(finalMemory.workingSet), formatBytes(finalMemory.heapAllocated)));
		line(std::format("lockdep: new cycles {}, same-class nesting {}, blocking under monitor {}; watchdog dumps: deadlock {}, stall {}, slow task {}, "
			"reclaim lag {}, backlog {}", cycles, lockdep.reportCount(LockOrderValidator::ReportKind::SAME_CLASS_NESTING) - nestingBefore,
			lockdep.reportCount(LockOrderValidator::ReportKind::BLOCKING_UNDER_MONITOR) - blockingBefore, deadlockDumps, stallDumps,
			run.dumpsByReason[static_cast<size_t>(Watchdog::Reason::SLOW_TASK)].load(),
			run.dumpsByReason[static_cast<size_t>(Watchdog::Reason::RECLAIM_LAG)].load(),
			run.dumpsByReason[static_cast<size_t>(Watchdog::Reason::BACKLOG)].load()));
		line(std::format("checks: canary failures {}, unexpected exceptions {}, borrow-free pin violations {}", run.canaryFailures.load(),
			run.unexpectedExceptions.load(), run.pinViolations.load()));
		std::string opLine = "ops:";
		for (size_t i = 0; i < OP_COUNT; ++i)
			opLine += std::format(" {}={}", opName(static_cast<Op>(i)), opTotals[i]);
		line(opLine);
		for (const std::string& summary : run.dumpSummaries.snapshot())
			line("watchdog dump: " + summary);
		result.failures = run.failures.snapshot();
		if (run.failures.total() > result.failures.size())
			result.failures.push_back(std::format("... {} more failures", run.failures.total() - result.failures.size()));
		result.warnings = run.warnings.snapshot();
		for (const std::string& warning : result.warnings)
			line("WARNING: " + warning);
		for (const std::string& failure : result.failures)
			line("FAILURE: " + failure);
		result.passed = run.failures.total() == 0;
		line(result.passed ? "RESULT: PASS" : "RESULT: FAIL");
		result.report = std::move(report);
	}

	std::printf("%s", result.report.c_str());
	std::fflush(stdout);
	if (!config.reportPath.empty()) {
		std::ofstream out(config.reportPath, std::ios::app);
		out << result.report << '\n';
	}
	return result;
}

} // namespace aion::gameserver::runtime::stress
