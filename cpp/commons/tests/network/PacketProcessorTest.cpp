#include <gtest/gtest.h>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "NetworkTestUtils.h"
#include "aion/commons/network/PacketProcessor.h"
#include "aion/commons/utils/Exception.h"

using namespace nettest;
using namespace aion::commons;
using network::PacketProcessor;
using network::packet::BaseClientPacket;

namespace {

struct FakeConnection {
	explicit FakeConnection(int id) : id(id) {}
	std::string toString() const { return "FakeConnection #" + std::to_string(id); }

	const int id;
	std::atomic<int> running = 0;
	std::atomic<bool> concurrentExecution = false;
	std::mutex mutex;
	std::vector<int> executed;
};

class CM_TASK : public BaseClientPacket<FakeConnection> {
public:
	CM_TASK(std::shared_ptr<FakeConnection> connection, int sequence, std::function<void()> action = {})
		: BaseClientPacket(1), sequence(sequence), action(std::move(action)) {
		setConnection(std::move(connection));
		instances++;
	}
	~CM_TASK() override { instances--; }

	static inline std::atomic<int> instances = 0;

protected:
	void readImpl() override {}

	void runImpl() override {
		FakeConnection& con = *getConnection();
		if (++con.running != 1)
			con.concurrentExecution = true;
		if (action)
			action();
		{
			std::lock_guard lock(con.mutex);
			con.executed.push_back(sequence);
		}
		con.running--;
	}

private:
	const int sequence;
	const std::function<void()> action;
};

/** Blocks packets until opened. */
class Gate {
public:
	void pass() {
		std::unique_lock lock(mutex);
		waiting++;
		changed.notify_all();
		changed.wait(lock, [this] { return open; });
		waiting--;
	}
	void release() {
		std::lock_guard lock(mutex);
		open = true;
		changed.notify_all();
	}
	int waitingCount() {
		std::lock_guard lock(mutex);
		return waiting;
	}

private:
	std::mutex mutex;
	std::condition_variable changed;
	bool open = false;
	int waiting = 0;
};

std::unique_ptr<CM_TASK> task(const std::shared_ptr<FakeConnection>& con, int sequence, std::function<void()> action = {}) {
	return std::make_unique<CM_TASK>(con, sequence, std::move(action));
}

size_t executedCount(FakeConnection& con) {
	std::lock_guard lock(con.mutex);
	return con.executed.size();
}

} // namespace

TEST(PacketProcessorTest, InvalidArgumentsThrow) {
	auto expectMessage = [](std::function<void()> create, std::string_view message) {
		try {
			create();
			FAIL() << "expected IllegalArgumentException: " << message;
		} catch (const utils::IllegalArgumentException& e) {
			EXPECT_EQ(std::string_view(e.what()), message);
		}
	};
	expectMessage([] { PacketProcessor<FakeConnection>(0, 1, 1, 1); }, "Min Threads must be positive");
	expectMessage([] { PacketProcessor<FakeConnection>(2, 1, 1, 1); }, "Max Threads must be >= Min Threads");
	expectMessage([] { PacketProcessor<FakeConnection>(1, 1, 0, 1); }, "Thread Spawn Threshold must be positive");
	expectMessage([] { PacketProcessor<FakeConnection>(1, 1, 1, 0); }, "Thread Kill Threshold must be positive");
}

TEST(PacketProcessorTest, PacketsOfOneConnectionRunOneAtATimeInOrder) {
	PacketProcessor<FakeConnection> processor(4, 4, 50, 3);
	constexpr int connectionCount = 6;
	constexpr int packetsPerConnection = 400;
	std::vector<std::shared_ptr<FakeConnection>> connections;
	for (int i = 0; i < connectionCount; i++)
		connections.push_back(std::make_shared<FakeConnection>(i));

	// like IO threads: each connection's packets are queued by one thread, several connections concurrently
	std::vector<std::thread> producers;
	for (auto& con : connections) {
		producers.emplace_back([&processor, con] {
			for (int i = 0; i < packetsPerConnection; i++)
				processor.executePacket(task(con, i, [i] {
					if (i % 50 == 0)
						std::this_thread::yield();
				}));
		});
	}
	for (auto& producer : producers)
		producer.join();

	for (auto& con : connections) {
		ASSERT_TRUE(waitUntil([&] { return executedCount(*con) == packetsPerConnection; })) << con->toString();
		EXPECT_FALSE(con->concurrentExecution) << con->toString();
		std::lock_guard lock(con->mutex);
		for (int i = 0; i < packetsPerConnection; i++)
			ASSERT_EQ(con->executed[i], i) << con->toString();
	}
	EXPECT_TRUE(waitUntil([] { return CM_TASK::instances == 0; })); // packets are destroyed after execution
}

TEST(PacketProcessorTest, DifferentConnectionsRunInParallelUpToMaxThreads) {
	PacketProcessor<FakeConnection> processor(3, 3, 50, 3);
	Gate gate;
	std::set<std::string> threadNames;
	std::mutex namesMutex;
	std::vector<std::shared_ptr<FakeConnection>> connections;
	for (int i = 0; i < 5; i++) {
		connections.push_back(std::make_shared<FakeConnection>(i));
		processor.executePacket(task(connections.back(), 0, [&] {
			{
				std::lock_guard lock(namesMutex);
				threadNames.insert(utils::concurrent::getCurrentThreadName());
			}
			gate.pass();
		}));
	}
	ASSERT_TRUE(waitUntil([&] { return gate.waitingCount() == 3; }));
	std::this_thread::sleep_for(50ms);
	EXPECT_EQ(gate.waitingCount(), 3); // never more than maxThreads
	EXPECT_EQ(processor.getWaitingPacketCount(), 2);
	gate.release();

	for (auto& con : connections)
		ASSERT_TRUE(waitUntil([&] { return executedCount(*con) == 1; }));
	std::lock_guard lock(namesMutex);
	EXPECT_EQ(threadNames, (std::set<std::string>{"PacketProcessor:0", "PacketProcessor:1", "PacketProcessor:2"}));
}

TEST(PacketProcessorTest, BusyConnectionDoesNotBlockOthers) {
	PacketProcessor<FakeConnection> processor(2, 2, 50, 3);
	Gate gate;
	auto slow = std::make_shared<FakeConnection>(1);
	auto fast = std::make_shared<FakeConnection>(2);
	processor.executePacket(task(slow, 0, [&] { gate.pass(); }));
	processor.executePacket(task(slow, 1)); // must wait for packet 0 although a thread is idle
	for (int i = 0; i < 10; i++)
		processor.executePacket(task(fast, i));
	ASSERT_TRUE(waitUntil([&] { return executedCount(*fast) == 10; }));
	EXPECT_EQ(executedCount(*slow), 0u);
	gate.release();
	ASSERT_TRUE(waitUntil([&] { return executedCount(*slow) == 2; }));
	std::lock_guard lock(slow->mutex);
	EXPECT_EQ(slow->executed, (std::vector<int>{0, 1}));
}

TEST(PacketProcessorTest, ExceptionsDoNotStopWorkers) {
	LogCapture& logs = LogCapture::instance();
	logs.clear();
	PacketProcessor<FakeConnection> processor(1, 1, 50, 3);
	auto con = std::make_shared<FakeConnection>(1);
	processor.executePacket(task(con, 0, [] { throw utils::IllegalStateException("packet failure"); }));
	processor.executePacket(task(con, 1, [] { throw 42; }));
	processor.executePacket(task(con, 2));
	ASSERT_TRUE(waitUntil([&] { return executedCount(*con) == 1; }));
	EXPECT_EQ(con->executed[0], 2);
	EXPECT_EQ(logs.count("error|com.aionemu.commons.network.PacketProcessor|Unhandled exception while executing client packet [001] CM_TASK"), 2);
	EXPECT_TRUE(logs.contains("packet failure"));
	EXPECT_EQ(processor.getThreadCount(), 1);
}

TEST(PacketProcessorTest, ExecutorDecoratorExecutesPackets) {
	std::atomic<int> decorated = 0;
	PacketProcessor<FakeConnection> processor(1, 1, 50, 3, [&](network::packet::ClientPacketBase& packet) {
		decorated++;
		packet.run();
	});
	auto con = std::make_shared<FakeConnection>(1);
	for (int i = 0; i < 5; i++)
		processor.executePacket(task(con, i));
	ASSERT_TRUE(waitUntil([&] { return executedCount(*con) == 5; }));
	EXPECT_EQ(decorated, 5);
}

TEST(PacketProcessorTest, CheckerAdaptsThreadCountLikeJava) {
	LogCapture& logs = LogCapture::instance();
	logs.clear();
	PacketProcessor<FakeConnection> processor(1, 3, 2, 1);
	EXPECT_EQ(processor.getThreadCount(), 1);
	Gate gate;
	std::vector<std::shared_ptr<FakeConnection>> connections;
	auto submitBlocked = [&] {
		connections.push_back(std::make_shared<FakeConnection>(static_cast<int>(connections.size())));
		processor.executePacket(task(connections.back(), 0, [&] { gate.pass(); }));
	};

	submitBlocked();
	ASSERT_TRUE(waitUntil([&] { return gate.waitingCount() == 1; }));
	for (int i = 0; i < 7; i++)
		submitBlocked();
	EXPECT_EQ(processor.getWaitingPacketCount(), 7);

	processor.checkThreadCount(); // 7 waiting > spawn threshold 2: spawn
	EXPECT_EQ(processor.getThreadCount(), 2);
	ASSERT_TRUE(waitUntil([&] { return processor.getWaitingPacketCount() == 6; }));

	processor.checkThreadCount(); // 6 waiting: spawn
	EXPECT_EQ(processor.getThreadCount(), 3);
	ASSERT_TRUE(waitUntil([&] { return processor.getWaitingPacketCount() == 5; }));

	processor.checkThreadCount(); // 5 waiting: max threads reached, but 5 < 3 * 2, so no lag warning
	EXPECT_EQ(processor.getThreadCount(), 3);
	EXPECT_FALSE(logs.contains("Lag detected!"));

	submitBlocked();
	submitBlocked();
	processor.checkThreadCount(); // 7 waiting >= 6
	EXPECT_EQ(processor.getThreadCount(), 3);
	EXPECT_TRUE(logs.contains(
		"warning|com.aionemu.commons.network.PacketProcessor|Lag detected! [7 client packets are waiting for execution]. You should consider increasing PacketProcessor maxThreads or hardware upgrade."));

	gate.release();
	for (auto& con : connections)
		ASSERT_TRUE(waitUntil([&] { return executedCount(*con) == 1; }));
	EXPECT_EQ(processor.getWaitingPacketCount(), 0);

	processor.checkThreadCount(); // 0 waiting <= previous 7 and <= kill threshold 1: kill
	EXPECT_EQ(processor.getThreadCount(), 2);
	processor.checkThreadCount();
	EXPECT_EQ(processor.getThreadCount(), 1);
	processor.checkThreadCount(); // min threads reached
	EXPECT_EQ(processor.getThreadCount(), 1);

	// the remaining thread still works, and killed threads were joined without blocking
	auto con = std::make_shared<FakeConnection>(100);
	processor.executePacket(task(con, 0));
	ASSERT_TRUE(waitUntil([&] { return executedCount(*con) == 1; }));
}

TEST(PacketProcessorTest, WaitingCountGrowthPreventsKill) {
	PacketProcessor<FakeConnection> processor(1, 2, 5, 3);
	Gate gate;
	auto blocker = std::make_shared<FakeConnection>(0);
	processor.executePacket(task(blocker, 0, [&] { gate.pass(); }));
	ASSERT_TRUE(waitUntil([&] { return gate.waitingCount() == 1; }));

	processor.checkThreadCount(); // 0 waiting: kill attempt, but already at min threads
	auto other = std::make_shared<FakeConnection>(1);
	processor.executePacket(task(other, 0));
	processor.executePacket(task(other, 1));
	// 2 waiting > previous 0: no kill; 2 <= spawn threshold 5: no spawn
	ASSERT_TRUE(waitUntil([&] { return processor.getWaitingPacketCount() == 2; }));
	processor.checkThreadCount();
	EXPECT_EQ(processor.getThreadCount(), 1);
	gate.release();
	ASSERT_TRUE(waitUntil([&] { return executedCount(*other) == 2; }));
}

TEST(PacketProcessorTest, ShutdownDiscardsWaitingPacketsAndJoins) {
	CM_TASK::instances = 0;
	std::atomic<int> ran = 0;
	{
		PacketProcessor<FakeConnection> processor(1, 4, 50, 3);
		Gate gate;
		auto con = std::make_shared<FakeConnection>(1);
		processor.executePacket(task(con, 0, [&] {
			ran++;
			gate.pass();
		}));
		ASSERT_TRUE(waitUntil([&] { return gate.waitingCount() == 1; }));
		for (int i = 1; i < 10; i++)
			processor.executePacket(task(con, i, [&] { ran++; }));

		std::thread releaser([&] {
			std::this_thread::sleep_for(50ms);
			gate.release();
		});
		processor.shutdown(); // waits for the running packet
		releaser.join();
		EXPECT_EQ(ran, 1);
		EXPECT_EQ(CM_TASK::instances, 0);
		EXPECT_EQ(processor.getWaitingPacketCount(), 0);

		processor.executePacket(task(con, 99, [&] { ran++; }));
		EXPECT_EQ(CM_TASK::instances, 0);
		processor.shutdown();
	}
	EXPECT_EQ(ran, 1);
}

TEST(PacketProcessorTest, ShutdownFromPacketDoesNotDeadlock) {
	std::atomic<bool> shutdownReturned = false;
	auto processor = std::make_unique<PacketProcessor<FakeConnection>>(2, 2, 50, 3);
	auto con = std::make_shared<FakeConnection>(1);
	processor->executePacket(task(con, 0, [&] {
		processor->shutdown();
		shutdownReturned = true;
	}));
	ASSERT_TRUE(waitUntil([&] { return shutdownReturned.load(); }));
	processor.reset(); // waits for the detached worker
	EXPECT_EQ(executedCount(*con), 1u);
}

TEST(PacketProcessorTest, PacketKeepsConnectionAliveUntilExecuted) {
	PacketProcessor<FakeConnection> processor(1, 1, 50, 3);
	Gate gate;
	auto blocker = std::make_shared<FakeConnection>(0);
	processor.executePacket(task(blocker, 0, [&] { gate.pass(); }));

	auto con = std::make_shared<FakeConnection>(1);
	std::weak_ptr<FakeConnection> weak = con;
	processor.executePacket(task(con, 0));
	con.reset();
	EXPECT_FALSE(weak.expired());
	gate.release();
	EXPECT_TRUE(waitUntil([&] { return weak.expired(); }));
}
