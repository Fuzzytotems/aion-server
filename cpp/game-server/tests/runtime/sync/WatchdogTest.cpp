// Watchdog core (design §4.3, C7, D5): stall and slow-task detection per task, two-thread deadlock detection through the wait graph (bounded:
// the deadlock is broken by the test afterwards), probes, dumps and minidumps.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "SyncTestSupport.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/ThreadContext.h"
#include "aion/gameserver/runtime/sync/LockOrderValidator.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/runtime/sync/Watchdog.h"

using namespace aion::gameserver::runtime;
using namespace aion::gameserver::runtime::testsupport;
using namespace std::chrono_literals;

namespace {

/** Collects dump reports while alive. */
class RecordingListener {
public:
	RecordingListener()
		: id(Watchdog::getInstance().addDumpListener([this](const Watchdog::DumpReport& report) {
			  std::scoped_lock lock(mutex);
			  reports.push_back(report);
		  })) {}
	~RecordingListener() { Watchdog::getInstance().removeDumpListener(id); }
	RecordingListener(const RecordingListener&) = delete;
	RecordingListener& operator=(const RecordingListener&) = delete;

	std::vector<Watchdog::DumpReport> of(Watchdog::Reason reason, uint64_t threadId = 0) {
		std::scoped_lock lock(mutex);
		std::vector<Watchdog::DumpReport> matching;
		for (const Watchdog::DumpReport& report : reports)
			if (report.reason == reason &&
				(threadId == 0 || std::find(report.threadIds.begin(), report.threadIds.end(), threadId) != report.threadIds.end()))
				matching.push_back(report);
		return matching;
	}

private:
	std::mutex mutex;
	std::vector<Watchdog::DumpReport> reports;
	uint64_t id;
};

Watchdog::Config testConfig() {
	Watchdog::Config config;
	config.period = 10ms;
	config.slowTaskWarning = 1000ms;
	config.stall = 10000ms;
	config.writeMinidump = false;
	return config;
}

/** A thread that publishes a fake task record (as TaskScope does) with a chosen start time, until told to finish. */
class FakeTaskThread {
public:
	FakeTaskThread() {
		thread = std::thread([this] {
			id = ThreadContext::current().threadId();
			std::unique_lock lock(mutex);
			for (;;) {
				condition.wait(lock, [this] { return pending || finish; });
				if (finish)
					break;
				ThreadContext& context = ThreadContext::current();
				if (startNanos != 0)
					context.setTask(TaskInfo{std::source_location::current(), kind}, scopeId, startNanos);
				else
					context.clearTask();
				pending = false;
				condition.notify_all();
			}
			ThreadContext::current().clearTask();
		});
		setTask(0, 0);
	}
	~FakeTaskThread() {
		{
			std::scoped_lock lock(mutex);
			finish = true;
		}
		condition.notify_all();
		thread.join();
	}

	/** startNanos == 0 clears the task. Blocks until the thread published it. */
	void setTask(uint64_t newScopeId, int64_t newStartNanos, const char* newKind = TaskKind::TEST) {
		std::unique_lock lock(mutex);
		scopeId = newScopeId;
		startNanos = newStartNanos;
		kind = newKind;
		pending = true;
		condition.notify_all();
		condition.wait(lock, [this] { return !pending; });
	}

	uint64_t threadId() const { return id.load(); }

private:
	std::thread thread;
	std::mutex mutex;
	std::condition_variable condition;
	bool pending = false;
	bool finish = false;
	uint64_t scopeId = 0;
	int64_t startNanos = 0;
	const char* kind = TaskKind::TEST;
	std::atomic<uint64_t> id{0};
};

} // namespace

TEST(WatchdogTest, StalledTaskIsDumpedOncePerTaskRun) {
	HangGuard guard(30s);
	Watchdog& watchdog = Watchdog::getInstance();
	Watchdog::Config config = testConfig();
	config.stall = 500ms;
	watchdog.configure(config);
	RecordingListener listener;
	FakeTaskThread task;
	int64_t now = aion::commons::utils::nanoTime();

	task.setTask(1001, now - int64_t{2'000'000'000});
	watchdog.checkNow();
	std::vector<Watchdog::DumpReport> stalls = listener.of(Watchdog::Reason::STALL, task.threadId());
	ASSERT_EQ(stalls.size(), 1u);
	EXPECT_NE(stalls[0].summary.find("WatchdogTest.cpp"), std::string::npos) << stalls[0].summary;
	EXPECT_NE(stalls[0].text.find("* thread"), std::string::npos) << "the stalled thread is marked in the dump";
	EXPECT_TRUE(stalls[0].minidumpPath.empty());
	EXPECT_TRUE(listener.of(Watchdog::Reason::SLOW_TASK, task.threadId()).empty());

	watchdog.checkNow();
	EXPECT_EQ(listener.of(Watchdog::Reason::STALL, task.threadId()).size(), 1u) << "same task run: reported once";

	task.setTask(1002, now - int64_t{3'000'000'000}); // a new task run on the same thread (new start time)
	watchdog.checkNow();
	EXPECT_EQ(listener.of(Watchdog::Reason::STALL, task.threadId()).size(), 2u);

	task.setTask(0, 0);
	watchdog.checkNow();
	EXPECT_EQ(listener.of(Watchdog::Reason::STALL, task.threadId()).size(), 2u);
}

TEST(WatchdogTest, SlowTaskIsWarnedOnceWithoutAFullDump) {
	Watchdog& watchdog = Watchdog::getInstance();
	watchdog.configure(testConfig()); // slow 1 s, stall 10 s
	RecordingListener listener;
	FakeTaskThread task;
	task.setTask(2001, aion::commons::utils::nanoTime() - int64_t{2'000'000'000});
	watchdog.checkNow();
	watchdog.checkNow();
	std::vector<Watchdog::DumpReport> slow = listener.of(Watchdog::Reason::SLOW_TASK, task.threadId());
	ASSERT_EQ(slow.size(), 1u);
	EXPECT_EQ(slow[0].threads.size(), 1u);
	EXPECT_TRUE(listener.of(Watchdog::Reason::STALL, task.threadId()).empty());

	FakeTaskThread fresh;
	fresh.setTask(2002, aion::commons::utils::nanoTime());
	watchdog.checkNow();
	EXPECT_TRUE(listener.of(Watchdog::Reason::SLOW_TASK, fresh.threadId()).empty());
}

// Review finding: quiescentPoint() gives the task a new scope id but keeps its start time. A long QuiescentScope loop must not produce a STALL
// dump (and minidump) on every check; each quiescent point restarts the stall clock, and the slow-task warning fires once per run.
TEST(WatchdogTest, QuiescentPointsCountAsProgressAndDoNotRefireReports) {
	HangGuard guard(30s);
	Watchdog& watchdog = Watchdog::getInstance();
	Watchdog::Config config = testConfig();
	config.slowTaskWarning = 1000ms;
	config.stall = 500ms;
	watchdog.configure(config);
	RecordingListener listener;
	FakeTaskThread task;
	int64_t start = aion::commons::utils::nanoTime() - int64_t{2'000'000'000};

	task.setTask(3001, start);
	watchdog.checkNow();
	ASSERT_EQ(listener.of(Watchdog::Reason::STALL, task.threadId()).size(), 1u);
	for (uint64_t scope = 3002; scope < 3006; ++scope) { // quiescent points of the same run
		task.setTask(scope, start);
		watchdog.checkNow();
	}
	EXPECT_EQ(listener.of(Watchdog::Reason::STALL, task.threadId()).size(), 1u) << "quiescent points are progress, not new stalls";
	EXPECT_TRUE(listener.of(Watchdog::Reason::SLOW_TASK, task.threadId()).empty()) << "the stall dump superseded the slow warning";

	std::this_thread::sleep_for(700ms); // no progress for longer than `stall` after the last quiescent point
	watchdog.checkNow();
	std::vector<Watchdog::DumpReport> stalls = listener.of(Watchdog::Reason::STALL, task.threadId());
	ASSERT_EQ(stalls.size(), 2u) << "a step without progress for `stall` is a stall";
	EXPECT_NE(stalls[1].summary.find("since its last quiescent point"), std::string::npos) << stalls[1].summary;
	watchdog.checkNow();
	EXPECT_EQ(listener.of(Watchdog::Reason::STALL, task.threadId()).size(), 2u);

	FakeTaskThread slow;
	config.stall = 10000ms;
	watchdog.configure(config);
	int64_t slowStart = aion::commons::utils::nanoTime() - int64_t{2'000'000'000};
	slow.setTask(4001, slowStart);
	watchdog.checkNow();
	slow.setTask(4002, slowStart);
	watchdog.checkNow();
	EXPECT_EQ(listener.of(Watchdog::Reason::SLOW_TASK, slow.threadId()).size(), 1u) << "one slow-task warning per run";
}

// Review finding: expected long tasks (long-running pool, startup phases, the main thread) are exempt, as in Java.
TEST(WatchdogTest, LongRunningAndStartupKindsAreExempt) {
	Watchdog& watchdog = Watchdog::getInstance();
	Watchdog::Config config = testConfig();
	config.slowTaskWarning = 1000ms;
	config.stall = 1500ms;
	watchdog.configure(config);
	RecordingListener listener;
	FakeTaskThread longRunning;
	FakeTaskThread startup;
	FakeTaskThread shutdown;
	int64_t start = aion::commons::utils::nanoTime() - int64_t{5'000'000'000};
	longRunning.setTask(5001, start, TaskKind::LONG_RUNNING);
	startup.setTask(5002, start, TaskKind::STARTUP);
	shutdown.setTask(5003, start, TaskKind::SHUTDOWN);
	watchdog.checkNow();
	EXPECT_TRUE(listener.of(Watchdog::Reason::STALL, longRunning.threadId()).empty());
	EXPECT_TRUE(listener.of(Watchdog::Reason::SLOW_TASK, longRunning.threadId()).empty());
	EXPECT_TRUE(listener.of(Watchdog::Reason::STALL, startup.threadId()).empty());
	EXPECT_TRUE(listener.of(Watchdog::Reason::SLOW_TASK, startup.threadId()).empty());
	EXPECT_EQ(listener.of(Watchdog::Reason::STALL, shutdown.threadId()).size(), 1u) << "shutdown saves are only exempt from slow warnings";

	config.stallExemptKinds.clear();
	config.slowTaskExemptKinds.clear();
	watchdog.configure(config);
	FakeTaskThread configured;
	configured.setTask(5004, start, TaskKind::LONG_RUNNING);
	watchdog.checkNow();
	EXPECT_EQ(listener.of(Watchdog::Reason::STALL, configured.threadId()).size(), 1u) << "exemptions are configurable";
	watchdog.configure(testConfig());
}

TEST(WatchdogTest, TwoThreadDeadlockIsDetectedThroughTheWaitGraph) {
	HangGuard guard(60s);
	Watchdog& watchdog = Watchdog::getInstance();
	watchdog.configure(testConfig());
	RecordingListener listener;
	Monitor a{AION_LOCK_CLASS(WatchdogTest::a)};
	Monitor b{AION_LOCK_CLASS(WatchdogTest::b)};
	std::promise<void> aLocked;
	std::promise<void> bLocked;
	std::atomic<bool> breakDeadlock{false};
	std::atomic<uint64_t> firstId{0};
	std::atomic<uint64_t> secondId{0};

	// first: holds a, blocks in lock(b) until second gives b up
	std::thread first([&] {
		LockdepSuppression suppression("test provokes an inversion on purpose");
		firstId = ThreadContext::current().threadId();
		SYNCHRONIZED(a) {
			aLocked.set_value();
			bLocked.get_future().wait();
			SYNCHRONIZED(b) {
			}
		}
	});
	// second: holds b, waits for a with short timed attempts (each is a recorded wait) until the test breaks the deadlock
	std::thread second([&] {
		LockdepSuppression suppression("test provokes an inversion on purpose");
		secondId = ThreadContext::current().threadId();
		aLocked.get_future().wait();
		SYNCHRONIZED(b) {
			bLocked.set_value();
			while (!breakDeadlock.load()) {
				if (a.tryLock(20ms)) {
					ADD_FAILURE() << "a must stay held by the first thread";
					a.unlock();
					break;
				}
			}
		}
	});

	std::vector<Watchdog::DumpReport> deadlocks;
	auto deadline = std::chrono::steady_clock::now() + 20s;
	while (deadlocks.empty() && std::chrono::steady_clock::now() < deadline) {
		watchdog.checkNow();
		deadlocks = listener.of(Watchdog::Reason::DEADLOCK, firstId.load());
		if (deadlocks.empty())
			std::this_thread::sleep_for(5ms);
	}
	breakDeadlock = true;
	first.join();
	second.join();

	ASSERT_EQ(deadlocks.size(), 1u);
	const Watchdog::DumpReport& report = deadlocks[0];
	EXPECT_NE(std::find(report.threadIds.begin(), report.threadIds.end(), secondId.load()), report.threadIds.end());
	EXPECT_EQ(report.threadIds.size(), 2u);
	EXPECT_NE(report.summary.find("WatchdogTest::a"), std::string::npos) << report.summary;
	EXPECT_NE(report.summary.find("WatchdogTest::b"), std::string::npos) << report.summary;
	EXPECT_NE(report.text.find("holds [WatchdogTest::a]"), std::string::npos) << report.text;

	// once resolved, the cycle disappears and nothing new is reported
	watchdog.checkNow();
	watchdog.checkNow();
	EXPECT_EQ(listener.of(Watchdog::Reason::DEADLOCK, firstId.load()).size(), 1u);
}

TEST(WatchdogTest, ContendedButProgressingLocksAreNotADeadlock) {
	HangGuard guard(60s);
	Watchdog& watchdog = Watchdog::getInstance();
	watchdog.configure(testConfig());
	RecordingListener listener;
	Monitor a{AION_LOCK_CLASS(WatchdogTest::contendedA)};
	Monitor b{AION_LOCK_CLASS(WatchdogTest::contendedB)};
	std::atomic<bool> stop{false};
	std::vector<std::thread> threads;
	for (int t = 0; t < 4; ++t)
		threads.emplace_back([&] {
			while (!stop.load()) {
				SYNCHRONIZED(a) {
					SYNCHRONIZED(b) {
					}
				}
			}
		});
	for (int i = 0; i < 50; ++i) {
		watchdog.checkNow();
		std::this_thread::sleep_for(1ms);
	}
	stop = true;
	for (std::thread& thread : threads)
		thread.join();
	EXPECT_TRUE(listener.of(Watchdog::Reason::DEADLOCK).empty());
}

TEST(WatchdogTest, ProbesSnapshotsAndManualDumps) {
	Watchdog& watchdog = Watchdog::getInstance();
	watchdog.configure(testConfig());
	RecordingListener listener;
	Monitor held{AION_LOCK_CLASS(WatchdogTest::held)};
	std::atomic<int> probeCalls{0};
	uint64_t self = ThreadContext::current().threadId();
	uint64_t probe = watchdog.addProbe("test", [&](Watchdog& w, const std::vector<Watchdog::ThreadSnapshot>& threads) {
		probeCalls.fetch_add(1);
		for (const Watchdog::ThreadSnapshot& thread : threads)
			if (thread.threadId == self && !thread.heldLockClasses.empty())
				w.dump(Watchdog::Reason::BACKLOG, "probe saw a held lock", {self});
	});
	SYNCHRONIZED(held) {
		watchdog.checkNow();
	}
	watchdog.removeProbe(probe);
	watchdog.checkNow();
	EXPECT_EQ(probeCalls.load(), 1);
	std::vector<Watchdog::DumpReport> backlog = listener.of(Watchdog::Reason::BACKLOG, self);
	ASSERT_EQ(backlog.size(), 1u);
	EXPECT_NE(backlog[0].text.find("holds [WatchdogTest::held]"), std::string::npos) << backlog[0].text;

	Watchdog::DumpReport manual = watchdog.dump(Watchdog::Reason::MANUAL, "requested by //debug");
	EXPECT_NE(manual.text.find("Watchdog MANUAL: requested by //debug"), std::string::npos);
	EXPECT_FALSE(manual.threads.empty());
	EXPECT_STREQ(Watchdog::reasonName(Watchdog::Reason::RECLAIM_LAG), "RECLAIM_LAG");
}

TEST(WatchdogTest, WatchdogThreadDetectsStallsAndStops) {
	HangGuard guard(30s);
	Watchdog& watchdog = Watchdog::getInstance();
	Watchdog::Config config = testConfig();
	config.stall = 200ms;
	config.evenUnderDebugger = true;
	RecordingListener listener;
	FakeTaskThread task;
	watchdog.start(config);
	EXPECT_TRUE(watchdog.isRunning());
	task.setTask(3001, aion::commons::utils::nanoTime() - int64_t{1'000'000'000});
	EXPECT_TRUE(waitUntil([&] { return !listener.of(Watchdog::Reason::STALL, task.threadId()).empty(); }, 10s));
	watchdog.stop();
	EXPECT_FALSE(watchdog.isRunning());
	watchdog.stop(); // idempotent
	task.setTask(0, 0);
}

#if defined(_WIN32)
TEST(WatchdogTest, DeadlockAndStallDumpsWriteAMinidump) {
	Watchdog& watchdog = Watchdog::getInstance();
	Watchdog::Config config = testConfig();
	config.writeMinidump = true;
	std::filesystem::path directory = std::filesystem::temp_directory_path() / ("aion-watchdog-test-" + std::to_string(ThreadContext::current().threadId()));
	config.minidumpDirectory = directory.string();
	watchdog.configure(config);
	Watchdog::DumpReport report = watchdog.dump(Watchdog::Reason::STALL, "minidump test");
	watchdog.configure(testConfig());
	ASSERT_FALSE(report.minidumpPath.empty()) << report.text;
	EXPECT_TRUE(std::filesystem::exists(report.minidumpPath));
	EXPECT_GT(std::filesystem::file_size(report.minidumpPath), 0u);
	EXPECT_NE(report.text.find(report.minidumpPath), std::string::npos);
	std::error_code error;
	std::filesystem::remove_all(directory, error);
}
#endif
