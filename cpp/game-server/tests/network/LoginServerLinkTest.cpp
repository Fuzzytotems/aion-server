// The game server's login server link (LoginServer, LoginServerConnection, LsClientPacketFactory and the LS packets) against a fake login server
// on loopback TCP that speaks the plain LS <-> GS protocol ([u16 length][u8 opcode][data]); the expected bytes follow the Java writeImpl/readImpl
// sequences and the C++ login server's GS protocol tests (login-server/tests/server/GameServerFlowTest.cpp).
// C++ only: the test against the in-process C++ login server needs the login server library linked into this test executable (header request).

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

#include "aion/commons/network/NioServer.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/network/NetworkConfig.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/network/BannedMacManager.h"
#include "aion/gameserver/network/loginserver/LoginServer.h"
#include "aion/gameserver/network/loginserver/serverpackets/SM_ACCOUNT_AUTH.h"
#include "aion/gameserver/network/loginserver/serverpackets/SM_ACCOUNT_CONNECTION_INFO.h"
#include "aion/gameserver/network/loginserver/serverpackets/SM_ACCOUNT_DISCONNECTED.h"
#include "aion/gameserver/network/loginserver/serverpackets/SM_ACCOUNT_RECONNECT_KEY.h"
#include "aion/gameserver/network/loginserver/serverpackets/SM_BAN.h"
#include "aion/gameserver/network/loginserver/serverpackets/SM_GS_CHARACTER.h"
#include "aion/gameserver/network/loginserver/serverpackets/SM_HDDBAN_CONTROL.h"
#include "aion/gameserver/network/loginserver/serverpackets/SM_PTRANSFER_CONTROL.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "FakeGameClient.h"
#include "support/GameServerTestServer.h"
#include "NetworkTestSupport.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::test {
namespace {

using configs::network::NetworkConfig;
using loginserver::LoginServer;

const char* LS_LOGGER = "com.aionemu.gameserver.network.loginserver";

/** A fake login server: accepts the game server's link and exchanges plain frames */
class FakeLoginServer {
public:
	FakeLoginServer() : acceptor(io, asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0)) {}

	uint16_t port() const { return acceptor.local_endpoint().port(); }

	void accept() { socket = std::make_unique<TestSocket>(acceptor); }

	void send(std::span<const uint8_t> body) {
		std::vector<uint8_t> frame = PacketWriter().H(static_cast<int32_t>(body.size() + 2)).B(body).data;
		socket->send(frame);
	}

	/** @return the next packet body (opcode + data), std::nullopt on timeout or close */
	std::optional<std::vector<uint8_t>> readPacket(std::chrono::milliseconds timeout = 5s) {
		auto frame = socket->readFrame(timeout);
		if (!frame)
			return std::nullopt;
		return std::vector<uint8_t>(frame->begin() + 2, frame->end());
	}

	std::vector<uint8_t> expectPacket(uint8_t opcode) {
		auto packet = readPacket();
		if (!packet) {
			ADD_FAILURE() << "expected GS packet " << int(opcode) << " but got none";
			return {};
		}
		EXPECT_EQ((*packet)[0], opcode);
		return *packet;
	}

	asio::io_context io;
	asio::ip::tcp::acceptor acceptor;
	std::unique_ptr<TestSocket> socket;
};

class LoginServerLinkTest : public ::testing::Test {
protected:
	void SetUp() override {
		configureNetworkForTests();
		NetworkConfig::GAMESERVER_ID = 1;
		NetworkConfig::LOGIN_PASSWORD.set("gs-password");
		NetworkConfig::CLIENT_CONNECT_ADDRESS.set(commons::utils::InetSocketAddress{"127.0.0.1", 7777});
		NetworkConfig::MIN_ACCESS_LEVEL = 3;
		NetworkConfig::MAX_ONLINE_PLAYERS = 1000;
		NetworkConfig::LOGIN_ADDRESS.set(commons::utils::InetSocketAddress{"127.0.0.1", ls.port()});
		gameServer = &sharedGameServer();
		scope = std::make_unique<runtime::TaskScope>(AION_TASK_INFO(runtime::TaskKind::TEST)); // the test thread calls game server APIs
	}

	void TearDown() override {
		LoginServer::getInstance().disconnect();
		scope.reset();
	}

	/**
	 * The game server's NioServer for the link, never destroyed: like Java's, a reconnect scheduled by LoginServer keeps a pointer to it (the
	 * GameServer owns it for the whole run).
	 */
	static commons::network::NioServer& sharedGameServer() {
		static auto* server = [] {
			auto* created = new commons::network::NioServer(1, std::vector<commons::network::ServerCfg>{});
			created->connect();
			return created;
		}();
		return *server;
	}

	/** connect + SM_GS_AUTH + CM_GS_AUTH_RESPONSE(AUTHED, 2 servers) + SM_ACCOUNT_LIST */
	void connectAndAuthenticate() {
		LoginServer::getInstance().connect(*gameServer);
		ls.accept();
		EXPECT_EQ(ls.expectPacket(0x00),
			(PacketWriter().C(0x00).C(1).S("gs-password").C(4).C(127).C(0).C(0).C(1).H(7777).C(3).D(1000).data)); // SM_GS_AUTH
		EXPECT_FALSE(LoginServer::getInstance().isUp());
		ls.send(PacketWriter().C(0x00).C(0).C(2).data); // CM_GS_AUTH_RESPONSE: authed, 2 game servers
		EXPECT_EQ(ls.expectPacket(0x04), (PacketWriter().C(0x04).D(0).data)); // SM_ACCOUNT_LIST without logged in accounts
		EXPECT_TRUE(waitUntil([] { return LoginServer::getInstance().isUp(); }));
		EXPECT_EQ(LoginServer::getInstance().getGameServerCount(), 2);
	}

	FakeLoginServer ls;
	commons::network::NioServer* gameServer = nullptr;
	std::unique_ptr<runtime::TaskScope> scope;
};

TEST_F(LoginServerLinkTest, AuthenticationPingAndServerPackets) {
	LogCapture logs({LS_LOGGER});
	connectAndAuthenticate();
	EXPECT_TRUE(logs.contains("Connected to login server")) << logs.dump();

	ls.send(PacketWriter().C(0x0B).data); // CM_LS_PING
	EXPECT_EQ(ls.expectPacket(12), (PacketWriter().C(12).data)); // SM_LS_PONG

	LoginServer& loginServer = LoginServer::getInstance();
	EXPECT_TRUE(loginServer.sendPacket(loginserver::serverpackets::SM_ACCOUNT_DISCONNECTED(123)));
	EXPECT_EQ(ls.expectPacket(0x03), (PacketWriter().C(0x03).D(123).data));
	loginServer.sendBanPacket(3, 55, "10.0.*.*", -1, 777);
	EXPECT_EQ(ls.expectPacket(0x06), (PacketWriter().C(0x06).C(3).D(55).S("10.0.*.*").D(-1).D(777).data));
	loginServer.sendPacket(loginserver::serverpackets::SM_ACCOUNT_AUTH(7, 11, 22, 33));
	EXPECT_EQ(ls.expectPacket(0x01), (PacketWriter().C(0x01).D(7).D(11).D(22).D(33).data));
	loginServer.sendPacket(loginserver::serverpackets::SM_ACCOUNT_RECONNECT_KEY(8));
	EXPECT_EQ(ls.expectPacket(0x02), (PacketWriter().C(0x02).D(8).data));
	loginServer.sendPacket(loginserver::serverpackets::SM_ACCOUNT_CONNECTION_INFO(9, 1700000000123LL, "1.2.3.4", "AA-BB-CC-DD-EE-FF", "hdd"));
	EXPECT_EQ(ls.expectPacket(7), (PacketWriter().C(7).D(9).Q(1700000000123LL).S("1.2.3.4").S("AA-BB-CC-DD-EE-FF").S("hdd").data));
	loginServer.sendPacket(loginserver::serverpackets::SM_GS_CHARACTER(10, 300)); // writeC truncates like Java
	EXPECT_EQ(ls.expectPacket(0x08), (PacketWriter().C(0x08).D(10).C(300).data));
	loginServer.sendPacket(loginserver::serverpackets::SM_HDDBAN_CONTROL(services::ban::BanAction::BAN, "serial", 42));
	EXPECT_EQ(ls.expectPacket(10), (PacketWriter().C(10).C(1).S("serial").Q(42).data));
	loginServer.sendPacket(loginserver::serverpackets::SM_PTRANSFER_CONTROL(loginserver::serverpackets::SM_PTRANSFER_CONTROL::ERROR_, 5, "failed"));
	EXPECT_EQ(ls.expectPacket(13), (PacketWriter().C(13).C(2).D(5).S("failed").data));
	loginServer.sendPacket(loginserver::serverpackets::SM_PTRANSFER_CONTROL(loginserver::serverpackets::SM_PTRANSFER_CONTROL::OK, 6));
	EXPECT_EQ(ls.expectPacket(13), (PacketWriter().C(13).C(3).D(6).data));

	// unknown opcode and a packet that is invalid once authenticated
	ls.send(PacketWriter().C(0x07).D(1).data);
	EXPECT_TRUE(logs.waitFor("Login server 127.0.0.1 sent data with unknown opcode: 0x07, state=AUTHED")) << logs.dump();
	ls.send(PacketWriter().C(0x00).C(0).C(2).data);
	EXPECT_TRUE(logs.waitFor("Login server 127.0.0.1 sent CM_GS_AUTH_RESPONSE but the connections current state (AUTHED) is invalid for this packet."))
		<< logs.dump();

	// kicking an account that is not logged in does nothing
	ls.send(PacketWriter().C(0x02).D(424242).C(1).data);
	ls.send(PacketWriter().C(0x0B).data);
	EXPECT_EQ(ls.expectPacket(12), (PacketWriter().C(12).data)); // the kick ran before the ping (receive order)
	EXPECT_FALSE(logs.contains("Kicking account ID 424242"));
}

TEST_F(LoginServerLinkTest, BanListsAreLoadedWhileReading) {
	LogCapture logs({"com.aionemu.gameserver.network"});
	connectAndAuthenticate();
	const int64_t future = commons::utils::currentTimeMillis() + 3600000;
	ls.send(PacketWriter().C(0x09).D(2).S("00-11-22-33-44-55").Q(future).S("cheater").S("66-77-88-99-AA-BB").Q(1000).S("expired").data);
	EXPECT_TRUE(logs.waitFor("Loaded 2 banned mac addresses")) << logs.dump();
	EXPECT_TRUE(BannedMacManager::getInstance().isBanned("00-11-22-33-44-55"));
	EXPECT_FALSE(BannedMacManager::getInstance().isBanned("66-77-88-99-AA-BB"));

	// banning sends SM_MACBAN_CONTROL to the login server
	BannedMacManager::getInstance().banAddress("01-02-03-04-05-06", future, "manual");
	EXPECT_EQ(ls.expectPacket(9), (PacketWriter().C(9).C(1).S("01-02-03-04-05-06").S("manual").Q(future).data));
	EXPECT_TRUE(BannedMacManager::getInstance().isBanned("01-02-03-04-05-06"));
	EXPECT_TRUE(logs.contains("banned 01-02-03-04-05-06 to ")) << logs.dump();
	EXPECT_TRUE(BannedMacManager::getInstance().unbanAddress("01-02-03-04-05-06", "revoked"));
	EXPECT_EQ(ls.expectPacket(9), (PacketWriter().C(9).C(0).S("01-02-03-04-05-06").S("revoked").Q(0).data));
	EXPECT_FALSE(BannedMacManager::getInstance().unbanAddress("01-02-03-04-05-06", "again"));
}

TEST_F(LoginServerLinkTest, SendingWhileDownFails) {
	EXPECT_FALSE(LoginServer::getInstance().isUp());
	EXPECT_FALSE(LoginServer::getInstance().sendPacket(loginserver::serverpackets::SM_ACCOUNT_DISCONNECTED(1)));
	EXPECT_FALSE(BannedMacManager::getInstance().unbanAddress("never-banned", "x"));
}

TEST_F(LoginServerLinkTest, ClientDisconnectNotifiesTheLoginServer) {
	LogCapture logs({"com.aionemu.gameserver.network"});
	connectAndAuthenticate();
	GameServerTestServer clients;
	{
		auto client = std::make_unique<FakeGameClient>(clients.port);
		client->readKey();
		std::shared_ptr<TestAionConnection> connection = clients.connection();
		ASSERT_TRUE(connection);
		connection->setAccount(*model::account::Account::create(77));
		client.reset(); // the client closes its socket: AionConnection::onDisconnect -> LoginServer::onDisconnect
	}
	// Java LoginServer.onDisconnect: loggedInAccounts.remove(id), sendPacket(new SM_ACCOUNT_DISCONNECTED(id))
	EXPECT_EQ(ls.expectPacket(0x03), (PacketWriter().C(0x03).D(77).data));
	EXPECT_TRUE(logs.waitFor("Client disconnected: Account [id=77, name=")) << logs.dump();
}

/** An LS packet whose writeImpl blocks until released (a slow writeImpl, like SM_PTRANSFER_CONTROL's database loads) */
class BlockingLsPacket : public loginserver::LsServerPacket {
public:
	BlockingLsPacket(std::shared_ptr<std::atomic<int>> stage) : LsServerPacket(0x03), stage(std::move(stage)) {}

protected:
	void writeImpl(loginserver::LoginServerConnection* /*con*/, commons::utils::ByteBuffer& buf) override {
		stage->store(1); // writing
		waitUntil([this] { return stage->load() == 2; }); // released
		writeD(buf, 5);
	}

private:
	std::shared_ptr<std::atomic<int>> stage;
};

// D6 fix (docs/deviations/P4-15.md): the connection sendPacket checked is the one it sends through. The link drops (lsCon becomes null) while
// the packet is serialized between the check and the send; Java reads lsCon again and throws a NullPointerException, a second read in C++ would
// dereference a null std::shared_ptr.
TEST_F(LoginServerLinkTest, LinkDropWhileAPacketIsWritten) {
	connectAndAuthenticate();
	auto stage = std::make_shared<std::atomic<int>>(0);
	std::atomic<int> result{-1};
	std::thread sender([&] {
		runtime::TaskScope senderScope(AION_TASK_INFO(runtime::TaskKind::TEST));
		result = LoginServer::getInstance().sendPacket(BlockingLsPacket(stage)) ? 1 : 0;
	});
	ASSERT_TRUE(waitUntil([&] { return stage->load() == 1; }));
	EXPECT_EQ(result.load(), -1) << "C++: the packet is serialized inside sendPacket, on the sending thread";
	LoginServer::getInstance().disconnect();
	EXPECT_FALSE(LoginServer::getInstance().isUp());
	stage->store(2);
	sender.join();
	EXPECT_EQ(result.load(), 1) << "the link was up when the packet was sent (the closed connection ignores it)";
	EXPECT_FALSE(LoginServer::getInstance().sendPacket(loginserver::serverpackets::SM_ACCOUNT_DISCONNECTED(1)));
}

// D6 fix (docs/deviations/P4-15.md): sendPacket reads lsCon once. Java reads it in isUp() and again to send, so a sender racing with the link
// drop throws a NullPointerException; a second read in C++ would dereference a null std::shared_ptr and take the process down.
TEST_F(LoginServerLinkTest, SendingRacesWithTheLinkDrop) {
	connectAndAuthenticate();
	std::atomic<bool> stop{false};
	std::atomic<int64_t> sent{0};
	std::atomic<int64_t> refused{0};
	std::vector<std::thread> senders;
	for (int i = 0; i < 4; i++) {
		senders.emplace_back([&] {
			runtime::TaskScope senderScope(AION_TASK_INFO(runtime::TaskKind::TEST));
			while (!stop.load()) {
				if (LoginServer::getInstance().sendPacket(loginserver::serverpackets::SM_ACCOUNT_DISCONNECTED(1)))
					sent.fetch_add(1);
				else
					refused.fetch_add(1);
			}
		});
	}
	EXPECT_TRUE(waitUntil([&] { return sent.load() > 100; }));
	LoginServer::getInstance().disconnect(); // lsCon becomes null while the senders load it
	EXPECT_TRUE(waitUntil([&] { return refused.load() > 1000; }));
	stop = true;
	for (std::thread& sender : senders)
		sender.join();
	EXPECT_FALSE(LoginServer::getInstance().isUp());
	EXPECT_FALSE(LoginServer::getInstance().sendPacket(loginserver::serverpackets::SM_ACCOUNT_DISCONNECTED(1)));
}

// last in this file: it leaves a reconnect scheduled
TEST_F(LoginServerLinkTest, NotAuthenticatedClosesTheLink) {
	LogCapture logs({LS_LOGGER});
	LoginServer::getInstance().connect(*gameServer);
	ls.accept();
	ls.expectPacket(0x00);
	ls.send(PacketWriter().C(0x00).C(1).data); // CM_GS_AUTH_RESPONSE: not authed
	EXPECT_TRUE(ls.socket->waitClosed()) << logs.dump();
	EXPECT_TRUE(logs.waitFor("GameServer is not authenticated at LoginServer side!")) << logs.dump();
	EXPECT_FALSE(LoginServer::getInstance().isUp());
	EXPECT_TRUE(logs.waitFor("Lost connection with login server")) << logs.dump();
	EXPECT_TRUE(logs.waitFor("Reconnecting to login server in 15s...")) << logs.dump();
	// the scheduled reconnect runs in 15 s against the closed fake login server (refused, retried): harmless for the other tests
}

} // namespace
} // namespace aion::gameserver::network::test
