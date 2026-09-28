// The replacement of Netty's ExecutionHandler with an OrderedMemoryAwareThreadPoolExecutor: the events of a channel run one at a time in order,
// channels run in parallel, and the shutdown does not wait for a task beyond its timeout.

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "aion/chatserver/network/netty/pipeline/ExecutionHandler.h"
#include "support/TestUtils.h"

namespace aion::chatserver::test {
namespace {

using network::netty::pipeline::ExecutionHandler;

/** Flags shared with the tasks; tasks capture it by shared_ptr, so a task that outlives the test (a detached thread) stays valid. */
struct Flags {
	std::atomic<bool> release = false;
	std::atomic<bool> firstStarted = false;
	std::atomic<bool> secondStarted = false;
	std::atomic<bool> finished = false;
};

TEST(ExecutionHandlerTest, TasksOfAChannelRunInSubmissionOrder) {
	auto handler = std::make_shared<ExecutionHandler>(4);
	int channel = 0;
	std::mutex mutex;
	std::vector<int> order;
	std::atomic<int> running = 0;
	auto task = [&](int i) {
		return [&, i] {
			running++;
			std::this_thread::sleep_for(1ms);
			{
				std::lock_guard lock(mutex);
				order.push_back(i);
			}
			running--;
		};
	};
	// queued all at once, then one at a time while the previous one runs and nothing else is queued (the channel is running, its queue empty)
	for (int i = 0; i < 100; i++)
		handler->execute(&channel, task(i));
	for (int i = 100; i < 150; i++) {
		waitUntil([&] {
			size_t pending = handler->getPendingTaskCount();
			return (pending == 1 && running.load() == 1) || pending == 0;
		});
		handler->execute(&channel, task(i));
	}
	handler->shutdown(5s);
	ASSERT_EQ(order.size(), 150u);
	for (int i = 0; i < 150; i++)
		EXPECT_EQ(order[static_cast<size_t>(i)], i);
}

TEST(ExecutionHandlerTest, ATaskOfARunningChannelWaitsUntilTheRunningOneEnded) {
	// a second thread is free, but the channel's second task must not start while its first one runs
	auto handler = std::make_shared<ExecutionHandler>(2);
	int channel = 0;
	auto flags = std::make_shared<Flags>();
	handler->execute(&channel, [flags] {
		flags->firstStarted = true;
		waitUntil([&] { return flags->release.load(); }, 5s);
	});
	ASSERT_TRUE(waitUntil([&] { return flags->firstStarted.load(); }));
	handler->execute(&channel, [flags] { flags->secondStarted = true; });
	std::this_thread::sleep_for(200ms);
	EXPECT_FALSE(flags->secondStarted);
	std::fflush(stdout); // a handler that ran both at once may crash when the first one ends (corrupt queue state): report the failure before
	flags->release = true;
	EXPECT_TRUE(waitUntil([&] { return flags->secondStarted.load(); }));
	handler->shutdown(5s);
}

TEST(ExecutionHandlerTest, ChannelsRunInParallel) {
	auto handler = std::make_shared<ExecutionHandler>(2);
	int first = 0;
	int second = 0;
	auto flags = std::make_shared<Flags>();
	handler->execute(&first, [flags] {
		// blocks until the second channel's task ran, which needs a second thread (at most 10 s, so a single thread fails the check below)
		waitUntil([&] { return flags->release.load(); }, 10s);
	});
	handler->execute(&second, [flags] {
		flags->secondStarted = true;
		flags->release = true;
	});
	EXPECT_TRUE(waitUntil([&] { return flags->secondStarted.load(); }, 3s));
	handler->shutdown(5s);
}

TEST(ExecutionHandlerTest, AnIdleThreadDoesNotMakeASecondChannelWait) {
	// a thread that was notified but did not take its channel yet still counts as idle: two channels becoming ready at once need two threads
	for (int round = 0; round < 3; round++) {
		auto handler = std::make_shared<ExecutionHandler>(2);
		int warmUp = 0;
		int first = 0;
		int second = 0;
		handler->execute(&warmUp, [] {});
		ASSERT_TRUE(waitUntil([&] { return handler->getPendingTaskCount() == 0; })); // one thread, idle
		auto flags = std::make_shared<Flags>();
		handler->execute(&first, [flags] { waitUntil([&] { return flags->release.load(); }, 3s); });
		handler->execute(&second, [flags] { flags->secondStarted = true; });
		EXPECT_TRUE(waitUntil([&] { return flags->secondStarted.load(); }, 1s)) << "round " << round;
		flags->release = true;
		handler->shutdown(5s);
	}
}

TEST(ExecutionHandlerTest, ChannelsAreForgottenWhenTheirLastTaskEnded) {
	auto handler = std::make_shared<ExecutionHandler>(2);
	int channels[3] = {};
	auto flags = std::make_shared<Flags>();
	handler->execute(&channels[0], [flags] { waitUntil([&] { return flags->release.load(); }, 5s); });
	handler->execute(&channels[1], [] {});
	handler->execute(&channels[2], [] {});
	waitUntil([&] { return handler->getPendingTaskCount() == 1; });
	EXPECT_EQ(handler->getChannelCount(), 1u);
	flags->release = true;
	waitUntil([&] { return handler->getPendingTaskCount() == 0; });
	EXPECT_EQ(handler->getChannelCount(), 0u);
	handler->shutdown(5s);
}

TEST(ExecutionHandlerTest, ShutdownRunsQueuedTasksAndDiscardsLaterOnes) {
	auto handler = std::make_shared<ExecutionHandler>(1);
	int channel = 0;
	std::atomic<int> ran = 0;
	for (int i = 0; i < 50; i++)
		handler->execute(&channel, [&] { ran++; });
	handler->shutdown(5s);
	EXPECT_EQ(ran.load(), 50);
	handler->execute(&channel, [&] { ran++; });
	std::this_thread::sleep_for(50ms);
	EXPECT_EQ(ran.load(), 50);
	EXPECT_EQ(handler->getPendingTaskCount(), 0u);
}

TEST(ExecutionHandlerTest, ShutdownDoesNotWaitForARunningTaskBeyondItsTimeout) {
	// e.g. a chat log insert on a database that does not answer: the thread is left to end after its task, holding the handler
	LogCapture log({"org.jboss.netty.handler.execution.ExecutionHandler"});
	auto handler = std::make_shared<ExecutionHandler>(1);
	int channel = 0;
	auto flags = std::make_shared<Flags>();
	handler->execute(&channel, [flags] {
		flags->firstStarted = true;
		waitUntil([&] { return flags->release.load(); }, 5s);
		flags->finished = true;
	});
	ASSERT_TRUE(waitUntil([&] { return flags->firstStarted.load(); }));
	handler->execute(&channel, [flags] { flags->secondStarted = true; });
	auto start = std::chrono::steady_clock::now();
	handler->shutdown(200ms);
	EXPECT_LT(std::chrono::steady_clock::now() - start, 2s);
	EXPECT_TRUE(log.contains("Discarded 1 channel events on shutdown")) << log.dump();
	EXPECT_TRUE(log.contains("1 threads still run a channel event after the shutdown timeout, they end after it")) << log.dump();
	handler.reset(); // the running thread keeps it
	flags->release = true;
	waitUntil([&] { return flags->finished.load(); });
}

TEST(ExecutionHandlerTest, AnExceptionIsLoggedAndTheChannelContinues) {
	LogCapture log({"org.jboss.netty.handler.execution.ExecutionHandler"});
	auto handler = std::make_shared<ExecutionHandler>(1);
	int channel = 0;
	std::atomic<bool> ran = false;
	handler->execute(&channel, [] { throw std::runtime_error("boom"); });
	handler->execute(&channel, [&] { ran = true; });
	EXPECT_TRUE(waitUntil([&] { return ran.load(); }));
	EXPECT_TRUE(log.contains("Exception while executing a channel event")) << log.dump();
	handler->shutdown(5s);
}

} // namespace
} // namespace aion::chatserver::test
