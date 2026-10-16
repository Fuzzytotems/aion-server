// SerialExecutor (design §1.4 "LS/CS link packets run in order per link", §11) on the real pools and on the DeterministicExecutor.

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "SchedTestSupport.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Finally.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/SerialExecutor.h"

namespace aion::gameserver::runtime::schedtest {
namespace {

using namespace std::chrono_literals;

class SerialExecutorTest : public SchedBackendTest {};

INSTANTIATE_TEST_SUITE_P(Backends, SerialExecutorTest, testing::Values(BackendKind::REAL, BackendKind::DETERMINISTIC), backendName);

struct LinkLog {
	std::mutex mutex;
	std::vector<int32_t> order;
	std::atomic<int32_t> running{0};
	std::atomic<int32_t> overlaps{0};
	std::atomic<int32_t> completed{0};
};

TEST_P(SerialExecutorTest, TasksRunInSubmissionOrderWithoutOverlap) {
	constexpr int32_t TASKS = 300;
	LinkLog log;
	SerialExecutor link("LoginServerConnection");
	for (int32_t i = 0; i < TASKS; ++i) {
		link.execute(Pin(), [&log, i] {
			if (log.running.fetch_add(1) != 0)
				log.overlaps.fetch_add(1);
			{
				std::scoped_lock lock(log.mutex);
				log.order.push_back(i);
			}
			if (i % 50 == 0)
				std::this_thread::yield();
			log.running.fetch_sub(1);
			log.completed.fetch_add(1);
		});
	}
	ASSERT_TRUE(waitUntil([&] { return log.completed.load() == TASKS; }, 5000ms));
	EXPECT_EQ(log.overlaps.load(), 0);
	std::scoped_lock lock(log.mutex);
	ASSERT_EQ(log.order.size(), static_cast<size_t>(TASKS));
	for (int32_t i = 0; i < TASKS; ++i)
		ASSERT_EQ(log.order[static_cast<size_t>(i)], i);
	EXPECT_TRUE(waitUntil([&] { return !link.isActive(); }, 1000ms));
	EXPECT_EQ(link.queuedTasks(), 0u);
}

TEST_P(SerialExecutorTest, TwoLinksRunIndependentlyAndExceptionsDoNotStopTheLink) {
	LinkLog chat;
	LinkLog login;
	SerialExecutor chatLink("ChatServerConnection");
	SerialExecutor loginLink("LoginServerConnection");
	for (int32_t i = 0; i < 20; ++i) {
		chatLink.execute(Pin(), [&chat, i] {
			{
				std::scoped_lock lock(chat.mutex);
				chat.order.push_back(i);
			}
			chat.completed.fetch_add(1);
			if (i == 3)
				throw IllegalStateException("packet handler failure (expected by the test)");
		});
		loginLink.execute([] {}); // unpinned form
		loginLink.execute(Pin(), [&login] { login.completed.fetch_add(1); });
	}
	ASSERT_TRUE(waitUntil([&] { return chat.completed.load() == 20 && login.completed.load() == 20; }, 5000ms));
	std::scoped_lock lock(chat.mutex);
	for (int32_t i = 0; i < 20; ++i)
		EXPECT_EQ(chat.order[static_cast<size_t>(i)], i);
}

TEST_P(SerialExecutorTest, EachTaskRunsInItsOwnScopeWithItsCallSite) {
	std::vector<uint64_t> scopeIds;
	std::vector<uint_least32_t> lines;
	std::mutex mutex;
	std::atomic<int32_t> completed{0};
	SerialExecutor link("CS");
	auto record = [&] {
		std::scoped_lock lock(mutex);
		scopeIds.push_back(TaskScope::currentScopeId());
		lines.push_back(TaskScope::currentTaskInfo().where.line());
		EXPECT_EQ(TaskScope::depth(), 1u);
		EXPECT_STREQ(TaskScope::currentTaskInfo().kind, TaskKind::SERIAL);
		completed.fetch_add(1);
	};
	uint_least32_t first = std::source_location::current().line() + 1;
	link.execute(Pin(), record);
	uint_least32_t second = std::source_location::current().line() + 1;
	link.execute(Pin(), record);
	ASSERT_TRUE(waitUntil([&] { return completed.load() == 2; }, 1000ms));
	std::scoped_lock lock(mutex);
	EXPECT_NE(scopeIds[0], scopeIds[1]);
	EXPECT_EQ(lines[0], first);
	EXPECT_EQ(lines[1], second);
}

// Review finding: with a full instant queue the rejection policy runs a dispatched successor inline on the dispatching thread; the successor's
// continuation used to dispatch the next task inline again, one stack level per queued task (stack overflow for long LS/CS link queues).
TEST_P(SerialExecutorTest, HandOverUnderAFullInstantQueueDoesNotRecurse) {
	if (isDeterministic()) {
		GTEST_SKIP() << "the DeterministicExecutor has no bounded queue (rejection is a real-pool policy)";
	}
	install(BackendKind::REAL, testConfig(), 1, 1);
	auto gate = std::make_shared<std::mutex>(); // shared with the blocking task: it may take the gate after this test body returned
	std::unique_lock blocker(*gate);
	auto blockerStarted = std::make_shared<std::atomic<bool>>(false);
	auto unblockOnExit = finally([&blocker]() noexcept {
		if (blocker.owns_lock())
			blocker.unlock();
	});
	ThreadPoolManager::getInstance().execute(Pin(), [gate, blockerStarted] {
		blockerStarted->store(true);
		std::scoped_lock wait(*gate);
	});
	ASSERT_TRUE(waitUntil([&] { return blockerStarted->load(); }, 1000ms));
	ThreadPoolManager::getInstance().execute(Pin(), [] {}); // occupies the only queue slot: every later dispatch is rejected and runs inline

	constexpr int32_t TASKS = 150; // without the trampoline: ~3 KB of stack per task in Debug (a stack overflow for long link queues)
	SerialExecutor link("CacheServerConnection");
	std::vector<int32_t> order;
	uintptr_t lowest = UINTPTR_MAX;
	uintptr_t highest = 0;
	auto recordStack = [&lowest, &highest] {
		char marker = 0;
		auto address = static_cast<uintptr_t>(reinterpret_cast<std::uintptr_t>(&marker));
		lowest = std::min(lowest, address);
		highest = std::max(highest, address);
	};
	link.execute(Pin(), [&] { // runs inline on this thread (rejected); queues the rest while it is active
		recordStack();
		order.push_back(0);
		for (int32_t i = 1; i < TASKS; ++i) {
			link.execute(Pin(), [&, i] {
				recordStack();
				order.push_back(i);
			});
		}
	});
	ASSERT_EQ(order.size(), static_cast<size_t>(TASKS)) << "every task ran inline on the caller, synchronously";
	for (int32_t i = 0; i < TASKS; ++i)
		EXPECT_EQ(order[static_cast<size_t>(i)], i);
	EXPECT_LT(highest - lowest, size_t{64 * 1024}) << "hand-over recursion: the stack grew by " << (highest - lowest) << " bytes over " << TASKS << " tasks";
	EXPECT_FALSE(link.isActive());
}

TEST_P(SerialExecutorTest, DestroyingTheExecutorCancelsQueuedTasks) {
	Ref<Npc> npc = Npc::create();
	Npc* raw = npc.get();
	std::mutex gate;
	std::unique_lock blocker(gate);
	auto unblockOnExit = finally([&blocker]() noexcept {
		if (blocker.owns_lock())
			blocker.unlock();
	});
	std::atomic<bool> firstStarted{false};
	auto link = std::make_unique<SerialExecutor>("closing");
	if (isDeterministic()) {
		link->execute(raw, [raw] { raw->runs.fetch_add(1); });
		link->execute(raw, [raw] { raw->runs.fetch_add(1); });
		EXPECT_EQ(link->queuedTasks(), 1u);
		link.reset();
		EXPECT_EQ(npc->refCount(), 2u) << "queued task cancelled, the dispatched one still pinned";
		deterministic->runReady();
		EXPECT_EQ(raw->runs.load(), 1);
		EXPECT_EQ(npc->refCount(), 1u);
		return;
	}
	link->execute(raw, [raw, &gate, &firstStarted] {
		firstStarted = true;
		std::scoped_lock wait(gate);
		raw->runs.fetch_add(1);
	});
	link->execute(raw, [raw] { raw->runs.fetch_add(100); });
	ASSERT_TRUE(waitUntil([&] { return firstStarted.load(); }, 1000ms));
	link.reset(); // memory-safe while the first task runs
	EXPECT_EQ(npc->refCount(), 2u);
	blocker.unlock();
	ASSERT_TRUE(waitUntil([&] { return npc->refCount() == 1u; }, 1000ms));
	EXPECT_EQ(raw->runs.load(), 1);
}

} // namespace
} // namespace aion::gameserver::runtime::schedtest
