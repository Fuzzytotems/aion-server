#include <gtest/gtest.h>

#include "aion/commons/database/SQLException.h"
#include "aion/commons/database/StatementParameters.h"

using namespace aion::commons::database;
using Kind = StatementParameters::Kind;

TEST(StatementParametersTest, TypedValues) {
	StatementParameters params(12);
	params.setBoolean(1, true);
	params.setByte(2, -5);
	params.setShort(3, 300);
	params.setInt(4, -70000);
	params.setLong(5, 1LL << 40);
	params.setUnsignedLong(6, 18446744073709551615ull);
	params.setFloat(7, 1.5f);
	params.setDouble(8, 2.25);
	params.setString(9, "text");
	std::vector<uint8_t> bytes = {0, 1, 255};
	params.setBytes(10, bytes);
	params.setDate(11, std::chrono::year(2024) / 2 / 29);
	params.setNull(12, Types::TIMESTAMP);

	EXPECT_EQ(params[0].kind, Kind::TINY);
	EXPECT_EQ(params[0].integer, 1);
	EXPECT_EQ(params[1].kind, Kind::TINY);
	EXPECT_EQ(params[1].integer, -5);
	EXPECT_EQ(params[2].kind, Kind::SHORT);
	EXPECT_EQ(params[3].kind, Kind::LONG);
	EXPECT_EQ(params[3].integer, -70000);
	EXPECT_EQ(params[4].kind, Kind::LONGLONG);
	EXPECT_FALSE(params[4].isUnsigned);
	EXPECT_EQ(params[5].kind, Kind::LONGLONG);
	EXPECT_TRUE(params[5].isUnsigned);
	EXPECT_EQ(static_cast<uint64_t>(params[5].integer), 18446744073709551615ull);
	EXPECT_EQ(params[6].kind, Kind::FLOAT);
	EXPECT_EQ(params[6].floating, 1.5);
	EXPECT_EQ(params[7].kind, Kind::DOUBLE);
	EXPECT_EQ(params[8].kind, Kind::STRING);
	EXPECT_EQ(params[8].bytes, "text");
	EXPECT_EQ(params[9].kind, Kind::BYTES);
	EXPECT_EQ(params[9].bytes, std::string("\x00\x01\xff", 3));
	EXPECT_EQ(params[10].kind, Kind::DATE);
	EXPECT_EQ(params[10].dateTime.toString(), "2024-02-29");
	EXPECT_EQ(params[11].kind, Kind::NULL_VALUE);
	EXPECT_EQ(params[11].sqlType, Types::TIMESTAMP);
	EXPECT_NO_THROW(StatementParameters::checkAllSet(params.getValues()));
}

TEST(StatementParametersTest, OverwritingResetsValue) {
	StatementParameters params(1);
	params.setString(1, "abc");
	params.setInt(1, 5);
	EXPECT_EQ(params[0].kind, Kind::LONG);
	EXPECT_TRUE(params[0].bytes.empty());
	DateTimeValue dt{.year = 2024, .month = 1, .day = 2, .hour = 3};
	params.setDateTime(1, dt);
	EXPECT_EQ(params[0].kind, Kind::DATETIME);
	EXPECT_EQ(params[0].dateTime.toString(), "2024-01-02 03:00:00");
}

TEST(StatementParametersTest, IndexOutOfRange) {
	StatementParameters params(2);
	try {
		params.setInt(3, 1);
		FAIL();
	} catch (const SQLException& e) {
		EXPECT_EQ(e.getSQLState(), "S1009");
		EXPECT_EQ(std::string(e.what()), "Parameter index out of range (3 > number of parameters, which is 2).");
	}
	EXPECT_THROW(params.setInt(0, 1), SQLException);
	EXPECT_THROW(StatementParameters(0).setNull(1, Types::INTEGER), SQLException);
}

TEST(StatementParametersTest, UnsetParametersAreReported) {
	StatementParameters params(3);
	params.setInt(1, 1);
	params.setInt(3, 3);
	try {
		StatementParameters::checkAllSet(params.getValues());
		FAIL();
	} catch (const SQLException& e) {
		EXPECT_EQ(e.getSQLState(), "07001");
		EXPECT_EQ(std::string(e.what()), "No value specified for parameter 2");
	}
	params.setNull(2, Types::INTEGER);
	EXPECT_NO_THROW(StatementParameters::checkAllSet(params.getValues()));
	params.clear();
	EXPECT_THROW(StatementParameters::checkAllSet(params.getValues()), SQLException);
	EXPECT_EQ(params[0].kind, Kind::UNSET);
}
