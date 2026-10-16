// Conformance of the Reclaimer (design §2.4-§2.7): scan rules, cascades, parts and nodes, destroy observer, hooks, stats, the thread.

#include <gtest/gtest.h>

#include <atomic>
#include <functional>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#include "LifetimeTestSupport.h"
#include "aion/gameserver/runtime/base/ThreadContext.h"
#include "aion/gameserver/runtime/lifetime/Pct.h"
#include "aion/gameserver/runtime/lifetime/detail/RefCountedAccess.h"
#include "aion/gameserver/runtime/sync/Watchdog.h"
#include "support/PctSupport.h"

using namespace aion::gameserver::runtime;
using namespace aion::gameserver::runtime::lifetimetest;

namespace {

/** Records destruction context facts. */
class Observed final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static inline std::atomic<bool> destructorInContext{false};
	static inline std::atomic<bool> destructorInScope{true};
	static Ref<Observed> create() { return makeRef<Observed>(); }

protected:
	Observed() = default;
	~Observed() override {
		destructorInContext = Reclaimer::inDestructorContext();
		destructorInScope = TaskScope::active();
	}
};

std::atomic<const RefCounted*> observedDestroy{nullptr};
std::atomic<uint32_t> observedCount{UINT32_MAX};

void recordDestroy(const RefCounted& object) noexcept {
	observedDestroy = &object;
	observedCount = object.refCount();
}

bool waitFor(const std::function<bool()>& condition, std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)) {
	auto deadline = std::chrono::steady_clock::now() + timeout;
	while (!condition()) {
		if (std::chrono::steady_clock::now() > deadline)
			return false;
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	return true;
}

} // namespace

TEST(ReclaimerTest, ScanAdvancesTheEpochAndRecordsMinActive) {
	Reclaimer& reclaimer = Reclaimer::getInstance();
	uint64_t before = Reclaimer::currentEpoch();
	uint64_t scans = reclaimer.stats().scans;
	reclaimer.reclaimNow();
	EXPECT_EQ(Reclaimer::currentEpoch(), before + 1);
	Reclaimer::Stats stats = reclaimer.stats();
	EXPECT_EQ(stats.scans, scans + 1);
	EXPECT_EQ(stats.minActive, before + 1) << "no thread published: m == E";
	EXPECT_EQ(stats.lag.count(), 0);
	EXPECT_EQ(stats.oldestPublishedThreadId, 0u);
}

TEST(ReclaimerTest, PublishedThreadPinsReclamationAndIsReported) {
	drainReclaimer();
	auto tracker = std::make_shared<Tracker>();
	std::atomic<bool> published{false};
	std::atomic<bool> release{false};
	uint64_t pinningThread = 0;
	std::thread pinning([&] {
		TaskScope scope(AION_TASK_INFO(TaskKind::LONG_RUNNING));
		TaskScope::ensurePublished();
		pinningThread = ThreadContext::current().threadId();
		published = true;
		while (!release.load())
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
	});
	ASSERT_TRUE(waitFor([&] { return published.load(); }));
	Ref<Tracked> object = Tracked::create(tracker);
	object.reset();
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
	EXPECT_FALSE(Reclaimer::getInstance().drain(8)) << "the published thread could have borrowed the object";
	EXPECT_EQ(tracker->destroyedCount(0), 0u);
	Reclaimer::Stats stats = Reclaimer::getInstance().stats();
	EXPECT_EQ(stats.backlog, 1u);
	EXPECT_EQ(stats.oldestPublishedThreadId, pinningThread);
	EXPECT_STREQ(stats.oldestPublishedTask.kind, TaskKind::LONG_RUNNING);
	EXPECT_GE(stats.lag.count(), 15);
	EXPECT_LT(stats.minActive, stats.epoch);
	release = true;
	pinning.join();
	drainReclaimer();
	EXPECT_EQ(tracker->destroyedCount(0), 1u);
}

TEST(ReclaimerTest, CascadesWaitOneScanPerLevel) {
	drainReclaimer();
	auto tracker = std::make_shared<Tracker>();
	Ref<Tracked> grandchild = Tracked::create(tracker);
	Ref<Tracked> child = Tracked::create(tracker, std::move(grandchild));
	Ref<Tracked> parent = Tracked::create(tracker, std::move(child));
	parent.reset();
	Reclaimer::getInstance().reclaimNow();
	EXPECT_EQ(tracker->destroyedCount(2), 1u);
	EXPECT_EQ(tracker->destroyedCount(1), 0u) << "released in the parent's destructor: stamped with the current epoch";
	Reclaimer::getInstance().reclaimNow();
	EXPECT_EQ(tracker->destroyedCount(1), 1u);
	EXPECT_EQ(tracker->destroyedCount(0), 0u);
	Reclaimer::getInstance().reclaimNow();
	EXPECT_EQ(tracker->destroyedCount(0), 1u);
	EXPECT_EQ(Reclaimer::getInstance().stats().backlog, 0u);
}

TEST(ReclaimerTest, DestructorsRunInDestructorContextOutsideScopes) {
	drainReclaimer();
	Observed::create().reset();
	drainReclaimer();
	EXPECT_TRUE(Observed::destructorInContext.load());
	EXPECT_FALSE(Observed::destructorInScope.load());
	EXPECT_FALSE(Reclaimer::inDestructorContext());
}

TEST(ReclaimerTest, DestroyObserverSeesObjectsBeforeDestruction) {
	drainReclaimer();
	Reclaimer::getInstance().setDestroyObserver(&recordDestroy);
	Ref<Observed> object = Observed::create();
	const RefCounted* address = object.get();
	object.reset();
	drainReclaimer();
	Reclaimer::getInstance().setDestroyObserver(nullptr);
	EXPECT_EQ(observedDestroy.load(), address);
	EXPECT_EQ(observedCount.load(), 0u);
}

TEST(ReclaimerTest, PostScanHooksRunInReclaimerScopeAndSurviveExceptions) {
	Reclaimer& reclaimer = Reclaimer::getInstance();
	std::atomic<int> runs{0};
	const char* kind = nullptr;
	bool inContext = true;
	uint64_t failing = reclaimer.addPostScanHook("failing", [] { throw std::runtime_error("hook failure"); });
	uint64_t recording = reclaimer.addPostScanHook("recording", [&] {
		++runs;
		kind = TaskScope::currentTaskInfo().kind;
		inContext = Reclaimer::inDestructorContext();
	});
	reclaimer.reclaimNow();
	reclaimer.removePostScanHook(failing);
	reclaimer.reclaimNow();
	reclaimer.removePostScanHook(recording);
	reclaimer.reclaimNow();
	EXPECT_EQ(runs.load(), 2);
	EXPECT_STREQ(kind, TaskKind::RECLAIMER);
	EXPECT_FALSE(inContext);
}

TEST(ReclaimerTest, RetiredPartHoldsItsOwnerUntilDestroyed) {
	drainReclaimer();
	auto tracker = std::make_shared<Tracker>();
	Ref<PartOwner> owner = PartOwner::create();
	{
		TaskScope scope(testTask());
		TaskScope::ensurePublished();
		Reclaimer::retirePart(*owner, std::make_unique<TrackedPart>(*owner, tracker));
		Reclaimer::retirePart(*owner, nullptr); // ignored
		EXPECT_EQ(owner->refCount(), 2u);
		Reclaimer::getInstance().reclaimNow();
		EXPECT_EQ(tracker->destroyedCount(0), 0u) << "C12: never freed while a task that could borrow it is active";
	}
	const PartOwner* address = owner.get();
	owner.reset(); // the retired part still retains the owner
	EXPECT_EQ(address->refCount(), 1u);
	drainReclaimer();
	EXPECT_EQ(tracker->destroyedCount(0), 1u);
}

TEST(ReclaimerTest, RetiredNodesCountTowardsBacklogBytes) {
	drainReclaimer();
	auto tracker = std::make_shared<Tracker>();
	uint64_t destroyedBefore = Reclaimer::getInstance().stats().destroyedTotal;
	{
		TaskScope scope(testTask());
		TaskScope::ensurePublished();
		Reclaimer::retireNode(std::make_unique<TrackedNode>(tracker));
		Reclaimer::retireNode(std::make_unique<TrackedNode>(tracker));
		Reclaimer::retireNode(nullptr);
	}
	Reclaimer::Stats stats = Reclaimer::getInstance().stats();
	EXPECT_EQ(stats.backlog, 2u);
	EXPECT_EQ(stats.backlogBytes, 2000u);
	drainReclaimer();
	EXPECT_EQ(tracker->destroyedCount(0), 1u);
	EXPECT_EQ(tracker->destroyedCount(1), 1u);
	EXPECT_EQ(Reclaimer::getInstance().stats().destroyedTotal, destroyedBefore + 2);
	EXPECT_EQ(Reclaimer::getInstance().stats().backlogBytes, 0u);
}

#if AION_CHECKED
TEST(ReclaimerTest, CheckedBuildsCountExactObjectBytes) {
	drainReclaimer();
	Ref<Tracked> object = Tracked::create(std::make_shared<Tracker>());
	object.reset();
	EXPECT_EQ(Reclaimer::getInstance().stats().backlogBytes, sizeof(Tracked));
	drainReclaimer();
}
#endif

TEST(ReclaimerTest, ReclaimerThreadReclaimsPeriodicallyAndWhenWoken) {
	drainReclaimer();
	Reclaimer& reclaimer = Reclaimer::getInstance();
	Reclaimer::Config original = reclaimer.getConfig();
	Reclaimer::Config config = original;
	config.period = std::chrono::milliseconds(5);
	reclaimer.start(config);
	EXPECT_TRUE(reclaimer.isRunning());
	reclaimer.start(config); // idempotent
	auto tracker = std::make_shared<Tracker>();
	Tracked::create(tracker).reset();
	EXPECT_TRUE(waitFor([&] { return tracker->destroyedCount(0) == 1; })) << "periodic scan";
	reclaimer.stop();
	EXPECT_FALSE(reclaimer.isRunning());
	reclaimer.stop(); // idempotent

	config.period = std::chrono::hours(1);
	config.wakeBacklog = 10;
	reclaimer.start(config);
	std::vector<Ref<Tracked>> objects;
	auto wakeTracker = std::make_shared<Tracker>();
	for (int i = 0; i < 20; ++i)
		objects.push_back(Tracked::create(wakeTracker));
	objects.clear(); // outside scopes: pushed one by one, the 11th wakes the thread
	EXPECT_TRUE(waitFor([&] { return wakeTracker->destroyedCount(19) == 1 || wakeTracker->destroyedCount(0) == 1; })) << "backlog wake-up";
	reclaimer.stop();
	reclaimer.configure(original);
	drainReclaimer();
}

TEST(ReclaimerTest, ConcurrentScansAndReleasesDestroyEveryObjectOnce) {
	drainReclaimer();
	auto tracker = std::make_shared<Tracker>();
	SharedLocation<Tracked> location;
	std::atomic<bool> stop{false};
	std::vector<std::thread> threads;
	threads.emplace_back([&] {
		while (!stop.load())
			Reclaimer::getInstance().reclaimNow();
	});
	threads.emplace_back([&] {
		while (!stop.load())
			Reclaimer::getInstance().reclaimNow();
	});
	std::atomic<bool> violation{false};
	for (int reader = 0; reader < 2; ++reader) {
		threads.emplace_back([&] {
			try {
				while (!stop.load()) {
					TaskScope scope(testTask());
					for (int i = 0; i < 20; ++i) {
						Ptr<Tracked> borrowed = location.load();
						useTracked(*tracker, borrowed);
						Ref<Tracked> copy(borrowed);
						useTracked(*tracker, copy);
					}
				}
			} catch (const std::exception&) {
				violation = true;
			}
		});
	}
	for (size_t i = 0; i < Tracker::MAX; ++i) {
		location.store(Tracked::create(tracker));
		std::this_thread::sleep_for(std::chrono::microseconds(200));
	}
	location.store(nullptr);
	stop = true;
	for (auto& thread : threads)
		thread.join();
	drainReclaimer();
	EXPECT_FALSE(violation.load());
	EXPECT_NO_THROW(tracker->expectAllDestroyedOnce("concurrent"));
}

// Backpressure of bulk producers (World creation's parallel region loop): waits while the backlog is above the limit, never for itself.
TEST(ReclaimerTest, AwaitBacklogBelowWaitsForTheReclaimerThreadAndNeverForItself) {
	drainReclaimer();
	Reclaimer& reclaimer = Reclaimer::getInstance();
	Reclaimer::Config original = reclaimer.getConfig();
	Reclaimer::Config config = original;
	config.period = std::chrono::milliseconds(5);
	auto tracker = std::make_shared<Tracker>();
	EXPECT_TRUE(reclaimer.awaitBacklogBelow(0, std::chrono::milliseconds(10))) << "empty backlog";

	// a published thread pins the retired objects: the backlog stays above the limit
	std::atomic<bool> published{false};
	std::atomic<bool> release{false};
	std::thread pinning([&] {
		TaskScope scope(testTask());
		TaskScope::ensurePublished();
		published = true;
		while (!release.load())
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
	});
	ASSERT_TRUE(waitFor([&] { return published.load(); }));
	for (int i = 0; i < 3; ++i)
		Tracked::create(tracker).reset(); // outside scopes: pushed at once
	EXPECT_FALSE(reclaimer.awaitBacklogBelow(1, std::chrono::milliseconds(50))) << "the Reclaimer thread is not running: no waiting";

	reclaimer.start(config);
	auto started = std::chrono::steady_clock::now();
	EXPECT_FALSE(reclaimer.awaitBacklogBelow(1, std::chrono::milliseconds(100))) << "pinned: times out";
	EXPECT_GE(std::chrono::steady_clock::now() - started, std::chrono::milliseconds(100));
	{
		TaskScope scope(testTask());
		TaskScope::ensurePublished();
		auto publishedCaller = std::chrono::steady_clock::now();
		EXPECT_FALSE(reclaimer.awaitBacklogBelow(1, std::chrono::seconds(30))) << "a published caller could pin the backlog itself";
		EXPECT_LT(std::chrono::steady_clock::now() - publishedCaller, std::chrono::seconds(5)) << "returns at once";
	}

	// the pin ends while a borrow-free task waits: the Reclaimer thread frees the objects and the wait ends
	std::thread unpin([&] {
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		release = true;
	});
	bool below = false;
	{
		TaskScope scope(testTask());
		below = reclaimer.awaitBacklogBelow(1, std::chrono::seconds(30));
		EXPECT_FALSE(TaskScope::isPublished()) << "waiting does not publish";
	}
	EXPECT_TRUE(below);
	unpin.join();
	pinning.join();
	EXPECT_TRUE(waitFor([&] { return tracker->destroyedCount(0) == 1 && tracker->destroyedCount(2) == 1; }));
	reclaimer.stop();
	reclaimer.configure(original);
	drainReclaimer();
}

TEST(ReclaimerTest, WatchdogProbeDumpsOnBacklogHighWaterMark) {
	drainReclaimer();
	Reclaimer& reclaimer = Reclaimer::getInstance();
	Reclaimer::Config original = reclaimer.getConfig();
	Reclaimer::Config config = original;
	config.period = std::chrono::hours(1); // scans only when this test asks
	config.backlogDumpObjects = 0;
	config.lagWarning = std::chrono::seconds(0);
	reclaimer.start(config);

	std::atomic<int> backlogDumps{0};
	uint64_t listener = Watchdog::getInstance().addDumpListener([&](const Watchdog::DumpReport& report) {
		if (report.reason == Watchdog::Reason::BACKLOG)
			++backlogDumps;
	});
	std::atomic<bool> published{false};
	std::atomic<bool> release{false};
	std::thread pinning([&] {
		TaskScope scope(testTask());
		TaskScope::ensurePublished();
		published = true;
		while (!release.load())
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
	});
	ASSERT_TRUE(waitFor([&] { return published.load(); }));
	Tracked::create(std::make_shared<Tracker>()).reset();
	reclaimer.reclaimNow();
	Watchdog::getInstance().checkNow();
	Watchdog::getInstance().checkNow(); // deduplicated until the backlog falls below half
	EXPECT_EQ(backlogDumps.load(), 1);

	release = true;
	pinning.join();
	reclaimer.stop();
	Watchdog::getInstance().removeDumpListener(listener);
	reclaimer.configure(original);
	drainReclaimer();
	Watchdog::getInstance().checkNow(); // the probe was removed by stop()
	EXPECT_EQ(backlogDumps.load(), 1);
}

// ------------------------------------------------------------------------------------------------------------------------------------------
// Review fixes
// ------------------------------------------------------------------------------------------------------------------------------------------

// A scan that takes and deletes a batch right after its CAS must not make the flushing thread read the freed batch or wrap the backlog counters
// (found by the stress harness: Stats::backlog = 2^64 - 3). The flushing thread is paused right after its publication, the scan runs, then
// the flush completes.
TEST(ReclaimerTest, ScanRightAfterABatchIsPublishedKeepsTheBacklogCountersExact) {
	AION_SKIP_WITHOUT_PCT();
	Reclaimer& reclaimer = Reclaimer::getInstance();
	ASSERT_FALSE(reclaimer.isRunning());
	drainReclaimer();
	auto tracker = std::make_shared<Tracker>();
	std::vector<std::function<void()>> bodies = {
		[tracker] { Reclaimer::retireNode(std::make_unique<TrackedNode>(tracker)); }, // outside any scope: pushed as its own batch
		[] { Reclaimer::getInstance().reclaimNow(); },
	};
	pct::Options options;
	options.script = {{0, "Reclaimer::flush:published"}, {1, ""}, {0, ""}};
	pct::ScheduleResult result = pct::PctScheduler(options).run(std::move(bodies));
	AION_ASSERT_SCHEDULE_OK(result);
	Reclaimer::Stats stats = reclaimer.stats();
	EXPECT_LT(stats.backlog, 2u) << "the counters wrapped below zero";
	EXPECT_LT(stats.backlogBytes, 2000u);
	drainReclaimer();
	EXPECT_EQ(reclaimer.stats().backlog, 0u);
	EXPECT_EQ(reclaimer.stats().backlogBytes, 0u);
	EXPECT_NO_THROW(tracker->expectAllDestroyedOnce("scan after publication"));
}

namespace {

struct SmallNode final : RetiredNode {
	size_t retiredBytes() const noexcept override { return 64; }
};

} // namespace

// Bounded real-thread variant of the stress reproducer: many single-entry flushes race a tight scan loop; the counters never wrap.
TEST(ReclaimerTest, ConcurrentFlushesAndScansNeverWrapTheBacklogCounters) {
	Reclaimer& reclaimer = Reclaimer::getInstance();
	ASSERT_FALSE(reclaimer.isRunning());
	drainReclaimer();
	std::atomic<bool> stop{false};
	std::atomic<bool> wrapped{false};
	std::vector<std::thread> threads;
	for (int t = 0; t < 8; ++t) {
		threads.emplace_back([] {
			for (int i = 0; i < 20000; ++i)
				Reclaimer::retireNode(std::make_unique<SmallNode>());
		});
	}
	std::thread scanner([&] {
		while (!stop.load(std::memory_order_relaxed)) {
			Reclaimer::getInstance().reclaimNow();
			if (Reclaimer::getInstance().stats().backlog > (uint64_t{1} << 62))
				wrapped = true;
		}
	});
	for (std::thread& thread : threads)
		thread.join();
	stop = true;
	scanner.join();
	EXPECT_FALSE(wrapped.load());
	drainReclaimer();
	EXPECT_EQ(reclaimer.stats().backlog, 0u);
	EXPECT_EQ(reclaimer.stats().backlogBytes, 0u);
}

// Stats name the task that published the oldest epoch, not whatever the thread runs when the scan finishes.
TEST(ReclaimerTest, StatsNameTheTaskThatPublishedTheOldestEpoch) {
	AION_SKIP_WITHOUT_PCT();
	static constexpr const char* PINNING_KIND = "review-pinning-task";
	static constexpr const char* BORROW_FREE_KIND = "review-borrow-free-task";
	Reclaimer& reclaimer = Reclaimer::getInstance();
	ASSERT_FALSE(reclaimer.isRunning());
	drainReclaimer();
	auto threadId = std::make_shared<std::atomic<uint64_t>>(0);
	std::vector<std::function<void()>> bodies = {
		[threadId] {
			threadId->store(ThreadContext::current().threadId());
			{
				TaskScope pinning(TaskInfo{std::source_location::current(), PINNING_KIND});
				TaskScope::ensurePublished();
				pct::yieldPoint("test:published");
			}
			{
				TaskScope borrowFree(TaskInfo{std::source_location::current(), BORROW_FREE_KIND});
				pct::yieldPoint("test:inBorrowFreeTask");
			}
		},
		[] { Reclaimer::getInstance().reclaimNow(); },
	};
	pct::Options options;
	options.script = {{0, "test:published"}, {1, "Reclaimer::scan:take"}, {0, "test:inBorrowFreeTask"}, {1, ""}, {0, ""}};
	pct::ScheduleResult result = pct::PctScheduler(options).run(std::move(bodies));
	AION_ASSERT_SCHEDULE_OK(result);
	Reclaimer::Stats stats = reclaimer.stats();
	EXPECT_EQ(stats.oldestPublishedThreadId, threadId->load());
	EXPECT_STREQ(stats.oldestPublishedTask.kind, PINNING_KIND);
	drainReclaimer();
}

// removePostScanHook returns only after a hook that is running on the scanning thread has finished.
TEST(ReclaimerTest, RemovePostScanHookWaitsForARunningHook) {
	Reclaimer& reclaimer = Reclaimer::getInstance();
	ASSERT_FALSE(reclaimer.isRunning());
	auto entered = std::make_shared<std::atomic<bool>>(false);
	auto release = std::make_shared<std::atomic<bool>>(false);
	auto finished = std::make_shared<std::atomic<bool>>(false);
	uint64_t hookId = reclaimer.addPostScanHook("review", [entered, release, finished] {
		entered->store(true);
		for (int i = 0; i < 2000 && !release->load(); ++i) // bounded: at most 2 s
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		finished->store(true);
	});
	std::thread scanner([&reclaimer] { reclaimer.reclaimNow(); });
	ASSERT_TRUE(waitFor([&] { return entered->load(); }));
	auto returnedWhileRunning = std::make_shared<std::atomic<bool>>(false);
	std::thread remover([&reclaimer, hookId, finished, returnedWhileRunning] {
		reclaimer.removePostScanHook(hookId);
		returnedWhileRunning->store(!finished->load());
	});
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	release->store(true);
	remover.join();
	scanner.join();
	EXPECT_FALSE(returnedWhileRunning->load()) << "removePostScanHook returned while the hook was still running";
	entered->store(false);
	reclaimer.reclaimNow();
	EXPECT_FALSE(entered->load()) << "the removed hook ran again";
}

// A hook may remove itself (and the destroy observer) from the scanning thread without waiting for its own scan.
TEST(ReclaimerTest, HookMayRemoveItselfFromTheScanningThread) {
	Reclaimer& reclaimer = Reclaimer::getInstance();
	auto hookId = std::make_shared<std::atomic<uint64_t>>(0);
	auto runs = std::make_shared<std::atomic<int>>(0);
	hookId->store(reclaimer.addPostScanHook("self-removing", [hookId, runs] {
		runs->fetch_add(1);
		Reclaimer::getInstance().removePostScanHook(hookId->load());
		Reclaimer::getInstance().setDestroyObserver(nullptr);
	}));
	reclaimer.reclaimNow();
	reclaimer.reclaimNow();
	EXPECT_EQ(runs->load(), 1);
}

namespace {

std::atomic<bool> inSlowObserver{false};
std::atomic<bool> releaseSlowObserver{false};
std::atomic<bool> slowObserverFinished{false};

void slowObserver(const RefCounted&) noexcept {
	inSlowObserver = true;
	for (int i = 0; i < 2000 && !releaseSlowObserver.load(); ++i) // bounded: at most 2 s
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	slowObserverFinished = true;
}

} // namespace

// setDestroyObserver returns only after a scan that may still call the previous observer has finished.
TEST(ReclaimerTest, ReplacingTheDestroyObserverWaitsForARunningScan) {
	Reclaimer& reclaimer = Reclaimer::getInstance();
	ASSERT_FALSE(reclaimer.isRunning());
	drainReclaimer();
	inSlowObserver = false;
	releaseSlowObserver = false;
	slowObserverFinished = false;
	reclaimer.setDestroyObserver(&slowObserver);
	Tracked::create(std::make_shared<Tracker>()).reset(); // outside scopes: queued for the next scan
	std::thread scanner([&reclaimer] { reclaimer.reclaimNow(); });
	ASSERT_TRUE(waitFor([] { return inSlowObserver.load(); }));
	std::atomic<bool> returnedWhileRunning{false};
	std::thread remover([&] {
		reclaimer.setDestroyObserver(nullptr);
		returnedWhileRunning = !slowObserverFinished.load();
	});
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	releaseSlowObserver = true;
	remover.join();
	scanner.join();
	EXPECT_FALSE(returnedWhileRunning.load());
	drainReclaimer();
}
