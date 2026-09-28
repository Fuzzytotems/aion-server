// CronService (design §1.1, §1.6, §7.5): scheduling, fire times on a ManualClock (EXECUTOR driver on DeterministicExecutor), misfires, time
// zones, cancel (also from inside a job), findJobs/findNextFireTimes, pins, errors, runners; the Java CronServiceTest on the real "Cron" thread;
// the single_executor mode.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "ServicesTestSupport.h"
#include "aion/gameserver/runtime/base/ThreadContext.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/ForkJoinPool.h"
#include "aion/gameserver/runtime/sched/PoolBackends.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/utils/cron/ThreadPoolManagerRunnableRunner.h"

namespace aion::gameserver::runtime::servicestest {
namespace {

using namespace std::chrono;
using services::cron::CronExpression;
using services::cron::CronExpressionParseException;
using services::cron::CronExpressions;
using services::cron::CronJob;
using services::cron::CronService;
using services::cron::CronServiceException;
using services::cron::CurrentThreadRunnableRunner;
using services::cron::JobDetail;
using services::cron::RunnableRunner;
using utils::cron::ThreadPoolManagerRunnableRunner;

sys_seconds utc(int y, unsigned mo, unsigned d, int h = 0, int mi = 0, int s = 0) {
	return sys_days(year(y) / month(mo) / day(d)) + hours(h) + minutes(mi) + seconds(s);
}

/** Records hand-offs ("instant:<tag>" / "long:<tag>") and runs the job on the calling thread. */
struct Recorder {
	std::mutex mutex;
	std::vector<std::string> calls;
	void add(std::string call) {
		std::scoped_lock lock(mutex);
		calls.push_back(std::move(call));
	}
	std::vector<std::string> take() {
		std::scoped_lock lock(mutex);
		return std::exchange(calls, {});
	}
};

class RecordingRunner final : public RunnableRunner {
public:
	explicit RecordingRunner(Recorder& recorder) : recorder(recorder) {}
	void executeRunnable(const CronJob& job) override {
		recorder.add("instant");
		job();
	}
	void executeLongRunningRunnable(const CronJob& job) override {
		recorder.add("long");
		job();
	}

private:
	Recorder& recorder;
};

struct SiegeStartRunnable : TaskStruct {
	int32_t locationId;
	void operator()() const {}
};

struct OtherRunnable : TaskStruct {
	int32_t value;
	void operator()() const {}
};

class CronServiceTest : public DeterministicServicesTest {
protected:
	void init(std::unique_ptr<RunnableRunner> runner, std::string_view zoneName = "UTC") {
		CronService::initSingleton(std::move(runner), std::chrono::locate_zone(zoneName), CronService::Driver::EXECUTOR);
	}
	CronService& cron() { return CronService::getInstance(); }
};

TEST_F(CronServiceTest, FiresExactlyAtTheFireTimeOnTheInstantPool) {
	init(std::make_unique<ThreadPoolManagerRunnableRunner>());
	std::atomic<int32_t> runs{0};
	std::string kind;
	Ref<JobDetail> job = cron().schedule(CronJob(Pin(), [&] {
		++runs;
		kind = TaskScope::currentTaskInfo().kind;
	}),
		"0 0 9 ? * *");
	EXPECT_EQ(cron().getDriver(), CronService::Driver::EXECUTOR);
	EXPECT_EQ(cron().getNextFireTime(job.get()), utc(2024, 1, 1, 9));
	EXPECT_EQ(job->getCronExpression().getCronExpression(), "0 0 9 ? * *");
	EXPECT_TRUE(job->getKey().starts_with("JobKey:Started at ms"));
	EXPECT_FALSE(job->isLongRunning());

	pass(hours(9) - seconds(1));
	EXPECT_EQ(runs.load(), 0);
	pass(seconds(1));
	EXPECT_EQ(runs.load(), 1);
	EXPECT_EQ(kind, TaskKind::INSTANT);
	EXPECT_EQ(cron().getNextFireTime(job.get()), utc(2024, 1, 2, 9));
	pass(hours(24));
	EXPECT_EQ(runs.load(), 2);
	pass(hours(23) + minutes(59));
	EXPECT_EQ(runs.load(), 2);
}

TEST_F(CronServiceTest, LongRunningJobsGoToTheLongRunningPool) {
	init(std::make_unique<ThreadPoolManagerRunnableRunner>());
	std::string kind;
	(void)cron().schedule(CronJob(Pin(), [&] { kind = TaskScope::currentTaskInfo().kind; }), "0 0 0 ? * *", true);
	pass(hours(24));
	EXPECT_EQ(kind, TaskKind::LONG_RUNNING);
}

TEST_F(CronServiceTest, RunnerRecordsTheJobCallSiteAndPool) {
	ThreadPoolManagerRunnableRunner runner;
	std::atomic<int32_t> runs{0};
	CronJob job(Pin(), [&] { ++runs; });
	runner.executeLongRunningRunnable(job);
	runner.executeRunnable(job);
	std::vector<FutureRef> pending = executor->pendingTasks();
	ASSERT_EQ(pending.size(), 2u);
	EXPECT_EQ(pending[0]->getPool(), PoolKind::LONG_RUNNING);
	EXPECT_EQ(pending[1]->getPool(), PoolKind::INSTANT);
	EXPECT_EQ(pending[0]->getTaskInfo().where.line(), job.getTaskInfo().where.line());
	pending.clear();
	executor->runReady();
	EXPECT_EQ(runs.load(), 2);
}

TEST_F(CronServiceTest, MissedFireTimesCollapseIntoOneRun) {
	Recorder recorder;
	init(std::make_unique<RecordingRunner>(recorder));
	Ref<JobDetail> job = cron().schedule(CronJob(Pin(), [] {}), "0 0 * ? * *");
	clock.advance(hours(5)); // the executor was blocked for five fire times
	executor->runReady();
	EXPECT_EQ(recorder.take().size(), 1u);
	EXPECT_EQ(cron().getNextFireTime(job.get()), utc(2024, 1, 1, 6));
	pass(hours(1));
	EXPECT_EQ(recorder.take().size(), 1u);
}

TEST_F(CronServiceTest, JobsDueTogetherAreHandedOffInSchedulingOrder) {
	Recorder recorder;
	init(std::make_unique<RecordingRunner>(recorder));
	(void)cron().schedule(CronJob(Pin(), [&] { recorder.add("first"); }), "0 0 12 ? * *");
	(void)cron().schedule(CronJob(Pin(), [&] { recorder.add("second"); }), "0 0 12 ? * *", true);
	(void)cron().schedule(CronJob(Pin(), [&] { recorder.add("earlier"); }), "0 0 11 ? * *");
	pass(hours(12));
	EXPECT_EQ(recorder.take(), (std::vector<std::string>{"instant", "earlier", "instant", "first", "long", "second"}));
}

TEST_F(CronServiceTest, EvaluatesInTheServiceTimeZone) {
	init(std::make_unique<CurrentThreadRunnableRunner>(), "Europe/Berlin");
	EXPECT_EQ(cron().getTimeZone(), std::chrono::locate_zone("Europe/Berlin"));
	std::atomic<int32_t> runs{0};
	Ref<JobDetail> job = cron().schedule(CronJob(Pin(), [&] { ++runs; }), "0 0 9 ? * *");
	EXPECT_EQ(cron().getNextFireTime(job.get()), utc(2024, 1, 1, 8)); // 09:00 CET
	pass(hours(8));
	EXPECT_EQ(runs.load(), 1);
}

TEST_F(CronServiceTest, AJobScheduledDuringAMatchingSecondFiresImmediately) {
	init(std::make_unique<CurrentThreadRunnableRunner>());
	std::atomic<int32_t> runs{0};
	clock.advance(milliseconds(500)); // 00:00:00.500
	Ref<JobDetail> job = cron().schedule(CronJob(Pin(), [&] { ++runs; }), "0 0 0 ? * *");
	EXPECT_EQ(cron().getNextFireTime(job.get()), utc(2024, 1, 1)); // Quartz computeFirstFireTime: first match after start - 1 s
	executor->runReady();
	EXPECT_EQ(runs.load(), 1);
	EXPECT_EQ(cron().getNextFireTime(job.get()), utc(2024, 1, 2));
}

TEST_F(CronServiceTest, CancelByHandleByJobAndFromInsideTheJob) {
	init(std::make_unique<ThreadPoolManagerRunnableRunner>());
	std::atomic<int32_t> runs{0};
	CronJob job(Pin(), [&] { ++runs; });
	Ref<JobDetail> first = cron().schedule(job, "0 0 1 ? * *");
	Ref<JobDetail> second = cron().schedule(job, "0 0 2 ? * *");
	Ref<JobDetail> other = cron().schedule(CronJob(Pin(), [&] { ++runs; }), "0 0 3 ? * *");
	EXPECT_EQ(cron().findJobDetails(job), (std::vector<Ref<JobDetail>>{first, second}));
	EXPECT_EQ(cron().getJobCount(), 3u);

	EXPECT_FALSE(cron().cancel(static_cast<const JobDetail*>(nullptr)));
	EXPECT_TRUE(cron().cancel(other));
	EXPECT_FALSE(cron().cancel(other));
	EXPECT_EQ(cron().getNextFireTime(other.get()), std::nullopt);
	EXPECT_TRUE(cron().cancel(job));
	EXPECT_FALSE(cron().cancel(job));
	EXPECT_TRUE(cron().findJobDetails(job).empty());
	EXPECT_EQ(cron().getJobCount(), 0u);
	pass(hours(48));
	EXPECT_EQ(runs.load(), 0);

	// AtreianPassportService.java:47-50: the job cancels its own handle
	Ref<JobDetail> self;
	self = cron().schedule(CronJob(Pin(), [&] {
		++runs;
		EXPECT_TRUE(CronService::getInstance().cancel(self));
	}),
		"0 0 9 ? * *");
	pass(hours(48));
	EXPECT_EQ(runs.load(), 1);
	EXPECT_EQ(cron().getJobCount(), 0u);
}

TEST_F(CronServiceTest, FindJobsByTypeAndNextFireTimesPerJob) {
	init(std::make_unique<CurrentThreadRunnableRunner>());
	CronJob fortress1(SiegeStartRunnable{.locationId = 1011});
	CronJob fortress2(SiegeStartRunnable{.locationId = 1131});
	CronJob fortress3(SiegeStartRunnable{.locationId = 1221});
	(void)cron().schedule(fortress1, "0 0 21 ? * FRI");
	(void)cron().schedule(fortress2, "0 0 17 ? * TUE,THU,SAT");
	Ref<JobDetail> saturday = cron().schedule(fortress3, "0 0 21 ? * SAT");
	Ref<JobDetail> monday = cron().schedule(fortress3, "0 0 19 ? * MON");
	(void)cron().schedule(CronJob(OtherRunnable{.value = 1}), "0 0 * ? * *");

	EXPECT_EQ(cron().findJobs<SiegeStartRunnable>().size(), 4u);
	EXPECT_EQ(cron().findJobs(typeid(OtherRunnable)).size(), 1u);
	EXPECT_TRUE(cron().findJobs<int>().empty());

	auto fireTimes = cron().findNextFireTimes<SiegeStartRunnable>();
	ASSERT_EQ(fireTimes.size(), 3u); // one entry per distinct job, earliest fire time
	EXPECT_EQ(fireTimes[0].first, monday);
	EXPECT_EQ(fireTimes[0].second, utc(2024, 1, 1, 19));
	EXPECT_EQ(fireTimes[1].first->getJob(), fortress2);
	EXPECT_EQ(fireTimes[1].second, utc(2024, 1, 2, 17));
	EXPECT_EQ(fireTimes[2].first->getJob(), fortress1);
	EXPECT_EQ(fireTimes[2].second, utc(2024, 1, 5, 21));
	// SiegeService.java:323-326 reads entry.getKey().getLocationId() from the job object
	std::vector<int32_t> locations;
	for (const auto& [job, fireTime] : fireTimes) {
		const SiegeStartRunnable* runnable = job->getJob().target<SiegeStartRunnable>();
		ASSERT_NE(runnable, nullptr);
		locations.push_back(runnable->locationId);
	}
	EXPECT_EQ(locations, (std::vector<int32_t>{1221, 1131, 1011}));
	EXPECT_EQ(fireTimes[0].first->getJob().target<OtherRunnable>(), nullptr);
	(void)saturday;
}

TEST_F(CronServiceTest, ThePinKeepsTheOwnerAliveUntilTheJobIsCancelled) {
	init(std::make_unique<CurrentThreadRunnableRunner>());
	int32_t liveBefore = TestObject::live.load();
	Ref<TestObject> siege = TestObject::create(7);
	TestObject* raw = siege.get();
	std::atomic<int32_t> seen{0};
	Ref<JobDetail> job = cron().schedule(CronJob(Pin(raw), [raw, &seen] { seen = raw->objectId; }), "0 0 12 ? * *");
	siege.reset();
	reclaim();
	EXPECT_EQ(TestObject::live.load(), liveBefore + 1);
	pass(hours(12));
	EXPECT_EQ(seen.load(), 7);
	EXPECT_TRUE(cron().cancel(job));
	job.reset();
	reclaim();
	EXPECT_EQ(TestObject::live.load(), liveBefore);
}

TEST_F(CronServiceTest, ExceptionsOfJobsAreLoggedAndTheServiceContinues) {
	LogCapture capture("com.aionemu.gameserver.services.cron.CronService");
	init(std::make_unique<CurrentThreadRunnableRunner>());
	std::atomic<int32_t> runs{0};
	(void)cron().schedule(CronJob(Pin(), [&] {
		++runs;
		throw IllegalStateException("job failure");
	}),
		"0 0 9 ? * *");
	pass(hours(24 + 9));
	EXPECT_EQ(runs.load(), 2);
	EXPECT_EQ(capture.count("Failed to execute cron job"), 2u);
	EXPECT_TRUE(capture.contains("job failure"));
}

TEST_F(CronServiceTest, Errors) {
	EXPECT_THROW((void)CronService::getInstance(), CronServiceException);
	EXPECT_THROW(CronService::initSingleton(nullptr, nullptr, CronService::Driver::EXECUTOR), CronServiceException);
	init(std::make_unique<CurrentThreadRunnableRunner>());
	EXPECT_THROW(init(std::make_unique<CurrentThreadRunnableRunner>()), CronServiceException);

	try {
		(void)cron().schedule(CronJob(Pin(), [] {}), "0 0 9 * * *");
		ADD_FAILURE() << "invalid expression accepted";
	} catch (const CronServiceException& e) {
		ASSERT_TRUE(e.cause());
		EXPECT_THROW(std::rethrow_exception(e.cause()), CronExpressionParseException);
	}
	EXPECT_THROW((void)cron().schedule(CronJob(), "0 0 9 ? * *"), CronServiceException);
	EXPECT_THROW((void)cron().schedule(CronJob(Pin(), [] {}), "0 0 0 1 1 ? 2000"), CronServiceException); // would never fire
	EXPECT_THROW((void)cron().schedule(CronJob(Pin(), [] {}), nullptr, CronExpression("0 0 9 ? * *"), false), CronServiceException);

	// a temporary expression is interned, so the job never refers to the caller's object
	Ref<JobDetail> job = cron().schedule(CronJob(Pin(), [] {}), CronExpression("0 0 9 ? * *"));
	EXPECT_EQ(&job->getCronExpression(), &CronExpressions::getOrCreate("0 0 9 ? * *"));

	cron().shutdown();
	cron().shutdown();
	EXPECT_EQ(cron().getJobCount(), 0u);
	EXPECT_THROW((void)cron().schedule(CronJob(Pin(), [] {}), "0 0 9 ? * *"), CronServiceException);
	EXPECT_THROW((void)cron().cancel(job), CronServiceException);
	EXPECT_EQ(cron().runDueJobs(), 0u);
}

TEST_F(CronServiceTest, SingleExecutorModeRunsCronOnTheSingleExecutorThread) {
	ThreadPoolManager::installBackend(nullptr);
	ThreadPoolManager::Config config;
	config.singleExecutor = true;
	ThreadPoolManager::configure(config);
	executor = nullptr;
	CronService::initSingleton(std::make_unique<ThreadPoolManagerRunnableRunner>(), nullptr);
	EXPECT_EQ(cron().getDriver(), CronService::Driver::EXECUTOR);
	std::atomic<bool> ran{false};
	std::string threadName;
	(void)cron().schedule(CronJob(Pin(), [&] {
		threadName = ThreadContext::current().threadName();
		ran = true;
	}),
		"* * * * * ?");
	EXPECT_TRUE(waitRealTime([&] { return ran.load(); }, milliseconds(3000)));
	CronService::resetForTests();
	ThreadPoolManager::installBackend(nullptr);
	ThreadPoolManager::configure(ThreadPoolManager::Config{});
	ForkJoinPool::commonPool().setSerial(false);
	EXPECT_EQ(threadName, "SingleExecutor");
}

// ------------------------------------------------------------------------------------------ real "Cron" thread (Java CronServiceTest)

class CronThreadTest : public testing::Test {
protected:
	void SetUp() override {
		CronService::resetForTests();
		ThreadPoolManager::installBackend(nullptr);
		ThreadPoolManager::configure(ThreadPoolManager::Config{});
		ThreadPoolBackend::Options options;
		options.instantThreads = 2;
		options.scheduledThreads = 2;
		ThreadPoolManager::installBackend(std::make_unique<ThreadPoolBackend>(options));
		CronService::initSingleton(std::make_unique<CurrentThreadRunnableRunner>(), nullptr);
	}
	void TearDown() override {
		CronService::resetForTests();
		ThreadPoolManager::installBackend(nullptr);
		Reclaimer::getInstance().drain();
	}
	CronService& cron() { return CronService::getInstance(); }
};

TEST_F(CronThreadTest, JobActuallyStarting) {
	EXPECT_EQ(cron().getDriver(), CronService::Driver::THREAD);
	std::atomic<bool> started{false};
	std::string threadName;
	(void)cron().schedule(CronJob(Pin(), [&] {
		if (!started)
			threadName = ThreadContext::current().threadName();
		started = true;
	}),
		"* * * * * ?");
	EXPECT_TRUE(waitRealTime([&] { return started.load(); }, milliseconds(3000)));
	cron().shutdown(); // joins the thread before threadName is read
	EXPECT_EQ(threadName, "Cron");
}

TEST_F(CronThreadTest, FindJobDetails) {
	CronJob test(Pin(), [] {});
	(void)cron().schedule(test, "* * * * * ?");
	EXPECT_EQ(cron().findJobDetails(test).size(), 1u);
}

TEST_F(CronThreadTest, CancelTaskByRunnableReference) {
	CronJob test(Pin(), [] {});
	(void)cron().schedule(test, "* * * * * ?");
	EXPECT_TRUE(cron().cancel(test));
}

TEST_F(CronThreadTest, CancelTaskByJobDetails) {
	Ref<JobDetail> jobDetail = cron().schedule(CronJob(Pin(), [] {}), "* * * * * ?");
	EXPECT_TRUE(cron().cancel(jobDetail));
}

TEST_F(CronThreadTest, GetJobTriggers) {
	Ref<JobDetail> jobDetail = cron().schedule(CronJob(Pin(), [] {}), "* * * * * ?");
	EXPECT_TRUE(cron().getNextFireTime(jobDetail.get()).has_value());
}

TEST_F(CronThreadTest, ShutdownFromAJobOnTheCronThreadDoesNotDeadlock) {
	std::atomic<bool> done{false};
	(void)cron().schedule(CronJob(Pin(), [&] {
		CronService::getInstance().shutdown();
		done = true;
	}),
		"* * * * * ?");
	EXPECT_TRUE(waitRealTime([&] { return done.load(); }, milliseconds(3000)));
	EXPECT_THROW((void)cron().schedule(CronJob(Pin(), [] {}), "* * * * * ?"), CronServiceException);
}

TEST_F(CronThreadTest, ThreadPoolManagerRunnableRunnerOnTheRealPools) {
	CronService::resetForTests();
	CronService::initSingleton(std::make_unique<ThreadPoolManagerRunnableRunner>(), nullptr, CronService::Driver::THREAD);
	std::atomic<int32_t> runs{0};
	std::string kind;
	(void)cron().schedule(CronJob(Pin(), [&] {
		if (runs == 0)
			kind = TaskScope::currentTaskInfo().kind;
		++runs;
	}),
		"* * * * * ?", true);
	EXPECT_TRUE(waitRealTime([&] { return runs.load() >= 2; }, milliseconds(4000)));
	cron().shutdown();
	ThreadPoolManager::installBackend(nullptr); // joins the pools before the captured locals go away
	EXPECT_EQ(kind, TaskKind::LONG_RUNNING);
}

} // namespace
} // namespace aion::gameserver::runtime::servicestest
