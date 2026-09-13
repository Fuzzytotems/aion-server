#pragma once

#include <concepts>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>

#include "aion/commons/configuration/transformers/PropertyTransformer.h"

namespace aion::commons::configuration::transformers {

/**
 * Java number parsing functions with their exact acceptance rules. They throw utils::NumberFormatException (an IllegalArgumentException) with the message of Java's
 * NumberFormatException (e.g. "For input string: \"abc\""). These are the building blocks of the number transformers, but usable on their own.
 * <p>
 * Deviation: Java's integer parsing uses Character.digit, which also accepts non-ASCII digits (e.g. fullwidth or Arabic-Indic digits); only ASCII
 * digits and letters are accepted here.
 */
namespace NumberParser {

/**
 * Java: Integer.decode. Accepts an optional sign followed by a decimal number, a hexadecimal number ("0x", "0X" or "#" prefix) or an octal number
 * (leading "0"). No whitespace is allowed.
 */
int32_t decodeInt(std::string_view nm);

/** Java: Long.decode (see decodeInt) */
int64_t decodeLong(std::string_view nm);

/** Java: Short.decode (decodeInt with a range check: "Value 40000 out of range from input 40000") */
int16_t decodeShort(std::string_view nm);

/** Java: Byte.decode (decodeInt with a range check) */
int8_t decodeByte(std::string_view nm);

/**
 * Not in Java: decodeLong syntax for unsigned targets. "-0" is accepted, any other negative value fails with "Value -1 out of range from input -1",
 * like values above max.
 */
uint64_t decodeUnsigned(std::string_view nm, uint64_t max);

/**
 * Java: Double.valueOf(String). Leading and trailing characters &lt;= ' ' are ignored. Accepts "NaN", "Infinity" (with optional sign), decimal
 * numbers with optional fraction and exponent, hexadecimal floating point numbers ("0x1.8p3"), each with an optional f/F/d/D suffix. Values are
 * correctly rounded; overflow yields infinity, underflow zero.
 */
double parseDouble(std::string_view value);

/** Java: Float.valueOf(String), see parseDouble (the decimal value is rounded to float directly, not via double) */
float parseFloat(std::string_view value);

} // namespace NumberParser

/** Integer types that are parsed as numbers (char types and bool have their own transformers). */
template <typename T>
concept TransformableInteger = std::integral<T> && !std::same_as<T, bool> && !std::same_as<T, char> && !std::same_as<T, wchar_t> &&
                               !std::same_as<T, char8_t> && !std::same_as<T, char16_t> && !std::same_as<T, char32_t>;

/**
 * Parses integers with Java's decode semantics: int8_t = Byte.decode, int16_t = Short.decode, int32_t = Integer.decode, int64_t = Long.decode.
 * Unsigned types (not in Java) use the same syntax with a range check.
 * <p>
 * Java: com.aionemu.commons.configuration.transformers.NumberTransformer
 *
 * @author Neon
 */
template <TransformableInteger T>
struct PropertyTransformer<T> {
	static std::string typeName() {
		if constexpr (std::is_unsigned_v<T>)
			return "uint" + std::to_string(sizeof(T) * 8) + "_t";
		else if constexpr (sizeof(T) == 1)
			return "byte";
		else if constexpr (sizeof(T) == 2)
			return "short";
		else if constexpr (sizeof(T) == 4)
			return "int";
		else
			return "long";
	}

	static T parseObject(std::string_view value) {
		if constexpr (std::is_unsigned_v<T>)
			return static_cast<T>(NumberParser::decodeUnsigned(value, std::numeric_limits<T>::max()));
		else if constexpr (sizeof(T) == 1)
			return static_cast<T>(NumberParser::decodeByte(value));
		else if constexpr (sizeof(T) == 2)
			return static_cast<T>(NumberParser::decodeShort(value));
		else if constexpr (sizeof(T) == 4)
			return static_cast<T>(NumberParser::decodeInt(value));
		else
			return static_cast<T>(NumberParser::decodeLong(value));
	}
};

/** Java: NumberTransformer for float (Float.valueOf) */
template <>
struct PropertyTransformer<float> {
	static std::string typeName() { return "float"; }
	static float parseObject(std::string_view value) { return NumberParser::parseFloat(value); }
};

/** Java: NumberTransformer for double (Double.valueOf) */
template <>
struct PropertyTransformer<double> {
	static std::string typeName() { return "double"; }
	static double parseObject(std::string_view value) { return NumberParser::parseDouble(value); }
};

} // namespace aion::commons::configuration::transformers
