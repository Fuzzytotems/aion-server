// The chat server link (ChatServer, ChatServerConnection, CsClientPacketFactory and the CS packets) against a fake chat server on loopback TCP,
// plus the connection flood filter (FloodManager) and NetFlusher.

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

#include "aion/commons/network/NioServer.h"
#include "aion/gameserver/configs/network/NetworkConfig.h"
#include "aion/gameserver/network/chatserver/ChatServer.h"
#include "aion/gameserver/network/sequrity/FloodManager.h"
#include "aion/gameserver/network/sequrity/FloodManager_ResultInfo.h"
#include "aion/gameserver/network/sequrity/NetFlusher.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "support/GameServerTestServer.h"
#include "NetworkTestSupport.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::test {
namespace {

using chatserver::ChatServer;
using configs::network::NetworkConfig;

class ChatServerLinkTest : public ::testing::Test {
protected:
	ChatServerLinkTest() : acceptor(io, asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0)) {}

	void SetUp() override {
		configureNetworkForTests();
		NetworkConfig::GAMESERVER_ID = 2;
		NetworkConfig::CHAT_PASSWORD.set("chat-password");
		NetworkConfig::CHAT_ADDRESS.set(commons::utils::InetSocketAddress{"127.0.0.1", acceptor.local_endpoint().port()});
		scope = std::make_unique<runtime::TaskScope>(AION_TASK_INFO(runtime::TaskKind::TEST)); // the test thread calls game server APIs
	}

	void TearDown() override {
		ChatServer::getInstance().disconnect();
		scope.reset();
	}

	/** never destroyed: a reconnect scheduled by ChatServer keeps a pointer to it (like the GameServer's NioServer) */
	static commons::network::NioServer& sharedGameServer() {
		static auto* server = [] {
			auto* created = new commons::network::NioServer(1, std::vector<commons::network::ServerCfg>{});
			created->connect();
			return created;
		}();
		return *server;
	}

	void send(std::span<const uint8_t> body) { cs->send(PacketWriter().H(static_cast<int32_t>(body.size() + 2)).B(body).data); }

	std::vector<uint8_t> expectPacket(uint8_t opcode) {
		auto frame = cs->readFrame();
		if (!frame) {
			ADD_FAILURE() << "expected CS packet " << int(opcode);
			return {};
		}
		std::vector<uint8_t> body(frame->begin() + 2, frame->end());
		EXPECT_EQ(body.at(0), opcode);
		return body;
	}

	asio::io_context io;
	asio::ip::tcp::acceptor acceptor;
	std::unique_ptr<TestSocket> cs;
	std::unique_ptr<runtime::TaskScope> scope;
};

TEST_F(ChatServerLinkTest, AuthenticationSetsThePublicAddress) {
	LogCapture logs({"com.aionemu.gameserver.network.chatserver"});
	ChatServer::getInstance().connect(sharedGameServer());
	cs = std::make_unique<TestSocket>(acceptor);
	EXPECT_EQ(expectPacket(0x00), (PacketWriter().C(0x00).C(2).S("chat-password").data)); // SM_CS_AUTH
	EXPECT_TRUE(logs.contains("Connected to chat server")) << logs.dump();
	EXPECT_FALSE(ChatServer::getInstance().isUp());
	EXPECT_EQ(ChatServer::getInstance().getPublicIP()->length(), 0);

	send(PacketWriter().C(0x00).C(0).C(4).C(10).C(0).C(0).C(7).H(10241).data); // CM_CS_AUTH_RESPONSE: authed, 10.0.0.7:10241
	// Java order: the state becomes AUTHED before the address is set
	ASSERT_TRUE(waitUntil([] { return ChatServer::getInstance().isUp() && ChatServer::getInstance().getPublicPort() != 0; })) << logs.dump();
	runtime::Ptr<runtime::Array<int8_t>> ip = ChatServer::getInstance().getPublicIP();
	ASSERT_EQ(ip->length(), 4);
	EXPECT_EQ(ip->get(0), 10);
	EXPECT_EQ(ip->get(3), 7);
	EXPECT_EQ(ChatServer::getInstance().getPublicPort(), 10241);

	ChatServer::getInstance().sendPlayerGagPacket(123456, 600000);
	EXPECT_EQ(expectPacket(0x03), (PacketWriter().C(0x03).D(123456).Q(600000).data)); // SM_CS_PLAYER_GAG

	send(PacketWriter().C(0x05).data);
	EXPECT_TRUE(logs.waitFor("Chat server 127.0.0.1 sent data with unknown opcode: 0x05, state=AUTHED")) << logs.dump();

	ChatServer::getInstance().disconnect();
	EXPECT_TRUE(cs->waitClosed());
	EXPECT_FALSE(ChatServer::getInstance().isUp());
	EXPECT_EQ(ChatServer::getInstance().getPublicIP()->length(), 0);
	EXPECT_EQ(ChatServer::getInstance().getPublicPort(), 0);
}

TEST_F(ChatServerLinkTest, PacketsOfOtherStatesAreIgnored) {
	LogCapture logs({"com.aionemu.gameserver.network.chatserver"});
	ChatServer::getInstance().connect(sharedGameServer());
	cs = std::make_unique<TestSocket>(acceptor);
	expectPacket(0x00);
	send(PacketWriter().C(0x01).D(1).C(0).data); // CM_CS_PLAYER_AUTH_RESPONSE before authentication
	EXPECT_TRUE(logs.waitFor("sent CM_CS_PLAYER_AUTH_RESPONSE but the connections current state (CONNECTED) is invalid for this packet."))
		<< logs.dump();
	EXPECT_THROW(ChatServer::getInstance().connect(sharedGameServer()), commons::utils::IllegalStateException);
}

// D6 fix (docs/deviations/P4-15.md): the send methods read csCon once (Java: isUp() and a second read, a NullPointerException on a link drop)
TEST_F(ChatServerLinkTest, SendingRacesWithTheLinkDrop) {
	ChatServer::getInstance().connect(sharedGameServer());
	cs = std::make_unique<TestSocket>(acceptor);
	expectPacket(0x00);
	send(PacketWriter().C(0x00).C(0).C(4).C(10).C(0).C(0).C(7).H(10241).data); // CM_CS_AUTH_RESPONSE: authed
	ASSERT_TRUE(waitUntil([] { return ChatServer::getInstance().isUp(); }));

	std::atomic<bool> stop{false};
	std::atomic<int64_t> calls{0};
	std::vector<std::thread> senders;
	for (int i = 0; i < 4; i++) {
		senders.emplace_back([&] {
			runtime::TaskScope senderScope(AION_TASK_INFO(runtime::TaskKind::TEST));
			while (!stop.load()) {
				ChatServer::getInstance().sendPlayerGagPacket(1, 1);
				calls.fetch_add(1);
			}
		});
	}
	EXPECT_TRUE(waitUntil([&] { return calls.load() > 100; }));
	ChatServer::getInstance().disconnect(); // csCon becomes null while the senders load it
	const int64_t atDisconnect = calls.load();
	EXPECT_TRUE(waitUntil([&] { return calls.load() > atDisconnect + 1000; }));
	stop = true;
	for (std::thread& sender : senders)
		sender.join();
	EXPECT_FALSE(ChatServer::getInstance().isUp());
}

TEST(FloodManagerTest, CountsEventsPerKeyAndTick) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	using Result = sequrity::FloodManager::Result;
	// one tick per hour: every call of this test falls into the same tick (unless it happens to cross an hour boundary)
	runtime::Ref<sequrity::FloodManager> manager = sequrity::FloodManager::create(3600000,
		{sequrity::FloodManager::FloodFilter::create(2, 4, 1), sequrity::FloodManager::FloodFilter::create(10, 20, 3)});

	EXPECT_EQ(manager->isFlooding("", true), Result::REJECTED); // Java: null or empty key
	EXPECT_EQ(manager->isFlooding("1.2.3.4", true), Result::ACCEPTED); // 1
	EXPECT_EQ(manager->isFlooding("1.2.3.4", true), Result::ACCEPTED); // 2 (warn limit 2 is not exceeded)
	EXPECT_EQ(manager->isFlooding("1.2.3.4", true), Result::WARNED);   // 3
	EXPECT_EQ(manager->isFlooding("1.2.3.4", false), Result::WARNED);  // still 3
	EXPECT_EQ(manager->isFlooding("1.2.3.4", true), Result::WARNED);   // 4 (reject limit 4 is not exceeded)
	EXPECT_EQ(manager->isFlooding("1.2.3.4", true), Result::REJECTED); // 5
	EXPECT_EQ(manager->isFlooding("5.6.7.8", true), Result::ACCEPTED); // other keys have their own counts

	EXPECT_EQ(sequrity::max(Result::WARNED, Result::ACCEPTED), Result::WARNED);
	EXPECT_EQ(sequrity::max(Result::WARNED, Result::REJECTED), Result::REJECTED);
}

TEST(NetFlusherTest, RunsTheTaskPeriodically) {
	auto runs = std::make_shared<std::atomic<int>>(0);
	sequrity::NetFlusher::add(runtime::PinnedCallback<void()>(runtime::Pin(), [runs] { runs->fetch_add(1); }), 20);
	EXPECT_TRUE(waitUntil([&] { return runs->load() >= 3; }));
}

} // namespace
} // namespace aion::gameserver::network::test
