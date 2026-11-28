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
#include "aion/commons/network/netty/ConnectionHandler.h"
#include "aion/commons/network/netty/NettyPipeline.h"
#include "aion/commons/network/netty/PacketExecutorGroup.h"
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

/** A NioServer on an ephemeral port creating connections of type C (like NetConnector) and remembering the last one. */
template <typename C>
class CapturingServer {
public:
	CapturingServer() {
		auto packetExecutors = std::make_shared<commons::network::netty::PacketExecutorGroup>(2);
		auto pingScheduler = std::make_shared<utils::ScheduledExecutor>("TestPingPong");
		executors = packetExecutors;
		scheduler = pingScheduler;
		ServerCfg cfg{{"127.0.0.1", 0}, "test clients",
			[this, packetExecutors, pingScheduler](asio::ip::tcp::socket socket, NioServer& nioServer) -> std::shared_ptr<commons::network::AConnectionBase> {
				auto channel = std::make_shared<commons::network::netty::ConnectionHandler>(std::move(socket), nioServer,
					std::is_same_v<C, GsConnection> ? 8192 * 8 : 8192 * 2);
				std::shared_ptr<C> connection = create(channel, packetExecutors->next(), pingScheduler);
				commons::network::netty::NettyPipeline::init(*channel, connection);
				std::lock_guard lock(mutex);
				last = connection;
				return channel;
			}};
		server = std::make_unique<NioServer>(1, std::vector<ServerCfg>{std::move(cfg)}, 1);
		server->connect();
	}

	~CapturingServer() {
		server->shutdown();
		executors->shutdown(std::chrono::seconds(3));
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
	static std::shared_ptr<C> create(std::shared_ptr<commons::network::netty::ConnectionHandler> channel,
		commons::network::netty::NettyConnectionBase::Executor executor, const std::shared_ptr<utils::ScheduledExecutor>& scheduler) {
		if constexpr (std::is_same_v<C, GsConnection>)
			return std::make_shared<GsConnection>(std::move(channel), std::move(executor), scheduler);
		else
			return std::make_shared<LoginConnection>(std::move(channel), std::move(executor));
	}

	std::shared_ptr<commons::network::netty::PacketExecutorGroup> executors;
	std::shared_ptr<utils::ScheduledExecutor> scheduler;
	std::unique_ptr<NioServer> server;
	std::mutex mutex;
	std::shared_ptr<C> last;
};

class AionPacketTest : public ::testing::Test {
protected:
	static void SetUpTestSuite() { network::ncrypt::KeyGen::init(); }

	/**
	 * Compares a decrypted server packet body with the expected payload (opcode + data): upstream bce89b2fc encrypts the whole payload, followed
	 * by zeroed padding and the checksum in the last word.
	 */
	static void expectPayload(const std::vector<uint8_t>& body, const std::vector<uint8_t>& payload) {
		ASSERT_GE(body.size(), payload.size() + 4);
		EXPECT_EQ(std::vector<uint8_t>(body.begin(), body.begin() + static_cast<ptrdiff_t>(payload.size())), payload);
		EXPECT_EQ(std::vector<uint8_t>(body.begin() + static_cast<ptrdiff_t>(payload.size()), body.end() - 4),
			std::vector<uint8_t>(body.size() - 4 - payload.size()))
			<< "padding";
		EXPECT_EQ(body.size(), (payload.size() + 4 + 8) / 8 * 8); // Java: length += 4; length += 8 - length % 8
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
	// upstream bce89b2fc, SM_INIT body: opcode, session id, revision, 128 modulus bytes, 16 zeros, blowfish key, C 0 (login mode: password)
	PacketReader reader(init.body);
	reader.C();
	reader.D();
	reader.D();
	reader.B(128);
	EXPECT_EQ(reader.B(16), std::vector<uint8_t>(16));
	reader.B(16);
	EXPECT_EQ(reader.C(), 0);
	// 170 payload bytes, then the padding, the XOR key and the last word (Java: length += 4 + 4, padded to whole blocks)
	EXPECT_EQ(init.body.size(), 184u);
	EXPECT_EQ(con->getState(), LoginConnection::State::CONNECTED);
	EXPECT_EQ(con->toString(), "Client 127.0.0.1");
}

TEST_F(AionPacketTest, LayoutOfAllAionServerPackets) {
	AionTestClient client(server.port());
	client.readInit();
	auto con = server.connection();
	SessionKey key(0x11223344, 0x55667788, static_cast<int32_t>(0x99AABBCC), 0x0DDEEFF0);

	// upstream bce89b2fc: the layouts the 4.8 client reads
	con->sendPacket(SM_ACCOUNT_BANNED());
	expectPayload(client.expectPacket(0x02), PacketWriter().C(0x02).D(0).D(0).data);
	con->sendPacket(SM_ACCOUNT_BANNED(27, 0));
	expectPayload(client.expectPacket(0x02), PacketWriter().C(0x02).D(27).D(0).data);

	con->sendPacket(SM_ACCOUNT_BANNED_2());
	expectPayload(client.expectPacket(0x09), PacketWriter().C(0x09).C(0).data);

	con->sendPacket(SM_ACCOUNT_KICK(AionAuthResponse::STR_L2AUTH_S_KICKED_DOUBLE_LOGIN));
	expectPayload(client.expectPacket(0x08), PacketWriter().C(0x08).C(13).data);

	con->sendPacket(SM_AUTH_GG());
	expectPayload(client.expectPacket(0x0b), {0x0b});

	con->sendPacket(SM_LOGIN_FAIL(AionAuthResponse::STR_L2AUTH_S_INCORRECT_PWD));
	expectPayload(client.expectPacket(0x01), PacketWriter().C(0x01).C(3).data);

	con->sendPacket(SM_LOGIN_OK(key));
	expectPayload(client.expectPacket(0x03), PacketWriter().C(0x03).D(key.accountId).D(key.loginOk).zeros(20).D(0).zeros(16).D(0).data);

	con->sendPacket(SM_PLAY_FAIL(AionAuthResponse::STR_L2AUTH_S_SERVER_DOWN));
	expectPayload(client.expectPacket(0x06), PacketWriter().C(0x06).C(8).data);

	con->sendPacket(SM_PLAY_OK(key, int8_t{3}));
	expectPayload(client.expectPacket(0x07), PacketWriter().C(0x07).D(key.playOk1).D(key.playOk2).C(3).data);

	con->sendPacket(SM_UPDATE_SESSION(key));
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
	con->sendPacket(SM_SERVER_LIST());
	expectPayload(client.expectPacket(0x04), PacketWriter().C(0x04).C(0).C(5).H(1).C(1).data);
}

TEST_F(AionPacketTest, ServerListWithoutAccountDisconnects) {
	AionTestClient client(server.port());
	client.readInit();
	server.connection()->sendPacket(SM_SERVER_LIST());
	EXPECT_TRUE(client.socket.waitClosed());
}

TEST_F(AionPacketTest, CmAuthGgWithRightAndWrongSessionId) {
	{
		AionTestClient client(server.port());
		auto init = client.readInit();
		client.sendPacket(AionTestClient::buildCM_AUTH_GG(init.sessionId));
		client.expectPacket(0x0b); // empty (upstream bce89b2fc)
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
		EXPECT_FALSE(logs.contains("shorter than expected")) << logs.dump();
		EXPECT_FALSE(logs.contains("not fully read")) << logs.dump();
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

	// upstream bce89b2fc: the login data is as many whole RSA blocks as fit before the 43 bytes of fields, so a packet shorter than the fields is
	// too short, which closes the connection (upstream e1a399e62, 459b35094)
	payload = PacketWriter().C(0x00).D(client.crypto.getSessionId()).zeros(20).data;
	client.sendPacket(payload);
	EXPECT_TRUE(client.socket.waitClosed());
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
		"Unknown packet received from client: opCode=0x01 state=CONNECTED length=7 data=[AA BB 00 00 00 00 00]"))
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

	con->sendPacket(SM_PING());
	EXPECT_EQ(gs.readPacket(5s, true), (std::vector<uint8_t>{11}));

	con->sendPacket(SM_REQUEST_KICK_ACCOUNT(0x01020304, true));
	EXPECT_EQ(gs.readPacket(), PacketWriter().C(2).D(0x01020304).C(1).data);
	con->sendPacket(SM_REQUEST_KICK_ACCOUNT(7, false));
	EXPECT_EQ(gs.readPacket(), PacketWriter().C(2).D(7).C(0).data);

	con->sendPacket(SM_ACCOUNT_RECONNECT_KEY(42, -5));
	EXPECT_EQ(gs.readPacket(), PacketWriter().C(3).D(42).D(-5).data);

	con->sendPacket(SM_BAN_RESPONSE(int8_t{3}, 42, "10.0.0.1", -1, 99, true));
	EXPECT_EQ(gs.readPacket(), PacketWriter().C(5).C(3).D(42).S("10.0.0.1").D(-1).D(99).C(1).data);

	con->sendPacket(SM_GS_CHARACTER_RESPONSE(1234));
	EXPECT_EQ(gs.readPacket(), PacketWriter().C(8).D(1234).data);

	con->sendPacket(SM_LS_CONTROL_RESPONSE(int8_t{2}, int8_t{-1}, 42, 77, false));
	EXPECT_EQ(gs.readPacket(), PacketWriter().C(4).C(2).C(-1).D(42).D(77).C(0).data);

	con->sendPacket(SM_GS_AUTH_RESPONSE(GsAuthResponse::NOT_AUTHED));
	EXPECT_EQ(gs.readPacket(), PacketWriter().C(0).C(1).data);
	con->sendPacket(SM_GS_AUTH_RESPONSE(GsAuthResponse::ALREADY_REGISTERED));
	EXPECT_EQ(gs.readPacket(), PacketWriter().C(0).C(2).data);
	con->sendPacket(SM_GS_AUTH_RESPONSE(GsAuthResponse::AUTHED));
	EXPECT_EQ(gs.readPacket(), PacketWriter().C(0).C(0).C(GameServerTable::size()).data);

	model::AccountTime accountTime;
	accountTime.setAccumulatedOnlineTime(1234567890123);
	accountTime.setAccumulatedRestTime(-2);
	con->sendPacket(SM_ACCOUNT_AUTH_RESPONSE(42, true, "Nameä", 1700000000123, int8_t{3}, int8_t{1}, std::string("HDD"), accountTime));
	EXPECT_EQ(gs.readPacket(), PacketWriter().C(1).D(42).C(1).S("Nameä").Q(1700000000123).Q(1234567890123).Q(-2).C(3).C(1).S("HDD").data);
	con->sendPacket(SM_ACCOUNT_AUTH_RESPONSE(42, true, "n", 0, int8_t{0}, int8_t{0}, std::nullopt, accountTime));
	EXPECT_EQ(gs.readPacket(), PacketWriter().C(1).D(42).C(1).S("n").Q(0).Q(1234567890123).Q(-2).C(0).C(0).S("").data);
	con->sendPacket(SM_ACCOUNT_AUTH_RESPONSE(43, false, "", 0, int8_t{0}, int8_t{0}, std::nullopt, std::nullopt));
	EXPECT_EQ(gs.readPacket(), PacketWriter().C(1).D(43).C(0).data);

	PlayerTransferTask task;
	task.id = 9;
	task.sourceServerId = 1;
	task.targetServerId = -2;
	task.sourceAccountId = 100;
	task.targetAccountId = 200;
	task.playerId = 300;
	con->sendPacket(SM_PTRANSFER_RESPONSE(PlayerTransferResultStatus::PERFORM_ACTION, task));
	EXPECT_EQ(gs.readPacket(), PacketWriter().C(12).D(23).C(1).C(-2).D(100).D(200).D(300).D(9).data);

	con->sendPacket(SM_PTRANSFER_RESPONSE(PlayerTransferResultStatus::ERROR_, 9, "reason"));
	EXPECT_EQ(gs.readPacket(), PacketWriter().C(12).D(22).D(9).S("reason").data);

	con->sendPacket(SM_PTRANSFER_RESPONSE(PlayerTransferResultStatus::OK, 9));
	EXPECT_EQ(gs.readPacket(), PacketWriter().C(12).D(21).D(9).data);

	auto request = std::make_shared<PlayerTransferRequest>(service::ptransfer::PlayerTransferStatus::STEP1);
	request->taskId = 9;
	request->targetAccountId = 200;
	request->name = "Char";
	request->db = {1, 2, 3, 4, 5};
	request->targetAccount = std::make_shared<model::Account>();
	request->targetAccount->setName("target");
	con->sendPacket(SM_PTRANSFER_RESPONSE(PlayerTransferResultStatus::SEND_INFO, std::shared_ptr<const PlayerTransferRequest>(request)));
	EXPECT_EQ(gs.readPacket(), PacketWriter().C(12).D(20).D(200).D(9).S("Char").S("target").D(5).B(request->db).data);
	con->sendPacket(SM_PTRANSFER_RESPONSE(PlayerTransferResultStatus::OK, std::shared_ptr<const PlayerTransferRequest>(request)));
	EXPECT_EQ(gs.readPacket(), PacketWriter().C(12).D(21).D(9).data);

	EXPECT_EQ(con->toString(), "Game server 127.0.0.1");
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

		// malformed: negative ip length, which makes the packet too short (readB) and closes the connection (upstream e1a399e62, bce89b2fc)
		gs.send(PacketWriter().C(0).C(1).S("pw").C(-1).data);
		EXPECT_TRUE(gs.socket.waitClosed());
	}
	{
		GsTestClient gs(server.port());
		gs.send(GsTestClient::buildCM_GS_AUTH(99, "password"));
		EXPECT_EQ(gs.readPacket(), PacketWriter().C(0).C(1).data);
		EXPECT_TRUE(gs.socket.waitClosed());
		EXPECT_TRUE(logs.waitFor("Game server 127.0.0.1 requestedID: 99 is not registered in LS database!")) << logs.dump();
		EXPECT_TRUE(logs.waitFor("Game server connected: 127.0.0.1")) << logs.dump();
		EXPECT_TRUE(logs.waitFor("Game server 127.0.0.1 disconnected")) << logs.dump();
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
	EXPECT_TRUE(logs.waitFor("Game server #null connection died, closing it.")) << logs.dump();
	PingPongTask::PERIOD = oldPeriod;
}

} // namespace
} // namespace aion::loginserver::test
