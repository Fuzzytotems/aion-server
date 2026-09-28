// CronExpression (design §7.5): parsing of the Quartz subset, golden next fire times, DST rules and conformance of every cron string the Java
// server uses (configs, config/schedule/*.xml, goods list sales times, sources, siege preparation strings) against an independent brute-force
// oracle in several time zones.

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/configuration/transformers/PropertyTransformer.h"
#include "aion/gameserver/services/cron/CronExpression.h"
#include "aion/gameserver/services/cron/CronService.h"

namespace {

using namespace std::chrono;
using aion::gameserver::services::cron::CronExpression;
using aion::gameserver::services::cron::CronExpressionParseException;
using aion::gameserver::services::cron::CronExpressions;

sys_seconds utc(int y, unsigned mo, unsigned d, int h = 0, int mi = 0, int s = 0) {
	return sys_days(year(y) / month(mo) / day(d)) + hours(h) + minutes(mi) + seconds(s);
}

std::string format(std::optional<sys_seconds> time) {
	return time ? std::format("{:%Y-%m-%dT%H:%M:%S}Z", *time) : std::string("never");
}

const time_zone* zone(std::string_view name) {
	return locate_zone(name);
}

std::optional<sys_seconds> next(std::string_view expression, sys_seconds after, std::string_view zoneName = "UTC") {
	return CronExpression(expression).getNextValidTimeAfter(after, zone(zoneName));
}

// ------------------------------------------------------------------------------------------------------------------------------ parsing

TEST(CronExpressionTest, AcceptsTheQuartzSubset) {
	for (std::string_view text : {"0 0 12 ? * MON-FRI", "0 0/5 * * * ?", "0 0 9 ? * WED *", "0 0 09-18 ? * FRI", "0 30 17 3,18 * ?", "* * * * * ?",
			 "10-40/10 5/20 */6 ? * SUN,SAT", "/15 0 0 1 JAN-DEC ?", "0 0 22-2 * * ?", "0 0 12 ? * FRI-MON", "0 0 12 1 1 ? 2025,2027-2030/2",
			 "0\t0  12 ? *\tsun", "0 0 12 ? * 1-7"}) {
		EXPECT_TRUE(CronExpression::isValidExpression(text)) << text;
	}
	CronExpression expression("0 0 12 ? * mon-fri");
	EXPECT_EQ(expression.getCronExpression(), "0 0 12 ? * mon-fri");
	EXPECT_EQ(expression.toString(), "0 0 12 ? * mon-fri");
	EXPECT_EQ(expression, CronExpression("0 0 12 ? * mon-fri"));
}

TEST(CronExpressionTest, RejectsInvalidExpressions) {
	for (std::string_view text : {
			 "",                        // empty
			 "0 0 12 ? *",              // five fields
			 "0 0 12 ? * * 2025 extra", // eight fields
			 "0 0 12 * * *",            // neither day field is '?'
			 "0 0 12 ? * ?",            // both are '?'
			 "? 0 12 ? * *",            // '?' outside the day fields
			 "0 0 12 1,? * ?",          // '?' in a list
			 "60 0 12 ? * *",           // second out of range
			 "0 60 12 ? * *",           // minute out of range
			 "0 0 24 ? * *",            // hour out of range
			 "0 0 12 0 * ?",            // day-of-month out of range
			 "0 0 12 32 * ?",           // day-of-month out of range
			 "0 0 12 ? 13 *",           // month out of range
			 "0 0 12 ? * 8",            // day-of-week out of range
			 "0 0 12 ? * 0",            // day-of-week out of range (Quartz: 1-7)
			 "0 0 12 1 1 ? 1969",       // year out of range
			 "0 0 12 1 1 ? 2030-2025",  // descending year range
			 "0 0 12 ? * MOX",          // unknown day name
			 "0 0 12 ? FOO *",          // unknown month name
			 "0 0 MON ? * *",           // name in a numeric field
			 "0 0 12 ? * MONX",         // trailing characters after a name
			 "0 0 12x ? * *",           // trailing characters after a number
			 "0 0/0 12 ? * *",          // increment 0
			 "0 0/60 12 ? * *",         // increment too large
			 "0 0 12/24 ? * *",         // increment too large
			 "0 0 12 ? * 1/8",          // increment too large
			 "0 0/ 12 ? * *",           // '/' without a number
			 "0 0 12 ? * 1,",           // empty list element
			 "0 0 12 ? * ,1",           // empty list element
			 "0 0 12 1-? * ?",          // '?' in a range
			 "0 0 1234567890 ? * *",    // number too large
			 "0 0 12 ? * -",            // incomplete range
		 }) {
		EXPECT_FALSE(CronExpression::isValidExpression(text)) << "'" << text << "'";
		EXPECT_THROW(CronExpression{text}, CronExpressionParseException) << "'" << text << "'";
	}
}

TEST(CronExpressionTest, RejectsLWAndHashWithAClearMessage) {
	for (std::string_view text : {"0 0 12 L * ?", "0 0 12 LW * ?", "0 0 12 15W * ?", "0 0 12 ? * 6L", "0 0 12 ? * FRIL", "0 0 12 ? * 6#3", "0 0 12 ? * FRI#3",
			 "0 0 12 L-3 * ?", "0 0 12 W * ?"}) {
		try {
			CronExpression expression(text);
			ADD_FAILURE() << "accepted: " << text;
		} catch (const CronExpressionParseException& e) {
			EXPECT_NE(std::string(e.what()).find("L, W and # is not implemented"), std::string::npos) << text << ": " << e.what();
			EXPECT_NE(std::string(e.what()).find(text), std::string::npos) << "the message names the expression: " << e.what();
		}
	}
}

// ------------------------------------------------------------------------------------------------------------------- golden fire times

struct Golden {
	std::string_view expression;
	sys_seconds after;
	std::string_view zone;
	std::optional<sys_seconds> expected;
};

TEST(CronExpressionTest, GoldenNextFireTimes) {
	const sys_seconds monday = utc(2024, 1, 1); // 2024-01-01T00:00:00Z is a Monday
	const std::vector<Golden> goldens = {
		// configs (autogroup, custom, housing, ranking, siege properties and @Property defaults)
		{"0 0 0,12,20 ? * *", monday, "UTC", utc(2024, 1, 1, 12)},
		{"0 0 0,12,20 ? * *", utc(2024, 1, 1, 20), "UTC", utc(2024, 1, 2, 0)},
		{"0 0 0,20 ? * MON,WED,SAT", monday, "UTC", utc(2024, 1, 1, 20)},
		{"0 0 0,20 ? * MON,WED,SAT", utc(2024, 1, 1, 20), "UTC", utc(2024, 1, 3, 0)},
		{"0 0 12,19 ? * *", monday, "UTC", utc(2024, 1, 1, 12)},
		{"0 0 0,12 ? * SUN", monday, "UTC", utc(2024, 1, 7, 0)},
		{"0 0 23 ? * *", monday, "UTC", utc(2024, 1, 1, 23)},
		{"0 0 16 ? * SAT", monday, "UTC", utc(2024, 1, 6, 16)},
		{"0 0 16 ? * SUN", monday, "UTC", utc(2024, 1, 7, 16)},
		{"0 0 0 ? * *", monday, "UTC", utc(2024, 1, 2, 0)},
		{"0 30 14,18,21 ? * *", monday, "UTC", utc(2024, 1, 1, 14, 30)},
		{"0 0 12 ? * SUN", monday, "UTC", utc(2024, 1, 7, 12)},
		{"0 0 0 ? * MON", monday, "UTC", utc(2024, 1, 8, 0)},
		{"0 0 12 ? * *", monday, "UTC", utc(2024, 1, 1, 12)},
		{"0 0 22 ? * SUN", monday, "UTC", utc(2024, 1, 7, 22)},
		{"0 50 18 ? * SUN", monday, "UTC", utc(2024, 1, 7, 18, 50)},
		{"0 0 5 ? * WED", monday, "UTC", utc(2024, 1, 3, 5)},
		// sources
		{"0 0 9 ? * WED *", monday, "UTC", utc(2024, 1, 3, 9)},
		{"0 0/5 * ? * *", monday, "UTC", utc(2024, 1, 1, 0, 5)},
		{"0 0/5 * ? * *", utc(2024, 1, 1, 23, 59, 59), "UTC", utc(2024, 1, 2)},
		{"0 0 9 ? * *", monday, "UTC", utc(2024, 1, 1, 9)},
		{"0 0 * ? * *", monday, "UTC", utc(2024, 1, 1, 1)},
		{"0 0 * ? * *", utc(2024, 1, 1, 0, 59, 59), "UTC", utc(2024, 1, 1, 1)},
		// config/schedule
		{"0 0 17 ? * SAT", monday, "UTC", utc(2024, 1, 6, 17)},
		{"0 0 18 ? * FRI,MON", monday, "UTC", utc(2024, 1, 1, 18)},
		{"0 0 18 ? * FRI,MON", utc(2024, 1, 1, 18), "UTC", utc(2024, 1, 5, 18)},
		{"0 0 21 ? * FRI", monday, "UTC", utc(2024, 1, 5, 21)},
		{"0 0 17 ? * TUE,THU,SAT", monday, "UTC", utc(2024, 1, 2, 17)},
		{"0 0 19 ? * MON", monday, "UTC", utc(2024, 1, 1, 19)},
		{"0 55 20 ? * FRI", monday, "UTC", utc(2024, 1, 5, 20, 55)},
		{"0 30 17 3,18 * ?", monday, "UTC", utc(2024, 1, 3, 17, 30)},
		{"0 30 17 3,18 * ?", utc(2024, 1, 3, 17, 30), "UTC", utc(2024, 1, 18, 17, 30)},
		{"0 30 19 12,27 * ?", utc(2024, 1, 27, 19, 30), "UTC", utc(2024, 2, 12, 19, 30)},
		// goods list sales times
		{"0 0 09-18 ? * FRI", monday, "UTC", utc(2024, 1, 5, 9)},
		{"0 0 09-18 ? * FRI", utc(2024, 1, 5, 18), "UTC", utc(2024, 1, 12, 9)},
		{"0 0 0,10,12,14,18,22 ? * *", monday, "UTC", utc(2024, 1, 1, 10)},
		{"0 0 0 ? * WED", monday, "UTC", utc(2024, 1, 3)},
		// syntax details
		{"* * * * * ?", monday, "UTC", utc(2024, 1, 1, 0, 0, 1)},
		{"0 0 12 ? * 1", monday, "UTC", utc(2024, 1, 7, 12)}, // Quartz day-of-week 1 = SUN
		{"0 0 12 ? * 7", monday, "UTC", utc(2024, 1, 6, 12)}, // 7 = SAT
		{"0 0 12 ? * 2-6/2", utc(2024, 1, 1, 12), "UTC", utc(2024, 1, 3, 12)},   // MON, WED, FRI
		{"0 0 12 ? * FRI-MON", utc(2024, 1, 1, 12), "UTC", utc(2024, 1, 5, 12)}, // wraps: FRI SAT SUN MON
		{"0 0 22-2 * * ?", utc(2024, 1, 1, 2), "UTC", utc(2024, 1, 1, 22)},     // wraps: 22 23 0 1 2
		{"0 0 22-2 * * ?", utc(2024, 1, 1, 23), "UTC", utc(2024, 1, 2, 0)},
		{"0 5/20 * * * ?", utc(2024, 1, 1, 0, 45), "UTC", utc(2024, 1, 1, 1, 5)},
		{"10-40/10 0 0 * * ?", utc(2024, 1, 1, 0, 0, 30), "UTC", utc(2024, 1, 1, 0, 0, 40)},
		{"/15 0 0 * * ?", utc(2024, 1, 1, 0, 0, 30), "UTC", utc(2024, 1, 1, 0, 0, 45)},
		{"0 0 0 31 * ?", monday, "UTC", utc(2024, 1, 31)},
		{"0 0 0 31 * ?", utc(2024, 1, 31), "UTC", utc(2024, 3, 31)}, // skips February and its missing 31st
		{"0 0 0 29 2 ?", monday, "UTC", utc(2024, 2, 29)},
		{"0 0 0 29 2 ?", utc(2024, 2, 29), "UTC", utc(2028, 2, 29)}, // leap years only
		{"0 0 12 1 1 ? 2025,2027", monday, "UTC", utc(2025, 1, 1, 12)},
		{"0 0 12 1 1 ? 2025,2027", utc(2025, 1, 1, 12), "UTC", utc(2027, 1, 1, 12)},
		{"0 0 12 1 1 ? 2025,2027", utc(2027, 1, 1, 12), "UTC", std::nullopt},
		{"0 0 0 30 2 ?", monday, "UTC", std::nullopt}, // never
		{"0 0 12 ? * MON 2299", utc(2299, 12, 31), "UTC", std::nullopt},
		{"0 0 0 1 1 ? *", utc(2298, 6, 1), "UTC", utc(2299, 1, 1)},
		{"0 0 0 1 1 ? *", utc(2299, 6, 1), "UTC", std::nullopt}, // search horizon
		// time zones
		{"0 0 9 ? * *", monday, "Europe/Berlin", utc(2024, 1, 1, 8)},  // CET = UTC+1
		{"0 0 9 ? * *", utc(2024, 7, 1), "Europe/Berlin", utc(2024, 7, 1, 7)}, // CEST = UTC+2
		{"0 0 0 ? * MON", utc(2024, 1, 7, 22), "Europe/Berlin", utc(2024, 1, 7, 23)}, // Monday midnight in Berlin is Sunday 23:00 UTC
		{"0 0 9 ? * *", utc(2023, 12, 31, 12), "Asia/Seoul", utc(2024, 1, 1, 0)}, // 09:00 KST = 00:00Z
		// DST: a nonexistent local time is skipped
		{"0 30 2 * * ?", utc(2024, 3, 30, 23), "Europe/Berlin", utc(2024, 4, 1, 0, 30)}, // 2024-03-31 02:30 does not exist
		{"0 30 2 ? * SUN", utc(2024, 3, 9), "America/New_York", utc(2024, 3, 17, 6, 30)}, // 2024-03-10 02:30 does not exist
		{"0 0 * * * ?", utc(2024, 3, 31, 0, 30), "Europe/Berlin", utc(2024, 3, 31, 1)},    // 01:30 CET -> 02:00 does not exist -> 03:00 CEST
		// DST: an ambiguous local time fires once, at its earlier instant, and the repeated hour yields no further times
		{"0 30 2 * * ?", utc(2024, 10, 26, 23), "Europe/Berlin", utc(2024, 10, 27, 0, 30)}, // 02:30 CEST
		{"0 30 2 * * ?", utc(2024, 10, 27, 0, 30), "Europe/Berlin", utc(2024, 10, 28, 1, 30)},
		{"0 0/15 * * * ?", utc(2024, 10, 27, 0, 45), "Europe/Berlin", utc(2024, 10, 27, 2)}, // 02:45 CEST -> 03:00 CET
		{"0 0/15 * * * ?", utc(2024, 10, 27, 1, 20), "Europe/Berlin", utc(2024, 10, 27, 2)}, // inside the repeated hour (02:20 CET)
	};
	for (const Golden& golden : goldens) {
		std::optional<sys_seconds> actual = next(golden.expression, golden.after, golden.zone);
		EXPECT_EQ(format(actual), format(golden.expected)) << golden.expression << " after " << format(golden.after) << " in " << golden.zone;
	}
}

TEST(CronExpressionTest, IsSatisfiedByMatchesFireTimes) {
	CronExpression daily("0 0 9 ? * *");
	EXPECT_TRUE(daily.isSatisfiedBy(utc(2024, 1, 1, 9), zone("UTC")));
	EXPECT_FALSE(daily.isSatisfiedBy(utc(2024, 1, 1, 9, 0, 1), zone("UTC")));
	EXPECT_FALSE(daily.isSatisfiedBy(utc(2024, 1, 1, 9), zone("Europe/Berlin")));
	EXPECT_TRUE(daily.isSatisfiedBy(utc(2024, 1, 1, 8), zone("Europe/Berlin")));
	CronExpression quarter("0 0/15 * * * ?");
	EXPECT_TRUE(quarter.isSatisfiedBy(utc(2024, 10, 27, 0, 45), zone("Europe/Berlin")));
	EXPECT_FALSE(quarter.isSatisfiedBy(utc(2024, 10, 27, 1, 45), zone("Europe/Berlin"))); // second 02:45 of the repeated hour
	EXPECT_EQ(daily.getTimeAfter(utc(2024, 1, 1), zone("UTC")), utc(2024, 1, 1, 9));
}

TEST(CronExpressionTest, NullZoneIsTheSystemZone) {
	CronExpression expression("0 0 9 ? * *");
	EXPECT_EQ(expression.getNextValidTimeAfter(utc(2024, 1, 1), nullptr), expression.getNextValidTimeAfter(utc(2024, 1, 1), current_zone()));
}

TEST(CronExpressionTest, CacheAndPropertyTransformer) {
	const CronExpression& first = CronExpressions::getOrCreate("0 0 9 ? * *");
	EXPECT_EQ(&first, &CronExpressions::getOrCreate("0 0 9 ? * *"));
	EXPECT_NE(&first, &CronExpressions::getOrCreate("0 0 9 ? * * *")); // keyed by the exact text
	EXPECT_THROW((void)CronExpressions::getOrCreate("0 0 9 * * *"), CronExpressionParseException);
	using Transformer = aion::commons::configuration::transformers::PropertyTransformer<const CronExpression*>;
	EXPECT_EQ(Transformer::parseObject(""), nullptr);
	EXPECT_EQ(Transformer::parseObject("0 0 9 ? * *"), &first);
	EXPECT_EQ(Transformer::typeName(), "CronExpression");
}

// ----------------------------------------------------------------------------------------------------- conformance with the Java server

/** Independent oracle: a straightforward parser for the syntax the Java server uses and a day/hour/minute/second brute-force search. */
class Oracle {
public:
	explicit Oracle(std::string_view text) {
		std::istringstream in{std::string(text)};
		std::vector<std::string> fields;
		for (std::string field; in >> field;)
			fields.push_back(field);
		if (fields.size() < 6 || fields.size() > 7)
			throw std::runtime_error("oracle: field count");
		parse(fields[0], 0, 59, {}, seconds);
		parse(fields[1], 0, 59, {}, minutesSet);
		parse(fields[2], 0, 23, {}, hoursSet);
		domAny = fields[3] == "?";
		if (!domAny)
			parse(fields[3], 1, 31, {}, dom);
		parse(fields[4], 1, 12, {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"}, months);
		dowAny = fields[5] == "?";
		if (!dowAny)
			parse(fields[5], 1, 7, {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"}, dow);
		if (fields.size() == 7 && fields[6] != "*")
			throw std::runtime_error("oracle: years are not supported");
	}

	std::optional<sys_seconds> next(sys_seconds after, const time_zone* tz, days horizon = days(400)) const {
		local_seconds start = tz->to_local(after) + 1s;
		for (local_days d = floor<days>(start); d <= floor<days>(start) + horizon; d += days(1)) {
			year_month_day date(d);
			if (!months[static_cast<unsigned>(date.month())])
				continue;
			bool dayMatches = domAny ? dow[weekday(d).c_encoding() + 1] : dom[static_cast<unsigned>(date.day())];
			if (!dayMatches)
				continue;
			for (int h = 0; h < 24; ++h) {
				if (!hoursSet[h])
					continue;
				for (int mi = 0; mi < 60; ++mi) {
					if (!minutesSet[mi])
						continue;
					for (int s = 0; s < 60; ++s) {
						if (!seconds[s])
							continue;
						local_seconds candidate = d + hours(h) + minutes(mi) + std::chrono::seconds(s);
						if (candidate < start)
							continue;
						local_info info = tz->get_info(candidate);
						if (info.result == local_info::nonexistent)
							continue;
						sys_seconds instant(candidate.time_since_epoch() - info.first.offset);
						if (instant > after)
							return instant;
					}
				}
			}
		}
		return std::nullopt;
	}

private:
	static int value(const std::string& token, const std::vector<std::string>& names, int first) {
		for (size_t i = 0; i < names.size(); ++i)
			if (names[i] == token)
				return static_cast<int>(i) + first;
		return std::stoi(token);
	}

	static void parse(const std::string& field, int min, int max, const std::vector<std::string>& names, std::array<bool, 60>& out) {
		std::stringstream list(field);
		for (std::string element; std::getline(list, element, ',');) {
			int step = 1;
			if (size_t slash = element.find('/'); slash != std::string::npos) {
				step = std::stoi(element.substr(slash + 1));
				element = element.substr(0, slash);
				if (element != "*" && element.find('-') == std::string::npos)
					element += "-" + std::to_string(max);
			}
			int from = min;
			int to = max;
			if (element != "*") {
				size_t dash = element.find('-');
				from = value(element.substr(0, dash), names, min);
				to = dash == std::string::npos ? from : value(element.substr(dash + 1), names, min);
			}
			if (to < from)
				throw std::runtime_error("oracle: wrapping ranges are not supported");
			for (int v = from; v <= to; v += step)
				out[static_cast<size_t>(v)] = true;
		}
	}

	std::array<bool, 60> seconds{};
	std::array<bool, 60> minutesSet{};
	std::array<bool, 60> hoursSet{};
	std::array<bool, 60> dom{};
	std::array<bool, 60> months{};
	std::array<bool, 60> dow{};
	bool domAny = false;
	bool dowAny = false;
};

bool isCronTokenChar(char c) {
	return std::isdigit(static_cast<unsigned char>(c)) || (c >= 'A' && c <= 'Z') || c == '*' || c == '?' || c == '/' || c == ',' || c == '-';
}

bool isNumericToken(std::string_view token) {
	return !token.empty() && std::ranges::all_of(token, [](char c) { return std::isdigit(static_cast<unsigned char>(c)) || c == '*' || c == '/' || c == ',' || c == '-'; });
}

/** cron-looking substrings of a line: 6 or 7 space-separated tokens with '?' as day-of-month or day-of-week */
void extractCandidates(std::string_view line, std::set<std::string>& out) {
	size_t pos = 0;
	while (pos < line.size()) {
		while (pos < line.size() && !isCronTokenChar(line[pos]))
			++pos;
		size_t end = pos;
		while (end < line.size() && (isCronTokenChar(line[end]) || line[end] == ' ' || line[end] == '\t'))
			++end;
		std::istringstream run{std::string(line.substr(pos, end - pos))};
		std::vector<std::string> tokens;
		for (std::string token; run >> token;)
			tokens.push_back(token);
		for (size_t i = 0; i + 6 <= tokens.size(); ++i) {
			if (!isNumericToken(tokens[i]) || (tokens[i + 3] != "?" && tokens[i + 5] != "?"))
				continue;
			size_t count = i + 7 <= tokens.size() && (tokens[i + 6] == "*" || (tokens[i + 6].size() == 4 && isNumericToken(tokens[i + 6]))) ? 7 : 6;
			std::string candidate;
			for (size_t k = i; k < i + count; ++k)
				candidate += (k == i ? "" : " ") + tokens[k];
			out.insert(candidate);
			i += count - 1;
		}
		pos = end;
	}
}

void scanFile(const std::filesystem::path& file, std::set<std::string>& out, std::string_view requiredSubstring = {}) {
	std::ifstream in(file, std::ios::binary);
	ASSERT_TRUE(in) << file;
	std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	// visit only the lines containing '?' (the goods lists are megabytes of XML)
	for (size_t question = content.find('?'); question != std::string::npos;) {
		size_t begin = content.rfind('\n', question);
		begin = begin == std::string::npos ? 0 : begin + 1;
		size_t end = content.find('\n', question);
		end = end == std::string::npos ? content.size() : end;
		std::string_view line(content.data() + begin, end - begin);
		question = content.find('?', end);
		if (!requiredSubstring.empty() && line.find(requiredSubstring) == std::string_view::npos)
			continue;
		std::string_view trimmed = line;
		trimmed.remove_prefix(std::min(trimmed.find_first_not_of(" \t"), trimmed.size()));
		if (trimmed.starts_with("#") || trimmed.starts_with("//") || trimmed.starts_with("*") || trimmed.starts_with("<!--"))
			continue;
		extractCandidates(line, out);
	}
}

/** Java SiegeService.getPreparationCronString */
std::string preparationCron(const std::string& siegeTime, int minutesBefore) {
	std::istringstream in(siegeTime);
	std::vector<std::string> parts;
	for (std::string part; in >> part;)
		parts.push_back(part);
	int minutesValue = std::stoi(parts[1]) - minutesBefore;
	int hoursValue = std::stoi(parts[2]);
	if (minutesValue < 0) {
		minutesValue += 60;
		hoursValue -= 1;
	}
	parts[1] = std::to_string(minutesValue);
	parts[2] = std::to_string(hoursValue);
	std::string result;
	for (const std::string& part : parts)
		result += (result.empty() ? "" : " ") + part;
	return result;
}

std::set<std::string> javaServerCronExpressions() {
	const std::filesystem::path java = AION_GAMESERVER_JAVA_DIR;
	std::set<std::string> found;
	for (const auto& entry : std::filesystem::directory_iterator(java / "config" / "main"))
		if (entry.path().extension() == ".properties")
			scanFile(entry.path(), found);
	std::set<std::string> schedules;
	for (const auto& entry : std::filesystem::directory_iterator(java / "config" / "schedule"))
		if (entry.path().extension() == ".xml")
			scanFile(entry.path(), schedules);
	for (const auto& entry : std::filesystem::directory_iterator(java / "data" / "static_data" / "goodslists"))
		if (entry.path().extension() == ".xml")
			scanFile(entry.path(), found, "<salestime>");
	const std::filesystem::path src = java / "src" / "com" / "aionemu" / "gameserver";
	for (const char* file : {"configs/main/AutoGroupConfig.java", "configs/main/CustomConfig.java", "configs/main/HousingConfig.java",
			 "configs/main/RankingConfig.java", "configs/main/SiegeConfig.java", "configs/main/ShutdownConfig.java", "questEngine/QuestEngine.java",
			 "services/AtreianPassportService.java", "services/SiegeService.java", "services/CronJobService.java", "services/event/EventService.java"})
		scanFile(src / file, found);
	for (const std::string& schedule : schedules) {
		found.insert(schedule);
		// siege_schedule.xml siege times also run as preparation crons (5 min before, 10 min for Panesterra fortresses)
		std::istringstream in(schedule);
		std::vector<std::string> parts;
		for (std::string part; in >> part;)
			parts.push_back(part);
		auto digits = [](const std::string& token) { return std::ranges::all_of(token, [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; }); };
		if (digits(parts[1]) && digits(parts[2]) && std::stoi(parts[2]) > 0) {
			found.insert(preparationCron(schedule, 5));
			found.insert(preparationCron(schedule, 10));
		}
	}
	return found;
}

TEST(CronExpressionTest, EveryJavaServerCronStringParsesAndMatchesTheOracle) {
	std::set<std::string> expressions = javaServerCronExpressions();
	// the strings named in research/critic.md and found in the schedule files, sales times and sources must all be discovered
	for (const char* known : {"0 0 0,12,20 ? * *", "0 0 0,20 ? * MON,WED,SAT", "0 0 12,19 ? * *", "0 0 0,12 ? * SUN", "0 0 23 ? * *", "0 0 16 ? * SAT",
			 "0 0 16 ? * SUN", "0 0 0 ? * *", "0 30 14,18,21 ? * *", "0 0 12 ? * SUN", "0 0 0 ? * MON", "0 0 12 ? * *", "0 0 22 ? * SUN", "0 50 18 ? * SUN",
			 "0 0 9 ? * WED *", "0 0/5 * ? * *", "0 0 9 ? * *", "0 0 * ? * *", "0 0 17 ? * TUE,THU,SAT", "0 0 21 ? * FRI", "0 30 17 3,18 * ?",
			 "0 30 19 12,27 * ?", "0 0 18 ? * FRI,MON", "0 0 23 ? * WED,SAT", "0 0 09-18 ? * FRI", "0 0 0,10,12,16,20,22 ? * *", "0 0 10 ? * *",
			 "0 0 12 ? * MON", "0 0 0 ? * WED", "0 55 20 ? * FRI"})
		EXPECT_TRUE(expressions.contains(known)) << "not found in the Java server: " << known;
	EXPECT_GE(expressions.size(), 40u);

	// (zone, instant) pairs: every instant in UTC, DST transition days in zones with DST (std::chrono time zone lookups are slow, especially
	// under ASan, so the cross product is not used)
	struct Case {
		const time_zone* tz;
		sys_seconds after;
	};
	std::vector<Case> utcCases;
	for (sys_seconds after : {utc(2023, 12, 31, 23, 59, 59), utc(2024, 2, 28, 23, 30), utc(2025, 6, 15, 12, 34, 56), utc(2027, 4, 4, 15, 59, 59)})
		utcCases.push_back({zone("UTC"), after});
	std::vector<Case> zoneCases;
	for (sys_seconds after : {utc(2024, 1, 1), utc(2024, 3, 31, 0, 59, 59), utc(2024, 10, 27, 0, 59, 59)})
		zoneCases.push_back({zone("Europe/Berlin"), after});
	for (sys_seconds after : {utc(2024, 3, 10, 6, 59, 59), utc(2024, 11, 3, 5, 30)})
		zoneCases.push_back({zone("America/New_York"), after});
	zoneCases.push_back({zone("Asia/Seoul"), utc(2026, 9, 13, 17, 0)});
	for (sys_seconds after : {utc(2024, 4, 6, 15, 59, 59), utc(2024, 10, 5, 15, 59, 59)}) // Southern hemisphere transitions
		zoneCases.push_back({zone("Australia/Sydney"), after});

	size_t checked = 0;
	size_t expectedChecks = 0;
	size_t index = 0;
	for (const std::string& text : expressions) {
		SCOPED_TRACE(text);
		ASSERT_TRUE(CronExpression::isValidExpression(text)) << "Java server cron string rejected: " << text;
		CronExpression expression(text);
		Oracle oracle(text);
		std::vector<Case> cases = utcCases;
		if (index++ % 3 == 0) // every third expression also across DST transitions
			cases.insert(cases.end(), zoneCases.begin(), zoneCases.end());
		expectedChecks += cases.size();
		for (const Case& test : cases) {
			std::optional<sys_seconds> actual = expression.getNextValidTimeAfter(test.after, test.tz);
			std::optional<sys_seconds> expected = oracle.next(test.after, test.tz);
			if (actual != expected)
				FAIL() << text << " after " << format(test.after) << " in " << test.tz->name() << ": " << format(actual) << " != " << format(expected);
			ASSERT_TRUE(actual.has_value());
			// the fire time after that one as well (walks across the next day/week boundary)
			std::optional<sys_seconds> second = expression.getNextValidTimeAfter(*actual, test.tz);
			if (second != oracle.next(*actual, test.tz))
				FAIL() << text << " after " << format(actual) << " in " << test.tz->name() << ": " << format(second);
			++checked;
		}
		EXPECT_TRUE(expression.isSatisfiedBy(*expression.getNextValidTimeAfter(utc(2024, 1, 1), zone("UTC")), zone("UTC")));
	}
	EXPECT_EQ(checked, expectedChecks);
}

} // namespace
