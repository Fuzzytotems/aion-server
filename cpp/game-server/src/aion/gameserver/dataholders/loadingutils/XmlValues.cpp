#include "aion/gameserver/dataholders/loadingutils/XmlValues.h"

#include <charconv>
#include <cmath>
#include <limits>
#include <system_error>

namespace aion::gameserver::xml {

namespace {

[[noreturn]] void notANumber(std::string_view value, std::string_view type) {
	throw XmlValueException("Not a valid " + std::string(type) + " value: '" + std::string(value) + "'");
}

/** trimmed [+-]?[0-9]+ in [minimum, maximum] */
int64_t parseInteger(std::string_view value, int64_t minimum, int64_t maximum, std::string_view type) {
	std::string_view s = trimXml(value);
	size_t pos = 0;
	bool negative = false;
	if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) {
		negative = s[pos] == '-';
		++pos;
	}
	if (pos == s.size())
		notANumber(value, type);
	// accumulate as a negative number, so the minimum of int64 is representable
	int64_t result = 0;
	const int64_t limit = negative ? minimum : -maximum;
	for (; pos < s.size(); ++pos) {
		char c = s[pos];
		if (c < '0' || c > '9')
			notANumber(value, type);
		int digit = c - '0';
		if (result < (limit + digit) / 10)
			throw XmlValueException("Value out of range for " + std::string(type) + ": '" + std::string(value) + "'");
		result = result * 10 - digit;
	}
	if (!negative)
		result = -result;
	return result;
}

template <class F>
F parseFloating(std::string_view value, std::string_view type) {
	std::string_view s = trimXml(value);
	// DatatypeConverterImpl._parseFloat / _parseDouble special values
	if (s == "NaN")
		return std::numeric_limits<F>::quiet_NaN();
	if (s == "INF")
		return std::numeric_limits<F>::infinity();
	if (s == "-INF")
		return -std::numeric_limits<F>::infinity();
	bool negative = false;
	std::string_view digits = s;
	if (!digits.empty() && (digits.front() == '+' || digits.front() == '-')) {
		negative = digits.front() == '-';
		digits.remove_prefix(1);
	}
	// from_chars accepts "inf"/"nan" spellings and no '+': only digits, '.', exponents are allowed here
	if (digits.empty())
		notANumber(value, type);
	bool mantissaDigit = false;
	for (size_t i = 0; i < digits.size(); ++i) {
		char c = digits[i];
		if (c >= '0' && c <= '9') {
			mantissaDigit = true;
		} else if (c == '.') {
		} else if (c == 'e' || c == 'E') {
			if (!mantissaDigit)
				notANumber(value, type);
			size_t j = i + 1;
			if (j < digits.size() && (digits[j] == '+' || digits[j] == '-'))
				++j;
			if (j == digits.size())
				notANumber(value, type);
			for (; j < digits.size(); ++j) {
				if (digits[j] < '0' || digits[j] > '9')
					notANumber(value, type);
			}
			break;
		} else {
			notANumber(value, type);
		}
	}
	if (!mantissaDigit)
		notANumber(value, type);
	F result{};
	auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), result, std::chars_format::general);
	if (error == std::errc::result_out_of_range)
		throw XmlValueException("Value out of range for " + std::string(type) + ": '" + std::string(value) + "'");
	if (error != std::errc() || end != digits.data() + digits.size())
		notANumber(value, type);
	return negative ? -result : result;
}

} // namespace

std::string_view trimXml(std::string_view value) noexcept {
	size_t begin = 0;
	size_t end = value.size();
	while (begin < end && isXmlWhitespace(value[begin]))
		++begin;
	while (end > begin && isXmlWhitespace(value[end - 1]))
		--end;
	return value.substr(begin, end - begin);
}

int8_t parseInt8(std::string_view value) {
	return static_cast<int8_t>(parseInteger(value, std::numeric_limits<int8_t>::min(), std::numeric_limits<int8_t>::max(), "byte"));
}

int16_t parseInt16(std::string_view value) {
	return static_cast<int16_t>(parseInteger(value, std::numeric_limits<int16_t>::min(), std::numeric_limits<int16_t>::max(), "short"));
}

int32_t parseInt32(std::string_view value) {
	return static_cast<int32_t>(parseInteger(value, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max(), "int"));
}

int64_t parseInt64(std::string_view value) {
	return parseInteger(value, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max(), "long");
}

float parseFloat(std::string_view value) {
	return parseFloating<float>(value, "float");
}

double parseDouble(std::string_view value) {
	return parseFloating<double>(value, "double");
}

bool parseBool(std::string_view value) {
	std::string_view s = trimXml(value);
	if (s == "true" || s == "1")
		return true;
	if (s == "false" || s == "0")
		return false;
	throw XmlValueException("Not a valid boolean value: '" + std::string(value) + "'");
}

std::vector<std::string_view> splitXmlList(std::string_view value) {
	std::vector<std::string_view> tokens;
	size_t pos = 0;
	while (pos < value.size()) {
		while (pos < value.size() && isXmlWhitespace(value[pos]))
			++pos;
		size_t start = pos;
		while (pos < value.size() && !isXmlWhitespace(value[pos]))
			++pos;
		if (pos > start)
			tokens.push_back(value.substr(start, pos - start));
	}
	return tokens;
}

} // namespace aion::gameserver::xml
