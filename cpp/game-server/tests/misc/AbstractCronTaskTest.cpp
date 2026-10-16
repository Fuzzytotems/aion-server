// AbstractCronTask (m5a-plan.md F-03, the base of the housing cron tasks): a deactivated task (null expression), the next run and the last
// planned run before the server start (AbstractCronTask.findLastPlannedRun) of a daily expression in UTC, and run() updating lastRun and nextRun
// before executeTask. The constructor alone schedules nothing (postConstruct does, docs/deviations/P5-14.md), so no pools are needed.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <optional>

#include "aion/commons/database/SqlTypes.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/cron/CronExpression.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/taskmanager/AbstractCronTask.h"

namespace aion::gameserver::taskmanager {
namespace {

using commons::database::Timestamp;
using namespace std::chrono;

class TestCronTask final : public AbstractCronTask {
	AION_MAKE_REF_FRIEND

public:
	static runtime::Ref<TestCronTask> create(const services::cron::CronExpression* expression) { return runtime::makeRef<TestCronTask>(expression); }

	std::atomic<int32_t> executions{0};

	using AbstractCronTask::postConstruct;

protected:
	explicit TestCronTask(const services::cron::CronExpression* expression)
		: AbstractCronTask(expression, "com.aionemu.gameserver.taskmanager.tasks.TestCronTask") {}
	~TestCronTask() override = default;

	void executeTask() override { executions.fetch_add(1); }
};

class AbstractCronTaskTest : public testing::Test {
protected:
	void SetUp() override {
		timeZone = configs::main::GSConfig::TIME_ZONE_ID.load();
		configs::main::GSConfig::TIME_ZONE_ID.store(std::chrono::locate_zone("UTC"));
	}

	void TearDown() override { configs::main::GSConfig::TIME_ZONE_ID.store(timeZone); }

	const std::chrono::time_zone* timeZone = nullptr;
};

TEST_F(AbstractCronTaskTest, ANullExpressionDeactivatesTheTask) {
	runtime::Ref<TestCronTask> task = TestCronTask::create(nullptr);
	EXPECT_FALSE(task->getNextRun());
	EXPECT_FALSE(task->getLastRun());
	EXPECT_FALSE(task->getLastPlannedRun());
	EXPECT_EQ(task->getMillisSinceLastRun(), -1);
	task->postConstruct(); // does nothing for a deactivated task (no pool, no cron service needed)
	EXPECT_EQ(task->executions.load(), 0);
}

TEST_F(AbstractCronTaskTest, NextRunAndLastPlannedRunOfADailyExpression) {
	const services::cron::CronExpression& noon = services::cron::CronExpressions::getOrCreate("0 0 12 * * ?");
	const int64_t before = commons::utils::currentTimeMillis();
	runtime::Ref<TestCronTask> task = TestCronTask::create(&noon);
	const int64_t after = commons::utils::currentTimeMillis();

	std::optional<Timestamp> next = task->getNextRun();
	ASSERT_TRUE(next);
	// the next noon (UTC) strictly after the construction
	sys_days day = floor<days>(sys_time<milliseconds>(milliseconds(before)));
	Timestamp todayNoon = day + hours(12);
	Timestamp expectedNext = todayNoon.time_since_epoch().count() > before ? todayNoon : todayNoon + days(1);
	if (after / 1000 == before / 1000)
		EXPECT_EQ(*next, expectedNext);
	EXPECT_GT(next->time_since_epoch().count(), after - 1000);

	// the planned run before the start is the previous noon: 24 hours earlier, and before now
	std::optional<Timestamp> lastPlanned = task->getLastPlannedRun();
	ASSERT_TRUE(lastPlanned);
	EXPECT_EQ(*next - *lastPlanned, hours(24));
	EXPECT_LT(lastPlanned->time_since_epoch().count(), before + 1000);
	EXPECT_FALSE(task->getLastRun());
	EXPECT_EQ(task->getMillisSinceLastRun(), -1);
	EXPECT_EQ(task->getNextRunAfter(Timestamp(milliseconds(0))), Timestamp(hours(12))); // 1970-01-01T12:00Z
	EXPECT_GT(task->getMillisUntilNextRun(), -1000);
}

TEST_F(AbstractCronTaskTest, RunRecordsTheRunAndThenExecutes) {
	const services::cron::CronExpression& everyMinute = services::cron::CronExpressions::getOrCreate("0 * * * * ?");
	runtime::Ref<TestCronTask> task = TestCronTask::create(&everyMinute);
	task->run();
	EXPECT_EQ(task->executions.load(), 1);
	std::optional<Timestamp> lastRun = task->getLastRun();
	ASSERT_TRUE(lastRun);
	EXPECT_EQ(task->getLastPlannedRun(), lastRun); // the last run wins once there is one
	std::optional<Timestamp> next = task->getNextRun();
	ASSERT_TRUE(next);
	EXPECT_GT(*next, *lastRun);
	EXPECT_LE(*next - *lastRun, seconds(60));
	EXPECT_EQ(floor<seconds>(*next).time_since_epoch().count() % 60, 0);
	EXPECT_GE(task->getMillisSinceLastRun(), 0);
}

} // namespace
} // namespace aion::gameserver::taskmanager
