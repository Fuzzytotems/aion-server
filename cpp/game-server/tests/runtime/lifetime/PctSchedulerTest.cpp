// Tests of the PCT scheduler itself: serialization, determinism, script mode, blocking brackets, timeouts, failures and bug finding.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "aion/gameserver/runtime/base/YieldPoint.h"
#include "aion/gameserver/runtime/lifetime/Pct.h"
#include "support/PctSupport.h"

using namespace aion::gameserver::runtime;

namespace {

using Bodies = std::vector<std::function<void()>>;

/** Shared counter incremented non-atomically with a yield point between the load and the store (a depth-2 atomicity bug). */
struct RacyCounter {
	int value = 0;
	std::atomic<int> inside{0};
	std::atomic<int> maxInside{0};

	void increment() {
		enter();
		int loaded = value;
		leave();
		pct::yieldPoint("racy:between");
		enter();
		value = loaded + 1;
		leave();
	}
	void enter() {
		int now = inside.fetch_add(1) + 1;
		int seen = maxInside.load();
		while (now > seen && !maxInside.compare_exchange_weak(seen, now)) {
		}
	}
	void leave() { inside.fetch_sub(1); }
};

pct::ScenarioFactory racyScenario(std::shared_ptr<RacyCounter>& counter, int threads, int incrementsPerThread) {
	return [&counter, threads, incrementsPerThread](uint64_t) {
		counter = std::make_shared<RacyCounter>();
		Bodies bodies;
		for (int t = 0; t < threads; ++t) {
			bodies.push_back([c = counter, incrementsPerThread] {
				for (int i = 0; i < incrementsPerThread; ++i) {
					pct::yieldPoint("racy:before");
					c->increment();
				}
			});
		}
		return bodies;
	};
}

} // namespace

TEST(PctSchedulerTest, RunsExactlyOneThreadAtATime) {
	// yield points are a plain function (independent of AION_PCT), so the scheduler works in every build
	auto counter = std::make_shared<RacyCounter>();
	Bodies bodies;
	for (int t = 0; t < 4; ++t) {
		bodies.push_back([counter] {
			for (int i = 0; i < 50; ++i) {
				counter->enter();
				std::this_thread::yield(); // real concurrency would show up here
				counter->leave();
				pct::yieldPoint("exclusive");
			}
		});
	}
	pct::Options options;
	options.seed = 7;
	pct::ScheduleResult result = pct::PctScheduler(options).run(std::move(bodies));
	AION_EXPECT_SCHEDULE_OK(result);
	EXPECT_EQ(counter->maxInside.load(), 1);
	EXPECT_EQ(result.steps, 200u);
	EXPECT_EQ(pct::controlledThreadIndex(), -1);
}

TEST(PctSchedulerTest, SameSeedGivesSameScheduleAndSeedsDiffer) {
	auto traceFor = [](uint64_t seed) {
		std::shared_ptr<RacyCounter> counter;
		pct::Options options;
		options.seed = seed;
		options.keepTrace = true;
		options.maxSteps = 40;
		return pct::PctScheduler(options).run(racyScenario(counter, 3, 5)(seed)).trace;
	};
	EXPECT_EQ(traceFor(11), traceFor(11));
	std::set<std::vector<std::string>> distinct;
	for (uint64_t seed = 1; seed <= 20; ++seed)
		distinct.insert(traceFor(seed));
	EXPECT_GT(distinct.size(), 3u);
}

TEST(PctSchedulerTest, FindsAtomicityViolationAndReplaysIt) {
	std::shared_ptr<RacyCounter> counter;
	auto check = [&counter] {
		if (counter->value != 4)
			throw std::runtime_error("lost update: " + std::to_string(counter->value));
	};
	pct::Options options;
	options.maxSteps = 8; // 2 threads x 2 increments x 2 yield points
	pct::ScheduleResult found = pct::explore(500, 1, racyScenario(counter, 2, 2), check, options);
	ASSERT_FALSE(found.completed) << "PCT did not find the depth-2 lost update in 500 schedules";
	EXPECT_NE(found.failure.find("lost update"), std::string::npos) << found.failure;

	// replay with the reported seed
	options.seed = found.seed;
	pct::ScheduleResult replay = pct::PctScheduler(options).run(racyScenario(counter, 2, 2)(found.seed));
	AION_EXPECT_SCHEDULE_OK(replay);
	EXPECT_NE(counter->value, 4) << "the failing seed must reproduce the lost update";
}

TEST(PctSchedulerTest, ScriptDirectsTheInterleaving) {
	auto order = std::make_shared<std::vector<std::string>>();
	auto record = [order](const char* what) { order->push_back(what); };
	Bodies bodies = {
		[record] {
			record("a1");
			pct::yieldPoint("a:mid");
			record("a2");
		},
		[record] {
			record("b1");
			pct::yieldPoint("b:mid");
			record("b2");
		},
	};
	pct::Options options;
	options.script = {{0, "a:mid"}, {1, "b:mid"}, {0, ""}, {1, ""}};
	pct::ScheduleResult result = pct::PctScheduler(options).run(std::move(bodies));
	AION_EXPECT_SCHEDULE_OK(result);
	EXPECT_EQ(*order, (std::vector<std::string>{"a1", "b1", "a2", "b2"}));
}

TEST(PctSchedulerTest, ScriptOccurrencesCountArrivals) {
	auto order = std::make_shared<std::vector<int>>();
	Bodies bodies = {
		[order] {
			for (int i = 0; i < 3; ++i) {
				pct::yieldPoint("loop");
				order->push_back(i);
			}
		},
		[order] { order->push_back(100); },
	};
	pct::Options options;
	options.script = {{0, "loop", 2}, {1, ""}, {0, ""}};
	pct::ScheduleResult result = pct::PctScheduler(options).run(std::move(bodies));
	AION_EXPECT_SCHEDULE_OK(result);
	EXPECT_EQ(*order, (std::vector<int>{0, 100, 1, 2}));
}

TEST(PctSchedulerTest, UnreachableScriptSiteFailsLoudly) {
	Bodies bodies = {[] { pct::yieldPoint("exists"); }, [] {}};
	pct::Options options;
	options.script = {{0, "renamed-site"}, {1, ""}};
	pct::ScheduleResult result = pct::PctScheduler(options).run(std::move(bodies));
	EXPECT_FALSE(result.completed);
	EXPECT_NE(result.failure.find("renamed-site"), std::string::npos) << result.failure;
}

TEST(PctSchedulerTest, CapturesTheFirstException) {
	Bodies bodies = {[] { throw std::runtime_error("boom"); }, [] { pct::yieldPoint("other"); }};
	pct::ScheduleResult result = pct::PctScheduler(pct::Options{}).run(std::move(bodies));
	EXPECT_FALSE(result.completed);
	EXPECT_FALSE(result.deadlock);
	EXPECT_NE(result.failure.find("boom"), std::string::npos);
}

TEST(PctSchedulerTest, BlockingBracketsHandTheTurnToOtherThreads) {
	// thread 0 holds the mutex across a yield point; thread 1 blocks on it inside a bracket, so thread 0 can continue and release it
	auto mutex = std::make_shared<std::mutex>();
	auto order = std::make_shared<std::vector<std::string>>();
	auto orderMutex = std::make_shared<std::mutex>();
	auto record = [order, orderMutex](const char* what) {
		std::scoped_lock lock(*orderMutex);
		order->push_back(what);
	};
	Bodies bodies = {
		[mutex, record] {
			mutex->lock();
			record("0:locked");
			pct::yieldPoint("holding");
			record("0:unlock");
			mutex->unlock();
		},
		[mutex, record] {
			if (!mutex->try_lock()) {
				pct::blockingBegin("test:mutex");
				mutex->lock();
				pct::blockingEnd();
			}
			record("1:locked");
			mutex->unlock();
		},
	};
	pct::Options options;
	options.script = {{0, "holding"}, {1, ""}, {0, ""}, {1, ""}};
	options.timeout = std::chrono::milliseconds(10000);
	pct::ScheduleResult result = pct::PctScheduler(options).run(std::move(bodies));
	AION_EXPECT_SCHEDULE_OK(result);
	EXPECT_EQ(*order, (std::vector<std::string>{"0:locked", "0:unlock", "1:locked"}));
}

TEST(PctSchedulerTest, DeadlockEndsTheScheduleAfterTheTimeout) {
	auto first = std::make_shared<std::timed_mutex>();
	auto second = std::make_shared<std::timed_mutex>();
	auto lockBoth = [](std::timed_mutex& a, std::timed_mutex& b, const char* site) {
		std::unique_lock lockA(a);
		pct::yieldPoint(site);
		pct::blockingBegin("test:second");
		bool acquired = b.try_lock_for(std::chrono::milliseconds(1500)); // bounded, so the abandoned threads finish
		pct::blockingEnd();
		if (acquired)
			b.unlock();
	};
	Bodies bodies = {[=] { lockBoth(*first, *second, "a"); }, [=] { lockBoth(*second, *first, "b"); }};
	pct::Options options;
	options.script = {{0, "a"}, {1, "b"}};
	options.timeout = std::chrono::milliseconds(300);
	auto started = std::chrono::steady_clock::now();
	pct::ScheduleResult result = pct::PctScheduler(options).run(std::move(bodies));
	EXPECT_TRUE(result.deadlock);
	EXPECT_FALSE(result.completed);
	EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::seconds(8));
}

TEST(PctSchedulerTest, OnlyOneScheduleAtATime) {
	auto nestedFailed = std::make_shared<std::atomic<bool>>(false);
	Bodies bodies = {[nestedFailed] {
		try {
			pct::PctScheduler(pct::Options{}).run({[] {}});
		} catch (const std::logic_error&) {
			nestedFailed->store(true);
		}
	}};
	pct::ScheduleResult result = pct::PctScheduler(pct::Options{}).run(std::move(bodies));
	AION_EXPECT_SCHEDULE_OK(result);
	EXPECT_TRUE(nestedFailed->load());
}

TEST(PctSchedulerTest, UncontrolledThreadsPassThroughYieldPoints) {
	std::atomic<bool> release{false};
	std::atomic<int> outsideYields{0};
	std::thread outsider([&] {
		while (!release.load()) {
			pct::yieldPoint("outside");
			outsideYields.fetch_add(1);
			std::this_thread::yield();
		}
	});
	Bodies bodies = {[&] {
		for (int i = 0; i < 1000 && outsideYields.load() < 10; ++i)
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}};
	pct::ScheduleResult result = pct::PctScheduler(pct::Options{}).run(std::move(bodies));
	release.store(true);
	outsider.join();
	AION_EXPECT_SCHEDULE_OK(result);
	EXPECT_GE(outsideYields.load(), 10);
}

TEST(PctSchedulerTest, SchedulesFromEnvironmentFallsBackToDefault) {
	if (std::getenv("AION_PCT_SCHEDULES") == nullptr)
		EXPECT_EQ(pct::schedulesFromEnvironment(123), 123u);
	else
		EXPECT_EQ(pct::schedulesFromEnvironment(123), std::strtoul(std::getenv("AION_PCT_SCHEDULES"), nullptr, 10));
}
