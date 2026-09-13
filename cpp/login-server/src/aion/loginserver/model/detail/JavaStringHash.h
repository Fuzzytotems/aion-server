#pragma once

#include <cstdint>
#include <string_view>

#include "aion/commons/utils/StringUtils.h"

namespace aion::loginserver::model::detail {

/**
 * Java: String.hashCode() of the UTF-16 representation of a UTF-8 string (s[0]*31^(n-1) + ... + s[n-1], wrapping on overflow). Used by the
 * models whose Java hashCode (and therefore Object.toString) is based on string fields.
 */
inline int32_t javaStringHashCode(std::string_view utf8) {
	uint32_t hash = 0;
	for (char16_t unit : commons::utils::StringUtils::toUtf16(utf8))
		hash = 31u * hash + unit;
	return static_cast<int32_t>(hash);
}

} // namespace aion::loginserver::model::detail
