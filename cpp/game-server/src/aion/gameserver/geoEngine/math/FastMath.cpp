#include "aion/gameserver/geoEngine/math/FastMath.h"

#include <cmath>
#include <numbers>

#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/geoEngine/math/StrictFp.h"
#include "aion/gameserver/geoEngine/math/StrictMath.h"
#include "aion/gameserver/geoEngine/math/Vector2f.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"

namespace aion::gameserver::geoEngine::math {

int32_t FastMath::nearestPowerOfTwo(int32_t number) noexcept {
	return JavaFloat::doubleToInt(std::pow(2.0, std::ceil(std::log(static_cast<double>(number)) / std::log(2.0))));
}

float FastMath::interpolateLinear(float scale, float startValue, float endValue) noexcept {
	if (startValue == endValue)
		return startValue;
	if (scale <= 0.0f)
		return startValue;
	if (scale >= 1.0f)
		return endValue;
	return ((1.0f - scale) * startValue) + (scale * endValue);
}

Vector3f FastMath::interpolateLinear(float scale, const Vector3f& startValue, const Vector3f& endValue) noexcept {
	Vector3f res;
	res.x = interpolateLinear(scale, startValue.x, endValue.x);
	res.y = interpolateLinear(scale, startValue.y, endValue.y);
	res.z = interpolateLinear(scale, startValue.z, endValue.z);
	return res;
}

float FastMath::interpolateCatmullRom(float u, float T, float p0, float p1, float p2, float p3) noexcept {
	// Java types kept exactly: c2 mixes double and float terms, c3 and c4 are float expressions stored in doubles
	double c1, c2, c3, c4;
	c1 = p1;
	c2 = -1.0 * T * p0 + T * p2;
	c3 = 2 * T * p0 + (T - 3) * p1 + (3 - 2 * T) * p2 + -T * p3;
	c4 = -T * p0 + (2 - T) * p1 + (T - 2) * p2 + T * p3;
	return static_cast<float>(((c4 * u + c3) * u + c2) * u + c1);
}

Vector3f FastMath::interpolateCatmullRom(float u, float T, const Vector3f& p0, const Vector3f& p1, const Vector3f& p2, const Vector3f& p3) noexcept {
	Vector3f res;
	res.x = interpolateCatmullRom(u, T, p0.x, p1.x, p2.x, p3.x);
	res.y = interpolateCatmullRom(u, T, p0.y, p1.y, p2.y, p3.y);
	res.z = interpolateCatmullRom(u, T, p0.z, p1.z, p2.z, p3.z);
	return res;
}

float FastMath::acos(float fValue) noexcept {
	if (-1.0f < fValue) {
		if (fValue < 1.0f)
			return static_cast<float>(StrictMath::acos(fValue));
		return 0.0f;
	}
	return PI;
}

float FastMath::asin(float fValue) noexcept {
	if (-1.0f < fValue) {
		if (fValue < 1.0f)
			return static_cast<float>(StrictMath::asin(fValue));
		return HALF_PI;
	}
	return -HALF_PI;
}

float FastMath::atan(float fValue) noexcept {
	return static_cast<float>(StrictMath::atan(fValue));
}

float FastMath::atan2(float fY, float fX) noexcept {
	return static_cast<float>(StrictMath::atan2(fY, fX));
}

float FastMath::ceil(float fValue) noexcept {
	return static_cast<float>(std::ceil(static_cast<double>(fValue)));
}

float FastMath::reduceSinAngle(float radians) noexcept {
	radians = std::fmod(radians, TWO_PI); // Java float %: truncated remainder with the sign of the dividend, exact
	if (JavaFloat::mathAbs(radians) > PI)
		radians = radians - (TWO_PI);
	if (JavaFloat::mathAbs(radians) > HALF_PI)
		radians = PI - radians;
	return radians;
}

float FastMath::sin2(float fValue) noexcept {
	fValue = reduceSinAngle(fValue);
	if (JavaFloat::mathAbs(fValue) <= std::numbers::pi / 4)
		return static_cast<float>(std::sin(static_cast<double>(fValue)));
	return static_cast<float>(std::cos(std::numbers::pi / 2 - fValue));
}

float FastMath::cos2(float fValue) noexcept {
	return sin2(fValue + HALF_PI);
}

float FastMath::cos(float v) noexcept {
	return static_cast<float>(std::cos(static_cast<double>(v)));
}

float FastMath::sin(float v) noexcept {
	return static_cast<float>(std::sin(static_cast<double>(v)));
}

float FastMath::exp(float fValue) noexcept {
	return static_cast<float>(std::exp(static_cast<double>(fValue)));
}

float FastMath::floor(float fValue) noexcept {
	return static_cast<float>(std::floor(static_cast<double>(fValue)));
}

float FastMath::invSqrt(float fValue) noexcept {
	return static_cast<float>(1.0f / std::sqrt(static_cast<double>(fValue)));
}

float FastMath::fastInvSqrt(float x) noexcept {
	const float xhalf = 0.5f * x;
	int32_t i = JavaFloat::floatToIntBits(x);                              // get bits for floating value
	i = static_cast<int32_t>(0x5f375a86u - static_cast<uint32_t>(i >> 1)); // gives initial guess y0 (Java int arithmetic wraps)
	x = JavaFloat::intBitsToFloat(i);                                      // convert bits back to float
	x = x * (1.5f - xhalf * x * x);                                        // Newton step, repeating increases accuracy
	return x;
}

float FastMath::log(float fValue) noexcept {
	return static_cast<float>(std::log(static_cast<double>(fValue)));
}

float FastMath::log(float value, float base) noexcept {
	return static_cast<float>(std::log(static_cast<double>(value)) / std::log(static_cast<double>(base)));
}

float FastMath::pow(float fBase, float fExponent) noexcept {
	return static_cast<float>(std::pow(static_cast<double>(fBase), static_cast<double>(fExponent)));
}

float FastMath::sqrt(float fValue) noexcept {
	return static_cast<float>(std::sqrt(static_cast<double>(fValue)));
}

float FastMath::tan(float fValue) noexcept {
	return static_cast<float>(std::tan(static_cast<double>(fValue)));
}

float FastMath::sign(float fValue) noexcept {
	return JavaFloat::signum(fValue);
}

int32_t FastMath::counterClockwise(const Vector2f& p0, const Vector2f& p1, const Vector2f& p2) noexcept {
	float dx1, dx2, dy1, dy2;
	dx1 = p1.x - p0.x;
	dy1 = p1.y - p0.y;
	dx2 = p2.x - p0.x;
	dy2 = p2.y - p0.y;
	if (dx1 * dy2 > dy1 * dx2)
		return 1;
	if (dx1 * dy2 < dy1 * dx2)
		return -1;
	if ((dx1 * dx2 < 0) || (dy1 * dy2 < 0))
		return -1;
	if ((dx1 * dx1 + dy1 * dy1) < (dx2 * dx2 + dy2 * dy2))
		return 1;
	return 0;
}

int32_t FastMath::pointInsideTriangle(const Vector2f& t0, const Vector2f& t1, const Vector2f& t2, const Vector2f& p) noexcept {
	const int32_t val1 = counterClockwise(t0, t1, p);
	if (val1 == 0)
		return 1;
	const int32_t val2 = counterClockwise(t1, t2, p);
	if (val2 == 0)
		return 1;
	if (val2 != val1)
		return 0;
	const int32_t val3 = counterClockwise(t2, t0, p);
	if (val3 == 0)
		return 1;
	if (val3 != val1)
		return 0;
	return val3;
}

float FastMath::determinant(double m00, double m01, double m02, double m03, double m10, double m11, double m12, double m13, double m20, double m21,
                            double m22, double m23, double m30, double m31, double m32, double m33) noexcept {
	const double det01 = m20 * m31 - m21 * m30;
	const double det02 = m20 * m32 - m22 * m30;
	const double det03 = m20 * m33 - m23 * m30;
	const double det12 = m21 * m32 - m22 * m31;
	const double det13 = m21 * m33 - m23 * m31;
	const double det23 = m22 * m33 - m23 * m32;
	return static_cast<float>(m00 * (m11 * det23 - m12 * det13 + m13 * det12) - m01 * (m10 * det23 - m12 * det03 + m13 * det02) +
	                          m02 * (m10 * det13 - m11 * det03 + m13 * det01) - m03 * (m10 * det12 - m11 * det02 + m12 * det01));
}

Vector3f& FastMath::sphericalToCartesian(const Vector3f& sphereCoords, Vector3f& store) noexcept {
	store.y = sphereCoords.x * FastMath::sin(sphereCoords.z);
	const float a = sphereCoords.x * FastMath::cos(sphereCoords.z);
	store.x = a * FastMath::cos(sphereCoords.y);
	store.z = a * FastMath::sin(sphereCoords.y);
	return store;
}

Vector3f& FastMath::cartesianToSpherical(Vector3f& cartCoords, Vector3f& store) noexcept {
	if (cartCoords.x == 0)
		cartCoords.x = FLOAT_EPSILON;
	store.x = FastMath::sqrt((cartCoords.x * cartCoords.x) + (cartCoords.y * cartCoords.y) + (cartCoords.z * cartCoords.z));
	store.y = FastMath::atan(cartCoords.z / cartCoords.x);
	if (cartCoords.x < 0)
		store.y += PI;
	store.z = FastMath::asin(cartCoords.y / store.x);
	return store;
}

Vector3f& FastMath::sphericalToCartesianZ(const Vector3f& sphereCoords, Vector3f& store) noexcept {
	store.z = sphereCoords.x * FastMath::sin(sphereCoords.z);
	const float a = sphereCoords.x * FastMath::cos(sphereCoords.z);
	store.x = a * FastMath::cos(sphereCoords.y);
	store.y = a * FastMath::sin(sphereCoords.y);
	return store;
}

Vector3f& FastMath::cartesianZToSpherical(Vector3f& cartCoords, Vector3f& store) noexcept {
	if (cartCoords.x == 0)
		cartCoords.x = FLOAT_EPSILON;
	store.x = FastMath::sqrt((cartCoords.x * cartCoords.x) + (cartCoords.y * cartCoords.y) + (cartCoords.z * cartCoords.z));
	store.z = FastMath::atan(cartCoords.z / cartCoords.x);
	if (cartCoords.x < 0)
		store.z += PI;
	store.y = FastMath::asin(cartCoords.y / store.x);
	return store;
}

float FastMath::normalize(float val, float min, float max) noexcept {
	if (JavaFloat::isInfinite(val) || JavaFloat::isNaN(val))
		return 0.0f;
	const float range = max - min;
	while (val > max)
		val -= range;
	while (val < min)
		val += range;
	return val;
}

float FastMath::copysign(float x, float y) noexcept {
	if (y >= 0 && x <= -0)
		return -x;
	else if (y < 0 && x >= 0)
		return -x;
	else
		return x;
}

float FastMath::convertHalfToFloat(int16_t half) noexcept {
	switch (half) {
		case static_cast<int16_t>(0x0000):
			return 0.0f;
		case static_cast<int16_t>(0x8000):
			return -0.0f;
		case static_cast<int16_t>(0x7c00):
			return std::numeric_limits<float>::infinity();
		case static_cast<int16_t>(0xfc00):
			return -std::numeric_limits<float>::infinity();
		default: {
			// half is sign-extended to int like in Java; the masks drop the extension
			const uint32_t h = static_cast<uint32_t>(static_cast<int32_t>(half));
			return JavaFloat::intBitsToFloat(static_cast<int32_t>(((h & 0x8000u) << 16) | (((h & 0x7c00u) + 0x1C000u) << 13) | ((h & 0x03FFu) << 13)));
		}
	}
}

int16_t FastMath::convertFloatToHalf(float flt) {
	if (JavaFloat::isNaN(flt))
		throw commons::utils::UnsupportedOperationException("NaN to half conversion not supported!");
	else if (flt == std::numeric_limits<float>::infinity())
		return static_cast<int16_t>(0x7c00);
	else if (flt == -std::numeric_limits<float>::infinity())
		return static_cast<int16_t>(0xfc00);
	else if (flt == 0.0f)
		return static_cast<int16_t>(0x0000);
	else if (flt == -0.0f)
		return static_cast<int16_t>(0x8000);
	else if (flt > 65504.0f) // max value supported by half float
		return 0x7bff;
	else if (flt < -65504.0f)
		return static_cast<int16_t>(0x7bff | 0x8000);
	else if (flt > 0.0f && flt < 5.96046E-8f)
		return 0x0001;
	else if (flt < 0.0f && flt > -5.96046E-8f)
		return static_cast<int16_t>(0x8001);

	const int32_t f = JavaFloat::floatToIntBits(flt);
	return static_cast<int16_t>(((f >> 16) & 0x8000) | ((((f & 0x7f800000) - 0x38000000) >> 13) & 0x7c00) | ((f >> 13) & 0x03ff));
}

} // namespace aion::gameserver::geoEngine::math
