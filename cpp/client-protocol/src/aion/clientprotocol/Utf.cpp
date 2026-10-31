#include "aion/clientprotocol/Utf.h"

#include <cstdint>

namespace aion::clientprotocol {

namespace {

constexpr char32_t REPLACEMENT_CHAR = 0xFFFD;

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

void appendUtf16(std::u16string& out, char32_t cp) {
	if (cp >= 0x10000) {
		cp -= 0x10000;
		out += static_cast<char16_t>(0xD800 + (cp >> 10));
		out += static_cast<char16_t>(0xDC00 + (cp & 0x3FF));
	} else {
		out += static_cast<char16_t>(cp);
	}
}

constexpr bool isContinuation(uint8_t b) noexcept {
	return (b & 0xC0) == 0x80;
}

} // namespace

std::string utf16ToUtf8(std::u16string_view utf16) {
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

std::u16string utf8ToUtf16(std::string_view utf8) {
	std::u16string out;
	out.reserve(utf8.size());
	size_t pos = 0;
	while (pos < utf8.size()) {
		const uint8_t b1 = static_cast<uint8_t>(utf8[pos]);
		size_t length = 0;
		char32_t cp = 0;
		char32_t min = 0;
		if (b1 < 0x80) {
			length = 1;
			cp = b1;
		} else if ((b1 & 0xE0) == 0xC0) {
			length = 2;
			cp = b1 & 0x1Fu;
			min = 0x80;
		} else if ((b1 & 0xF0) == 0xE0) {
			length = 3;
			cp = b1 & 0x0Fu;
			min = 0x800;
		} else if ((b1 & 0xF8) == 0xF0) {
			length = 4;
			cp = b1 & 0x07u;
			min = 0x10000;
		}
		bool valid = length != 0 && pos + length <= utf8.size();
		for (size_t i = 1; valid && i < length; i++) {
			const uint8_t b = static_cast<uint8_t>(utf8[pos + i]);
			valid = isContinuation(b);
			cp = cp << 6 | (b & 0x3Fu);
		}
		valid = valid && cp >= min && cp <= 0x10FFFF && !(cp >= 0xD800 && cp <= 0xDFFF);
		if (valid) {
			appendUtf16(out, cp);
			pos += length;
		} else {
			// skip the lead byte and the continuation bytes that follow it
			out += static_cast<char16_t>(REPLACEMENT_CHAR);
			pos++;
			while (pos < utf8.size() && isContinuation(static_cast<uint8_t>(utf8[pos])))
				pos++;
		}
	}
	return out;
}

} // namespace aion::clientprotocol
