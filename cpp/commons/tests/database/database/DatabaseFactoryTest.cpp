#include <gtest/gtest.h>

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
