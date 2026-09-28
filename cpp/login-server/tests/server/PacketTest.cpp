// Packet-level tests over real connections, without the database: the byte layout of every server packet as the remote side receives it, and
// the parsing of client packets whose handling needs no database.

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "ServerTestUtils.h"
#include "aion/commons/network/NioServer.h"
#include "aion/commons/network/ServerCfg.h"
#include "aion/loginserver/GameServerTable.h"
#include "aion/loginserver/model/Account.h"
#include "aion/loginserver/network/aion/LoginConnection.h"
#include "aion/loginserver/network/aion/serverpackets/SM_ACCOUNT_BANNED.h"
#include "aion/loginserver/network/aion/serverpackets/SM_ACCOUNT_BANNED_2.h"
#include "aion/loginserver/network/aion/serverpackets/SM_ACCOUNT_KICK.h"
#include "aion/loginserver/network/aion/serverpackets/SM_AUTH_GG.h"
#include "aion/loginserver/network/aion/serverpackets/SM_LOGIN_FAIL.h"
#include "aion/loginserver/network/aion/serverpackets/SM_LOGIN_OK.h"
#include "aion/loginserver/network/aion/serverpackets/SM_PLAY_FAIL.h"
#include "aion/loginserver/network/aion/serverpackets/SM_PLAY_OK.h"
#include "aion/loginserver/network/aion/serverpackets/SM_SERVER_LIST.h"
#include "aion/loginserver/network/aion/serverpackets/SM_UPDATE_SESSION.h"
#include "aion/loginserver/network/gameserver/GsConnection.h"
#include "aion/loginserver/network/gameserver/serverpackets/SM_ACCOUNT_AUTH_RESPONSE.h"
#include "aion/loginserver/network/gameserver/serverpackets/SM_ACCOUNT_RECONNECT_KEY.h"
#include "aion/loginserver/network/gameserver/serverpackets/SM_BAN_RESPONSE.h"
#include "aion/loginserver/network/gameserver/serverpackets/SM_GS_AUTH_RESPONSE.h"
#include "aion/loginserver/network/gameserver/serverpackets/SM_GS_CHARACTER_RESPONSE.h"
#include "aion/loginserver/network/gameserver/serverpackets/SM_LS_CONTROL_RESPONSE.h"
#include "aion/loginserver/network/gameserver/serverpackets/SM_PING.h"
#include "aion/loginserver/network/gameserver/serverpackets/SM_PTRANSFER_RESPONSE.h"
#include "aion/loginserver/network/gameserver/serverpackets/SM_REQUEST_KICK_ACCOUNT.h"
#include "aion/loginserver/network/ncrypt/KeyGen.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::loginserver::test {
namespace {

using commons::network::NioServer;
using commons::network::ServerCfg;
using network::aion::AionAuthResponse;
using network::aion::LoginConnection;
using network::aion::SessionKey;
using network::gameserver::GsAuthResponse;
using network::gameserver::GsConnection;
using namespace network::aion::serverpackets;
using namespace network::gameserver::serverpackets;

/** A NioServer on an ephemeral port creating connections of type C and remembering the last one. */
template <typename C>
class CapturingServer {
public:
	CapturingServer() {
		auto packetProcessor = std::make_shared<typename C::Processor>(1, 2, 50, 3);
		auto pingScheduler = std::make_shared<utils::ScheduledExecutor>("TestPingPong");
		processor = packetProcessor;
		scheduler = pingScheduler;
		ServerCfg cfg{{"127.0.0.1", 0}, "test clients", [this, packetProcessor, pingScheduler](asio::ip::tcp::socket socket, NioServer& nioServer) {
			std::shared_ptr<C> connection = create(std::move(socket), nioServer, packetProcessor, pingScheduler);
			std::lock_guard lock(mutex);
			last = connection;
			return connection;
		}};
		server = std::make_unique<NioServer>(1, std::vector<ServerCfg>{std::move(cfg)}, 1);
		server->connect();
	}

	~CapturingServer() {
		server->shutdown();
		processor->shutdown();
		scheduler->shutdown();
	}

	uint16_t port() const { return server->getBoundAddresses().at(0).port; }

	/** @return the connection created last (waits until there is one) */
	std::shared_ptr<C> connection() {
		std::shared_ptr<C> result;
		waitUntil([&] {
			std::lock_guard lock(mutex);
			result = last;
			return result != nullptr;
		});
		return result;
	}

private:
	static std::shared_ptr<C> create(asio::ip::tcp::socket socket, NioServer& nioServer, const std::shared_ptr<typename C::Processor>& processor,
		const std::shared_ptr<utils::ScheduledExecutor>& scheduler) {
		if constexpr (std::is_same_v<C, GsConnection>)
			return std::make_shared<GsConnection>(std::move(socket), nioServer, processor, scheduler);
		else
			return std::make_shared<LoginConnection>(std::move(socket), nioServer, processor);
	}

	std::shared_ptr<typename C::Processor> processor;
	std::shared_ptr<utils::ScheduledExecutor> scheduler;
	std::unique_ptr<NioServer> server;
	std::mutex mutex;
	std::shared_ptr<C> last;
};

class AionPacketTest : public ::testing::Test {
protected:
	static void SetUpTestSuite() { network::ncrypt::KeyGen::init(); }

	/** Compares a decrypted server packet body with the expected payload (opcode + data), except a last byte lost to the checksum. */
	static void expectPayload(const std::vector<uint8_t>& body, const std::vector<uint8_t>& payload) {
		ASSERT_GE(body.size(), payload.size());
		// see AionServerPacket::write: for payload sizes % 8 == 5 the checksum overwrites the last payload byte (like Java)
		size_t comparable = payload.size() % 8 == 5 ? payload.size() - 1 : payload.size();
		EXPECT_EQ(std::vector<uint8_t>(body.begin(), body.begin() + static_cast<ptrdiff_t>(comparable)),
			std::vector<uint8_t>(payload.begin(), payload.begin() + static_cast<ptrdiff_t>(comparable)));
		EXPECT_EQ(body.size() % 8, 0u);
	}

	CapturingServer<LoginConnection> server;
};

TEST_F(AionPacketTest, SmInitAndConnectionState) {
	AionTestClient client(server.port());
	auto init = client.readInit();
	auto con = server.connection();
	EXPECT_EQ(init.sessionId, con->getSessionId());
	EXPECT_GT(init.sessionId, 0);
	EXPECT_EQ(init.protocolRevision, 0xc621);
	EXPECT_TRUE(std::equal(init.encryptedModulus.begin(), init.encryptedModulus.end(), con->getEncryptedModulus().begin()));
	// SM_INIT body: opcode, session id, revision, 128 modulus bytes, 16 zeros, blowfish key, 7 zeros, C 0, D 0, H 0, C 0, D 0x3FCE09ED, D 0
	PacketReader reader(init.body);
	reader.C();
	reader.D();
	reader.D();
	reader.B(128);
	EXPECT_EQ(reader.B(16), std::vector<uint8_t>(16));
	reader.B(16);
	EXPECT_EQ(reader.B(7), std::vector<uint8_t>(7));
	EXPECT_EQ(reader.C(), 0);
	EXPECT_EQ(reader.D(), 0);
	EXPECT_EQ(reader.H(), 0);
	EXPECT_EQ(reader.C(), 0);
	EXPECT_EQ(reader.D(), 0x3FCE09ED);
	EXPECT_EQ(con->getState(), LoginConnection::State::CONNECTED);
	EXPECT_EQ(con->toString(), "Client 127.0.0.1");
}

TEST_F(AionPacketTest, LayoutOfAllAionServerPackets) {
	AionTestClient client(server.port());
	client.readInit();
	auto con = server.connection();
	SessionKey key(0x11223344, 0x55667788, static_cast<int32_t>(0x99AABBCC), 0x0DDEEFF0);

	con->sendPacket(std::make_shared<SM_ACCOUNT_BANNED>());
	expectPayload(client.expectPacket(0x02), {0x02});

	con->sendPacket(std::make_shared<SM_ACCOUNT_BANNED_2>());
	expectPayload(client.expectPacket(0x09), {0x09});

	con->sendPacket(std::make_shared<SM_ACCOUNT_KICK>(AionAuthResponse::STR_L2AUTH_S_KICKED_DOUBLE_LOGIN));
	expectPayload(client.expectPacket(0x08), PacketWriter().C(0x08).D(13).data);

	con->sendPacket(std::make_shared<SM_AUTH_GG>(0x12345678));
	expectPayload(client.expectPacket(0x0b),
		PacketWriter().C(0x0b).D(0x12345678).D(0).D(0).D(0).D(0).D(0xCD5000).D(0).D(0x0b000000).D(0x12345678 ^ 0xCD5000).zeros(3).data);

	con->sendPacket(std::make_shared<SM_LOGIN_FAIL>(AionAuthResponse::STR_L2AUTH_S_INCORRECT_PWD));
	expectPayload(client.expectPacket(0x01), PacketWriter().C(0x01).D(3).data);

	con->sendPacket(std::make_shared<SM_LOGIN_OK>(key));
	expectPayload(client.expectPacket(0x03),
		PacketWriter().C(0x03).D(key.accountId).D(key.loginOk).D(0).D(0).D(0x3ea).D(0).D(0).D(0).D(0).D(0).D(0).D(0).zeros(0x13).data);

	con->sendPacket(std::make_shared<SM_PLAY_FAIL>(AionAuthResponse::STR_L2AUTH_S_SERVER_DOWN));
	expectPayload(client.expectPacket(0x06), PacketWriter().C(0x06).D(8).data);

	con->sendPacket(std::make_shared<SM_PLAY_OK>(key, int8_t{3}));
	expectPayload(client.expectPacket(0x07), PacketWriter().C(0x07).D(key.playOk1).D(key.playOk2).C(3).zeros(0x0E).data);

	con->sendPacket(std::make_shared<SM_UPDATE_SESSION>(key));
	expectPayload(client.expectPacket(0x0c), PacketWriter().C(0x0c).D(key.accountId).D(key.loginOk).C(0).data);
}

TEST_F(AionPacketTest, SmServerListLayoutWithoutGameServers) {
	if (GameServerTable::size() != 0)
		GTEST_SKIP() << "game servers were loaded by another test";
	AionTestClient client(server.port());
	client.readInit();
	auto con = server.connection();
	auto account = std::make_shared<model::Account>();
	account->setId(987654);
	account->setLastServer(5);
	con->setAccount(account);
	con->sendPacket(std::make_shared<SM_SERVER_LIST>());
	expectPayload(client.expectPacket(0x04), PacketWriter().C(0x04).C(0).C(5).H(1).C(1).zeros(13).data);
}

TEST_F(AionPacketTest, ServerListWithoutAccountDisconnects) {
	AionTestClient client(server.port());
	client.readInit();
	server.connection()->sendPacket(std::make_shared<SM_SERVER_LIST>());
	EXPECT_TRUE(client.socket.waitClosed());
}

TEST_F(AionPacketTest, CmAuthGgWithRightAndWrongSessionId) {
	{
		AionTestClient client(server.port());
		auto init = client.readInit();
		client.sendPacket(AionTestClient::buildCM_AUTH_GG(init.sessionId));
		PacketReader reader(client.expectPacket(0x0b));
		reader.C();
		EXPECT_EQ(reader.D(), init.sessionId);
		EXPECT_EQ(server.connection()->getState(), LoginConnection::State::AUTHED_GG);
	}
	{
		LogCapture logs("com.aionemu.commons.network.packet.BaseClientPacket", spdlog::level::info);
		AionTestClient client(server.port());
		auto init = client.readInit();
		client.sendPacket(AionTestClient::buildCM_AUTH_GG(init.sessionId + 1));
		PacketReader reader(client.expectPacket(0x01));
		reader.C();
		EXPECT_EQ(reader.D() & 0xFFFFFF, 20); // STR_L2AUTH_S_SYSTEM_ERROR
		EXPECT_TRUE(client.socket.waitClosed());
		EXPECT_EQ(server.connection()->getState(), LoginConnection::State::CONNECTED);
		EXPECT_FALSE(logs.contains("Missing")) << logs.dump();
	}
}

TEST_F(AionPacketTest, CmLoginWithWrongSessionIdOrInvalidRsaDataFails) {
	LogCapture logs("com.aionemu.loginserver.network.aion.AionClientPacket");
	AionTestClient client(server.port());
	client.authGG();

	// wrong session id: SM_LOGIN_FAIL without closing
	client.sendPacket(client.crypto.buildCM_LOGIN("user", "password", false, -1, client.crypto.getSessionId() + 1));
	PacketReader wrongSession(client.expectPacket(0x01));
	wrongSession.C();
	EXPECT_EQ(wrongSession.D() & 0xFFFFFF, 20);

	// RSA block not smaller than the modulus: decryption fails (Java: GeneralSecurityException)
	std::vector<uint8_t> payload = PacketWriter().C(0x00).B(std::vector<uint8_t>(128, 0xFF)).D(client.crypto.getSessionId()).zeros(16).zeros(7).zeros(16).data;
	client.sendPacket(payload);
	PacketReader invalidRsa(client.expectPacket(0x01));
	invalidRsa.C();
	EXPECT_EQ(invalidRsa.D() & 0xFFFFFF, 20);

	// login data size not a multiple of 128: the exception is logged, no response
	payload = PacketWriter().C(0x00).zeros(104).D(client.crypto.getSessionId()).zeros(39).data;
	client.sendPacket(payload);
	EXPECT_TRUE(logs.waitFor("error handling client (127.0.0.1) message [000] CM_LOGIN")) << logs.dump();
	EXPECT_FALSE(client.readPacket(300ms).has_value());
	EXPECT_FALSE(client.socket.isClosed());
}

TEST_F(AionPacketTest, CmPlayCmServerListAndCmUpdateSessionChecks) {
	{
		// CM_PLAY for an unknown game server
		AionTestClient client(server.port());
		client.readInit();
		auto con = server.connection();
		auto account = std::make_shared<model::Account>();
		account->setId(555);
		con->setAccount(account);
		con->setSessionKey(SessionKey(555, 777, 1, 2));
		con->setState(LoginConnection::State::AUTHED_LOGIN);
		client.sendPacket(AionTestClient::buildCM_PLAY(555, 777, 120));
		PacketReader reader(client.expectPacket(0x06));
		reader.C();
		EXPECT_EQ(reader.D() & 0xFFFFFF, 8); // STR_L2AUTH_S_SERVER_DOWN
		EXPECT_FALSE(con->isJoinedGs());

		// CM_PLAY with wrong session key
		client.sendPacket(AionTestClient::buildCM_PLAY(555, 778, 1));
		PacketReader fail(client.expectPacket(0x01));
		fail.C();
		EXPECT_EQ(fail.D() & 0xFFFFFF, 20);
		EXPECT_TRUE(client.socket.waitClosed());
	}
	{
		// CM_SERVER_LIST with wrong session key
		AionTestClient client(server.port());
		client.readInit();
		auto con = server.connection();
		con->setSessionKey(SessionKey(556, 1, 1, 2));
		con->setState(LoginConnection::State::AUTHED_LOGIN);
		client.sendPacket(AionTestClient::buildCM_SERVER_LIST(556, 2));
		PacketReader fail(client.expectPacket(0x01));
		fail.C();
		EXPECT_EQ(fail.D() & 0xFFFFFF, 20);
		EXPECT_TRUE(client.socket.waitClosed());
	}
	{
		// CM_UPDATE_SESSION for an account that is not reconnecting
		AionTestClient client(server.port());
		client.readInit();
		client.sendPacket(AionTestClient::buildCM_UPDATE_SESSION(123456789, 1, 2));
		EXPECT_TRUE(client.socket.waitClosed());
	}
}

TEST_F(AionPacketTest, UnknownPacketsAreLoggedAndWrongChecksumsDisconnect) {
	LogCapture logs({"com.aionemu.loginserver.network.factories.AionPacketHandlerFactory", "com.aionemu.loginserver.network.aion.LoginConnection"});
	AionTestClient client(server.port());
	client.readInit();
	client.sendPacket(std::vector<uint8_t>{0x01, 0xAA, 0xBB});
	EXPECT_TRUE(logs.waitFor(
		"Unknown packet received from client: opCode=0x01 state=CONNECTED length=15 data=[AA BB 00 00 00 00 00 01 AA BB 00 00 00 00 00]"))
		<< logs.dump();

	// CM_LOGIN is unknown before CM_AUTH_GG
	client.sendPacket(std::vector<uint8_t>{0x00});
	EXPECT_TRUE(logs.waitFor("opCode=0x00 state=CONNECTED")) << logs.dump();
	EXPECT_FALSE(client.socket.isClosed());

	std::vector<uint8_t> garbage(16, 0x5A);
	client.socket.send(AionLoginClientCrypto::makeFrame(garbage));
	EXPECT_TRUE(client.socket.waitClosed());
	EXPECT_TRUE(logs.waitFor("Wrong checksum from Client 127.0.0.1")) << logs.dump();
}

class GsPacketTest : public ::testing::Test {
protected:
	CapturingServer<GsConnection> server;
};

TEST_F(GsPacketTest, LayoutOfGameServerPackets) {
	GsTestClient gs(server.port());
	auto con = server.connection();
	using service::ptransfer::PlayerTransferRequest;
	using service::ptransfer::PlayerTransferResultStatus;
	using service::ptransfer::PlayerTransferTask;

	con->sendPacket(std::make_shared<SM_PING>());
	EXPECT_EQ(gs.readPacket(5s, true), (std::vector<uint8_t>{11}));

	con->sendPacket(std::make_shared<SM_REQUEST_KICK_ACCOUNT>(0x01020304, true));
	EXPECT_EQ(gs.readPacket(), PacketWriter().C(2).D(0x01020304).C(1).data);
	con->sendPacket(std::make_shared<SM_REQUEST_KICK_ACCOUNT>(7, false));
	EXPECT_EQ(gs.readPacket(), PacketWriter().C(2).D(7).C(0).data);

	con->sendPacket(std::make_shared<SM_ACCOUNT_RECONNECT_KEY>(42, -5));
	EXPECT_EQ(gs.readPacket(), PacketWriter().C(3).D(42).D(-5).data);

	con->sendPacket(std::make_shared<SM_BAN_RESPONSE>(int8_t{3}, 42, "10.0.0.1", -1, 99, true));
	EXPECT_EQ(gs.readPacket(), PacketWriter().C(5).C(3).D(42).S("10.0.0.1").D(-1).D(99).C(1).data);

	con->sendPacket(std::make_shared<SM_GS_CHARACTER_RESPONSE>(1234));
	EXPECT_EQ(gs.readPacket(), PacketWriter().C(8).D(1234).data);

	con->sendPacket(std::make_shared<SM_LS_CONTROL_RESPONSE>(int8_t{2}, int8_t{-1}, 42, 77, false));
	EXPECT_EQ(gs.readPacket(), PacketWriter().C(4).C(2).C(-1).D(42).D(77).C(0).data);

	con->sendPacket(std::make_shared<SM_GS_AUTH_RESPONSE>(GsAuthResponse::NOT_AUTHED));
	EXPECT_EQ(gs.readPacket(), PacketWriter().C(0).C(1).data);
	con->sendPacket(std::make_shared<SM_GS_AUTH_RESPONSE>(GsAuthResponse::ALREADY_REGISTERED));
	EXPECT_EQ(gs.readPacket(), PacketWriter().C(0).C(2).data);
	con->sendPacket(std::make_shared<SM_GS_AUTH_RESPONSE>(GsAuthResponse::AUTHED));
	EXPECT_EQ(gs.readPacket(), PacketWriter().C(0).C(0).C(GameServerTable::size()).data);

	model::AccountTime accountTime;
	accountTime.setAccumulatedOnlineTime(1234567890123);
	accountTime.setAccumulatedRestTime(-2);
	con->sendPacket(std::make_shared<SM_ACCOUNT_AUTH_RESPONSE>(42, true, "Nameä", 1700000000123, int8_t{3}, int8_t{1}, std::string("HDD"), accountTime));
	EXPECT_EQ(gs.readPacket(), PacketWriter().C(1).D(42).C(1).S("Nameä").Q(1700000000123).Q(1234567890123).Q(-2).C(3).C(1).S("HDD").data);
	con->sendPacket(std::make_shared<SM_ACCOUNT_AUTH_RESPONSE>(42, true, "n", 0, int8_t{0}, int8_t{0}, std::nullopt, accountTime));
	EXPECT_EQ(gs.readPacket(), PacketWriter().C(1).D(42).C(1).S("n").Q(0).Q(1234567890123).Q(-2).C(0).C(0).S("").data);
	con->sendPacket(std::make_shared<SM_ACCOUNT_AUTH_RESPONSE>(43, false, "", 0, int8_t{0}, int8_t{0}, std::nullopt, std::nullopt));
	EXPECT_EQ(gs.readPacket(), PacketWriter().C(1).D(43).C(0).data);

	PlayerTransferTask task;
	task.id = 9;
	task.sourceServerId = 1;
	task.targetServerId = -2;
	task.sourceAccountId = 100;
	task.targetAccountId = 200;
	task.playerId = 300;
	con->sendPacket(std::make_shared<SM_PTRANSFER_RESPONSE>(PlayerTransferResultStatus::PERFORM_ACTION, task));
	EXPECT_EQ(gs.readPacket(), PacketWriter().C(12).D(23).C(1).C(-2).D(100).D(200).D(300).D(9).data);

	con->sendPacket(std::make_shared<SM_PTRANSFER_RESPONSE>(PlayerTransferResultStatus::ERROR_, 9, "reason"));
	EXPECT_EQ(gs.readPacket(), PacketWriter().C(12).D(22).D(9).S("reason").data);

	con->sendPacket(std::make_shared<SM_PTRANSFER_RESPONSE>(PlayerTransferResultStatus::OK, 9));
	EXPECT_EQ(gs.readPacket(), PacketWriter().C(12).D(21).D(9).data);

	auto request = std::make_shared<PlayerTransferRequest>(service::ptransfer::PlayerTransferStatus::STEP1);
	request->taskId = 9;
	request->targetAccountId = 200;
	request->name = "Char";
	request->db = {1, 2, 3, 4, 5};
	request->targetAccount = std::make_shared<model::Account>();
	request->targetAccount->setName("target");
	con->sendPacket(std::make_shared<SM_PTRANSFER_RESPONSE>(PlayerTransferResultStatus::SEND_INFO, std::shared_ptr<const PlayerTransferRequest>(request)));
	EXPECT_EQ(gs.readPacket(), PacketWriter().C(12).D(20).D(200).D(9).S("Char").S("target").D(5).B(request->db).data);
	con->sendPacket(std::make_shared<SM_PTRANSFER_RESPONSE>(PlayerTransferResultStatus::OK, std::shared_ptr<const PlayerTransferRequest>(request)));
	EXPECT_EQ(gs.readPacket(), PacketWriter().C(12).D(21).D(9).data);

	EXPECT_EQ(con->toString(), "Gameserver 127.0.0.1");
	EXPECT_EQ(con->getState(), GsConnection::State::CONNECTED);
}

TEST_F(GsPacketTest, CmGsAuthWithUnknownIdAndUnknownOrMalformedPackets) {
	LogCapture logs({"com.aionemu.loginserver.GameServerTable", "com.aionemu.loginserver.network.factories.GsPacketHandlerFactory",
		"com.aionemu.commons.network.packet.BaseClientPacket", "com.aionemu.loginserver.network.gameserver.GsConnection"});
	{
		GsTestClient gs(server.port());
		// packets other than CM_GS_AUTH are unknown before the authentication
		gs.send(PacketWriter().C(1).D(1).D(2).D(3).D(4).data);
		EXPECT_TRUE(logs.waitFor("Unknown packet received from Game Server: 0x01 state=CONNECTED")) << logs.dump();
		gs.send(PacketWriter().C(0xFE).data);
		EXPECT_TRUE(logs.waitFor("Unknown packet received from Game Server: 0xFE state=CONNECTED")) << logs.dump();

		// malformed: negative ip length
		gs.send(PacketWriter().C(0).C(1).S("pw").C(-1).data);
		EXPECT_TRUE(logs.waitFor("Reading failed for packet [000] CM_GS_AUTH")) << logs.dump();
		EXPECT_FALSE(gs.readPacket(200ms).has_value());

		gs.send(GsTestClient::buildCM_GS_AUTH(99, "password"));
		EXPECT_EQ(gs.readPacket(), PacketWriter().C(0).C(1).data);
		EXPECT_TRUE(gs.socket.waitClosed());
		EXPECT_TRUE(logs.waitFor("Gameserver 127.0.0.1 requestedID: 99 is not registered in LS database!")) << logs.dump();
		EXPECT_TRUE(logs.waitFor("Gameserver connection attempt from: 127.0.0.1")) << logs.dump();
		EXPECT_TRUE(logs.waitFor("Gameserver 127.0.0.1 disconnected")) << logs.dump();
	}
	{
		// an empty packet (only parsed when more data follows, like Java) closes the connection
		GsTestClient gs(server.port());
		gs.socket.send(std::vector<uint8_t>{0x02, 0x00, 0x03, 0x00, 0x0C});
		EXPECT_TRUE(gs.socket.waitClosed());
	}
}

TEST_F(GsPacketTest, PingPongTaskStartsWhenAuthedAndClosesDeadConnections) {
	LogCapture logs("com.aionemu.loginserver.PingPongTask");
	auto oldPeriod = PingPongTask::PERIOD.load();
	PingPongTask::PERIOD = 50ms;
	GsTestClient gs(server.port());
	auto con = server.connection();
	con->setState(GsConnection::State::AUTHED); // starts the task
	EXPECT_THROW(con->getPingPongTask().start(*std::make_shared<utils::ScheduledExecutor>("unused")), commons::utils::UnsupportedOperationException);
	for (int i = 0; i < 3; i++)
		EXPECT_EQ(gs.readPacket(2s, true), (std::vector<uint8_t>{11}));
	EXPECT_TRUE(gs.socket.waitClosed(3s));
	EXPECT_TRUE(logs.waitFor("Gameserver #null connection died, closing it.")) << logs.dump();
	PingPongTask::PERIOD = oldPeriod;
}

} // namespace
} // namespace aion::loginserver::test
