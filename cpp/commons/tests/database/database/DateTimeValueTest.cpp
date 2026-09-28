#include <gtest/gtest.h>

#include "aion/commons/database/DateTimeValue.h"

using namespace aion::commons::database;
using Kind = DateTimeValue::Kind;

TEST(DateTimeValueTest, FormatsLikeTextProtocol) {
	DateTimeValue dt{.kind = Kind::DATETIME, .year = 2024, .month = 2, .day = 9, .hour = 7, .minute = 5, .second = 3, .microsecond = 123456};
	EXPECT_EQ(dt.toString(), "2024-02-09 07:05:03");
	EXPECT_EQ(dt.toString(3), "2024-02-09 07:05:03.123");
	EXPECT_EQ(dt.toString(6), "2024-02-09 07:05:03.123456");
	EXPECT_EQ(dt.toString(9), "2024-02-09 07:05:03.123456");

	DateTimeValue date{.kind = Kind::DATE, .year = 999, .month = 12, .day = 31};
	EXPECT_EQ(date.toString(6), "0999-12-31");

	DateTimeValue time{.kind = Kind::TIME, .negative = true, .hour = 838, .minute = 59, .second = 58, .microsecond = 500000};
	EXPECT_EQ(time.toString(), "-838:59:58");
	EXPECT_EQ(time.toString(1), "-838:59:58.5");
}

TEST(DateTimeValueTest, ParsesDateTimes) {
	auto dt = DateTimeValue::parse("2024-02-09 07:05:03.12");
	ASSERT_TRUE(dt);
	EXPECT_EQ(*dt, (DateTimeValue{.kind = Kind::DATETIME, .year = 2024, .month = 2, .day = 9, .hour = 7, .minute = 5, .second = 3, .microsecond = 120000}));
	EXPECT_EQ(DateTimeValue::parse("2024-02-09T07:05:03")->hour, 7u);
	EXPECT_EQ(DateTimeValue::parse("2024-02-09 07:05:03.1234567")->microsecond, 123456u);

	auto date = DateTimeValue::parse("0000-00-00");
	ASSERT_TRUE(date);
	EXPECT_EQ(date->kind, Kind::DATE);
	EXPECT_TRUE(date->isZeroDate());

	auto time = DateTimeValue::parse("-100:00:01");
	ASSERT_TRUE(time);
	EXPECT_EQ(time->kind, Kind::TIME);
	EXPECT_TRUE(time->negative);
	EXPECT_EQ(time->hour, 100u);
	EXPECT_FALSE(time->isZeroDate());
}

TEST(DateTimeValueTest, RejectsInvalidText) {
	for (const char* text : {"", "abc", "2024-2-09", "2024-02-09 07:05", "2024-02-09 07:05:03.", "12:00", "1234:00:00",
			 "2024-02-09x", "2024-02-09 07:05:03 "}) {
		EXPECT_FALSE(DateTimeValue::parse(text)) << text;
	}
}
