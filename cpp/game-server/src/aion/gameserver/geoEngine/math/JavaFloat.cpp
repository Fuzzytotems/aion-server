#include "aion/gameserver/geoEngine/math/JavaFloat.h"

#include <array>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <string_view>
#include <system_error>

#include "aion/gameserver/geoEngine/math/StrictFp.h"

namespace aion::gameserver::geoEngine::math {

namespace {

/** Splits the output of std::to_chars(scientific) ("d.ddde+XX") into the digits without dot and the decimal exponent of the first digit. */
void splitScientific(std::string_view text, std::string& digits, int32_t& exponent) {
	const size_t e = text.find('e');
	digits.clear();
	for (char c : text.substr(0, e)) {
		if (c != '.')
			digits.push_back(c);
	}
	std::string_view exp = text.substr(e + 1);
	if (!exp.empty() && exp.front() == '+')
		exp.remove_prefix(1);
	exponent = 0;
	std::from_chars(exp.data(), exp.data() + exp.size(), exponent);
}

} // namespace

std::string JavaFloat::toString(float value) {
	if (value != value)
		return "NaN";
	if (value == std::numeric_limits<float>::infinity())
		return "Infinity";
	if (value == -std::numeric_limits<float>::infinity())
		return "-Infinity";
	if (value == 0.0f)
		return std::signbit(value) ? "-0.0" : "0.0";

	const float magnitude = std::fabs(value);
	std::array<char, 64> buffer{};
	// shortest round-trip digits; among several shortest candidates the closest one (std::to_chars guarantees both)
	auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), magnitude, std::chars_format::scientific);
	std::string digits;
	int32_t exponent = 0;
	splitScientific(std::string_view(buffer.data(), static_cast<size_t>(result.ptr - buffer.data())), digits, exponent);
	if (digits.size() == 1) {
		// Java: if one digit suffices, the closest decimal with two digits is chosen (it also rounds to the value)
		result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), magnitude, std::chars_format::scientific, 1);
		splitScientific(std::string_view(buffer.data(), static_cast<size_t>(result.ptr - buffer.data())), digits, exponent);
	}
	while (digits.size() > 1 && digits.back() == '0')
		digits.pop_back();

	std::string out;
	if (value < 0.0f)
		out.push_back('-');
	if (magnitude >= 1e-3 && magnitude < 1e7) {
		if (exponent >= 0) {
			const size_t integerDigits = static_cast<size_t>(exponent) + 1;
			for (size_t i = 0; i < integerDigits; i++)
				out.push_back(i < digits.size() ? digits[i] : '0');
			out.push_back('.');
			if (digits.size() > integerDigits)
				out.append(digits, integerDigits, std::string::npos);
			else
				out.push_back('0');
		} else {
			out.append("0.");
			out.append(static_cast<size_t>(-exponent - 1), '0');
			out.append(digits);
		}
	} else {
		out.push_back(digits[0]);
		out.push_back('.');
		if (digits.size() > 1)
			out.append(digits, 1, std::string::npos);
		else
			out.push_back('0');
		out.push_back('E');
		out.append(std::to_string(exponent));
	}
	return out;
}

} // namespace aion::gameserver::geoEngine::math
