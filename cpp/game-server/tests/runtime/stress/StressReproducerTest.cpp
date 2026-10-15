// Minimal reproducers of failures found by the kernel stress harness (tests/runtime/stress/StressHarness.cpp). Each test states the expected
// (Java-compatible, memory-safe) behaviour. Reproducers of fixed issues are regular regression tests; a reproducer of an issue that is still
// open is disabled (run with --gtest_also_run_disabled_tests) so the regular test run stays green until the owning area fixes it.

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <thread>
#include <stdexcept>
#include <vector>

#include "aion/gameserver/runtime/base/ThreadContext.h"
#include "aion/gameserver/runtime/base/YieldPoint.h"
#include "aion/gameserver/runtime/collections/Collections.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Pct.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/PoolBackends.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "support/PctSupport.h"

using namespace aion::gameserver::runtime;

namespace {

class Item final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<Item> create(int32_t value) { return makeRef<Item>(value); }
	const int32_t value;

protected:
	explicit Item(int32_t v) : value(v) {}
	~Item() override = default;
};

struct ComparatorFault : std::runtime_error {
	ComparatorFault() : std::runtime_error("comparator fault") {}
};

} // namespace

// Found by the stress harness (listOps with a throwing comparator): ArrayList::sort runs std::stable_sort in place over the Ref elements. When the
// comparator throws, std::stable_sort has moved elements into its temporary buffer or a local, so the list is left with moved-from (null) Refs and
// without the original elements; later reads see Java-impossible nulls (NullPointerException in game code) and size() counts them.
// Java's ArrayList.sort sorts a copy of the element array reference in place with TimSort; a throwing Comparator can leave the order partially
// changed but never loses elements or inserts nulls. Expected: after the exception the list holds exactly the original elements (any order).
TEST(StressReproducerTest, ArrayListSortWithThrowingComparatorKeepsAllElements) {
	Reclaimer::getInstance().drain();
	{
		TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
		ArrayList<Ref<Item>> list;
		std::vector<Ref<Item>> originals;
		for (int32_t i = 0; i < 200; ++i) {
			originals.push_back(Item::create((i * 7919) % 200));
			list.add(originals.back());
		}
		int32_t compares = 0;
		EXPECT_THROW(list.sort([&](const Ptr<Item>& a, const Ptr<Item>& b) -> int32_t {
			if (++compares == 300)
				throw ComparatorFault();
			return a->value - b->value;
		}),
			ComparatorFault);
		ASSERT_EQ(list.size(), 200);
		int32_t nulls = 0;
		std::vector<Item*> seen;
		for (Ptr<Item> element : list) {
			if (!element)
				++nulls;
			else
				seen.push_back(element.rawPointer());
		}
		EXPECT_EQ(nulls, 0) << "null elements left behind by the interrupted sort";
		std::vector<Item*> expected;
		for (const Ref<Item>& item : originals)
			expected.push_back(item.get());
		std::sort(seen.begin(), seen.end());
		std::sort(expected.begin(), expected.end());
		EXPECT_EQ(seen, expected) << "elements lost or duplicated by the interrupted sort";
	}
	Reclaimer::getInstance().drain();
}

namespace {

/** Elements of a Ref shim after an interrupted operation: counts nulls and checks that exactly `originals` remain (any order). */
template <class Range>
void expectSameElements(const Range& elements, const std::vector<Ref<Item>>& originals, const char* what) {
	int32_t nulls = 0;
	std::vector<Item*> seen;
	for (Ptr<Item> element : elements) {
		if (!element)
			++nulls;
		else
			seen.push_back(element.rawPointer());
	}
	std::vector<Item*> expected;
	for (const Ref<Item>& item : originals)
		expected.push_back(item.get());
	std::sort(seen.begin(), seen.end());
	std::sort(expected.begin(), expected.end());
	EXPECT_EQ(nulls, 0) << what << ": null elements left behind";
	EXPECT_EQ(seen, expected) << what << ": elements lost or duplicated";
}

} // namespace

// Same exception-safety question for the heap operations of PriorityQueue (std::push_heap/std::pop_heap move elements through a local).
// Java's PriorityQueue.siftUp keeps the new element in the array slot until the comparison succeeds, so nothing is lost. Expected: all offered
// elements remain after a throwing comparison during offer and during poll.
TEST(StressReproducerTest, PriorityQueueWithThrowingComparatorKeepsAllElements) {
	Reclaimer::getInstance().drain();
	{
		TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
		bool armed = false;
		int32_t compares = 0;
		PriorityQueue<Ref<Item>> queue(JavaComparator<Ref<Item>>([&](const Ptr<Item>& a, const Ptr<Item>& b) -> int32_t {
			if (armed && ++compares == 3)
				throw ComparatorFault();
			return a->value - b->value;
		}));
		std::vector<Ref<Item>> originals;
		for (int32_t i = 0; i < 64; ++i) {
			originals.push_back(Item::create((i * 37) % 64));
			queue.offer(originals.back());
		}
		originals.push_back(Item::create(-1)); // sifts to the root: several comparisons
		armed = true;
		EXPECT_THROW(queue.offer(originals.back()), ComparatorFault);
		EXPECT_EQ(queue.size(), 65);
		expectSameElements(queue.snapshot(), originals, "PriorityQueue after a throwing offer");
		compares = 0;
		EXPECT_THROW((void)queue.poll(), ComparatorFault);
		armed = false;
		std::vector<Ptr<Item>> remaining = queue.snapshot();
		std::vector<Ref<Item>> stillThere;
		for (const Ref<Item>& item : originals)
			if (std::find(remaining.begin(), remaining.end(), item.borrow()) != remaining.end())
				stillThere.push_back(item);
		EXPECT_GE(stillThere.size(), originals.size() - 1) << "a throwing poll may drop the polled head at most";
		expectSameElements(remaining, stillThere, "PriorityQueue after a throwing poll");
	}
	Reclaimer::getInstance().drain();
}

// TreeSet insertion with a throwing comparator (Java TreeMap.put: the tree is unchanged when compare throws).
TEST(StressReproducerTest, TreeSetWithThrowingComparatorKeepsAllElements) {
	Reclaimer::getInstance().drain();
	{
		TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
		bool armed = false;
		int32_t compares = 0;
		TreeSet<Ref<Item>> set(JavaComparator<Ref<Item>>([&](const Ptr<Item>& a, const Ptr<Item>& b) -> int32_t {
			if (armed && ++compares == 3)
				throw ComparatorFault();
			return a->value - b->value;
		}));
		std::vector<Ref<Item>> originals;
		for (int32_t i = 0; i < 64; ++i) {
			originals.push_back(Item::create(i));
			(void)set.add(originals.back());
		}
		armed = true;
		EXPECT_THROW((void)set.add(Item::create(1000)), ComparatorFault);
		armed = false;
		EXPECT_EQ(set.size(), 64);
		expectSameElements(set.snapshot(), originals, "TreeSet after a throwing add");
	}
	Reclaimer::getInstance().drain();
}

namespace {

class Holder final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<Holder> create() { return makeRef<Holder>(); }
	Field<Ref<Item>> field;

protected:
	Holder() = default;
	~Holder() override = default;
};

constexpr const char* PINNING_KIND = "reproducer-pinning-task";
constexpr const char* BORROW_FREE_KIND = "reproducer-borrow-free-task";

} // namespace

// Found by the stress harness (sampler: Reclaimer::Stats named borrow-free tasks as the oldest publishing task in ~20% of the samples while the
// seqlock-checked read of the same threads' publishedEpoch never saw a publication). Reclaimer::scan computes m and remembers the ThreadContext
// holding the oldest published epoch, but reads that thread's TaskInfo (oldest->task()) only after taking the incoming stack and destroying
// objects. By then the thread may have left the pinning task and started another one, so Stats::oldestPublishedTask (and the watchdog's
// RECLAIM_LAG/BACKLOG dumps built from it, design §2.6 "dumps the pinning task's TaskInfo") name an unrelated, possibly borrow-free task.
// Expected: the TaskInfo reported for the oldest published epoch is the task that published it (read together with publishedEpoch, e.g. by
// reading task() with a scope-id/epoch re-check right in computeMin).
TEST(StressReproducerTest, ReclaimerStatsNameTheTaskThatPublishedTheOldestEpoch) {
	AION_SKIP_WITHOUT_PCT();
	Reclaimer& reclaimer = Reclaimer::getInstance();
	ASSERT_FALSE(reclaimer.isRunning()) << "the scripted scan must be the only scan";
	reclaimer.drain();
	Ref<Holder> holder = Holder::create();
	holder->field = Item::create(1);
	auto threadId = std::make_shared<std::atomic<uint64_t>>(0);
	std::vector<std::function<void()>> bodies = {
		[holder, threadId] {
			threadId->store(ThreadContext::current().threadId());
			{
				TaskScope pinning(TaskInfo{std::source_location::current(), PINNING_KIND});
				(void)holder->field.get(); // publishes the epoch
				pct::yieldPoint("reproducer:published");
			}
			{
				TaskScope borrowFree(TaskInfo{std::source_location::current(), BORROW_FREE_KIND});
				pct::yieldPoint("reproducer:inBorrowFreeTask");
			}
		},
		[] { Reclaimer::getInstance().reclaimNow(); },
	};
	pct::Options options;
	options.script = {{0, "reproducer:published"}, {1, "Reclaimer::scan:take"}, {0, "reproducer:inBorrowFreeTask"}, {1, ""}, {0, ""}};
	pct::ScheduleResult result = pct::PctScheduler(options).run(std::move(bodies));
	AION_ASSERT_SCHEDULE_OK(result);
	Reclaimer::Stats stats = reclaimer.stats();
	EXPECT_EQ(stats.oldestPublishedThreadId, threadId->load()) << "the scan saw the published epoch of the scripted thread";
	EXPECT_STREQ(stats.oldestPublishedTask.kind, PINNING_KIND) << "Stats name the task running at the end of the scan, not the one holding the epoch";
	holder.reset();
	reclaimer.drain();
}

// Found while building the stress harness teardown (a smoke run logged "ThreadPoolManager: Initialized with 32 instant, 32 scheduler ..." after
// ThreadPoolManager::installBackend(nullptr)): Reclaimer::removePostScanHook returns while the hook is still running on the scanning thread (hooks
// are copied out and invoked outside hooksMutex), so CleanerQueue::uninstall() does not stop a CleanerDrain post that is already in flight; that
// hook then calls ThreadPoolManager::getInstance() after the teardown and lazily creates the default pools again (a later configure() throws).
// Expected: after removePostScanHook returns, the hook is not running and will not run again (or the contract documents that callers must
// wait for the next scan, and CleanerQueue's hook re-checks its installed flag and never creates pools).
TEST(StressReproducerTest, RemovePostScanHookWaitsForARunningHook) {
	Reclaimer& reclaimer = Reclaimer::getInstance();
	ASSERT_FALSE(reclaimer.isRunning());
	auto entered = std::make_shared<std::atomic<bool>>(false);
	auto release = std::make_shared<std::atomic<bool>>(false);
	auto finished = std::make_shared<std::atomic<bool>>(false);
	uint64_t hookId = reclaimer.addPostScanHook("reproducer", [entered, release, finished] {
		entered->store(true);
		for (int i = 0; i < 2000 && !release->load(); ++i) // bounded: at most 2 s
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		finished->store(true);
	});
	std::thread scanner([&reclaimer] { reclaimer.reclaimNow(); });
	for (int i = 0; i < 2000 && !entered->load(); ++i)
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	ASSERT_TRUE(entered->load());
	auto removed = std::make_shared<std::atomic<bool>>(false);
	std::thread remover([&reclaimer, hookId, removed] {
		reclaimer.removePostScanHook(hookId);
		removed->store(true);
	});
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	bool removalReturnedWhileHookRan = removed->load() && !finished->load();
	release->store(true);
	remover.join();
	scanner.join();
	EXPECT_TRUE(finished->load());
	EXPECT_FALSE(removalReturnedWhileHookRan) << "removePostScanHook returned while the hook was still running";
}

// Found by the stress harness teardown ("ThreadPoolManager: Initialized with 32 instant, 32 scheduler ..." logged right after
// installBackend(nullptr)): ThreadPoolManager::installBackend publishes the new backend (nullptr = lazy default pools) and clears the shutdown
// flag BEFORE destroying the previous backend, whose destructor joins tasks that are still running. A running task that uses
// ThreadPoolManager in that window (SerialExecutor handing over its next task, a CleanerDrain, any schedule/execute) sees no backend and
// lazily creates the default pools, which then run beside a test backend and make a later ThreadPoolManager::configure() throw.
// Expected: tasks of the previous backend never observe the replacement state (e.g. destroy or shut down the previous backend before the
// swap, or keep submissions rejected until it is destroyed), so no default pools exist after installBackend(nullptr) returns.
TEST(StressReproducerTest, InstallBackendDoesNotLetRunningTasksRecreateTheDefaultPools) {
	using aion::gameserver::utils::ThreadPoolManager;
	ThreadPoolManager::installBackend(nullptr);
	ThreadPoolManager::configure(ThreadPoolManager::Config{});
	ThreadPoolBackend::Options options;
	options.instantThreads = 1;
	options.scheduledThreads = 1;
	ThreadPoolManager::installBackend(std::make_unique<ThreadPoolBackend>(options));
	auto started = std::make_shared<std::atomic<bool>>(false);
	ThreadPoolManager::getInstance().execute(Pin(), [started] {
		started->store(true);
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		// a running task using the pools while the backend is being replaced (e.g. SerialExecutor's hand-over)
		ThreadPoolManager::getInstance().execute([] {});
	});
	for (int i = 0; i < 2000 && !started->load(); ++i)
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	ASSERT_TRUE(started->load());
	ThreadPoolManager::installBackend(nullptr); // joins the running task
	bool defaultPoolsCreated = false;
	try {
		ThreadPoolManager::configure(ThreadPoolManager::Config{}); // throws only if the default pools exist
	} catch (const IllegalStateException&) {
		defaultPoolsCreated = true;
	}
	EXPECT_FALSE(defaultPoolsCreated) << "a task of the replaced backend lazily created the default pools during installBackend(nullptr)";
	ThreadPoolManager::installBackend(nullptr);
	Reclaimer::getInstance().drain();
}

namespace {

struct ReproducerNode final : RetiredNode {
	size_t retiredBytes() const noexcept override { return 64; }
};

} // namespace

// Found by the 10-minute RelWithDebInfo stress run: after the teardown every object was destroyed but Reclaimer::Stats::backlog read
// 18446744073709551613 (-3), which also fired a watchdog BACKLOG dump and kept drain() from ever returning true. Reclaimer.cpp pushBatch()
// publishes the batch with the incoming-stack CAS first and only then reads batch->items.size() / batch->bytes for incomingCount/incomingBytes.
// A scan running in between takes the stack, subtracts the sizes and DELETES the batch, so pushBatch reads a freed RetireBatch (heap
// use-after-free; ASan reports it when the interleaving happens) and adds a garbage size, leaving the counters permanently wrong.
// Expected: sizes are read before the CAS (and added before publication), no batch is touched after it was published, backlog returns to 0.
// The window is a few instructions wide: the reproducer oversubscribes the cores with threads that retire outside any TaskScope (each retire is
// pushed immediately as its own batch) against a tight reclaimNow() loop, for a bounded time. It is probabilistic: the stress run hit the
// interleaving about three times in 10 minutes with 64 workers; a single reproducer run usually passes (run it repeatedly, preferably under ASan).
// Fixed in the lifetime area (pushBatch reads the sizes before publishing); stays opt-in because it is long (about 7 s in Debug) and
// probabilistic: run with --gtest_also_run_disabled_tests.
TEST(StressReproducerTest, DISABLED_ReclaimerBacklogCountersSurviveConcurrentFlushAndScan) {
	Reclaimer& reclaimer = Reclaimer::getInstance();
	ASSERT_FALSE(reclaimer.isRunning());
	reclaimer.drain();
	std::atomic<bool> stop{false};
	std::vector<std::thread> threads;
	auto hardware = std::max(2u, std::thread::hardware_concurrency());
	for (unsigned t = 0; t < 2 * hardware; ++t) {
		threads.emplace_back([&stop] {
			for (int i = 0; i < 100'000 && !stop.load(std::memory_order_relaxed); ++i) // bounded, so the final drain stays short
				Reclaimer::retireNode(std::make_unique<ReproducerNode>()); // outside a TaskScope: pushed immediately
		});
	}
	std::thread scanner([&stop, &reclaimer] {
		while (!stop.load(std::memory_order_relaxed))
			reclaimer.reclaimNow();
	});
	bool wrapped = false;
	auto end = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	while (std::chrono::steady_clock::now() < end && !wrapped) {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		wrapped = reclaimer.stats().backlog > (uint64_t{1} << 62);
	}
	stop.store(true);
	for (std::thread& thread : threads)
		thread.join();
	scanner.join();
	bool drained = reclaimer.drain(256);
	Reclaimer::Stats stats = reclaimer.stats();
	EXPECT_FALSE(wrapped) << "Stats::backlog went negative (wrapped) while flushes and scans raced";
	EXPECT_TRUE(drained) << "drain() did not reach an empty backlog, backlog " << stats.backlog << ", bytes " << stats.backlogBytes;
	EXPECT_EQ(stats.backlog, 0u);
	EXPECT_EQ(stats.backlogBytes, 0u);
}
