// AionClientPacketFactory: the opcode table built from ClientPacketInfo.gen.inc and the AION_CLIENT_PACKET registry, packet creation and state
// gating for all 186 client opcodes (wire opcode -> Crypt.decodeClientPacketOpcode -> packet), and classes without a C++ port.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/network/Crypt.h"
#include "aion/gameserver/network/aion/AionClientPacket.h"
#include "aion/gameserver/network/aion/AionClientPacketFactory.h"
#include "FakeGameClient.h"
#include "support/GameServerTestServer.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::test {
namespace {

using aion::AionClientPacketFactory;
using State = aion::AionConnection::State;

struct GeneratedPacket {
	int32_t opcode;
	uint16_t wireOpcode;
	std::string_view name;
	aion::StateSet states;
};

std::vector<GeneratedPacket> generatedPackets() {
	using enum aion::AionConnection_State;
	std::vector<GeneratedPacket> packets;
#define AION_CLIENT_PACKET_INFO(opcode, wireOpcode, Class, clientName, ...) packets.push_back({opcode, wireOpcode, #Class, aion::StateSet{__VA_ARGS__}});
#include "aion/gameserver/network/aion/ClientPacketInfo.gen.inc"
#undef AION_CLIENT_PACKET_INFO
	return packets;
}

/** The decrypted body of a client packet: [wire opcode][0x65][~wire opcode][data] */
std::vector<uint8_t> clientBody(uint16_t wireOpcode, std::span<const uint8_t> data = {}) {
	return PacketWriter().H(wireOpcode).C(0x65).H(static_cast<uint16_t>(~wireOpcode)).B(data).data;
}

TEST(AionClientPacketFactoryTest, TableMatchesTheGeneratedPacketInfo) {
	AionClientPacketFactory::setEntries(recordingPacketEntries());
	const std::vector<GeneratedPacket> packets = generatedPackets();
	ASSERT_EQ(packets.size(), 186u);
	std::vector<bool> slotUsed(250);
	for (const GeneratedPacket& packet : packets) {
		const AionClientPacketFactory::PacketInfo* info = AionClientPacketFactory::getPacketInfo(packet.opcode);
		ASSERT_NE(info, nullptr) << packet.name;
		EXPECT_EQ(info->getPacketClassName(), packet.name);
		EXPECT_EQ(info->validStates, packet.states) << packet.name;
		EXPECT_NE(info->packetFactory, nullptr) << packet.name;
		EXPECT_EQ(Crypt::decodeClientPacketOpcode(packet.wireOpcode), packet.opcode) << packet.name;
		EXPECT_EQ(FakeGameClientCrypto::clientWireOpcode(packet.opcode), packet.wireOpcode) << packet.name;
		slotUsed[static_cast<size_t>(packet.opcode)] = true;
	}
	for (int32_t opcode = 0; opcode < 250; opcode++) {
		if (!slotUsed[static_cast<size_t>(opcode)])
			EXPECT_EQ(AionClientPacketFactory::getPacketInfo(opcode), nullptr) << opcode;
	}
	EXPECT_EQ(AionClientPacketFactory::getPacketInfo(-1), nullptr);
	EXPECT_EQ(AionClientPacketFactory::getPacketInfo(250), nullptr);
	// spot checks against the Java static initializer
	EXPECT_EQ(AionClientPacketFactory::getPacketInfo(0)->validStates, (aion::StateSet{State::CONNECTED}));       // CM_VERSION_CHECK
	EXPECT_EQ(AionClientPacketFactory::getPacketInfo(44)->validStates, (aion::StateSet{State::IN_GAME, State::AUTHED})); // CM_PING
	EXPECT_EQ(AionClientPacketFactory::getPacketInfo(18)->validStates.size(), 3);                                     // CM_TIME_CHECK
}

TEST(AionClientPacketFactoryTest, CreatesEveryPacketOnlyInItsStates) {
	LogCapture logs({"com.aionemu.gameserver.network.aion.AionClientPacketFactory"});
	GameServerTestServer server;
	TestSocket socket(server.port);
	auto connection = server.connection();

	const std::vector<uint8_t> data = PacketWriter().D(0x55667788).data;
	int created = 0;
	int rejected = 0;
	for (const GeneratedPacket& packet : generatedPackets()) {
		for (State state : {State::CONNECTED, State::AUTHED, State::IN_GAME}) {
			connection->setState(state);
			std::vector<uint8_t> body = clientBody(packet.wireOpcode, data);
			commons::utils::ByteBuffer buffer = commons::utils::ByteBuffer::wrap(body);
			std::unique_ptr<aion::AionClientPacket> created_packet = AionClientPacketFactory::tryCreatePacket(buffer, connection.get());
			if (packet.states.contains(state)) {
				ASSERT_TRUE(created_packet) << packet.name << " in state " << static_cast<int>(state);
				EXPECT_EQ(created_packet->getOpCode(), packet.opcode);
				EXPECT_EQ(created_packet->getConnection(), connection);
				EXPECT_EQ(created_packet->getRemainingBytes(), 4) << "the header is skipped, the data is attached";
				EXPECT_TRUE(created_packet->isValid());
				created++;
			} else {
				EXPECT_FALSE(created_packet) << packet.name << " in state " << static_cast<int>(state);
				rejected++;
			}
		}
	}
	EXPECT_EQ(created + rejected, 186 * 3);
	EXPECT_EQ(logs.count("is invalid for this packet. Packet won't be instantiated."), rejected);
}

TEST(AionClientPacketFactoryTest, UnknownOpcodesAndUnportedClasses) {
	LogCapture logs({"com.aionemu.gameserver.network.aion.AionClientPacketFactory"});
	GameServerTestServer server;
	TestSocket socket(server.port);
	auto connection = server.connection();

	std::vector<uint8_t> unknown = clientBody(FakeGameClientCrypto::clientWireOpcode(249)); // [C_REQ_REGISTER_MONEY_TRADE] is commented out
	commons::utils::ByteBuffer unknownBuffer = commons::utils::ByteBuffer::wrap(unknown);
	EXPECT_FALSE(AionClientPacketFactory::tryCreatePacket(unknownBuffer, connection.get()));
	EXPECT_TRUE(logs.contains("Aion client sent data with unknown opcode: 0x0F9, state=CONNECTED")) << logs.dump();

	// a wire value that decodes outside the table
	std::vector<uint8_t> outside = clientBody(0xFFFF);
	commons::utils::ByteBuffer outsideBuffer = commons::utils::ByteBuffer::wrap(outside);
	EXPECT_FALSE(AionClientPacketFactory::tryCreatePacket(outsideBuffer, connection.get()));
	const std::string decoded = fmt::format("0x{:03X}", static_cast<uint32_t>(Crypt::decodeClientPacketOpcode(0xFFFF)));
	EXPECT_TRUE(logs.contains("unknown opcode: " + decoded)) << logs.dump();

	// no AION_CLIENT_PACKET registration (the C++ packet is not ported yet): logged once per class
	AionClientPacketFactory::setEntries({});
	for (int i = 0; i < 2; i++) {
		std::vector<uint8_t> version = clientBody(FakeGameClientCrypto::clientWireOpcode(0));
		commons::utils::ByteBuffer versionBuffer = commons::utils::ByteBuffer::wrap(version);
		EXPECT_FALSE(AionClientPacketFactory::tryCreatePacket(versionBuffer, connection.get()));
	}
	EXPECT_EQ(logs.count("sent CM_VERSION_CHECK, which is not ported yet."), 1) << logs.dump();
	// the classes of those warnings, once each (header request 5a-pre-7)
	for (int i = 0; i < 2; i++) {
		std::vector<uint8_t> ping = clientBody(FakeGameClientCrypto::clientWireOpcode(44)); // CM_PING, valid in AUTHED and IN_GAME
		commons::utils::ByteBuffer pingBuffer = commons::utils::ByteBuffer::wrap(ping);
		connection->setState(State::AUTHED);
		EXPECT_FALSE(AionClientPacketFactory::tryCreatePacket(pingBuffer, connection.get()));
		connection->setState(State::CONNECTED);
	}
	std::vector<std::string> seen = AionClientPacketFactory::unportedPacketClassesSeen();
	EXPECT_EQ(std::ranges::count(seen, "CM_VERSION_CHECK"), 1);
	EXPECT_EQ(std::ranges::count(seen, "CM_PING"), 1);
	EXPECT_TRUE(std::ranges::is_sorted(seen));
	EXPECT_EQ(std::ranges::count(seen, "CM_QUIT"), 0) << "a class no client sent is not listed";
	AionClientPacketFactory::setEntries(recordingPacketEntries());
}

TEST(AionClientPacketTest, ReadFixedLengthStrings) {
	// Java readS(characterCount): reads the string, then skips the rest of the fixed size (characters * 2 + 2 bytes in total)
	struct FixedStringPacket : aion::AionClientPacket {
		FixedStringPacket() : AionClientPacket(0, aion::StateSet{State::CONNECTED}) {}
		std::string first, second;
		int32_t after = 0;
		void readImpl() override {
			first = readS(5);
			second = readS(2);
			after = readD();
		}
		void runImpl() override {}
	};
	std::vector<uint8_t> body = PacketWriter().S("ab").zeros(6).S("xy").D(42).data;
	FixedStringPacket packet;
	packet.setBuffer(commons::utils::ByteBuffer::wrap(body));
	ASSERT_TRUE(packet.read());
	EXPECT_EQ(packet.first, "ab");
	EXPECT_EQ(packet.second, "xy");
	EXPECT_EQ(packet.after, 42);
	EXPECT_EQ(packet.getRemainingBytes(), 0);
}

} // namespace
} // namespace aion::gameserver::network::test
