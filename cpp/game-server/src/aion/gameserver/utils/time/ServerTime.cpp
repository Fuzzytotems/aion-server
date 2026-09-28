#include "aion/gameserver/utils/time/ServerTime.h"

#include <string>

#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::utils::time {

using namespace std::chrono;

namespace {

[[noreturn]] void throwParseError(std::string_view text, size_t index) {
	throw runtime::IllegalArgumentException("Text '" + std::string(text) + "' could not be parsed at index " + std::to_string(index));
}

/** Reads exactly `digits` decimal digits at pos */
int32_t readDigits(std::string_view text, size_t& pos, size_t digits) {
	if (pos + digits > text.size())
		throwParseError(text, pos);
	int32_t value = 0;
	for (size_t i = 0; i < digits; ++i) {
		char c = text[pos + i];
		if (c < '0' || c > '9')
			throwParseError(text, pos + i);
		value = value * 10 + (c - '0');
	}
	pos += digits;
	return value;
}

void expect(std::string_view text, size_t& pos, char c) {
	if (pos >= text.size() || text[pos] != c)
		throwParseError(text, pos);
	++pos;
}

/**
 * Java ZonedDateTime.ofLocal without a preferred offset (ZonedDateTime.of): a local time in a gap is shifted forward by the gap (the offset
 * before it), an ambiguous one takes the earlier offset
 */
sys_time<milliseconds> toInstant(const time_zone* zone, local_time<milliseconds> local) {
	local_info info = zone->get_info(local);
	return sys_time<milliseconds>(local.time_since_epoch() - info.first.offset);
}

} // namespace

const std::chrono::time_zone* ServerTime::zone() {
	const std::chrono::time_zone* timeZone = configs::main::GSConfig::TIME_ZONE_ID.load();
	if (timeZone == nullptr)
		throw runtime::NullPointerException("zone");
	return timeZone;
}

ServerTime::ZonedDateTime ServerTime::now() {
	return ZonedDateTime(zone(), floor<milliseconds>(system_clock::now()));
}

ServerTime::ZonedDateTime ServerTime::of(LocalDateTime localDateTime) {
	const time_zone* timeZone = zone();
	return ZonedDateTime(timeZone, toInstant(timeZone, localDateTime));
}

ServerTime::ZonedDateTime ServerTime::atDate(commons::database::Timestamp date) {
	return ZonedDateTime(zone(), date);
}

ServerTime::ZonedDateTime ServerTime::ofEpochMilli(int64_t epochMilli) {
	return ZonedDateTime(zone(), sys_time<milliseconds>(milliseconds(epochMilli)));
}

ServerTime::ZonedDateTime ServerTime::ofEpochSecond(int64_t epochSecond) {
	return ZonedDateTime(zone(), sys_time<milliseconds>(seconds(epochSecond)));
}

ServerTime::LocalDateTime ServerTime::parseLocalDateTime(std::string_view text, size_t& pos) {
	size_t yearStart = pos;
	int32_t yearValue = readDigits(text, pos, 4);
	expect(text, pos, '-');
	int32_t monthValue = readDigits(text, pos, 2);
	expect(text, pos, '-');
	int32_t dayValue = readDigits(text, pos, 2);
	year_month_day date{year(yearValue), month(static_cast<unsigned>(monthValue)), day(static_cast<unsigned>(dayValue))};
	if (!date.ok())
		throwParseError(text, yearStart);
	expect(text, pos, 'T');
	size_t timeStart = pos;
	int32_t hourValue = readDigits(text, pos, 2);
	expect(text, pos, ':');
	int32_t minuteValue = readDigits(text, pos, 2);
	int32_t secondValue = 0;
	int32_t millis = 0;
	if (pos < text.size() && text[pos] == ':') {
		++pos;
		secondValue = readDigits(text, pos, 2);
		if (pos < text.size() && text[pos] == '.') {
			++pos;
			size_t fractionDigits = 0;
			while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9' && fractionDigits < 9) {
				if (fractionDigits < 3)
					millis = millis * 10 + (text[pos] - '0');
				++pos;
				++fractionDigits;
			}
			if (fractionDigits == 0)
				throwParseError(text, pos);
			for (size_t i = fractionDigits; i < 3; ++i)
				millis *= 10;
		}
	}
	if (hourValue > 23 || minuteValue > 59 || secondValue > 59)
		throwParseError(text, timeStart);
	return local_days(date) + hours(hourValue) + minutes(minuteValue) + seconds(secondValue) + milliseconds(millis);
}

ServerTime::ZonedDateTime ServerTime::parse(std::string_view text) {
	size_t pos = 0;
	LocalDateTime local = parseLocalDateTime(text, pos);
	seconds offset{0};
	if (pos < text.size() && text[pos] == 'Z') {
		++pos;
	} else if (pos < text.size() && (text[pos] == '+' || text[pos] == '-')) {
		bool negative = text[pos] == '-';
		++pos;
		int32_t offsetHours = readDigits(text, pos, 2);
		expect(text, pos, ':');
		int32_t offsetMinutes = readDigits(text, pos, 2);
		int32_t offsetSeconds = 0;
		if (pos < text.size() && text[pos] == ':') {
			++pos;
			offsetSeconds = readDigits(text, pos, 2);
		}
		if (offsetHours > 18 || offsetMinutes > 59 || offsetSeconds > 59)
			throwParseError(text, pos);
		offset = hours(offsetHours) + minutes(offsetMinutes) + seconds(offsetSeconds);
		if (negative)
			offset = -offset;
	} else {
		throwParseError(text, pos);
	}
	// Java: java.time.format.Parsed.resolveInstant gives the parsed offset priority over the zone ("Offset (if present) will be given priority
	// over the zone"), and ZonedDateTime.from uses that INSTANT_SECONDS; the region id only has to be valid, withZoneSameInstant replaces it
	sys_time<milliseconds> instant(local.time_since_epoch() - offset);
	if (pos < text.size() && text[pos] == '[') {
		size_t end = text.find(']', pos);
		if (end == std::string_view::npos || end + 1 != text.size())
			throwParseError(text, pos);
		std::string regionId(text.substr(pos + 1, end - pos - 1));
		try {
			static_cast<void>(locate_zone(regionId));
		} catch (const std::runtime_error&) {
			throwParseError(text, pos + 1);
		}
		pos = end + 1;
	}
	if (pos != text.size())
		throwParseError(text, pos);
	return ZonedDateTime(zone(), instant);
}

ServerTime::ZonedDateTime ServerTime::parseLocal(std::string_view text) {
	size_t pos = 0;
	LocalDateTime local = parseLocalDateTime(text, pos);
	if (pos != text.size())
		throwParseError(text, pos);
	return of(local);
}

int32_t ServerTime::getDaylightSavings() {
	sys_info info = zone()->get_info(system_clock::now());
	return static_cast<int32_t>(duration_cast<seconds>(info.save).count());
}

int32_t ServerTime::getOffset() {
	sys_info info = zone()->get_info(system_clock::now());
	return static_cast<int32_t>(info.offset.count());
}

int32_t ServerTime::getStandardOffset() {
	sys_info info = zone()->get_info(system_clock::now());
	return static_cast<int32_t>((info.offset - duration_cast<seconds>(info.save)).count());
}

} // namespace aion::gameserver::utils::time
