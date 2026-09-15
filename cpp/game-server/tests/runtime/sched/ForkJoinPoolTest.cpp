// ForkJoinPool::commonPool().parallelForEach (design §2.6, §7.6): JOIN vs PER_ELEMENT isolation, exceptions, serial modes.

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <numeric>
#include <optional>
#include <set>
#include <source_location>
#include <string>
#include <thread>
#include <vector>

#include "SchedTestSupport.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/ForkJoinPool.h"

namespace aion::gameserver::runtime::schedtest {
namespace {

using namespace std::chrono_literals;

template <class R>
concept PerElementCallable = requires(ForkJoinPool& pool, R& range) { pool.parallelForEach(range, [](auto&&) {}, Isolation::PER_ELEMENT); };
static_assert(PerElementCallable<std::vector<Ref<Npc>>>);
static_assert(PerElementCallable<std::vector<int32_t>>);
static_assert(!PerElementCallable<std::vector<Ptr<Npc>>>, "PER_ELEMENT rejects borrowed elements at compile time");
static_assert(!PerElementCallable<std::vector<Npc*>>);

struct Observation {
	std::thread::id thread;
	uint64_t scopeId = 0;
	uint32_t depth = 0;
	bool joinedHelper = false;
};

class ForkJoinPoolTest : public testing::Test {
protected:
	void TearDown() override {
		ForkJoinPool::commonPool().setSerial(false);
		reclaimAll();
	}
	ForkJoinPool& pool() { return ForkJoinPool::commonPool(); }
};

TEST_F(ForkJoinPoolTest, JoinHelpersShareTheCallersScopeId) {
	TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
	uint64_t callerScope = TaskScope::currentScopeId();
	std::vector<int32_t> items(64);
	std::iota(items.begin(), items.end(), 0);
	std::vector<Observation> seen(items.size());
	std::atomic<int64_t> sum{0};
	pool().parallelForEach(items, [&](int32_t item) {
		std::this_thread::sleep_for(1ms);
		seen[static_cast<size_t>(item)] = {std::this_thread::get_id(), TaskScope::currentScopeId(), TaskScope::depth(), TaskScope::isJoinedHelper()};
		sum.fetch_add(item);
	});
	EXPECT_EQ(sum.load(), 63 * 64 / 2);
	std::set<std::thread::id> threads;
	for (const Observation& observation : seen) {
		threads.insert(observation.thread);
		EXPECT_EQ(observation.scopeId, callerScope) << "Ptrs of the submitter stay valid on helpers";
		EXPECT_EQ(observation.joinedHelper, observation.thread != std::this_thread::get_id());
	}
	if (pool().getParallelism() >= 1)
		EXPECT_GT(threads.size(), 1u) << "helpers took part";
	EXPECT_TRUE(threads.contains(std::this_thread::get_id())) << "the caller works too";
}

TEST_F(ForkJoinPoolTest, ElementsOfStartupAndShutdownPhasesKeepThePhaseKind) {
	// M4 gate: the watchdog exempts startup and shutdown phases from STALL dumps, and a startup load element (a terrain PNG in a Debug build)
	// can run longer than the stall limit, so the helpers run the elements of such a phase with the phase's kind instead of fork-join (the
	// caller's own elements run in a nested scope, whose task info is the caller's)
	for (const char* phase : {TaskKind::STARTUP, TaskKind::SHUTDOWN, TaskKind::TEST}) {
		TaskScope scope(TaskInfo{std::source_location::current(), phase});
		std::vector<int32_t> items(64);
		std::iota(items.begin(), items.end(), 0);
		std::mutex mutex;
		std::set<std::string> helperKinds;
		pool().parallelForEach(items, [&](int32_t) {
			std::this_thread::sleep_for(1ms);
			if (TaskScope::isJoinedHelper()) {
				std::scoped_lock lock(mutex);
				helperKinds.insert(TaskScope::currentTaskInfo().kind);
			}
		});
		if (pool().getParallelism() >= 1)
			EXPECT_EQ(helperKinds, std::set<std::string>{phase == TaskKind::TEST ? TaskKind::FORK_JOIN : phase}) << phase;
	}
}

TEST_F(ForkJoinPoolTest, PerElementRunsEachElementInItsOwnScopeOnHelpers) {
	TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
	uint64_t callerScope = TaskScope::currentScopeId();
	std::vector<Ref<Npc>> spawns;
	for (int i = 0; i < 16; ++i)
		spawns.push_back(Npc::create());
	std::mutex mutex;
	std::vector<Observation> seen;
	uint_least32_t callLine = std::source_location::current().line() + 1;
	pool().parallelForEach(spawns, [&](const Ref<Npc>& spawn) {
		spawn->runs.fetch_add(1);
		std::scoped_lock lock(mutex);
		seen.push_back({std::this_thread::get_id(), TaskScope::currentScopeId(), TaskScope::depth(), TaskScope::isJoinedHelper()});
		EXPECT_EQ(TaskScope::currentTaskInfo().where.line(), callLine);
		EXPECT_STREQ(TaskScope::currentTaskInfo().kind, TaskKind::FORK_JOIN);
	}, Isolation::PER_ELEMENT);
	ASSERT_EQ(seen.size(), spawns.size());
	std::set<uint64_t> scopes;
	for (const Observation& observation : seen) {
		EXPECT_NE(observation.thread, std::this_thread::get_id()) << "only helpers run PER_ELEMENT elements";
		EXPECT_EQ(observation.depth, 1u) << "outermost scope per element";
		EXPECT_FALSE(observation.joinedHelper);
		EXPECT_NE(observation.scopeId, callerScope);
		scopes.insert(observation.scopeId);
	}
	EXPECT_EQ(scopes.size(), spawns.size());
	for (const Ref<Npc>& spawn : spawns)
		EXPECT_EQ(spawn->runs.load(), 1);
}

TEST_F(ForkJoinPoolTest, FirstExceptionIsRethrownAfterAllElementsRan) {
	std::vector<int32_t> items(40);
	std::iota(items.begin(), items.end(), 0);
	std::atomic<int32_t> ran{0};
	EXPECT_THROW(pool().parallelForEach(items, [&](int32_t item) {
		ran.fetch_add(1);
		if (item % 10 == 3)
			throw IllegalStateException("element failure");
	}), IllegalStateException);
	EXPECT_EQ(ran.load(), 40);

	ran = 0;
	EXPECT_THROW(pool().parallelForEach(items, [&](int32_t item) {
		ran.fetch_add(1);
		if (item == 39)
			throw IllegalArgumentException("last element failure");
	}, Isolation::PER_ELEMENT), IllegalArgumentException);
	EXPECT_EQ(ran.load(), 40);
}

TEST_F(ForkJoinPoolTest, SerialModeRunsInOrderOnTheCaller) {
	pool().setSerial(true); // gameserver.debug.serial_movement / single_executor
	EXPECT_TRUE(pool().isSerial());
	std::vector<int32_t> items(20);
	std::iota(items.begin(), items.end(), 0);
	std::vector<int32_t> order;
	std::set<std::thread::id> threads;
	pool().parallelForEach(items, [&](int32_t item) {
		order.push_back(item);
		threads.insert(std::this_thread::get_id());
	});
	pool().parallelForEach(items, [&](int32_t item) { order.push_back(item); }, Isolation::PER_ELEMENT);
	ASSERT_EQ(order.size(), 40u);
	for (size_t i = 0; i < 40; ++i)
		EXPECT_EQ(order[i], static_cast<int32_t>(i % 20));
	EXPECT_EQ(threads, std::set<std::thread::id>{std::this_thread::get_id()});
}

TEST_F(ForkJoinPoolTest, NestedCallsOnHelpersRunSerially) {
	TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
	std::vector<int32_t> outer(8);
	std::vector<int32_t> inner(8);
	std::atomic<int32_t> mismatches{0};
	std::atomic<int32_t> innerRuns{0};
	pool().parallelForEach(outer, [&](int32_t) {
		std::thread::id self = std::this_thread::get_id();
		bool helper = TaskScope::isJoinedHelper();
		pool().parallelForEach(inner, [&](int32_t) {
			innerRuns.fetch_add(1);
			if (helper && std::this_thread::get_id() != self)
				mismatches.fetch_add(1);
		});
	});
	EXPECT_EQ(innerRuns.load(), 64);
	EXPECT_EQ(mismatches.load(), 0);
}

// Review finding (JOIN borrows): a helper loads a pointer, hands the Ptr to the caller and leaves the job (its scope ends, it unpublishes);
// then another thread unlinks the object and the Reclaimer scans. The Ptr carries the caller's scope id, so C1 cannot flag it: the caller's
// own publication (made before dispatch) must keep the object alive until the caller's scope ends.
TEST_F(ForkJoinPoolTest, JoinBorrowsHandedFromAHelperToTheCallerSurviveTheHelpersScope) {
	reclaimAll();
	int32_t liveBefore = Npc::live.load();
	std::mutex fieldMutex; // stands in for a Field<Ref<Npc>>: loads publish first, the unlink drops the last reference
	std::optional<Ref<Npc>> field = Npc::create();
	{
		TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
		ASSERT_FALSE(TaskScope::isPublished()) << "the caller has loaded nothing yet";
		std::vector<int32_t> items(64);
		std::atomic<bool> claimed{false};
		std::optional<Ptr<Npc>> handed;
		std::mutex handedMutex;
		pool().parallelForEach(items, [&](int32_t) {
			if (TaskScope::isJoinedHelper()) {
				if (!claimed.exchange(true)) {
					TaskScope::ensurePublished(); // pointer load on the helper
					Ptr<Npc> loaded;
					{
						std::scoped_lock lock(fieldMutex);
						loaded = **field;
					}
					std::scoped_lock lock(handedMutex);
					handed = loaded;
				}
				return;
			}
			for (int i = 0; i < 5000 && !claimed.load(); ++i) // the caller lets a helper do the load (bounded)
				std::this_thread::sleep_for(1ms);
		});
		ASSERT_TRUE(claimed.load());
		ASSERT_TRUE(handed.has_value());
		// every helper left the job, so its scope ended and it is unpublished; now unlink and scan on unregistered threads (pushed immediately)
		std::thread unlinker([&] {
			std::optional<Ref<Npc>> old;
			{
				std::scoped_lock lock(fieldMutex);
				old.swap(field);
			}
			old.reset(); // last release: stamps the current epoch
			for (int i = 0; i < 4; ++i)
				Reclaimer::getInstance().reclaimNow();
		});
		unlinker.join();
		EXPECT_EQ(Npc::live.load(), liveBefore + 1) << "the object a helper handed to the caller was freed while the caller still holds the Ptr";
		EXPECT_EQ((*handed)->runs.load(), 0) << "the Ptr is still usable in the caller's scope";
	}
	reclaimAll();
	EXPECT_EQ(Npc::live.load(), liveBefore);
}

// Review finding: in JOIN mode the caller used to run elements at its own scope depth, so a quiescentPoint() reached from an element on the
// caller thread (inside a caller's QuiescentScope) unpublished it and changed its scope id while helpers still used its borrows.
TEST_F(ForkJoinPoolTest, QuiescentPointInAJoinElementIsANoOpOnTheCaller) {
	TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
	QuiescentScope quiescent;
	TaskScope::ensurePublished();
	uint64_t callerScope = TaskScope::currentScopeId();
	std::vector<int32_t> items(32);
	std::atomic<bool> callerRan{false};
	pool().parallelForEach(items, [&](int32_t) {
		if (!TaskScope::isJoinedHelper())
			callerRan = true;
		std::this_thread::sleep_for(1ms);
		quiescentPoint();
	});
	EXPECT_TRUE(callerRan.load()) << "the caller works on elements too";
	EXPECT_EQ(TaskScope::currentScopeId(), callerScope) << "quiescentPoint on the caller changed its scope id during a JOIN job";
	EXPECT_TRUE(TaskScope::isPublished()) << "quiescentPoint on the caller unpublished it during a JOIN job";
	EXPECT_EQ(TaskScope::depth(), 1u);
}

TEST_F(ForkJoinPoolTest, EmptyAndSingleElementRangesAndCallerWithoutScope) {
	std::vector<int32_t> empty;
	pool().parallelForEach(empty, [](int32_t) { FAIL(); });
	std::vector<int32_t> one{5};
	int32_t seen = 0;
	pool().parallelForEach(one, [&](int32_t item) { seen = item; });
	EXPECT_EQ(seen, 5);
	ASSERT_FALSE(TaskScope::active());
	std::vector<int32_t> items(10, 1);
	std::atomic<int32_t> sum{0};
	std::atomic<int32_t> withoutScope{0};
	pool().parallelForEach(items, [&](int32_t item) {
		sum.fetch_add(item);
		if (!TaskScope::active())
			withoutScope.fetch_add(1);
	});
	EXPECT_EQ(sum.load(), 10);
	EXPECT_EQ(withoutScope.load(), 0) << "a caller without a scope gets one for its helpers to join";
	EXPECT_FALSE(TaskScope::active());
}

} // namespace
} // namespace aion::gameserver::runtime::schedtest
