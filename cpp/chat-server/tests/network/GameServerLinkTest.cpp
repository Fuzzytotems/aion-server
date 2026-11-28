// The game server side of the chat server over TCP: the handshake (CM_CS_AUTH / SM_GS_AUTH_RESPONSE), the connection states, and the
// registration rules of GameServerService.

#include <memory>

#include <gtest/gtest.h>

#include "ChatServerTestFixture.h"
#include "aion/chatserver/network/gameserver/GsAuthResponse.h"
#include "aion/chatserver/service/GameServerService.h"
#include "support/FakePeers.h"
#include "support/TestUtils.h"

namespace aion::chatserver::test {
namespace {

using configs::network::NetworkConfig;

class GameServerLinkTest : public ChatServerTestFixture {};

TEST_F(GameServerLinkTest, RightPasswordAuthenticatesAndAnnouncesTheConnectAddress) {
	LogCapture log({"com.aionemu.chatserver"});
	connectGameServer(); // checks the SM_GS_AUTH_RESPONSE bytes
	EXPECT_TRUE(log.waitFor("Game server #1 is now online")) << log.dump();
	EXPECT_TRUE(log.contains("Game server connected: 127.0.0.1"));
}

// upstream 01aadf926: a failed attempt gets its answer and is disconnected
TEST_F(GameServerLinkTest, WrongPasswordIsRejectedAndDisconnected) {
	LogCapture log({"com.aionemu.chatserver"});
	FakeGameServer gs(gsPort);
	Bytes response = gs.authenticate(7, "wrong");
	EXPECT_EQ(response, (Bytes{0x04, 0x00, 0x00, 0x01})) << hex(response); // NOT_AUTHED, no address
	EXPECT_TRUE(log.waitFor("Game server #7 (IP: 127.0.0.1) tried to register with an invalid password")) << log.dump();
	EXPECT_TRUE(gs.socket.waitClosed());
	// the closed connection was never authenticated, so it does not set the service offline, and a game server can still authenticate
	EXPECT_FALSE(log.contains("disconnected")) << log.dump();
	connectGameServer();
}

TEST_F(GameServerLinkTest, AnEmptyPasswordIsOnlyAcceptedFromLoopback) {
	LogCapture log({"com.aionemu.chatserver"});
	NetworkConfig::GAMESERVER_PASSWORD = "";
	struct Restore {
		~Restore() { NetworkConfig::GAMESERVER_PASSWORD = GS_PASSWORD; }
	} restore;
	// no game server is online in this test, so the password rule decides
	auto& service = service::GameServerService::getInstance();
	EXPECT_EQ(service.registerGameServer(5, "", false), network::gameserver::GsAuthResponse::NOT_AUTHED);
	EXPECT_TRUE(log.contains("Rejected game server #5 from a remote address: chatserver.network.gameserver.password is not set")) << log.dump();
	// the fake game server connects from 127.0.0.1
	FakeGameServer gs(gsPort);
	Bytes response = gs.authenticate(1, "");
	EXPECT_EQ(response, (Bytes{0x0B, 0x00, 0x00, 0x00, 0x04, 0x7F, 0x00, 0x00, 0x01, 0x01, 0x28})) << hex(response);
}

TEST_F(GameServerLinkTest, SecondGameServerIsAlreadyRegistered) {
	LogCapture log({"com.aionemu.chatserver"});
	auto first = connectGameServer();
	FakeGameServer second(gsPort);
	Bytes response = second.authenticate(2, GS_PASSWORD);
	EXPECT_EQ(response, (Bytes{0x04, 0x00, 0x00, 0x02})) << hex(response); // ALREADY_REGISTERED
	EXPECT_TRUE(log.waitFor("Game server #2 is already registered")) << log.dump();
	// the registration is checked before the password: a wrong password gets ALREADY_REGISTERED as well
	FakeGameServer third(gsPort);
	Bytes wrongPassword = third.authenticate(3, "wrong");
	EXPECT_EQ(wrongPassword, (Bytes{0x04, 0x00, 0x00, 0x02})) << hex(wrongPassword);
}

// upstream 01aadf926: only the disconnect of the authenticated game server sets the service offline
TEST_F(GameServerLinkTest, OnlyTheAuthenticatedGameServerSetsTheServiceOfflineWhenItDisconnects) {
	LogCapture log({"com.aionemu.chatserver"});
	auto first = connectGameServer();
	FakeGameServer rejected(gsPort);
	EXPECT_EQ(rejected.authenticate(2, GS_PASSWORD), (Bytes{0x04, 0x00, 0x00, 0x02})); // ALREADY_REGISTERED
	EXPECT_TRUE(rejected.socket.waitClosed()); // the chat server closes the rejected connection
	FakeGameServer stillRegistered(gsPort);
	EXPECT_EQ(stillRegistered.authenticate(3, GS_PASSWORD), (Bytes{0x04, 0x00, 0x00, 0x02}));
	EXPECT_FALSE(log.contains("Game server #1 disconnected")) << log.dump();

	first->socket.close();
	EXPECT_TRUE(log.waitFor("Game server #1 disconnected")) << log.dump();
	connectGameServer();
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

TEST_F(GameServerLinkTest, TruncatedPlayerAuthClosesTheConnection) {
	// upstream e1a399e62: commons' read helpers (Java BaseClientPacket) throw on the missing raceId and accessLevel, read() logs the packet as too
	// short and it is not executed; upstream 459b35094: a packet that fails to read closes the connection
	LogCapture log({"com.aionemu.commons.network.packet.BaseClientPacket"});
	auto gs = connectGameServer();
	gs->send(PacketWriter().C(0x01).D(nextPlayerId()).S("acc").S("Nick").frame()); // raceId and accessLevel missing
	EXPECT_TRUE(log.waitFor("[001] CM_PLAYER_AUTH from Game server 127.0.0.1 is shorter than expected")) << log.dump();
	EXPECT_TRUE(gs->socket.waitClosed());
}

} // namespace
} // namespace aion::chatserver::test
