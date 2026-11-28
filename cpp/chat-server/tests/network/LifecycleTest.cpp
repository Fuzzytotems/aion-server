// What keeps a client's ChatClient and connection (AionConnection) alive, and when they are released: ChatService and BroadcastService
// hold the ChatClient until the game server logs the player out, the ChatClient holds the connection, and the connection holds the ChatClient
// until its disconnect callback (the last task of the connection) drops it. Also the shutdown with clients still connected.

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include "ChatServerTestFixture.h"
#include "aion/chatserver/model/ChatClient.h"
#include "aion/chatserver/model/Race.h"
#include "aion/chatserver/network/NetConnector.h"
#include "aion/chatserver/network/aion/AionConnection.h"
#include "aion/chatserver/network/netty/PacketExecutorGroup.h"
#include "aion/chatserver/service/ChatService.h"
#include "support/FakePeers.h"
#include "support/TestUtils.h"

namespace aion::chatserver::test {
namespace {

using model::ChatClient;
using network::aion::AionConnection;

/** the threads of NetConnector's packet executors */
constexpr int PACKET_EXECUTOR_THREADS = 8;

class LifecycleTest : public ChatServerTestFixture {
protected:
	/** Registers the player directly at the ChatService (like the game server's CM_PLAYER_AUTH does), so the test holds its ChatClient. */
	static std::shared_ptr<ChatClient> registerPlayer(int32_t playerId, std::string_view name) {
		return service::ChatService::getInstance().registerPlayer(playerId, "acc", name, model::Race::ELYOS, 0);
	}
};

TEST_F(LifecycleTest, LogoutReleasesTheChatClientAndItsConnection) {
	auto gs = connectGameServer();
	int32_t playerId = nextPlayerId();
	std::shared_ptr<ChatClient> chatClient = registerPlayer(playerId, "Lou");
	FakeChatClient client(clientPort);
	client.login(playerId, "Lou@AION", "acc", chatClient->getToken());
	client.joinChannel(1, channelIdentifier("public", uniqueName("map"), GS_ID, 0));
	std::weak_ptr<ChatClient> weakClient = chatClient;
	std::weak_ptr<AionConnection> weakConnection = chatClient->getConnection();
	chatClient.reset();

	gs->send(FakeGameServer::buildPlayerLogout(playerId));
	EXPECT_TRUE(client.socket.waitClosed());
	EXPECT_TRUE(waitUntil([&] { return weakClient.expired(); }));
	EXPECT_TRUE(waitUntil([&] { return weakConnection.expired(); }));
}

TEST_F(LifecycleTest, AClientDisconnectingWithoutLogoutIsReleasedAtTheLogout) {
	LogCapture log({"com.aionemu.chatserver"});
	auto gs = connectGameServer();
	int32_t playerId = nextPlayerId();
	std::shared_ptr<ChatClient> chatClient = registerPlayer(playerId, "Mia");
	FakeChatClient client(clientPort);
	client.login(playerId, "Mia@AION", "acc", chatClient->getToken());
	std::weak_ptr<ChatClient> weakClient = chatClient;
	std::weak_ptr<AionConnection> weakConnection = chatClient->getConnection();
	chatClient.reset();

	client.socket.close();
	ASSERT_TRUE(log.waitFor("Client disconnected: ")) << log.dump();
	// the player is still registered (the game server did not log it out), so the ChatClient and its closed connection are kept until the logout
	gs->send(FakeGameServer::buildPlayerLogout(playerId));
	EXPECT_TRUE(log.waitFor("Player[id=" + std::to_string(playerId) + "] logged out ")) << log.dump();
	EXPECT_TRUE(waitUntil([&] { return weakClient.expired(); }));
	EXPECT_TRUE(waitUntil([&] { return weakConnection.expired(); }));
}

TEST_F(LifecycleTest, ALoginReadBeforeTheDisconnectRunsBeforeTheDisconnectCallback) {
	// the disconnect callback runs after the packets received before it: a login still queued when the client disconnects runs first,
	// and the disconnect callback then drops the connection's reference to the ChatClient again
	LogCapture log({"com.aionemu.chatserver"});
	auto gs = connectGameServer();
	int32_t playerId = nextPlayerId();
	std::shared_ptr<ChatClient> chatClient = registerPlayer(playerId, "Nia");
	FakeChatClient client(clientPort);
	ASSERT_TRUE(log.waitFor("Client connected: 127.0.0.1")) << log.dump();

	// occupy every packet executor thread, so the client's next packets stay queued
	std::shared_ptr<network::netty::PacketExecutorGroup> executor = network::NetConnector::getPacketExecutors();
	auto release = std::make_shared<std::atomic<bool>>(false);
	auto busy = std::make_shared<std::atomic<int32_t>>(0);
	int blockers[PACKET_EXECUTOR_THREADS] = {};
	for (int& blocker : blockers) {
		executor->execute(&blocker, [release, busy] {
			(*busy)++;
			waitUntil([&] { return release->load(); }, 10s);
		});
	}
	ASSERT_TRUE(waitUntil([&] { return busy->load() == PACKET_EXECUTOR_THREADS; }));
	client.send(FakeChatClient::buildPlayerAuth(playerId, "Nia@AION", "acc", chatClient->getToken()));
	client.socket.close();
	std::this_thread::sleep_for(500ms); // the server reads the login and the end of the stream meanwhile
	release->store(true);
	ASSERT_TRUE(log.waitFor("Client disconnected: ")) << log.dump();

	waitUntil([&] { return chatClient->getConnection() != nullptr; }); // set by the login
	std::weak_ptr<ChatClient> weakClient = chatClient;
	std::weak_ptr<AionConnection> weakConnection = chatClient->getConnection();
	chatClient.reset();
	gs->send(FakeGameServer::buildPlayerLogout(playerId));
	EXPECT_TRUE(log.waitFor("Player[id=" + std::to_string(playerId) + "] logged out ")) << log.dump();
	EXPECT_TRUE(waitUntil([&] { return weakClient.expired(); }));
	EXPECT_TRUE(waitUntil([&] { return weakConnection.expired(); }));
}

TEST_F(LifecycleTest, ShutdownClosesConnectedClientsAndRunsTheirDisconnectCallbacks) {
	// the disconnect callbacks of the clients the shutdown closes are run, also when they have to wait for busy executor threads (up to 3 s)
	LogCapture log({"com.aionemu.chatserver"});
	auto gs = connectGameServer();
	Player alice = loginPlayer(*gs, "Alice", 0);
	Player bob = loginPlayer(*gs, "Bob", 0);
	std::string identifier = channelIdentifier("public", uniqueName("map"), GS_ID, 0);
	alice.client->joinChannel(1, identifier);
	bob.client->joinChannel(1, identifier);

	std::shared_ptr<network::netty::PacketExecutorGroup> executor = network::NetConnector::getPacketExecutors();
	auto busy = std::make_shared<std::atomic<int32_t>>(0);
	int blockers[PACKET_EXECUTOR_THREADS] = {};
	for (int& blocker : blockers) {
		executor->execute(&blocker, [busy] {
			(*busy)++;
			std::this_thread::sleep_for(300ms);
		});
	}
	waitUntil([&] { return busy->load() == PACKET_EXECUTOR_THREADS; });
	network::NetConnector::shutdown();
	EXPECT_EQ(log.count("Client disconnected: "), 2) << log.dump();
	EXPECT_FALSE(log.contains("Discarded")) << log.dump();
}

} // namespace
} // namespace aion::chatserver::test
