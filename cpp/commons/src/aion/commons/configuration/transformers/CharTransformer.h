#pragma once

#include <string>
#include <string_view>

#include "aion/commons/configuration/transformers/PropertyTransformer.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/StringUtils.h"

namespace aion::commons::configuration::transformers {

/**
 * Transforms string representation of character to character (Java char = one UTF-16 code unit). The value must consist of exactly one UTF-16
 * code unit, so characters outside the BMP (surrogate pairs) are rejected like in Java.
 * <p>
 * Java: com.aionemu.commons.configuration.transformers.CharTransformer
 */
template <>
struct PropertyTransformer<char16_t> {
	static std::string typeName() { return "char"; }

	static char16_t parseObject(std::string_view value) {
		std::u16string chars = utils::StringUtils::toUtf16(value);
		if (chars.empty())
			throw utils::IllegalArgumentException("Cannot convert empty string to character.");
		if (chars.size() > 1)
			throw utils::IllegalArgumentException("Too many characters in the value.");
		return chars[0];
	}
};

} // namespace aion::commons::configuration::transformers
