#include "aion/commons/configuration/transformers/ZoneIdTransformer.h"

#include <gtest/gtest.h>

using namespace aion::commons;
using namespace aion::commons::configuration::transformers;
using TimeZone = const std::chrono::time_zone*;

namespace {

std::string errorMessage(std::string_view value) {
	try {
		ZoneIdTransformer::of(value);
	} catch (const utils::IllegalArgumentException& e) {
		return e.what();
	}
	return "<no exception>";
}

} // namespace

TEST(ZoneIdTransformerTest, EmptyValueIsSystemTimeZone) {
	EXPECT_EQ(transform<TimeZone>(""), std::chrono::current_zone());
	EXPECT_EQ(ZoneIdTransformer::systemDefault(), std::chrono::current_zone());
	EXPECT_EQ(&ZoneIdTransformer::database(), &std::chrono::get_tzdb());
	EXPECT_EQ(typeName<TimeZone>(), "ZoneId");
}

TEST(ZoneIdTransformerTest, RegionIds) {
	EXPECT_EQ(transform<TimeZone>("Europe/Berlin"), std::chrono::locate_zone("Europe/Berlin"));
	EXPECT_EQ(transform<TimeZone>("America/New_York"), std::chrono::locate_zone("America/New_York"));
	EXPECT_EQ(errorMessage("Invalid/Zone"), "Unknown time-zone ID: Invalid/Zone");
	EXPECT_EQ(errorMessage("europe/berlin_x"), "Unknown time-zone ID: europe/berlin_x");
	EXPECT_EQ(errorMessage("1abc"), "Invalid ID for region-based ZoneId, invalid format: 1abc");
	EXPECT_EQ(errorMessage("Europe/Ber lin"), "Invalid ID for region-based ZoneId, invalid format: Europe/Ber lin");
	EXPECT_EQ(errorMessage("UTCx"), "Unknown time-zone ID: UTCx");
}

TEST(ZoneIdTransformerTest, UtcAndOffsets) {
	TimeZone utc = std::chrono::locate_zone("UTC");
	EXPECT_EQ(ZoneIdTransformer::of("Z"), utc);
	EXPECT_EQ(ZoneIdTransformer::of("UTC"), utc);
	EXPECT_EQ(ZoneIdTransformer::of("UT"), utc);
	EXPECT_EQ(ZoneIdTransformer::of("GMT"), std::chrono::locate_zone("GMT"));
	EXPECT_EQ(ZoneIdTransformer::of("+00:00"), utc);
	EXPECT_EQ(ZoneIdTransformer::of("-00"), utc);
	EXPECT_EQ(ZoneIdTransformer::of("UTC+0"), utc);

	TimeZone plus2 = std::chrono::locate_zone("Etc/GMT-2");
	EXPECT_EQ(ZoneIdTransformer::of("+2"), plus2);
	EXPECT_EQ(ZoneIdTransformer::of("+02"), plus2);
	EXPECT_EQ(ZoneIdTransformer::of("+0200"), plus2);
	EXPECT_EQ(ZoneIdTransformer::of("+02:00"), plus2);
	EXPECT_EQ(ZoneIdTransformer::of("+020000"), plus2);
	EXPECT_EQ(ZoneIdTransformer::of("+02:00:00"), plus2);
	EXPECT_EQ(ZoneIdTransformer::of("UTC+2"), plus2);
	EXPECT_EQ(ZoneIdTransformer::of("GMT+02:00"), plus2);
	EXPECT_EQ(ZoneIdTransformer::of("UT+02"), plus2);
	EXPECT_EQ(ZoneIdTransformer::of("-12:00"), std::chrono::locate_zone("Etc/GMT+12"));
	EXPECT_EQ(ZoneIdTransformer::of("+14"), std::chrono::locate_zone("Etc/GMT-14"));
	auto now = std::chrono::system_clock::now();
	EXPECT_EQ(ZoneIdTransformer::of("-05:00")->get_info(now).offset, std::chrono::hours(-5));
}

TEST(ZoneIdTransformerTest, InvalidOffsets) {
	EXPECT_EQ(errorMessage("+"), "Invalid ID for ZoneOffset, invalid format: +");
	EXPECT_EQ(errorMessage("+123"), "Invalid ID for ZoneOffset, invalid format: +123");
	EXPECT_EQ(errorMessage("+1a"), "Invalid ID for ZoneOffset, non numeric characters found: +1a");
	EXPECT_EQ(errorMessage("+01-00"), "Invalid ID for ZoneOffset, colon not found when expected: +01-00");
	EXPECT_EQ(errorMessage("+19"), "Zone offset hours not in valid range: value 19 is not in the range -18 to 18");
	EXPECT_EQ(errorMessage("+18:01"), "Zone offset not in valid range: -18:00 to +18:00");
	EXPECT_EQ(errorMessage("+01:60"), "Zone offset minutes not in valid range: value 60 is not in the range -59 to 59");
	EXPECT_EQ(errorMessage("UTC+x"), "Invalid ID for offset-based ZoneId: UTC+x");
	// Deviation: not representable by std::chrono
	EXPECT_EQ(errorMessage("+01:30"), "Fixed offset time zones other than whole hours between -12 and +14 are not supported: +01:30");
	EXPECT_EQ(errorMessage("GMT-13"), "Invalid ID for offset-based ZoneId: GMT-13");
}
