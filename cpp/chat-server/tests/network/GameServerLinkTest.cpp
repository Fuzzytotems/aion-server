// The game server side of the chat server over TCP: the handshake (CM_CS_AUTH / SM_GS_AUTH_RESPONSE), the connection states, and the
// registration rules of GameServerService.

#include <memory>

#include <gtest/gtest.h>

#include "ChatServerTestFixture.h"
#include "support/FakePeers.h"
#include "support/TestUtils.h"

namespace aion::chatserver::test {
namespace {

class GameServerLinkTest : public ChatServerTestFixture {};

TEST_F(GameServerLinkTest, RightPasswordAuthenticatesAndAnnouncesTheConnectAddress) {
	LogCapture log({"com.aionemu.chatserver"});
	connectGameServer(); // checks the SM_GS_AUTH_RESPONSE bytes
	EXPECT_TRUE(log.waitFor("Gameserver #1 is now online")) << log.dump();
	EXPECT_TRUE(log.contains("Gameserver connection attempt from: 127.0.0.1"));
}

TEST_F(GameServerLinkTest, WrongPasswordIsRejectedAndTheConnectionStaysUnauthenticated) {
	LogCapture log({"com.aionemu.chatserver"});
	FakeGameServer gs(gsPort);
	Bytes response = gs.authenticate(7, "wrong");
	EXPECT_EQ(response, (Bytes{0x04, 0x00, 0x00, 0x01})) << hex(response); // NOT_AUTHED, no address
	EXPECT_TRUE(log.waitFor("Gameserver #7 (IP: 127.0.0.1) tried to register with an invalid password")) << log.dump();
	// like Java the chat server does not close the connection (the game server does); it stays in state CONNECTED, so a player registration is
	// an unknown packet
	gs.send(FakeGameServer::buildPlayerAuth(nextPlayerId(), "acc", "Nick", 0, 0));
	EXPECT_TRUE(log.waitFor("Unknown packet received from Game Server: 0x01 state CONNECTED")) << log.dump();
	EXPECT_TRUE(gs.socket.expectSilence());
	// and it can still authenticate with the right password
	Bytes second = gs.authenticate(1, GS_PASSWORD);
	EXPECT_EQ(second, (Bytes{0x0B, 0x00, 0x00, 0x00, 0x04, 0x7F, 0x00, 0x00, 0x01, 0x01, 0x28})) << hex(second);
}

TEST_F(GameServerLinkTest, SecondGameServerIsAlreadyRegistered) {
	LogCapture log({"com.aionemu.chatserver"});
	auto first = connectGameServer();
	FakeGameServer second(gsPort);
	Bytes response = second.authenticate(2, GS_PASSWORD);
	EXPECT_EQ(response, (Bytes{0x04, 0x00, 0x00, 0x02})) << hex(response); // ALREADY_REGISTERED
	EXPECT_TRUE(log.waitFor("Gameserver #2 is already registered")) << log.dump();
	// the registration is checked before the password: a wrong password gets ALREADY_REGISTERED as well
	FakeGameServer third(gsPort);
	Bytes wrongPassword = third.authenticate(3, "wrong");
	EXPECT_EQ(wrongPassword, (Bytes{0x04, 0x00, 0x00, 0x02})) << hex(wrongPassword);
}

TEST_F(GameServerLinkTest, DisconnectOfAnyGameServerSetsTheServiceOffline) {
	// Java: GsConnection.onDisconnect calls setOffline for every game server connection, also one rejected as ALREADY_REGISTERED; the registered
	// game server stays connected but the next one can register
	LogCapture log({"com.aionemu.chatserver"});
	auto first = connectGameServer();
	{
		FakeGameServer rejected(gsPort);
		EXPECT_EQ(rejected.authenticate(2, GS_PASSWORD), (Bytes{0x04, 0x00, 0x00, 0x02}));
		rejected.socket.close();
	}
	EXPECT_TRUE(log.waitFor("Gameserver #1 is disconnected")) << log.dump();
	FakeGameServer third(gsPort);
	Bytes response = third.authenticate(3, GS_PASSWORD);
	EXPECT_EQ(response, (Bytes{0x0B, 0x00, 0x00, 0x00, 0x04, 0x7F, 0x00, 0x00, 0x01, 0x01, 0x28})) << hex(response);
}

TEST_F(GameServerLinkTest, UnknownOpcodesAreLoggedPerState) {
	LogCapture log({"com.aionemu.chatserver"});
	FakeGameServer gs(gsPort);
	gs.send(PacketWriter().C(0x05).D(1).frame());
	EXPECT_TRUE(log.waitFor("Unknown packet received from Game Server: 0x05 state CONNECTED")) << log.dump();
	EXPECT_EQ(gs.authenticate(1, GS_PASSWORD).size(), 11u);
	gs.send(PacketWriter().C(0x00).C(1).S(GS_PASSWORD).frame()); // CM_CS_AUTH is only known before the authentication
	EXPECT_TRUE(log.waitFor("Unknown packet received from Game Server: 0x00 state AUTHED")) << log.dump();
	gs.send(PacketWriter().C(0x04).frame());
	EXPECT_TRUE(log.waitFor("Unknown packet received from Game Server: 0x04 state AUTHED")) << log.dump();
	EXPECT_TRUE(gs.socket.expectSilence());
}

TEST_F(GameServerLinkTest, PlayerAuthAnswersWithTheTokenOfTheAccount) {
	auto gs = connectGameServer();
	int32_t playerId = nextPlayerId();
	// the token is 16 random bytes and SHA-256 of the account name; SHA-256("abc") is the FIPS 180-2 test vector
	Bytes token = gs->registerPlayer(playerId, "abc", "Nick", 0);
	Bytes digest(token.begin() + 16, token.end());
	EXPECT_EQ(hex(digest), "BA 78 16 BF 8F 01 CF EA 41 41 40 DE 5D AE 22 23 B0 03 61 A3 96 17 7A 9C B4 10 FF 61 F2 00 15 AD");
	// the random part differs between registrations
	Bytes again = gs->registerPlayer(playerId, "abc", "Nick", 0);
	EXPECT_EQ(Bytes(again.begin() + 16, again.end()), digest);
	EXPECT_NE(Bytes(again.begin(), again.begin() + 16), Bytes(token.begin(), token.begin() + 16));
}

TEST_F(GameServerLinkTest, TheTokenHashesAsManyUtf8BytesAsTheAccountNameHasChars) {
	// Java: md.update(accName.getBytes(UTF_8), 0, accName.length()) - "äb" is C3 A4 62 in UTF-8 but 2 chars long, so only C3 A4 is hashed
	auto gs = connectGameServer();
	int32_t playerId = nextPlayerId();
	gs->send(PacketWriter().C(0x01).D(playerId).S(u"äb").S("Nick").D(0).C(0).frame());
	Bytes token = gs->expectPlayerAuthResponse(playerId);
	Bytes digest(token.begin() + 16, token.end());
	// SHA-256 of the bytes C3 A4
	EXPECT_EQ(hex(digest), "33 E6 D7 3F EE 82 90 4C 8D 7A FB 78 DE 11 54 D1 E8 DC 2A 0E DB 08 12 0E 63 DF 5B 93 85 C2 D9 CC");
}

TEST_F(GameServerLinkTest, TruncatedPlayerAuthIsReadWithDefaults) {
	// commons' read helpers (Java BaseClientPacket) log the missing raceId and accessLevel and return 0, and the packet still runs
	LogCapture log({"com.aionemu.commons.network.packet.BaseClientPacket"});
	auto gs = connectGameServer();
	gs->send(PacketWriter().C(0x01).D(nextPlayerId()).S("acc").S("Nick").frame()); // raceId and accessLevel missing
	EXPECT_TRUE(log.waitFor("Missing D for: [001] CM_PLAYER_AUTH")) << log.dump();
	EXPECT_TRUE(log.waitFor("Missing C for: [001] CM_PLAYER_AUTH")) << log.dump();
	Bytes response = gs->expectFrame("SM_PLAYER_AUTH_RESPONSE");
	EXPECT_EQ(response.size(), 56u) << hex(response);
}

} // namespace
} // namespace aion::chatserver::test
