// Systematic interleavings (PCT, design §12.4) of the services' concurrent paths: CleanerQueue producers vs a drain, LeakCensus events vs
// Reclaimer scans destroying the objects (the destroy observer must erase entries before the memory is freed; ASan reports otherwise), and
// CronService schedule/cancel vs firing on the EXECUTOR driver. AION_PCT_SCHEDULES raises the number of schedules.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "ServicesTestSupport.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "support/PctSupport.h"

namespace aion::gameserver::runtime::servicestest {
namespace {

using services::cron::CronJob;
using services::cron::CronService;
using services::cron::CurrentThreadRunnableRunner;
using services::cron::JobDetail;

pct::Options pctOptions() {
	pct::Options options;
	options.depth = 3;
	options.timeout = std::chrono::milliseconds(20'000);
	return options;
}

class ServicesPctTest : public DeterministicServicesTest {};

struct CleanerCounters {
	static inline std::atomic<int32_t> processed{0};
	static inline std::atomic<int32_t> orderViolations{0};
	static inline std::array<std::atomic<int32_t>, 2> last{};
	static void action(int32_t id, const char*) {
		int32_t producer = id / 100;
		int32_t sequence = id % 100;
		if (last[static_cast<size_t>(producer)].exchange(sequence) >= sequence)
			++orderViolations;
		++processed;
	}
};

TEST_F(ServicesPctTest, CleanerQueueProducersAgainstADrain) {
	AION_SKIP_WITHOUT_PCT();
	CleanerQueue::setCleanerAction(&CleanerCounters::action);
	pct::ScheduleResult result = pct::explore(
		testsupport::pctSchedules(300), 1,
		[](uint64_t) {
			CleanerCounters::processed = 0;
			CleanerCounters::orderViolations = 0;
			for (auto& last : CleanerCounters::last)
				last = -1;
			std::vector<std::function<void()>> bodies;
			for (int32_t producer = 0; producer < 2; ++producer)
				bodies.emplace_back([producer] {
					for (int32_t i = 0; i < 3; ++i)
						CleanerQueue::push(producer * 100 + i);
				});
			bodies.emplace_back([] {
				for (int i = 0; i < 2; ++i)
					(void)CleanerQueue::drainNow();
			});
			return bodies;
		},
		[] {
			(void)CleanerQueue::drainNow();
			if (CleanerCounters::processed != 6 || CleanerCounters::orderViolations != 0 || !CleanerQueue::isEmpty())
				throw IllegalStateException("cleaner queue lost, duplicated or reordered ids");
		},
		pctOptions());
	AION_EXPECT_SCHEDULE_OK(result);
}

TEST_F(ServicesPctTest, CensusEventsAgainstScansDestroyingTheObjects) {
	AION_SKIP_WITHOUT_PCT();
	LeakCensus& census = LeakCensus::getInstance();
	census.install();
	int32_t liveBefore = TestObject::live.load();
	pct::ScheduleResult result = pct::explore(
		testsupport::pctSchedules(200), 1,
		[&census](uint64_t) {
			auto running = std::make_shared<std::atomic<int32_t>>(2);
			std::vector<std::function<void()>> bodies;
			for (int32_t world = 0; world < 2; ++world)
				bodies.emplace_back([&census, running, world] {
					for (int32_t i = 0; i < 2; ++i) {
						TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
						Ref<TestObject> object = TestObject::create(world * 10 + i);
						census.onRemovedFromWorld(*object, "Npc", object->objectId);
						if (i == 1)
							census.onAddedToWorld(*object);
					} // released inside the scope: retired at scope exit
					running->fetch_sub(1);
				});
			bodies.emplace_back([running] {
				for (int i = 0; i < 16 && running->load() > 0; ++i) {
					Reclaimer::getInstance().reclaimNow();
					pct::yieldPoint("test:reclaimer");
				}
			});
			return bodies;
		},
		[&census, liveBefore] {
			Reclaimer::getInstance().drain();
			if (census.trackedCount() != 0 || TestObject::live.load() != liveBefore)
				throw IllegalStateException("census entries or objects left after all objects were released");
		},
		pctOptions());
	AION_EXPECT_SCHEDULE_OK(result);
}

struct CronCounters {
	static inline std::atomic<int32_t> runs{0};
};

TEST_F(ServicesPctTest, CronScheduleAndCancelAgainstFiring) {
	AION_SKIP_WITHOUT_PCT();
	CronService::initSingleton(std::make_unique<CurrentThreadRunnableRunner>(), std::chrono::locate_zone("UTC"), CronService::Driver::EXECUTOR);
	pct::ScheduleResult result = pct::explore(
		testsupport::pctSchedules(200), 1,
		[](uint64_t) {
			CronCounters::runs = 0;
			std::vector<std::function<void()>> bodies;
			bodies.emplace_back([] {
				Ref<JobDetail> job = CronService::getInstance().schedule(CronJob([] { ++CronCounters::runs; }), "* * * * * ?");
				pct::yieldPoint("test:scheduled");
				(void)CronService::getInstance().cancel(job);
			});
			bodies.emplace_back([] {
				Ref<JobDetail> job = CronService::getInstance().schedule(CronJob([] { ++CronCounters::runs; }), "* * * * * ?");
				pct::yieldPoint("test:scheduled");
				(void)CronService::getInstance().cancel(job);
			});
			bodies.emplace_back([] {
				for (int i = 0; i < 3; ++i) {
					(void)CronService::getInstance().runDueJobs();
					pct::yieldPoint("test:fired");
				}
			});
			return bodies;
		},
		[] {
			if (CronService::getInstance().getJobCount() != 0)
				throw IllegalStateException("cancelled cron jobs are still scheduled");
			if (CronCounters::runs > 2)
				throw IllegalStateException("a job fired more than once within one second");
		},
		pctOptions());
	AION_EXPECT_SCHEDULE_OK(result);
}

} // namespace
} // namespace aion::gameserver::runtime::servicestest
