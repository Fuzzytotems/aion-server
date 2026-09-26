// The client side of the token flow: the game server registers a player (CM_PLAYER_AUTH from the game server), the client logs in with the token
// (CM_CHAT_INI, CM_PLAYER_AUTH) and only a matching login gets SM_PLAYER_AUTH_RESPONSE and the AUTHED state.

#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "ChatServerTestFixture.h"
#include "support/FakePeers.h"
#include "support/TestUtils.h"

namespace aion::chatserver::test {
namespace {

class ClientAuthTest : public ChatServerTestFixture {
protected:
	/** registers a player and returns the token */
	Bytes registerPlayer(FakeGameServer& gs, int32_t playerId, std::string_view account, std::string_view name) {
		return gs.registerPlayer(playerId, account, name, 0);
	}
};

TEST_F(ClientAuthTest, ChatIniIsAnsweredBeforeTheLogin) {
	LogCapture log({"com.aionemu.chatserver"});
	FakeChatClient client(clientPort);
	EXPECT_TRUE(log.waitFor("Channel connected Ip: 127.0.0.1")) << log.dump();
	client.send(FakeChatClient::buildChatIni());
	Bytes response = client.expectFrame("SM_CHAT_INI");
	EXPECT_EQ(response, SM_CHAT_INI_BYTES) << hex(response);
	EXPECT_FALSE(log.contains("was not fully read")) << log.dump();
}

TEST_F(ClientAuthTest, MatchingTokenLogsIn) {
	LogCapture log({"com.aionemu.chatserver"});
	auto gs = connectGameServer();
	int32_t playerId = nextPlayerId();
	Bytes token = registerPlayer(*gs, playerId, "Account1", "Alice");
	FakeChatClient client(clientPort);
	// the client sends the account name in lower case: compared ignoring case
	client.send(FakeChatClient::buildPlayerAuth(playerId, "Alice@AION_KOR", "account1", token));
	Bytes response = client.expectFrame("SM_PLAYER_AUTH_RESPONSE");
	EXPECT_EQ(response, SM_PLAYER_AUTH_RESPONSE_BYTES) << hex(response);
	EXPECT_FALSE(log.contains("was not fully read")) << log.dump();
	// AUTHED: channel requests are handled now
	client.send(FakeChatClient::buildChannelRequest(1, channelIdentifier("public", uniqueName("auth_map"), GS_ID, 0)));
	EXPECT_EQ(client.expectFrame("SM_CHANNEL_RESPONSE").size(), 14u);
}

TEST_F(ClientAuthTest, WrongTokenIsRejected) {
	LogCapture log({"com.aionemu.chatserver"});
	auto gs = connectGameServer();
	int32_t playerId = nextPlayerId();
	Bytes token = registerPlayer(*gs, playerId, "acc", "Bob");
	token[20] ^= 0x01;
	FakeChatClient client(clientPort);
	client.send(FakeChatClient::buildPlayerAuth(playerId, "Bob@AION", "acc", token));
	EXPECT_TRUE(log.waitFor("Client tried to connect but given token doesn't match")) << log.dump();
	EXPECT_TRUE(client.socket.expectSilence());
	// still CONNECTED: a channel request is an unknown packet
	client.send(FakeChatClient::buildChannelRequest(1, channelIdentifier("public", "x", GS_ID, 0)));
	EXPECT_TRUE(log.waitFor("Unknown packet received from client: opCode=0x10 state=CONNECTED")) << log.dump();
}

TEST_F(ClientAuthTest, UnregisteredPlayerIsRejected) {
	LogCapture log({"com.aionemu.chatserver"});
	FakeChatClient client(clientPort);
	client.send(FakeChatClient::buildPlayerAuth(nextPlayerId(), "Nobody@AION", "acc", Bytes(48)));
	EXPECT_TRUE(log.waitFor("Client tried to connect but was not yet registered from game server side")) << log.dump();
	EXPECT_TRUE(client.socket.expectSilence());
}

TEST_F(ClientAuthTest, WrongAccountNameIsRejected) {
	LogCapture log({"com.aionemu.chatserver"});
	auto gs = connectGameServer();
	int32_t playerId = nextPlayerId();
	Bytes token = registerPlayer(*gs, playerId, "rightacc", "Carol");
	FakeChatClient client(clientPort);
	client.send(FakeChatClient::buildPlayerAuth(playerId, "Carol@AION", "wrongacc", token));
	EXPECT_TRUE(log.waitFor("Client tried to connect with account name: wrongacc (expected: rightacc)")) << log.dump();
	EXPECT_TRUE(client.socket.expectSilence());
}

TEST_F(ClientAuthTest, WrongCharacterNameIsRejected) {
	LogCapture log({"com.aionemu.chatserver"});
	auto gs = connectGameServer();
	int32_t playerId = nextPlayerId();
	Bytes token = registerPlayer(*gs, playerId, "acc", "Dave");
	FakeChatClient client(clientPort);
	// the name is the identifier up to the last '@': "Dave@x@AION" is "Dave@x"
	client.send(FakeChatClient::buildPlayerAuth(playerId, "Dave@x@AION", "acc", token));
	EXPECT_TRUE(log.waitFor("Client tried to connect with character name: Dave@x (expected: Dave)")) << log.dump();
	EXPECT_TRUE(client.socket.expectSilence());
}

TEST_F(ClientAuthTest, TheNameSeparatorIsTheOneThePacketSends) {
	// Java reads the separator from the packet (readB(2)) and cuts the name at its last occurrence
	LogCapture log({"com.aionemu.chatserver"});
	auto gs = connectGameServer();
	int32_t playerId = nextPlayerId();
	Bytes token = registerPlayer(*gs, playerId, "acc", "Gus");
	FakeChatClient client(clientPort);
	client.send(FakeChatClient::buildPlayerAuth(playerId, "Gus#AION", "acc", token, u'#'));
	Bytes response = client.expectFrame("SM_PLAYER_AUTH_RESPONSE");
	EXPECT_EQ(response, SM_PLAYER_AUTH_RESPONSE_BYTES) << hex(response);
	// with '#' as the separator, "Gus@AION" has none: substring(0, -1)
	FakeChatClient other(clientPort);
	other.send(FakeChatClient::buildPlayerAuth(playerId, "Gus@AION", "acc", token, u'#'));
	EXPECT_TRUE(log.waitFor("begin 0, end -1, length 8", EXCEPTION_LOG_TIMEOUT)) << log.dump();
	EXPECT_TRUE(other.socket.expectSilence());
}

TEST_F(ClientAuthTest, ARegistrationAgainReplacesTheFirstOne) {
	// Java: ChatService.players.put and BroadcastService.clients.put replace the entry of the same player id (the game server registers a player
	// again after a relog): only the new token logs in, and the broadcasts go to the new connection
	LogCapture log({"com.aionemu.chatserver"});
	auto gs = connectGameServer();
	Player other = loginPlayer(*gs, "Other", 0);
	int32_t playerId = nextPlayerId();
	Bytes firstToken = registerPlayer(*gs, playerId, "acc", "Rex");
	FakeChatClient first(clientPort);
	first.login(playerId, "Rex@AION", "acc", firstToken);
	std::string identifier = channelIdentifier("public", uniqueName("map"), GS_ID, 0);
	int32_t channelId = first.joinChannel(1, identifier);
	other.client->joinChannel(1, identifier);

	Bytes secondToken = registerPlayer(*gs, playerId, "acc", "Rex");
	FakeChatClient stale(clientPort);
	stale.send(FakeChatClient::buildPlayerAuth(playerId, "Rex@AION", "acc", firstToken));
	EXPECT_TRUE(log.waitFor("Client tried to connect but given token doesn't match")) << log.dump();
	EXPECT_TRUE(stale.socket.expectSilence());

	FakeChatClient second(clientPort);
	second.login(playerId, "Rex@AION", "acc", secondToken);
	EXPECT_EQ(second.joinChannel(1, identifier), channelId);
	other.client->send(FakeChatClient::buildChannelMessage(channelId, "welcome back"));
	Bytes expected = expectedChannelMessage(channelId, other.id, other.nameIdentifier, "welcome back");
	EXPECT_EQ(second.expectFrame("SM_CHANNEL_MESSAGE"), expected);
	EXPECT_EQ(other.client->expectFrame("SM_CHANNEL_MESSAGE"), expected);
	EXPECT_TRUE(first.socket.expectSilence());
}

TEST_F(ClientAuthTest, PacketsAreLoggedAtDebugLevel) {
	LogCapture log({"com.aionemu.chatserver.network.netty.handler.ClientChannelHandler"}, spdlog::level::debug);
	FakeChatClient client(clientPort);
	client.send(FakeChatClient::buildChatIni());
	EXPECT_EQ(client.expectFrame("SM_CHAT_INI"), SM_CHAT_INI_BYTES);
	EXPECT_TRUE(log.waitFor("debug|com.aionemu.chatserver.network.netty.handler.ClientChannelHandler|Sent packet: SM_CHAT_INI [opCode=0x31]"))
		<< log.dump();
	EXPECT_TRUE(log.waitFor("debug|com.aionemu.chatserver.network.netty.handler.ClientChannelHandler|Received packet: CM_CHAT_INI [opCode=0x30]"))
		<< log.dump();
}

TEST_F(ClientAuthTest, IdentifierWithoutSeparatorFailsInRunImpl) {
	// Java: nameIdentifier.substring(0, -1) throws a StringIndexOutOfBoundsException, logged by BaseClientPacket.run
	LogCapture log({"com.aionemu.chatserver"});
	auto gs = connectGameServer();
	int32_t playerId = nextPlayerId();
	Bytes token = registerPlayer(*gs, playerId, "acc", "Erin");
	FakeChatClient client(clientPort);
	client.send(FakeChatClient::buildPlayerAuth(playerId, "Erin", "acc", token));
	EXPECT_TRUE(log.waitFor("Running failed for packet CM_PLAYER_AUTH [opCode=0x05]", EXCEPTION_LOG_TIMEOUT)) << log.dump();
	EXPECT_TRUE(log.contains("begin 0, end -1, length 4")) << log.dump();
	EXPECT_TRUE(client.socket.expectSilence());
}

TEST_F(ClientAuthTest, OnlyChatIniAndPlayerAuthAreKnownBeforeTheLogin) {
	LogCapture log({"com.aionemu.chatserver"});
	FakeChatClient client(clientPort);
	client.send(FakeChatClient::buildPing());
	// the 19 bytes after the opcode as hex digits without separators
	EXPECT_TRUE(log.waitFor("Unknown packet received from client: opCode=0xFF state=CONNECTED length=19 data=[" + std::string(38, '0') + "]"))
		<< log.dump();
	EXPECT_TRUE(client.socket.expectSilence());
}

TEST_F(ClientAuthTest, LoginPacketsAreUnknownAfterTheLogin) {
	LogCapture log({"com.aionemu.chatserver"});
	auto gs = connectGameServer();
	Player player = loginPlayer(*gs, "Fay", 0);
	player.client->send(FakeChatClient::buildChatIni());
	EXPECT_TRUE(log.waitFor("Unknown packet received from client: opCode=0x30 state=AUTHED")) << log.dump();
	EXPECT_TRUE(player.client->socket.expectSilence());
}

TEST_F(ClientAuthTest, ReadHelpersLogMissingFieldsAndTheWholeFrameOnShortPackets) {
	// Java BaseClientPacket: "Missing X for: <packet>" per missing field; readB returns zeros; the packet still runs
	LogCapture log({"com.aionemu.chatserver"});
	FakeChatClient client(clientPort);
	client.send(PacketWriter().C(0x30).C(0x40).H(0).frame()); // CM_CHAT_INI without its three D
	EXPECT_EQ(client.expectFrame("SM_CHAT_INI"), SM_CHAT_INI_BYTES); // the packet runs after its read, so the three lines are logged by now
	EXPECT_EQ(log.count("Missing D for: CM_CHAT_INI [opCode=0x30]"), 3) << log.dump();
	// a longer packet: warning with the hex dump of the frame after the opcode, and it still runs
	client.send(PacketWriter().C(0x30).C(0x40).H(0).D(0).D(0).D(0).C(0x7E).frame());
	EXPECT_TRUE(log.waitFor("CM_CHAT_INI [opCode=0x30] was not fully read! Last 1 bytes were not read from buffer: \n"
													"0000: 40 00 00 00 00 00 00 00 00 00 00 00 00 00 00 7E "))
		<< log.dump();
	EXPECT_EQ(client.expectFrame("SM_CHAT_INI"), SM_CHAT_INI_BYTES);
}

} // namespace
} // namespace aion::chatserver::test
