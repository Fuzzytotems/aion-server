// What keeps a client's ChatClient and connection (ClientChannelHandler) alive, and when they are released: ChatService and BroadcastService
// hold the ChatClient until the game server logs the player out, the ChatClient holds the connection, and the connection holds the ChatClient
// until its disconnect event (the last event of the channel) drops it. Also the shutdown with clients still connected.

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include "ChatServerTestFixture.h"
#include "aion/chatserver/model/ChatClient.h"
#include "aion/chatserver/model/Race.h"
#include "aion/chatserver/network/netty/handler/ClientChannelHandler.h"
#include "aion/chatserver/network/netty/pipeline/ExecutionHandler.h"
#include "aion/chatserver/network/netty/pipeline/LoginToClientPipeLineFactory.h"
#include "aion/chatserver/service/ChatService.h"
#include "support/FakePeers.h"
#include "support/TestUtils.h"

namespace aion::chatserver::test {
namespace {

using model::ChatClient;
using network::netty::handler::ClientChannelHandler;

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
	std::weak_ptr<ClientChannelHandler> weakHandler = chatClient->getChannelHandler();
	chatClient.reset();

	gs->send(FakeGameServer::buildPlayerLogout(playerId));
	EXPECT_TRUE(client.socket.waitClosed());
	EXPECT_TRUE(waitUntil([&] { return weakClient.expired(); }));
	EXPECT_TRUE(waitUntil([&] { return weakHandler.expired(); }));
}

TEST_F(LifecycleTest, AClientDisconnectingWithoutLogoutIsReleasedAtTheLogout) {
	LogCapture log({"com.aionemu.chatserver"});
	auto gs = connectGameServer();
	int32_t playerId = nextPlayerId();
	std::shared_ptr<ChatClient> chatClient = registerPlayer(playerId, "Mia");
	FakeChatClient client(clientPort);
	client.login(playerId, "Mia@AION", "acc", chatClient->getToken());
	std::weak_ptr<ChatClient> weakClient = chatClient;
	std::weak_ptr<ClientChannelHandler> weakHandler = chatClient->getChannelHandler();
	chatClient.reset();

	client.socket.close();
	ASSERT_TRUE(log.waitFor("Channel disconnected IP: 127.0.0.1")) << log.dump();
	// the player is still registered (the game server did not log it out), so the ChatClient and its closed connection are kept until the logout
	gs->send(FakeGameServer::buildPlayerLogout(playerId));
	EXPECT_TRUE(log.waitFor("Player[id=" + std::to_string(playerId) + "] logged out ")) << log.dump();
	EXPECT_TRUE(waitUntil([&] { return weakClient.expired(); }));
	EXPECT_TRUE(waitUntil([&] { return weakHandler.expired(); }));
}

TEST_F(LifecycleTest, ALoginReadBeforeTheDisconnectRunsBeforeTheDisconnectEvent) {
	// the disconnect event runs after the events of the data received before it: a login still queued when the client disconnects runs first,
	// and the disconnect event then drops the connection's reference to the ChatClient again
	LogCapture log({"com.aionemu.chatserver"});
	auto gs = connectGameServer();
	int32_t playerId = nextPlayerId();
	std::shared_ptr<ChatClient> chatClient = registerPlayer(playerId, "Nia");
	FakeChatClient client(clientPort);
	ASSERT_TRUE(log.waitFor("Channel connected Ip: 127.0.0.1")) << log.dump();

	// occupy every thread of the client events, so the client's next events stay queued
	std::shared_ptr<network::netty::pipeline::ExecutionHandler> executor = server->getExecutionHandler();
	auto release = std::make_shared<std::atomic<bool>>(false);
	auto busy = std::make_shared<std::atomic<int32_t>>(0);
	int blockers[network::netty::pipeline::LoginToClientPipeLineFactory::THREADS_MAX] = {};
	for (int& blocker : blockers) {
		executor->execute(&blocker, [release, busy] {
			(*busy)++;
			waitUntil([&] { return release->load(); }, 10s);
		});
	}
	ASSERT_TRUE(waitUntil([&] { return busy->load() == network::netty::pipeline::LoginToClientPipeLineFactory::THREADS_MAX; }));
	client.send(FakeChatClient::buildPlayerAuth(playerId, "Nia@AION", "acc", chatClient->getToken()));
	client.socket.close();
	std::this_thread::sleep_for(500ms); // the server reads the login and the end of the stream meanwhile
	release->store(true);
	ASSERT_TRUE(log.waitFor("Channel disconnected IP: 127.0.0.1")) << log.dump();

	waitUntil([&] { return chatClient->getChannelHandler() != nullptr; }); // set by the login
	std::weak_ptr<ChatClient> weakClient = chatClient;
	std::weak_ptr<ClientChannelHandler> weakHandler = chatClient->getChannelHandler();
	chatClient.reset();
	gs->send(FakeGameServer::buildPlayerLogout(playerId));
	EXPECT_TRUE(log.waitFor("Player[id=" + std::to_string(playerId) + "] logged out ")) << log.dump();
	EXPECT_TRUE(waitUntil([&] { return weakClient.expired(); }));
	EXPECT_TRUE(waitUntil([&] { return weakHandler.expired(); }));
}

TEST_F(LifecycleTest, ShutdownClosesConnectedClientsAndRunsTheirDisconnectEvents) {
	// the disconnect events of the clients the shutdown closes are run, also when they have to wait for busy event threads (up to 5 s)
	LogCapture log({"com.aionemu.chatserver", "org.jboss.netty.handler.execution.ExecutionHandler"});
	auto gs = connectGameServer();
	Player alice = loginPlayer(*gs, "Alice", 0);
	Player bob = loginPlayer(*gs, "Bob", 0);
	std::string identifier = channelIdentifier("public", uniqueName("map"), GS_ID, 0);
	alice.client->joinChannel(1, identifier);
	bob.client->joinChannel(1, identifier);

	std::shared_ptr<network::netty::pipeline::ExecutionHandler> executor = server->getExecutionHandler();
	auto busy = std::make_shared<std::atomic<int32_t>>(0);
	int blockers[network::netty::pipeline::LoginToClientPipeLineFactory::THREADS_MAX] = {};
	for (int& blocker : blockers) {
		executor->execute(&blocker, [busy] {
			(*busy)++;
			std::this_thread::sleep_for(300ms);
		});
	}
	waitUntil([&] { return busy->load() == network::netty::pipeline::LoginToClientPipeLineFactory::THREADS_MAX; });
	server->shutdownAll();
	EXPECT_EQ(log.count("Channel disconnected IP: 127.0.0.1"), 2) << log.dump();
	EXPECT_FALSE(log.contains("Discarded")) << log.dump();
}

} // namespace
} // namespace aion::chatserver::test
