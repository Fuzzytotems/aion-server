#include "aion/commons/utils/Numbers.h"

#include <limits>
#include <string>

#include <fmt/format.h>

#include "aion/commons/utils/Exception.h"

namespace aion::commons::utils {

namespace {

constexpr int32_t MIN_RADIX = 2;
constexpr int32_t MAX_RADIX = 36;

/** Java: NumberFormatException.forInputString */
[[noreturn]] void throwForInputString(std::string_view s, int32_t radix) {
	throw NumberFormatException(fmt::format("For input string: \"{}\"{}", s, radix == 10 ? "" : fmt::format(" under radix {}", radix)));
}

int32_t digit(char c, int32_t radix) noexcept {
	int32_t value = -1;
	if (c >= '0' && c <= '9')
		value = c - '0';
	else if (c >= 'a' && c <= 'z')
		value = c - 'a' + 10;
	else if (c >= 'A' && c <= 'Z')
		value = c - 'A' + 10;
	return value < radix ? value : -1;
}

/** Java: Long.parseLong - accumulates negatively, so the most negative value can be parsed without overflow */
template <typename T>
T parse(std::string_view s, int32_t radix) {
	if (radix < MIN_RADIX)
		throw NumberFormatException(fmt::format("radix {} less than Character.MIN_RADIX", radix));
	if (radix > MAX_RADIX)
		throw NumberFormatException(fmt::format("radix {} greater than Character.MAX_RADIX", radix));
	if (s.empty())
		throwForInputString(s, radix);
	bool negative = false;
	size_t i = 0;
	T limit = -std::numeric_limits<T>::max();
	if (s[0] < '0') { // possible leading "+" or "-"
		if (s[0] == '-') {
			negative = true;
			limit = std::numeric_limits<T>::min();
		} else if (s[0] != '+') {
			throwForInputString(s, radix);
		}
		if (s.size() == 1) // cannot have lone "+" or "-"
			throwForInputString(s, radix);
		i++;
	}
	const T multmin = limit / radix;
	T result = 0;
	for (; i < s.size(); i++) {
		int32_t d = digit(s[i], radix);
		if (d < 0 || result < multmin)
			throwForInputString(s, radix);
		result *= static_cast<T>(radix);
		if (result < limit + d)
			throwForInputString(s, radix);
		result -= static_cast<T>(d);
	}
	return negative ? result : -result;
}

} // namespace

int32_t parseInt(std::string_view s, int32_t radix) {
	return parse<int32_t>(s, radix);
}

int64_t parseLong(std::string_view s, int32_t radix) {
	return parse<int64_t>(s, radix);
}

} // namespace aion::commons::utils
