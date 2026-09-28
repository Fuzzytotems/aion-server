#pragma once

#include <bit>
#include <cstdint>
#include <limits>
#include <string>

namespace aion::gameserver::geoEngine::math {

/**
 * The java.lang.Float / java.lang.Math helpers the jME math classes rely on, with Java semantics (no Java counterpart class of its own).
 * <p>
 * Used by Vector2f, Vector3f, Matrix3f and Matrix4f for equals/hashCode/toString and by FastMath for the bit tricks.
 */
struct JavaFloat final {
	JavaFloat() = delete;

	/** Java: Float.floatToRawIntBits */
	static constexpr int32_t floatToRawIntBits(float value) noexcept { return std::bit_cast<int32_t>(value); }

	/** Java: Float.floatToIntBits - like floatToRawIntBits, but every NaN maps to the canonical 0x7fc00000 */
	static constexpr int32_t floatToIntBits(float value) noexcept { return value != value ? 0x7fc00000 : std::bit_cast<int32_t>(value); }

	/** Java: Float.intBitsToFloat (NaN payloads are preserved on x64) */
	static constexpr float intBitsToFloat(int32_t bits) noexcept { return std::bit_cast<float>(bits); }

	/** Java: Float.isNaN */
	static constexpr bool isNaN(float value) noexcept { return value != value; }

	/** Java: Float.isInfinite */
	static constexpr bool isInfinite(float value) noexcept {
		return value == std::numeric_limits<float>::infinity() || value == -std::numeric_limits<float>::infinity();
	}

	/**
	 * Java: Float.compare - numerical order, except that -0.0f is smaller than 0.0f and NaN is equal to itself and greater than everything
	 * else (including positive infinity). Float.compare(a, b) == 0 exactly when floatToIntBits(a) == floatToIntBits(b).
	 */
	static constexpr int32_t compare(float f1, float f2) noexcept {
		if (f1 < f2)
			return -1;
		if (f1 > f2)
			return 1;
		const int32_t thisBits = floatToIntBits(f1);
		const int32_t anotherBits = floatToIntBits(f2);
		return thisBits == anotherBits ? 0 : (thisBits < anotherBits ? -1 : 1);
	}

	/** Java: Math.signum(float) - NaN and +-0.0f are returned unchanged, otherwise +-1.0f */
	static constexpr float signum(float f) noexcept {
		if (f == 0.0f || f != f)
			return f;
		return f < 0.0f ? -1.0f : 1.0f;
	}

	/** Java: Math.abs(float) - clears the sign bit (Math.abs(-0.0f) == 0.0f, unlike FastMath.abs) */
	static constexpr float mathAbs(float a) noexcept { return std::bit_cast<float>(std::bit_cast<uint32_t>(a) & 0x7fffffffu); }

	/** Java: the (int) cast of a double - NaN becomes 0, out-of-range values saturate */
	static constexpr int32_t doubleToInt(double value) noexcept {
		if (value != value)
			return 0;
		if (value >= 2147483647.0)
			return std::numeric_limits<int32_t>::max();
		if (value <= -2147483648.0)
			return std::numeric_limits<int32_t>::min();
		return static_cast<int32_t>(value);
	}

	/**
	 * Java: Float.toString (JDK 19+ algorithm, the server runs on Java 25): "NaN", "Infinity", "-Infinity", "0.0", "-0.0"; values in
	 * [10^-3, 10^7) as plain decimals with at least one fractional digit ("100.0", "0.001", "255.49063"); others in computerized scientific
	 * notation ("1.0E7", "1.0E-4", "3.4028235E38"). The digits are the shortest decimal that rounds to the value, closest to it when several
	 * exist; when a single digit is enough, the closest two-digit decimal is used instead ("1.4E-45" for Float.MIN_VALUE).
	 */
	static std::string toString(float value);
};

} // namespace aion::gameserver::geoEngine::math
