#pragma once

#include <concepts>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_set>
#include <vector>

#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"

namespace aion::gameserver::xml {

/**
 * A lexical value that cannot be converted (malformed number, out of range, unknown enum constant, ...). Thrown without location by the free
 * converters below; BindContext rethrows it as StaticDataException with file:line:col and the element path.
 */
class XmlValueException : public commons::utils::IllegalArgumentException {
public:
	using IllegalArgumentException::IllegalArgumentException;
};

/**
 * Value converters with JAXB (DatatypeConverterImpl) semantics where cheap (docs/design/static-data.md §2.4). Where C++ is stricter than
 * JAXB, the lexical census (V4) proves the data never reaches the difference:
 * - XML whitespace is space, tab, CR and LF (WhiteSpaceProcessor.isWhiteSpace).
 * - Integers: surrounding XML whitespace is trimmed, then an optional single '+' or '-' and at least one ASCII decimal digit. Out of range is
 *   an error (JAXB: _parseInt accumulates with wrap-around and narrows byte/short silently; it also skips whitespace and signs anywhere).
 *   An empty value is an error (JAXB: 0).
 * - float/double: trimmed; "NaN", "INF", "-INF" as in _parseFloat; otherwise an optional sign, decimal digits with optional '.', optional
 *   exponent, correctly rounded (std::from_chars, like Float.parseFloat). Java-only forms (hex, "1f", "Infinity") and values that overflow
 *   or underflow the type are errors.
 * - boolean: trimmed "true", "false", "1", "0" (_parseBoolean); anything else is an error.
 * - enums: trimmed (JAXB's token enum leaf), then EnumTraits<E>::xmlSorted; an unknown constant is an error.
 * - strings: unchanged. Attribute values arrive already normalized by the parser (tab/CR/LF become spaces), element text is concatenated
 *   PCDATA/CDATA exactly like JAXB's String leaf.
 * - @XmlList and collection-typed attributes: split on runs of XML whitespace, leading/trailing whitespace ignored, "" gives an empty list.
 */
constexpr bool isXmlWhitespace(char c) noexcept {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/** Removes leading and trailing XML whitespace. */
std::string_view trimXml(std::string_view value) noexcept;

int8_t parseInt8(std::string_view value);
int16_t parseInt16(std::string_view value);
int32_t parseInt32(std::string_view value);
int64_t parseInt64(std::string_view value);
float parseFloat(std::string_view value);
double parseDouble(std::string_view value);
bool parseBool(std::string_view value);

/** Tokens of an @XmlList value (views into `value`). */
std::vector<std::string_view> splitXmlList(std::string_view value);

/** Java: E.valueOf with XML lexical forms. @throws XmlValueException "Unknown <javaName> constant 'X'" */
template <XmlEnum E>
E parseEnum(std::string_view value) {
	std::string_view trimmed = trimXml(value);
	if (std::optional<E> constant = enumFromXml<E>(trimmed))
		return *constant;
	throw XmlValueException("Unknown " + std::string(EnumTraits<E>::javaName) + " constant '" + std::string(value) + "'");
}

/** Scalar types with a lexical converter: Java int/long/short/byte/float/double/boolean, String and generated enums. */
template <class T>
concept XmlScalar = std::same_as<T, int8_t> || std::same_as<T, int16_t> || std::same_as<T, int32_t> || std::same_as<T, int64_t> ||
                    std::same_as<T, float> || std::same_as<T, double> || std::same_as<T, bool> || std::same_as<T, std::string> || XmlEnum<T>;

/** Converts one lexical value to a scalar type (see the rules above). */
template <XmlScalar T>
T parseValue(std::string_view value) {
	if constexpr (std::same_as<T, int8_t>)
		return parseInt8(value);
	else if constexpr (std::same_as<T, int16_t>)
		return parseInt16(value);
	else if constexpr (std::same_as<T, int32_t>)
		return parseInt32(value);
	else if constexpr (std::same_as<T, int64_t>)
		return parseInt64(value);
	else if constexpr (std::same_as<T, float>)
		return parseFloat(value);
	else if constexpr (std::same_as<T, double>)
		return parseDouble(value);
	else if constexpr (std::same_as<T, bool>)
		return parseBool(value);
	else if constexpr (std::same_as<T, std::string>)
		return std::string(value);
	else
		return parseEnum<T>(value);
}

namespace detail {
template <class T>
struct IsOptional : std::false_type {};
template <class T>
struct IsOptional<std::optional<T>> : std::true_type {};
template <class T>
struct IsVector : std::false_type {};
template <class T, class A>
struct IsVector<std::vector<T, A>> : std::true_type {};
template <class T>
struct IsUnorderedSet : std::false_type {};
template <class T, class H, class Q, class A>
struct IsUnorderedSet<std::unordered_set<T, H, Q, A>> : std::true_type {};
} // namespace detail

/** std::optional<T> */
template <class T>
concept Optional = detail::IsOptional<T>::value;

/** std::vector of a scalar or std::unordered_set of a scalar: the containers of @XmlList and attribute collections (Java List/array/Set). */
template <class C>
concept XmlScalarCollection = (detail::IsVector<C>::value || detail::IsUnorderedSet<C>::value) && XmlScalar<typename C::value_type>;

/**
 * Converts an @XmlList / collection attribute value into a container. `C` is std::vector<T> or std::unordered_set<T> (JAXB uses HashSet for
 * Set), or std::optional of either (present-empty yields an engaged empty container, docs/design/static-data.md §2.4).
 */
template <class C>
  requires XmlScalarCollection<C> || (Optional<C> && XmlScalarCollection<typename C::value_type>)
C parseList(std::string_view value) {
	if constexpr (Optional<C>) {
		return C(parseList<typename C::value_type>(value));
	} else {
		C result;
		std::vector<std::string_view> tokens = splitXmlList(value);
		if constexpr (detail::IsVector<C>::value)
			result.reserve(tokens.size());
		for (std::string_view token : tokens)
			result.insert(result.end(), parseValue<typename C::value_type>(token));
		return result;
	}
}

} // namespace aion::gameserver::xml
