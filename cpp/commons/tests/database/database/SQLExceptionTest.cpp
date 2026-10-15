#include <gtest/gtest.h>

#include "aion/commons/database/SQLException.h"
#include "aion/commons/database/SqlTypes.h"

using namespace aion::commons::database;

TEST(SQLExceptionTest, CarriesStateAndCode) {
	SQLException e("Duplicate entry '1' for key 'PRIMARY'", "23000", 1062);
	EXPECT_STREQ(e.what(), "Duplicate entry '1' for key 'PRIMARY'");
	EXPECT_EQ(e.getSQLState(), "23000");
	EXPECT_EQ(e.getErrorCode(), 1062);
	EXPECT_FALSE(e.cause());
}

TEST(SQLExceptionTest, WrapsCause) {
	try {
		try {
			throw SQLException("inner", "08S01", 2013);
		} catch (...) {
			throw SQLException("outer", std::current_exception());
		}
	} catch (const SQLException& e) {
		EXPECT_STREQ(e.what(), "outer");
		EXPECT_EQ(e.getSQLState(), "");
		EXPECT_EQ(e.getErrorCode(), 0);
		ASSERT_TRUE(e.cause());
		std::string trace = aion::commons::utils::toStackTraceString(e);
		EXPECT_NE(trace.find("Caused by: aion::commons::database::SQLException: inner"), std::string::npos) << trace;
	}
}

TEST(SQLExceptionTest, TransientConnectionExceptionIsSQLException) {
	try {
		throw SQLTransientConnectionException("timeout", "08001", 0, nullptr);
	} catch (const SQLException& e) {
		EXPECT_EQ(e.getSQLState(), "08001");
	}
}

TEST(SQLExceptionTest, BatchUpdateCountsAreClamped) {
	BatchUpdateException e("failed", "23000", 1062, {1, Statement::EXECUTE_FAILED, 5000000000LL}, nullptr);
	EXPECT_EQ(e.getLargeUpdateCounts(), (std::vector<int64_t>{1, -3, 5000000000LL}));
	EXPECT_EQ(e.getUpdateCounts(), (std::vector<int32_t>{1, -3, 2147483647}));
	EXPECT_EQ(e.getErrorCode(), 1062);
}
