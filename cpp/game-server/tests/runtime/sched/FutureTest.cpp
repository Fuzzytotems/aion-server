// Future state machine (design §7.1, §7.2, §12.1 "callable destroyed while running"): cancel, run, get, re-arming and coalescing, driven
// directly through the backend interface (runFromExecutor) and by real threads; cancel vs run vs get under the PCT explorer.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "SchedTestSupport.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/ThreadContext.h"
#include "aion/gameserver/runtime/base/YieldPoint.h"
#include "aion/gameserver/runtime/lifetime/Pct.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::runtime::schedtest {
namespace {

using namespace std::chrono_literals;
using SteadyTime = std::chrono::steady_clock::time_point;

constexpr int64_t NO_WARNING = INT64_MAX;

/** counts copies alive, to verify when a callable is destroyed */
struct Tracked {
	explicit Tracked(std::shared_ptr<std::atomic<int32_t>> alive) : alive(std::move(alive)) { this->alive->fetch_add(1); }
	Tracked(const Tracked& other) : alive(other.alive) { alive->fetch_add(1); }
	Tracked(Tracked&& other) noexcept : alive(other.alive) { alive->fetch_add(1); }
	Tracked& operator=(const Tracked&) = delete;
	~Tracked() { alive->fetch_sub(1); }
	std::shared_ptr<std::atomic<int32_t>> alive;
};

FutureRef makeTask(Pin pin, Future::Body body, PoolKind pool, SteadyTime due = {}, std::chrono::nanoseconds period = 0ns, bool logExceptions = true) {
	Future::Schedule schedule;
	schedule.pool = pool;
	schedule.due = due;
	schedule.period = period;
	schedule.logExceptions = logExceptions;
	return Future::create(std::move(pin), std::move(body), AION_TASK_INFO(TaskKind::TEST), schedule);
}

class FutureTest : public testing::Test {
protected:
	void SetUp() override { ThreadPoolManager::installBackend(nullptr); } // no leftover backend: the tests drive runFromExecutor themselves
	void TearDown() override {
		ThreadPoolManager::installBackend(nullptr);
		reclaimAll();
	}
};

TEST_F(FutureTest, CancelFromPendingReleasesCallableAndPinImmediately) {
	auto alive = std::make_shared<std::atomic<int32_t>>(0);
	Ref<Npc> npc = Npc::create();
	FutureRef task = makeTask(Pin(npc), Future::Body([tracked = Tracked(alive)](Future&) {}), PoolKind::SCHEDULED);
	EXPECT_EQ(alive->load(), 1);
	EXPECT_EQ(npc->refCount(), 2u);
	EXPECT_TRUE(task->pins(*npc));
	EXPECT_EQ(task->getState(), Future::State::PENDING);

	EXPECT_TRUE(task->cancel(true));
	EXPECT_EQ(alive->load(), 0) << "callable destroyed by cancel()";
	EXPECT_EQ(npc->refCount(), 1u) << "pin released by cancel()";
	EXPECT_FALSE(task->pins(*npc));
	EXPECT_TRUE(task->isCancelled());
	EXPECT_TRUE(task->isDone());
	EXPECT_FALSE(task->cancel(false));
	EXPECT_EQ(task->runFromExecutor(SteadyTime{}, NO_WARNING, 0, 0ms), Future::RunOutcome::FINISHED);
	EXPECT_FALSE(task->runNowIfPending());
	EXPECT_THROW(task->get(), CancellationException);
	EXPECT_THROW(task->get(1, TimeUnit::SECONDS), CancellationException);
}

TEST_F(FutureTest, CallableIsNeverDestroyedWhileItRuns) {
	auto alive = std::make_shared<std::atomic<int32_t>>(0);
	std::atomic<bool> inBody{false};
	std::atomic<bool> release{false};
	std::atomic<int32_t> aliveSeenAfterCancel{-1};
	FutureRef task = makeTask(Pin(), Future::Body([tracked = Tracked(alive), &inBody, &release, &aliveSeenAfterCancel](Future&) {
		inBody = true;
		while (!release.load())
			std::this_thread::yield();
		aliveSeenAfterCancel = tracked.alive->load();
	}), PoolKind::INSTANT);
	std::thread runner([&] { (void)task->runFromExecutor(std::chrono::steady_clock::now(), NO_WARNING, 0, 0ms); });
	while (!inBody.load())
		std::this_thread::yield();
	EXPECT_EQ(task->getState(), Future::State::RUNNING);
	EXPECT_TRUE(task->cancel(false)) << "RUNNING -> CANCELLED";
	EXPECT_THROW(task->get(), CancellationException) << "get() is woken at once, the body still runs";
	EXPECT_EQ(alive->load(), 1);
	release = true;
	runner.join();
	EXPECT_EQ(aliveSeenAfterCancel.load(), 1);
	EXPECT_EQ(alive->load(), 0) << "released by the running thread after the body";
	EXPECT_EQ(task->getState(), Future::State::CANCELLED);
}

TEST_F(FutureTest, GetWithTimeoutIsWokenByCancelFromAnotherThreadInsideABlockingRegion) {
	FutureRef task = makeTask(Pin(), Future::Body([](Future&) {}), PoolKind::NONE);
	std::atomic<bool> cancelled{false};
	std::atomic<uint64_t> waiterThreadId{0};
	auto begin = std::chrono::steady_clock::now();
	std::thread waiter([&] {
		waiterThreadId = ThreadContext::current().threadId();
		try {
			task->get(5, TimeUnit::SECONDS);
		} catch (const CancellationException&) {
			cancelled = true;
		}
	});
	bool sawBlockingRegion = false;
	for (auto deadline = std::chrono::steady_clock::now() + 4s; !sawBlockingRegion && std::chrono::steady_clock::now() < deadline;) {
		ThreadContext::forEach([&](const ThreadContext& context) {
			if (context.threadId() == waiterThreadId.load()) {
				ThreadContext::BlockingSnapshot blocking = context.blocking();
				sawBlockingRegion = blocking.active && std::string_view(blocking.what != nullptr ? blocking.what : "") == "Future.get";
			}
		});
		std::this_thread::sleep_for(1ms);
	}
	EXPECT_TRUE(sawBlockingRegion) << "the watchdog sees the waiter in BlockingRegion(\"Future.get\")";
	EXPECT_TRUE(task->cancel(false));
	waiter.join();
	EXPECT_TRUE(cancelled.load());
	EXPECT_LT(std::chrono::steady_clock::now() - begin, 4500ms);
}

TEST_F(FutureTest, GetTimesOutAndReportsFailures) {
	FutureRef pending = makeTask(Pin(), Future::Body([](Future&) {}), PoolKind::NONE);
	auto begin = std::chrono::steady_clock::now();
	EXPECT_THROW(pending->get(30, TimeUnit::MILLISECONDS), TimeoutException);
	EXPECT_GE(std::chrono::steady_clock::now() - begin, 29ms);
	EXPECT_THROW(pending->get(0, TimeUnit::SECONDS), TimeoutException);

	FutureRef stored = makeTask(Pin(), Future::Body([](Future&) { throw IllegalStateException("stored"); }), PoolKind::INSTANT, {}, 0ns, false);
	EXPECT_EQ(stored->runFromExecutor(std::chrono::steady_clock::now(), NO_WARNING, 0, 0ms), Future::RunOutcome::FINISHED);
	EXPECT_EQ(stored->getState(), Future::State::FAILED);
	EXPECT_THROW(stored->get(), ExecutionException);

	FutureRef logged = makeTask(Pin(), Future::Body([](Future&) { throw IllegalStateException("logged (expected by the test)"); }), PoolKind::INSTANT);
	EXPECT_EQ(logged->runFromExecutor(std::chrono::steady_clock::now(), NO_WARNING, 0, 0ms), Future::RunOutcome::FINISHED);
	EXPECT_EQ(logged->getState(), Future::State::DONE);
	EXPECT_NO_THROW(logged->get());
}

TEST_F(FutureTest, BodyRunsInATaskScopeNestedInTheCallersScope) {
	std::atomic<uint32_t> depth{0};
	std::atomic<uint_least32_t> line{0};
	FutureRef outer = makeTask(Pin(), Future::Body([&](Future&) {
		depth = TaskScope::depth();
		line = TaskScope::currentTaskInfo().where.line();
	}), PoolKind::NONE);
	outer->run();
	EXPECT_EQ(depth.load(), 1u);
	EXPECT_NE(line.load(), 0u);
	{
		TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
		FutureRef nested = makeTask(Pin(), Future::Body([&](Future&) { depth = TaskScope::depth(); }), PoolKind::NONE);
		nested->run();
		EXPECT_EQ(depth.load(), 2u);
	}
}

TEST_F(FutureTest, PeriodicRearmAndCoalescing) {
	constexpr auto period = 100ms;
	SteadyTime t0 = SteadyTime(10s);
	std::atomic<int32_t> runs{0};
	FutureRef task = makeTask(Pin(), Future::Body([&runs](Future&) { runs.fetch_add(1); }), PoolKind::SCHEDULED, t0, period);
	EXPECT_TRUE(task->isPeriodic());

	EXPECT_EQ(task->runFromExecutor(t0, NO_WARNING, 10, 2000ms), Future::RunOutcome::RESCHEDULE);
	EXPECT_EQ(task->getDueTime(), t0 + period) << "next = previous due + period";
	EXPECT_EQ(task->getState(), Future::State::PENDING);

	// started late, exactly 10 periods behind the next due time: not coalesced (more than 10 are required)
	EXPECT_EQ(task->runFromExecutor(t0 + period + 1100ms, NO_WARNING, 10, 1000ms), Future::RunOutcome::RESCHEDULE);
	EXPECT_EQ(task->getDueTime(), t0 + 2 * period);

	// 10.5 periods behind: more than 10, coalesced (minimum lag 1 s reached)
	SteadyTime slightlyMore = t0 + 3 * period + 1050ms; // next due is t0 + 300 ms
	EXPECT_EQ(task->runFromExecutor(slightlyMore, NO_WARNING, 10, 1000ms), Future::RunOutcome::RESCHEDULE);
	EXPECT_EQ(task->getDueTime(), slightlyMore + period);
	runs.fetch_sub(1); // keep the run count of the scenario below

	// 47 periods (4.7 s) behind: coalesced, realigned to now + period
	SteadyTime late = t0 + 5s;
	EXPECT_EQ(task->runFromExecutor(late, NO_WARNING, 10, 2000ms), Future::RunOutcome::RESCHEDULE);
	EXPECT_EQ(task->getDueTime(), late + period);

	// far behind in periods but below the minimum lag: not coalesced
	SteadyTime due = task->getDueTime();
	EXPECT_EQ(task->runFromExecutor(due + 1500ms, NO_WARNING, 10, 2000ms), Future::RunOutcome::RESCHEDULE);
	EXPECT_EQ(task->getDueTime(), due + period);

	// coalescing disabled
	due = task->getDueTime();
	EXPECT_EQ(task->runFromExecutor(due + 60s, NO_WARNING, 0, 2000ms), Future::RunOutcome::RESCHEDULE);
	EXPECT_EQ(task->getDueTime(), due + period);

	// explicit run() of a periodic task: runs once, stays PENDING with its due time
	due = task->getDueTime();
	EXPECT_TRUE(task->runNowIfPending());
	EXPECT_EQ(task->getState(), Future::State::PENDING);
	EXPECT_EQ(task->getDueTime(), due);
	EXPECT_EQ(runs.load(), 6);

	EXPECT_TRUE(task->cancel(false));
	EXPECT_EQ(task->runFromExecutor(due, NO_WARNING, 10, 2000ms), Future::RunOutcome::FINISHED);
	EXPECT_EQ(runs.load(), 6);
}

TEST_F(FutureTest, NotRunWhileRunningElsewhere) {
	std::atomic<bool> inBody{false};
	std::atomic<bool> release{false};
	FutureRef task = makeTask(Pin(), Future::Body([&](Future&) {
		inBody = true;
		while (!release.load())
			std::this_thread::yield();
	}), PoolKind::SCHEDULED, SteadyTime(1s), 1s);
	std::thread runner([&] { task->run(); });
	while (!inBody.load())
		std::this_thread::yield();
	EXPECT_EQ(task->runFromExecutor(SteadyTime(1s), NO_WARNING, 10, 0ms), Future::RunOutcome::NOT_RUN) << "no overlap";
	release = true;
	runner.join();
	EXPECT_EQ(task->getState(), Future::State::PENDING);
	EXPECT_TRUE(task->cancel(false));
}

// Review finding: the scheduled pool pops a periodic task exactly while an explicit run() executes its body; runFromExecutor returns NOT_RUN and
// the pool drops its only heap entry. The task must be handed back afterwards instead of never running again (nobody cancelled it).
TEST_F(FutureTest, PeriodicTaskPoppedWhileAnExplicitRunExecutesItIsRearmed) {
	ManualClock clock;
	auto owned = std::make_unique<DeterministicExecutor>(clock, 7);
	DeterministicExecutor* executor = owned.get();
	ThreadPoolManager::installBackend(std::move(owned));
	std::atomic<int32_t> runs{0};
	std::atomic<bool> inBody{false};
	std::atomic<bool> release{false};
	FutureRef task = makeTask(Pin(), Future::Body([&](Future&) {
		runs.fetch_add(1);
		inBody = true;
		for (int i = 0; i < 10'000 && !release.load(); ++i) // bounded
			std::this_thread::sleep_for(1ms);
		inBody = false;
	}), PoolKind::SCHEDULED, clock.now() + 1s, 1s);
	executor->schedule(task);
	std::thread runner([&] { task->run(); });
	while (!inBody.load())
		std::this_thread::yield();
	clock.advance(1s);
	EXPECT_EQ(executor->runReady(), 1u) << "the pool popped the due task and found it RUNNING";
	EXPECT_EQ(executor->pendingTaskCount(), 0u);
	release = true;
	runner.join();
	EXPECT_EQ(runs.load(), 1);
	EXPECT_EQ(task->getState(), Future::State::PENDING);
	EXPECT_EQ(executor->pendingTaskCount(), 1u) << "run() handed the periodic task back to the pool";
	executor->runReady(); // overdue (due time unchanged): runs now and re-arms
	EXPECT_EQ(runs.load(), 2);
	executor->advance(3s);
	EXPECT_EQ(runs.load(), 5) << "the task keeps running periodically";
	EXPECT_TRUE(task->cancel(false));
}

TEST_F(FutureTest, CreateValidatesArguments) {
	EXPECT_THROW((void)makeTask(Pin(), Future::Body([](Future&) {}), PoolKind::SCHEDULED, {}, -1ns), IllegalArgumentException);
	EXPECT_THROW((void)makeTask(Pin(), Future::Body(), PoolKind::SCHEDULED), NullPointerException);
}

#if AION_PCT // the scenario needs the yield points and blocking hooks (compiled out with AION_PCT=0, e.g. Release)
/** cancel vs run vs get on three threads: exactly one outcome, the callable destroyed exactly once, captures released, get consistent. */
TEST_F(FutureTest, PctCancelVersusRunVersusGet) {
	struct Shared {
		std::shared_ptr<std::atomic<int32_t>> alive = std::make_shared<std::atomic<int32_t>>(0);
		std::atomic<int32_t> bodyRuns{0};
		std::atomic<bool> cancelResult{false};
		enum class GetOutcome { NONE, NORMAL, CANCELLED } getOutcome = GetOutcome::NONE;
		Future::RunOutcome runOutcome = Future::RunOutcome::NOT_RUN;
		Ref<Npc> owner = Npc::create();
		FutureRef task;
	};
	std::shared_ptr<Shared> shared;
	uint32_t schedules = pct::schedulesFromEnvironment(200);
	pct::ScheduleResult result = pct::explore(
		schedules, 1,
		[&](uint64_t) {
			shared = std::make_shared<Shared>();
			Shared* s = shared.get();
			s->task = makeTask(Pin(s->owner), Future::Body([tracked = Tracked(s->alive), s](Future&) { s->bodyRuns.fetch_add(1); }), PoolKind::INSTANT);
			return std::vector<std::function<void()>>{
				[s] { s->runOutcome = s->task->runFromExecutor(std::chrono::steady_clock::now(), NO_WARNING, 0, 0ms); },
				[s] { s->cancelResult = s->task->cancel(false); },
				[s] {
					try {
						s->task->get(10, TimeUnit::SECONDS);
						s->getOutcome = Shared::GetOutcome::NORMAL;
					} catch (const CancellationException&) {
						s->getOutcome = Shared::GetOutcome::CANCELLED;
					}
				},
			};
		},
		[&] {
			Shared& s = *shared;
			ASSERT_TRUE(s.task->isDone());
			EXPECT_LE(s.bodyRuns.load(), 1);
			EXPECT_EQ(s.alive->load(), 0) << "callable destroyed";
			EXPECT_EQ(s.owner->refCount(), 1u) << "pin released";
			if (s.cancelResult.load()) {
				EXPECT_EQ(s.task->getState(), Future::State::CANCELLED);
				EXPECT_EQ(s.getOutcome, Shared::GetOutcome::CANCELLED);
			} else {
				EXPECT_EQ(s.bodyRuns.load(), 1);
				EXPECT_EQ(s.task->getState(), Future::State::DONE);
				EXPECT_EQ(s.getOutcome, Shared::GetOutcome::NORMAL);
			}
			if (s.bodyRuns.load() == 0)
				EXPECT_EQ(s.runOutcome, Future::RunOutcome::FINISHED) << "found cancelled";
		});
	EXPECT_FALSE(result.deadlock) << "seed " << result.seed;
	EXPECT_TRUE(result.failure.empty()) << "seed " << result.seed << ": " << result.failure;
	shared.reset();
}
#endif

#if AION_PCT
/** explicit run() vs the pool's run of a periodic task: in every interleaving the task is re-armed exactly once (never dropped, never doubled). */
TEST_F(FutureTest, PctExplicitRunVersusPoolRunOfAPeriodicTaskRearmsExactlyOnce) {
	ManualClock clock;
	auto owned = std::make_unique<DeterministicExecutor>(clock, 11);
	DeterministicExecutor* executor = owned.get();
	ThreadPoolManager::installBackend(std::move(owned));
	struct Shared {
		std::atomic<int32_t> bodyRuns{0};
		Future::RunOutcome poolOutcome = Future::RunOutcome::NOT_RUN;
		FutureRef task;
		size_t pendingBefore = 0;
	};
	std::shared_ptr<Shared> shared;
	pct::ScheduleResult result = pct::explore(
		pct::schedulesFromEnvironment(300), 1,
		[&](uint64_t) {
			shared = std::make_shared<Shared>();
			Shared* s = shared.get();
			s->pendingBefore = executor->pendingTaskCount();
			s->task = makeTask(Pin(), Future::Body([s](Future&) { s->bodyRuns.fetch_add(1); }), PoolKind::SCHEDULED, clock.now(), 1s);
			return std::vector<std::function<void()>>{
				[s] { (void)s->task->runNowIfPending(); },
				[s] { s->poolOutcome = s->task->runFromExecutor(std::chrono::steady_clock::now(), NO_WARNING, 0, 0ms); },
			};
		},
		[&] {
			Shared& s = *shared;
			size_t handedBack = executor->pendingTaskCount() - s.pendingBefore;
			size_t rearms = handedBack + (s.poolOutcome == Future::RunOutcome::RESCHEDULE ? 1 : 0);
			EXPECT_EQ(rearms, 1u) << "pool outcome " << static_cast<int>(s.poolOutcome) << ", handed back " << handedBack << ", body runs " << s.bodyRuns.load();
			EXPECT_GE(s.bodyRuns.load(), 1);
			EXPECT_EQ(s.task->getState(), Future::State::PENDING);
			(void)s.task->cancel(false); // leaves a cancelled shell, not counted as pending
		});
	EXPECT_FALSE(result.deadlock) << "seed " << result.seed;
	EXPECT_TRUE(result.failure.empty()) << "seed " << result.seed << ": " << result.failure;
	shared.reset();
}
#endif

/** Stress: many real threads cancel, run and wait on the same tasks (bounded). */
TEST_F(FutureTest, ConcurrentCancelRunGetStress) {
	for (int round = 0; round < 200; ++round) {
		auto alive = std::make_shared<std::atomic<int32_t>>(0);
		std::atomic<int32_t> runs{0};
		Ref<Npc> owner = Npc::create();
		FutureRef task = makeTask(Pin(owner), Future::Body([tracked = Tracked(alive), &runs](Future&) { runs.fetch_add(1); }), PoolKind::INSTANT);
		std::atomic<int32_t> cancels{0};
		std::vector<std::jthread> threads;
		for (int i = 0; i < 2; ++i) {
			threads.emplace_back([&] { (void)task->runFromExecutor(std::chrono::steady_clock::now(), NO_WARNING, 0, 0ms); });
			threads.emplace_back([&] { cancels.fetch_add(task->cancel(false) ? 1 : 0); });
			threads.emplace_back([&] {
				try {
					task->get(5, TimeUnit::SECONDS);
				} catch (const CancellationException&) {
				}
			});
		}
		threads.clear();
		ASSERT_LE(cancels.load(), 1);
		ASSERT_LE(runs.load(), 1);
		ASSERT_EQ(alive->load(), 0);
		ASSERT_EQ(owner->refCount(), 1u);
		ASSERT_TRUE(cancels.load() == 1 || runs.load() == 1);
	}
}

} // namespace
} // namespace aion::gameserver::runtime::schedtest
