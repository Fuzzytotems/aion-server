// Channel requests of authenticated clients over TCP (CM_CHANNEL_REQUEST / SM_CHANNEL_RESPONSE, CM_CHANNEL_LEAVE) and the packets the chat
// server reads without acting on them (CM_PING, CM_PLAYER_INFO, CM_CHANNEL_CREATE, CM_CHANNEL_JOIN): their exact sizes are checked through the
// "not fully read" warning and the "Missing" errors.

#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "ChatServerTestFixture.h"
#include "aion/chatserver/configs/main/LoggingConfig.h"
#include "support/FakePeers.h"
#include "support/TestUtils.h"

namespace aion::chatserver::test {
namespace {

class ChannelFlowTest : public ChatServerTestFixture {};

TEST_F(ChannelFlowTest, SameIdentifierGivesTheSameChannelAndAnswersWithTheRequestId) {
	auto gs = connectGameServer();
	Player alice = loginPlayer(*gs, "Alice", 0);
	Player bob = loginPlayer(*gs, "Bob", 0);
	std::string identifier = channelIdentifier("public", uniqueName("map"), GS_ID, 0);
	int32_t aliceChannel = alice.client->joinChannel(41, identifier);
	int32_t bobChannel = bob.client->joinChannel(0x7FFFFFFF, identifier);
	EXPECT_EQ(aliceChannel, bobChannel);
	// another map is another channel
	int32_t other = alice.client->joinChannel(42, channelIdentifier("public", uniqueName("map"), GS_ID, 0));
	EXPECT_NE(other, aliceChannel);
}

TEST_F(ChannelFlowTest, ChannelOfTheOtherRaceIsRefusedUnlessStaff) {
	LogCapture log({"com.aionemu.chatserver"});
	auto gs = connectGameServer();
	Player elyos = loginPlayer(*gs, "Ely", 0);
	std::string asmodianChannel = channelIdentifier("trade", uniqueName("map"), GS_ID, 1);
	elyos.client->send(FakeChatClient::buildChannelRequest(5, asmodianChannel));
	EXPECT_TRUE(log.waitFor("requested channel of race: ASMODIANS")) << log.dump();
	EXPECT_TRUE(elyos.client->socket.expectSilence());
	Player gm = loginPlayer(*gs, "Gm", 0, 3);
	EXPECT_GT(gm.client->joinChannel(6, asmodianChannel), 0);
	// the access level is a signed byte compared with 0 (Java: getAccessLevel() == 0), so 0x80..0xFF count as staff as well
	Player negative = loginPlayer(*gs, "Neg", 0, 0xFF);
	EXPECT_GT(negative.client->joinChannel(7, asmodianChannel), 0);
}

TEST_F(ChannelFlowTest, LocalizedJobNamesShareTheChannelOfTheirClass) {
	// the identifier arrives as UTF-16LE: the Korean and the Russian name of the Cleric class give the channel of "job_Cleric"
	auto gs = connectGameServer();
	Player player = loginPlayer(*gs, "Kim", 0);
	std::u16string restrictions = u"\x01" u"77.0.AION.KOR"; // a game server id no other test uses
	int32_t cleric = player.client->joinChannel(1, u"@\x01" u"job_Cleric" + restrictions);
	EXPECT_EQ(player.client->joinChannel(2, u"@\x01" u"job_치유성" + restrictions), cleric);
	EXPECT_EQ(player.client->joinChannel(3, u"@\x01" u"job_Целитель" + restrictions), cleric);
}

TEST_F(ChannelFlowTest, MalformedIdentifiersGetNoResponse) {
	LogCapture log({"com.aionemu.chatserver"});
	auto gs = connectGameServer();
	Player player = loginPlayer(*gs, "Mal", 0);
	// two parts only: null channel, nothing sent
	player.client->send(FakeChatClient::buildChannelRequest(1, "@\x01public_map"));
	EXPECT_TRUE(player.client->socket.expectSilence());
	// no '_' in the type part: Java ArrayIndexOutOfBoundsException, logged by BaseClientPacket.run
	player.client->send(FakeChatClient::buildChannelRequest(2, "@\x01public\x01" "1.0.AION.KOR"));
	EXPECT_TRUE(log.waitFor("Running failed for packet CM_CHANNEL_REQUEST [opCode=0x10]", EXCEPTION_LOG_TIMEOUT)) << log.dump();
	EXPECT_TRUE(log.contains("Index 1 out of bounds for length 1")) << log.dump();
	EXPECT_TRUE(player.client->socket.expectSilence());
}

TEST_F(ChannelFlowTest, LeftChannelDoesNotDeliverMessagesAnymore) {
	LogCapture log({"com.aionemu.chatserver"});
	auto gs = connectGameServer();
	Player alice = loginPlayer(*gs, "Alice", 0);
	Player bob = loginPlayer(*gs, "Bob", 0);
	std::string identifier = channelIdentifier("public", uniqueName("map"), GS_ID, 0);
	int32_t channelId = alice.client->joinChannel(1, identifier);
	bob.client->joinChannel(1, identifier);

	bob.client->send(FakeChatClient::buildChannelLeave(channelId));
	bob.client->send(FakeChatClient::buildChannelLeave(channelId)); // not in it anymore
	EXPECT_TRUE(log.waitFor("Player [name=Bob, id=" + std::to_string(bob.id) + ", race=ELYOS], couldn't leave channel: com.aionemu.chatserver.model.channel.RegionChannel@"))
		<< log.dump();
	EXPECT_FALSE(log.contains("was not fully read")) << log.dump();

	alice.client->send(FakeChatClient::buildChannelMessage(channelId, "hello"));
	EXPECT_EQ(alice.client->expectFrame("SM_CHANNEL_MESSAGE"), expectedChannelMessage(channelId, alice.id, alice.nameIdentifier, "hello"));
	EXPECT_TRUE(bob.client->socket.expectSilence());
}

TEST_F(ChannelFlowTest, LeavingAnUnknownChannelLogsNull) {
	LogCapture log({"com.aionemu.chatserver"});
	auto gs = connectGameServer();
	Player player = loginPlayer(*gs, "Nul", 0);
	player.client->send(FakeChatClient::buildChannelLeave(-5));
	EXPECT_TRUE(log.waitFor(", couldn't leave channel: null (id: -5)")) << log.dump();
}

TEST_F(ChannelFlowTest, UnknownChannelIdIsNotLoggedByDefault) {
	LogCapture log({"com.aionemu.chatserver"});
	auto gs = connectGameServer();
	Player player = loginPlayer(*gs, "Def", 0);
	player.client->send(FakeChatClient::buildChannelMessage(-8, "lost"));
	player.client->joinChannel(4, channelIdentifier("public", uniqueName("quiet"), GS_ID, 0)); // processed after the message
	EXPECT_FALSE(log.contains("No registered channel with id")) << log.dump();
	EXPECT_FALSE(log.contains("requested channel:")) << log.dump();
}

TEST_F(ChannelFlowTest, UnknownChannelIdAndRequestsAreLoggedWhenEnabled) {
	configs::main::LoggingConfig::LOG_CHANNEL_INVALID = true;
	configs::main::LoggingConfig::LOG_CHANNEL_REQUEST = true;
	startServer();
	LogCapture log({"com.aionemu.chatserver"});
	auto gs = connectGameServer();
	Player player = loginPlayer(*gs, "Inv", 0);
	player.client->send(FakeChatClient::buildChannelMessage(-7, "lost"));
	EXPECT_TRUE(log.waitFor("No registered channel with id -7")) << log.dump();
	std::string identifier = channelIdentifier("public", uniqueName("logged"), GS_ID, 0);
	player.client->joinChannel(3, identifier);
	EXPECT_TRUE(log.contains("Player [name=Inv, id=" + std::to_string(player.id) + ", race=ELYOS] requested channel: " + identifier)) << log.dump();
	EXPECT_TRUE(player.client->socket.expectSilence());
}

TEST_F(ChannelFlowTest, PacketsWithoutActionAreReadExactly) {
	LogCapture log({"com.aionemu.chatserver"});
	auto gs = connectGameServer();
	Player player = loginPlayer(*gs, "Quiet", 0);
	player.client->send(FakeChatClient::buildPing());
	player.client->send(FakeChatClient::buildPlayerInfo(3, 65));
	player.client->send(FakeChatClient::buildChannelCreate(9, "private", "pw"));
	player.client->send(FakeChatClient::buildChannelJoin(10, "private", "pw"));
	// a last packet with a response proves the four were processed (the events of a client run in order)
	player.client->joinChannel(11, channelIdentifier("partyFind", "", GS_ID, 0));
	EXPECT_FALSE(log.contains("was not fully read")) << log.dump();
	EXPECT_FALSE(log.contains("Missing")) << log.dump();
	EXPECT_FALSE(log.contains("Unknown packet")) << log.dump();
}

TEST_F(ChannelFlowTest, ShortOrLongPacketsWithoutActionAreDetected) {
	LogCapture log({"com.aionemu.chatserver"});
	auto gs = connectGameServer();
	Player player = loginPlayer(*gs, "Odd", 0);
	Bytes info = FakeChatClient::buildPlayerInfo(3, 65);
	info.pop_back(); // one byte of the 135 missing: readB fails as a whole
	info[0] = static_cast<uint8_t>(info.size());
	info[1] = static_cast<uint8_t>(info.size() >> 8);
	player.client->send(info);
	EXPECT_TRUE(log.waitFor("Missing byte[] for: CM_PLAYER_INFO [opCode=0x2C]")) << log.dump();
	Bytes ping = FakeChatClient::buildPing();
	ping.push_back(0x55);
	ping[0] = static_cast<uint8_t>(ping.size());
	player.client->send(ping);
	EXPECT_TRUE(log.waitFor("CM_PING [opCode=0xFF] was not fully read! Last 1 bytes were not read from buffer")) << log.dump();
}

} // namespace
} // namespace aion::chatserver::test
