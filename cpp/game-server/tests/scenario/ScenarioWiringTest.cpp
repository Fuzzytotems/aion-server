// Build wiring of the M5a scenario harness (chunk P5-SC, m5a-plan.md I-01): the test executable sees the login client crypto of
// login-server/tests/support and the game client of tests/support by name, links aion_loginserver_crypto, and knows the paths of both server
// executables (built before it) and of the Java module directories. Written by the integrator pre-stage; the harness lane (F-04) owns the file.

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "AionLoginClientCrypto.h"
#include "FakeGameClient.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace {

TEST(ScenarioWiringTest, ServerExecutablesAndJavaDirectoriesAreKnown) {
	const std::filesystem::path gameServer(AION_GAME_SERVER_EXECUTABLE);
	const std::filesystem::path loginServer(AION_LOGIN_SERVER_EXECUTABLE);
	EXPECT_EQ(gameServer.stem(), "aion_game_server");
	EXPECT_EQ(loginServer.stem(), "aion_login_server");
	EXPECT_TRUE(std::filesystem::is_regular_file(gameServer)) << gameServer << ": the test depends on aion_game_server";
	EXPECT_TRUE(std::filesystem::is_regular_file(loginServer)) << loginServer << ": the test depends on aion_login_server";
	EXPECT_TRUE(std::filesystem::is_regular_file(std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "sql" / "aion_gs.sql"));
	EXPECT_TRUE(std::filesystem::is_regular_file(std::filesystem::path(AION_LOGINSERVER_JAVA_DIR) / "sql" / "aion_ls.sql"));
	EXPECT_FALSE(std::string(AION_SCENARIO_OUTPUT_DIR).empty());
}

TEST(ScenarioWiringTest, ClientProtocolSupportLinks) {
	using aion::gameserver::network::test::FakeGameClientCrypto;
	// the SM_KEY opcode is sent unencrypted; its wire value only depends on the protocol constants
	EXPECT_EQ(FakeGameClientCrypto::serverWireOpcode(FakeGameClientCrypto::SM_KEY_OPCODE),
		static_cast<uint16_t>((FakeGameClientCrypto::SM_KEY_OPCODE + FakeGameClientCrypto::INTERNAL_VERSION) ^ 0xDF));

	using aion::loginserver::test::AionLoginClientCrypto;
	AionLoginClientCrypto loginCrypto; // Blowfish with the static initial key: links aion_loginserver_crypto
	const std::vector<uint8_t> body = {0x01, 0x02, 0x03};
	const std::vector<uint8_t> frame = AionLoginClientCrypto::makeFrame(body);
	EXPECT_EQ(frame, (std::vector<uint8_t>{0x05, 0x00, 0x01, 0x02, 0x03}));
	EXPECT_EQ(AionLoginClientCrypto::frameBody(frame), body);
}

} // namespace
