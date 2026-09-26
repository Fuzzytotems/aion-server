// Byte vectors of every packet the chat server sends, written without a connection. The expected bytes are transcribed by hand from the Java
// writeImpl methods (and AbstractServerPacket.write + LoginPacketEncoder for the client packets, GsServerPacket.write for the game server ones).

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "aion/chatserver/configs/network/NetworkConfig.h"
#include "aion/chatserver/model/ChatClient.h"
#include "aion/chatserver/model/channel/RegionChannel.h"
#include "aion/chatserver/model/message/Message.h"
#include "aion/chatserver/network/aion/serverpackets/SM_CHANNEL_MESSAGE.h"
#include "aion/chatserver/network/aion/serverpackets/SM_CHANNEL_RESPONSE.h"
#include "aion/chatserver/network/aion/serverpackets/SM_CHAT_INI.h"
#include "aion/chatserver/network/aion/serverpackets/SM_PLAYER_AUTH_RESPONSE.h"
#include "aion/chatserver/network/gameserver/serverpackets/SM_GS_AUTH_RESPONSE.h"
#include "aion/chatserver/network/gameserver/serverpackets/SM_PLAYER_AUTH_RESPONSE.h"
#include "aion/chatserver/network/netty/coder/LoginPacketEncoder.h"
#include "aion/chatserver/network/netty/handler/ClientChannelHandler.h"
#include "aion/commons/utils/ByteBuffer.h"
#include "aion/commons/utils/Exception.h"
#include "support/FakePeers.h"
#include "support/TestUtils.h"

namespace aion::chatserver::test {
namespace {

using commons::utils::ByteBuffer;
using configs::network::NetworkConfig;
using model::ChatClient;
using model::Race;
using model::channel::RegionChannel;
using model::message::Message;
namespace clientsm = network::aion::serverpackets;
namespace gssm = network::gameserver::serverpackets;

/** ClientChannelHandler.sendPacket without the channel: a 16 KiB buffer, AbstractServerPacket.write, LoginPacketEncoder */
Bytes writeClientPacket(const network::aion::AbstractServerPacket& packet) {
	ByteBuffer buf = ByteBuffer::allocate(network::netty::handler::ClientChannelHandler::SEND_BUFFER_SIZE);
	packet.write(nullptr, buf);
	network::netty::coder::LoginPacketEncoder::encode(buf);
	return Bytes(buf.remainingSpan().begin(), buf.remainingSpan().end());
}

/** GsConnection.writeData without the connection */
Bytes writeGsPacket(const network::gameserver::GsServerPacket& packet) {
	ByteBuffer buf = ByteBuffer::allocate(8192 * 8);
	packet.write(nullptr, buf);
	return Bytes(buf.remainingSpan().begin(), buf.remainingSpan().end());
}

std::shared_ptr<ChatClient> client(int32_t id, std::optional<std::string> nameIdentifier) {
	auto chatClient = std::make_shared<ChatClient>(id, Bytes(48), "account", "Name", Race::ELYOS, int8_t{0});
	if (nameIdentifier)
		chatClient->setIdentifier(ascii16(*nameIdentifier));
	return chatClient;
}

TEST(ServerPacketBytesTest, SmChatIni) {
	EXPECT_EQ(writeClientPacket(clientsm::SM_CHAT_INI()), (Bytes{0x0A, 0x00, 0x31, 0x40, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00}));
}

TEST(ServerPacketBytesTest, SmPlayerAuthResponseToTheClient) {
	EXPECT_EQ(writeClientPacket(clientsm::SM_PLAYER_AUTH_RESPONSE()), (Bytes{0x0C, 0x00, 0x02, 0x40, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x08}));
}

TEST(ServerPacketBytesTest, SmChannelResponse) {
	RegionChannel channel(1, Race::ELYOS, "map");
	int32_t id = channel.getChannelId();
	Bytes expected{0x0E, 0x00, 0x11, 0x40, 0x0D, 0x0C, 0x0B, 0x0A, 0x00, 0x00, static_cast<uint8_t>(id), static_cast<uint8_t>(id >> 8),
		static_cast<uint8_t>(id >> 16), static_cast<uint8_t>(id >> 24)};
	EXPECT_EQ(writeClientPacket(clientsm::SM_CHANNEL_RESPONSE(channel, 0x0A0B0C0D)), expected);
}

TEST(ServerPacketBytesTest, SmChannelMessage) {
	auto channel = std::make_shared<RegionChannel>(1, Race::ELYOS, "map");
	int32_t id = channel->getChannelId();
	Message message(channel, ascii16("hi"), client(12345, "A@B"));
	// size 39, opcode 0x1A, C 0, D 0, D 0, D channel id, D 12345, D 0, C 0, H 3, "A@B", H 2, "hi"
	Bytes expected{0x27, 0x00, 0x1A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, static_cast<uint8_t>(id), static_cast<uint8_t>(id >> 8),
		static_cast<uint8_t>(id >> 16), static_cast<uint8_t>(id >> 24), 0x39, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 'A', 0x00, '@',
		0x00, 'B', 0x00, 0x02, 0x00, 'h', 0x00, 'i', 0x00};
	EXPECT_EQ(writeClientPacket(clientsm::SM_CHANNEL_MESSAGE(message)), expected);
}

TEST(ServerPacketBytesTest, SmChannelMessageWithReplacedText) {
	auto channel = std::make_shared<RegionChannel>(1, Race::ASMODIANS, "map");
	Message message(channel, ascii16("original"), client(77, "Bob@AION"));
	message.setText("You have been gagged for 3 minutes.");
	EXPECT_EQ(writeClientPacket(clientsm::SM_CHANNEL_MESSAGE(message)),
		expectedChannelMessage(channel->getChannelId(), 77, "Bob@AION", "You have been gagged for 3 minutes."));
}

TEST(ServerPacketBytesTest, SmChannelMessageOfASenderWithoutIdentifierThrows) {
	// Java: NullPointerException (getIdentifier() is null before the client authenticated)
	auto channel = std::make_shared<RegionChannel>(1, Race::ELYOS, "map");
	Message message(channel, ascii16("x"), client(5, std::nullopt));
	EXPECT_THROW(writeClientPacket(clientsm::SM_CHANNEL_MESSAGE(message)), commons::utils::IllegalStateException);
}

TEST(ServerPacketBytesTest, SmChannelMessageBiggerThanTheSendBufferThrows) {
	// Java: ChannelBuffers.buffer(LITTLE_ENDIAN, 2 * 8192) is fixed, writing past it throws in sendPacket
	auto channel = std::make_shared<RegionChannel>(1, Race::ELYOS, "map");
	std::string identifier = "A@B"; // 6 bytes: 29 + 6 + text
	Message fits(channel, Bytes(16384 - 29 - 6), client(6, identifier));
	EXPECT_EQ(writeClientPacket(clientsm::SM_CHANNEL_MESSAGE(fits)).size(), 16384u);
	Message tooBig(channel, Bytes(16384 - 29 - 6 + 2), client(6, identifier));
	EXPECT_THROW(writeClientPacket(clientsm::SM_CHANNEL_MESSAGE(tooBig)), commons::utils::BufferOverflowException);
}

TEST(ServerPacketBytesTest, SmGsAuthResponseAuthedCarriesTheConnectAddress) {
	NetworkConfig::CLIENT_CONNECT_ADDRESS = {"127.0.0.1", 10241};
	// size 11, opcode 0, AUTHED 0, C 4, 127.0.0.1, H 10241 (0x2801)
	EXPECT_EQ(writeGsPacket(gssm::SM_GS_AUTH_RESPONSE(network::gameserver::GsAuthResponse::AUTHED)),
		(Bytes{0x0B, 0x00, 0x00, 0x00, 0x04, 0x7F, 0x00, 0x00, 0x01, 0x01, 0x28}));
	NetworkConfig::CLIENT_CONNECT_ADDRESS = {"10.1.2.3", 7777};
	EXPECT_EQ(writeGsPacket(gssm::SM_GS_AUTH_RESPONSE(network::gameserver::GsAuthResponse::AUTHED)),
		(Bytes{0x0B, 0x00, 0x00, 0x00, 0x04, 0x0A, 0x01, 0x02, 0x03, 0x61, 0x1E}));
	// the length byte is the length of the address: 16 for IPv6 (the game server's SM_VERSION_CHECK needs IPv4, though)
	NetworkConfig::CLIENT_CONNECT_ADDRESS = {"::1", 10241};
	EXPECT_EQ(writeGsPacket(gssm::SM_GS_AUTH_RESPONSE(network::gameserver::GsAuthResponse::AUTHED)),
		(Bytes{0x17, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01,
			0x28}));
}

TEST(ServerPacketBytesTest, SmGsAuthResponseRejections) {
	EXPECT_EQ(writeGsPacket(gssm::SM_GS_AUTH_RESPONSE(network::gameserver::GsAuthResponse::NOT_AUTHED)), (Bytes{0x04, 0x00, 0x00, 0x01}));
	EXPECT_EQ(writeGsPacket(gssm::SM_GS_AUTH_RESPONSE(network::gameserver::GsAuthResponse::ALREADY_REGISTERED)), (Bytes{0x04, 0x00, 0x00, 0x02}));
}

TEST(ServerPacketBytesTest, SmPlayerAuthResponseToTheGameServer) {
	Bytes token(48);
	for (size_t i = 0; i < token.size(); i++)
		token[i] = static_cast<uint8_t>(i);
	ChatClient chatClient(0x01020304, token, "account", "Name", Race::ASMODIANS, int8_t{0});
	// size 56, opcode 1, D player id, C 48, token
	Bytes expected{0x38, 0x00, 0x01, 0x04, 0x03, 0x02, 0x01, 0x30};
	expected.insert(expected.end(), token.begin(), token.end());
	EXPECT_EQ(writeGsPacket(gssm::SM_PLAYER_AUTH_RESPONSE(chatClient)), expected);
}

TEST(ServerPacketBytesTest, PacketNamesLikeJava) {
	// Java AbstractPacket.toString: simple class name + " [opCode=0x%02X]"
	EXPECT_EQ(clientsm::SM_CHAT_INI().toString(), "SM_CHAT_INI [opCode=0x31]");
	EXPECT_EQ(clientsm::SM_PLAYER_AUTH_RESPONSE().toString(), "SM_PLAYER_AUTH_RESPONSE [opCode=0x02]");
}

} // namespace
} // namespace aion::chatserver::test
