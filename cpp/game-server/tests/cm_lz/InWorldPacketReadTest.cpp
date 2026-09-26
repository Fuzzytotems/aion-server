// readImpl byte vectors of the four P5-16 in-world client packets that need no service (m5a-plan.md D7/C-01, leased by P4-17 in stage 2):
// CM_MAY_QUIT, CM_PING_REQUEST, CM_SHOW_FRIENDLIST and CM_SUBZONE_CHANGE. Every body below is laid out field by field from the Java readImpl,
// and read() must consume it exactly (the base class logs "Missing X" on underflow). The generated opcode table is checked against the Java
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
#include "aion/gameserver/network/aion/clientpackets/CM_MAY_QUIT.h"
#include "aion/gameserver/network/aion/clientpackets/CM_PING_REQUEST.h"
#include "aion/gameserver/network/aion/clientpackets/CM_SHOW_FRIENDLIST.h"
#include "aion/gameserver/network/aion/clientpackets/CM_SUBZONE_CHANGE.h"
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
	// CM_MAY_QUIT, CM_PING_REQUEST, CM_SHOW_FRIENDLIST: empty bodies
	expectExactRead<CM_MAY_QUIT>({}, 220);
	expectExactRead<CM_PING_REQUEST>({}, 221);
	expectExactRead<CM_SHOW_FRIENDLIST>({}, 222);
	// CM_SUBZONE_CHANGE: C unk (always 1, maybe 0 for neutral zones)
	expectExactRead<CM_SUBZONE_CHANGE>(PacketWriter().C(1).data, 223);
	expectExactRead<CM_SUBZONE_CHANGE>(PacketWriter().C(0).data, 224);
}

TEST(InWorldPacketReadTest, ShortAndLongBodies) {
	{ // CM_SUBZONE_CHANGE without its byte
		LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
		EXPECT_EQ(unreadBytesAfterRead<CM_SUBZONE_CHANGE>({}, 230), 0);
		EXPECT_TRUE(capture.contains("Missing C")) << capture.dump();
	}
	{ // the empty-body packets read nothing, so trailing bytes stay unread
		LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
		EXPECT_EQ(unreadBytesAfterRead<CM_MAY_QUIT>(PacketWriter().D(1).data, 231), 4);
		EXPECT_EQ(unreadBytesAfterRead<CM_PING_REQUEST>(PacketWriter().C(1).data, 232), 1);
		EXPECT_EQ(unreadBytesAfterRead<CM_SHOW_FRIENDLIST>(PacketWriter().H(1).data, 233), 2);
		EXPECT_EQ(unreadBytesAfterRead<CM_SUBZONE_CHANGE>(PacketWriter().C(1).C(2).data, 234), 1);
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
	// AionClientPacketFactory.java:32, :131, :191, :258
	const std::pair<std::string_view, int32_t> expected[]{
		{"CM_MAY_QUIT", 4}, {"CM_PING_REQUEST", 103}, {"CM_SUBZONE_CHANGE", 163}, {"CM_SHOW_FRIENDLIST", 230}};
	for (const auto& [name, opcode] : expected) {
		const TableEntry* entry = entryOf(packets, name);
		ASSERT_NE(entry, nullptr) << name;
		EXPECT_EQ(entry->opcode, opcode) << name;
		EXPECT_EQ(entry->states, (StateSet{IN_GAME})) << name;
	}
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets
