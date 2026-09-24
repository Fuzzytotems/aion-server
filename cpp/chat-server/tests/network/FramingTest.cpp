// The framing of the client connections (Java: PacketFrameDecoder, LoginPacketDecoder, LoginPacketEncoder): little endian uint16 sizes that
// include the two size bytes, in both directions.

#include <thread>

#include <gtest/gtest.h>

#include "ChatServerTestFixture.h"
#include "aion/chatserver/network/netty/coder/LoginPacketDecoder.h"
#include "aion/chatserver/network/netty/coder/LoginPacketEncoder.h"
#include "aion/chatserver/network/netty/coder/PacketFrameDecoder.h"
#include "aion/commons/utils/ByteBuffer.h"
#include "support/FakePeers.h"
#include "support/TestUtils.h"

namespace aion::chatserver::test {
namespace {

using commons::utils::ByteBuffer;
using network::netty::coder::LoginPacketDecoder;
using network::netty::coder::LoginPacketEncoder;
using network::netty::coder::PacketFrameDecoder;

TEST(PacketCoderTest, EncoderWritesTheSizeOfTheWholePacketLittleEndian) {
	ByteBuffer buf = ByteBuffer::allocate(16384);
	buf.putShort(0); // AbstractServerPacket.write
	for (int i = 0; i < 0x123; i++)
		buf.put(static_cast<int8_t>(i));
	LoginPacketEncoder::encode(buf);
	EXPECT_EQ(buf.position(), 0);
	EXPECT_EQ(buf.remaining(), 0x125);
	EXPECT_EQ(static_cast<uint8_t>(buf.get(0)), 0x25);
	EXPECT_EQ(static_cast<uint8_t>(buf.get(1)), 0x01);
	EXPECT_EQ(static_cast<uint8_t>(buf.get(2)), 0x00);
	EXPECT_EQ(static_cast<uint8_t>(buf.get(0x124)), 0x22);
}

TEST(PacketCoderTest, DecoderPassesFramesOn) {
	ByteBuffer frame = ByteBuffer::allocate(3);
	frame.put(int8_t{5});
	frame.flip();
	ByteBuffer decoded = LoginPacketDecoder::decode(frame);
	EXPECT_EQ(decoded.data(), frame.data());
	EXPECT_EQ(decoded.remaining(), 1);
}

TEST(PacketCoderTest, FrameDecoderParameters) {
	// Java: LengthFieldBasedFrameDecoder(8192 * 2, 0, 2, -2, 2)
	EXPECT_EQ(PacketFrameDecoder::MAX_PACKET_LENGTH, 16384);
	EXPECT_EQ(PacketFrameDecoder::LENGTH_FIELD_OFFSET, 0);
	EXPECT_EQ(PacketFrameDecoder::LENGTH_FIELD_LENGTH, 2);
	EXPECT_EQ(PacketFrameDecoder::LENGTH_FIELD_ADJUSTMENT, -2);
	EXPECT_EQ(PacketFrameDecoder::INITIAL_BYTES_TO_STRIP, 2);
}

class ClientFramingTest : public ChatServerTestFixture {};

TEST_F(ClientFramingTest, SeveralFramesInOneWriteAreAllProcessedInOrder) {
	FakeChatClient client(clientPort);
	Bytes ini = FakeChatClient::buildChatIni();
	Bytes three;
	for (int i = 0; i < 3; i++)
		three.insert(three.end(), ini.begin(), ini.end());
	client.send(three);
	for (int i = 0; i < 3; i++)
		EXPECT_EQ(client.expectFrame("SM_CHAT_INI"), SM_CHAT_INI_BYTES);
}

TEST_F(ClientFramingTest, AFrameSplitAcrossWritesIsReassembled) {
	FakeChatClient client(clientPort);
	Bytes ini = FakeChatClient::buildChatIni();
	client.send(std::span<const uint8_t>(ini).first(1));
	std::this_thread::sleep_for(50ms);
	client.send(std::span<const uint8_t>(ini).subspan(1, 6));
	std::this_thread::sleep_for(50ms);
	client.send(std::span<const uint8_t>(ini).subspan(7));
	EXPECT_EQ(client.expectFrame("SM_CHAT_INI"), SM_CHAT_INI_BYTES);
}

TEST_F(ClientFramingTest, AFrameOfTheMaximumLengthIsAccepted) {
	LogCapture log({"com.aionemu.chatserver"});
	FakeChatClient client(clientPort);
	PacketWriter ini;
	ini.C(0x30).C(0x40).H(0).D(0).D(0).D(0);
	ini.zeros(16384 - 2 - ini.data.size()); // frame size 16384
	Bytes frame = ini.frame();
	ASSERT_EQ(frame.size(), 16384u);
	client.send(frame);
	EXPECT_EQ(client.expectFrame("SM_CHAT_INI"), SM_CHAT_INI_BYTES);
	EXPECT_TRUE(log.contains("CM_CHAT_INI [opCode=0x30] was not fully read! Last 16366 bytes")) << log.dump();
}

TEST_F(ClientFramingTest, AFrameLongerThanTheMaximumClosesTheConnection) {
	// Deviation (docs/deviations/chat-server.md): Netty discards the frame and keeps the connection, commons' framing disconnects
	LogCapture log({"com.aionemu.commons.network"});
	FakeChatClient client(clientPort);
	client.send(Bytes{0x01, 0x40, 0x30}); // declares 16385 bytes
	EXPECT_TRUE(client.socket.waitClosed());
	EXPECT_TRUE(log.contains("Received packet with a size of 16385 bytes")) << log.dump();
}

} // namespace
} // namespace aion::chatserver::test
