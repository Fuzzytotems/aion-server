// The framing of the connections (Java: NettyPipeline's LengthFieldBasedFrameDecoder and PacketEncoder): little endian uint16 sizes that
// include the two size bytes, in both directions, and packets up to the maximum packet size (8192 * 2 for the clients).

#include <thread>

#include <gtest/gtest.h>

#include "ChatServerTestFixture.h"

namespace aion::chatserver::test {
namespace {

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
	LogCapture log({"com.aionemu.commons.network.packet.BaseClientPacket"});
	FakeChatClient client(clientPort);
	PacketWriter ini;
	ini.C(0x30).C(0x40).H(0).D(0).D(0).D(0);
	ini.zeros(16384 - 2 - ini.data.size()); // frame size 16384
	Bytes frame = ini.frame();
	ASSERT_EQ(frame.size(), 16384u);
	client.send(frame);
	EXPECT_EQ(client.expectFrame("SM_CHAT_INI"), SM_CHAT_INI_BYTES);
	// logged once per opcode and process, so only if no other test sent a CM_CHAT_INI with trailing bytes before
	if (log.contains("CM_CHAT_INI was not fully read"))
		EXPECT_TRUE(log.contains("[048] CM_CHAT_INI was not fully read! Last 16366 bytes")) << log.dump();
}

TEST_F(ClientFramingTest, AFrameLongerThanTheMaximumIsAnInvalidPacket) {
	LogCapture log({"com.aionemu.commons.network.netty.ConnectionHandler"});
	FakeChatClient client(clientPort);
	client.send(Bytes{0x01, 0x40, 0x30}); // declares 16385 bytes
	EXPECT_TRUE(client.socket.waitClosed());
	EXPECT_TRUE(log.waitFor("Client 127.0.0.1 sent an invalid packet, disconnecting: Adjusted frame length exceeds 16384: 16385 - discarded"))
		<< log.dump();
}

TEST_F(ClientFramingTest, AFrameShorterThanItsSizeHeaderIsAnInvalidPacket) {
	LogCapture log({"com.aionemu.commons.network.netty.ConnectionHandler"});
	FakeChatClient client(clientPort);
	client.send(Bytes{0x01, 0x00, 0x30});
	EXPECT_TRUE(client.socket.waitClosed());
	EXPECT_TRUE(log.waitFor("Client 127.0.0.1 sent an invalid packet, disconnecting: Adjusted frame length (1) is less than lengthFieldEndOffset: 2"))
		<< log.dump();
}

TEST_F(ClientFramingTest, AFrameWithoutBodyIsAnEmptyPacket) {
	LogCapture log({"com.aionemu.commons.network.netty.ConnectionHandler"});
	FakeChatClient client(clientPort);
	client.send(Bytes{0x02, 0x00, 0x02}); // the third byte starts the next frame
	EXPECT_TRUE(client.socket.waitClosed());
	EXPECT_TRUE(log.waitFor("Client 127.0.0.1 sent an empty packet, disconnecting")) << log.dump();
}

// upstream 459b35094: malformed packets disconnect
TEST_F(ClientFramingTest, APacketShorterThanItsFieldsClosesTheConnection) {
	LogCapture log({"com.aionemu.commons.network.packet.BaseClientPacket"});
	FakeChatClient client(clientPort);
	client.send(PacketWriter().C(0x30).C(0x40).H(0).frame()); // CM_CHAT_INI without its three D
	EXPECT_TRUE(client.socket.waitClosed());
	EXPECT_TRUE(client.socket.isClosed());
}

} // namespace
} // namespace aion::chatserver::test
