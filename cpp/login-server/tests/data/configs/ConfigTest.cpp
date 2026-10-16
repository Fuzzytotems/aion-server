#include <filesystem>

#include <gtest/gtest.h>

#include "DataTestUtils.h"
#include "aion/commons/configs/DatabaseConfig.h"
#include "aion/commons/configuration/TransformationException.h"
#include "aion/commons/utils/Exception.h"
#include "aion/loginserver/configs/Config.h"

using namespace aion::loginserver;
using namespace aion::loginserver::test;
using aion::commons::configs::DatabaseConfig;
using aion::commons::utils::InetSocketAddress;
using configs::Config;
namespace fs = std::filesystem;

namespace {

const char* const CONFIG_LOGGER = "com.aionemu.loginserver.configs.Config";

/** Resets the fields to values that differ from all defaults, so the tests see which fields load() assigned */
void setSentinels() {
	Config::CLIENT_SOCKET_ADDRESS = {"sentinel", 1};
	Config::GAMESERVER_SOCKET_ADDRESS = {"sentinel", 2};
	Config::LOGIN_TRY_BEFORE_BAN = -1;
	Config::WRONG_LOGIN_BAN_TIME = -1;
	Config::NIO_READ_WRITE_THREADS = -1;
	Config::ACCOUNT_AUTO_CREATION = false;
	Config::EXTERNAL_AUTH_URL = "sentinel";
	Config::ENABLE_BRUTEFORCE_PROTECTION = false;
	Config::LOG_LOGINS = true;
}

class ConfigTest : public ::testing::Test {
protected:
	void TearDown() override {
		// other tests of this executable (DAOs) expect the default: no external authentication
		Config::EXTERNAL_AUTH_URL.clear();
		Config::LOG_LOGINS = false;
	}
};

/**
 * Copies the shipped default configuration (config/main and config/network of the Java server directory) into dir. config/myls.properties,
 * which an operator creates to configure the real server, is left out.
 */
void copyShippedConfig(const TempDirectory& dir) {
	fs::path javaConfig = fs::path(AION_LOGINSERVER_JAVA_DIR) / "config";
	ASSERT_TRUE(fs::is_directory(javaConfig / "main")) << javaConfig;
	ASSERT_TRUE(fs::is_directory(javaConfig / "network")) << javaConfig;
	fs::create_directories(dir.path / "config");
	for (const char* subdirectory : {"main", "network"})
		fs::copy(javaConfig / subdirectory, dir.path / "config" / subdirectory, fs::copy_options::recursive);
}

} // namespace

TEST_F(ConfigTest, LoadsShippedConfigDirectory) {
	TempDirectory dir;
	ASSERT_NO_FATAL_FAILURE(copyShippedConfig(dir));
	setSentinels();
	LogCapture log(CONFIG_LOGGER);
	{
		ScopedCurrentPath cwd(dir.path);
		Config::load();
	}

	EXPECT_EQ(Config::CLIENT_SOCKET_ADDRESS, (InetSocketAddress{"0.0.0.0", 2106}));
	EXPECT_EQ(Config::GAMESERVER_SOCKET_ADDRESS, (InetSocketAddress{"0.0.0.0", 9014}));
	EXPECT_EQ(Config::LOGIN_TRY_BEFORE_BAN, 5);
	EXPECT_EQ(Config::WRONG_LOGIN_BAN_TIME, 15);
	EXPECT_EQ(Config::NIO_READ_WRITE_THREADS, 0);
	EXPECT_TRUE(Config::ACCOUNT_AUTO_CREATION);
	EXPECT_EQ(Config::EXTERNAL_AUTH_URL, "");
	EXPECT_FALSE(Config::useExternalAuth());
	EXPECT_TRUE(Config::ENABLE_BRUTEFORCE_PROTECTION);
	EXPECT_FALSE(Config::LOG_LOGINS);
	EXPECT_EQ(DatabaseConfig::DATABASE_URL, "jdbc:mysql://localhost:3306/aion_ls?serverTimezone=&characterEncoding=UTF-8");
	EXPECT_EQ(DatabaseConfig::DATABASE_USER, "root");
	EXPECT_EQ(DatabaseConfig::DATABASE_PASSWORD, "");
	EXPECT_EQ(DatabaseConfig::DATABASE_CONNECTIONS_MAX, 5);
	EXPECT_EQ(DatabaseConfig::DATABASE_TIMEOUT, 5000);

	EXPECT_TRUE(log.contains("info|Loading default configuration values from: ./config/main/*\n")) << log.str();
	EXPECT_TRUE(log.contains("info|Loading default configuration values from: ./config/network/*\n")) << log.str();
	EXPECT_TRUE(log.contains("info|Loading: ./config/myls.properties\n")) << log.str();
	EXPECT_TRUE(log.contains("info|No override properties found\n")) << log.str();
	// the discord properties in config/main/logging.properties are used by Logging and must not be reported
	EXPECT_FALSE(log.contains("unknown")) << log.str();
	EXPECT_FALSE(log.contains("warning|")) << log.str();
}

TEST_F(ConfigTest, LoadsLoggingConfigOfShippedConfigDirectory) {
	TempDirectory dir;
	ASSERT_NO_FATAL_FAILURE(copyShippedConfig(dir));
	ScopedCurrentPath cwd(dir.path);
	aion::commons::logging::Logging::Config config = Config::loadLoggingConfig();
	EXPECT_EQ(config.statusDiscordWebhookUrl, "");
	EXPECT_EQ(config.statusDiscordAvatarUrl, "");
	EXPECT_EQ(config.logFolder, fs::path("log"));
	EXPECT_TRUE(config.archiveLogs);
}

TEST_F(ConfigTest, OverridesDefaultsAndWarnsAboutUnknownProperties) {
	TempDirectory dir;
	dir.write("config/main/logging.properties", "loginserver.log.status.discord.webhook_url = https://discord.test/hook\n"
																							"loginserver.log.status.discord.avatar_url = https://avatar.test/%level.png   \n"
																							"loginserver.log.logins = false\n");
	dir.write("config/network/network.properties", "loginserver.network.client.socket_address = 127.0.0.1:1234\n"
																								 "loginserver.network.nio.threads = 3\n"
																								 "unknown.network.key = 1\n");
	dir.write("config/network/ignored.txt", "loginserver.network.client.logintrybeforeban = 99\n");
	dir.write("config/myls.properties", "loginserver.log.logins = true\n"
																			"loginserver.accounts.external_auth.url = http://auth.test/login\n"
																			"loginserver.accounts.autocreate = false\n"
																			"loginserver.log.status.discord.webhook_url = https://override.test/hook\n"
																			"my.unknown = x\n");
	setSentinels();
	LogCapture log(CONFIG_LOGGER);
	aion::commons::logging::Logging::Config loggingConfig;
	{
		ScopedCurrentPath cwd(dir.path);
		loggingConfig = Config::loadLoggingConfig();
		EXPECT_EQ(log.str(), ""); // loadLoggingConfig logs nothing
		Config::load();
	}

	EXPECT_EQ(Config::CLIENT_SOCKET_ADDRESS, (InetSocketAddress{"127.0.0.1", 1234}));
	EXPECT_EQ(Config::GAMESERVER_SOCKET_ADDRESS, (InetSocketAddress{"0.0.0.0", 9014})); // default
	EXPECT_EQ(Config::LOGIN_TRY_BEFORE_BAN, 5);																				// default, .txt files are not loaded
	EXPECT_EQ(Config::WRONG_LOGIN_BAN_TIME, 15);
	EXPECT_EQ(Config::NIO_READ_WRITE_THREADS, 3);
	EXPECT_FALSE(Config::ACCOUNT_AUTO_CREATION);
	EXPECT_EQ(Config::EXTERNAL_AUTH_URL, "http://auth.test/login");
	EXPECT_TRUE(Config::useExternalAuth());
	EXPECT_TRUE(Config::ENABLE_BRUTEFORCE_PROTECTION);
	EXPECT_TRUE(Config::LOG_LOGINS);

	EXPECT_FALSE(log.contains("No override properties found")) << log.str();
	EXPECT_TRUE(log.contains("warning|Config property my.unknown is unknown and therefore ignored.\n")) << log.str();
	EXPECT_TRUE(log.contains("warning|Config property unknown.network.key is unknown and therefore ignored.\n")) << log.str();
	EXPECT_FALSE(log.contains("discord")) << log.str();

	// logback.xml: logging.properties, overridden by myls.properties, values trimmed
	EXPECT_EQ(loggingConfig.statusDiscordWebhookUrl, "https://override.test/hook");
	EXPECT_EQ(loggingConfig.statusDiscordAvatarUrl, "https://avatar.test/%level.png");
}

TEST_F(ConfigTest, NoOverrideFileAndNoLoggingProperties) {
	TempDirectory dir;
	dir.write("config/main/empty.properties", "");
	dir.write("config/network/network.properties", "loginserver.network.client.logintrybeforeban = 7\n");
	LogCapture log(CONFIG_LOGGER);
	aion::commons::logging::Logging::Config loggingConfig;
	{
		ScopedCurrentPath cwd(dir.path);
		loggingConfig = Config::loadLoggingConfig();
		Config::load();
	}
	EXPECT_EQ(Config::LOGIN_TRY_BEFORE_BAN, 7);
	EXPECT_TRUE(log.contains("info|No override properties found\n")) << log.str();
	EXPECT_FALSE(log.contains("warning|")) << log.str();
	EXPECT_EQ(loggingConfig.statusDiscordWebhookUrl, "");
	EXPECT_EQ(loggingConfig.statusDiscordAvatarUrl, "");
}

TEST_F(ConfigTest, MissingConfigDirectoryThrows) {
	TempDirectory dir;
	LogCapture log(CONFIG_LOGGER);
	ScopedCurrentPath cwd(dir.path);
	try {
		Config::load();
		FAIL() << "no exception";
	} catch (const aion::commons::utils::Exception& e) {
		EXPECT_STREQ(e.what(), "Can't load loginserver configuration:");
		EXPECT_TRUE(e.cause() != nullptr);
		EXPECT_THROW(std::rethrow_exception(e.cause()), aion::commons::utils::IOException);
	}
}

TEST_F(ConfigTest, InvalidValueThrows) {
	TempDirectory dir;
	dir.write("config/main/logging.properties", "");
	dir.write("config/network/network.properties", "loginserver.network.nio.threads = many\n");
	LogCapture log(CONFIG_LOGGER);
	ScopedCurrentPath cwd(dir.path);
	EXPECT_THROW(Config::load(), aion::commons::configuration::TransformationException);
}

TEST_F(ConfigTest, UseExternalAuthIgnoresBlankUrl) {
	Config::EXTERNAL_AUTH_URL = "";
	EXPECT_FALSE(Config::useExternalAuth());
	Config::EXTERNAL_AUTH_URL = " \t ";
	EXPECT_FALSE(Config::useExternalAuth());
	Config::EXTERNAL_AUTH_URL = " http://x ";
	EXPECT_TRUE(Config::useExternalAuth());
}
