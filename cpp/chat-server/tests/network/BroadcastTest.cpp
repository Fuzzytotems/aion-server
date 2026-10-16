// Who receives SM_CHANNEL_MESSAGE (BroadcastService, CM_CHANNEL_MESSAGE): the members of the channel including the sender, nobody outside it;
// gagged senders and senders within the flood protection time get a notice instead; logged out players get nothing. Also the chat log.

#include <memory>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include "ChatServerTestFixture.h"
#include "aion/chatserver/configs/main/LoggingConfig.h"
#include "aion/commons/utils/TimeUtils.h"
#include "support/FakePeers.h"
#include "support/TestUtils.h"

namespace aion::chatserver::test {
namespace {

class BroadcastTest : public ChatServerTestFixture {};

TEST_F(BroadcastTest, MembersOfTheChannelReceiveTheMessageOthersDoNot) {
	auto gs = connectGameServer();
	Player alice = loginPlayer(*gs, "Alice", 0);
	Player bob = loginPlayer(*gs, "Bob", 0);
	Player carol = loginPlayer(*gs, "Carol", 0); // in another channel of the same type
	Player dave = loginPlayer(*gs, "Dave", 0); // in no channel
	std::string identifier = channelIdentifier("public", uniqueName("map"), GS_ID, 0);
	int32_t channelId = alice.client->joinChannel(1, identifier);
	bob.client->joinChannel(1, identifier);
	carol.client->joinChannel(1, channelIdentifier("public", uniqueName("map"), GS_ID, 0));

	alice.client->send(FakeChatClient::buildChannelMessage(channelId, "Hello Bob"));
	Bytes expected = expectedChannelMessage(channelId, alice.id, "Alice@AION", "Hello Bob");
	Bytes toBob = bob.client->expectFrame("SM_CHANNEL_MESSAGE");
	EXPECT_EQ(toBob, expected) << hex(toBob);
	Bytes toAlice = alice.client->expectFrame("SM_CHANNEL_MESSAGE");
	EXPECT_EQ(toAlice, expected) << hex(toAlice);
	EXPECT_TRUE(carol.client->socket.expectSilence());
	EXPECT_TRUE(dave.client->socket.expectSilence());
}

TEST_F(BroadcastTest, AnyClientCanSendToAChannelById) {
	// Java does not check that the sender is in the channel: the members get the message, the sender does not (it is not in the channel)
	auto gs = connectGameServer();
	Player member = loginPlayer(*gs, "Member", 0);
	Player outsider = loginPlayer(*gs, "Outsider", 1);
	int32_t channelId = member.client->joinChannel(1, channelIdentifier("public", uniqueName("map"), GS_ID, 0));
	outsider.client->send(FakeChatClient::buildChannelMessage(channelId, "psst"));
	EXPECT_EQ(member.client->expectFrame("SM_CHANNEL_MESSAGE"), expectedChannelMessage(channelId, outsider.id, "Outsider@AION", "psst"));
	EXPECT_TRUE(outsider.client->socket.expectSilence());
}

TEST_F(BroadcastTest, FloodProtectionAnswersOnlyTheSender) {
	auto gs = connectGameServer();
	Player alice = loginPlayer(*gs, "Alice", 0);
	Player bob = loginPlayer(*gs, "Bob", 0);
	std::string region = channelIdentifier("public", uniqueName("map"), GS_ID, 0);
	std::string trade = channelIdentifier("trade", uniqueName("map"), GS_ID, 0);
	int32_t regionId = alice.client->joinChannel(1, region);
	int32_t tradeId = alice.client->joinChannel(2, trade);
	bob.client->joinChannel(1, region);
	bob.client->joinChannel(2, trade);

	alice.client->send(FakeChatClient::buildChannelMessage(regionId, "one"));
	EXPECT_EQ(bob.client->expectFrame("first"), expectedChannelMessage(regionId, alice.id, "Alice@AION", "one"));
	EXPECT_EQ(alice.client->expectFrame("first"), expectedChannelMessage(regionId, alice.id, "Alice@AION", "one"));
	// region channels: 1 s between messages (the remaining time rounds down, but at least 1)
	alice.client->send(FakeChatClient::buildChannelMessage(regionId, "two"));
	EXPECT_EQ(alice.client->expectFrame("notice"),
		expectedChannelMessage(regionId, alice.id, "Alice@AION", "You can chat again in this channel in 1 second."));
	EXPECT_TRUE(bob.client->socket.expectSilence());

	// trade channels: 30 s, counted per channel type
	alice.client->send(FakeChatClient::buildChannelMessage(tradeId, "sell"));
	EXPECT_EQ(bob.client->expectFrame("trade"), expectedChannelMessage(tradeId, alice.id, "Alice@AION", "sell"));
	EXPECT_EQ(alice.client->expectFrame("trade"), expectedChannelMessage(tradeId, alice.id, "Alice@AION", "sell"));
	std::this_thread::sleep_for(20ms); // in the same millisecond the remaining time would still be 30 s
	alice.client->send(FakeChatClient::buildChannelMessage(tradeId, "sell again"));
	EXPECT_EQ(alice.client->expectFrame("notice"),
		expectedChannelMessage(tradeId, alice.id, "Alice@AION", "You can chat again in this channel in 29 seconds."));
	EXPECT_TRUE(bob.client->socket.expectSilence());
}

TEST_F(BroadcastTest, GaggedPlayerGetsANoticeAndNobodyElseAnything) {
	LogCapture log({"com.aionemu.chatserver.service.ChatService"});
	auto gs = connectGameServer();
	Player alice = loginPlayer(*gs, "Alice", 0);
	Player bob = loginPlayer(*gs, "Bob", 0);
	std::string identifier = channelIdentifier("public", uniqueName("map"), GS_ID, 0);
	int32_t channelId = alice.client->joinChannel(1, identifier);
	bob.client->joinChannel(1, identifier);

	// the chat server compares the gag time with the current time: 10 minutes 30 seconds from now
	int64_t gagTime = commons::utils::currentTimeMillis() + 630000;
	gs->send(FakeGameServer::buildPlayerGag(alice.id, gagTime));
	EXPECT_TRUE(log.waitFor("Player[id=" + std::to_string(alice.id) + "] was gagged for " + std::to_string(gagTime / 60000) + " minutes")) << log.dump();

	alice.client->send(FakeChatClient::buildChannelMessage(channelId, "can you hear me"));
	EXPECT_EQ(alice.client->expectFrame("gag notice"), expectedChannelMessage(channelId, alice.id, "Alice@AION", "You have been gagged for 10 minutes."));
	EXPECT_TRUE(bob.client->socket.expectSilence());

	// ungag (0)
	gs->send(FakeGameServer::buildPlayerGag(alice.id, 0));
	EXPECT_TRUE(log.waitFor("Player[id=" + std::to_string(alice.id) + "] was gagged for 0 minutes")) << log.dump();
	alice.client->send(FakeChatClient::buildChannelMessage(channelId, "back"));
	EXPECT_EQ(bob.client->expectFrame("SM_CHANNEL_MESSAGE"), expectedChannelMessage(channelId, alice.id, "Alice@AION", "back"));
}

TEST_F(BroadcastTest, TheGagIsCheckedBeforeTheFloodProtection) {
	// right after a message in a trade channel (30 s flood protection) a gagged player gets the gag notice, not the flood notice
	LogCapture log({"com.aionemu.chatserver.service.ChatService"});
	auto gs = connectGameServer();
	Player alice = loginPlayer(*gs, "Alice", 0);
	int32_t tradeId = alice.client->joinChannel(1, channelIdentifier("trade", uniqueName("map"), GS_ID, 0));
	alice.client->send(FakeChatClient::buildChannelMessage(tradeId, "sell"));
	EXPECT_EQ(alice.client->expectFrame("SM_CHANNEL_MESSAGE"), expectedChannelMessage(tradeId, alice.id, "Alice@AION", "sell"));
	int64_t gagTime = commons::utils::currentTimeMillis() + 630000;
	gs->send(FakeGameServer::buildPlayerGag(alice.id, gagTime));
	ASSERT_TRUE(log.waitFor("Player[id=" + std::to_string(alice.id) + "] was gagged for")) << log.dump();
	alice.client->send(FakeChatClient::buildChannelMessage(tradeId, "sell again"));
	EXPECT_EQ(alice.client->expectFrame("gag notice"), expectedChannelMessage(tradeId, alice.id, "Alice@AION", "You have been gagged for 10 minutes."));
}

TEST_F(BroadcastTest, GagWithADurationDoesNotGag) {
	// The Java game server sends the gag duration (ChatBanService: minutes * 60000), the Java chat server compares it with the current time
	// (ChatClient.isGagged): a duration is always in the past, so the player is not gagged. Ported as it is (see the protocol findings).
	LogCapture log({"com.aionemu.chatserver.service.ChatService"});
	auto gs = connectGameServer();
	Player alice = loginPlayer(*gs, "Alice", 0);
	Player bob = loginPlayer(*gs, "Bob", 0);
	std::string identifier = channelIdentifier("public", uniqueName("map"), GS_ID, 0);
	int32_t channelId = alice.client->joinChannel(1, identifier);
	bob.client->joinChannel(1, identifier);
	gs->send(FakeGameServer::buildPlayerGag(alice.id, 5 * 60000));
	EXPECT_TRUE(log.waitFor("Player[id=" + std::to_string(alice.id) + "] was gagged for 5 minutes")) << log.dump();
	alice.client->send(FakeChatClient::buildChannelMessage(channelId, "still talking"));
	EXPECT_EQ(bob.client->expectFrame("SM_CHANNEL_MESSAGE"), expectedChannelMessage(channelId, alice.id, "Alice@AION", "still talking"));
}

TEST_F(BroadcastTest, LogoutClosesTheClientAndStopsItsMessages) {
	LogCapture log({"com.aionemu.chatserver"});
	auto gs = connectGameServer();
	Player alice = loginPlayer(*gs, "Alice", 0);
	Player bob = loginPlayer(*gs, "Bob", 0);
	std::string identifier = channelIdentifier("public", uniqueName("map"), GS_ID, 0);
	int32_t channelId = alice.client->joinChannel(1, identifier);
	bob.client->joinChannel(1, identifier);

	gs->send(FakeGameServer::buildPlayerLogout(bob.id));
	EXPECT_TRUE(bob.client->socket.waitClosed()) << log.dump();
	EXPECT_TRUE(log.waitFor("Player[id=" + std::to_string(bob.id) + "] logged out ")) << log.dump();
	EXPECT_TRUE(log.waitFor("Channel disconnected IP: 127.0.0.1")) << log.dump();

	alice.client->send(FakeChatClient::buildChannelMessage(channelId, "bye"));
	EXPECT_EQ(alice.client->expectFrame("SM_CHANNEL_MESSAGE"), expectedChannelMessage(channelId, alice.id, "Alice@AION", "bye"));

	// a second logout of the same id and a logout of an unknown id are ignored
	gs->send(FakeGameServer::buildPlayerLogout(bob.id));
	gs->send(FakeGameServer::buildPlayerLogout(nextPlayerId()));
	EXPECT_TRUE(gs->socket.expectSilence());
	EXPECT_EQ(log.count("logged out"), 1) << log.dump();
}

TEST_F(BroadcastTest, LogoutOfAPlayerWhoseClientNeverConnectedIsLogged) {
	LogCapture log({"com.aionemu.chatserver"});
	auto gs = connectGameServer();
	int32_t playerId = nextPlayerId();
	gs->registerPlayer(playerId, "acc", "Ghost", 0);
	gs->send(FakeGameServer::buildPlayerLogout(playerId));
	EXPECT_TRUE(log.waitFor("Received logout event without client authentication for player " + std::to_string(playerId))) << log.dump();
}

TEST_F(BroadcastTest, MessageTooBigForTheSendBufferReachesNobody) {
	// the content length is read as chars (H * 2); a declared length beyond the frame gives a zero-filled array of that length (Java readB), and
	// the SM_CHANNEL_MESSAGE of 20000 text bytes does not fit the 16 KiB send buffer: sendPacket throws, BaseClientPacket.run logs it
	LogCapture log({"com.aionemu.chatserver"});
	auto gs = connectGameServer();
	Player alice = loginPlayer(*gs, "Alice", 0);
	Player bob = loginPlayer(*gs, "Bob", 0);
	std::string identifier = channelIdentifier("public", uniqueName("map"), GS_ID, 0);
	int32_t channelId = alice.client->joinChannel(1, identifier);
	bob.client->joinChannel(1, identifier);
	alice.client->send(PacketWriter().C(0x18).H(0).C(0).D(0).D(0).D(0).D(0).D(channelId).C(0).H(10000).B(ascii16("short")).frame());
	EXPECT_TRUE(log.waitFor("Missing byte[] for: CM_CHANNEL_MESSAGE [channelId=" + std::to_string(channelId) + ", content=null]")) << log.dump();
	EXPECT_TRUE(log.waitFor("Running failed for packet CM_CHANNEL_MESSAGE [channelId=" + std::to_string(channelId) + ", content=[0, 0, 0",
		EXCEPTION_LOG_TIMEOUT))
		<< log.dump();
	EXPECT_TRUE(alice.client->socket.expectSilence());
	EXPECT_TRUE(bob.client->socket.expectSilence());
}

TEST_F(BroadcastTest, DeclaredLengthsAbove32767CharsAreUnsigned) {
	// Java readH is unsigned: H 0x8000 declares 65536 bytes, readB logs the missing bytes and returns zeros, and the packet still runs (its answer
	// does not fit the send buffer)
	LogCapture log({"com.aionemu.chatserver"});
	auto gs = connectGameServer();
	Player alice = loginPlayer(*gs, "Alice", 0);
	int32_t channelId = alice.client->joinChannel(1, channelIdentifier("public", uniqueName("map"), GS_ID, 0));
	alice.client->send(PacketWriter().C(0x18).H(0).C(0).D(0).D(0).D(0).D(0).D(channelId).C(0).H(0x8000).B(ascii16("short")).frame());
	EXPECT_TRUE(log.waitFor("Missing byte[] for: CM_CHANNEL_MESSAGE [channelId=" + std::to_string(channelId) + ", content=null]")) << log.dump();
	EXPECT_TRUE(log.waitFor("Running failed for packet CM_CHANNEL_MESSAGE [channelId=" + std::to_string(channelId), EXCEPTION_LOG_TIMEOUT))
		<< log.dump();
	EXPECT_FALSE(log.contains("Reading failed")) << log.dump();
}

TEST_F(BroadcastTest, ContentBytesAreLoggedAsSignedBytes) {
	// Java: Arrays.toString(byte[]) in CM_CHANNEL_MESSAGE.toString, here in the "not fully read" warning of a packet with one extra byte
	LogCapture log({"com.aionemu.chatserver"});
	auto gs = connectGameServer();
	Player alice = loginPlayer(*gs, "Alice", 0);
	int32_t channelId = alice.client->joinChannel(1, channelIdentifier("public", uniqueName("map"), GS_ID, 0));
	alice.client->send(PacketWriter().C(0x18).H(0).C(0).D(0).D(0).D(0).D(0).D(channelId).C(0).H(1).B({0xFF, 0x80}).C(0x7F).frame());
	EXPECT_TRUE(log.waitFor("CM_CHANNEL_MESSAGE [channelId=" + std::to_string(channelId) + ", content=[-1, -128]] was not fully read!")) << log.dump();
	EXPECT_EQ(alice.client->expectFrame("SM_CHANNEL_MESSAGE"), expectedChannelMessage(channelId, alice.id, "Alice@AION", u"胿"));
}

TEST_F(BroadcastTest, ChatLogWritesChannelNameSenderAndText) {
	configs::main::LoggingConfig::LOG_CHAT = true;
	startServer();
	LogCapture log({"CHAT_LOG"});
	auto gs = connectGameServer();
	Player alice = loginPlayer(*gs, "Alice", 1);
	int32_t regionId = alice.client->joinChannel(1, channelIdentifier("public", uniqueName("map"), GS_ID, 1));
	alice.client->send(FakeChatClient::buildChannelMessage(regionId, "hi all"));
	alice.client->expectFrame("SM_CHANNEL_MESSAGE");
	EXPECT_TRUE(log.waitFor("info|CHAT_LOG|[REGION (A)] Alice: hi all")) << log.dump();
	// any text: UTF-16LE on the wire, UTF-8 in the log (and the chatlog table, which gets the same string)
	std::this_thread::sleep_for(1100ms); // flood protection of region channels
	alice.client->send(FakeChatClient::buildChannelMessage(regionId, u"Привет, 친구"));
	EXPECT_EQ(alice.client->expectFrame("SM_CHANNEL_MESSAGE"), expectedChannelMessage(regionId, alice.id, "Alice@AION", u"Привет, 친구"));
	EXPECT_TRUE(log.waitFor("info|CHAT_LOG|[REGION (A)] Alice: Привет, 친구")) << log.dump();
	int32_t jobId = alice.client->joinChannel(2, channelIdentifier("job", "Kleriker", GS_ID, 1));
	alice.client->send(FakeChatClient::buildChannelMessage(jobId, "heal"));
	alice.client->expectFrame("SM_CHANNEL_MESSAGE");
	EXPECT_TRUE(log.waitFor("info|CHAT_LOG|[Cleric (A)] Alice: heal")) << log.dump();
	int32_t langId = alice.client->joinChannel(3, channelIdentifier("User", uniqueName("lang"), GS_ID, 1));
	alice.client->send(FakeChatClient::buildChannelMessage(langId, "hallo"));
	alice.client->expectFrame("SM_CHANNEL_MESSAGE");
	EXPECT_TRUE(log.waitFor("info|CHAT_LOG|[LANG: lang")) << log.dump();
	EXPECT_TRUE(log.contains(" (A)] Alice: hallo")) << log.dump();
}

TEST_F(BroadcastTest, NoChatLogByDefault) {
	LogCapture log({"CHAT_LOG"});
	auto gs = connectGameServer();
	Player alice = loginPlayer(*gs, "Alice", 0);
	int32_t regionId = alice.client->joinChannel(1, channelIdentifier("public", uniqueName("map"), GS_ID, 0));
	alice.client->send(FakeChatClient::buildChannelMessage(regionId, "unlogged"));
	alice.client->expectFrame("SM_CHANNEL_MESSAGE");
	EXPECT_EQ(log.dump(), "");
}

} // namespace
} // namespace aion::chatserver::test
