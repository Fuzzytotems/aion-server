// ThreadPoolManager singleton, configuration and debug modes (design §1.5, §1.6, §7.1), PinnedCallback capture rules (§7.3).

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <typeinfo>

#include "SchedTestSupport.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/PinnedCallback.h"

namespace aion::gameserver::runtime::schedtest {
namespace {

using namespace std::chrono_literals;

class ThreadPoolManagerTest : public testing::Test {
protected:
	void TearDown() override {
		ThreadPoolManager::installBackend(nullptr);
		ThreadPoolManager::configure(testConfig());
		ForkJoinPool::commonPool().setSerial(false);
		reclaimAll();
	}
};

bool waitReal(const std::function<bool()>& predicate, std::chrono::milliseconds limit = 10s) {
	for (auto deadline = std::chrono::steady_clock::now() + limit; !predicate();) {
		if (std::chrono::steady_clock::now() >= deadline)
			return false;
		std::this_thread::sleep_for(1ms);
	}
	return true;
}

TEST_F(ThreadPoolManagerTest, DefaultPoolsAreCreatedFromTheConfigurationOnFirstUse) {
	ThreadPoolManager::installBackend(nullptr);
	ThreadPoolManager::Config config = testConfig();
	config.baseThreadPoolSize = 2; // max(4, 2)
	config.scheduledThreadPoolSize = 5;
	ThreadPoolManager::configure(config);
	ThreadPoolManager& pools = ThreadPoolManager::getInstance();
	auto* backend = dynamic_cast<ThreadPoolBackend*>(&pools.backend());
	ASSERT_NE(backend, nullptr);
	EXPECT_EQ(backend->getInstantPoolSize(), 4);
	EXPECT_EQ(backend->getScheduledPoolSize(), 5);
	EXPECT_EQ(pools.getConfig().scheduledThreadPoolSize, 5);
	EXPECT_THROW(ThreadPoolManager::configure(config), IllegalStateException) << "the pools already exist";

	std::atomic<bool> ranOnPool{false};
	pools.execute(Pin(), [&] { ranOnPool = pools.backend().isExecutorThread(); });
	ASSERT_TRUE(waitReal([&] { return ranOnPool.load(); }));
	std::vector<std::string> stats = pools.getStats();
	ASSERT_GE(stats.size(), 30u);
	EXPECT_EQ(stats[1], "Scheduled pool:");
	EXPECT_EQ(stats[12], "Instant pool:");
	EXPECT_EQ(stats[23], "Long running pool:");
}

TEST_F(ThreadPoolManagerTest, SingleExecutorModeRunsEveryPoolOnOneThreadAndGetHelps) {
	ThreadPoolManager::installBackend(nullptr);
	ThreadPoolManager::Config config = testConfig();
	config.singleExecutor = true;
	ThreadPoolManager::configure(config);
	ThreadPoolManager& pools = ThreadPoolManager::getInstance();
	ASSERT_NE(dynamic_cast<SingleExecutorBackend*>(&pools.backend()), nullptr);
	EXPECT_TRUE(ForkJoinPool::commonPool().isSerial());

	std::atomic<std::thread::id> instantThread{};
	std::atomic<std::thread::id> scheduledThread{};
	std::atomic<std::thread::id> longThread{};
	std::atomic<bool> helped{false};
	std::atomic<bool> timedOut{false};
	pools.execute(Pin(), [&] {
		instantThread = std::this_thread::get_id();
		// without helping, this get() would deadlock: the awaited task needs the only executor thread
		FutureRef inner = pools.schedule(Pin(), [&] { scheduledThread = std::this_thread::get_id(); }, 20);
		FutureRef longRunning = pools.submitLongRunning(Pin(), [&] { longThread = std::this_thread::get_id(); });
		inner->get();
		longRunning->get(1, TimeUnit::SECONDS);
		helped = true;
		FutureRef never = pools.schedule(Pin(), [] {}, 60'000);
		try {
			never->get(30, TimeUnit::MILLISECONDS);
		} catch (const TimeoutException&) {
			timedOut = true;
		}
		(void)never->cancel(false);
	});
	ASSERT_TRUE(waitReal([&] { return helped.load() && timedOut.load(); }));
	EXPECT_EQ(instantThread.load(), scheduledThread.load());
	EXPECT_EQ(instantThread.load(), longThread.load());
	EXPECT_NE(instantThread.load(), std::this_thread::get_id());

	// FixPath on the single executor: a self-cancelling periodic task awaited with get(timeout)
	std::atomic<bool> cancelled{false};
	std::atomic<int32_t> runs{0};
	pools.execute(Pin(), [&] {
		FutureRef waitTask = pools.scheduleAtFixedRate(Pin(), [&](Future& self) {
			if (runs.fetch_add(1) + 1 == 3)
				(void)self.cancel(true);
		}, 10, 5);
		try {
			waitTask->get(5, TimeUnit::SECONDS);
		} catch (const CancellationException&) {
			cancelled = true;
		}
	});
	ASSERT_TRUE(waitReal([&] { return cancelled.load(); }));
	EXPECT_EQ(runs.load(), 3);

	pools.shutdown();
	EXPECT_TRUE(pools.isShutdown());
}

TEST_F(ThreadPoolManagerTest, InstallBackendReplacesThePools) {
	ManualClock clock;
	auto first = std::make_unique<DeterministicExecutor>(clock, 1);
	DeterministicExecutor* raw = first.get();
	ThreadPoolManager::installBackend(std::move(first));
	EXPECT_EQ(&ThreadPoolManager::getInstance().backend(), raw);
	ThreadPoolManager::Config config = testConfig();
	config.maximumRuntimeInMillisecWithoutWarning = 1234;
	ThreadPoolManager::configure(config) ; // allowed: no default pools
	EXPECT_EQ(ThreadPoolManager::getInstance().getConfig().maximumRuntimeInMillisecWithoutWarning, 1234);
	ThreadPoolManager::getInstance().shutdown();
	EXPECT_TRUE(ThreadPoolManager::getInstance().isShutdown());
	ThreadPoolManager::installBackend(std::make_unique<DeterministicExecutor>(clock, 2));
	EXPECT_FALSE(ThreadPoolManager::getInstance().isShutdown()) << "installBackend starts over";
}

// Review finding: installBackend destroyed the previous backend while lock-free readers (submitters, Future::getDelay/get, Reclaimer post-scan
// hooks) could still use it. A replaced backend is now retired (shut down, threads joined) and kept alive: late use through a reference loaded
// before the swap is memory-safe and its submissions are cancelled (ASan would report the old use-after-free here).
TEST_F(ThreadPoolManagerTest, AReplacedBackendStaysUsableForReadersThatLoadedItBeforeTheSwap) {
	ThreadPoolBackend::Options options;
	options.instantThreads = 1;
	options.scheduledThreads = 1;
	ThreadPoolManager::installBackend(std::make_unique<ThreadPoolBackend>(options));
	ExecutorBackend& old = ThreadPoolManager::getInstance().backend();
	EXPECT_EQ(ThreadPoolManager::installedBackend(), &old);
	Ref<Npc> npc = Npc::create();
	FutureRef delayed = ThreadPoolManager::getInstance().schedule(Pin(npc.get()), [] {}, 60'000);
	EXPECT_EQ(npc->refCount(), 2u);

	ManualClock clock;
	ThreadPoolManager::installBackend(std::make_unique<DeterministicExecutor>(clock, 3));
	EXPECT_NE(ThreadPoolManager::installedBackend(), &old);
	EXPECT_TRUE(delayed->isCancelled()) << "retiring cancels the pending timers of the replaced backend";
	EXPECT_EQ(npc->refCount(), 1u) << "and releases their captures";
	EXPECT_TRUE(old.isShutdown());
	EXPECT_TRUE(old.pendingTasks().empty());
	EXPECT_FALSE(old.getStats().empty());
	(void)old.clock().now();
	FutureRef late = Future::create(Pin(), Future::Body([](Future&) {}), AION_TASK_INFO(TaskKind::TEST), Future::Schedule{PoolKind::INSTANT});
	old.execute(PoolKind::INSTANT, late); // a submitter that loaded the old backend before the swap
	EXPECT_TRUE(late->isCancelled()) << "late submissions to a retired backend are cancelled and dropped";
	EXPECT_EQ(&ThreadPoolManager::clock(), &clock) << "clock() follows the installed backend";
	ThreadPoolManager::installBackend(nullptr);
	EXPECT_EQ(ThreadPoolManager::installedBackend(), nullptr);
	EXPECT_EQ(&ThreadPoolManager::clock(), &SystemClock::getInstance()) << "clock() never creates the default pools";
	EXPECT_EQ(ThreadPoolManager::submitIfInstalled([] {}), nullptr) << "submitIfInstalled never creates the default pools";
	EXPECT_EQ(ThreadPoolManager::installedBackend(), nullptr);
	EXPECT_NO_THROW(ThreadPoolManager::configure(testConfig())) << "no default pools were created";
}

// Review finding / stress reproducer: a task still running on the replaced backend that uses ThreadPoolManager during installBackend(nullptr)
// must not lazily create the default pools (it sees the retired backend, whose submissions are cancelled).
TEST_F(ThreadPoolManagerTest, RunningTasksOfAReplacedBackendDoNotRecreateTheDefaultPools) {
	ThreadPoolBackend::Options options;
	options.instantThreads = 1;
	options.scheduledThreads = 1;
	ThreadPoolManager::installBackend(std::make_unique<ThreadPoolBackend>(options));
	auto started = std::make_shared<std::atomic<bool>>(false);
	auto lateCancelled = std::make_shared<std::atomic<bool>>(false);
	ThreadPoolManager::getInstance().execute(Pin(), [started, lateCancelled] {
		started->store(true);
		std::this_thread::sleep_for(200ms);
		FutureRef late = ThreadPoolManager::getInstance().submit([] {});
		lateCancelled->store(late->isCancelled());
	});
	ASSERT_TRUE(waitReal([&] { return started->load(); }));
	ThreadPoolManager::installBackend(nullptr); // waits for the running task
	EXPECT_TRUE(lateCancelled->load());
	EXPECT_EQ(ThreadPoolManager::installedBackend(), nullptr);
	EXPECT_NO_THROW(ThreadPoolManager::configure(testConfig())) << "a task of the replaced backend created the default pools";
}

// ------------------------------------------------------------------------------------------------------------------------ PinnedCallback

struct Observer : TaskStruct {
	int32_t id = 0;
	Ref<Npc> npc;
	void operator()(int32_t value) const { npc->runs.fetch_add(value + id); }
};

template <class F>
concept UnpinnedCallbackOfInt = std::constructible_from<PinnedCallback<void(int32_t)>, F>;
static_assert(UnpinnedCallbackOfInt<Observer>);
static_assert(UnpinnedCallbackOfInt<decltype([](int32_t) {})>);
static_assert(!UnpinnedCallbackOfInt<decltype([x = 1](int32_t) { (void)x; })>, "captures need a Pin");

TEST_F(ThreadPoolManagerTest, PinnedCallbackKeepsItsOwnersAliveUntilTheLastCopyIsGone) {
	Ref<Npc> npc = Npc::create();
	Npc* raw = npc.get();
	uint_least32_t line = std::source_location::current().line() + 1;
	PinnedCallback<void(int32_t)> callback(Pin(raw), [raw](int32_t value) { raw->runs.fetch_add(value); });
	EXPECT_EQ(callback.getTaskInfo().where.line(), line);
	EXPECT_STREQ(callback.getTaskInfo().kind, TaskKind::CALLBACK);
	EXPECT_EQ(npc->refCount(), 2u);
	{
		PinnedCallback<void(int32_t)> copy = callback;
		EXPECT_EQ(npc->refCount(), 2u) << "copies share one implementation";
		callback.reset();
		EXPECT_EQ(npc->refCount(), 2u);
		copy(5);
		EXPECT_FALSE(copy == callback);
	}
	EXPECT_EQ(npc->refCount(), 1u);
	EXPECT_EQ(raw->runs.load(), 5);
	EXPECT_THROW(callback(1), NullPointerException);

	PinnedCallback<void(int32_t)> observer(Observer{{}, 10, npc});
	EXPECT_EQ(observer.target_type(), typeid(Observer));
	ASSERT_NE(observer.target<Observer>(), nullptr) << "typed access to the stored callable (SiegeStartRunnable.getLocationId)";
	EXPECT_EQ(observer.target<Observer>()->id, 10);
	EXPECT_EQ(observer.target<int32_t>(), nullptr) << "exact type only";
	EXPECT_EQ(PinnedCallback<void(int32_t)>().target<Observer>(), nullptr);
	EXPECT_FALSE(observer.pins(*npc)) << "a TaskStruct retains through its Ref member, not a Pin";
	observer(1);
	EXPECT_EQ(raw->runs.load(), 16);
	PinnedCallback<void(int32_t)> bound(bindTask([](int32_t value, Npc& target) { target.runs.fetch_add(value * 100); }, npc));
	bound(1);
	EXPECT_EQ(raw->runs.load(), 116);
}

} // namespace
} // namespace aion::gameserver::runtime::schedtest
