#pragma once

#include <bit>
#include <cstdint>
#include <limits>

namespace aion::gameserver::utils {

/**
 * Java: java.lang.Math.round, bit-exact (the JDK 8+ implementation): the closest integer with ties rounded towards positive infinity
 * (Math.round(-2.5f) == -2, Math.round(2.5f) == 3, Math.round(0.49999997f) == 0), NaN rounds to 0 and values outside of the result range
 * saturate like Java's (int)/(long) casts. C++-only helper of P4-05; a static-only class (K5), thread-safe.
 */
class JavaMath final {
public:
	JavaMath() = delete;

	/** Java: Math.round(float) */
	static constexpr int32_t round(float a) noexcept {
		int32_t intBits = std::bit_cast<int32_t>(a);
		int32_t biasedExp = (intBits & 0x7F800000) >> 23;
		int32_t shift = (24 - 2 + 127) - biasedExp;
		if ((shift & -32) == 0) { // shift >= 0 && shift < 32
			int32_t r = (intBits & 0x007FFFFF) | (0x007FFFFF + 1);
			if (intBits < 0)
				r = -r;
			return ((r >> shift) + 1) >> 1;
		}
		return floatToInt(a); // |a| >= 2^23 (already an integer, infinite or NaN) or |a| < 2^-10
	}

	/** Java: Math.round(double) */
	static constexpr int64_t round(double a) noexcept {
		int64_t longBits = std::bit_cast<int64_t>(a);
		int64_t biasedExp = (longBits & 0x7FF0000000000000LL) >> 52;
		int64_t shift = (53 - 2 + 1023) - biasedExp;
		if ((shift & -64) == 0) { // shift >= 0 && shift < 64
			int64_t r = (longBits & 0x000FFFFFFFFFFFFFLL) | (0x000FFFFFFFFFFFFFLL + 1);
			if (longBits < 0)
				r = -r;
			return ((r >> shift) + 1) >> 1;
		}
		return doubleToLong(a);
	}

private:
	/** Java: (int) a - NaN 0, saturating */
	static constexpr int32_t floatToInt(float a) noexcept {
		if (a != a)
			return 0;
		if (a >= 2147483648.0f)
			return std::numeric_limits<int32_t>::max();
		if (a <= -2147483648.0f)
			return std::numeric_limits<int32_t>::min();
		return static_cast<int32_t>(a);
	}

	/** Java: (long) a - NaN 0, saturating */
	static constexpr int64_t doubleToLong(double a) noexcept {
		if (a != a)
			return 0;
		if (a >= 9223372036854775808.0)
			return std::numeric_limits<int64_t>::max();
		if (a <= -9223372036854775808.0)
			return std::numeric_limits<int64_t>::min();
		return static_cast<int64_t>(a);
	}
};

} // namespace aion::gameserver::utils
