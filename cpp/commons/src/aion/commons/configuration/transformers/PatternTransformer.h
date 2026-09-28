#pragma once

#include <regex>
#include <string>
#include <string_view>

#include "aion/commons/configuration/transformers/PropertyTransformer.h"

namespace aion::commons::configuration::transformers {

namespace PatternTransformer {

/** Converts UTF-8 to the wchar_t encoding (UTF-16 on Windows like Java's strings, UTF-32 elsewhere). */
std::wstring toWide(std::string_view utf8);

} // namespace PatternTransformer

/**
 * Automatic pattern transformer for RegExp resolving (Java: Pattern.compile(value)), compiled with the ECMAScript grammar.
 * <p>
 * Java returns null for an empty value: bind a std::optional&lt;std::regex&gt; (or std::optional&lt;std::wregex&gt;) for that (see
 * OptionalTransformer.h). Deviation: for a plain std::regex field, an empty value is an error.
 * <p>
 * Differences to java.util.regex.Pattern to keep in mind when porting code that uses the pattern:
 * <ul>
 * <li>ECMAScript lacks possessive quantifiers, atomic groups, lookbehind, \\p{...} classes, inline flags like (?i), \\A, \\Z and \\z.</li>
 * <li>Java's Matcher.matches() is std::regex_match, Matcher.find() is std::regex_search.</li>
 * <li>std::regex works on bytes: '.' and character classes see single UTF-8 bytes, so "{1,32}" counts bytes, not characters. Use std::wregex
 * (whose input is UTF-16 on Windows, like Java) for patterns that are matched against non-ASCII text such as player or legion names.</li>
 * </ul>
 * Java: com.aionemu.commons.configuration.transformers.PatternTransformer
 *
 * @author SoulKeeper
 */
template <>
struct PropertyTransformer<std::regex> {
	static std::string typeName() { return "Pattern"; }

	static std::regex parseObject(std::string_view value);
};

/** Like PropertyTransformer&lt;std::regex&gt;, but for wide character input (the pattern is converted from UTF-8). */
template <>
struct PropertyTransformer<std::wregex> {
	static std::string typeName() { return "Pattern"; }

	static std::wregex parseObject(std::string_view value);
};

} // namespace aion::commons::configuration::transformers
