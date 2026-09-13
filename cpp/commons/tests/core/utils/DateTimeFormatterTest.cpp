#include <gtest/gtest.h>

#include "aion/commons/utils/DateTimeFormatter.h"
#include "aion/commons/utils/Exception.h"

using namespace aion::commons::utils;
using namespace std::chrono;

namespace {

/** 2026-09-12T13:42:30.1234567Z, a Saturday */
system_clock::time_point sampleTime() {
	return time_point_cast<system_clock::duration>(sys_days(2026y / September / 12) + 13h + 42min + 30s + 123456700ns); // system_clock may have 100 ns resolution
}

std::string formatSample(std::string_view pattern, seconds offset) {
	std::string out;
	DateTimeFormatter::ofPattern(pattern).formatTo(out, DateTimeFields::of(sampleTime(), offset));
	return out;
}

} // namespace

TEST(DateTimeFormatterTest, LogbackPatterns) {
	EXPECT_EQ(formatSample("yyyy-MM-dd'T'HH:mm:ss,SSSXXX", 2h), "2026-09-12T15:42:30,123+02:00");
	EXPECT_EQ(formatSample("yyyy-MM-dd'T'HH:mm:ss,SSSXXX", 0s), "2026-09-12T13:42:30,123Z");
	EXPECT_EQ(formatSample("yyyy-MM-dd'T'HH:mm:ss,SSSXXX", -(5h + 30min)), "2026-09-12T08:12:30,123-05:30");
	EXPECT_EQ(formatSample("HH:mm:ss", 0s), "13:42:30");
	EXPECT_EQ(formatSample("yyyy-MM-dd HH.mm", 0s), "2026-09-12 13.42");
	EXPECT_EQ(formatSample("yyyy-MM-dd HH:mm", 1h), "2026-09-12 14:42");
	EXPECT_EQ(formatSample("H:mm:ss", -12h), "1:42:30");
}

TEST(DateTimeFormatterTest, Fields) {
	EXPECT_EQ(formatSample("yy M d D", 0s), "26 9 12 255");
	EXPECT_EQ(formatSample("MMM MMMM EEE EEEE", 0s), "Sep September Sat Saturday");
	EXPECT_EQ(formatSample("h K k a", 0s), "1 1 13 PM");
	EXPECT_EQ(formatSample("hh a kk", -13h), "12 AM 24");
	EXPECT_EQ(formatSample("S SS SSSSSS SSSSSSSSS", 0s), "1 12 123456 123456700");
	EXPECT_EQ(formatSample("'it''s' '' 'o''clock'", 0s), "it's ' o'clock");
	EXPECT_EQ(formatSample("X XX XXX x xx xxx Z ZZZZ ZZZZZ", 0s), "Z Z Z +00 +0000 +00:00 +0000 GMT Z");
	EXPECT_EQ(formatSample("X XX XXX x Z ZZZZ", 5h + 45min), "+0545 +0545 +05:45 +0545 +0545 GMT+05:45");
	EXPECT_EQ(formatSample("X", 3h), "+03");
}

TEST(DateTimeFormatterTest, DateBoundariesWithOffsets) {
	system_clock::time_point newYear = time_point_cast<system_clock::duration>(sys_days(2027y / January / 1) + 30min);
	std::string out;
	DateTimeFormatter::ofPattern("yyyy-MM-dd HH:mm D").formatTo(out, DateTimeFields::of(newYear, -1h));
	EXPECT_EQ(out, "2026-12-31 23:30 365");
	system_clock::time_point beforeEpoch = time_point_cast<system_clock::duration>(sys_days(1969y / December / 31) + 23h + 59min + 59s + 500ms);
	out.clear();
	DateTimeFormatter::ofPattern("yyyy-MM-dd HH:mm:ss.SSS").formatTo(out, DateTimeFields::of(beforeEpoch, 0s));
	EXPECT_EQ(out, "1969-12-31 23:59:59.500");
}

TEST(DateTimeFormatterTest, TimesOutsideTheNanosecondRange) {
	// system_clock (100 ns ticks on MSVC) covers years that do not fit into int64_t nanoseconds since 1970 (about 1677 to 2262), e.g. file times
	auto format = [](sys_days day, system_clock::duration timeOfDay, seconds offset) {
		std::string out;
		system_clock::time_point time = time_point_cast<system_clock::duration>(day) + timeOfDay; // days and hours have 32 bit representations
		DateTimeFormatter::ofPattern("yyyy-MM-dd HH:mm:ss.SSS").formatTo(out, DateTimeFields::of(time, offset));
		return out;
	};
	if (duration_cast<days>(system_clock::duration::max()).count() > 3'000'000) { // more than about 8000 years from 1970
		EXPECT_EQ(format(sys_days(9999y / December / 31), 23h + 59min + 59s + 999ms, 0s), "9999-12-31 23:59:59.999");
		EXPECT_EQ(format(sys_days(2500y / March / 1), 30min, -1h), "2500-02-28 23:30:00.000");
		EXPECT_EQ(format(sys_days(1601y / January / 1), 0s, 0s), "1601-01-01 00:00:00.000");
	}
}

TEST(DateTimeFormatterTest, InvalidPatterns) {
	EXPECT_THROW(DateTimeFormatter::ofPattern("yyyy-MM-dd'T"), IllegalArgumentException);
	EXPECT_THROW(DateTimeFormatter::ofPattern("HHH"), IllegalArgumentException);
	EXPECT_THROW(DateTimeFormatter::ofPattern("G"), IllegalArgumentException);
	EXPECT_THROW(DateTimeFormatter::ofPattern("[HH]"), IllegalArgumentException);
}

TEST(DateTimeFormatterTest, TimeZones) {
	const time_zone* berlin = nullptr;
	try {
		berlin = findTimeZone("Europe/Berlin");
	} catch (const IllegalArgumentException&) {
		GTEST_SKIP() << "no time zone database available";
	}
	ASSERT_NE(berlin, nullptr);
	EXPECT_EQ(findTimeZone(""), nullptr);
	EXPECT_THROW(findTimeZone("Nowhere/Nothing"), IllegalArgumentException);

	auto formatter = DateTimeFormatter::ofPattern("yyyy-MM-dd HH:mm XXX").withZone(berlin);
	EXPECT_EQ(formatter.getZone(), berlin);
	EXPECT_EQ(formatter.format(sampleTime()), "2026-09-12 15:42 +02:00"); // summer time
	EXPECT_EQ(formatter.format(sys_days(2026y / January / 1) + 0s), "2026-01-01 01:00 +01:00");
	EXPECT_EQ(getUtcOffset(sys_days(2026y / July / 1) + 0s, berlin), 2h);
}

TEST(DateTimeFormatterTest, SystemDefaultZoneMatchesTimeZoneDatabase) {
	const time_zone* current = nullptr;
	try {
		current = current_zone();
	} catch (const std::exception&) {
		GTEST_SKIP() << "no time zone database available";
	}
	for (sys_seconds time : {sys_days(2026y / January / 15) + 12h, sys_days(2026y / July / 15) + 12h}) {
		EXPECT_EQ(getUtcOffset(time, nullptr), current->get_info(time).offset) << current->name();
	}
}
