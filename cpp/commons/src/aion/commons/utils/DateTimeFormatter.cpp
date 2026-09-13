#include "aion/commons/utils/DateTimeFormatter.h"

#include <array>
#include <ctime>

#include "aion/commons/utils/Exception.h"

namespace aion::commons::utils {

namespace {

constexpr std::array<std::string_view, 12> MONTH_NAMES = {"January", "February", "March",     "April",   "May",      "June",
																													 "July",    "August",   "September", "October", "November", "December"};
constexpr std::array<std::string_view, 7> DAY_NAMES = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};

void appendPadded(std::string& out, int64_t value, int32_t minWidth) {
	if (value < 0) {
		out += '-';
		value = -value;
	}
	char digits[20];
	int length = 0;
	do {
		digits[length++] = static_cast<char>('0' + value % 10);
		value /= 10;
	} while (value > 0);
	for (int i = length; i < minWidth; i++)
		out += '0';
	while (length > 0)
		out += digits[--length];
}

/**
 * Java: DateTimeFormatterBuilder.appendOffset(pattern, noOffsetText).
 *
 * @param withColons +HH:MM instead of +HHMM
 * @param minutes 0: omitted if zero, 1: always
 * @param seconds 0: never, 1: if non-zero
 */
void appendOffset(std::string& out, int32_t offsetSeconds, bool withColons, int minutes, int seconds, std::string_view noOffsetText) {
	if (offsetSeconds == 0) {
		out += noOffsetText;
		return;
	}
	int32_t absolute = offsetSeconds < 0 ? -offsetSeconds : offsetSeconds;
	int32_t h = absolute / 3600;
	int32_t m = absolute / 60 % 60;
	int32_t s = absolute % 60;
	out += offsetSeconds < 0 ? '-' : '+';
	appendPadded(out, h, 2);
	if (minutes == 1 || m != 0 || (seconds == 1 && s != 0)) {
		if (withColons)
			out += ':';
		appendPadded(out, m, 2);
		if (seconds == 1 && s != 0) {
			if (withColons)
				out += ':';
			appendPadded(out, s, 2);
		}
	}
}

[[noreturn]] void throwTooMany(char letter) {
	throw IllegalArgumentException(std::string("Too many pattern letters: ") + letter);
}

} // namespace

DateTimeFields DateTimeFields::of(std::chrono::system_clock::time_point time, std::chrono::seconds utcOffset) {
	using namespace std::chrono;
	// computed in the clock's own resolution: converting the whole time point to nanoseconds would overflow for years outside about 1677-2262
	// (e.g. file times), only the time of day is converted
	auto local = time + utcOffset;
	sys_days days = floor<std::chrono::days>(local);
	year_month_day date(days);
	hh_mm_ss<nanoseconds> timeOfDay(duration_cast<nanoseconds>(local - days));

	DateTimeFields fields;
	fields.year = static_cast<int32_t>(date.year());
	fields.month = static_cast<int32_t>(static_cast<unsigned>(date.month()));
	fields.day = static_cast<int32_t>(static_cast<unsigned>(date.day()));
	fields.dayOfWeek = static_cast<int32_t>(weekday(days).iso_encoding());
	fields.dayOfYear = static_cast<int32_t>((days - sys_days(date.year() / January / 1)).count()) + 1;
	fields.hour = static_cast<int32_t>(timeOfDay.hours().count());
	fields.minute = static_cast<int32_t>(timeOfDay.minutes().count());
	fields.second = static_cast<int32_t>(timeOfDay.seconds().count());
	fields.nano = static_cast<int32_t>(timeOfDay.subseconds().count());
	fields.offsetSeconds = static_cast<int32_t>(utcOffset.count());
	return fields;
}

DateTimeFields DateTimeFields::of(std::chrono::system_clock::time_point time, const std::chrono::time_zone* timeZone) {
	return of(time, getUtcOffset(std::chrono::floor<std::chrono::seconds>(time), timeZone));
}

std::chrono::seconds getUtcOffset(std::chrono::sys_seconds time, const std::chrono::time_zone* timeZone) {
	if (timeZone)
		return timeZone->get_info(time).offset;
	std::time_t t = static_cast<std::time_t>(time.time_since_epoch().count());
	std::tm local{};
#ifdef _WIN32
	if (localtime_s(&local, &t) != 0)
		return std::chrono::seconds(0);
	return std::chrono::seconds(_mkgmtime(&local) - t);
#else
	if (!localtime_r(&t, &local))
		return std::chrono::seconds(0);
	return std::chrono::seconds(local.tm_gmtoff);
#endif
}

const std::chrono::time_zone* findTimeZone(std::string_view zoneId) {
	if (zoneId.empty())
		return nullptr;
	try {
		return std::chrono::locate_zone(zoneId);
	} catch (const std::exception&) {
		throw IllegalArgumentException("Unknown time zone: " + std::string(zoneId), std::current_exception());
	}
}

DateTimeFormatter DateTimeFormatter::ofPattern(std::string_view pattern) {
	DateTimeFormatter formatter;
	auto addLiteral = [&](std::string_view text) {
		if (formatter.elements.empty() || formatter.elements.back().field != Field::LITERAL)
			formatter.elements.push_back({Field::LITERAL});
		formatter.elements.back().literal += text;
	};
	for (size_t pos = 0; pos < pattern.size();) {
		char c = pattern[pos];
		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
			size_t start = pos;
			while (pos < pattern.size() && pattern[pos] == c)
				pos++;
			int32_t count = static_cast<int32_t>(pos - start);
			Field field;
			switch (c) {
				case 'y':
				case 'u':
					field = count == 2 ? Field::YEAR_2_DIGITS : Field::YEAR;
					break;
				case 'M':
				case 'L':
					if (count > 4)
						throwTooMany(c);
					field = count == 3 ? Field::MONTH_SHORT_NAME : count == 4 ? Field::MONTH_NAME : Field::MONTH;
					break;
				case 'd':
					if (count > 2)
						throwTooMany(c);
					field = Field::DAY_OF_MONTH;
					break;
				case 'D':
					if (count > 3)
						throwTooMany(c);
					field = Field::DAY_OF_YEAR;
					break;
				case 'E':
					if (count > 4)
						throwTooMany(c);
					field = count == 4 ? Field::DAY_OF_WEEK_NAME : Field::DAY_OF_WEEK_SHORT_NAME;
					break;
				case 'a':
					if (count > 1)
						throwTooMany(c);
					field = Field::AM_PM;
					break;
				case 'H':
				case 'k':
				case 'K':
				case 'h':
				case 'm':
				case 's':
					if (count > 2)
						throwTooMany(c);
					field = c == 'H' ? Field::HOUR_OF_DAY
						: c == 'k'     ? Field::CLOCK_HOUR_OF_DAY
						: c == 'K'     ? Field::HOUR_OF_AM_PM
						: c == 'h'     ? Field::CLOCK_HOUR_OF_AM_PM
						: c == 'm'     ? Field::MINUTE
													 : Field::SECOND;
					break;
				case 'S':
					if (count > 9)
						throwTooMany(c);
					field = Field::FRACTION;
					break;
				case 'X':
				case 'x':
				case 'Z':
					if (count > 5)
						throwTooMany(c);
					field = c == 'X' ? Field::OFFSET_X : c == 'x' ? Field::OFFSET_x : Field::OFFSET_Z;
					break;
				default:
					throw IllegalArgumentException(std::string("Unsupported pattern letter: ") + c);
			}
			formatter.elements.push_back({field, count});
		} else if (c == '\'') {
			size_t end = ++pos;
			std::string text;
			while (true) {
				if (end >= pattern.size())
					throw IllegalArgumentException("Pattern ends with an incomplete string literal: " + std::string(pattern));
				if (pattern[end] == '\'') {
					if (end + 1 < pattern.size() && pattern[end + 1] == '\'') {
						text += '\'';
						end += 2;
						continue;
					}
					break;
				}
				text += pattern[end++];
			}
			addLiteral(end == pos ? "'" : text); // '' outside of a literal is a single quote
			pos = end + 1;
		} else if (c == '[' || c == ']' || c == '{' || c == '}' || c == '#') {
			throw IllegalArgumentException(std::string("Unsupported pattern character: ") + c);
		} else {
			addLiteral(pattern.substr(pos++, 1));
		}
	}
	return formatter;
}

DateTimeFormatter DateTimeFormatter::withZone(const std::chrono::time_zone* timeZone) const {
	DateTimeFormatter copy = *this;
	copy.zone = timeZone;
	return copy;
}

std::string DateTimeFormatter::format(std::chrono::system_clock::time_point time) const {
	std::string out;
	formatTo(out, DateTimeFields::of(time, zone));
	return out;
}

void DateTimeFormatter::formatTo(std::string& out, const DateTimeFields& f) const {
	for (const Element& e : elements) {
		switch (e.field) {
			case Field::LITERAL:
				out += e.literal;
				break;
			case Field::YEAR:
				appendPadded(out, f.year, e.count);
				break;
			case Field::YEAR_2_DIGITS:
				appendPadded(out, ((f.year % 100) + 100) % 100, 2);
				break;
			case Field::MONTH:
				appendPadded(out, f.month, e.count);
				break;
			case Field::MONTH_SHORT_NAME:
				out += MONTH_NAMES[static_cast<size_t>(f.month - 1)].substr(0, 3);
				break;
			case Field::MONTH_NAME:
				out += MONTH_NAMES[static_cast<size_t>(f.month - 1)];
				break;
			case Field::DAY_OF_MONTH:
				appendPadded(out, f.day, e.count);
				break;
			case Field::DAY_OF_YEAR:
				appendPadded(out, f.dayOfYear, e.count);
				break;
			case Field::DAY_OF_WEEK_SHORT_NAME:
				out += DAY_NAMES[static_cast<size_t>(f.dayOfWeek - 1)].substr(0, 3);
				break;
			case Field::DAY_OF_WEEK_NAME:
				out += DAY_NAMES[static_cast<size_t>(f.dayOfWeek - 1)];
				break;
			case Field::AM_PM:
				out += f.hour < 12 ? "AM" : "PM";
				break;
			case Field::HOUR_OF_DAY:
				appendPadded(out, f.hour, e.count);
				break;
			case Field::CLOCK_HOUR_OF_DAY:
				appendPadded(out, f.hour == 0 ? 24 : f.hour, e.count);
				break;
			case Field::HOUR_OF_AM_PM:
				appendPadded(out, f.hour % 12, e.count);
				break;
			case Field::CLOCK_HOUR_OF_AM_PM:
				appendPadded(out, f.hour % 12 == 0 ? 12 : f.hour % 12, e.count);
				break;
			case Field::MINUTE:
				appendPadded(out, f.minute, e.count);
				break;
			case Field::SECOND:
				appendPadded(out, f.second, e.count);
				break;
			case Field::FRACTION: {
				int32_t value = f.nano;
				for (int32_t digits = 9; digits > e.count; digits--)
					value /= 10;
				appendPadded(out, value, e.count);
				break;
			}
			case Field::OFFSET_X:
			case Field::OFFSET_x: {
				bool x = e.field == Field::OFFSET_x;
				static constexpr std::array<std::string_view, 5> ZERO_x = {"+00", "+0000", "+00:00", "+0000", "+00:00"};
				std::string_view noOffsetText = x ? ZERO_x[static_cast<size_t>(e.count - 1)] : "Z";
				// patterns: +HHmm, +HHMM, +HH:MM, +HHMMss, +HH:MM:ss
				appendOffset(out, f.offsetSeconds, e.count == 3 || e.count == 5, e.count == 1 ? 0 : 1, e.count >= 4 ? 1 : 0, noOffsetText);
				break;
			}
			case Field::OFFSET_Z:
				if (e.count <= 3) {
					appendOffset(out, f.offsetSeconds, false, 1, 0, "+0000");
				} else if (e.count == 4) {
					out += "GMT";
					if (f.offsetSeconds != 0)
						appendOffset(out, f.offsetSeconds, true, 1, 1, "");
				} else {
					appendOffset(out, f.offsetSeconds, true, 1, 1, "Z");
				}
				break;
		}
	}
}

} // namespace aion::commons::utils
