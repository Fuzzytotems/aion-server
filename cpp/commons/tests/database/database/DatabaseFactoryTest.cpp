#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <string>

#include "aion/commons/configs/DatabaseConfig.h"
#include "aion/commons/database/DB.h"
#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/utils/Exception.h"

using namespace aion::commons::database;

// These tests need no database server: they cover the behaviour when the database is unreachable or not configured.

namespace {

/** a local port where no server listens */
const char* UNREACHABLE_URL = "jdbc:mysql://127.0.0.1:1/aion_test?connectTimeout=2000";

} // namespace

TEST(DatabaseFactoryTest, GetConnectionBeforeInit) {
	DatabaseFactory::shutdown();
	EXPECT_FALSE(DatabaseFactory::isInitialized());
	try {
		DatabaseFactory::getConnection();
		FAIL();
	} catch (const SQLException& e) {
		EXPECT_EQ(std::string(e.what()), "DatabaseFactory is not initialized");
	}
	EXPECT_EQ(DatabaseFactory::getPool(), nullptr);
}

TEST(DatabaseFactoryTest, RejectsInvalidPoolSettings) {
	EXPECT_THROW(DatabaseFactory::init(UNREACHABLE_URL, "root", "", 0, 5000), aion::commons::utils::IllegalArgumentException);
	EXPECT_THROW(DatabaseFactory::init(UNREACHABLE_URL, "root", "", 5, 249), aion::commons::utils::IllegalArgumentException);
	EXPECT_THROW(DatabaseFactory::init("jdbc:oracle:thin:@localhost", "root", "", 5, 5000), SQLException);
	EXPECT_FALSE(DatabaseFactory::isInitialized());
}

TEST(DatabaseFactoryTest, InitFailsFastWhenDatabaseIsUnreachable) {
	try {
		DatabaseFactory::init(UNREACHABLE_URL, "root", "", 5, 5000);
		DatabaseFactory::shutdown();
		FAIL() << "a server is listening on port 1?";
	} catch (const SQLException& e) {
		std::string message = e.what();
		EXPECT_TRUE(message.starts_with("Failed to initialize pool: ")) << message;
		EXPECT_TRUE(e.getErrorCode() == 2002 || e.getErrorCode() == 2003) << e.getErrorCode();
		EXPECT_EQ(e.getSQLState(), "08S01");
		EXPECT_TRUE(e.cause());
	}
	EXPECT_FALSE(DatabaseFactory::isInitialized());
}

TEST(DatabaseFactoryTest, ResolveSocketTimeout) {
	using namespace std::chrono_literals;
	using Options = DatabaseFactory::Options;
	const char* plainUrl = "jdbc:mysql://127.0.0.1:1/aion_test";
	const char* urlWithTimeout = "jdbc:mysql://127.0.0.1:1/aion_test?socketTimeout=1500";
	const char* urlWithZeroTimeout = "jdbc:mysql://127.0.0.1:1/aion_test?SOCKETTIMEOUT=0";

	// default options: like Connector/J (URL parameter or none)
	EXPECT_EQ(DatabaseFactory::resolveSocketTimeout(plainUrl, Options{}), 0ms);
	EXPECT_EQ(DatabaseFactory::resolveSocketTimeout(urlWithTimeout, Options{}), 1500ms);

	// precedence: explicit option (database.socket_timeout), then URL parameter, then the server default
	Options withDefault{.socketTimeout = std::nullopt, .defaultSocketTimeout = 60s, .requireSocketTimeout = false};
	EXPECT_EQ(DatabaseFactory::resolveSocketTimeout(plainUrl, withDefault), 60s);
	EXPECT_EQ(DatabaseFactory::resolveSocketTimeout(urlWithTimeout, withDefault), 1500ms);
	EXPECT_EQ(DatabaseFactory::resolveSocketTimeout(urlWithZeroTimeout, withDefault), 0ms);
	Options explicitTimeout{.socketTimeout = 2500ms, .defaultSocketTimeout = 60s, .requireSocketTimeout = false};
	EXPECT_EQ(DatabaseFactory::resolveSocketTimeout(urlWithTimeout, explicitTimeout), 2500ms);

	// the game server requires a timeout > 0
	Options gs = DatabaseFactory::gameServerOptions();
	EXPECT_TRUE(gs.requireSocketTimeout);
	EXPECT_EQ(DatabaseFactory::resolveSocketTimeout(plainUrl, gs), 60s);
	EXPECT_EQ(DatabaseFactory::resolveSocketTimeout(urlWithTimeout, gs), 1500ms);
	EXPECT_THROW(DatabaseFactory::resolveSocketTimeout(urlWithZeroTimeout, gs), aion::commons::utils::IllegalArgumentException);
	gs.socketTimeout = 0ms;
	EXPECT_THROW(DatabaseFactory::resolveSocketTimeout(urlWithTimeout, gs), aion::commons::utils::IllegalArgumentException);
	EXPECT_THROW(DatabaseFactory::resolveSocketTimeout(plainUrl, Options{.socketTimeout = -1ms}), aion::commons::utils::IllegalArgumentException);
	EXPECT_THROW(DatabaseFactory::resolveSocketTimeout("jdbc:oracle:thin:@localhost", Options{}), SQLException);
}

TEST(DatabaseFactoryTest, InitRejectsMissingSocketTimeoutBeforeConnecting) {
	using namespace std::chrono_literals;
	DatabaseFactory::shutdown();
	DatabaseFactory::Options required{.socketTimeout = std::nullopt, .defaultSocketTimeout = 0ms, .requireSocketTimeout = true};
	try {
		DatabaseFactory::init(UNREACHABLE_URL, "root", "", 5, 5000, required);
		DatabaseFactory::shutdown();
		FAIL();
	} catch (const aion::commons::utils::IllegalArgumentException& e) {
		EXPECT_NE(std::string(e.what()).find("database.socket_timeout"), std::string::npos) << e.what();
	}
	EXPECT_FALSE(DatabaseFactory::isInitialized());
	EXPECT_EQ(DatabaseFactory::getSocketTimeout(), std::nullopt);

	// with a timeout the requirement passes and init fails only because nothing listens
	EXPECT_THROW(DatabaseFactory::init(UNREACHABLE_URL, "root", "", 5, 5000, DatabaseFactory::gameServerOptions()), SQLException);
	EXPECT_FALSE(DatabaseFactory::isInitialized());
}

TEST(DatabaseFactoryTest, InitFromDatabaseConfigUsesSocketTimeoutKey) {
	using aion::commons::configs::DatabaseConfig;
	DatabaseFactory::shutdown();
	const std::string savedUrl = DatabaseConfig::DATABASE_URL;
	const int32_t savedMax = DatabaseConfig::DATABASE_CONNECTIONS_MAX;
	const int32_t savedTimeout = DatabaseConfig::DATABASE_TIMEOUT;
	const std::optional<int32_t> savedSocketTimeout = DatabaseConfig::DATABASE_SOCKET_TIMEOUT;
	DatabaseConfig::DATABASE_URL = UNREACHABLE_URL;
	DatabaseConfig::DATABASE_CONNECTIONS_MAX = 5;
	DatabaseConfig::DATABASE_TIMEOUT = 5000;

	DatabaseConfig::DATABASE_SOCKET_TIMEOUT = 0; // configured 0 overrides the game server default
	EXPECT_THROW(DatabaseFactory::init(DatabaseFactory::gameServerOptions()), aion::commons::utils::IllegalArgumentException);
	DatabaseConfig::DATABASE_SOCKET_TIMEOUT = 30000;
	EXPECT_THROW(DatabaseFactory::init(DatabaseFactory::gameServerOptions()), SQLException); // requirement met, database unreachable
	DatabaseConfig::DATABASE_SOCKET_TIMEOUT = std::nullopt;
	EXPECT_THROW(DatabaseFactory::init(DatabaseFactory::gameServerOptions()), SQLException); // 60 s default

	DatabaseConfig::DATABASE_URL = savedUrl;
	DatabaseConfig::DATABASE_CONNECTIONS_MAX = savedMax;
	DatabaseConfig::DATABASE_TIMEOUT = savedTimeout;
	DatabaseConfig::DATABASE_SOCKET_TIMEOUT = savedSocketTimeout;
	EXPECT_FALSE(DatabaseFactory::isInitialized());
}

TEST(DatabaseFactoryTest, ConnectionOpenReportsCommunicationFailure) {
	try {
		Connection::open(ConnectionProperties::parse(UNREACHABLE_URL, "root", ""));
		FAIL();
	} catch (const SQLException& e) {
		EXPECT_EQ(e.getSQLState(), "08S01");
		EXPECT_FALSE(std::string(e.what()).empty());
	}
}

TEST(DatabaseFactoryTest, DbHelpersReportErrorsWithoutDatabase) {
	DatabaseFactory::shutdown();
	bool readerCalled = false;
	EXPECT_FALSE(DB::select("SELECT 1", [&](ResultSet&) { readerCalled = true; }));
	EXPECT_FALSE(DB::select("SELECT ?", [](PreparedStatement& st) { st.setInt(1, 1); }, [&](ResultSet&) { readerCalled = true; }));
	EXPECT_FALSE(DB::select("SELECT 1", ParamReadStH{.setParams = nullptr, .handleRead = [&](ResultSet&) { readerCalled = true; }}));
	EXPECT_FALSE(DB::call("CALL x()", [&](ResultSet&) { readerCalled = true; }));
	EXPECT_FALSE(readerCalled);
	EXPECT_FALSE(DB::insertUpdate("UPDATE x SET y = 1"));
	EXPECT_FALSE(DB::insertUpdate("UPDATE x SET y = ?", [](PreparedStatement& st) { st.setInt(1, 1); }));
	EXPECT_EQ(DB::prepareStatement("SELECT 1"), nullptr);
	EXPECT_EQ(DB::executeUpdate(nullptr), -1);
	EXPECT_EQ(DB::executeQuerry(nullptr), nullptr);
	std::unique_ptr<PreparedStatement> none;
	DB::close(none);
	DB::executeUpdateAndClose(none);
	EXPECT_THROW(DB::beginTransaction(), SQLException);
}
