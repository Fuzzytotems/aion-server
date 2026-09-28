// Contract test of aion_gs_runtime_base (headers stage). Base is fully implemented, so these tests run.

#include <gtest/gtest.h>

#include <atomic>
#include <string>
#include <thread>

#include "aion/gameserver/runtime/base/Checked.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Finally.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/ThreadContext.h"
#include "aion/gameserver/runtime/base/YieldPoint.h"

using namespace aion::gameserver::runtime;

TEST(BaseContractTest, JavaExceptionHierarchy) {
	EXPECT_THROW(throw NullPointerException("npe"), Exception);
	EXPECT_THROW(throw ClassCastException("cce"), Exception);
	EXPECT_THROW(throw ArrayIndexOutOfBoundsException("aioobe"), IndexOutOfBoundsException);
	EXPECT_THROW(throw IllegalMonitorStateException("imse"), IllegalStateException);
	EXPECT_THROW(throw CancellationException("cancelled"), IllegalStateException);
	try {
		try {
			throw std::runtime_error("cause");
		} catch (...) {
			throw ExecutionException("failed", std::current_exception());
		}
	} catch (const ExecutionException& e) {
		EXPECT_TRUE(static_cast<bool>(e.cause()));
	}
}

TEST(BaseContractTest, TaskInfoMacroRecordsCallSite) {
	TaskInfo info = AION_TASK_INFO(TaskKind::TEST);
	EXPECT_STREQ(info.kind, TaskKind::TEST);
	EXPECT_NE(std::string(info.where.file_name()).find("BaseContractTest"), std::string::npos);
	EXPECT_TRUE(std::is_trivially_copyable_v<TaskInfo>);
}

TEST(BaseContractTest, ThreadContextIsRegisteredLazilyAndVisible) {
	ThreadContext& context = ThreadContext::current();
	EXPECT_EQ(&context, ThreadContext::currentIfRegistered());
	EXPECT_NE(context.threadId(), 0u);
	EXPECT_EQ(context.publishedEpoch.load(), EPOCH_IDLE);

	context.setTask(AION_TASK_INFO(TaskKind::TEST), 42, 7);
	bool found = false;
	ThreadContext::forEach([&](const ThreadContext& other) {
		if (&other == &context) {
			ThreadContext::TaskSnapshot task = other.task();
			found = task.active && task.scopeId == 42 && task.startNanos == 7;
		}
	});
	context.clearTask();
	EXPECT_TRUE(found);
	EXPECT_FALSE(context.task().active);
}

TEST(BaseContractTest, ThreadIdsAreNotReusedWhenRecordsAre) {
	std::atomic<uint64_t> first{0};
	std::atomic<uint64_t> second{0};
	std::thread([&] { first = ThreadContext::current().threadId(); }).join();
	std::thread([&] { second = ThreadContext::current().threadId(); }).join();
	EXPECT_NE(first.load(), second.load());
}

TEST(BaseContractTest, BlockingAndWaitSnapshots) {
	ThreadContext& context = ThreadContext::current();
	context.setBlocking("test.block", std::source_location::current(), 1);
	EXPECT_TRUE(context.blocking().active);
	EXPECT_STREQ(context.blocking().what, "test.block");
	context.clearBlocking();
	EXPECT_FALSE(context.blocking().active);

	context.setWait("Owner::lock", 0x1234, 99, 5);
	ThreadContext::WaitSnapshot wait = context.wait();
	EXPECT_TRUE(wait.waiting);
	EXPECT_EQ(wait.ownerThreadId, 99u);
	context.clearWait();
	EXPECT_FALSE(context.wait().waiting);
}

namespace {
std::atomic<int> yieldCount{0};
void countYield(const char*) noexcept {
	yieldCount.fetch_add(1);
}
} // namespace

TEST(BaseContractTest, YieldPointsCallInstalledHooks) {
	static constexpr pct::PctHooks hooks{&countYield, nullptr, nullptr};
	pct::installHooks(&hooks);
	AION_YIELD_POINT("test");
	AION_PCT_BLOCKING_BEGIN("test");
	AION_PCT_BLOCKING_END();
	pct::installHooks(nullptr);
	AION_YIELD_POINT("test");
#if AION_PCT
	EXPECT_EQ(yieldCount.load(), 1);
#else
	EXPECT_EQ(yieldCount.load(), 0);
#endif
}

TEST(BaseContractTest, FinallyRunsOnExceptionAndCanBeDismissed) {
	int runs = 0;
	try {
		auto guard = finally([&]() noexcept { ++runs; });
		throw IllegalStateException("body failed");
	} catch (const IllegalStateException&) {
	}
	EXPECT_EQ(runs, 1);
	{
		auto guard = finally([&]() noexcept { ++runs; });
		guard.dismiss();
	}
	EXPECT_EQ(runs, 1);
}

#if defined(AION_ASAN)
namespace {
int readAfterFree() {
	int* volatile pointer = new int(42);
	delete pointer;
	return *pointer;
}
} // namespace

TEST(BaseContractDeathTest, AddressSanitizerIsActive) {
	GTEST_FLAG_SET(death_test_style, "threadsafe");
	EXPECT_DEATH((void)readAfterFree(), "heap-use-after-free");
}
#endif

#if AION_CHECKED
TEST(BaseContractDeathTest, CheckFailureTerminates) {
	GTEST_FLAG_SET(death_test_style, "threadsafe");
	EXPECT_DEATH(AION_CHECK("C0", false, "expected failure"), "expected failure");
}
#endif
