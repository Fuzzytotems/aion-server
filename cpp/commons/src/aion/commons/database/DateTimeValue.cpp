#include "aion/commons/database/DateTimeValue.h"

#include <algorithm>

#include <fmt/format.h>

namespace aion::commons::database {

namespace {

/** Reads exactly count digits at pos. */
bool readDigits(std::string_view text, size_t& pos, size_t count, uint32_t& out) {
	if (pos + count > text.size())
		return false;
	uint32_t value = 0;
	for (size_t i = 0; i < count; ++i) {
		char c = text[pos + i];
		if (c < '0' || c > '9')
			return false;
		value = value * 10 + static_cast<uint32_t>(c - '0');
	}
	pos += count;
	out = value;
	return true;
}

bool expect(std::string_view text, size_t& pos, char c) {
	if (pos >= text.size() || text[pos] != c)
		return false;
	++pos;
	return true;
}

/** Parses an optional fraction ".f{1,}" into microseconds. */
bool readFraction(std::string_view text, size_t& pos, uint32_t& microsecond) {
	microsecond = 0;
	if (pos == text.size())
		return true;
	if (!expect(text, pos, '.'))
		return false;
	size_t digits = 0;
	uint32_t value = 0;
	while (pos < text.size()) {
		char c = text[pos];
		if (c < '0' || c > '9')
			return false;
		if (digits < 6)
			value = value * 10 + static_cast<uint32_t>(c - '0');
		++digits;
		++pos;
	}
	if (digits == 0)
		return false;
	for (size_t i = digits; i < 6; ++i)
		value *= 10;
	microsecond = value;
	return true;
}

bool parseTimeOfDay(std::string_view text, size_t& pos, DateTimeValue& value, size_t maxHourDigits) {
	size_t hourDigits = 0;
	while (pos + hourDigits < text.size() && text[pos + hourDigits] >= '0' && text[pos + hourDigits] <= '9')
		++hourDigits;
	if (hourDigits == 0 || hourDigits > maxHourDigits)
		return false;
	return readDigits(text, pos, hourDigits, value.hour) && expect(text, pos, ':') && readDigits(text, pos, 2, value.minute) &&
		expect(text, pos, ':') && readDigits(text, pos, 2, value.second) && readFraction(text, pos, value.microsecond);
}

} // namespace

std::string DateTimeValue::toString(uint32_t decimals) const {
	std::string text;
	switch (kind) {
		case Kind::DATE:
			return fmt::format("{:04}-{:02}-{:02}", year, month, day);
		case Kind::TIME:
			text = fmt::format("{}{:02}:{:02}:{:02}", negative ? "-" : "", hour, minute, second);
			break;
		case Kind::DATETIME:
			text = fmt::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}", year, month, day, hour, minute, second);
			break;
	}
	decimals = std::min(decimals, 6u);
	if (decimals > 0) {
		std::string fraction = fmt::format("{:06}", microsecond % 1000000);
		text += '.';
		text.append(fraction, 0, decimals);
	}
	return text;
}

std::optional<DateTimeValue> DateTimeValue::parse(std::string_view text) {
	DateTimeValue value;
	size_t pos = 0;
	// date or datetime: yyyy-MM-dd
	if (text.size() >= 10 && text[4] == '-') {
		if (!readDigits(text, pos, 4, value.year) || !expect(text, pos, '-') || !readDigits(text, pos, 2, value.month) || !expect(text, pos, '-') ||
			!readDigits(text, pos, 2, value.day))
			return std::nullopt;
		if (pos == text.size()) {
			value.kind = Kind::DATE;
			return value;
		}
		if (text[pos] != ' ' && text[pos] != 'T')
			return std::nullopt;
		++pos;
		value.kind = Kind::DATETIME;
		if (!parseTimeOfDay(text, pos, value, 2) || pos != text.size())
			return std::nullopt;
		return value;
	}
	value.kind = Kind::TIME;
	if (!text.empty() && text[0] == '-') {
		value.negative = true;
		++pos;
	}
	if (!parseTimeOfDay(text, pos, value, 3) || pos != text.size())
		return std::nullopt;
	return value;
}

} // namespace aion::commons::database
