// DeterministicExecutor + ManualClock (design §7.7): ordering, runReady/advance, exact reclamation, helping get, seeded Rnd, shutdown.

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "SchedTestSupport.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::runtime::schedtest {
namespace {

using namespace std::chrono_literals;

class DeterministicExecutorTest : public testing::Test {
protected:
	void SetUp() override {
		ThreadPoolManager::installBackend(nullptr);
		ThreadPoolManager::configure(testConfig());
		auto owned = std::make_unique<DeterministicExecutor>(clock, 7);
		executor = owned.get();
		ThreadPoolManager::installBackend(std::move(owned));
	}
	void TearDown() override {
		ThreadPoolManager::installBackend(nullptr);
		reclaimAll();
	}

	ThreadPoolManager& pools() { return ThreadPoolManager::getInstance(); }

	ManualClock clock;
	DeterministicExecutor* executor = nullptr;
	std::vector<std::string> order;
};

TEST_F(DeterministicExecutorTest, NothingRunsOnItsOwn) {
	std::vector<std::string>* log = &order;
	(void)pools().schedule(Pin(), [log] { log->push_back("timer"); }, 0);
	pools().execute(Pin(), [log] { log->push_back("instant"); });
	EXPECT_TRUE(order.empty());
	EXPECT_EQ(executor->pendingTaskCount(), 2u);
	EXPECT_EQ(executor->runReady(), 2u);
	EXPECT_EQ((std::vector<std::string>{"timer", "instant"}), order) << "due timers before queued tasks";
	EXPECT_EQ(executor->pendingTasksCount(), 0u);
}

TEST_F(DeterministicExecutorTest, TimersRunInDueThenSequenceOrder) {
	std::vector<std::string>* log = &order;
	(void)pools().schedule(Pin(), [log] { log->push_back("30"); }, 30);
	(void)pools().schedule(Pin(), [log] { log->push_back("10a"); }, 10);
	(void)pools().schedule(Pin(), [log] { log->push_back("10b"); }, 10);
	(void)pools().schedule(Pin(), [log] { log->push_back("20"); }, 20);
	pools().execute(Pin(), [log] { log->push_back("i1"); });
	pools().executeLongRunning(Pin(), [log] { log->push_back("l1"); });
	pools().execute(Pin(), [log] { log->push_back("i2"); });
	EXPECT_EQ(executor->nextDueTime(), clock.now() + 10ms);
	EXPECT_EQ(executor->advance(9ms), 3u);
	EXPECT_EQ((std::vector<std::string>{"i1", "l1", "i2"}), order) << "queued tasks in submission order, pools interleaved";
	EXPECT_EQ(executor->advance(100ms), 4u);
	EXPECT_EQ((std::vector<std::string>{"i1", "l1", "i2", "10a", "10b", "20", "30"}), order);
}

TEST_F(DeterministicExecutorTest, TasksSubmittedWhileRunningRunInTheSameCallWhenDue) {
	std::vector<std::string>* log = &order;
	pools().execute(Pin(), [this, log] {
		log->push_back("a");
		pools().execute(Pin(), [log] { log->push_back("b"); });
		(void)pools().schedule(Pin(), [log] { log->push_back("now"); }, 0);
		(void)pools().schedule(Pin(), [log] { log->push_back("later"); }, 1);
	});
	EXPECT_EQ(executor->runReady(), 3u);
	EXPECT_EQ((std::vector<std::string>{"a", "now", "b"}), order);
	EXPECT_EQ(executor->pendingTaskCount(), 1u);
}

TEST_F(DeterministicExecutorTest, AdvanceRunsPeriodicTasksAtEveryPeriod) {
	std::vector<int64_t> times;
	auto start = clock.now();
	FutureRef task = pools().scheduleAtFixedRate(Pin(), [&] { times.push_back((clock.now() - start) / 1ms); }, 0, 100);
	EXPECT_EQ(executor->advance(1000ms), 11u);
	EXPECT_EQ((std::vector<int64_t>{0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000}), times);
	EXPECT_EQ(clock.now() - start, 1000ms);
	EXPECT_EQ(task->getDelay(), 100);
	EXPECT_TRUE(task->cancel(false));
	EXPECT_EQ(executor->pendingTaskCount(), 0u) << "cancelled shells are not pending";
	EXPECT_FALSE(executor->nextDueTime().has_value());
}

TEST_F(DeterministicExecutorTest, DestructionIsExactAfterEachTask) {
	int32_t liveBefore = Npc::live.load();
	int32_t liveDuringSecondTask = -1;
	{
		Ref<Npc> npc = Npc::create();
		pools().execute(Pin(), [captured = npc] { captured->runs.fetch_add(1); });
	}
	pools().execute(Pin(), [&] { liveDuringSecondTask = Npc::live.load(); });
	EXPECT_EQ(Npc::live.load(), liveBefore + 1) << "kept alive by the task's capture";
	EXPECT_EQ(executor->runReady(), 2u);
	EXPECT_EQ(liveDuringSecondTask, liveBefore) << "reclaimed right after the first task";
	EXPECT_EQ(Npc::live.load(), liveBefore);
}

TEST_F(DeterministicExecutorTest, HelpingGetAdvancesTheClock) {
	auto start = clock.now();
	Ref<Npc> npc = Npc::create();
	Npc* raw = npc.get();
	FutureRef task = pools().schedule(raw, [raw] { raw->runs.fetch_add(1); }, 5, TimeUnit::SECONDS);
	task->get(); // the test thread created the executor: get() helps
	EXPECT_EQ(raw->runs.load(), 1);
	EXPECT_EQ(clock.now() - start, 5s);

	FutureRef far = pools().schedule(raw, [raw] { raw->runs.fetch_add(1); }, 10, TimeUnit::SECONDS);
	EXPECT_THROW(far->get(1, TimeUnit::SECONDS), TimeoutException);
	EXPECT_EQ(clock.now() - start, 6s) << "a timed out get() let exactly the timeout pass";
	EXPECT_EQ(far->getDelay(TimeUnit::SECONDS), 9);
	EXPECT_TRUE(far->cancel(false));

	// a task completed by another thread while nothing is scheduled: real-time polling
	FutureRef external = pools().submit(Pin(), [] {});
	std::thread other;
	{
		FutureRef deferred = Future::deferred(Pin(), [] {});
		other = std::thread([deferred] {
			std::this_thread::sleep_for(20ms);
			deferred->run();
		});
		executor->runReady();
		deferred->get();
		EXPECT_TRUE(deferred->isDone());
	}
	other.join();
	EXPECT_TRUE(external->isDone());
}

TEST_F(DeterministicExecutorTest, SeedsTheRndGeneratorOfTheCreatingThread) {
	uint64_t first = commons::utils::Rnd::generator()();
	uint64_t second = commons::utils::Rnd::generator()();
	ThreadPoolManager::installBackend(std::make_unique<DeterministicExecutor>(clock, 7)); // same seed again
	EXPECT_EQ(commons::utils::Rnd::generator()(), first);
	EXPECT_EQ(commons::utils::Rnd::generator()(), second);
	executor = nullptr;
}

TEST_F(DeterministicExecutorTest, ShutdownDropsTimersAndRunsQueuedTasks) {
	std::vector<std::string>* log = &order;
	FutureRef timer = pools().schedule(Pin(), [log] { log->push_back("timer"); }, 10);
	pools().execute(Pin(), [log] { log->push_back("queued"); });
	EXPECT_TRUE(executor->shutdown(0ms));
	EXPECT_TRUE(executor->isShutdown());
	EXPECT_TRUE(timer->isCancelled());
	EXPECT_EQ((std::vector<std::string>{"queued"}), order);
	FutureRef late = pools().submit(Pin(), [log] { log->push_back("late"); });
	EXPECT_TRUE(late->isCancelled());
	EXPECT_EQ(executor->advance(1000ms), 0u);
	EXPECT_FALSE(executor->getStats().empty());
}

TEST_F(DeterministicExecutorTest, DestroyingTheExecutorCancelsPendingTasks) {
	Ref<Npc> npc = Npc::create();
	Npc* raw = npc.get();
	FutureRef timer = pools().schedule(raw, [raw] { raw->runs.fetch_add(1); }, 10);
	pools().execute(raw, [raw] { raw->runs.fetch_add(1); });
	EXPECT_EQ(npc->refCount(), 3u);
	ThreadPoolManager::installBackend(nullptr);
	executor = nullptr;
	EXPECT_TRUE(timer->isCancelled());
	EXPECT_EQ(npc->refCount(), 1u);
	EXPECT_EQ(raw->runs.load(), 0);
}

/** design P3 "50k-timer load": ordering holds and cancelled shells are purged; prints the measured cost per operation. */
TEST_F(DeterministicExecutorTest, FiftyThousandTimers) {
	constexpr int32_t TIMERS = 50'000;
	std::vector<int64_t> dueOrder;
	dueOrder.reserve(TIMERS);
	std::vector<FutureRef> tasks;
	tasks.reserve(TIMERS);
	auto start = clock.now();
	auto scheduleBegin = std::chrono::steady_clock::now();
	for (int32_t i = 0; i < TIMERS; ++i) {
		int64_t delay = static_cast<int64_t>(commons::utils::Rnd::generator()() % 60'000);
		tasks.push_back(pools().schedule(Pin(), [&dueOrder, this, start] { dueOrder.push_back((clock.now() - start) / 1ms); }, delay));
	}
	auto scheduleNanos = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - scheduleBegin).count();
	auto cancelBegin = std::chrono::steady_clock::now();
	for (int32_t i = 0; i < TIMERS; i += 3)
		(void)tasks[static_cast<size_t>(i)]->cancel(false);
	auto cancelNanos = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - cancelBegin).count();
	tasks.clear();
	EXPECT_EQ(executor->pendingTaskCount(), static_cast<size_t>(TIMERS - (TIMERS + 2) / 3));
	auto runBegin = std::chrono::steady_clock::now();
	executor->advance(60'000ms);
	auto runNanos = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - runBegin).count();
	ASSERT_EQ(dueOrder.size(), static_cast<size_t>(TIMERS - (TIMERS + 2) / 3));
	EXPECT_TRUE(std::is_sorted(dueOrder.begin(), dueOrder.end()));
	std::printf("[ measure  ] schedule %lld ns/op, cancel %lld ns/op, pop+run+reclaimNow %lld ns/op\n", static_cast<long long>(scheduleNanos / TIMERS),
		static_cast<long long>(cancelNanos / (TIMERS / 3)), static_cast<long long>(runNanos / static_cast<int64_t>(dueOrder.size())));
}

} // namespace
} // namespace aion::gameserver::runtime::schedtest
