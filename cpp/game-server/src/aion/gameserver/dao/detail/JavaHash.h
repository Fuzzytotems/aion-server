#pragma once

#include <bit>
#include <cstdint>
#include <limits>
#include <string_view>

#include "aion/commons/utils/StringUtils.h"

/**
 * Java-compatible hash functions for the record hashCode ports of the DAO records (java.lang.runtime.ObjectMethods: h = 31 * h + hash(component),
 * starting at 0). No Java counterpart. Arithmetic is unsigned, so the wrap-around matches Java's int overflow.
 */
namespace aion::gameserver::dao::detail {

/** Java String.hashCode(): s[0]*31^(n-1) + ... + s[n-1] over the UTF-16 code units of the (UTF-8) text */
inline int32_t javaStringHashCode(std::string_view text) {
	uint32_t hash = 0;
	for (char16_t c : commons::utils::StringUtils::toUtf16(text))
		hash = 31 * hash + c;
	return static_cast<int32_t>(hash);
}

/** Java Float.hashCode(value) = Float.floatToIntBits(value): the raw bits, with every NaN collapsed to 0x7fc00000 */
inline int32_t javaFloatHashCode(float value) {
	if (value != value)
		return 0x7fc00000;
	return static_cast<int32_t>(std::bit_cast<uint32_t>(value));
}

/** Java Long.hashCode(value) = (int) (value ^ (value >>> 32)) */
inline int32_t javaLongHashCode(int64_t value) {
	const uint64_t bits = static_cast<uint64_t>(value);
	return static_cast<int32_t>(static_cast<uint32_t>(bits ^ (bits >> 32)));
}

/** One step of the record hash combiner: 31 * h + componentHash (Java int arithmetic) */
inline int32_t combineHash(int32_t hash, int32_t componentHash) {
	return static_cast<int32_t>(31u * static_cast<uint32_t>(hash) + static_cast<uint32_t>(componentHash));
}

} // namespace aion::gameserver::dao::detail
