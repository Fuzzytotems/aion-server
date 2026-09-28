#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <vector>

#include "aion/commons/network/packet/BaseClientPacket.h"
#include "aion/commons/network/packet/BaseServerPacket.h"

using namespace aion::commons;
using namespace aion::commons::network::packet;

namespace {

/** Exposes the protected static write helpers. */
class Writer : public BaseServerPacket {
public:
	using BaseServerPacket::writeB;
	using BaseServerPacket::writeC;
	using BaseServerPacket::writeD;
	using BaseServerPacket::writeDF;
	using BaseServerPacket::writeF;
	using BaseServerPacket::writeH;
	using BaseServerPacket::writeQ;
	using BaseServerPacket::writeS;
};

struct NoConnection {
	std::string toString() const { return "none"; }
};

class Reader : public BaseClientPacket<NoConnection> {
public:
	explicit Reader(utils::ByteBuffer buffer) : BaseClientPacket(std::move(buffer), 0) {}
	using BaseClientPacket::readS;

protected:
	void readImpl() override {}
	void runImpl() override {}
};

} // namespace

TEST(BaseServerPacketTest, WritesLittleEndianValues) {
	utils::ByteBuffer buf = utils::ByteBuffer::allocate(64);
	Writer::writeD(buf, 0x01020304);
	Writer::writeH(buf, 0x0506);
	Writer::writeC(buf, 0x07);
	Writer::writeQ(buf, 0x08090A0B0C0D0E0FLL);
	Writer::writeF(buf, 1.5f);
	Writer::writeDF(buf, -2.25);
	std::array<uint8_t, 3> bytes{0xAA, 0xBB, 0xCC};
	Writer::writeB(buf, bytes);
	buf.flip();

	EXPECT_EQ(buf.limit(), 4 + 2 + 1 + 8 + 4 + 8 + 3);
	std::vector<uint8_t> expectedStart{0x04, 0x03, 0x02, 0x01, 0x06, 0x05, 0x07, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08};
	for (size_t i = 0; i < expectedStart.size(); i++)
		EXPECT_EQ(static_cast<uint8_t>(buf.get(static_cast<int32_t>(i))), expectedStart[i]) << "index " << i;
	EXPECT_EQ(buf.getFloat(15), 1.5f);
	EXPECT_EQ(buf.getDouble(19), -2.25);
	EXPECT_EQ(static_cast<uint8_t>(buf.get(27)), 0xAA);
	EXPECT_EQ(static_cast<uint8_t>(buf.get(29)), 0xCC);
}

TEST(BaseServerPacketTest, WriteHAndWriteCTruncateLikeJavaCasts) {
	utils::ByteBuffer buf = utils::ByteBuffer::allocate(8);
	Writer::writeH(buf, 0x12345678);
	Writer::writeH(buf, -1);
	Writer::writeC(buf, 0x1FF);
	Writer::writeC(buf, -2);
	EXPECT_EQ(buf.getShort(0), 0x5678);
	EXPECT_EQ(buf.getShort(2), -1);
	EXPECT_EQ(static_cast<uint8_t>(buf.get(4)), 0xFF);
	EXPECT_EQ(buf.get(5), -2);
}

TEST(BaseServerPacketTest, WriteSEncodesUtf16LeWithTerminator) {
	utils::ByteBuffer buf = utils::ByteBuffer::allocate(32);
	Writer::writeS(buf, "Ab\xC3\xA4"); // "Abä"
	EXPECT_EQ(buf.position(), 8);
	EXPECT_EQ(buf.getChar(0), u'A');
	EXPECT_EQ(buf.getChar(2), u'b');
	EXPECT_EQ(buf.getChar(4), u'ä');
	EXPECT_EQ(buf.getChar(6), 0);

	utils::ByteBuffer empty = utils::ByteBuffer::allocate(4);
	Writer::writeS(empty, "");
	EXPECT_EQ(empty.position(), 2); // Java: writeS(null) or writeS("") write only the terminator
}

TEST(BaseServerPacketTest, WriteSRoundTripsThroughReadS) {
	const std::string texts[] = {"", "plain ascii", "Umlaute \xC3\xA4\xC3\xB6\xC3\xBC", "\xE4\xBD\xA0\xE5\xA5\xBD", "emoji \xF0\x9F\x98\x80 surrogates"};
	utils::ByteBuffer buf = utils::ByteBuffer::allocate(256);
	for (const auto& text : texts)
		Writer::writeS(buf, text);
	buf.flip();

	Reader reader(buf);
	for (const auto& text : texts)
		EXPECT_EQ(reader.readS(), text);
	EXPECT_EQ(reader.getRemainingBytes(), 0);
}

TEST(BaseServerPacketTest, OverflowThrows) {
	utils::ByteBuffer buf = utils::ByteBuffer::allocate(3);
	EXPECT_THROW(Writer::writeD(buf, 1), utils::BufferOverflowException);
	EXPECT_EQ(buf.position(), 0);
	EXPECT_THROW(Writer::writeS(buf, "ab"), utils::BufferOverflowException);
}
