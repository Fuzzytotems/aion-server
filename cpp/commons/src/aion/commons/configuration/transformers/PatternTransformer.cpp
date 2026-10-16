#include "aion/commons/configuration/transformers/PatternTransformer.h"

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/StringUtils.h"

namespace aion::commons::configuration::transformers {

namespace {

void checkNotEmpty(std::string_view value) {
	if (value.empty())
		throw utils::IllegalArgumentException("Cannot convert empty string to Pattern (bind a std::optional to allow empty values)");
}

} // namespace

std::wstring PatternTransformer::toWide(std::string_view utf8) {
	std::u16string utf16 = utils::StringUtils::toUtf16(utf8);
	std::wstring wide;
	wide.reserve(utf16.size());
	if constexpr (sizeof(wchar_t) == sizeof(char16_t)) {
		for (char16_t c : utf16)
			wide.push_back(static_cast<wchar_t>(c));
	} else {
		// combine surrogate pairs (toUtf16 never produces unpaired surrogates)
		for (std::size_t i = 0; i < utf16.size(); i++) {
			char32_t c = utf16[i];
			if (c >= 0xD800 && c <= 0xDBFF && i + 1 < utf16.size())
				c = 0x10000 + ((c - 0xD800) << 10) + (utf16[++i] - 0xDC00);
			wide.push_back(static_cast<wchar_t>(c));
		}
	}
	return wide;
}

std::regex PropertyTransformer<std::regex>::parseObject(std::string_view value) {
	checkNotEmpty(value);
	return std::regex(value.begin(), value.end(), std::regex::ECMAScript);
}

std::wregex PropertyTransformer<std::wregex>::parseObject(std::string_view value) {
	checkNotEmpty(value);
	return std::wregex(PatternTransformer::toWide(value), std::regex::ECMAScript);
}

} // namespace aion::commons::configuration::transformers
