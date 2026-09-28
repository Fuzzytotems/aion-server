#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "NetworkTestUtils.h"
#include "aion/commons/network/packet/BaseClientPacket.h"

using namespace aion::commons;
using namespace aion::commons::network::packet;
using nettest::LogCapture;

namespace {

struct FakeConnection {
	std::string toString() const { return "FakeConnection 1.2.3.4"; }
};

/** A client packet whose readImpl is a lambda, exposing the read helpers. */
class CM_FAKE : public BaseClientPacket<FakeConnection> {
public:
	CM_FAKE(utils::ByteBuffer buffer, int32_t opcode, std::function<void(CM_FAKE&)> reader = {})
		: BaseClientPacket(std::move(buffer), opcode), reader(std::move(reader)) {}

	using BaseClientPacket::readB;
	using BaseClientPacket::readC;
	using BaseClientPacket::readD;
	using BaseClientPacket::readDF;
	using BaseClientPacket::readF;
	using BaseClientPacket::readH;
	using BaseClientPacket::readQ;
	using BaseClientPacket::readS;
	using BaseClientPacket::readUC;
	using BaseClientPacket::readUH;

	int runCount = 0;

protected:
	void readImpl() override {
		if (reader)
			reader(*this);
	}
	void runImpl() override { runCount++; }

private:
	std::function<void(CM_FAKE&)> reader;
};

/** A connection that is only forward declared where its packet base class is defined (the game server's AionClientPacket.h). */
struct LateConnection;

class CM_LATE : public BaseClientPacket<LateConnection> {
public:
	explicit CM_LATE(utils::ByteBuffer buffer) : BaseClientPacket(std::move(buffer), 3) {}

	using BaseClientPacket::readD;

protected:
	void readImpl() override {}
	void runImpl() override {}
};

struct LateConnection {
	std::string toString() const { return "LateConnection 5.6.7.8"; }
};

utils::ByteBuffer bufferOf(std::vector<uint8_t> bytes) {
	utils::ByteBuffer buf = utils::ByteBuffer::allocate(static_cast<int32_t>(bytes.size()));
	buf.put(bytes);
	buf.flip();
	return buf;
}

} // namespace

TEST(BaseClientPacketTest, ReadsLittleEndianValues) {
	utils::ByteBuffer buf = utils::ByteBuffer::allocate(64);
	buf.putInt(-2).putShort(-3).put(int8_t(-4)).put(uint8_t(0xF0)).putShort(static_cast<int16_t>(0xFFFE)).putLong(1234567890123LL);
	buf.putFloat(0.5f).putDouble(8.25).put(uint8_t(9)).put(uint8_t(8));
	buf.flip();

	CM_FAKE packet(buf, 1);
	EXPECT_EQ(packet.readD(), -2);
	EXPECT_EQ(packet.readH(), -3);
	EXPECT_EQ(packet.readC(), -4);
	EXPECT_EQ(packet.readUC(), 0xF0);
	EXPECT_EQ(packet.readUH(), 0xFFFE);
	EXPECT_EQ(packet.readQ(), 1234567890123LL);
	EXPECT_EQ(packet.readF(), 0.5f);
	EXPECT_EQ(packet.readDF(), 8.25);
	EXPECT_EQ(packet.readB(2), (std::vector<uint8_t>{9, 8}));
	EXPECT_EQ(packet.getRemainingBytes(), 0);
}

TEST(BaseClientPacketTest, UnderflowLogsAndReturnsZero) {
	LogCapture& logs = LogCapture::instance();
	logs.clear();

	CM_FAKE packet(bufferOf({0x01, 0x02, 0x03}), 7);
	packet.setConnection(std::make_shared<FakeConnection>());
	EXPECT_EQ(packet.readD(), 0);
	EXPECT_EQ(packet.getRemainingBytes(), 3); // nothing consumed
	EXPECT_TRUE(logs.contains("error|com.aionemu.commons.network.packet.BaseClientPacket|Missing D for: [007] CM_FAKE (sent from FakeConnection 1.2.3.4)"));
	EXPECT_EQ(packet.readQ(), 0);
	EXPECT_TRUE(logs.contains("Missing Q for: [007] CM_FAKE"));
	EXPECT_EQ(packet.readDF(), 0.0);
	EXPECT_TRUE(logs.contains("Missing DF for"));
	EXPECT_EQ(packet.readF(), 0.0f);
	EXPECT_TRUE(logs.contains("Missing F for"));

	EXPECT_EQ(packet.readH(), 0x0201);
	EXPECT_EQ(packet.readUH(), 0);
	EXPECT_TRUE(logs.contains("Missing H for"));
	EXPECT_EQ(packet.readC(), 3);
	EXPECT_EQ(packet.readC(), 0);
	EXPECT_EQ(packet.readUC(), 0);
	EXPECT_EQ(logs.count("Missing C for"), 2);
}

TEST(BaseClientPacketTest, UnderflowWithoutConnectionPrintsNull) {
	LogCapture& logs = LogCapture::instance();
	logs.clear();
	CM_FAKE packet(bufferOf({}), 12);
	EXPECT_EQ(packet.readD(), 0);
	EXPECT_TRUE(logs.contains("Missing D for: [012] CM_FAKE (sent from null)"));
}

TEST(BaseClientPacketTest, PacketBaseCompilesAgainstAForwardDeclaredConnection) {
	LogCapture& logs = LogCapture::instance();
	logs.clear();
	CM_LATE packet(bufferOf({}));
	EXPECT_EQ(packet.readD(), 0);
	EXPECT_TRUE(logs.contains("Missing D for: [003] CM_LATE (sent from null)"));
	packet.setConnection(std::make_shared<LateConnection>());
	EXPECT_EQ(packet.readD(), 0);
	EXPECT_TRUE(logs.contains("Missing D for: [003] CM_LATE (sent from LateConnection 5.6.7.8)"));
}

TEST(BaseClientPacketTest, ReadSReturnsPartialStringOnUnderflow) {
	LogCapture& logs = LogCapture::instance();
	logs.clear();
	// "Hi" without terminator, plus one odd byte
	CM_FAKE packet(bufferOf({'H', 0, 'i', 0, 'x'}), 1);
	EXPECT_EQ(packet.readS(), "Hi");
	EXPECT_TRUE(logs.contains("Missing S for"));
	EXPECT_EQ(packet.getRemainingBytes(), 1);
}

TEST(BaseClientPacketTest, ReadSStopsAtTerminator) {
	CM_FAKE packet(bufferOf({'a', 0, 0xE4, 0, 0, 0, 'b', 0, 0, 0}), 1); // "aä", "b"
	EXPECT_EQ(packet.readS(), "a\xC3\xA4");
	EXPECT_EQ(packet.readS(), "b");
	EXPECT_EQ(packet.readS(), "");
}

TEST(BaseClientPacketTest, ReadBUnderflowReturnsZeroesWithoutConsuming) {
	LogCapture& logs = LogCapture::instance();
	logs.clear();
	CM_FAKE packet(bufferOf({5, 6, 7}), 1);
	EXPECT_EQ(packet.readB(4), (std::vector<uint8_t>{0, 0, 0, 0}));
	EXPECT_TRUE(logs.contains("Missing byte[] for"));
	EXPECT_EQ(packet.getRemainingBytes(), 3);
	EXPECT_EQ(packet.readB(0), std::vector<uint8_t>{});
	EXPECT_THROW(packet.readB(-1), utils::IllegalArgumentException);
}

TEST(BaseClientPacketTest, ReadReturnsTrueAndWarnsOncePerOpcodeIfNotFullyRead) {
	LogCapture& logs = LogCapture::instance();
	logs.clear();
	auto readOneByte = [](CM_FAKE& p) { p.readC(); };
	// the warning is logged once per opcode per process, so use fresh opcodes when the test is repeated
	static int32_t opcode = 4000;
	opcode += 2;

	CM_FAKE first(bufferOf({1, 2, 3}), opcode, readOneByte);
	EXPECT_TRUE(first.read());
	EXPECT_TRUE(logs.contains("warning|com.aionemu.commons.network.packet.BaseClientPacket|[" + std::to_string(opcode) +
														"] CM_FAKE was not fully read! Last 2 bytes were not read from buffer:\n0000: 01 02 03"));

	CM_FAKE second(bufferOf({1, 2, 3}), opcode, readOneByte);
	EXPECT_TRUE(second.read());
	EXPECT_EQ(logs.count("was not fully read"), 1);

	CM_FAKE complete(bufferOf({1}), opcode + 1, readOneByte);
	EXPECT_TRUE(complete.read());
	EXPECT_EQ(logs.count("was not fully read"), 1);
}

TEST(BaseClientPacketTest, ReadReturnsFalseIfReadImplThrows) {
	LogCapture& logs = LogCapture::instance();
	logs.clear();
	CM_FAKE packet(bufferOf({0xAB, 0xCD, 0xEF}), 3, [](CM_FAKE& p) {
		p.readC();
		throw utils::IllegalStateException("broken packet");
	});
	EXPECT_FALSE(packet.read());
	EXPECT_TRUE(logs.contains("error|com.aionemu.commons.network.packet.BaseClientPacket|Reading failed for packet [003] CM_FAKE. Buffer Info (last 2 bytes were not read):\n0000: AB CD EF"));
	EXPECT_TRUE(logs.contains("broken packet"));
}

TEST(BaseClientPacketTest, RunCallsRunImplAndConnectionIsKept) {
	CM_FAKE packet(bufferOf({}), 1);
	auto connection = std::make_shared<FakeConnection>();
	packet.setConnection(connection);
	packet.run();
	EXPECT_EQ(packet.runCount, 1);
	EXPECT_EQ(packet.getConnection(), connection);
}
