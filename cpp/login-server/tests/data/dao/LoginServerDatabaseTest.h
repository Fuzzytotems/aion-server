#pragma once

#include <optional>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "DataTestUtils.h"
#include "aion/commons/database/DB.h"
#include "aion/commons/database/DatabaseFactory.h"
#include "aion/loginserver/configs/Config.h"
#include "support/LoginServerTestDatabase.h"

// Integration tests against the login server schema. They run only if AION_TEST_LS_DATABASE_URL is set (e.g.
// jdbc:mysql://localhost:3306/aion_ls_test, the database must exist), otherwise they are skipped. Credentials: AION_TEST_DATABASE_USER and
// AION_TEST_DATABASE_PASSWORD. Before each test all tables are dropped and recreated from login-server/sql/aion_ls.sql. The process holds a database
// lock from the first test on, so it does not run at the same time as other processes testing against the same database.

namespace aion::loginserver::test {

/** Base fixture for DAO tests: initializes DatabaseFactory once per test suite and recreates the schema before each test. */
class LoginServerDatabaseTest : public ::testing::Test {
protected:
	static void SetUpTestSuite() {
		std::string url = env("AION_TEST_LS_DATABASE_URL");
		if (url.empty())
			return;
		commons::database::DatabaseFactory::init(url, env("AION_TEST_DATABASE_USER"), env("AION_TEST_DATABASE_PASSWORD"), 5, 5000);
	}

	static void TearDownTestSuite() { commons::database::DatabaseFactory::shutdown(); }

	void SetUp() override {
		if (env("AION_TEST_LS_DATABASE_URL").empty())
			GTEST_SKIP() << "AION_TEST_LS_DATABASE_URL is not set";
		configs::Config::EXTERNAL_AUTH_URL.clear();
		ASSERT_NO_THROW(database::recreateSchema()); // serialized with other test processes (database::lockForProcess)
	}

	void TearDown() override { configs::Config::EXTERNAL_AUTH_URL.clear(); }

	/** Executes a statement without parameters */
	static void execute(std::string_view sql) {
		auto con = commons::database::DatabaseFactory::getConnection();
		con->executeSimple(sql);
	}

	/** @return the first column of the first row of a query, std::nullopt for NULL or no row */
	static std::optional<std::string> queryString(std::string_view sql) {
		auto con = commons::database::DatabaseFactory::getConnection();
		auto rs = con->prepareStatement(sql)->executeQuery();
		if (!rs->next())
			return std::nullopt;
		return rs->getObject<std::string>(1);
	}

	static int64_t queryLong(std::string_view sql) {
		auto con = commons::database::DatabaseFactory::getConnection();
		auto rs = con->prepareStatement(sql)->executeQuery();
		EXPECT_TRUE(rs->next()) << sql;
		return rs->getLong(1);
	}
};

} // namespace aion::loginserver::test
