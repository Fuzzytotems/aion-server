// Systematic interleaving tests (design §12.4 item 1) for the sync and fields kernel operations: Monitor contention and Field<Ref> exchange
// vs. borrow vs. scan. Every atomic step has an AION_YIELD_POINT; the PCT scheduler (runtime/lifetime/Pct.h) picks the interleavings.
// AION_PCT_SCHEDULES raises the schedule count for nightly runs.

#include <gtest/gtest.h>

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "sync/SyncTestSupport.h"
#include "aion/gameserver/runtime/base/YieldPoint.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Pct.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/runtime/sync/Semaphore.h"
#include "aion/gameserver/runtime/sync/StampedLock.h"

using namespace aion::gameserver::runtime;
using namespace aion::gameserver::runtime::testsupport;
using namespace std::chrono_literals;

namespace {

class Npc final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<Npc> create(int32_t id) { return makeRef<Npc>(id); }
	bool intact() const noexcept { return check == id * 13 + 1; }

protected:
	explicit Npc(int32_t id) : id(id), check(id * 13 + 1) {}
	~Npc() override { check = 0; }

private:
	const int32_t id;
	int32_t check;
};

pct::Options pctOptions() {
	pct::Options options;
	options.depth = 3;
	options.maxSteps = 400;
	options.timeout = 10s;
	return options;
}

/** Not constexpr, so tests after the skip are not unreachable code (C4702). */
bool pctBuild() {
	static bool enabled = AION_PCT != 0;
	return enabled;
}

/** Without AION_PCT the kernel's yield points and blocking brackets compile to nothing, so the scheduler cannot control kernel waits. */
#define AION_SKIP_WITHOUT_PCT() \
	if (!pctBuild())         \
	GTEST_SKIP() << "PCT yield points are compiled out (AION_PCT=0)"

std::string describe(const pct::ScheduleResult& result) {
	return "seed " + std::to_string(result.seed) + ", steps " + std::to_string(result.steps) + ", deadlock " + std::to_string(result.deadlock) +
		", failure: " + result.failure;
}

} // namespace

TEST(FieldsPctTest, MonitorProvidesMutualExclusionInEveryExploredSchedule) {
	AION_SKIP_WITHOUT_PCT();
	HangGuard guard(300s);
	struct Shared {
		Monitor monitor;
		int32_t counter = 0; // read and written in two steps with a yield point between them
		StampedLock stamped;
		int32_t stampedCounter = 0;
		Semaphore semaphore{1};
		int32_t semaphoreCounter = 0;
	};
	auto shared = std::make_shared<Shared>();
	constexpr int THREADS = 3;
	constexpr int ROUNDS = 2;
	pct::ScheduleResult result = pct::explore(
		pct::schedulesFromEnvironment(100), 1,
		[&](uint64_t) {
			shared = std::make_shared<Shared>();
			std::vector<std::function<void()>> bodies;
			for (int t = 0; t < THREADS; ++t)
				bodies.push_back([s = shared, t] {
					for (int round = 0; round < ROUNDS; ++round) {
						if ((t + round) % 3 == 0) {
							if (s->monitor.tryLock(1ms)) {
								int32_t value = s->counter;
								pct::yieldPoint("test: between read and write");
								s->counter = value + 1;
								s->monitor.unlock();
							} else {
								SYNCHRONIZED(s->monitor) {
									int32_t value = s->counter;
									pct::yieldPoint("test: between read and write");
									s->counter = value + 1;
								}
							}
						} else {
							SYNCHRONIZED(s->monitor) {
								SYNCHRONIZED(s->monitor) { // reentrant
									int32_t value = s->counter;
									pct::yieldPoint("test: between read and write");
									s->counter = value + 1;
								}
							}
						}
						int64_t stamp = s->stamped.writeLock();
						int32_t value = s->stampedCounter;
						pct::yieldPoint("test: stamped write");
						s->stampedCounter = value + 1;
						s->stamped.unlockWrite(stamp);
						s->semaphore.acquire();
						int32_t permitted = s->semaphoreCounter;
						pct::yieldPoint("test: semaphore");
						s->semaphoreCounter = permitted + 1;
						s->semaphore.release();
					}
				});
			return bodies;
		},
		[&] {
			if (shared->counter != THREADS * ROUNDS || shared->stampedCounter != THREADS * ROUNDS || shared->semaphoreCounter != THREADS * ROUNDS)
				throw IllegalStateException("lost update: " + std::to_string(shared->counter) + "/" + std::to_string(shared->stampedCounter) + "/" +
					std::to_string(shared->semaphoreCounter));
			if (shared->monitor.isLocked())
				throw IllegalStateException("monitor still locked");
		},
		pctOptions());
	EXPECT_TRUE(result.completed) << describe(result);
	EXPECT_FALSE(result.deadlock) << describe(result);
}

TEST(FieldsPctTest, RefFieldExchangeVersusBorrowVersusScan) {
	AION_SKIP_WITHOUT_PCT();
	HangGuard guard(300s);
	struct Shared {
		Field<Ref<Npc>> field;
		Field<std::string> name;
		std::atomic<bool> corrupt{false};
	};
	auto shared = std::make_shared<Shared>();
	pct::ScheduleResult result = pct::explore(
		pct::schedulesFromEnvironment(100), 1000,
		[&](uint64_t) {
			shared = std::make_shared<Shared>();
			shared->field = Npc::create(0);
			shared->name = "initial";
			std::vector<std::function<void()>> bodies;
			bodies.push_back([s = shared] { // writer: replaces the object and the string twice, drops the old ones
				TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
				for (int i = 1; i <= 2; ++i) {
					Ref<Npc> previous = s->field.exchange(Npc::create(i));
					s->name = std::string(static_cast<size_t>(i * 20), 'x');
				}
			});
			bodies.push_back([s = shared] { // reader: borrows, lets others run, then dereferences the borrow
				TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
				Ptr<Npc> borrowed = s->field.get();
				const std::string& text = s->name.get();
				pct::yieldPoint("test: holding borrows");
				if (!borrowed || !borrowed->intact())
					s->corrupt = true;
				for (char c : text)
					if (c != 'x' && text != "initial")
						s->corrupt = true;
			});
			bodies.push_back([] { // scanner
				Reclaimer::getInstance().reclaimNow();
				pct::yieldPoint("test: between scans");
				Reclaimer::getInstance().reclaimNow();
			});
			return bodies;
		},
		[&] {
			if (shared->corrupt.load())
				throw IllegalStateException("a borrowed object or string was freed or torn");
			shared->field = nullptr;
			shared->name = "";
			Reclaimer::getInstance().drain();
		},
		pctOptions());
	EXPECT_TRUE(result.completed) << describe(result);
	EXPECT_FALSE(result.deadlock) << describe(result);
}

TEST(FieldsPctTest, AtomicCompareAndSetHasExactlyOneWinner) {
	AION_SKIP_WITHOUT_PCT();
	HangGuard guard(300s);
	struct Shared {
		AtomicBoolean spawned;
		AtomicInteger winners;
		AtomicLong sum;
	};
	auto shared = std::make_shared<Shared>();
	pct::ScheduleResult result = pct::explore(
		pct::schedulesFromEnvironment(200), 5000,
		[&](uint64_t) {
			shared = std::make_shared<Shared>();
			std::vector<std::function<void()>> bodies;
			for (int t = 0; t < 3; ++t)
				bodies.push_back([s = shared] {
					if (s->spawned.compareAndSet(false, true))
						s->winners.incrementAndGet();
					s->sum.updateAndGet([](int64_t v) { return v + 5; });
				});
			return bodies;
		},
		[&] {
			if (shared->winners.get() != 1 || shared->sum.get() != 15)
				throw IllegalStateException("atomic operation not atomic");
		},
		pctOptions());
	EXPECT_TRUE(result.completed) << describe(result);
}
