#include "aion/commons/utils/StringUtils.h"

#include <algorithm>

#include <fmt/format.h>

#include "aion/commons/utils/Exception.h"

namespace aion::commons::utils::StringUtils {

namespace {

constexpr char32_t REPLACEMENT_CHAR = 0xFFFD;

bool isNotContinuation(uint8_t b) noexcept {
	return (b & 0xC0) != 0x80;
}

/** Java: StringCoding/String.malformed3 - the length of the malformed subsequence starting at a 3-byte lead byte */
size_t malformed3(uint8_t b1, uint8_t b2) noexcept {
	return ((b1 == 0xE0 && (b2 & 0xE0) == 0x80) || isNotContinuation(b2)) ? 1 : 2;
}

bool isMalformed3_2(uint8_t b1, uint8_t b2) noexcept {
	return (b1 == 0xE0 && (b2 & 0xE0) == 0x80) || isNotContinuation(b2);
}

/** Java: String.malformed4 - the length of the malformed subsequence starting at a 4-byte lead byte */
size_t malformed4(uint8_t b1, uint8_t b2, uint8_t b3) noexcept {
	if (b1 > 0xF4 || (b1 == 0xF0 && (b2 < 0x90 || b2 > 0xBF)) || (b1 == 0xF4 && (b2 & 0xF0) != 0x80) || isNotContinuation(b2))
		return 1;
	return isNotContinuation(b3) ? 2 : 3;
}

bool isMalformed4_2(uint8_t b1, uint8_t b2) noexcept {
	return (b1 == 0xF0 && (b2 < 0x90 || b2 > 0xBF)) || (b1 == 0xF4 && (b2 & 0xF0) != 0x80) || isNotContinuation(b2);
}

/**
 * Java: String.decodeUTF8_UTF16 - calls emit for each decoded code point, U+FFFD for each malformed subsequence. Surrogate code points and
 * overlong forms are malformed. A truncated sequence at the end of the input produces one U+FFFD and ends decoding (like Java).
 */
template <typename Emit>
void decodeUtf8(std::string_view s, Emit&& emit) {
	auto byteAt = [&](size_t index) { return static_cast<uint8_t>(s[index]); };
	size_t sp = 0;
	const size_t sl = s.size();
	while (sp < sl) {
		uint8_t b1 = byteAt(sp++);
		if (b1 < 0x80) {
			emit(b1);
		} else if ((b1 & 0xE0) == 0xC0 && (b1 & 0x1E) != 0) { // C2..DF
			if (sp < sl) {
				uint8_t b2 = byteAt(sp++);
				if (isNotContinuation(b2)) {
					emit(REPLACEMENT_CHAR);
					sp--;
				} else {
					emit(((b1 & 0x1Fu) << 6) | (b2 & 0x3Fu));
				}
				continue;
			}
			emit(REPLACEMENT_CHAR);
			break;
		} else if ((b1 & 0xF0) == 0xE0) {
			if (sp + 1 < sl) {
				uint8_t b2 = byteAt(sp++);
				uint8_t b3 = byteAt(sp++);
				if (isMalformed3_2(b1, b2) || isNotContinuation(b3)) {
					emit(REPLACEMENT_CHAR);
					sp = sp - 3 + malformed3(b1, b2);
				} else {
					char32_t c = ((b1 & 0x0Fu) << 12) | ((b2 & 0x3Fu) << 6) | (b3 & 0x3Fu);
					emit(c >= 0xD800 && c <= 0xDFFF ? REPLACEMENT_CHAR : c);
				}
				continue;
			}
			if (sp < sl && isMalformed3_2(b1, byteAt(sp))) {
				emit(REPLACEMENT_CHAR);
				continue;
			}
			emit(REPLACEMENT_CHAR);
			break;
		} else if ((b1 & 0xF8) == 0xF0) {
			if (sp + 2 < sl) {
				uint8_t b2 = byteAt(sp++);
				uint8_t b3 = byteAt(sp++);
				uint8_t b4 = byteAt(sp++);
				char32_t uc = ((b1 & 0x07u) << 18) | ((b2 & 0x3Fu) << 12) | ((b3 & 0x3Fu) << 6) | (b4 & 0x3Fu);
				if (isNotContinuation(b2) || isNotContinuation(b3) || isNotContinuation(b4) || uc < 0x10000 || uc > 0x10FFFF) {
					emit(REPLACEMENT_CHAR);
					sp = sp - 4 + malformed4(b1, b2, b3);
				} else {
					emit(uc);
				}
				continue;
			}
			if (b1 > 0xF4 || (sp < sl && isMalformed4_2(b1, byteAt(sp)))) {
				emit(REPLACEMENT_CHAR);
				continue;
			}
			sp++;
			emit(REPLACEMENT_CHAR);
			if (sp < sl && isNotContinuation(byteAt(sp)))
				continue;
			break;
		} else {
			emit(REPLACEMENT_CHAR);
		}
	}
}

void encodeUtf8(std::string& out, char32_t cp) {
	if (cp < 0x80) {
		out += static_cast<char>(cp);
	} else if (cp < 0x800) {
		out += static_cast<char>(0xC0 | (cp >> 6));
		out += static_cast<char>(0x80 | (cp & 0x3F));
	} else if (cp < 0x10000) {
		out += static_cast<char>(0xE0 | (cp >> 12));
		out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
		out += static_cast<char>(0x80 | (cp & 0x3F));
	} else {
		out += static_cast<char>(0xF0 | (cp >> 18));
		out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
		out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
		out += static_cast<char>(0x80 | (cp & 0x3F));
	}
}

constexpr bool in(char32_t c, char32_t first, char32_t last) noexcept {
	return c >= first && c <= last;
}

void appendUtf16(std::u16string& out, char32_t cp) {
	if (cp >= 0x10000) {
		cp -= 0x10000;
		out += static_cast<char16_t>(0xD800 + (cp >> 10));
		out += static_cast<char16_t>(0xDC00 + (cp & 0x3FF));
	} else {
		out += static_cast<char16_t>(cp);
	}
}

constexpr bool isAsciiWhitespace(char c) noexcept {
	return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

} // namespace

std::u16string toUtf16(std::string_view utf8) {
	std::u16string out;
	out.reserve(utf8.size());
	decodeUtf8(utf8, [&](char32_t cp) { appendUtf16(out, cp); });
	return out;
}

std::string toUtf8(std::u16string_view utf16) {
	std::string out;
	out.reserve(utf16.size());
	for (size_t i = 0; i < utf16.size(); i++) {
		char32_t c = utf16[i];
		if (c >= 0xD800 && c <= 0xDBFF) {
			if (i + 1 < utf16.size() && utf16[i + 1] >= 0xDC00 && utf16[i + 1] <= 0xDFFF) {
				c = 0x10000 + ((c - 0xD800) << 10) + (utf16[i + 1] - 0xDC00);
				++i;
			} else {
				c = REPLACEMENT_CHAR;
			}
		} else if (c >= 0xDC00 && c <= 0xDFFF) {
			c = REPLACEMENT_CHAR;
		}
		encodeUtf8(out, c);
	}
	return out;
}

int32_t utf16Length(std::string_view utf8) {
	int32_t length = 0;
	decodeUtf8(utf8, [&](char32_t cp) { length += cp >= 0x10000 ? 2 : 1; });
	return length;
}

char32_t toLowerCase(char32_t c) noexcept {
	if (c < 0x80)
		return in(c, 'A', 'Z') ? c + 32 : c;
	if (in(c, 0xC0, 0xDE) && c != 0xD7)
		return c + 32;
	if (c < 0x100)
		return c;
	if (c <= 0x017F) { // Latin Extended-A
		if (c == 0x0130)
			return 0x0069;
		if (c == 0x0178)
			return 0x00FF;
		if (in(c, 0x0100, 0x012F) || in(c, 0x0132, 0x0137) || in(c, 0x014A, 0x0177))
			return c % 2 == 0 ? c + 1 : c;
		if (in(c, 0x0139, 0x0148) || in(c, 0x0179, 0x017E))
			return c % 2 == 1 ? c + 1 : c;
		return c;
	}
	if (in(c, 0x0370, 0x03FF)) { // Greek
		if (c == 0x0386)
			return 0x03AC;
		if (in(c, 0x0388, 0x038A))
			return c + 37;
		if (c == 0x038C)
			return 0x03CC;
		if (in(c, 0x038E, 0x038F))
			return c + 63;
		if (in(c, 0x0391, 0x03AB) && c != 0x03A2)
			return c + 32;
		return c;
	}
	if (in(c, 0x0400, 0x052F)) { // Cyrillic and Cyrillic Supplement
		if (in(c, 0x0400, 0x040F))
			return c + 80;
		if (in(c, 0x0410, 0x042F))
			return c + 32;
		if (c == 0x04C0)
			return 0x04CF;
		if (in(c, 0x0460, 0x0481) || in(c, 0x048A, 0x04BF) || in(c, 0x04D0, 0x052F))
			return c % 2 == 0 ? c + 1 : c;
		if (in(c, 0x04C1, 0x04CE))
			return c % 2 == 1 ? c + 1 : c;
	}
	return c;
}

char32_t toUpperCase(char32_t c) noexcept {
	if (c < 0x80)
		return in(c, 'a', 'z') ? c - 32 : c;
	if (c == 0x00B5)
		return 0x039C;
	if (in(c, 0xE0, 0xFE) && c != 0xF7)
		return c - 32;
	if (c == 0xFF)
		return 0x0178;
	if (c < 0x100)
		return c;
	if (c <= 0x017F) { // Latin Extended-A
		if (c == 0x0131)
			return 0x0049;
		if (c == 0x017F)
			return 0x0053;
		if (in(c, 0x0101, 0x012F) || in(c, 0x0133, 0x0137) || in(c, 0x014B, 0x0177))
			return c % 2 == 1 ? c - 1 : c;
		if (in(c, 0x013A, 0x0148) || in(c, 0x017A, 0x017E))
			return c % 2 == 0 ? c - 1 : c;
		return c;
	}
	if (in(c, 0x0370, 0x03FF)) { // Greek
		if (c == 0x03AC)
			return 0x0386;
		if (in(c, 0x03AD, 0x03AF))
			return c - 37;
		if (c == 0x03CC)
			return 0x038C;
		if (in(c, 0x03CD, 0x03CE))
			return c - 63;
		if (c == 0x03C2)
			return 0x03A3;
		if (in(c, 0x03B1, 0x03CB))
			return c - 32;
		return c;
	}
	if (in(c, 0x0400, 0x052F)) { // Cyrillic and Cyrillic Supplement
		if (in(c, 0x0430, 0x044F))
			return c - 32;
		if (in(c, 0x0450, 0x045F))
			return c - 80;
		if (c == 0x04CF)
			return 0x04C0;
		if (in(c, 0x0461, 0x0481) || in(c, 0x048B, 0x04BF) || in(c, 0x04D1, 0x052F))
			return c % 2 == 1 ? c - 1 : c;
		if (in(c, 0x04C2, 0x04CE))
			return c % 2 == 0 ? c - 1 : c;
	}
	return c;
}

bool equalsIgnoreCase(std::string_view a, std::string_view b) noexcept {
	bool asciiOnly = std::ranges::all_of(a, [](char c) { return static_cast<uint8_t>(c) < 0x80; }) &&
		std::ranges::all_of(b, [](char c) { return static_cast<uint8_t>(c) < 0x80; });
	if (asciiOnly) {
		return a.size() == b.size() &&
			std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) { return toLowerCase(char32_t(x)) == toLowerCase(char32_t(y)); });
	}
	try {
		std::u16string x = toUtf16(a);
		std::u16string y = toUtf16(b);
		if (x.size() != y.size())
			return false;
		for (size_t i = 0; i < x.size(); i++) {
			char32_t c1 = x[i];
			char32_t c2 = y[i];
			if (c1 == c2)
				continue;
			char32_t u1 = toUpperCase(c1);
			char32_t u2 = toUpperCase(c2);
			if (u1 != u2 && toLowerCase(u1) != toLowerCase(u2))
				return false;
		}
		return true;
	} catch (...) { // out of memory
		return false;
	}
}

std::string toLowerCase(std::string_view s) {
	std::string out;
	out.reserve(s.size());
	decodeUtf8(s, [&](char32_t cp) {
		if (cp == 0x0130) // Java: String.toLowerCase maps LATIN CAPITAL LETTER I WITH DOT ABOVE to i + COMBINING DOT ABOVE
			out += "i\xCC\x87";
		else
			encodeUtf8(out, toLowerCase(cp));
	});
	return out;
}

std::string toUpperCase(std::string_view s) {
	std::string out;
	out.reserve(s.size());
	decodeUtf8(s, [&](char32_t cp) {
		if (cp == 0x00DF) // Java: String.toUpperCase maps LATIN SMALL LETTER SHARP S to "SS"
			out += "SS";
		else
			encodeUtf8(out, toUpperCase(cp));
	});
	return out;
}

std::string substring(std::string_view s, int32_t beginIndex, int32_t endIndex) {
	std::u16string utf16 = toUtf16(s);
	int32_t length = static_cast<int32_t>(utf16.size());
	if (beginIndex < 0 || endIndex > length || beginIndex > endIndex)
		throw IndexOutOfBoundsException(fmt::format("begin {}, end {}, length {}", beginIndex, endIndex, length));
	return toUtf8(std::u16string_view(utf16).substr(static_cast<size_t>(beginIndex), static_cast<size_t>(endIndex - beginIndex)));
}

std::string substring(std::string_view s, int32_t beginIndex) {
	return substring(s, beginIndex, utf16Length(s));
}

std::string_view trim(std::string_view s) noexcept {
	size_t start = 0;
	size_t end = s.size();
	while (start < end && static_cast<uint8_t>(s[start]) <= ' ')
		++start;
	while (end > start && static_cast<uint8_t>(s[end - 1]) <= ' ')
		--end;
	return s.substr(start, end - start);
}

std::string_view strip(std::string_view s) noexcept {
	size_t start = 0;
	size_t end = s.size();
	while (start < end && isAsciiWhitespace(s[start]))
		++start;
	while (end > start && isAsciiWhitespace(s[end - 1]))
		--end;
	return s.substr(start, end - start);
}

bool isBlank(std::string_view s) noexcept {
	return strip(s).empty();
}

std::vector<std::string> split(std::string_view s, std::string_view delimiter) {
	std::vector<std::string> parts;
	if (delimiter.empty()) {
		parts.emplace_back(s);
		return parts;
	}
	size_t start = 0;
	for (size_t found; (found = s.find(delimiter, start)) != std::string_view::npos; start = found + delimiter.size())
		parts.emplace_back(s.substr(start, found - start));
	parts.emplace_back(s.substr(start));
	return parts;
}

std::vector<std::string> splitJava(std::string_view s, std::string_view delimiter) {
	std::vector<std::string> parts = split(s, delimiter);
	if (s.empty())
		return parts; // Java: "".split(x) -> [""]
	while (!parts.empty() && parts.back().empty())
		parts.pop_back();
	return parts;
}

std::string join(const std::vector<std::string>& parts, std::string_view delimiter) {
	std::string out;
	for (size_t i = 0; i < parts.size(); i++) {
		if (i > 0)
			out += delimiter;
		out += parts[i];
	}
	return out;
}

std::string replace(std::string_view s, std::string_view target, std::string_view replacement) {
	if (target.empty())
		return std::string(s);
	std::string out;
	size_t start = 0;
	for (size_t found; (found = s.find(target, start)) != std::string_view::npos; start = found + target.size()) {
		out.append(s.substr(start, found - start));
		out.append(replacement);
	}
	out.append(s.substr(start));
	return out;
}

} // namespace aion::commons::utils::StringUtils
