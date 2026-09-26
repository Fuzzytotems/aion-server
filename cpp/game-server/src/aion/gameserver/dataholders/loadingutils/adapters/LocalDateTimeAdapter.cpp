#include "aion/gameserver/dataholders/loadingutils/adapters/LocalDateTimeAdapter.h"

#include <cstdio>

#include "aion/gameserver/dataholders/loadingutils/BindContext.h"
#include "aion/gameserver/dataholders/loadingutils/XmlValues.h"

namespace aion::gameserver::xml::adapters {

namespace {

[[noreturn]] void invalid(std::string_view value, std::string_view reason) {
	throw XmlValueException("Text '" + std::string(value) + "' could not be parsed as LocalDateTime: " + std::string(reason));
}

/** reads exactly `digits` ASCII digits at `pos` */
int32_t readDigits(std::string_view value, size_t& pos, size_t digits) {
	if (pos + digits > value.size())
		invalid(value, "unexpected end");
	int32_t result = 0;
	for (size_t i = 0; i < digits; ++i, ++pos) {
		char c = value[pos];
		if (c < '0' || c > '9')
			invalid(value, "digit expected at index " + std::to_string(pos));
		result = result * 10 + (c - '0');
	}
	return result;
}

void expect(std::string_view value, size_t& pos, char expected) {
	if (pos >= value.size() || value[pos] != expected)
		invalid(value, std::string("'") + expected + "' expected at index " + std::to_string(pos));
	++pos;
}

} // namespace

LocalDateTime parseLocalDateTime(std::string_view value) {
	using namespace std::chrono;
	size_t pos = 0;
	if (!value.empty() && (value[0] == '+' || value[0] == '-'))
		invalid(value, "signed years are not supported");
	int32_t yearValue = readDigits(value, pos, 4);
	if (pos < value.size() && value[pos] >= '0' && value[pos] <= '9')
		invalid(value, "years with more than 4 digits are not supported");
	expect(value, pos, '-');
	int32_t monthValue = readDigits(value, pos, 2);
	expect(value, pos, '-');
	int32_t dayValue = readDigits(value, pos, 2);
	if (pos >= value.size() || (value[pos] != 'T' && value[pos] != 't'))
		invalid(value, "'T' expected at index " + std::to_string(pos));
	++pos;
	int32_t hourValue = readDigits(value, pos, 2);
	expect(value, pos, ':');
	int32_t minuteValue = readDigits(value, pos, 2);
	int32_t secondValue = 0;
	int32_t millisValue = 0;
	if (pos < value.size() && value[pos] == ':') {
		++pos;
		secondValue = readDigits(value, pos, 2);
		if (pos < value.size() && value[pos] == '.') {
			++pos;
			size_t fractionDigits = 0;
			for (; pos < value.size() && value[pos] >= '0' && value[pos] <= '9'; ++pos, ++fractionDigits) {
				if (fractionDigits >= 9)
					invalid(value, "more than 9 fraction digits");
				int32_t digit = value[pos] - '0';
				if (fractionDigits < 3)
					millisValue = millisValue * 10 + digit;
				else if (digit != 0)
					invalid(value, "sub-millisecond precision is not supported");
			}
			for (size_t i = fractionDigits; i < 3; ++i)
				millisValue *= 10;
		}
	}
	if (pos != value.size())
		invalid(value, "unparsed text found at index " + std::to_string(pos));
	if (monthValue < 1 || monthValue > 12)
		invalid(value, "Invalid value for MonthOfYear: " + std::to_string(monthValue));
	year_month_day date{year{yearValue}, month{static_cast<unsigned>(monthValue)}, day{static_cast<unsigned>(dayValue)}};
	if (dayValue < 1 || !date.ok())
		invalid(value, "Invalid date");
	if (hourValue > 23)
		invalid(value, "Invalid value for HourOfDay: " + std::to_string(hourValue));
	if (minuteValue > 59)
		invalid(value, "Invalid value for MinuteOfHour: " + std::to_string(minuteValue));
	if (secondValue > 59)
		invalid(value, "Invalid value for SecondOfMinute: " + std::to_string(secondValue));
	return local_days{date} + hours{hourValue} + minutes{minuteValue} + seconds{secondValue} + milliseconds{millisValue};
}

LocalDateTime parseLocalDateTime(BindContext& context, std::string_view value) {
	return context.adapt([](std::string_view v) { return parseLocalDateTime(v); }, value);
}

std::string printLocalDateTime(LocalDateTime value) {
	using namespace std::chrono;
	local_days days = floor<std::chrono::days>(value);
	year_month_day date{days};
	hh_mm_ss<milliseconds> time{value - days};
	char buffer[64];
	int length = std::snprintf(buffer, sizeof(buffer), "%04d-%02u-%02uT%02d:%02d", static_cast<int>(date.year()), static_cast<unsigned>(date.month()),
	                           static_cast<unsigned>(date.day()), static_cast<int>(time.hours().count()), static_cast<int>(time.minutes().count()));
	std::string result(buffer, static_cast<size_t>(length));
	auto secondsValue = time.seconds().count();
	auto millisValue = time.subseconds().count();
	if (secondsValue > 0 || millisValue > 0) {
		length = std::snprintf(buffer, sizeof(buffer), ":%02d", static_cast<int>(secondsValue));
		result.append(buffer, static_cast<size_t>(length));
		if (millisValue > 0) {
			length = std::snprintf(buffer, sizeof(buffer), ".%03d", static_cast<int>(millisValue));
			result.append(buffer, static_cast<size_t>(length));
		}
	}
	return result;
}

} // namespace aion::gameserver::xml::adapters
