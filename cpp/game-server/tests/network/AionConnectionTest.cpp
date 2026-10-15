// AionConnection against FakeGameClient over loopback TCP (handlers-and-porting-plan.md §3.2 P4-15): SM_KEY first and unencrypted, the key
// exchange with the inverse client cipher, encrypted traffic in both directions, 3-strike decryption, dropping data before SM_KEY, state
// gating, per-connection packet order, the send queue ordered by serialization sequence, close packets and the packet flood filter.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/configs/network/PffConfig.h"
#include "aion/gameserver/network/Crypt.h"
#include "aion/gameserver/network/EncryptionKeyPair.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/SerializedBody.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "FakeGameClient.h"
#include "support/GameServerTestServer.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::test {
namespace {

using State = aion::AionConnection::State;

constexpr int32_t CM_VERSION_CHECK = 0; // CONNECTED
constexpr int32_t CM_ENTER_WORLD = 8;   // AUTHED
constexpr int32_t CM_TIME_CHECK = 18;   // CONNECTED, AUTHED, IN_GAME
constexpr int32_t CM_PING = 44;         // AUTHED, IN_GAME (the recording packet echoes it)

const char* CONNECTION_LOGGER = "com.aionemu.gameserver.network.aion.AionConnection";
const char* FACTORY_LOGGER = "com.aionemu.gameserver.network.aion.AionClientPacketFactory";

class AionConnectionTest : public ::testing::Test {
protected:
	void SetUp() override { PacketRecorder::instance().clear(); }

	/** Waits until the recorder holds count packets. */
	static bool waitForRecords(size_t count) {
		return waitUntil([count] { return PacketRecorder::instance().size() >= count; });
	}
};

TEST(CryptGoldenTest, ServerCryptAndFakeClientAreInverse) {
	// hand-derived vector: base key 0x12345678 -> key bytes 78 56 34 12 A1 6C 54 87; plain 01 02 03 -> cipher 79 66 1E
	Crypt server;
	const int32_t sentKey = server.enableKey(0x12345678);
	EXPECT_EQ(static_cast<uint32_t>(sentKey), 0x1F997F76u); // (key ^ 0xCD92E4DF) + 0x3FF2CCCF
	EXPECT_EQ(FakeGameClientCrypto::decipherKey(sentKey), 0x12345678);

	FakeGameClientCrypto client;
	client.setBaseKey(FakeGameClientCrypto::decipherKey(sentKey));

	std::vector<uint8_t> keyPacket{0xAA, 0xBB};
	server.encrypt(keyPacket); // the first packet (SM_KEY) is only enabling the crypt
	EXPECT_EQ(keyPacket, (std::vector<uint8_t>{0xAA, 0xBB}));
	EXPECT_TRUE(server.isEnabled());

	std::vector<uint8_t> body{1, 2, 3};
	server.encrypt(body);
	EXPECT_EQ(body, (std::vector<uint8_t>{0x79, 0x66, 0x1E}));
	client.decryptServerBody(body);
	EXPECT_EQ(body, (std::vector<uint8_t>{1, 2, 3}));

	// both keys advanced by 3: the next server packet decrypts too
	std::vector<uint8_t> second{9, 8, 7, 6, 5, 4, 3, 2, 1};
	std::vector<uint8_t> plain = second;
	server.encrypt(second);
	client.decryptServerBody(second);
	EXPECT_EQ(second, plain);

	// client -> server: a valid client packet body [wire][0x65][~wire] decrypts and advances the server's client key
	const uint16_t wire = FakeGameClientCrypto::clientWireOpcode(CM_TIME_CHECK);
	std::vector<uint8_t> clientBody = PacketWriter().H(wire).C(0x65).H(static_cast<uint16_t>(~wire)).D(1234).data;
	std::vector<uint8_t> clientPlain = clientBody;
	client.encryptClientBody(clientBody, true);
	EXPECT_NE(clientBody, clientPlain);
	EXPECT_TRUE(server.decrypt(clientBody));
	EXPECT_EQ(clientBody, clientPlain);
	EXPECT_EQ(Crypt::decodeClientPacketOpcode(wire), CM_TIME_CHECK);
}

TEST_F(AionConnectionTest, KeyExchangeThenEncryptedTrafficInBothDirections) {
	GameServerTestServer server;
	FakeGameClient client(server.port);
	auto frame = client.socket.readFrame();
	ASSERT_TRUE(frame);
	// SM_KEY: 2 + 5 + 4 bytes, unencrypted, opcode 72
	ASSERT_EQ(frame->size(), 11u);
	FakeGameClient::ServerPacket key = FakeGameClient::parseServerFrame(*frame);
	EXPECT_EQ(key.opcode, 72);
	client.crypto.setBaseKey(FakeGameClientCrypto::decipherKey(PacketReader(key.data).D()));
	client.keyReceived = true;

	auto connection = server.connection();
	EXPECT_EQ(connection->getState(), State::CONNECTED);

	client.sendPacket(CM_TIME_CHECK, PacketWriter().D(0x01020304).data);
	ASSERT_TRUE(waitForRecords(1));
	EXPECT_EQ(PacketRecorder::instance().snapshot()[0].opcode, CM_TIME_CHECK);
	EXPECT_EQ(PacketRecorder::instance().snapshot()[0].data, (PacketWriter().D(0x01020304).data));

	connection->setState(State::AUTHED);
	client.sendPacket(CM_PING, PacketWriter().S("echo").data);
	FakeGameClient::ServerPacket echo = client.expectPacket(RecordingClientPacket::ECHO_RESPONSE_OPCODE);
	EXPECT_EQ(PacketReader(echo.data).S(), "echo");

	// many packets keep their order on the connection and every key stays in sync
	for (int32_t i = 0; i < 40; i++)
		client.sendPacket(CM_PING, PacketWriter().D(i).data);
	for (int32_t i = 0; i < 40; i++) {
		FakeGameClient::ServerPacket response = client.expectPacket(RecordingClientPacket::ECHO_RESPONSE_OPCODE);
		ASSERT_EQ(PacketReader(response.data).D(), i);
	}
}

TEST_F(AionConnectionTest, ThreeCorruptPacketsDisconnect) {
	LogCapture logs({CONNECTION_LOGGER});
	GameServerTestServer server;
	FakeGameClient client(server.port);
	client.readKey();

	client.sendCorruptPacket();
	client.sendCorruptPacket();
	// the server did not advance its client key for the corrupt packets: a valid packet still decrypts
	client.sendPacket(CM_TIME_CHECK, PacketWriter().D(7).data);
	ASSERT_TRUE(waitForRecords(1)) << logs.dump();
	EXPECT_FALSE(client.socket.isClosed());
	EXPECT_FALSE(logs.contains("times, disconnecting"));

	client.sendCorruptPacket();
	EXPECT_TRUE(client.socket.waitClosed()) << logs.dump();
	EXPECT_TRUE(logs.waitFor("Client packet decryption failed 3 times, disconnecting AionConnection [state=CONNECTED")) << logs.dump();
	EXPECT_EQ(logs.count("Decrypt fail, client packet passed"), 2) << logs.dump();
}

TEST_F(AionConnectionTest, DataBeforeTheKeyIsDropped) {
	LogCapture logs({CONNECTION_LOGGER, "com.aionemu.commons.network.Dispatcher"});
	GameServerTestServer server(false); // the connection never sends SM_KEY, so its crypt stays disabled
	FakeGameClient client(server.port);
	for (int i = 0; i < 5; i++)
		client.sendCorruptPacket(32);
	client.sendRaw(PacketWriter().H(9).C(1).C(2).C(3).C(4).C(5).C(6).C(7).data);
	EXPECT_FALSE(client.socket.waitClosed(500ms)) << logs.dump();
	EXPECT_EQ(PacketRecorder::instance().size(), 0u);
	EXPECT_FALSE(logs.contains("Decrypt fail")) << logs.dump();
	EXPECT_FALSE(logs.contains("decryption failed")) << logs.dump();
	EXPECT_FALSE(client.readPacket(200ms).has_value()); // and nothing was sent
}

TEST_F(AionConnectionTest, PacketsAreGatedByTheConnectionState) {
	LogCapture logs({FACTORY_LOGGER});
	GameServerTestServer server;
	FakeGameClient client(server.port);
	client.readKey();
	auto connection = server.connection();

	client.sendPacket(CM_ENTER_WORLD, PacketWriter().D(1).data); // AUTHED only
	EXPECT_TRUE(logs.waitFor("sent CM_ENTER_WORLD but the connections current state (CONNECTED) is invalid for this packet.")) << logs.dump();
	client.sendPacket(CM_VERSION_CHECK, PacketWriter().D(2).data); // CONNECTED only
	ASSERT_TRUE(waitForRecords(1));
	EXPECT_EQ(PacketRecorder::instance().snapshot()[0].opcode, CM_VERSION_CHECK);

	connection->setState(State::AUTHED);
	client.sendPacket(CM_VERSION_CHECK, PacketWriter().D(3).data);
	EXPECT_TRUE(logs.waitFor("sent CM_VERSION_CHECK but the connections current state (AUTHED) is invalid for this packet.")) << logs.dump();
	client.sendPacket(CM_ENTER_WORLD, PacketWriter().D(4).data);
	ASSERT_TRUE(waitForRecords(2));
	EXPECT_EQ(PacketRecorder::instance().snapshot()[1].opcode, CM_ENTER_WORLD);

	// unknown opcodes are logged with the decoded opcode and the state (1 has no packet in Java's table)
	client.sendPacket(1, PacketWriter().D(5).data);
	EXPECT_TRUE(logs.waitFor("Aion client sent data with unknown opcode: 0x001, state=AUTHED")) << logs.dump();
	EXPECT_EQ(PacketRecorder::instance().size(), 2u);
	EXPECT_FALSE(client.socket.isClosed());

	// a packet whose state became invalid before it runs is not executed (AionClientPacket::isValid)
	RecordingClientPacket stale(CM_VERSION_CHECK, aion::StateSet{State::CONNECTED});
	stale.setConnection(connection);
	stale.run();
	EXPECT_EQ(PacketRecorder::instance().size(), 2u);
}

TEST_F(AionConnectionTest, QueueIsOrderedBySerializationSequence) {
	GameServerTestServer server;
	FakeGameClient client(server.port);
	client.readKey();
	auto connection = server.connection();

	TestServerPacket first(101, PacketWriter().D(1).data);
	TestServerPacket second(102, PacketWriter().D(2).data);
	aion::SerializedBody firstBody = first.serialize(nullptr);
	aion::SerializedBody secondBody = second.serialize(nullptr);
	ASSERT_LT(firstBody.seq, secondBody.seq);
	{
		// hold the queue so both bodies are inserted before the IO strand writes
		std::lock_guard lock(connection->guard);
		connection->enqueue(secondBody);
		connection->enqueue(firstBody);
		ASSERT_EQ(connection->getSendMsgQueue().size(), 2u);
		EXPECT_EQ(connection->getSendMsgQueue().front().opCode, 101);
	}
	EXPECT_EQ(client.expectPacket(101).data, (PacketWriter().D(1).data));
	EXPECT_EQ(client.expectPacket(102).data, (PacketWriter().D(2).data));
}

TEST_F(AionConnectionTest, ClosePacketIsSentBeforeTheConnectionCloses) {
	LogCapture logs({CONNECTION_LOGGER});
	GameServerTestServer server;
	FakeGameClient client(server.port);
	client.readKey();
	auto connection = server.connection();

	connection->close(TestServerPacket(120, PacketWriter().S("bye").data));
	connection->sendPacket(TestServerPacket(121, {})); // ignored: the connection is closing
	FakeGameClient::ServerPacket bye = client.expectPacket(120);
	EXPECT_EQ(PacketReader(bye.data).S(), "bye");
	EXPECT_TRUE(client.socket.waitClosed());
	EXPECT_TRUE(logs.waitFor("Client disconnected: AionConnection [state=CONNECTED, account=null, activePlayer=null, macAddress=null, getIP()=127.0.0.1]"))
		<< logs.dump();
}

TEST_F(AionConnectionTest, FloodingClientsAreDisconnectedInPffMode1) {
	using configs::network::PffConfig;
	LogCapture logs({CONNECTION_LOGGER});
	PffConfig::PFF_MODE = 1;
	PffConfig::THRESHOLD_MILLIS_BY_PACKET_OPCODE.set(std::unordered_map<int32_t, int32_t>{{CM_TIME_CHECK, 60000}});
	struct Restore {
		~Restore() {
			PffConfig::PFF_MODE = 0;
			PffConfig::THRESHOLD_MILLIS_BY_PACKET_OPCODE.set({});
		}
	} restore;

	GameServerTestServer server;
	FakeGameClient client(server.port);
	client.readKey();
	client.sendPacket(CM_VERSION_CHECK, {}); // no threshold for this opcode
	client.sendPacket(CM_TIME_CHECK, PacketWriter().D(1).data);
	ASSERT_TRUE(waitForRecords(2));
	client.sendPacket(CM_TIME_CHECK, PacketWriter().D(2).data);
	EXPECT_TRUE(client.socket.waitClosed()) << logs.dump();
	EXPECT_TRUE(logs.waitFor(" is flooding RecordingClientPacket (last diff: ")) << logs.dump();
	EXPECT_EQ(PacketRecorder::instance().size(), 2u);
}

TEST_F(AionConnectionTest, ConnectionStateAccessors) {
	GameServerTestServer server;
	FakeGameClient client(server.port);
	client.readKey();
	auto connection = server.connection();
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));

	EXPECT_FALSE(connection->getAccount());
	EXPECT_FALSE(connection->getActivePlayer());
	EXPECT_TRUE(connection->setActivePlayer(nullptr)); // Java: leaving the world sets AUTHED
	EXPECT_EQ(connection->getState(), State::AUTHED);
	connection->setMacAddress("00-11-22-33-44-55");
	connection->setHddSerial("serial");
	EXPECT_EQ(connection->getMacAddress(), "00-11-22-33-44-55");
	EXPECT_EQ(connection->getHddSerial(), "serial");
	EXPECT_EQ(connection->increaseAndGetPingFailCount(), 1);
	EXPECT_EQ(connection->increaseAndGetPingFailCount(), 2);
	connection->resetPingFailCount();
	EXPECT_EQ(connection->increaseAndGetPingFailCount(), 1);
	connection->setLastPingTime(1234);
	EXPECT_EQ(connection->getLastPingTime(), 1234);
	EXPECT_GT(connection->getLastClientMessageTime(), 0);
	EXPECT_EQ(connection->toString(), "AionConnection [state=AUTHED, account=null, activePlayer=null, macAddress=00-11-22-33-44-55, getIP()=127.0.0.1]");
}

} // namespace
} // namespace aion::gameserver::network::test
