#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace aion::commons::utils::StringUtils {

/**
 * Converts UTF-8 to UTF-16. Malformed input is replaced with U+FFFD like Java's String decoding (new String(bytes, UTF_8)): one replacement
 * character per malformed subsequence (e.g. a truncated multi-byte sequence or an encoded surrogate), and a truncated sequence at the end of
 * the input ends the text.
 */
std::u16string toUtf16(std::string_view utf8);

/**
 * Converts UTF-16 to UTF-8. Unpaired surrogates are replaced with U+FFFD (Java encodes them as '?'; the replacement char is safer for display).
 */
std::string toUtf8(std::u16string_view utf16);

/**
 * @return the length of the string in UTF-16 code units, i.e. what Java's String.length() would return for the same text.
 */
int32_t utf16Length(std::string_view utf8);

/**
 * Java: Character.toLowerCase(int) / Character.toUpperCase(int).
 * <p>
 * Deviation: simple case mapping of the scripts the servers support in names (ASCII, Latin-1 Supplement, Latin Extended-A, Greek, Cyrillic);
 * other code points are returned unchanged.
 */
char32_t toLowerCase(char32_t codePoint) noexcept;
char32_t toUpperCase(char32_t codePoint) noexcept;

/**
 * Java: String.equalsIgnoreCase - compares the UTF-16 lengths and then each character after toUpperCase, and after toLowerCase of that (see
 * the case mapping deviation of toLowerCase(char32_t)).
 */
bool equalsIgnoreCase(std::string_view a, std::string_view b) noexcept;

/**
 * Java: String.toLowerCase(Locale.ROOT) / String.toUpperCase(Locale.ROOT), including the special cases U+0130 -&gt; "i\u0307" and
 * U+00DF -&gt; "SS". Deviation: see toLowerCase(char32_t); the context-dependent final sigma rule is not applied.
 */
std::string toLowerCase(std::string_view s);
std::string toUpperCase(std::string_view s);

/**
 * Java: String.substring(beginIndex, endIndex) with indexes in UTF-16 code units. A surrogate pair cut in half becomes U+FFFD (see toUtf8).
 *
 * @throws IndexOutOfBoundsException if beginIndex &lt; 0, endIndex &gt; utf16Length(s) or beginIndex &gt; endIndex
 */
std::string substring(std::string_view s, int32_t beginIndex, int32_t endIndex);

/** Java: String.substring(beginIndex) with an index in UTF-16 code units, see substring(s, beginIndex, endIndex). */
std::string substring(std::string_view s, int32_t beginIndex);

/** Java: String.trim() - removes all leading and trailing characters <= ' '. */
std::string_view trim(std::string_view s) noexcept;

/** Java: String.strip()/isBlank() use Unicode whitespace; this treats ASCII whitespace (space, \t, \n, \v, \f, \r) as blank. */
std::string_view strip(std::string_view s) noexcept;
bool isBlank(std::string_view s) noexcept;

/**
 * Splits by a literal delimiter. Unlike Java's String.split(regex), trailing empty strings are kept; use splitJava for exact String.split
 * semantics.
 */
std::vector<std::string> split(std::string_view s, std::string_view delimiter);

/**
 * Exact semantics of Java's String.split(literal) for a non-regex delimiter: trailing empty strings are removed, and an empty input yields [""].
 */
std::vector<std::string> splitJava(std::string_view s, std::string_view delimiter);

std::string join(const std::vector<std::string>& parts, std::string_view delimiter);

/** Replaces all occurrences of a literal (Java: String.replace(CharSequence, CharSequence)). */
std::string replace(std::string_view s, std::string_view target, std::string_view replacement);

} // namespace aion::commons::utils::StringUtils
