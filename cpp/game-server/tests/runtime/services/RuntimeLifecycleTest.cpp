// RuntimeLifecycle (design §1.1, §5.4, §6, §7, §11): start and shutdown of the kernel services on the DeterministicExecutor and on real pools,
// tasks, cron jobs and auto-release ids pending at shutdown, LeakCensus after shutdown, used-ids seeding, rollback of a failed start and the
// double start/shutdown rules.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include "ServicesTestSupport.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/ForkJoinPool.h"
#include "aion/gameserver/runtime/services/RuntimeLifecycle.h"

namespace aion::gameserver::runtime::servicestest {
namespace {

using namespace std::chrono;
using services::cron::CronJob;
using services::cron::CronService;
using services::cron::CronServiceException;
using utils::idfactory::IDFactoryError;
using State = RuntimeLifecycle::State;

class RuntimeLifecycleTest : public testing::Test {
protected:
	void SetUp() override {
		resetRuntime();
		TestObject::live.store(0);
	}

	void TearDown() override {
		if (RuntimeLifecycle::isRunning())
			(void)RuntimeLifecycle::shutdown();
		resetRuntime();
		Reclaimer::getInstance().drain();
	}

	static void resetRuntime() {
		RuntimeLifecycle::resetForTests();
		(void)CleanerQueue::drainNow();
	}

	/** options for the deterministic harness: no kernel threads, the executor installed through the options */
	RuntimeLifecycle::Options deterministicOptions() {
		RuntimeLifecycle::Options options;
		auto backend = std::make_unique<DeterministicExecutor>(clock, 7);
		executor = backend.get();
		options.backend = std::move(backend);
		options.startReclaimerThread = false;
		options.startWatchdog = false;
		return options;
	}

	ManualClock clock{JAN_1_2024_MILLIS};
	DeterministicExecutor* executor = nullptr;
};

TEST_F(RuntimeLifecycleTest, DeterministicStartWiresEveryService) {
	LogCapture idLog("com.aionemu.gameserver.utils.idfactory.IDFactory");
	RuntimeLifecycle::Options options = deterministicOptions();
	options.idFactory.releaseDelay = seconds(0);
	options.usedIds = {{"PlayerDAO", [] { return std::vector<int32_t>{5, 7}; }}, {"InventoryDAO", [] { return std::vector<int32_t>{100}; }}};
	RuntimeLifecycle::start(std::move(options));

	EXPECT_EQ(RuntimeLifecycle::getState(), State::RUNNING);
	EXPECT_EQ(ThreadPoolManager::installedBackend(), executor);
	EXPECT_TRUE(CleanerQueue::isInstalled());
	EXPECT_TRUE(LeakCensus::getInstance().isInstalled());
	EXPECT_FALSE(Reclaimer::getInstance().isRunning());
	EXPECT_FALSE(Watchdog::getInstance().isRunning());
	EXPECT_TRUE(ForkJoinPool::commonPool().isSerial());
	EXPECT_EQ(CronService::getInstance().getDriver(), CronService::Driver::EXECUTOR);
	EXPECT_EQ(IDFactory::getInstance().getConfig().releaseDelay, seconds(0));
	EXPECT_EQ(IDFactory::getInstance().getUsedCount(), 4); // 0, 5, 7, 100
	EXPECT_TRUE(idLog.contains("IDFactory: 4 IDs used."));
	std::vector<int32_t> allocated;
	for (int i = 0; i < 8; ++i)
		allocated.push_back(IDFactory::getInstance().nextId());
	EXPECT_EQ(std::ranges::count_if(allocated, [](int32_t id) { return id == 0 || id == 5 || id == 7; }), 0);

	// the kernel runs: a cron job fires on the ManualClock through the ThreadPoolManagerRunnableRunner, a task runs on the executor
	std::atomic<int32_t> cronRuns{0};
	static std::atomic<int32_t>* cronCounter = nullptr;
	cronCounter = &cronRuns;
	(void)CronService::getInstance().schedule(CronJob([] { cronCounter->fetch_add(1); }), "0 * * * * ?");
	executor->advance(minutes(2));
	EXPECT_EQ(cronRuns.load(), 3); // at 00:00:00 (scheduled in a matching second), 00:01:00 and 00:02:00

	RuntimeLifecycle::ShutdownReport report = RuntimeLifecycle::shutdown();
	EXPECT_TRUE(report.performed);
	EXPECT_EQ(RuntimeLifecycle::getState(), State::SHUT_DOWN);
	EXPECT_TRUE(ThreadPoolManager::getInstance().isShutdown());
	EXPECT_THROW((void)CronService::getInstance().schedule(CronJob([] {}), "0 * * * * ?"), CronServiceException);
	EXPECT_FALSE(CleanerQueue::isInstalled());
	EXPECT_FALSE(LeakCensus::getInstance().isInstalled());
	cronCounter = nullptr;
}

TEST_F(RuntimeLifecycleTest, PendingTasksAtShutdownReleaseTheirCapturesAndIds) {
	RuntimeLifecycle::Options options = deterministicOptions();
	options.idFactory.releaseDelay = seconds(0);
	RuntimeLifecycle::start(std::move(options));

	IDFactory& ids = IDFactory::getInstance();
	int32_t scheduledId = ids.nextId();
	int32_t cronId = ids.nextId();
	int32_t queuedId = ids.nextId();
	int32_t usedBefore = ids.getUsedCount();
	static std::atomic<int32_t> queuedRuns{0};
	static std::atomic<int32_t> delayedRuns{0};
	queuedRuns.store(0);
	delayedRuns.store(0);
	{
		// a delayed task, a cron job and a queued instant task, each the last owner of an auto-release object
		Ref<TestObject> scheduled = TestObject::create(scheduledId, true);
		Ref<TestObject> cron = TestObject::create(cronId, true);
		Ref<TestObject> queued = TestObject::create(queuedId, true);
		(void)ThreadPoolManager::getInstance().schedule(Pin(scheduled), [] { delayedRuns.fetch_add(1); }, 3'600'000);
		(void)CronService::getInstance().schedule(CronJob(Pin(cron), [] {}), "0 0 12 * * ?");
		ThreadPoolManager::getInstance().execute(Pin(queued), [] { queuedRuns.fetch_add(1); });
	}
	Reclaimer::getInstance().drain();
	EXPECT_EQ(TestObject::live.load(), 3);
	EXPECT_EQ(executor->pendingTaskCount(), 3u); // delayed task, cron tick, queued task

	RuntimeLifecycle::ShutdownReport report = RuntimeLifecycle::shutdown();
	EXPECT_TRUE(report.performed);
	EXPECT_EQ(queuedRuns.load(), 1);  // DeterministicExecutor::shutdown runs queued tasks (Java: instant pool tasks get 5 s)
	EXPECT_EQ(delayedRuns.load(), 0); // delayed tasks are dropped (setExecuteExistingDelayedTasksAfterShutdownPolicy(false))
	EXPECT_EQ(report.tasksLeft, 0u);
	EXPECT_EQ(TestObject::live.load(), 0);
	EXPECT_EQ(report.cleanerIdsDrained, 3u);
	EXPECT_EQ(report.reclaimerBacklog, 0u);
	EXPECT_EQ(ids.getUsedCount(), usedBefore - 3); // released by the final CleanerDrain (release delay 0)

	// submissions after shutdown are cancelled and dropped, never run and never create new pools
	FutureRef late = ThreadPoolManager::getInstance().submit([] { queuedRuns.fetch_add(1); });
	EXPECT_TRUE(late->isCancelled());
	EXPECT_EQ(ThreadPoolManager::installedBackend(), executor);
}

TEST_F(RuntimeLifecycleTest, LeakCensusIsCleanAfterShutdown) {
	RuntimeLifecycle::Options options = deterministicOptions();
	RuntimeLifecycle::start(std::move(options));
	{
		// removed from the world while a periodic task still pins it: shutdown cancels the task, so nothing is left
		Ref<TestObject> despawned = TestObject::create(IDFactory::getInstance().nextId());
		LeakCensus::getInstance().onRemovedFromWorld(*despawned, "TestObject", despawned->objectId);
		(void)ThreadPoolManager::getInstance().scheduleAtFixedRate(Pin(despawned), [] {}, 1000, 1000);
	}
	executor->advance(seconds(3));
	EXPECT_EQ(LeakCensus::getInstance().trackedCount(), 1u);

	RuntimeLifecycle::ShutdownReport report = RuntimeLifecycle::shutdown();
	EXPECT_TRUE(report.performed);
	EXPECT_EQ(report.censusTracked, 0u);
	EXPECT_EQ(TestObject::live.load(), 0);
}

TEST_F(RuntimeLifecycleTest, ShutdownReportsObjectsStillAlive) {
	LogCapture log("com.aionemu.gameserver.runtime.RuntimeLifecycle");
	RuntimeLifecycle::start(deterministicOptions());
	Ref<TestObject> leaked = TestObject::create(IDFactory::getInstance().nextId());
	LeakCensus::getInstance().onRemovedFromWorld(*leaked, "TestObject", leaked->objectId);
	executor->runReady();

	RuntimeLifecycle::ShutdownReport report = RuntimeLifecycle::shutdown();
	EXPECT_EQ(report.censusTracked, 1u);
	EXPECT_TRUE(log.contains("warning|Runtime shutdown: 1 objects removed from the world are still alive")) << log.str();
	EXPECT_FALSE(LeakCensus::getInstance().isInstalled());
	leaked.reset();
	Reclaimer::getInstance().drain();
	EXPECT_EQ(TestObject::live.load(), 0);
}

TEST_F(RuntimeLifecycleTest, RealPoolsStartAndShutdown) {
	LogCapture poolLog("com.aionemu.gameserver.utils.ThreadPoolManager");
	RuntimeLifecycle::Options options;
	options.reclaimer.period = milliseconds(5);
	options.watchdog.writeMinidump = false;
	options.threadPool.baseThreadPoolSize = 4;
	options.threadPool.scheduledThreadPoolSize = 4;
	options.idFactory.releaseDelay = seconds(0);
	bool reclaimerWasRunning = Reclaimer::getInstance().isRunning();
	bool watchdogWasRunning = Watchdog::getInstance().isRunning();
	RuntimeLifecycle::start(std::move(options));

	EXPECT_TRUE(Reclaimer::getInstance().isRunning());
	EXPECT_TRUE(Watchdog::getInstance().isRunning());
	EXPECT_EQ(CronService::getInstance().getDriver(), CronService::Driver::THREAD);
	EXPECT_FALSE(ForkJoinPool::commonPool().isSerial());
	EXPECT_TRUE(poolLog.contains("ThreadPoolManager: Initialized with 4 instant, 4 scheduler and 0 long running threads")) << poolLog.str();

	static std::atomic<int32_t> runs{0};
	runs.store(0);
	FutureRef done = ThreadPoolManager::getInstance().submit([] { runs.fetch_add(1); });
	done->get();
	EXPECT_EQ(runs.load(), 1);

	IDFactory& ids = IDFactory::getInstance();
	int32_t usedBefore = ids.getUsedCount();
	{
		// the last owner of an auto-release object is a task that would run in an hour
		Ref<TestObject> object = TestObject::create(ids.nextId(), true);
		(void)ThreadPoolManager::getInstance().schedule(Pin(object), [] { runs.fetch_add(100); }, 3'600'000);
		ThreadPoolManager::getInstance().execute([] { std::this_thread::sleep_for(milliseconds(20)); });
	}

	RuntimeLifecycle::ShutdownReport report = RuntimeLifecycle::shutdown();
	EXPECT_TRUE(report.performed);
	EXPECT_EQ(report.tasksLeft, 0u);
	EXPECT_EQ(runs.load(), 1);
	EXPECT_EQ(TestObject::live.load(), 0);
	EXPECT_EQ(report.censusTracked, 0u);
	EXPECT_EQ(ids.getUsedCount(), usedBefore); // the auto-release id came back through the final CleanerDrain
	EXPECT_TRUE(poolLog.contains("ThreadPoolManager: Shutting down."));
	EXPECT_EQ(Reclaimer::getInstance().isRunning(), reclaimerWasRunning);
	EXPECT_EQ(Watchdog::getInstance().isRunning(), watchdogWasRunning);
}

TEST_F(RuntimeLifecycleTest, DoubleStartAndShutdownRules) {
	// shutdown before start does nothing
	EXPECT_FALSE(RuntimeLifecycle::shutdown().performed);
	EXPECT_EQ(RuntimeLifecycle::getState(), State::NEW);

	RuntimeLifecycle::start(deterministicOptions());
	DeterministicExecutor* first = executor;
	EXPECT_THROW(RuntimeLifecycle::start(deterministicOptions()), IllegalStateException);
	EXPECT_EQ(RuntimeLifecycle::getState(), State::RUNNING);
	EXPECT_EQ(ThreadPoolManager::installedBackend(), first); // the rejected start changed nothing
	EXPECT_THROW(RuntimeLifecycle::resetForTests(), IllegalStateException);

	EXPECT_TRUE(RuntimeLifecycle::shutdown().performed);
	EXPECT_FALSE(RuntimeLifecycle::shutdown().performed);
	EXPECT_THROW(RuntimeLifecycle::start(deterministicOptions()), IllegalStateException); // one run per process
	EXPECT_EQ(RuntimeLifecycle::getState(), State::SHUT_DOWN);

	RuntimeLifecycle::resetForTests();
	EXPECT_EQ(RuntimeLifecycle::getState(), State::NEW);
	RuntimeLifecycle::start(deterministicOptions());
	EXPECT_TRUE(RuntimeLifecycle::isRunning());
	EXPECT_TRUE(RuntimeLifecycle::shutdown().performed);
}

TEST_F(RuntimeLifecycleTest, ConcurrentShutdownsRunTheSequenceOnce) {
	RuntimeLifecycle::Options options;
	options.watchdog.writeMinidump = false;
	options.startWatchdog = false;
	options.threadPool.baseThreadPoolSize = 4;
	options.threadPool.scheduledThreadPoolSize = 4;
	RuntimeLifecycle::start(std::move(options));
	std::atomic<int32_t> performed{0};
	std::vector<std::thread> threads;
	for (int i = 0; i < 4; ++i)
		threads.emplace_back([&performed] {
			if (RuntimeLifecycle::shutdown().performed)
				performed.fetch_add(1);
		});
	for (std::thread& thread : threads)
		thread.join();
	EXPECT_EQ(performed.load(), 1);
	EXPECT_EQ(RuntimeLifecycle::getState(), State::SHUT_DOWN);
}

TEST_F(RuntimeLifecycleTest, ShutdownInsideATaskScopeThrows) {
	RuntimeLifecycle::start(deterministicOptions());
	{
		TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
		EXPECT_THROW((void)RuntimeLifecycle::shutdown(), IllegalStateException);
	}
	EXPECT_TRUE(RuntimeLifecycle::isRunning());
	EXPECT_TRUE(RuntimeLifecycle::shutdown().performed);
}

TEST_F(RuntimeLifecycleTest, FailedStartRollsBack) {
	LogCapture log("com.aionemu.gameserver.utils.ThreadPoolManager");
	RuntimeLifecycle::Options options;
	options.watchdog.writeMinidump = false;
	options.threadPool.baseThreadPoolSize = 4;
	options.threadPool.scheduledThreadPoolSize = 4;
	options.usedIds = {{"PlayerDAO", [] { return std::vector<int32_t>{10, 11}; }}, {"InventoryDAO", [] { return std::vector<int32_t>{11}; }}};
	bool reclaimerWasRunning = Reclaimer::getInstance().isRunning();
	bool watchdogWasRunning = Watchdog::getInstance().isRunning();
	EXPECT_THROW(RuntimeLifecycle::start(std::move(options)), IDFactoryError); // id 11 locked twice (Java: IDFactoryError at startup)

	EXPECT_EQ(RuntimeLifecycle::getState(), State::SHUT_DOWN);
	EXPECT_TRUE(ThreadPoolManager::getInstance().isShutdown());
	EXPECT_TRUE(log.contains("ThreadPoolManager: Shutting down."));
	EXPECT_THROW((void)CronService::getInstance().schedule(CronJob([] {}), "0 * * * * ?"), CronServiceException);
	EXPECT_FALSE(CleanerQueue::isInstalled());
	EXPECT_FALSE(LeakCensus::getInstance().isInstalled());
	EXPECT_EQ(Reclaimer::getInstance().isRunning(), reclaimerWasRunning);
	EXPECT_EQ(Watchdog::getInstance().isRunning(), watchdogWasRunning);
	EXPECT_FALSE(RuntimeLifecycle::shutdown().performed);
	EXPECT_THROW(RuntimeLifecycle::start(RuntimeLifecycle::Options{}), IllegalStateException);
}

TEST_F(RuntimeLifecycleTest, BackendCreatedBeforeStartIsRetired) {
	LogCapture log("com.aionemu.gameserver.runtime.RuntimeLifecycle");
	ManualClock otherClock{JAN_1_2024_MILLIS};
	auto early = std::make_unique<DeterministicExecutor>(otherClock, 1);
	DeterministicExecutor* earlyExecutor = early.get();
	ThreadPoolManager::installBackend(std::move(early));
	FutureRef cancelled = ThreadPoolManager::getInstance().schedule([] {}, 1000);

	RuntimeLifecycle::start(deterministicOptions());
	EXPECT_NE(ThreadPoolManager::installedBackend(), earlyExecutor);
	EXPECT_TRUE(cancelled->isCancelled());
	EXPECT_TRUE(log.contains("warning|RuntimeLifecycle::start: retiring a thread pool backend created before the runtime start")) << log.str();
	EXPECT_TRUE(RuntimeLifecycle::shutdown().performed);
}

} // namespace
} // namespace aion::gameserver::runtime::servicestest
