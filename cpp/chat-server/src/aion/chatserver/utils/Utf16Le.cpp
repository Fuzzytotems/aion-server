#include "aion/chatserver/utils/Utf16Le.h"

#include "aion/commons/utils/StringUtils.h"

namespace aion::chatserver::utils::Utf16Le {

namespace {

constexpr char16_t REPLACEMENT = u'�';
/** Java: UnicodeDecoder.REVERSED_MARK */
constexpr char16_t REVERSED_MARK = u'￾';

constexpr bool isHighSurrogate(char16_t c) noexcept {
	return c >= 0xD800 && c <= 0xDBFF;
}

constexpr bool isLowSurrogate(char16_t c) noexcept {
	return c >= 0xDC00 && c <= 0xDFFF;
}

char16_t charAt(std::span<const uint8_t> bytes, size_t index) noexcept {
	return static_cast<char16_t>(bytes[index] | bytes[index + 1] << 8);
}

} // namespace

std::u16string decode(std::span<const uint8_t> bytes) {
	std::u16string result;
	result.reserve(bytes.size() / 2 + 1);
	size_t pos = 0;
	// Java: UnicodeDecoder.decodeLoop, with CharsetDecoder replacing each malformed input by one U+FFFD and skipping its length
	while (bytes.size() - pos > 1) {
		char16_t c = charAt(bytes, pos);
		if (c == REVERSED_MARK) {
			result += REPLACEMENT; // malformedForLength(2)
			pos += 2;
			continue;
		}
		if (isHighSurrogate(c)) {
			if (bytes.size() - pos - 2 < 2)
				break; // underflow: the bytes left are replaced at the end of the input
			char16_t c2 = charAt(bytes, pos + 2);
			if (!isLowSurrogate(c2)) {
				result += REPLACEMENT; // malformedForLength(4)
				pos += 4;
				continue;
			}
			result += c;
			result += c2;
			pos += 4;
			continue;
		}
		if (isLowSurrogate(c)) {
			result += REPLACEMENT; // unpaired low surrogate: malformedForLength(2)
			pos += 2;
			continue;
		}
		result += c;
		pos += 2;
	}
	// CharsetDecoder.decode(in, out, true): input left after an underflow is malformed for its whole length
	if (pos < bytes.size())
		result += REPLACEMENT;
	return result;
}

std::string newString(std::span<const uint8_t> bytes) {
	return commons::utils::StringUtils::toUtf8(decode(bytes));
}

std::vector<uint8_t> getBytes(std::string_view utf8) {
	std::u16string text = commons::utils::StringUtils::toUtf16(utf8);
	std::vector<uint8_t> bytes;
	bytes.reserve(text.size() * 2);
	for (char16_t c : text) {
		bytes.push_back(static_cast<uint8_t>(c));
		bytes.push_back(static_cast<uint8_t>(c >> 8));
	}
	return bytes;
}

} // namespace aion::chatserver::utils::Utf16Le
