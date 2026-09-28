// World chunk (P4-10): AbstractPeriodicTaskManager and AbstractFIFOPeriodicTaskManager on a deterministic scheduler. Expectations are
// hand-derived from AbstractPeriodicTaskManager.java and AbstractFIFOPeriodicTaskManager.java: the "initialized" line with the subclass's
// simple name, the first run 500-550 ms after construction, duplicate tasks processed once per run (LinkedHashSet), a throwing task logged with
// the subclass name and String.valueOf(task), and the warning after counterLimit runs with a growing number of tasks.

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <spdlog/sinks/ostream_sink.h>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/taskmanager/AbstractFIFOPeriodicTaskManager.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::taskmanager {
namespace {

/** A queued task: counts its calls, throws if asked to */
class CountingTask final : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
public:
	const std::string name;
	const bool throws;
	runtime::Field<int32_t> calls{0};

	CountingTask(std::string nameValue, bool throwsValue) : name(std::move(nameValue)), throws(throwsValue) {}

	std::string toString() const { return "task " + name; }

protected:
	~CountingTask() override = default;
};

/** Java: a subclass of AbstractFIFOPeriodicTaskManager<CountingTask> with a 500 ms period */
class CountingTaskManager final : public AbstractFIFOPeriodicTaskManager<CountingTask> {
	AION_MAKE_REF_FRIEND
public:
	explicit CountingTaskManager(int32_t period) : AbstractFIFOPeriodicTaskManager(period, "CountingTaskManager") {}

	int32_t runs = 0;

protected:
	void callTask(CountingTask& task) override {
		task.calls += 1;
		if (task.throws)
			throw runtime::IllegalStateException("broken task");
	}

	std::string getCalledMethodName() override { return "count()"; }

	~CountingTaskManager() override = default;
};

class TaskManagerTest : public ::testing::Test {
protected:
	void SetUp() override {
		auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(logStream);
		sink->set_pattern("%l|%v");
		commons::logging::LoggerFactory::configure(LOGGER, {.sinks = {sink}, .additive = false});
		utils::ThreadPoolManager::installBackend(nullptr);
		auto backend = std::make_unique<runtime::DeterministicExecutor>(clock, 7);
		executor = backend.get();
		utils::ThreadPoolManager::installBackend(std::move(backend));
	}

	void TearDown() override {
		utils::ThreadPoolManager::installBackend(nullptr);
		runtime::Reclaimer::getInstance().drain();
		commons::logging::LoggerFactory::removeConfig(LOGGER);
	}

	static constexpr const char* LOGGER = "com.aionemu.gameserver.taskmanager.AbstractPeriodicTaskManager";
	std::ostringstream logStream;
	runtime::ManualClock clock{0};
	runtime::DeterministicExecutor* executor = nullptr;
};

TEST_F(TaskManagerTest, FifoRunProcessesEachTaskOnceAndLogsFailuresWithTheSubclassName) {
	runtime::Ref<CountingTaskManager> manager;
	runtime::Ref<CountingTask> broken;
	runtime::Ref<CountingTask> plain;
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		manager = runtime::makeRef<CountingTaskManager>(500);
		broken = runtime::makeRef<CountingTask>("broken", true);
		plain = runtime::makeRef<CountingTask>("plain", false);
	}
	EXPECT_NE(logStream.str().find("info|CountingTaskManager initialized."), std::string::npos) << logStream.str();
	manager->add(*broken);
	manager->add(*plain);
	manager->add(*plain); // queued twice: the LinkedHashSet of the run keeps it once
	executor->advance(std::chrono::milliseconds(499));
	EXPECT_EQ(plain->calls.get(), 0) << "the first run comes 500-550 ms after construction";
	executor->advance(std::chrono::milliseconds(51));
	EXPECT_EQ(plain->calls.get(), 1);
	EXPECT_EQ(broken->calls.get(), 1) << "the exception of one task does not stop the run";
	EXPECT_NE(logStream.str().find("error|Exception in CountingTaskManager processing task broken"), std::string::npos) << logStream.str();
	executor->advance(std::chrono::milliseconds(500));
	EXPECT_EQ(plain->calls.get(), 1) << "processed tasks leave the queue";
	manager.reset();
	broken.reset();
	plain.reset();
}

TEST_F(TaskManagerTest, FifoWarnsWhenTheTasksGrowForTheWarningPeriod) {
	runtime::Ref<CountingTaskManager> manager;
	std::vector<runtime::Ref<CountingTask>> tasks;
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		manager = runtime::makeRef<CountingTaskManager>(500); // counterLimit = max(5, 10 * 1000 / 500) = 20
		for (int32_t i = 0; i < 25; i++)
			tasks.push_back(runtime::makeRef<CountingTask>(std::to_string(i), false));
	}
	executor->advance(std::chrono::milliseconds(550)); // the first run (no tasks yet: counter = 0)
	for (int32_t run = 1; run <= 20; run++) {
		for (int32_t i = 0; i < run; i++)
			manager->add(*tasks[i]);
		EXPECT_EQ(logStream.str().find("are added faster"), std::string::npos) << "run " << run;
		executor->advance(std::chrono::milliseconds(500)); // run `run` processes `run` tasks, one more than before: counter = run
	}
	EXPECT_NE(logStream.str().find("warning|Tasks for CountingTaskManager are added faster than they can be executed (currently 20 tasks)."),
		std::string::npos)
		<< logStream.str();
	EXPECT_EQ(tasks[0]->calls.get(), 20);
	EXPECT_EQ(tasks[19]->calls.get(), 1);
	manager.reset();
	tasks.clear();
}

} // namespace
} // namespace aion::gameserver::taskmanager
