// Java usage patterns of ThreadPoolManager and Future (design §1.5, §7, §14.2 g), each on the real pools and on the DeterministicExecutor.

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "SchedTestSupport.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Finally.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/detail/SchedRuntime.h"

namespace aion::gameserver::runtime::schedtest {
namespace {

using namespace std::chrono_literals;

class JavaPatternsTest : public SchedBackendTest {};

INSTANTIATE_TEST_SUITE_P(Backends, JavaPatternsTest, testing::Values(BackendKind::REAL, BackendKind::DETERMINISTIC), backendName);

ThreadPoolManager& pools() {
	return ThreadPoolManager::getInstance();
}

// ---------------------------------------------------------------------------------------------------------------- CM_TELEPORT_ANIMATION_DONE

/** PlayerReviveService.java:251 schedules the TELEPORT task; CM_TELEPORT_ANIMATION_DONE.java:36-41 runs it early: task.run(); task.get(); */
TEST_P(JavaPatternsTest, TeleportScheduledTaskRunNowThenGet) {
	Ref<Npc> player = Npc::create();
	Npc* raw = player.get();
	FutureRef task = pools().schedule(raw, [raw] { raw->runs.fetch_add(1); }, 150);
	ASSERT_FALSE(task->isDone());
	ASSERT_EQ(player->refCount(), 2u);

	task->run(); // run now since it's not started yet
	task->get(); // get to throw exception, if any
	EXPECT_EQ(raw->runs.load(), 1);
	EXPECT_TRUE(task->isDone());
	EXPECT_FALSE(task->isCancelled());
	EXPECT_EQ(player->refCount(), 1u) << "captures are released when the task finished";
	EXPECT_FALSE(task->runNowIfPending());

	pass(300ms); // the pool finds the task completed and drops it
	EXPECT_EQ(raw->runs.load(), 1);
}

/** TeleportService.java:191 stores `new FutureTask<Void>(spawnTask, null)`: a deferred task not bound to any pool. */
TEST_P(JavaPatternsTest, TeleportDeferredTaskIsNotBoundToThePools) {
	TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
	Ref<Npc> player = Npc::create();
	Npc* raw = player.get();
	FutureRef task = Future::deferred(Pin(raw), [raw] { raw->runs.fetch_add(1); });
	pass(100ms);
	EXPECT_EQ(raw->runs.load(), 0) << "no pool runs a deferred task";
	EXPECT_EQ(task->getDelay(), 0);
	EXPECT_TRUE(pools().backend().pendingTasks().empty());
	task->run();
	task->get();
	EXPECT_EQ(raw->runs.load(), 1);

	FutureRef failing = Future::deferred(Pin(raw), [] { throw IllegalStateException("spawn failed"); });
	failing->run();
	try {
		failing->get();
		FAIL() << "ExecutionException expected";
	} catch (const ExecutionException& e) {
		ASSERT_TRUE(e.cause());
		EXPECT_THROW(std::rethrow_exception(e.cause()), IllegalStateException);
	}
}

// ---------------------------------------------------------------------------------------------------------------------------------- FixPath

/** FixPath.java:143-152: a self-cancelling fixed-rate task, awaited with get(5, SECONDS) from an instant pool task. */
TEST_P(JavaPatternsTest, FixPathGetWithTimeoutIsWokenByCancel) {
	Ref<Npc> admin = Npc::create();
	Npc* raw = admin.get();
	enum class Outcome { NONE, CANCELLED, TIMEOUT, OTHER };
	std::atomic<Outcome> outcome{Outcome::NONE};
	std::atomic<int64_t> waitedMillis{-1};
	auto started = clock.now();
	pools().execute(raw, [raw, &outcome, &waitedMillis, this] {
		struct Holder { // Java: final ScheduledFuture<?>[] waitTask = { null };
			std::mutex mutex;
			FutureRef task;
			FutureRef load() {
				std::scoped_lock lock(mutex);
				return task;
			}
		};
		auto holder = std::make_shared<Holder>();
		FutureRef waitTask = pools().scheduleAtFixedRate(raw, [raw, holder] {
			if (raw->runs.fetch_add(1) + 1 == 3)
				(void)holder->load()->cancel(true); // cancelling releases the body, which breaks the holder <-> body cycle
		}, 50, 25);
		{
			std::scoped_lock lock(holder->mutex);
			holder->task = waitTask;
		}
		auto begin = isDeterministic() ? clock.now() : std::chrono::steady_clock::now();
		try {
			holder->load()->get(5, TimeUnit::SECONDS);
			outcome = Outcome::OTHER;
		} catch (const CancellationException&) {
			outcome = Outcome::CANCELLED;
		} catch (const TimeoutException&) {
			outcome = Outcome::TIMEOUT;
		}
		auto end = isDeterministic() ? clock.now() : std::chrono::steady_clock::now();
		waitedMillis = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
	});
	ASSERT_TRUE(waitUntil([&] { return outcome.load() != Outcome::NONE; }, 6000ms));
	EXPECT_EQ(outcome.load(), Outcome::CANCELLED);
	EXPECT_EQ(raw->runs.load(), 3);
	if (isDeterministic()) {
		EXPECT_EQ(waitedMillis.load(), 100) << "helping get() advanced the ManualClock exactly to the cancelling run";
		EXPECT_EQ(clock.now() - started, 100ms);
	} else {
		EXPECT_LT(waitedMillis.load(), 4000) << "get(5 s) returned when the task was cancelled";
	}
	pass(200ms);
	EXPECT_EQ(raw->runs.load(), 3) << "a cancelled periodic task is not re-armed";
	EXPECT_EQ(admin->refCount(), 1u) << "pins of the finished tasks are released";
}

TEST_P(JavaPatternsTest, FixPathGetWithTimeoutTimesOut) {
	Ref<Npc> admin = Npc::create();
	Npc* raw = admin.get();
	std::atomic<bool> timedOut{false};
	std::atomic<bool> finished{false};
	std::atomic<int64_t> waitedMillis{-1};
	pools().execute(raw, [raw, &timedOut, &finished, &waitedMillis, this] {
		FutureRef waitTask = pools().scheduleAtFixedRate(raw, [raw] { raw->runs.fetch_add(1); }, 50, 25);
		auto begin = isDeterministic() ? clock.now() : std::chrono::steady_clock::now();
		try {
			waitTask->get(200, TimeUnit::MILLISECONDS);
		} catch (const TimeoutException&) {
			timedOut = true;
		}
		auto end = isDeterministic() ? clock.now() : std::chrono::steady_clock::now();
		waitedMillis = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
		(void)waitTask->cancel(false); // Java: stop()
		finished = true;
	});
	ASSERT_TRUE(waitUntil([&] { return finished.load(); }, 1000ms));
	EXPECT_TRUE(timedOut.load());
	if (isDeterministic()) {
		EXPECT_EQ(waitedMillis.load(), 200);
		EXPECT_EQ(raw->runs.load(), 7) << "runs at 50, 75, ..., 200 ms while get() helped";
	} else {
		EXPECT_GE(waitedMillis.load(), 199);
	}
}

// ---------------------------------------------------------------------------------------------------------------- CreatureController.addTask

/** CreatureController.java:400-428 (design §14.2 g): adding a task under the same id cancels the previous one. */
TEST_P(JavaPatternsTest, AddTaskReplacesAndCancelsThePreviousTask) {
	struct Controller {
		std::mutex mutex;
		std::map<int32_t, FutureRef> tasks;
		void addTask(int32_t taskId, FutureRef task) {
			std::scoped_lock lock(mutex);
			FutureRef& slot = tasks[taskId];
			if (slot)
				(void)slot->cancel(false);
			slot = std::move(task);
		}
		void cancelAllTasks() {
			std::scoped_lock lock(mutex);
			for (auto& [id, task] : tasks)
				(void)task->cancel(false);
			tasks.clear();
		}
	} controller;
	constexpr int32_t DESPAWN = 1;

	Ref<Npc> player = Npc::create();
	Npc* raw = player.get();
	FutureRef first = pools().schedule(raw, [raw] { raw->runs.fetch_add(1); }, 60'000);
	controller.addTask(DESPAWN, first);
	FutureRef second = pools().schedule(raw, [raw] { raw->runs.fetch_add(10); }, 150);
	EXPECT_EQ(player->refCount(), 3u);
	controller.addTask(DESPAWN, second);

	EXPECT_TRUE(first->isCancelled());
	EXPECT_EQ(player->refCount(), 2u) << "the cancelled task released its pin immediately (deviation 5)";
	EXPECT_THROW(first->get(), CancellationException);
	ASSERT_TRUE(waitUntil([&] { return second->isDone(); }, 1000ms));
	EXPECT_EQ(raw->runs.load(), 10);
	EXPECT_FALSE(first->cancel(false));
	EXPECT_FALSE(second->cancel(false)) << "cancel after completion returns false (Java)";

	FutureRef third = pools().scheduleAtFixedRate(raw, [raw] { raw->runs.fetch_add(100); }, 1000, 1000);
	controller.addTask(DESPAWN, third);
	controller.cancelAllTasks();
	EXPECT_TRUE(third->isCancelled());
	EXPECT_EQ(player->refCount(), 1u);
}

// ------------------------------------------------------------------------------------------------------------------------------ self-cancel

TEST_P(JavaPatternsTest, SelfCancelFromInsideTheBody) {
	Ref<Npc> npc = Npc::create();
	Npc* raw = npc.get();

	// one-shot: the body cancels its own Future (the Java holder pattern); the body finishes, get() reports the cancellation
	auto holder = std::make_shared<FutureRef>();
	std::atomic<bool> cancelResult{false};
	std::atomic<bool> bodyFinished{false};
	std::mutex holderMutex;
	{
		std::scoped_lock lock(holderMutex); // the body must not read the holder before it is assigned
		*holder = pools().schedule(raw, [raw, holder, &cancelResult, &bodyFinished, &holderMutex] {
			FutureRef self;
			{
				std::scoped_lock bodyLock(holderMutex);
				self = *holder;
			}
			cancelResult = self->cancel(false);
			raw->runs.fetch_add(1); // the callable is still alive after cancelling itself
			bodyFinished = true;
		}, 10);
	}
	ASSERT_TRUE(waitUntil([&] { return bodyFinished.load(); }, 1000ms));
	EXPECT_TRUE(cancelResult.load());
	EXPECT_TRUE((*holder)->isCancelled());
	EXPECT_THROW((*holder)->get(), CancellationException);
	ASSERT_TRUE(waitUntil([&] { return npc->refCount() == 1u; }, 1000ms)) << "captures released after the body returned";

	// periodic: cancels itself through its Future& parameter on the third run
	FutureRef periodic = pools().scheduleAtFixedRate(raw, [raw](Future& self) {
		if (raw->runs.fetch_add(1) + 1 == 4)
			(void)self.cancel(false);
	}, 10, 10);
	ASSERT_TRUE(waitUntil([&] { return periodic->isDone(); }, 1000ms));
	EXPECT_TRUE(periodic->isCancelled());
	pass(100ms);
	EXPECT_EQ(raw->runs.load(), 4);
}

// ---------------------------------------------------------------------------------------------------------------------------- exceptions

/** RunnableWrapper(catchAndLog = true): exceptions are logged and a fixed-rate task keeps running. */
TEST_P(JavaPatternsTest, PeriodicTaskSurvivesExceptions) {
	Ref<Npc> npc = Npc::create();
	Npc* raw = npc.get();
	FutureRef task = pools().scheduleAtFixedRate(raw, [raw] {
		if (raw->runs.fetch_add(1) % 2 == 0)
			throw IllegalStateException("periodic failure (expected by the test)");
	}, 0, 10);
	ASSERT_TRUE(waitUntil([&] { return raw->runs.load() >= 5; }, 1000ms));
	EXPECT_FALSE(task->isDone());
	EXPECT_TRUE(task->cancel(false));
	EXPECT_THROW(task->get(), CancellationException);
}

TEST_P(JavaPatternsTest, ExecuteLogsExceptionsAndSubmitStoresThem) {
	std::atomic<bool> ran{false};
	FutureRef submitted = pools().submit([] { throw IllegalArgumentException("submit failure (expected by the test)"); });
	pools().execute(Pin(), [&ran] {
		ran = true;
		throw IllegalArgumentException("execute failure (expected by the test)");
	});
	FutureRef longRunning = pools().submitLongRunning([] { throw IllegalStateException("long running failure (expected by the test)"); });
	ASSERT_TRUE(waitUntil([&] { return ran.load() && submitted->isDone() && longRunning->isDone(); }, 1000ms));
	EXPECT_EQ(submitted->getState(), Future::State::FAILED);
	EXPECT_THROW(submitted->get(), ExecutionException);
	EXPECT_THROW(longRunning->get(), ExecutionException);

	FutureRef scheduled = pools().schedule([] { throw IllegalStateException("schedule failure (expected by the test)"); }, 5);
	ASSERT_TRUE(waitUntil([&] { return scheduled->isDone(); }, 1000ms));
	EXPECT_EQ(scheduled->getState(), Future::State::DONE) << "schedule() logs instead of storing (RunnableWrapper catchAndLog)";
	EXPECT_NO_THROW(scheduled->get());
}

// ---------------------------------------------------------------------------------------------------------------------------- coalescing

TEST_P(JavaPatternsTest, FixedRateCatchUpIsCoalesced) {
	if (isDeterministic()) {
		Ref<Npc> npc = Npc::create();
		Npc* raw = npc.get();
		FutureRef task = pools().scheduleAtFixedRate(raw, [raw] { raw->runs.fetch_add(1); }, 100, 100);
		deterministic->advance(100ms);
		ASSERT_EQ(raw->runs.load(), 1);
		clock.advance(5000ms); // a stalled scheduler: 49 periods (4.9 s) behind
		deterministic->runReady();
		EXPECT_EQ(raw->runs.load(), 2) << "more than 10 periods and at least 2 s behind: run once and realign";
		EXPECT_EQ(task->getDelay(), 100);

		clock.advance(1000ms); // 9 periods behind: Java catch-up
		deterministic->runReady();
		EXPECT_EQ(raw->runs.load(), 12);
		(void)task->cancel(false);

		FutureRef fast = pools().scheduleAtFixedRate(raw, [raw] { raw->runs.fetch_add(1); }, 10, 10);
		deterministic->advance(10ms);
		int32_t before = raw->runs.load();
		clock.advance(500ms); // 50 periods but less than 2 s behind: catch-up
		deterministic->runReady();
		EXPECT_EQ(raw->runs.load() - before, 50);
		(void)fast->cancel(false);
		return;
	}
	ThreadPoolManager::Config config = testConfig();
	config.coalesceAfterPeriods = 2;
	config.coalesceMinimumLag = 50ms;
	ThreadPoolManager::configure(config);
	std::mutex mutex;
	std::vector<std::chrono::steady_clock::time_point> starts;
	std::atomic<int32_t> runs{0};
	FutureRef task = pools().scheduleAtFixedRate(Pin(), [&mutex, &starts, &runs] {
		{
			std::scoped_lock lock(mutex);
			starts.push_back(std::chrono::steady_clock::now());
		}
		if (runs.fetch_add(1) == 0)
			std::this_thread::sleep_for(300ms); // 30 periods behind afterwards
	}, 0, 10);
	ASSERT_TRUE(waitUntil([&] { return runs.load() >= 6; }, 2000ms));
	(void)task->cancel(false);
	std::scoped_lock lock(mutex);
	auto slowEnd = starts[0] + 300ms;
	auto catchUp = std::count_if(starts.begin() + 1, starts.end(), [&](auto start) { return start < slowEnd + 8ms; });
	EXPECT_LE(catchUp, 3) << "without coalescing about 30 catch-up runs would start right after the slow run";
}

// ------------------------------------------------------------------------------------------------------------------------------ capture release

TEST_P(JavaPatternsTest, CancelReleasesCapturesImmediatelyAndGetDelayStaysValid) {
	int32_t liveBefore = Npc::live.load();
	FutureRef task;
	{
		Ref<Npc> npc = Npc::create();
		Npc* raw = npc.get();
		task = pools().schedule(raw, [captured = npc] { captured->runs.fetch_add(1); }, 10'000);
		EXPECT_EQ(npc->refCount(), 3u) << "pin + capture";
		EXPECT_EQ(pools().tasksPinning(*npc).size(), 1u);
		pass(400ms);
		int64_t delay = task->getDelay();
		if (isDeterministic())
			EXPECT_EQ(delay, 9600);
		else
			EXPECT_TRUE(delay > 0 && delay <= 9600) << delay;
		EXPECT_TRUE(task->cancel(false));
		EXPECT_EQ(npc->refCount(), 1u);
		EXPECT_TRUE(pools().tasksPinning(*npc).empty());
		int64_t delayAfterCancel = task->getDelay(); // getDelay stays valid after cancel
		if (isDeterministic())
			EXPECT_EQ(delayAfterCancel, 9600);
		else
			EXPECT_TRUE(delayAfterCancel > 0 && delayAfterCancel <= delay) << delayAfterCancel;
		pass(100ms);
		if (isDeterministic())
			EXPECT_EQ(task->getDelay(), 9500);
		EXPECT_EQ(task->getDelay(TimeUnit::MINUTES), 0);
	}
	reclaimAll();
	EXPECT_EQ(Npc::live.load(), liveBefore) << "the Npc is destroyed although the cancelled Future is still referenced";
	EXPECT_TRUE(task->isCancelled());
}

// -------------------------------------------------------------------------------------------------------------------------- task scopes

TEST_P(JavaPatternsTest, EveryBodyRunsInATaskScopeWithItsCallSite) {
	struct Seen {
		std::atomic<bool> active{false};
		std::atomic<uint32_t> depth{0};
		std::atomic<uint_least32_t> line{0};
		std::atomic<const char*> kind{nullptr};
		std::atomic<bool> done{false};
	};
	auto record = [](Seen& seen) {
		seen.active = TaskScope::active();
		seen.depth = TaskScope::depth();
		seen.line = TaskScope::currentTaskInfo().where.line();
		seen.kind = TaskScope::currentTaskInfo().kind;
		seen.done = true;
	};
	Seen scheduled, instant, longRunning, periodic;
	uint_least32_t scheduledLine = std::source_location::current().line() + 1;
	(void)pools().schedule(Pin(), [&] { record(scheduled); }, 5);
	uint_least32_t instantLine = std::source_location::current().line() + 1;
	pools().execute(Pin(), [&] { record(instant); });
	uint_least32_t longLine = std::source_location::current().line() + 1;
	pools().executeLongRunning(Pin(), [&] { record(longRunning); });
	uint_least32_t periodicLine = std::source_location::current().line() + 1;
	FutureRef periodicTask = pools().scheduleAtFixedRate(Pin(), [&](Future& self) {
		record(periodic);
		(void)self.cancel(false);
	}, 5, 5);
	ASSERT_TRUE(waitUntil([&] { return scheduled.done && instant.done && longRunning.done && periodic.done; }, 1000ms));
	for (auto [seen, line, kind] : {std::tuple{&scheduled, scheduledLine, TaskKind::SCHEDULED}, std::tuple{&instant, instantLine, TaskKind::INSTANT},
				 std::tuple{&longRunning, longLine, TaskKind::LONG_RUNNING}, std::tuple{&periodic, periodicLine, TaskKind::SCHEDULED}}) {
		EXPECT_TRUE(seen->active.load());
		EXPECT_EQ(seen->depth.load(), 1u);
		EXPECT_EQ(seen->line.load(), line);
		EXPECT_STREQ(seen->kind.load(), kind);
	}
}

// ------------------------------------------------------------------------------------------------------------------------- introspection

TEST_P(JavaPatternsTest, TasksPinningAndPendingTaskSummary) {
	Ref<Npc> a = Npc::create();
	Ref<Npc> b = Npc::create();
	Npc* rawA = a.get();
	Npc* rawB = b.get();
	std::vector<FutureRef> tasks;
	for (int i = 0; i < 3; ++i)
		tasks.push_back(pools().schedule(rawA, [rawA] { rawA->runs.fetch_add(1); }, 60'000));
	tasks.push_back(pools().schedule(Pin{rawA, rawB}, [rawA, rawB] { rawA->runs.fetch_add(rawB->runs.load()); }, 60'000));
	tasks.push_back(pools().scheduleAtFixedRate(rawB, [rawB] { rawB->runs.fetch_add(1); }, 60'000, 1000));

	EXPECT_EQ(pools().tasksPinning(*a).size(), 4u);
	std::vector<TaskInfo> pinningB = pools().tasksPinning(*b);
	ASSERT_EQ(pinningB.size(), 2u);
	for (const TaskInfo& info : pinningB)
		EXPECT_STREQ(info.kind, TaskKind::SCHEDULED);

	std::vector<std::string> summary = pools().getPendingTaskSummary();
	ASSERT_GE(summary.size(), 4u);
	EXPECT_EQ(summary[0], "5 pending tasks at 3 call sites");
	EXPECT_TRUE(summary[1].starts_with("3 x scheduled task JavaPatternsTest.cpp:")) << summary[1];
	EXPECT_TRUE(std::any_of(summary.begin(), summary.end(), [](const std::string& line) { return line.ends_with("[periodic]"); }));
	EXPECT_FALSE(pools().getStats().empty());

	for (FutureRef& task : tasks)
		(void)task->cancel(false);
	EXPECT_TRUE(pools().tasksPinning(*a).empty());
	EXPECT_EQ(pools().getPendingTaskSummary()[0], "0 pending tasks at 0 call sites");
}

// ---------------------------------------------------------------------------------------------------------------------------- shutdown

TEST_P(JavaPatternsTest, ShutdownDropsDelayedTasksAndLaterSubmissions) {
	Ref<Npc> npc = Npc::create();
	Npc* raw = npc.get();
	FutureRef delayed = pools().schedule(raw, [raw] { raw->runs.fetch_add(1); }, 60'000);
	FutureRef periodic = pools().scheduleAtFixedRate(raw, [raw] { raw->runs.fetch_add(1); }, 60'000, 1000);
	std::atomic<bool> queuedRan{false};
	pools().execute(Pin(), [&queuedRan] { queuedRan = true; });
	pools().shutdown();
	EXPECT_TRUE(pools().isShutdown());
	EXPECT_TRUE(queuedRan.load()) << "queued instant tasks finish during shutdown";
	EXPECT_TRUE(delayed->isCancelled());
	EXPECT_TRUE(periodic->isCancelled());
	EXPECT_EQ(npc->refCount(), 1u);

	FutureRef late = pools().submit(raw, [raw] { raw->runs.fetch_add(1); });
	FutureRef lateScheduled = pools().schedule(raw, [raw] { raw->runs.fetch_add(1); }, 0);
	EXPECT_TRUE(late->isCancelled()) << "dropped like AionRejectedExecutionHandler does after shutdown";
	EXPECT_TRUE(lateScheduled->isCancelled());
	EXPECT_THROW(late->get(), CancellationException);
	pass(10ms);
	EXPECT_EQ(raw->runs.load(), 0);
	EXPECT_EQ(npc->refCount(), 1u);
}

TEST_P(JavaPatternsTest, FixedRatePeriodMustBePositive) {
	EXPECT_THROW((void)pools().scheduleAtFixedRate([] {}, 0, 0), IllegalArgumentException);
	EXPECT_THROW((void)pools().scheduleAtFixedRate(Pin(), [] {}, 0, -5), IllegalArgumentException);
	FutureRef negativeDelay = pools().schedule([] {}, -100);
	EXPECT_LE(negativeDelay->getDelay(), 0);
	ASSERT_TRUE(waitUntil([&] { return negativeDelay->isDone(); }, 1000ms));
}

// ---------------------------------------------------------------------------------------------------------------------------- long running

TEST_P(JavaPatternsTest, LongRunningPoolRunsConcurrentTasks) {
	std::atomic<int32_t> done{0};
	std::vector<FutureRef> tasks;
	for (int i = 0; i < 6; ++i)
		tasks.push_back(pools().submitLongRunning(Pin(), [&done] {
			std::this_thread::sleep_for(5ms);
			done.fetch_add(1);
		}));
	pools().executeLongRunning([] {});
	ASSERT_TRUE(waitUntil([&] { return done.load() == 6; }, 2000ms));
	for (FutureRef& task : tasks)
		EXPECT_NO_THROW(task->get());
}

// ------------------------------------------------------------------------------------------------------------------------------ rejection

/** AionRejectedExecutionHandler: a full instant queue runs the task on the caller, or on a new thread for callers above normal priority. */
TEST_P(JavaPatternsTest, InstantPoolRejectionPolicy) {
	if (isDeterministic()) {
		GTEST_SKIP() << "the DeterministicExecutor has no bounded queue (rejection is a real-pool policy)";
	}
	install(BackendKind::REAL, testConfig(), 1, 1);
	std::mutex gate;
	std::unique_lock blocker(gate);
	auto unblockOnExit = finally([&blocker]() noexcept {
		if (blocker.owns_lock())
			blocker.unlock();
	});
	std::atomic<bool> blockerStarted{false};
	pools().execute(Pin(), [&] {
		blockerStarted = true;
		std::scoped_lock wait(gate);
	});
	ASSERT_TRUE(waitUntil([&] { return blockerStarted.load(); }, 1000ms));
	std::atomic<bool> fillerRan{false};
	pools().execute(Pin(), [&fillerRan] { fillerRan = true; }); // occupies the only queue slot

	std::thread::id callerRanOn;
	pools().execute(Pin(), [&callerRanOn] { callerRanOn = std::this_thread::get_id(); });
	EXPECT_EQ(callerRanOn, std::this_thread::get_id()) << "normal priority caller: the rejected task ran on the caller, synchronously";

	std::atomic<bool> highPriorityRan{false};
	std::thread::id highPriorityCaller;
	std::atomic<std::thread::id> highPriorityRanOn{};
	std::thread highPriority([&] {
		highPriorityCaller = std::this_thread::get_id();
		sched_detail::setCurrentThreadJavaPriority(7);
		pools().execute(Pin(), [&] {
			highPriorityRanOn = std::this_thread::get_id();
			highPriorityRan = true;
		});
	});
	highPriority.join();
	ASSERT_TRUE(waitUntil([&] { return highPriorityRan.load(); }, 1000ms));
	EXPECT_NE(highPriorityRanOn.load(), highPriorityCaller) << "above normal priority caller: the rejected task ran on a new thread";

	blocker.unlock();
	ASSERT_TRUE(waitUntil([&] { return fillerRan.load(); }, 1000ms));
}

} // namespace
} // namespace aion::gameserver::runtime::schedtest
