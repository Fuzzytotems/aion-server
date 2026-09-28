#pragma once

#include <bit>
#include <cstdint>
#include <numbers>

namespace aion::gameserver::geoEngine::math {

class Vector2f;
class Vector3f;

/**
 * Java: com.aionemu.gameserver.geoEngine.math.FastMath (jMonkeyEngine) - float versions of Math functions and math constants.
 * <p>
 * Semantics are Java's: float expressions are evaluated in float, `(float) Math.f(fValue)` widens to double first, comparisons with NaN are
 * false (so e.g. clamp(NaN) returns NaN and abs(-0.0f) returns -0.0f, as in the Java code).
 * <p>
 * Deviations
 * - FLT_EPSILON and DBL_EPSILON are named FLOAT_EPSILON and DOUBLE_EPSILON (the Java names are &lt;cfloat&gt; macros).
 * - asin/acos/atan/atan2 are bit-exact (StrictMath, fdlibm). sin/cos/tan/exp/log/pow use the C runtime: HotSpot computes Math.sin/cos/tan/
 *   exp/log/pow with its own intrinsics, which are allowed to differ from each other by 1 ulp, so the last bit may differ from the Java server.
 * - rand, nextRandomFloat and nextRandomInt (a java.util.Random seeded with the wall clock, unused by the server) are not ported; use Rnd.
 *
 * @author Various
 */
struct FastMath final {
	FastMath() = delete;

	/** A "close to zero" double epsilon value for use (Java: DBL_EPSILON) */
	static constexpr double DOUBLE_EPSILON = 2.220446049250313E-16;
	/** A "close to zero" float epsilon value for use (Java: FLT_EPSILON) */
	static constexpr float FLOAT_EPSILON = 1.1920928955078125E-7f;
	/** A "close to zero" float epsilon value for use */
	static constexpr float ZERO_TOLERANCE = 0.0001f;
	static constexpr float ONE_THIRD = 1.0f / 3.0f;
	/** The value PI as a float. (180 degrees) */
	static constexpr float PI = static_cast<float>(std::numbers::pi);
	/** The value 2PI as a float. (360 degrees) */
	static constexpr float TWO_PI = 2.0f * PI;
	/** The value PI/2 as a float. (90 degrees) */
	static constexpr float HALF_PI = 0.5f * PI;
	/** The value PI/4 as a float. (45 degrees) */
	static constexpr float QUARTER_PI = 0.25f * PI;
	/** The value 1/PI as a float. */
	static constexpr float INV_PI = 1.0f / PI;
	/** The value 1/(2PI) as a float. */
	static constexpr float INV_TWO_PI = 1.0f / TWO_PI;
	/** A value to multiply a degree value by, to convert it to radians. */
	static constexpr float DEG_TO_RAD = PI / 180.0f;
	/** A value to multiply a radian value by, to convert it to degrees. */
	static constexpr float RAD_TO_DEG = 180.0f / PI;

	/** Returns true if the number is a power of 2 (2,4,8,16...) */
	static constexpr bool isPowerOfTwo(int32_t number) noexcept { return (number > 0) && (number & (number - 1)) == 0; }

	/** Java: (int) Math.pow(2, Math.ceil(Math.log(number) / Math.log(2))) */
	static int32_t nearestPowerOfTwo(int32_t number) noexcept;

	/** Linear interpolation from startValue to endValue by the given percent: ((1 - scale) * startValue) + (scale * endValue) */
	static float interpolateLinear(float scale, float startValue, float endValue) noexcept;
	static Vector3f interpolateLinear(float scale, const Vector3f& startValue, const Vector3f& endValue) noexcept;

	/** Interpolate a spline between at least 4 control points following the Catmull-Rom equation (mixed float/double arithmetic as in Java). */
	static float interpolateCatmullRom(float u, float T, float p0, float p1, float p2, float p3) noexcept;
	static Vector3f interpolateCatmullRom(float u, float T, const Vector3f& p0, const Vector3f& p1, const Vector3f& p2, const Vector3f& p3) noexcept;

	/** Arc cosine; PI if fValue is not greater than -1, 0 if it is not smaller than 1 (NaN also gives PI) */
	static float acos(float fValue) noexcept;
	/** Arc sine; -HALF_PI if fValue is not greater than -1, HALF_PI if it is not smaller than 1 (NaN also gives -HALF_PI) */
	static float asin(float fValue) noexcept;
	static float atan(float fValue) noexcept;
	static float atan2(float fY, float fX) noexcept;
	static float ceil(float fValue) noexcept;
	/** Puts the angle into the -PI/2..+PI/2 range for sin2 */
	static float reduceSinAngle(float radians) noexcept;
	static float sin2(float fValue) noexcept;
	static float cos2(float fValue) noexcept;
	static float cos(float v) noexcept;
	static float sin(float v) noexcept;
	static float exp(float fValue) noexcept;
	/** Returns -fValue if fValue < 0, else fValue (so abs(-0.0f) is -0.0f and a negative NaN keeps its sign, unlike Math.abs) */
	static constexpr float abs(float fValue) noexcept {
		// Not written as `fValue < 0 ? -fValue : fValue`: MSVC /O2 with /fp:precise folds that ternary into fabs, which clears the sign of
		// -0.0f (found by the Release run of FastMathTest). Negating a value below zero is the same as clearing its sign bit.
		const uint32_t bits = std::bit_cast<uint32_t>(fValue);
		return std::bit_cast<float>(fValue < 0.0f ? (bits & 0x7fffffffu) : bits);
	}
	static float floor(float fValue) noexcept;
	/** Returns (float) (1.0 / Math.sqrt(fValue)) */
	static float invSqrt(float fValue) noexcept;
	/** The 0x5f375a86 approximation with one Newton step */
	static float fastInvSqrt(float x) noexcept;
	static float log(float fValue) noexcept;
	/** Returns (float) (Math.log(value) / Math.log(base)) */
	static float log(float value, float base) noexcept;
	static float pow(float fBase, float fExponent) noexcept;
	static constexpr float sqr(float fValue) noexcept { return fValue * fValue; }
	/** Returns (float) Math.sqrt(fValue) */
	static float sqrt(float fValue) noexcept;
	static float tan(float fValue) noexcept;
	static constexpr int32_t sign(int32_t iValue) noexcept { return iValue > 0 ? 1 : (iValue < 0 ? -1 : 0); }
	/** Java: Math.signum(float) */
	static float sign(float fValue) noexcept;

	/** 1 if the points p0-p1-p2 are counter clockwise, -1 if not, 0 if p2 is between p0 and p1 */
	static int32_t counterClockwise(const Vector2f& p0, const Vector2f& p1, const Vector2f& p2) noexcept;
	/** 1 or -1 if the point is inside the triangle, 0 otherwise */
	static int32_t pointInsideTriangle(const Vector2f& t0, const Vector2f& t1, const Vector2f& t2, const Vector2f& p) noexcept;

	/** Returns the determinant of a 4x4 matrix (evaluated in double, cast to float). */
	static float determinant(double m00, double m01, double m02, double m03, double m10, double m11, double m12, double m13, double m20, double m21,
	                         double m22, double m23, double m30, double m31, double m32, double m33) noexcept;

	/** Converts spherical coordinates (radius, azimuth, polar) to Cartesian with positive Y as up. */
	static Vector3f& sphericalToCartesian(const Vector3f& sphereCoords, Vector3f& store) noexcept;
	/** Converts Cartesian coordinates (positive Y up) to spherical. Like Java, sets cartCoords.x to FLOAT_EPSILON if it is 0. */
	static Vector3f& cartesianToSpherical(Vector3f& cartCoords, Vector3f& store) noexcept;
	/** Converts spherical coordinates to Cartesian with positive Z as up. */
	static Vector3f& sphericalToCartesianZ(const Vector3f& sphereCoords, Vector3f& store) noexcept;
	/** Converts Cartesian coordinates (positive Z up) to spherical. Like Java, sets cartCoords.x to FLOAT_EPSILON if it is 0. */
	static Vector3f& cartesianZToSpherical(Vector3f& cartCoords, Vector3f& store) noexcept;

	/** Takes a value and expresses it in terms of min to max (0 for NaN or infinite values). */
	static float normalize(float val, float min, float max) noexcept;
	/** x with its sign changed to match the sign of y (the jME variant, which treats -0.0f like 0.0f) */
	static float copysign(float x, float y) noexcept;
	static constexpr float clamp(float input, float min, float max) noexcept { return (input < min) ? min : (input > max) ? max : input; }
	static constexpr float saturate(float input) noexcept { return clamp(input, 0.0f, 1.0f); }

	/** Converts a half precision (16 bit) float to single precision. */
	static float convertHalfToFloat(int16_t half) noexcept;
	/**
	 * Converts a single precision float to half precision.
	 * @throws UnsupportedOperationException for NaN
	 */
	static int16_t convertFloatToHalf(float flt);
};

} // namespace aion::gameserver::geoEngine::math
