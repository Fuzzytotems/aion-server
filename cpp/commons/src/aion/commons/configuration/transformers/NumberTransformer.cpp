#include "aion/commons/configuration/transformers/NumberTransformer.h"

#include <charconv>
#include <cmath>
#include <system_error>

#include <fmt/format.h>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/StringUtils.h"

namespace aion::commons::configuration::transformers::NumberParser {

namespace {


/** Java: NumberFormatException.forInputString(s, radix) */
[[noreturn]] void throwForInputString(std::string_view s, int radix) {
	throw utils::NumberFormatException(radix == 10 ? fmt::format("For input string: \"{}\"", s)
	                                           : fmt::format("For input string: \"{}\" under radix {}", s, radix));
}

int digitValue(char c, int radix) noexcept {
	int value;
	if (c >= '0' && c <= '9')
		value = c - '0';
	else if (c >= 'a' && c <= 'z')
		value = 10 + (c - 'a');
	else if (c >= 'A' && c <= 'Z')
		value = 10 + (c - 'A');
	else
		return -1;
	return value < radix ? value : -1;
}

struct SignedMagnitude {
	bool negative;
	uint64_t magnitude;
};

/**
 * Java: Integer.parseInt(s, radix) / Long.parseLong(s, radix), generalized to arbitrary limits for the magnitude of positive and negative
 * values. Throws with Java's message if the syntax is invalid or the value is out of range.
 */
SignedMagnitude parseSigned(std::string_view s, int radix, uint64_t positiveLimit, uint64_t negativeLimit) {
	if (s.empty())
		throwForInputString(s, radix);
	std::size_t i = 0;
	bool negative = false;
	if (s[0] == '-' || s[0] == '+') {
		negative = s[0] == '-';
		if (s.size() == 1) // cannot have lone "+" or "-"
			throwForInputString(s, radix);
		i = 1;
	}
	uint64_t limit = negative ? negativeLimit : positiveLimit;
	uint64_t result = 0;
	for (; i < s.size(); i++) {
		int digit = digitValue(s[i], radix);
		if (digit < 0 || result > (limit - static_cast<uint64_t>(digit)) / static_cast<uint64_t>(radix))
			throwForInputString(s, radix);
		result = result * static_cast<uint64_t>(radix) + static_cast<uint64_t>(digit);
	}
	return {negative, result};
}

/** Java: Integer.decode / Long.decode, generalized to arbitrary limits (see parseSigned). */
SignedMagnitude decode(std::string_view nm, uint64_t positiveLimit, uint64_t negativeLimit) {
	if (nm.empty())
		throw utils::NumberFormatException("Zero length string");
	std::size_t index = 0;
	bool negative = false;
	int radix = 10;
	// handle sign, if present
	if (nm[0] == '-') {
		negative = true;
		index++;
	} else if (nm[0] == '+') {
		index++;
	}
	// handle radix specifier, if present
	std::string_view rest = nm.substr(index);
	if (rest.starts_with("0x") || rest.starts_with("0X")) {
		index += 2;
		radix = 16;
	} else if (rest.starts_with('#')) {
		index++;
		radix = 16;
	} else if (rest.starts_with('0') && nm.size() > 1 + index) {
		index++;
		radix = 8;
	}
	rest = nm.substr(index);
	if (rest.starts_with('-') || rest.starts_with('+'))
		throw utils::NumberFormatException("Sign character in wrong position");
	// Java first parses the unsigned digits and, if that fails (e.g. for MIN_VALUE), parses "-" + digits again, whose result or error is final.
	// Parsing the signed constant directly is equivalent.
	std::string constant = negative ? "-" + std::string(rest) : std::string(rest);
	return parseSigned(constant, radix, positiveLimit, negativeLimit);
}

int64_t toSigned(SignedMagnitude value) noexcept {
	// conversion of the unsigned two's complement is well-defined modular arithmetic since C++20 (covers the magnitude of MIN_VALUE)
	return value.negative ? static_cast<int64_t>(0 - value.magnitude) : static_cast<int64_t>(value.magnitude);
}

/** Java: FloatingDecimal.readJavaFormatString and parseHexString, followed by correctly rounded conversion via std::from_chars. */
template <std::floating_point F>
F parseFloating(std::string_view original) {
	std::string_view in = utils::StringUtils::trim(original); // don't fool around with white space
	if (in.empty())
		throw utils::NumberFormatException("empty String");
	auto fail = [&]() -> F { throw utils::NumberFormatException(fmt::format("For input string: \"{}\"", in)); };

	std::size_t i = 0;
	bool negative = false;
	if (in[0] == '-' || in[0] == '+') {
		negative = in[0] == '-';
		i = 1;
	}
	if (i >= in.size())
		return fail();
	std::string_view body = in.substr(i);
	F sign = negative ? F(-1) : F(1);
	if (body[0] == 'N')
		return body == "NaN" ? std::numeric_limits<F>::quiet_NaN() : fail();
	if (body[0] == 'I')
		return body == "Infinity" ? sign * std::numeric_limits<F>::infinity() : fail();

	bool hex = body.size() > 1 && body[0] == '0' && (body[1] == 'x' || body[1] == 'X');
	auto isDigit = [&](char c) { return hex ? digitValue(c, 16) >= 0 : (c >= '0' && c <= '9'); };
	std::size_t p = hex ? 2 : 0;

	// mantissa: digits with at most one point, at least one digit
	std::size_t mantissaStart = p;
	std::size_t digitsBeforePoint = 0;
	std::size_t digitCount = 0;
	std::size_t firstNonZeroDigit = std::string_view::npos; // index among the digits
	bool pointSeen = false;
	for (; p < body.size(); p++) {
		char c = body[p];
		if (c == '.') {
			if (pointSeen) {
				if (hex)
					return fail();
				throw utils::NumberFormatException("multiple points");
			}
			pointSeen = true;
		} else if (isDigit(c)) {
			if (c != '0' && firstNonZeroDigit == std::string_view::npos)
				firstNonZeroDigit = digitCount;
			digitCount++;
			if (!pointSeen)
				digitsBeforePoint++;
		} else {
			break;
		}
	}
	if (digitCount == 0)
		return fail();

	// exponent: mandatory for hex ([pP]), optional for decimal ([eE]); sign and at least one decimal digit
	int64_t exponent = 0;
	bool hasExponent = p < body.size() && (hex ? (body[p] == 'p' || body[p] == 'P') : (body[p] == 'e' || body[p] == 'E'));
	if (hex && !hasExponent)
		return fail();
	if (hasExponent) {
		p++;
		bool negativeExponent = false;
		if (p < body.size() && (body[p] == '-' || body[p] == '+')) {
			negativeExponent = body[p] == '-';
			p++;
		}
		std::size_t exponentDigitsStart = p;
		for (; p < body.size() && body[p] >= '0' && body[p] <= '9'; p++) {
			if (exponent < 1'000'000'000) // saturate, the value is infinite or zero anyway
				exponent = exponent * 10 + (body[p] - '0');
		}
		if (p == exponentDigitsStart)
			return fail();
		if (negativeExponent)
			exponent = -exponent;
	}
	std::size_t numberEnd = p;

	// optional type suffix, then the end
	if (p < body.size() && !(p == body.size() - 1 && (body[p] == 'f' || body[p] == 'F' || body[p] == 'd' || body[p] == 'D')))
		return fail();

	if (firstNonZeroDigit == std::string_view::npos)
		return sign * F(0);

	std::string_view number = body.substr(mantissaStart, numberEnd - mantissaStart);
	F result{};
	auto [ptr, ec] = std::from_chars(number.data(), number.data() + number.size(), result, hex ? std::chars_format::hex : std::chars_format::general);
	if (ec == std::errc::result_out_of_range) {
		// decide between overflow and underflow by the position of the most significant digit
		int64_t significantDigitExponent = static_cast<int64_t>(digitsBeforePoint) - static_cast<int64_t>(firstNonZeroDigit) - 1;
		int64_t magnitude = hex ? significantDigitExponent * 4 + exponent : significantDigitExponent + exponent;
		return sign * (magnitude >= 0 ? std::numeric_limits<F>::infinity() : F(0));
	}
	if (ec != std::errc() || ptr != number.data() + number.size())
		return fail(); // not expected after validation
	return sign * result;
}

} // namespace

int32_t decodeInt(std::string_view nm) {
	return static_cast<int32_t>(toSigned(decode(nm, INT32_MAX, uint64_t{1} << 31)));
}

int64_t decodeLong(std::string_view nm) {
	return toSigned(decode(nm, INT64_MAX, uint64_t{1} << 63));
}

int16_t decodeShort(std::string_view nm) {
	int32_t i = decodeInt(nm);
	if (i < INT16_MIN || i > INT16_MAX)
		throw utils::NumberFormatException(fmt::format("Value {} out of range from input {}", i, nm));
	return static_cast<int16_t>(i);
}

int8_t decodeByte(std::string_view nm) {
	int32_t i = decodeInt(nm);
	if (i < INT8_MIN || i > INT8_MAX)
		throw utils::NumberFormatException(fmt::format("Value {} out of range from input {}", i, nm));
	return static_cast<int8_t>(i);
}

uint64_t decodeUnsigned(std::string_view nm, uint64_t max) {
	SignedMagnitude value = decode(nm, UINT64_MAX, UINT64_MAX);
	if ((value.negative && value.magnitude != 0) || value.magnitude > max)
		throw utils::NumberFormatException(fmt::format("Value {}{} out of range from input {}", value.negative ? "-" : "", value.magnitude, nm));
	return value.magnitude;
}

double parseDouble(std::string_view value) {
	return parseFloating<double>(value);
}

float parseFloat(std::string_view value) {
	return parseFloating<float>(value);
}

} // namespace aion::commons::configuration::transformers::NumberParser
