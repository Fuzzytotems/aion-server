#pragma once

#include <bit>
#include <cstdint>

namespace aion::gameserver::geoEngine::utils {

/**
 * C++ only: java.lang.Math.min/max for floats, which the geo engine uses where NaN can occur (BIH split planes with an infinite inverse ray
 * direction: 0 * infinity). std::min/std::max return their first argument for NaN and ignore the sign of zero; Java returns NaN if either
 * argument is NaN and treats -0.0f as smaller than 0.0f.
 */

/** Java: Math.max(float, float) */
constexpr float javaMax(float a, float b) noexcept {
	if (a != a)
		return a; // a is NaN
	if (a == 0.0f && b == 0.0f && std::bit_cast<uint32_t>(a) == 0x80000000u)
		return b; // max(-0.0f, +-0.0f) is the second argument
	return (a >= b) ? a : b;
}

/** Java: Math.min(float, float) */
constexpr float javaMin(float a, float b) noexcept {
	if (a != a)
		return a; // a is NaN
	if (a == 0.0f && b == 0.0f && std::bit_cast<uint32_t>(b) == 0x80000000u)
		return b; // min(+-0.0f, -0.0f) is -0.0f
	return (a <= b) ? a : b;
}

} // namespace aion::gameserver::geoEngine::utils
