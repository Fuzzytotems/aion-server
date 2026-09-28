// End to end: the real aion_chat_server executable with a test configuration on ports chosen at run time, a fake game server and two fake
// clients. Both players are registered by the game server, log in with their tokens, join the same channel; one talks and the other receives
// exactly the bytes of Java's SM_CHANNEL_MESSAGE; the game server logs both out; the server shuts down through its stop file and exits
// with 0. The chat log (log/chat.log) and the chatlog table get the message. Also the database options the startup applies.
//
// Needs the test schema (AION_TEST_CS_DATABASE_URL, see support/ChatServerTestDatabase.h), since the server connects to its database at startup.

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "ChildProcess.h"
#include "support/ChatServerTestDatabase.h"
#include "support/FakePeers.h"
#include "support/TestUtils.h"

namespace aion::chatserver::test {
namespace {

namespace fs = std::filesystem;

constexpr const char* GS_PASSWORD = "e2e-secret";

std::string readFile(const fs::path& file) {
	std::ifstream in(file, std::ios::binary);
	std::stringstream content;
	content << in.rdbuf();
	return content.str();
}

/**
 * Creates a working directory like chat-server/ in the test's working directory: ./config (the Java module's files plus a mycs.properties with
 * the given ports and the test database) and ./log.
 */
fs::path createServerDirectory(std::string_view name, uint16_t clientPort, uint16_t gsPort) {
	fs::path dir = fs::current_path() / name;
	fs::remove_all(dir);
	fs::create_directories(dir);
	fs::copy(fs::path(AION_CHATSERVER_JAVA_DIR) / "config", dir / "config", fs::copy_options::recursive);
	std::ofstream mycs(dir / "config" / "mycs.properties", std::ios::binary);
	mycs << "chatserver.network.client.socket_address = 127.0.0.1:" << clientPort << "\n"
			 << "chatserver.network.gameserver.socket_address = 127.0.0.1:" << gsPort << "\n"
			 << "chatserver.network.gameserver.password = " << GS_PASSWORD << "\n"
			 << "chatserver.log.chat = true\n"
			 << "chatserver.log.chat_to_db = true\n"
			 << "database.url = " << database::url() << "\n"
			 << "database.user = " << database::user() << "\n"
			 << "database.password = " << database::password() << "\n";
	return dir;
}

TEST(ChatServerProcessTest, FakeGameServerAndTwoClientsChat) {
	AION_CS_REQUIRE_DATABASE();
	ASSERT_NO_THROW(database::recreateSchema());

	uint16_t clientPort = probeFreePort();
	uint16_t gsPort = probeFreePort();
	while (gsPort == clientPort)
		gsPort = probeFreePort();
	fs::path dir = createServerDirectory("e2e_run", clientPort, gsPort);
	fs::path stopFile = dir / "stop";

	ChildProcess server(AION_CHAT_SERVER_EXECUTABLE, {"--stop-file=" + stopFile.string()}, dir, dir / "console.log");
	ASSERT_TRUE(server.waitForLog("Listening on 127.0.0.1:" + std::to_string(gsPort) + " for game servers", 60s)) << server.readLog();
	EXPECT_TRUE(server.readLog().find("Listening on 127.0.0.1:" + std::to_string(clientPort) + " for Aion game clients") != std::string::npos)
		<< server.readLog();

	// the game server authenticates; the connect address it gets is the client socket address (config/network: connect_address =
	// ${chatserver.network.client.socket_address})
	FakeGameServer gs(gsPort);
	Bytes authResponse = gs.authenticate(1, GS_PASSWORD);
	ASSERT_EQ(authResponse,
		(Bytes{0x0B, 0x00, 0x00, 0x00, 0x04, 0x7F, 0x00, 0x00, 0x01, static_cast<uint8_t>(clientPort), static_cast<uint8_t>(clientPort >> 8)}))
		<< hex(authResponse);
	PacketReader announced(authResponse);
	announced.B(9);
	uint16_t connectPort = static_cast<uint16_t>(announced.H());

	// both players enter the world: the game server registers them and passes the tokens on (SM_CHAT_INIT)
	constexpr int32_t ALICE = 1001;
	constexpr int32_t BOB = 1002;
	Bytes aliceToken = gs.registerPlayer(ALICE, "AliceAccount", "Alice", 0);
	Bytes bobToken = gs.registerPlayer(BOB, "BobAccount", "Bob", 0);

	FakeChatClient alice(connectPort);
	FakeChatClient bob(connectPort);
	alice.login(ALICE, "Alice@AION", "aliceaccount", aliceToken);
	bob.login(BOB, "Bob@AION", "bobaccount", bobToken);

	std::string channel = channelIdentifier("public", "e2e_map", 1, 0);
	int32_t channelId = alice.joinChannel(1, channel);
	EXPECT_EQ(bob.joinChannel(7, channel), channelId);

	alice.send(FakeChatClient::buildChannelMessage(channelId, "Hello from Alice"));
	Bytes received = bob.expectFrame("SM_CHANNEL_MESSAGE");
	// Java SM_CHANNEL_MESSAGE: size, 0x1A, C 0, D 0, D 0, D channel id, D sender id, D 0, C 0, H 10, "Alice@AION", H 16, "Hello from Alice"
	Bytes expected{0x51, 0x00, 0x1A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, static_cast<uint8_t>(channelId),
		static_cast<uint8_t>(channelId >> 8), static_cast<uint8_t>(channelId >> 16), static_cast<uint8_t>(channelId >> 24), 0xE9, 0x03, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x0A, 0x00};
	Bytes identifier = ascii16("Alice@AION");
	expected.insert(expected.end(), identifier.begin(), identifier.end());
	expected.push_back(0x10);
	expected.push_back(0x00);
	Bytes text = ascii16("Hello from Alice");
	expected.insert(expected.end(), text.begin(), text.end());
	ASSERT_EQ(expected.size(), 0x51u); // 29 + 20 + 32
	EXPECT_EQ(received, expected) << hex(received);
	EXPECT_EQ(alice.expectFrame("SM_CHANNEL_MESSAGE"), expected); // the sender is in the channel as well
	EXPECT_TRUE(bob.socket.expectSilence());

	// both leave the world: the game server logs them out and the chat server closes their connections
	gs.send(FakeGameServer::buildPlayerLogout(ALICE));
	gs.send(FakeGameServer::buildPlayerLogout(BOB));
	EXPECT_TRUE(alice.socket.waitClosed());
	EXPECT_TRUE(bob.socket.waitClosed());
	EXPECT_TRUE(server.waitForLog("Player[id=1002] logged out", 10s)) << server.readLog();

	// orderly shutdown through the stop file
	{
		std::ofstream stop(stopFile);
	}
	std::optional<int32_t> exitCode = server.waitForExit(30s);
	ASSERT_TRUE(exitCode.has_value()) << "the chat server did not exit\n" << server.readLog();
	EXPECT_EQ(*exitCode, 0) << server.readLog();
	std::string log = server.readLog();
	EXPECT_NE(log.find("Stop file"), std::string::npos) << log;
	EXPECT_NE(log.find("Gameserver #1 is disconnected"), std::string::npos) << log;
	EXPECT_EQ(log.find("ERROR"), std::string::npos) << log;
	EXPECT_EQ(log.find("WARN"), std::string::npos) << log;

	// chatserver.log.chat and chatserver.log.chat_to_db
	EXPECT_NE(readFile(dir / "log" / "chat.log").find("[REGION (E)] Alice: Hello from Alice"), std::string::npos) << readFile(dir / "log" / "chat.log");
	EXPECT_EQ(database::queryRows("SELECT sender, message, type FROM chatlog"), (std::vector<std::string>{"Alice|Hello from Alice|REGION (E)"}));
}

TEST(ChatServerProcessTest, TheStartupAppliesTheDatabaseSocketTimeout) {
	// the pool is created with the chat server's options (ChatServer::databaseOptions), which take database.socket_timeout: a negative value
	// fails the startup before connecting
	AION_CS_REQUIRE_DATABASE();
	fs::path dir = createServerDirectory("e2e_socket_timeout", probeFreePort(), probeFreePort());
	ChildProcess server(AION_CHAT_SERVER_EXECUTABLE, {"-Ddatabase.socket_timeout=-1"}, dir, dir / "console.log");
	std::optional<int32_t> exitCode = server.waitForExit(30s);
	ASSERT_TRUE(exitCode.has_value()) << "the chat server did not exit\n" << server.readLog();
	EXPECT_EQ(*exitCode, 1) << server.readLog();
	EXPECT_NE(server.readLog().find("database.socket_timeout cannot be negative: -1"), std::string::npos) << server.readLog();
}

} // namespace
} // namespace aion::chatserver::test
