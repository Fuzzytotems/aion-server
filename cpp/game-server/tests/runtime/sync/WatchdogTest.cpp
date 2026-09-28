// Watchdog core (design §4.3, C7, D5): stall and slow-task detection per task, two-thread deadlock detection through the wait graph (bounded:
// the deadlock is broken by the test afterwards), probes, dumps and minidumps.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <spdlog/sinks/base_sink.h>

#include "SyncTestSupport.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/ThreadContext.h"
#include "aion/gameserver/runtime/sync/LockOrderValidator.h"
#include "aion/gameserver/runtime/sync/MinidumpWriter.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/runtime/sync/Watchdog.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "aion/commons/utils/WindowsMacroGuard.h"
#endif

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

/** Captures the messages of the watchdog's logger while alive (they still reach the root sink). */
class WatchdogLogCapture {
public:
	WatchdogLogCapture() : sink(std::make_shared<Sink>()) {
		sink->set_pattern("%v");
		aion::commons::logging::LoggerFactory::configure(LOGGER, {.level = spdlog::level::debug, .sinks = {sink}, .additive = true});
	}
	~WatchdogLogCapture() { aion::commons::logging::LoggerFactory::removeConfig(LOGGER); }
	WatchdogLogCapture(const WatchdogLogCapture&) = delete;
	WatchdogLogCapture& operator=(const WatchdogLogCapture&) = delete;

	/** index of the first message containing the text, or -1 */
	int64_t find(std::string_view text) const {
		std::scoped_lock lock(sink->linesMutex);
		for (size_t i = 0; i < sink->lines.size(); ++i)
			if (sink->lines[i].find(text) != std::string::npos)
				return static_cast<int64_t>(i);
		return -1;
	}

private:
	static constexpr std::string_view LOGGER = "com.aionemu.gameserver.runtime.Watchdog";

	struct Sink : spdlog::sinks::base_sink<std::mutex> {
		mutable std::mutex linesMutex;
		std::vector<std::string> lines;

	protected:
		void sink_it_(const spdlog::details::log_msg& msg) override {
			spdlog::memory_buf_t formatted;
			formatter_->format(msg, formatted);
			std::scoped_lock lock(linesMutex);
			lines.emplace_back(formatted.data(), formatted.size());
		}
		void flush_() override {}
	};

	std::shared_ptr<Sink> sink;
};

/** UTF-8 text of a path (configuration strings and DumpReport::minidumpPath are UTF-8) */
std::string utf8(const std::filesystem::path& path) {
	std::u8string text = path.u8string();
	return std::string(text.begin(), text.end());
}

std::filesystem::path fromUtf8(std::string_view text) {
	return std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t*>(text.data()), text.size()));
}

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
	config.minidumpMinInterval = 0ms;
	config.maxMinidumps = 0;
	std::filesystem::path directory = std::filesystem::temp_directory_path() / ("aion-watchdog-test-" + std::to_string(ThreadContext::current().threadId()));
	config.minidumpDirectory = directory.string();
	watchdog.configure(config);
	Watchdog::DumpReport report = watchdog.dump(Watchdog::Reason::STALL, "minidump test");
	watchdog.configure(testConfig());
	ASSERT_FALSE(report.minidumpPath.empty()) << report.text;
	EXPECT_TRUE(std::filesystem::exists(report.minidumpPath));
	EXPECT_GT(std::filesystem::file_size(report.minidumpPath), 0u);
	EXPECT_NE(report.text.find(report.minidumpPath), std::string::npos);
	EXPECT_NE(report.text.find("helper process"), std::string::npos) << "this test executable registered its helper mode: " << report.text;
	std::error_code error;
	std::filesystem::remove_all(directory, error);
}

// Open item of M4: a parallel load reported every stalled ForkJoin element in the same check, one dump and one in-process MiniDumpWriteDump per
// thread, and the server hung inside the 45th. Now a burst gives one STALL dump and at most one minidump (by the helper process) per check, the
// rate limit covers later checks, and the process keeps running.
TEST(WatchdogTest, BurstOfSimultaneousStallsGivesOneDumpAndOneMinidumpPerCheck) {
	HangGuard guard(180s);
	Watchdog& watchdog = Watchdog::getInstance();
	Watchdog::Config config = testConfig();
	config.stall = 500ms;
	config.writeMinidump = true;
	config.minidumpMinInterval = 0ms;
	config.maxMinidumps = 0;
	std::filesystem::path directory =
		std::filesystem::temp_directory_path() / ("aion-watchdog-burst-" + std::to_string(ThreadContext::current().threadId()) + "-" +
													 std::to_string(aion::commons::utils::nanoTime()));
	config.minidumpDirectory = directory.string();
	watchdog.configure(config);
	RecordingListener listener;
	auto minidumpFiles = [&] {
		size_t files = 0;
		std::error_code error;
		for (const auto& entry : std::filesystem::directory_iterator(directory, error))
			files += entry.path().extension() == ".dmp" ? 1 : 0;
		return files;
	};

	constexpr size_t THREADS = 40;
	std::vector<std::unique_ptr<FakeTaskThread>> tasks;
	for (size_t i = 0; i < THREADS; ++i)
		tasks.push_back(std::make_unique<FakeTaskThread>());
	auto stallAll = [&](uint64_t firstScope) {
		int64_t start = aion::commons::utils::nanoTime() - int64_t{2'000'000'000};
		for (size_t i = 0; i < THREADS; ++i)
			tasks[i]->setTask(firstScope + i, start - static_cast<int64_t>(i));
	};

	// check 1: 40 stalls at once -> one STALL dump naming all of them, one minidump written by the helper process
	stallAll(9000);
	watchdog.checkNow();
	std::vector<Watchdog::DumpReport> stalls = listener.of(Watchdog::Reason::STALL);
	ASSERT_EQ(stalls.size(), 1u);
	EXPECT_EQ(stalls[0].threadIds.size(), THREADS);
	EXPECT_NE(stalls[0].summary.find(std::to_string(THREADS) + " stalled tasks:"), std::string::npos) << stalls[0].summary;
	ASSERT_FALSE(stalls[0].minidumpPath.empty()) << stalls[0].text;
	EXPECT_NE(stalls[0].text.find("helper process"), std::string::npos) << stalls[0].text;
	EXPECT_EQ(minidumpFiles(), 1u);

	// a second dump in the same check (a probe) names the check's minidump instead of writing another one
	uint64_t probe = watchdog.addProbe("burst", [](Watchdog& w, const std::vector<Watchdog::ThreadSnapshot>&) {
		w.dump(Watchdog::Reason::DEADLOCK, "probe dump in the same check");
		w.dump(Watchdog::Reason::STALL, "another dump in the same check");
	});
	stallAll(9100); // new task runs: stalled again
	watchdog.checkNow();
	watchdog.removeProbe(probe);
	stalls = listener.of(Watchdog::Reason::STALL);
	ASSERT_EQ(stalls.size(), 3u) << "check 2: one burst dump and the probe's second dump";
	EXPECT_FALSE(stalls[1].minidumpPath.empty());
	EXPECT_TRUE(stalls[2].minidumpPath.empty());
	EXPECT_NE(stalls[2].text.find("Minidump: one per watchdog check, see " + stalls[1].minidumpPath), std::string::npos) << stalls[2].text;
	std::vector<Watchdog::DumpReport> probeDeadlocks = listener.of(Watchdog::Reason::DEADLOCK);
	ASSERT_EQ(probeDeadlocks.size(), 1u);
	EXPECT_TRUE(probeDeadlocks[0].minidumpPath.empty());
	EXPECT_EQ(minidumpFiles(), 2u);

	// check 3 within the minimum interval: the text dump only
	config.minidumpMinInterval = 600'000ms;
	watchdog.configure(config);
	stallAll(9200);
	watchdog.checkNow();
	stalls = listener.of(Watchdog::Reason::STALL);
	ASSERT_EQ(stalls.size(), 4u);
	EXPECT_TRUE(stalls[3].minidumpPath.empty());
	EXPECT_NE(stalls[3].text.find("Minidump: skipped (rate limit"), std::string::npos) << stalls[3].text;
	EXPECT_EQ(minidumpFiles(), 2u);

	// the process keeps running: the stalled threads finish and the watchdog keeps checking
	for (auto& task : tasks)
		task->setTask(0, 0);
	tasks.clear();
	watchdog.checkNow();
	EXPECT_EQ(listener.of(Watchdog::Reason::STALL).size(), 4u);
	watchdog.configure(testConfig());
	std::error_code error;
	std::filesystem::remove_all(directory, error);
}

TEST(WatchdogTest, MinidumpLimitPerRun) {
	Watchdog& watchdog = Watchdog::getInstance();
	Watchdog::Config config = testConfig();
	config.writeMinidump = true;
	config.minidumpMinInterval = 0ms;
	std::filesystem::path directory = std::filesystem::temp_directory_path() / ("aion-watchdog-limit-" + std::to_string(aion::commons::utils::nanoTime()));
	config.minidumpDirectory = directory.string();
	// a limit at or below the number already attempted in this run: no further minidump
	config.maxMinidumps = 1;
	Watchdog::DumpReport first = [&] {
		Watchdog::Config unlimited = config;
		unlimited.maxMinidumps = 0;
		watchdog.configure(unlimited);
		return watchdog.dump(Watchdog::Reason::STALL, "limit test, unlimited");
	}();
	EXPECT_FALSE(first.minidumpPath.empty()) << first.text;
	watchdog.configure(config);
	Watchdog::DumpReport limited = watchdog.dump(Watchdog::Reason::STALL, "limit test, limited");
	watchdog.configure(testConfig());
	EXPECT_TRUE(limited.minidumpPath.empty());
	EXPECT_NE(limited.text.find("limit of 1 minidumps per run reached"), std::string::npos) << limited.text;
	std::error_code error;
	std::filesystem::remove_all(directory, error);
}

// Review finding (wave 3b-2): the STALL rate limit also suppressed the minidump of a confirmed deadlock, which is reported only once and may be
// followed by quick_exit. DEADLOCK ignores minidumpMinInterval and the STALL budget, keeps one per check, and has its own budget.
// (CTest runs every test in its own process: no DEADLOCK minidump was attempted before this test.)
TEST(WatchdogTest, DeadlockMinidumpIsNotSuppressedByTheStallRateLimit) {
	HangGuard guard(180s);
	Watchdog& watchdog = Watchdog::getInstance();
	Watchdog::Config config = testConfig();
	config.writeMinidump = true;
	config.minidumpMinInterval = 0ms;
	config.maxMinidumps = 0;
	std::filesystem::path directory = std::filesystem::temp_directory_path() / ("aion-watchdog-deadlock-" + std::to_string(aion::commons::utils::nanoTime()));
	config.minidumpDirectory = directory.string();
	watchdog.configure(config);
	Watchdog::DumpReport stall = watchdog.dump(Watchdog::Reason::STALL, "an unrelated stall just before");
	ASSERT_FALSE(stall.minidumpPath.empty()) << stall.text;

	// within the interval, and the STALL budget of 1 is used up
	config.minidumpMinInterval = 600'000ms;
	config.maxMinidumps = 1;
	watchdog.configure(config);
	RecordingListener listener;
	auto checkWithDump = [&](Watchdog::Reason reason, std::string summary) {
		uint64_t probe = watchdog.addProbe("finding", [reason, summary](Watchdog& w, const std::vector<Watchdog::ThreadSnapshot>&) { w.dump(reason, summary); });
		watchdog.checkNow();
		watchdog.removeProbe(probe);
		std::vector<Watchdog::DumpReport> reports = listener.of(reason);
		return reports.empty() ? Watchdog::DumpReport{} : reports.back();
	};
	Watchdog::DumpReport deadlock = checkWithDump(Watchdog::Reason::DEADLOCK, "a confirmed deadlock in a later check");
	ASSERT_EQ(listener.of(Watchdog::Reason::DEADLOCK).size(), 1u);
	ASSERT_FALSE(deadlock.minidumpPath.empty()) << deadlock.text;
	EXPECT_TRUE(std::filesystem::exists(deadlock.minidumpPath));

	// a STALL in the next check is still rate limited
	Watchdog::DumpReport laterStall = checkWithDump(Watchdog::Reason::STALL, "a stall in the next check");
	EXPECT_TRUE(laterStall.minidumpPath.empty());
	EXPECT_NE(laterStall.text.find("Minidump: skipped"), std::string::npos) << laterStall.text;

	// the DEADLOCK budget of maxMinidumps = 1 is used up now
	Watchdog::DumpReport secondDeadlock = checkWithDump(Watchdog::Reason::DEADLOCK, "another deadlock");
	ASSERT_EQ(listener.of(Watchdog::Reason::DEADLOCK).size(), 2u);
	EXPECT_TRUE(secondDeadlock.minidumpPath.empty());
	EXPECT_NE(secondDeadlock.text.find("limit of 1 DEADLOCK minidumps per run reached"), std::string::npos) << secondDeadlock.text;
	watchdog.configure(testConfig());
	std::error_code error;
	std::filesystem::remove_all(directory, error);
}

// Review finding (wave 3b-2): the text dump was logged only after the minidump writer returned (up to minidumpTimeout + 5 s), so a server killed
// meanwhile lost it. The text is logged first, the minidump result in a second message.
TEST(WatchdogTest, TextDumpIsLoggedBeforeTheMinidumpIsWritten) {
	HangGuard guard(120s);
	Watchdog& watchdog = Watchdog::getInstance();
	Watchdog::Config config = testConfig();
	config.writeMinidump = true;
	config.minidumpMinInterval = 0ms;
	config.maxMinidumps = 0;
	config.minidumpTimeout = 4000ms;
	config.minidumpHelperExecutable = utf8(MinidumpWriter::currentExecutable());
	std::filesystem::path directory = std::filesystem::temp_directory_path() / ("aion-watchdog-order-" + std::to_string(aion::commons::utils::nanoTime()));
	config.minidumpDirectory = directory.string();
	watchdog.configure(config);
	WatchdogLogCapture log;
	SetEnvironmentVariableW(L"AION_MINIDUMP_TEST_HELPER_HANG", L"1"); // MinidumpWriterTest.cpp: the helper hangs until it is terminated
	auto dumped = std::async(std::launch::async, [&watchdog] { return watchdog.dump(Watchdog::Reason::STALL, "order test"); });
	bool loggedWhileWriting = false;
	for (auto deadline = std::chrono::steady_clock::now() + 3500ms; std::chrono::steady_clock::now() < deadline;) {
		if (log.find("Watchdog STALL: order test") >= 0) {
			loggedWhileWriting = dumped.wait_for(0ms) != std::future_status::ready;
			break;
		}
		std::this_thread::sleep_for(5ms);
	}
	Watchdog::DumpReport report = dumped.get();
	SetEnvironmentVariableW(L"AION_MINIDUMP_TEST_HELPER_HANG", nullptr);
	watchdog.configure(testConfig());
	EXPECT_TRUE(loggedWhileWriting) << "the text dump is logged while the helper still runs";
	EXPECT_TRUE(report.minidumpPath.empty());
	int64_t text = log.find("Watchdog STALL: order test");
	int64_t result = log.find("Watchdog STALL: Minidump: could not be written (helper process");
	EXPECT_GE(text, 0);
	EXPECT_GT(result, text) << "the minidump result follows in its own message";
	EXPECT_NE(report.text.find("terminated"), std::string::npos) << "listeners see the text and the minidump line: " << report.text;
	std::error_code error;
	std::filesystem::remove_all(directory, error);
}

// Review finding (wave 3b-2): paths were converted with path::string(), which throws for characters outside the ANSI code page (and a UTF-8
// directory was read in the ANSI code page), so the dump failed before its text was logged. The directory is UTF-8 and the path is reported as
// UTF-8.
TEST(WatchdogTest, MinidumpDirectoryOutsideTheAnsiCodePage) {
	HangGuard guard(120s);
	Watchdog& watchdog = Watchdog::getInstance();
	Watchdog::Config config = testConfig();
	config.writeMinidump = true;
	config.minidumpMinInterval = 0ms;
	config.maxMinidumps = 0;
	std::filesystem::path directory = std::filesystem::temp_directory_path() /
		(u8"aion-watchdog-\u65e5\u672c-\u0436-" + fromUtf8(std::to_string(aion::commons::utils::nanoTime())).u8string());
	config.minidumpDirectory = utf8(directory);
	watchdog.configure(config);
	RecordingListener listener;
	Watchdog::DumpReport report = watchdog.dump(Watchdog::Reason::STALL, "non-ANSI directory");
	watchdog.configure(testConfig());
	ASSERT_FALSE(report.minidumpPath.empty()) << report.text;
	EXPECT_TRUE(std::filesystem::exists(fromUtf8(report.minidumpPath))) << report.minidumpPath;
	EXPECT_EQ(fromUtf8(report.minidumpPath).parent_path(), directory);
	EXPECT_NE(report.text.find(report.minidumpPath), std::string::npos);
	EXPECT_EQ(listener.of(Watchdog::Reason::STALL).size(), 1u);
	std::error_code error;
	std::filesystem::remove_all(directory, error);
}
#endif
