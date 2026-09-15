// AionServerPacket: eager serialization into the thread-local buffer (runtime-architecture.md §8.2), the obfuscated opcode header of all 237
// server packets, the Java write helpers (fixed strings, dye info) and PacketWriteHelper.

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/network/Crypt.h"
#include "aion/gameserver/network/PacketWriteHelper.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/SerializedBody.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "FakeGameClient.h"
#include "support/GameServerTestServer.h"

namespace aion::gameserver::network::test {
namespace {

using aion::AionServerPacket;
using aion::SerializedBody;

/** Exposes the protected Java write helpers through writeImpl steps */
class HelperPacket : public AionServerPacket {
public:
	enum class Step { FIXED_STRING, EMPTY_FIXED_STRING, DYE, NO_DYE, NESTED };

	HelperPacket(Step step, std::string text = {}, int32_t fixedLength = 0, std::optional<int32_t> rgb = std::nullopt)
		: AionServerPacket(25), step(step), text(std::move(text)), fixedLength(fixedLength), rgb(rgb) {}

protected:
	void writeImpl(aion::AionConnection*) override {
		switch (step) {
			case Step::FIXED_STRING:
			case Step::EMPTY_FIXED_STRING:
				writeS(text, fixedLength);
				break;
			case Step::DYE:
			case Step::NO_DYE:
				writeDyeInfo(rgb);
				break;
			case Step::NESTED: {
				writeD(0x11111111);
				TestServerPacket inner(7, {0xAB, 0xCD});
				nestedBody = inner.serialize(nullptr);
				writeD(0x22222222); // the outer buffer is unchanged by the nested serialization
				break;
			}
		}
	}

public:
	SerializedBody nestedBody;

private:
	Step step;
	std::string text;
	int32_t fixedLength;
	std::optional<int32_t> rgb;
};

std::vector<uint8_t> dataOf(const SerializedBody& body) {
	return std::vector<uint8_t>(body.bytes->begin() + 5, body.bytes->end());
}

TEST(AionServerPacketTest, SerializeWritesTheObfuscatedHeaderAndTheData) {
	TestServerPacket packet(aion::opcodeOf<aion::serverpackets::SM_SYSTEM_MESSAGE>, {1, 2, 3});
	SerializedBody body = packet.serialize(nullptr);
	// Java write(): [short length][short (opcode + 207) ^ 0xDF][byte 0x44][short ~op][data]; the length is added by AionConnection::writeData
	const int32_t op = Crypt::encodeServerPacketOpcode(25);
	EXPECT_EQ(op, (25 + 207) ^ 0xDF);
	EXPECT_EQ(*body.bytes, (PacketWriter().H(op).C(0x44).H(~op).C(1).C(2).C(3).data));
	EXPECT_EQ(body.opCode, 25);
	EXPECT_FALSE(body.enablesCrypt);
}

TEST(AionServerPacketTest, SequenceNumbersGrowAcrossThreads) {
	TestServerPacket packet(1, {});
	uint64_t first = packet.serialize(nullptr).seq;
	uint64_t other = 0;
	std::thread thread([&] { other = packet.serialize(nullptr).seq; });
	thread.join();
	uint64_t last = packet.serialize(nullptr).seq;
	EXPECT_LT(first, other);
	EXPECT_LT(other, last);
}

TEST(AionServerPacketTest, PerRecipientPacketsGetTheirConnection) {
	TestServerPacket shared(1, {}, false);
	shared.serialize(nullptr);
	EXPECT_EQ(shared.lastConnection, nullptr);
	EXPECT_EQ(shared.recipients(), AionServerPacket::Recipients::SHARED);
	TestServerPacket perRecipient(1, {}, true);
	EXPECT_EQ(perRecipient.recipients(), AionServerPacket::Recipients::PER_RECIPIENT);
}

TEST(AionServerPacketTest, AllServerOpcodesAreObfuscatedLikeTheGeneratedTable) {
	std::set<uint16_t> wires;
	for (const aion::ServerPacketsOpcodes::Entry& entry : aion::ServerPacketsOpcodes::ENTRIES) {
		TestServerPacket packet(entry.opcode, {});
		SerializedBody body = packet.serialize(nullptr);
		ASSERT_EQ(body.bytes->size(), 5u) << entry.name;
		const uint16_t wire = static_cast<uint16_t>((*body.bytes)[0] | (*body.bytes)[1] << 8);
		EXPECT_EQ(wire, entry.wireOpcode) << entry.name;
		EXPECT_EQ(wire, FakeGameClientCrypto::serverWireOpcode(entry.opcode)) << entry.name;
		EXPECT_EQ((*body.bytes)[2], 0x44) << entry.name;
		EXPECT_EQ(static_cast<uint16_t>((*body.bytes)[3] | (*body.bytes)[4] << 8), static_cast<uint16_t>(~wire)) << entry.name;
		// the client's decoding of the frame header gives the opcode back
		std::vector<uint8_t> frame{0, 0};
		frame.insert(frame.end(), body.bytes->begin(), body.bytes->end());
		EXPECT_EQ(FakeGameClient::parseServerFrame(frame).opcode, entry.opcode) << entry.name;
		EXPECT_TRUE(wires.insert(wire).second) << "duplicate wire opcode of " << entry.name;
	}
	EXPECT_EQ(wires.size(), 237u);
}

TEST(AionServerPacketTest, FixedLengthStrings) {
	// Java writeS(text, fixedLength): fixedLength chars (truncated or zero padded) plus the terminating char
	HelperPacket truncated(HelperPacket::Step::FIXED_STRING, "abcdef", 4);
	EXPECT_EQ(dataOf(truncated.serialize(nullptr)), (PacketWriter().H('a').H('b').H('c').H('d').H(0).data));
	HelperPacket padded(HelperPacket::Step::FIXED_STRING, "ab", 4);
	EXPECT_EQ(dataOf(padded.serialize(nullptr)), (PacketWriter().H('a').H('b').H(0).H(0).H(0).data));
	// null or empty: byteLengthForFixedString(fixedLength) zero bytes
	HelperPacket empty(HelperPacket::Step::EMPTY_FIXED_STRING, "", 3);
	EXPECT_EQ(dataOf(empty.serialize(nullptr)), std::vector<uint8_t>(8));
	EXPECT_EQ(AionServerPacket::byteLengthForFixedString(3), 8);
	EXPECT_EQ(AionServerPacket::byteLengthForString(""), 2);
	EXPECT_EQ(AionServerPacket::byteLengthForString("abc"), 8);
	EXPECT_EQ(AionServerPacket::byteLengthForString("\xF0\x9F\x98\x80"), 6); // a surrogate pair counts as two chars
}

TEST(AionServerPacketTest, DyeInfo) {
	HelperPacket dyed(HelperPacket::Step::DYE, {}, 0, 0x123456);
	EXPECT_EQ(dataOf(dyed.serialize(nullptr)), (std::vector<uint8_t>{1, 0x12, 0x34, 0x56}));
	HelperPacket undyed(HelperPacket::Step::NO_DYE);
	EXPECT_EQ(dataOf(undyed.serialize(nullptr)), (std::vector<uint8_t>{0, 0, 0, 0}));
}

TEST(AionServerPacketTest, NestedSerializationUsesItsOwnBuffer) {
	HelperPacket outer(HelperPacket::Step::NESTED);
	SerializedBody body = outer.serialize(nullptr);
	EXPECT_EQ(dataOf(body), (PacketWriter().D(0x11111111).D(0x22222222).data));
	EXPECT_EQ(dataOf(outer.nestedBody), (std::vector<uint8_t>{0xAB, 0xCD}));
	EXPECT_THROW(AionServerPacket::getBuf(), commons::utils::IllegalStateException);
}

TEST(AionServerPacketTest, OversizedPacketsThrowLikeAFullWriteBuffer) {
	TestServerPacket packet(1, std::vector<uint8_t>(8192 * 4));
	EXPECT_THROW(packet.serialize(nullptr), commons::utils::BufferOverflowException);
	// the buffer stack was unwound: the next serialization works
	TestServerPacket small(1, {9});
	EXPECT_EQ(small.serialize(nullptr).bytes->size(), 6u);
}

/** Calls the protected static PacketWriteHelper helpers */
struct WriteHelper : PacketWriteHelper {
	using PacketWriteHelper::skip;
	using PacketWriteHelper::writeC;
	using PacketWriteHelper::writeD;
	using PacketWriteHelper::writeDyeInfo;
	using PacketWriteHelper::writeH;
	using PacketWriteHelper::writeQ;
	using PacketWriteHelper::writeS;
};

TEST(PacketWriteHelperTest, WritesLikeJava) {
	commons::utils::ByteBuffer buf = commons::utils::ByteBuffer::allocate(64);
	WriteHelper::writeC(buf, 0x1FF);
	WriteHelper::writeH(buf, 0x12345);
	WriteHelper::writeD(buf, -2);
	WriteHelper::writeQ(buf, 0x0102030405060708LL);
	WriteHelper::writeS(buf, "hi");
	WriteHelper::writeS(buf, "ab", 8); // chars, then size - chars * 2 zero bytes
	WriteHelper::writeS(buf, "", 3);
	WriteHelper::writeDyeInfo(buf, std::nullopt);
	WriteHelper::writeDyeInfo(buf, 0xAABBCC);
	buf.flip();
	std::vector<uint8_t> written(buf.remainingSpan().begin(), buf.remainingSpan().end());
	EXPECT_EQ(written, (PacketWriter()
													 .C(0xFF)
													 .H(0x2345)
													 .D(-2)
													 .Q(0x0102030405060708LL)
													 .S("hi")
													 .H('a')
													 .H('b')
													 .zeros(4)
													 .zeros(3)
													 .zeros(4)
													 .C(1)
													 .C(0xAA)
													 .C(0xBB)
													 .C(0xCC)
													 .data));
	commons::utils::ByteBuffer longer = commons::utils::ByteBuffer::allocate(32);
	EXPECT_THROW(WriteHelper::writeS(longer, "abcdef", 4), commons::utils::IllegalArgumentException); // Java: NegativeArraySizeException
}

} // namespace
} // namespace aion::gameserver::network::test
