// The limits of the chat server's connections (upstream 459b35094, Java: ConnectionHandler and NettyConnection): the authentication timeout of
// the game clients and the maximum number of packets of a connection that may wait for execution.

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include <gtest/gtest.h>

#include "ChatServerTestFixture.h"
#include "aion/chatserver/configs/network/NetworkConfig.h"
#include "aion/chatserver/network/NetConnector.h"
#include "aion/chatserver/network/netty/PacketExecutorGroup.h"
#include "support/FakePeers.h"
#include "support/TestUtils.h"

namespace aion::chatserver::test {
namespace {

using configs::network::NetworkConfig;

class ConnectionHandlerTest : public ChatServerTestFixture {
protected:
	void TearDown() override {
		NetworkConfig::CLIENT_AUTH_TIMEOUT_SECONDS = 0;
		ChatServerTestFixture::TearDown();
	}
};

TEST_F(ConnectionHandlerTest, ClientsThatDoNotAuthenticateInTimeAreDisconnected) {
	NetworkConfig::CLIENT_AUTH_TIMEOUT_SECONDS = 1;
	startServer();
	LogCapture log({"com.aionemu.chatserver.network.netty.ConnectionHandler"});
	FakeChatClient client(clientPort);
	client.send(FakeChatClient::buildChatIni()); // answered, but no authentication
	EXPECT_EQ(client.expectFrame("SM_CHAT_INI"), SM_CHAT_INI_BYTES);
	auto started = std::chrono::steady_clock::now();
	EXPECT_TRUE(client.socket.waitClosed(5s));
	EXPECT_GE(std::chrono::steady_clock::now() - started, 500ms);
	EXPECT_TRUE(log.waitFor("info|com.aionemu.chatserver.network.netty.ConnectionHandler|Client 127.0.0.1 didn't authenticate within 1000 ms, "
													"disconnecting"))
		<< log.dump();
}

TEST_F(ConnectionHandlerTest, AuthenticatedClientsStayConnected) {
	NetworkConfig::CLIENT_AUTH_TIMEOUT_SECONDS = 1;
	startServer();
	auto gs = connectGameServer();
	Player player = loginPlayer(*gs, "Stay", 0);
	EXPECT_TRUE(player.client->socket.expectSilence(1500ms));
}

TEST_F(ConnectionHandlerTest, ClientsWithTooManyWaitingPacketsAreDisconnected) {
	LogCapture log({"com.aionemu.chatserver.network.netty.ConnectionHandler"});
	// occupy every packet executor thread, so the client's packets stay queued
	std::shared_ptr<network::netty::PacketExecutorGroup> executor = network::NetConnector::getPacketExecutors();
	auto release = std::make_shared<std::atomic<bool>>(false);
	auto busy = std::make_shared<std::atomic<int32_t>>(0);
	int blockers[8] = {};
	for (int& blocker : blockers) {
		executor->execute(&blocker, [release, busy] {
			(*busy)++;
			waitUntil([&] { return release->load(); }, 10s);
		});
	}
	ASSERT_TRUE(waitUntil([&] { return busy->load() == 8; }));

	FakeChatClient client(clientPort);
	Bytes packets;
	for (int i = 0; i < 101; i++) { // AionConnection.getMaxPendingPackets() is 100
		Bytes ini = FakeChatClient::buildChatIni();
		packets.insert(packets.end(), ini.begin(), ini.end());
	}
	client.send(packets);
	EXPECT_TRUE(log.waitFor("Client 127.0.0.1 has more than 100 packets waiting for execution, disconnecting")) << log.dump();
	release->store(true);
	EXPECT_TRUE(client.socket.waitClosed());
}

} // namespace
} // namespace aion::chatserver::test
