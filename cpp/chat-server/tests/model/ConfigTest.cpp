// Config.load with the chat server's config directory (copied from the Java module into a scratch working directory), the logging settings and
// the command line of ChatServer.

#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

#include "aion/chatserver/ChatServer.h"
#include "aion/chatserver/configs/Config.h"
#include "aion/chatserver/configs/main/LoggingConfig.h"
#include "aion/chatserver/configs/network/NetworkConfig.h"
#include "aion/commons/configs/DatabaseConfig.h"
#include "aion/commons/utils/Exception.h"
#include "support/TestUtils.h"

namespace aion::chatserver::test {
namespace {

namespace fs = std::filesystem;
using configs::Config;
using configs::main::LoggingConfig;
using configs::network::NetworkConfig;

/** Runs the test in <test work dir>/<name> with a copy of chat-server/config (Config reads ./config), restoring the working directory after. */
class ConfigTest : public ::testing::Test {
protected:
	void SetUp() override {
		previous = fs::current_path();
		dir = previous / ("config_" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
		fs::remove_all(dir);
		fs::create_directories(dir);
		fs::copy(fs::path(AION_CHATSERVER_JAVA_DIR) / "config", dir / "config", fs::copy_options::recursive);
		fs::current_path(dir);
	}

	void TearDown() override {
		fs::current_path(previous);
		std::error_code ignored;
		fs::remove_all(dir, ignored);
	}

	void writeMycs(std::string_view content) {
		std::ofstream out(dir / "config" / "mycs.properties", std::ios::binary);
		out << content;
	}

	fs::path previous;
	fs::path dir;
};

TEST_F(ConfigTest, JavaDefaults) {
	LogCapture log({"com.aionemu.chatserver.configs.Config"});
	Config::load();
	EXPECT_EQ(NetworkConfig::CLIENT_SOCKET_ADDRESS.toString(), "0.0.0.0:10241");
	EXPECT_EQ(NetworkConfig::GAMESERVER_SOCKET_ADDRESS.toString(), "0.0.0.0:9021");
	EXPECT_EQ(NetworkConfig::GAMESERVER_PASSWORD, "");
	EXPECT_EQ(NetworkConfig::NIO_READ_WRITE_THREADS, 1);
	EXPECT_FALSE(LoggingConfig::LOG_CHANNEL_REQUEST);
	EXPECT_FALSE(LoggingConfig::LOG_CHANNEL_INVALID);
	EXPECT_FALSE(LoggingConfig::LOG_CHAT);
	EXPECT_FALSE(LoggingConfig::LOG_CHAT_TO_DB);
	EXPECT_EQ(commons::configs::DatabaseConfig::DATABASE_URL, "jdbc:mysql://localhost:3306/aion_cs?serverTimezone=&characterEncoding=UTF-8");
	// the connect address follows the socket address (0.0.0.0:10241), so a local IPv4 address is used instead of the wildcard
	EXPECT_FALSE(NetworkConfig::CLIENT_CONNECT_ADDRESS.isAnyLocalAddress());
	EXPECT_EQ(NetworkConfig::CLIENT_CONNECT_ADDRESS.port, 10241);
	EXPECT_TRUE(log.contains("No connect IP for Aion client configured, using " + NetworkConfig::CLIENT_CONNECT_ADDRESS.host)) << log.dump();
	EXPECT_TRUE(log.contains("Loading default configuration values from: ./config/main/*")) << log.dump();
	EXPECT_TRUE(log.contains("Loading: ./config/mycs.properties")) << log.dump();
	EXPECT_TRUE(log.contains("No override properties found")) << log.dump();
	// the Discord settings of logging.properties are read by logback.xml in Java, so they are not unknown
	EXPECT_FALSE(log.contains("is unknown")) << log.dump();
}

TEST_F(ConfigTest, OverrideFileAndReferences) {
	writeMycs("chatserver.network.client.socket_address = 127.0.0.1:5555\n"
						"chatserver.network.gameserver.password = secret\n"
						"chatserver.log.chat = true\n"
						"chatserver.network.nio.threads = 3\n"
						"chatserver.unknown.key = 1\n");
	LogCapture log({"com.aionemu.chatserver.configs.Config"});
	Config::load();
	EXPECT_EQ(NetworkConfig::CLIENT_SOCKET_ADDRESS.toString(), "127.0.0.1:5555");
	EXPECT_EQ(NetworkConfig::CLIENT_CONNECT_ADDRESS.toString(), "127.0.0.1:5555"); // ${chatserver.network.client.socket_address}
	EXPECT_EQ(NetworkConfig::GAMESERVER_PASSWORD, "secret");
	EXPECT_EQ(NetworkConfig::NIO_READ_WRITE_THREADS, 3);
	EXPECT_TRUE(LoggingConfig::LOG_CHAT);
	EXPECT_TRUE(log.contains("Config property chatserver.unknown.key is unknown and therefore ignored.")) << log.dump();
	EXPECT_FALSE(log.contains("No connect IP")) << log.dump();
}

TEST_F(ConfigTest, CommandLineOverridesWin) {
	writeMycs("chatserver.network.gameserver.password = fromfile\n");
	commons::configuration::Properties overrides;
	overrides.setProperty("chatserver.network.gameserver.password", "fromcommandline");
	overrides.setProperty("chatserver.network.client.connect_address", "10.0.0.1:1234");
	overrides.setProperty("no.such.key", "x");
	LogCapture log({"com.aionemu.chatserver.configs.Config"});
	Config::load(overrides);
	EXPECT_EQ(NetworkConfig::GAMESERVER_PASSWORD, "fromcommandline");
	EXPECT_EQ(NetworkConfig::CLIENT_CONNECT_ADDRESS.toString(), "10.0.0.1:1234");
	EXPECT_TRUE(log.contains("Applied override properties from the command line: chatserver.network.client.connect_address, "
													 "chatserver.network.gameserver.password"))
		<< log.dump();
	EXPECT_TRUE(log.contains("Config property no.such.key is unknown and therefore ignored.")) << log.dump();
}

TEST_F(ConfigTest, MissingConfigDirectoryFails) {
	fs::remove_all(dir / "config");
	try {
		Config::load();
		FAIL() << "no exception";
	} catch (const commons::utils::Exception& e) {
		EXPECT_EQ(std::string(e.what()), "Can't load chatserver configuration:");
	}
}

TEST_F(ConfigTest, UnresolvableConnectAddressFailsTheStartup) {
	// Java resolves the address when binding it; its InetAddress is null and Config.load throws a NullPointerException
	writeMycs("chatserver.network.client.connect_address = no-such-host.invalid:10241\n");
	EXPECT_THROW(Config::load(), commons::utils::IOException);
}

TEST_F(ConfigTest, LoggingConfigReadsTheLogbackProperties) {
	writeMycs("chatserver.log.status.discord.webhook_url = https://example.invalid/hook  \n"
						"chatserver.log.status.discord.avatar_url = avatar-%level\n");
	commons::logging::Logging::Config config = Config::loadLoggingConfig();
	EXPECT_EQ(config.statusDiscordWebhookUrl, "https://example.invalid/hook");
	EXPECT_EQ(config.statusDiscordAvatarUrl, "avatar-%level");
	EXPECT_EQ(Config::loadLogbackProperties().getProperty("chatserver.log.chat.discord.webhook_url", "missing"), "");
	std::vector<std::string> keys = Config::getLogbackPropertyKeys();
	EXPECT_EQ(keys.size(), 4u);
}

TEST(ChatServerArgumentsTest, OverridesStopFileAndUnknownArguments) {
	std::vector<std::string_view> args{"-Dchatserver.network.gameserver.password=a=b", "--stop-file=C:/tmp/stop", "-D=x", "other"};
	ChatServer::Arguments arguments = ChatServer::parseArguments(args);
	EXPECT_EQ(arguments.overrides.getProperty("chatserver.network.gameserver.password", ""), "a=b");
	ASSERT_TRUE(arguments.stopFile.has_value());
	EXPECT_EQ(arguments.stopFile->string(), "C:/tmp/stop");
	EXPECT_EQ(arguments.unknownArguments, (std::vector<std::string>{"-D=x", "other"}));
}

TEST(ChatServerDatabaseOptionsTest, SocketTimeoutIs60SecondsUnlessConfigured) {
	using commons::database::DatabaseFactory;
	DatabaseFactory::Options options = ChatServer::databaseOptions();
	EXPECT_EQ(DatabaseFactory::resolveSocketTimeout("jdbc:mysql://localhost:3306/aion_cs", options), std::chrono::seconds(60));
	EXPECT_EQ(DatabaseFactory::resolveSocketTimeout("jdbc:mysql://localhost:3306/aion_cs?socketTimeout=5000", options), std::chrono::seconds(5));
	// no timeout is still possible on request (database.socket_timeout = 0, which DatabaseFactory::init puts into the options)
	options.socketTimeout = std::chrono::milliseconds(0);
	EXPECT_EQ(DatabaseFactory::resolveSocketTimeout("jdbc:mysql://localhost:3306/aion_cs", options), std::chrono::milliseconds(0));
}

} // namespace
} // namespace aion::chatserver::test
