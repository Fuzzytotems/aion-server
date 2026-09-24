// ClientChannelHandler on a commons NioServer of the test's own, created as a subclass that acts inside processData (on the connection's strand,
// where no write of the connection can run meanwhile): what sendPacket queues, and that frames received after close() are dropped.

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "aion/chatserver/network/aion/ClientPacketHandler.h"
#include "aion/chatserver/network/aion/serverpackets/SM_CHAT_INI.h"
#include "aion/chatserver/network/netty/handler/ClientChannelHandler.h"
#include "aion/chatserver/network/netty/pipeline/ExecutionHandler.h"
#include "aion/commons/network/NioServer.h"
#include "aion/commons/network/ServerCfg.h"
#include "support/FakePeers.h"
#include "support/TestUtils.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::chatserver::test {
namespace {

using commons::network::NioServer;
using commons::network::ServerCfg;
using network::aion::ClientPacketHandler;
using network::netty::handler::ClientChannelHandler;
using network::netty::pipeline::ExecutionHandler;

/** What the probe does with the received frames, and what it saw. */
struct ProbeState {
	/** send SM_CHAT_INI before passing each frame on, and record the capacities of the queued buffers */
	std::atomic<bool> sendChatIni = false;
	/** close() after passing the first frame on */
	std::atomic<bool> closeAfterFirstFrame = false;
	std::mutex mutex;
	std::vector<int32_t> queuedCapacities;
};

class ProbeHandler : public ClientChannelHandler {
public:
	ProbeHandler(asio::ip::tcp::socket socket, NioServer& server, std::shared_ptr<const ClientPacketHandler> clientPacketHandler,
		std::shared_ptr<ExecutionHandler> executionHandler, std::shared_ptr<ProbeState> state)
		: ClientChannelHandler(std::move(socket), server, std::move(clientPacketHandler), std::move(executionHandler)), state(std::move(state)) {}

protected:
	bool processData(commons::utils::ByteBuffer& data) override {
		if (state->sendChatIni) {
			sendPacket(network::aion::serverpackets::SM_CHAT_INI());
			std::lock_guard lock(guard);
			std::lock_guard stateLock(state->mutex);
			for (const auto& packet : sendMsgQueue)
				state->queuedCapacities.push_back(packet->capacity());
		}
		bool result = ClientChannelHandler::processData(data);
		if (state->closeAfterFirstFrame.exchange(false))
			close();
		return result;
	}

private:
	const std::shared_ptr<ProbeState> state;
};

class ClientChannelHandlerTest : public ::testing::Test {
protected:
	void SetUp() override {
		auto clientPacketHandler = std::make_shared<const ClientPacketHandler>();
		ServerCfg cfg{{"127.0.0.1", 0}, "probe clients", [this, clientPacketHandler](asio::ip::tcp::socket socket, NioServer& nioServer) {
			return std::make_shared<ProbeHandler>(std::move(socket), nioServer, clientPacketHandler, executionHandler, state);
		}};
		server = std::make_unique<NioServer>(1, std::vector<ServerCfg>{std::move(cfg)}, 1);
		server->connect();
		port = server->getBoundAddresses().at(0).port;
	}

	void TearDown() override {
		server->shutdown();
		executionHandler->shutdown(5s);
	}

	const std::shared_ptr<ProbeState> state = std::make_shared<ProbeState>();
	const std::shared_ptr<ExecutionHandler> executionHandler = std::make_shared<ExecutionHandler>(2);
	std::unique_ptr<NioServer> server;
	uint16_t port = 0;
};

TEST_F(ClientChannelHandlerTest, SendPacketQueuesOnlyTheEncodedBytes) {
	// the packet is written into a 16 KiB buffer, but only its 10 bytes stay queued until the socket takes them
	state->sendChatIni = true;
	FakeChatClient client(port);
	client.send(FakeChatClient::buildPing());
	EXPECT_EQ(client.expectFrame("SM_CHAT_INI"), SM_CHAT_INI_BYTES);
	std::lock_guard lock(state->mutex);
	EXPECT_EQ(state->queuedCapacities, std::vector<int32_t>{10});
}

TEST_F(ClientChannelHandlerTest, FramesReceivedAfterCloseAreDropped) {
	// Netty closes the socket in close(): of two frames read at once, the one after the close() never reaches the pipeline
	LogCapture log({"com.aionemu.chatserver"});
	state->closeAfterFirstFrame = true;
	FakeChatClient client(port);
	Bytes frames = FakeChatClient::buildPing(); // before the login both are unknown packets, which are logged
	Bytes request = FakeChatClient::buildChannelRequest(1, "x");
	frames.insert(frames.end(), request.begin(), request.end());
	client.send(frames);
	client.socket.waitClosed();
	ASSERT_TRUE(log.waitFor("Channel disconnected IP: 127.0.0.1")) << log.dump(); // the channel's last event
	EXPECT_TRUE(log.contains("Unknown packet received from client: opCode=0xFF state=CONNECTED")) << log.dump();
	EXPECT_FALSE(log.contains("opCode=0x10")) << log.dump();
}

} // namespace
} // namespace aion::chatserver::test
