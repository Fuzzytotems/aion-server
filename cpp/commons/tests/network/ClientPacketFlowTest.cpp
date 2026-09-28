#include <gtest/gtest.h>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>

#include "NetworkTestUtils.h"
#include "aion/commons/network/PacketProcessor.h"
#include "aion/commons/network/packet/BaseClientPacket.h"

// The path every server uses: AConnection::processData creates a client packet bound to the concrete connection type, reads it from the slice of the
// read buffer and hands it to a PacketProcessor (Java: AionConnection.processData -> AionPacketHandlerFactory.handle(data, this) ->
// PacketProcessor.executePacket).

using namespace nettest;
using namespace aion::commons;

namespace {

class FlowConnection;

/** Blocks the first executed packet until released. */
struct FlowState {
	std::mutex mutex;
	std::condition_variable changed;
	bool released = false;
	bool blocking = false;
	std::vector<int32_t> executed;
	std::vector<bool> closedAtExecution;
	std::atomic<int> disconnectCount = 0;

	size_t executedCount() {
		std::lock_guard lock(mutex);
		return executed.size();
	}
};

/** Client packet with an int32 value, replying with value + 1. */
class CM_VALUE : public network::packet::BaseClientPacket<FlowConnection> {
public:
	CM_VALUE(utils::ByteBuffer buffer, std::shared_ptr<FlowState> state) : BaseClientPacket(std::move(buffer), 0x01), state(std::move(state)) {}

protected:
	void readImpl() override { value = readD(); }
	void runImpl() override;

private:
	const std::shared_ptr<FlowState> state;
	int32_t value = 0;
};

class FlowConnection : public network::AConnection<TestServerPacket> {
public:
	FlowConnection(asio::ip::tcp::socket socket, network::NioServer& server, network::PacketProcessor<FlowConnection>& processor,
		std::shared_ptr<FlowState> state)
		: AConnection(std::move(socket), server, 1024, 1024), processor(processor), state(std::move(state)) {}

protected:
	bool processData(utils::ByteBuffer& data) override {
		auto packet = std::make_unique<CM_VALUE>(data, state); // shares the read buffer, only valid during this call
		std::shared_ptr<FlowConnection> self = sharedFromThis(); // typed as the concrete connection
		packet->setConnection(std::move(self));
		if (packet->read())
			processor.executePacket(std::move(packet));
		return true;
	}

	bool writeData(utils::ByteBuffer& data) override {
		if (sendMsgQueue.empty())
			return false;
		auto packet = std::move(sendMsgQueue.front());
		sendMsgQueue.pop_front();
		packet->write(data);
		return true;
	}

	void initialized() override {}
	void onDisconnect() override { state->disconnectCount++; }
	void onServerClose() override { close(); }

private:
	network::PacketProcessor<FlowConnection>& processor;
	const std::shared_ptr<FlowState> state;
};

void CM_VALUE::runImpl() {
	{
		std::unique_lock lock(state->mutex);
		if (state->blocking)
			state->changed.wait(lock, [&] { return state->released; });
		state->executed.push_back(value);
		state->closedAtExecution.push_back(getConnection()->isClosed());
	}
	int32_t reply = value + 1;
	getConnection()->sendPacket(makePacket({static_cast<uint8_t>(reply), 0, 0, 0})); // ignored once the connection is closed
}

std::vector<uint8_t> valueFrame(int32_t value) {
	return frame({static_cast<uint8_t>(value & 0xFF), static_cast<uint8_t>((value >> 8) & 0xFF), 0, 0});
}

struct FlowServer {
	std::shared_ptr<FlowState> state = std::make_shared<FlowState>();
	network::PacketProcessor<FlowConnection> processor{1, 1, 50, 3};
	std::mutex mutex;
	std::weak_ptr<FlowConnection> connection;
	std::unique_ptr<network::NioServer> server;
	uint16_t port = 0;

	FlowServer() {
		network::ServerCfg cfg{{"127.0.0.1", 0}, "flow clients", [this](asio::ip::tcp::socket socket, network::NioServer& nioServer) {
			auto con = std::make_shared<FlowConnection>(std::move(socket), nioServer, processor, state);
			std::lock_guard lock(mutex);
			connection = con;
			return con;
		}};
		server = std::make_unique<network::NioServer>(2, std::vector{cfg});
		server->connect();
		port = server->getBoundAddresses().at(0).port;
	}
};

} // namespace

TEST(ClientPacketFlowTest, PacketsAreReadInProcessDataAndExecutedInOrder) {
	FlowServer flow;
	TestClient client(flow.port);
	std::vector<uint8_t> data;
	for (int32_t value = 1; value <= 100; value++) {
		auto bytes = valueFrame(value * 2);
		data.insert(data.end(), bytes.begin(), bytes.end());
	}
	client.send(data); // many packets in few reads: each slice of the read buffer is read before the buffer is reused

	for (int32_t value = 1; value <= 100; value++) {
		auto reply = client.readFrame();
		ASSERT_TRUE(reply) << value;
		ASSERT_EQ((*reply)[0], static_cast<uint8_t>(value * 2 + 1)) << value;
	}
	std::lock_guard lock(flow.state->mutex);
	ASSERT_EQ(flow.state->executed.size(), 100u);
	for (int32_t i = 0; i < 100; i++)
		EXPECT_EQ(flow.state->executed[i], (i + 1) * 2);
}

TEST(ClientPacketFlowTest, QueuedPacketsKeepTheConnectionAliveAfterDisconnect) {
	FlowServer flow;
	flow.state->blocking = true;
	{
		TestClient client(flow.port);
		std::vector<uint8_t> data;
		for (int32_t value = 1; value <= 5; value++) {
			auto bytes = valueFrame(value);
			data.insert(data.end(), bytes.begin(), bytes.end());
		}
		client.send(data);
		ASSERT_TRUE(waitUntil([&] { return flow.processor.getWaitingPacketCount() == 4; }));
		client.close();
	}
	ASSERT_TRUE(waitUntil([&] { return flow.state->disconnectCount == 1; }));
	EXPECT_EQ(flow.server->getActiveConnectionCount(), 0u);

	std::weak_ptr<FlowConnection> weak;
	{
		std::lock_guard lock(flow.mutex);
		weak = flow.connection;
	}
	EXPECT_FALSE(weak.expired()); // only the queued packets reference it now
	{
		std::lock_guard lock(flow.state->mutex);
		flow.state->released = true;
	}
	flow.state->changed.notify_all();

	ASSERT_TRUE(waitUntil([&] { return flow.state->executedCount() == 5; }));
	ASSERT_TRUE(waitUntil([&] { return weak.expired(); }));
	std::lock_guard lock(flow.state->mutex);
	EXPECT_EQ(flow.state->executed, (std::vector<int32_t>{1, 2, 3, 4, 5}));
	for (size_t i = 0; i < 5; i++)
		EXPECT_TRUE(flow.state->closedAtExecution[i]) << i;
}
