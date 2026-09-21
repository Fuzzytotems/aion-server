// readImpl byte vectors of the three P5-15 in-world client packets that need no service (m5a-plan.md D7/C-01, leased by P4-16 in stage 2):
// CM_CHECK_MAIL_UNK, CM_CUSTOM_SETTINGS and CM_CHAT_AUTH. Every body below is laid out field by field from the Java readImpl, and read() must
// consume it exactly (the base class logs "Missing X" on underflow). The generated opcode table is checked against the Java
// AionClientPacketFactory static initializer for these classes.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/network/aion/StateSet.h"
#include "aion/gameserver/network/aion/clientpackets/CM_CHAT_AUTH.h"
#include "aion/gameserver/network/aion/clientpackets/CM_CHECK_MAIL_UNK.h"
#include "aion/gameserver/network/aion/clientpackets/CM_CUSTOM_SETTINGS.h"
#include "../support/NetworkTestSupport.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {
namespace {

using test::LogCapture;
using test::PacketWriter;

const char* BASE_CLIENT_PACKET_LOGGER = "com.aionemu.commons.network.packet.BaseClientPacket";

/** Reads `data` with a fresh packet of type P (no connection) and returns the unread byte count, -1 if read() failed */
template <class P>
int32_t unreadBytesAfterRead(const std::vector<uint8_t>& data, int32_t opcode = 1) {
	std::vector<uint8_t> copy = data;
	auto packet = std::make_unique<P>(opcode, StateSet{AionConnection_State::IN_GAME});
	packet->setBuffer(commons::utils::ByteBuffer::wrap(copy));
	if (!packet->read())
		return -1;
	return packet->getRemainingBytes();
}

/** Asserts that P consumes `data` exactly and logs no missing field */
template <class P>
void expectExactRead(const std::vector<uint8_t>& data, int32_t opcode) {
	LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
	EXPECT_EQ(unreadBytesAfterRead<P>(data, opcode), 0) << typeid(P).name();
	EXPECT_FALSE(capture.contains("Missing")) << capture.dump();
}

TEST(InWorldPacketReadTest, NoServicePacketBodies) {
	// CM_CHECK_MAIL_UNK: empty body
	expectExactRead<CM_CHECK_MAIL_UNK>({}, 200);
	// CM_CUSTOM_SETTINGS: UH display, UH deny
	expectExactRead<CM_CUSTOM_SETTINGS>(PacketWriter().H(0x000F).H(0x0021).data, 201);
	expectExactRead<CM_CUSTOM_SETTINGS>(PacketWriter().H(0xFFFF).H(0).data, 202);
	// CM_CHAT_AUTH: D object id, byte[6] mac address
	expectExactRead<CM_CHAT_AUTH>(PacketWriter().D(0x12345678).C(0x0A).C(0x1B).C(0x2C).C(0x3D).C(0x4E).C(0x5F).data, 203);
}

TEST(InWorldPacketReadTest, ShortBodiesLogTheMissingField) {
	{ // CM_CUSTOM_SETTINGS: the second unsigned short is missing (one byte short); the failed getShort consumes nothing
		LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
		EXPECT_EQ(unreadBytesAfterRead<CM_CUSTOM_SETTINGS>(PacketWriter().H(1).C(0).data, 210), 1);
		EXPECT_TRUE(capture.contains("Missing H")) << capture.dump();
	}
	{ // CM_CHAT_AUTH: readB(6) underflows, and (Java ByteBuffer.get(byte[])) consumes nothing
		LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
		EXPECT_EQ(unreadBytesAfterRead<CM_CHAT_AUTH>(PacketWriter().D(1).C(0).C(0).C(0).C(0).C(0).data, 211), 5);
		EXPECT_TRUE(capture.contains("Missing byte[]")) << capture.dump();
	}
	{ // a body longer than the layout leaves the extra bytes unread
		LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
		EXPECT_EQ(unreadBytesAfterRead<CM_CHECK_MAIL_UNK>(PacketWriter().D(1).data, 212), 4);
		EXPECT_FALSE(capture.contains("Missing")) << capture.dump();
	}
}

struct TableEntry {
	int32_t opcode;
	std::string_view name;
	StateSet states;
};

std::vector<TableEntry> generatedPackets() {
	using enum AionConnection_State;
	std::vector<TableEntry> packets;
#define AION_CLIENT_PACKET_INFO(opcode, wireOpcode, Class, clientName, ...) packets.push_back({opcode, #Class, StateSet{__VA_ARGS__}});
#include "aion/gameserver/network/aion/ClientPacketInfo.gen.inc"
#undef AION_CLIENT_PACKET_INFO
	return packets;
}

/** The opcode and states of `name` in the generated table (ClientPacketInfo.gen.inc) */
const TableEntry* entryOf(const std::vector<TableEntry>& packets, std::string_view name) {
	auto it = std::ranges::find(packets, name, &TableEntry::name);
	return it == packets.end() ? nullptr : &*it;
}

TEST(InWorldPacketReadTest, OpcodeTableMatchesTheJavaFactory) {
	using enum AionConnection_State;
	const std::vector<TableEntry> packets = generatedPackets();
	// AionClientPacketFactory.java:40, :202, :241
	const std::pair<std::string_view, int32_t> expected[]{{"CM_CUSTOM_SETTINGS", 12}, {"CM_CHAT_AUTH", 174}, {"CM_CHECK_MAIL_UNK", 213}};
	for (const auto& [name, opcode] : expected) {
		const TableEntry* entry = entryOf(packets, name);
		ASSERT_NE(entry, nullptr) << name;
		EXPECT_EQ(entry->opcode, opcode) << name;
		EXPECT_EQ(entry->states, (StateSet{IN_GAME})) << name;
	}
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets
