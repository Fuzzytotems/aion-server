#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "aion/commons/database/ConnectionTimeZone.h"
#include "aion/commons/database/SQLException.h"

using namespace aion::commons::database;
using namespace std::chrono;
using Kind = DateTimeValue::Kind;

namespace {

Timestamp utc(int y, unsigned m, unsigned d, int h, int min, int s, int ms = 0) {
	return sys_days(year(y) / month(m) / day(d)) + hours(h) + minutes(min) + seconds(s) + milliseconds(ms);
}

DateTimeValue dateTime(uint32_t y, uint32_t m, uint32_t d, uint32_t h, uint32_t min, uint32_t s, uint32_t micros = 0) {
	return DateTimeValue{.kind = Kind::DATETIME, .year = y, .month = m, .day = d, .hour = h, .minute = min, .second = s, .microsecond = micros};
}

} // namespace

TEST(ConnectionTimeZoneTest, FixedOffsetConversions) {
	ConnectionTimeZone tz = ConnectionTimeZone::of("+02:00");
	EXPECT_EQ(tz.getId(), "+02:00");
	EXPECT_EQ(tz.toDateTime(utc(2024, 1, 1, 23, 30, 0, 250)), dateTime(2024, 1, 2, 1, 30, 0, 250000));
	EXPECT_EQ(tz.toTimestamp(dateTime(2024, 1, 2, 1, 30, 0, 250999)), utc(2024, 1, 1, 23, 30, 0, 250));
}

TEST(ConnectionTimeZoneTest, ParsesJavaOffsetFormats) {
	EXPECT_EQ(ConnectionTimeZone::of("Z").getId(), "Z");
	EXPECT_EQ(ConnectionTimeZone::of("UTC+1").getId(), "+01:00");
	EXPECT_EQ(ConnectionTimeZone::of("GMT-0530").getId(), "-05:30");
	EXPECT_EQ(ConnectionTimeZone::of("-08:00:15").getId(), "-08:00:15");
	EXPECT_THROW(ConnectionTimeZone::of("Mars/Olympus_Mons"), SQLException);
	EXPECT_THROW(ConnectionTimeZone::of("+25:00"), SQLException);
	try {
		ConnectionTimeZone::of("bogus");
		FAIL();
	} catch (const SQLException& e) {
		EXPECT_EQ(std::string(e.what()), "The server time zone value 'bogus' is unrecognized or represents more than one time zone.");
	}
}

TEST(ConnectionTimeZoneTest, EmptyAndLocalMeanSystemZone) {
	std::string system = ConnectionTimeZone::systemDefault().getId();
	EXPECT_EQ(ConnectionTimeZone::of("").getId(), system);
	EXPECT_EQ(ConnectionTimeZone::of("LOCAL").getId(), system);
	EXPECT_EQ(ConnectionTimeZone().getId(), system);
}

TEST(ConnectionTimeZoneTest, NamedZoneHandlesDaylightSavingLikeJava) {
	ConnectionTimeZone berlin;
	try {
		berlin = ConnectionTimeZone::of("Europe/Berlin");
	} catch (const SQLException&) {
		GTEST_SKIP() << "time zone database not available";
	}
	EXPECT_EQ(berlin.getId(), "Europe/Berlin");
	// winter / summer
	EXPECT_EQ(berlin.toDateTime(utc(2024, 1, 15, 12, 0, 0)), dateTime(2024, 1, 15, 13, 0, 0));
	EXPECT_EQ(berlin.toDateTime(utc(2024, 7, 15, 12, 0, 0)), dateTime(2024, 7, 15, 14, 0, 0));
	EXPECT_EQ(berlin.toTimestamp(dateTime(2024, 7, 15, 14, 0, 0)), utc(2024, 7, 15, 12, 0, 0));
	// gap on 2024-03-31: 02:30 does not exist and becomes 03:30 CEST (= 01:30 UTC), like LocalDateTime.atZone
	EXPECT_EQ(berlin.toTimestamp(dateTime(2024, 3, 31, 2, 30, 0)), utc(2024, 3, 31, 1, 30, 0));
	// overlap on 2024-10-27: 02:30 exists twice, the earlier offset (CEST, +2) wins
	EXPECT_EQ(berlin.toTimestamp(dateTime(2024, 10, 27, 2, 30, 0)), utc(2024, 10, 27, 0, 30, 0));
}

TEST(ConnectionTimeZoneTest, DateAndTimeValues) {
	ConnectionTimeZone tz = ConnectionTimeZone::ofOffset(hours(-3));
	EXPECT_EQ(tz.toTimestamp(DateTimeValue{.kind = Kind::DATE, .year = 2020, .month = 2, .day = 29}), utc(2020, 2, 29, 3, 0, 0));
	EXPECT_EQ(tz.toTimestamp(DateTimeValue{.kind = Kind::TIME, .hour = 1, .minute = 2, .second = 3}), utc(1970, 1, 1, 4, 2, 3));
}

TEST(ConnectionTimeZoneTest, InstantsBeforeEpochRoundDown) {
	ConnectionTimeZone tz = ConnectionTimeZone::ofOffset(seconds(0));
	EXPECT_EQ(tz.toDateTime(Timestamp(milliseconds(-1))), dateTime(1969, 12, 31, 23, 59, 59, 999000));
	EXPECT_EQ(tz.toTimestamp(dateTime(1969, 12, 31, 23, 59, 59, 999999)), Timestamp(milliseconds(-1)));
}

namespace {

/** java.time's LocalDateTime.atZone(zone).toInstant() computed directly with the time zone database (no caching) */
ConnectionTimeZone::MicrosecondsSysTime referenceToSys(const time_zone& zone, ConnectionTimeZone::MicrosecondsLocalTime local) {
	return ConnectionTimeZone::MicrosecondsSysTime(local.time_since_epoch() - zone.get_info(floor<seconds>(local)).first.offset);
}

ConnectionTimeZone::MicrosecondsLocalTime referenceToLocal(const time_zone& zone, ConnectionTimeZone::MicrosecondsSysTime time) {
	return ConnectionTimeZone::MicrosecondsLocalTime(time.time_since_epoch() + zone.get_info(floor<seconds>(time)).offset);
}

const time_zone* findZone(std::string_view name) {
	try {
		return locate_zone(name);
	} catch (const std::exception&) {
		return nullptr;
	}
}

} // namespace

TEST(ConnectionTimeZoneTest, CachedConversionsMatchTimeZoneDatabase) {
	std::vector<std::pair<const time_zone*, ConnectionTimeZone>> zones;
	for (const char* name : {"Europe/Berlin", "America/Los_Angeles", "Australia/Lord_Howe", "Asia/Tokyo", "America/Sao_Paulo", "Pacific/Apia"}) {
		if (const time_zone* zone = findZone(name))
			zones.emplace_back(zone, ConnectionTimeZone::of(name));
	}
	if (zones.empty())
		GTEST_SKIP() << "time zone database not available";
	using Micros = ConnectionTimeZone::MicrosecondsSysTime;
	std::vector<Micros> instants;
	// every 5 minutes from 4 hours before to 4 hours after each transition in 2024 of any of the zones (forwards, then backwards), so that gaps,
	// overlaps and period boundaries are crossed with a warm cache in both directions
	for (auto& [zone, tz] : zones) {
		for (sys_seconds t = sys_days(2024y / January / 1); t < sys_days(2025y / January / 1);) {
			sys_info info = zone->get_info(t);
			if (info.end >= sys_days(2025y / January / 1))
				break;
			for (minutes m = -4h; m <= 4h; m += 5min)
				instants.push_back(Micros(info.end + m) + 123456us);
			for (minutes m = 4h; m >= -4h; m -= 5min)
				instants.push_back(Micros(info.end + m) - 1us);
			t = info.end;
		}
	}
	// every 6 hours through 2024 and jumps between distant periods
	for (Micros t = sys_days(2024y / January / 1); t < sys_days(2025y / January / 1); t += 6h)
		instants.push_back(t + 7s);
	for (int i = 0; i < 200; ++i)
		instants.push_back(Micros(sys_days(year(1850 + i * 2) / (1 + i % 12) / 28)) + hours(i % 24) + 7s);
	size_t failures = 0;
	for (const Micros& t : instants) {
		for (auto& [zone, tz] : zones) { // alternating zones on the same thread
			auto local = ConnectionTimeZone::MicrosecondsLocalTime(t.time_since_epoch());
			if (tz.toLocal(t) != referenceToLocal(*zone, t) || tz.toSys(local) != referenceToSys(*zone, local)) {
				if (++failures <= 10)
					ADD_FAILURE() << tz.getId() << " at " << t.time_since_epoch().count() << "us";
			}
		}
	}
	EXPECT_EQ(failures, 0u);
	// the same values again, each zone on its own (cache hits instead of alternating misses)
	for (auto& [zone, tz] : zones) {
		for (const Micros& t : instants) {
			auto local = ConnectionTimeZone::MicrosecondsLocalTime(t.time_since_epoch());
			if (tz.toLocal(t) != referenceToLocal(*zone, t) || tz.toSys(local) != referenceToSys(*zone, local)) {
				if (++failures <= 10)
					ADD_FAILURE() << tz.getId() << " at " << t.time_since_epoch().count() << "us (single zone)";
			}
		}
	}
	EXPECT_EQ(failures, 0u);
}

TEST(ConnectionTimeZoneTest, ConversionsDoNotQueryTheTimeZoneDatabaseForEachValue) {
	const time_zone* zone = findZone("America/Los_Angeles");
	if (!zone)
		GTEST_SKIP() << "time zone database not available";
	ConnectionTimeZone tz = ConnectionTimeZone::of("America/Los_Angeles");
	constexpr int count = 20000;
	auto base = local_days(2024y / May / 17) + 13h;
	auto start = steady_clock::now();
	int64_t referenceSum = 0;
	for (int i = 0; i < count; ++i)
		referenceSum += zone->get_info(base + seconds(i)).first.offset.count();
	auto uncached = steady_clock::now() - start;
	EXPECT_EQ(referenceSum, -7 * 3600 * int64_t{count}); // PDT
	start = steady_clock::now();
	size_t mismatches = 0;
	for (int i = 0; i < count; ++i) {
		// one read (toTimestamp) and one write (toDateTime) conversion per value
		DateTimeValue value = dateTime(2024, 5, 17, 13, static_cast<uint32_t>(i / 60 % 60), static_cast<uint32_t>(i % 60));
		if (tz.toDateTime(tz.toTimestamp(value)) != value)
			++mismatches;
	}
	auto cached = steady_clock::now() - start;
	EXPECT_EQ(mismatches, 0u);
	EXPECT_LT(cached * 5, uncached) << "cached: " << duration_cast<microseconds>(cached).count() << " us, one database lookup per value: "
																	<< duration_cast<microseconds>(uncached).count() << " us";
}

TEST(ConnectionTimeZoneTest, OperatingSystemZoneMatchesTimeZoneDatabaseForCurrentRules) {
	const time_zone* system = nullptr;
	try {
		system = current_zone();
	} catch (const std::exception&) {
		GTEST_SKIP() << "time zone database not available";
	}
	ConnectionTimeZone os = ConnectionTimeZone::operatingSystemZone();
	EXPECT_FALSE(os.getId().empty());
	using Micros = ConnectionTimeZone::MicrosecondsSysTime;
	size_t failures = 0;
	// every 3 hours through 2024 and 2025, and every 5 minutes around each transition of the system zone (gaps and overlaps, if it has DST)
	std::vector<Micros> instants;
	for (Micros t = sys_days(2024y / January / 1); t < sys_days(2026y / January / 1); t += 3h)
		instants.push_back(t);
	for (sys_seconds t = sys_days(2024y / January / 1);;) {
		sys_info info = system->get_info(t);
		if (info.end >= sys_days(2026y / January / 1))
			break;
		for (minutes m = -3h; m <= 3h; m += 5min)
			instants.push_back(Micros(info.end + m));
		t = info.end;
	}
	for (const Micros& t : instants) {
		Micros instant = t + 250ms;
		auto local = ConnectionTimeZone::MicrosecondsLocalTime(instant.time_since_epoch());
		if (os.toLocal(instant) != referenceToLocal(*system, instant) || os.toSys(local) != referenceToSys(*system, local)) {
			if (++failures <= 10)
				ADD_FAILURE() << os.getId() << " / " << system->name() << " at " << instant.time_since_epoch().count() << "us";
		}
	}
	EXPECT_EQ(failures, 0u);
	DateTimeValue value = os.toDateTime(Timestamp(sys_days(2024y / July / 1) + 12h));
	EXPECT_EQ(os.toTimestamp(value), Timestamp(sys_days(2024y / July / 1) + 12h));
}
