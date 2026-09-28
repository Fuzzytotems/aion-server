// fdlibm 5.3 (Sun Microsystems, freely distributable) as translated to Java in the JDK (java.lang.FdLibm): e_asin.c, e_acos.c, s_atan.c,
// e_atan2.c. Constants are given with their IEEE bit patterns (the decimal values of the original sources are in the comments).
//
// Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
// Developed at SunSoft, a Sun Microsystems, Inc. business. Permission to use, copy, modify, and distribute this software is freely granted,
// provided that this notice is preserved.

#include "aion/gameserver/geoEngine/math/StrictMath.h"

#include <bit>
#include <cmath>
#include <cstdint>

#include "aion/gameserver/geoEngine/math/StrictFp.h"

namespace aion::gameserver::geoEngine::math {

namespace {

constexpr double fromBits(uint32_t hi, uint32_t lo) noexcept {
	return std::bit_cast<double>((static_cast<uint64_t>(hi) << 32) | lo);
}

constexpr int32_t highWord(double x) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(std::bit_cast<uint64_t>(x) >> 32));
}

constexpr uint32_t lowWord(double x) noexcept {
	return static_cast<uint32_t>(std::bit_cast<uint64_t>(x));
}

constexpr double withLowWord(double x, uint32_t lo) noexcept {
	return std::bit_cast<double>((std::bit_cast<uint64_t>(x) & 0xffffffff00000000ull) | lo);
}

constexpr double withHighWord(double x, uint32_t hi) noexcept {
	return std::bit_cast<double>((std::bit_cast<uint64_t>(x) & 0x00000000ffffffffull) | (static_cast<uint64_t>(hi) << 32));
}

constexpr double one = 1.0;
constexpr double huge = 1.000e+300;
constexpr double tiny = 1.0e-300;

constexpr double pi = fromBits(0x400921FB, 0x54442D18);      // 3.14159265358979311600e+00
constexpr double pio2_hi = fromBits(0x3FF921FB, 0x54442D18); // 1.57079632679489655800e+00
constexpr double pio2_lo = fromBits(0x3C91A626, 0x33145C07); // 6.12323399573676603587e-17
constexpr double pio4_hi = fromBits(0x3FE921FB, 0x54442D18); // 7.85398163397448278999e-01

// coefficients for R(x^2) (asin, acos)
constexpr double pS0 = fromBits(0x3FC55555, 0x55555555); //  1.66666666666666657415e-01
constexpr double pS1 = fromBits(0xBFD4D612, 0x03EB6F7D); // -3.25565818622400915405e-01
constexpr double pS2 = fromBits(0x3FC9C155, 0x0E884455); //  2.01212532134862925881e-01
constexpr double pS3 = fromBits(0xBFA48228, 0xB5688F3B); // -4.00555345006794114027e-02
constexpr double pS4 = fromBits(0x3F49EFE0, 0x7501B288); //  7.91534994289814532176e-04
constexpr double pS5 = fromBits(0x3F023DE1, 0x0DFDF709); //  3.47933107596021167570e-05
constexpr double qS1 = fromBits(0xC0033A27, 0x1C8A2D4B); // -2.40339491173441421878e+00
constexpr double qS2 = fromBits(0x40002AE5, 0x9C598AC8); //  2.02094576023350569471e+00
constexpr double qS3 = fromBits(0xBFE6066C, 0x1B8D0159); // -6.88283971605453293030e-01
constexpr double qS4 = fromBits(0x3FB3B8C5, 0xB12E9282); //  7.70381505559019352791e-02

constexpr double atanhi[] = {
  fromBits(0x3FDDAC67, 0x0561BB4F), // 4.63647609000806093515e-01 atan(0.5)hi
  fromBits(0x3FE921FB, 0x54442D18), // 7.85398163397448278999e-01 atan(1.0)hi
  fromBits(0x3FEF730B, 0xD281F69B), // 9.82793723247329054082e-01 atan(1.5)hi
  fromBits(0x3FF921FB, 0x54442D18), // 1.57079632679489655800e+00 atan(inf)hi
};

constexpr double atanlo[] = {
  fromBits(0x3C7A2B7F, 0x222F65E2), // 2.26987774529616870924e-17 atan(0.5)lo
  fromBits(0x3C81A626, 0x33145C07), // 3.06161699786838301793e-17 atan(1.0)lo
  fromBits(0x3C700788, 0x7AF0CBBD), // 1.39033110312309984516e-17 atan(1.5)lo
  fromBits(0x3C91A626, 0x33145C07), // 6.12323399573676603587e-17 atan(inf)lo
};

constexpr double aT[] = {
  fromBits(0x3FD55555, 0x5555550D), //  3.33333333333329318027e-01
  fromBits(0xBFC99999, 0x9998EBC4), // -1.99999999998764832476e-01
  fromBits(0x3FC24924, 0x920083FF), //  1.42857142725034663711e-01
  fromBits(0xBFBC71C6, 0xFE231671), // -1.11111104054623557880e-01
  fromBits(0x3FB745CD, 0xC54C206E), //  9.09088713343650656196e-02
  fromBits(0xBFB3B0F2, 0xAF749A6D), // -7.69187620504482999495e-02
  fromBits(0x3FB10D66, 0xA0D03D51), //  6.66107313738753120669e-02
  fromBits(0xBFADDE2D, 0x52DEFD9A), // -5.83357013379057348645e-02
  fromBits(0x3FA97B4B, 0x24760DEB), //  4.97687799461593236017e-02
  fromBits(0xBFA2B444, 0x2C6A6C2F), // -3.65315727442169155270e-02
  fromBits(0x3F90AD3A, 0xE322DA11), //  1.62858201153657823623e-02
};

constexpr double pi_o_4 = fromBits(0x3FE921FB, 0x54442D18); // 7.8539816339744827900E-01
constexpr double pi_o_2 = fromBits(0x3FF921FB, 0x54442D18); // 1.5707963267948965580E+00
constexpr double pi_lo = fromBits(0x3CA1A626, 0x33145C07);  // 1.2246467991473531772E-16

} // namespace

double StrictMath::asin(double x) noexcept {
	double t, w, p, q, c, r, s;
	const int32_t hx = highWord(x);
	const int32_t ix = hx & 0x7fffffff;
	if (ix >= 0x3ff00000) {                                            // |x| >= 1
		if (((ix - 0x3ff00000) | static_cast<int32_t>(lowWord(x))) == 0) // asin(1) = +-pi/2 with inexact
			return x * pio2_hi + x * pio2_lo;
		return (x - x) / (x - x);   // asin(|x| > 1) is NaN
	} else if (ix < 0x3fe00000) { // |x| < 0.5
		if (ix < 0x3e400000) {      // if |x| < 2**-27
			if (huge + x > one)
				return x; // return x with inexact if x != 0
		}
		t = x * x;
		p = t * (pS0 + t * (pS1 + t * (pS2 + t * (pS3 + t * (pS4 + t * pS5)))));
		q = one + t * (qS1 + t * (qS2 + t * (qS3 + t * qS4)));
		w = p / q;
		return x + x * w;
	}
	// 1 > |x| >= 0.5
	w = one - std::fabs(x);
	t = w * 0.5;
	p = t * (pS0 + t * (pS1 + t * (pS2 + t * (pS3 + t * (pS4 + t * pS5)))));
	q = one + t * (qS1 + t * (qS2 + t * (qS3 + t * qS4)));
	s = std::sqrt(t);
	if (ix >= 0x3FEF3333) { // if |x| > 0.975
		w = p / q;
		t = pio2_hi - (2.0 * (s + s * w) - pio2_lo);
	} else {
		w = withLowWord(s, 0);
		c = (t - w * w) / (s + w);
		r = p / q;
		p = 2.0 * s * r - (pio2_lo - 2.0 * c);
		q = pio4_hi - 2.0 * w;
		t = pio4_hi - (p - q);
	}
	return hx > 0 ? t : -t;
}

double StrictMath::acos(double x) noexcept {
	double z, p, q, r, w, s, c, df;
	const int32_t hx = highWord(x);
	const int32_t ix = hx & 0x7fffffff;
	if (ix >= 0x3ff00000) {                                              // |x| >= 1
		if (((ix - 0x3ff00000) | static_cast<int32_t>(lowWord(x))) == 0) { // |x| == 1
			if (hx > 0)
				return 0.0;              // acos(1) = 0
			return pi + 2.0 * pio2_lo; // acos(-1) = pi
		}
		return (x - x) / (x - x); // acos(|x| > 1) is NaN
	}
	if (ix < 0x3fe00000) { // |x| < 0.5
		if (ix <= 0x3c600000)
			return pio2_hi + pio2_lo; // if |x| < 2**-57
		z = x * x;
		p = z * (pS0 + z * (pS1 + z * (pS2 + z * (pS3 + z * (pS4 + z * pS5)))));
		q = one + z * (qS1 + z * (qS2 + z * (qS3 + z * qS4)));
		r = p / q;
		return pio2_hi - (x - (pio2_lo - x * r));
	} else if (hx < 0) { // x < -0.5
		z = (one + x) * 0.5;
		p = z * (pS0 + z * (pS1 + z * (pS2 + z * (pS3 + z * (pS4 + z * pS5)))));
		q = one + z * (qS1 + z * (qS2 + z * (qS3 + z * qS4)));
		s = std::sqrt(z);
		r = p / q;
		w = r * s - pio2_lo;
		return pi - 2.0 * (s + w);
	} else { // x > 0.5
		z = (one - x) * 0.5;
		s = std::sqrt(z);
		df = withLowWord(s, 0);
		c = (z - df * df) / (s + df);
		p = z * (pS0 + z * (pS1 + z * (pS2 + z * (pS3 + z * (pS4 + z * pS5)))));
		q = one + z * (qS1 + z * (qS2 + z * (qS3 + z * qS4)));
		r = p / q;
		w = r * s + c;
		return 2.0 * (df + w);
	}
}

double StrictMath::atan(double x) noexcept {
	double w, s1, s2, z;
	int32_t id;
	const int32_t hx = highWord(x);
	const int32_t ix = hx & 0x7fffffff;
	if (ix >= 0x44100000) { // if |x| >= 2^66
		if (ix > 0x7ff00000 || (ix == 0x7ff00000 && lowWord(x) != 0))
			return x + x; // NaN
		if (hx > 0)
			return atanhi[3] + atanlo[3];
		return -atanhi[3] - atanlo[3];
	}
	if (ix < 0x3fdc0000) {   // |x| < 0.4375
		if (ix < 0x3e200000) { // |x| < 2^-29
			if (huge + x > one)
				return x; // raise inexact
		}
		id = -1;
	} else {
		x = std::fabs(x);
		if (ix < 0x3ff30000) {   // |x| < 1.1875
			if (ix < 0x3fe60000) { // 7/16 <= |x| < 11/16
				id = 0;
				x = (2.0 * x - one) / (2.0 + x);
			} else { // 11/16 <= |x| < 19/16
				id = 1;
				x = (x - one) / (x + one);
			}
		} else {
			if (ix < 0x40038000) { // |x| < 2.4375
				id = 2;
				x = (x - 1.5) / (one + 1.5 * x);
			} else { // 2.4375 <= |x| < 2^66
				id = 3;
				x = -1.0 / x;
			}
		}
	}
	// end of argument reduction
	z = x * x;
	w = z * z;
	// break sum from i=0 to 10 aT[i]z**(i+1) into odd and even poly
	s1 = z * (aT[0] + w * (aT[2] + w * (aT[4] + w * (aT[6] + w * (aT[8] + w * aT[10])))));
	s2 = w * (aT[1] + w * (aT[3] + w * (aT[5] + w * (aT[7] + w * aT[9]))));
	if (id < 0)
		return x - x * (s1 + s2);
	z = atanhi[id] - ((x * (s1 + s2) - atanlo[id]) - x);
	return hx < 0 ? -z : z;
}

double StrictMath::atan2(double y, double x) noexcept {
	double z;
	const int32_t hx = highWord(x);
	const int32_t ix = hx & 0x7fffffff;
	const uint32_t lx = lowWord(x);
	const int32_t hy = highWord(y);
	const int32_t iy = hy & 0x7fffffff;
	const uint32_t ly = lowWord(y);
	if ((static_cast<uint32_t>(ix) | ((lx | (0u - lx)) >> 31)) > 0x7ff00000u || (static_cast<uint32_t>(iy) | ((ly | (0u - ly)) >> 31)) > 0x7ff00000u)
		return x + y;                                            // x or y is NaN
	if (((static_cast<uint32_t>(hx) - 0x3ff00000u) | lx) == 0) // (Java int arithmetic wraps; unsigned avoids signed overflow)
		return atan(y);                                          // x = 1.0
	const int32_t m = ((hy >> 31) & 1) | ((hx >> 30) & 2);     // 2*sign(x)+sign(y)

	// when y = 0
	if ((static_cast<uint32_t>(iy) | ly) == 0) {
		switch (m) {
			case 0:
			case 1:
				return y; // atan(+-0,+anything)=+-0
			case 2:
				return pi + tiny; // atan(+0,-anything) = pi
			default:
				return -pi - tiny; // atan(-0,-anything) =-pi
		}
	}
	// when x = 0
	if ((static_cast<uint32_t>(ix) | lx) == 0)
		return hy < 0 ? -pi_o_2 - tiny : pi_o_2 + tiny;

	// when x is INF
	if (ix == 0x7ff00000) {
		if (iy == 0x7ff00000) {
			switch (m) {
				case 0:
					return pi_o_4 + tiny; // atan(+INF,+INF)
				case 1:
					return -pi_o_4 - tiny; // atan(-INF,+INF)
				case 2:
					return 3.0 * pi_o_4 + tiny; // atan(+INF,-INF)
				default:
					return -3.0 * pi_o_4 - tiny; // atan(-INF,-INF)
			}
		} else {
			switch (m) {
				case 0:
					return 0.0; // atan(+...,+INF)
				case 1:
					return -0.0; // atan(-...,+INF)
				case 2:
					return pi + tiny; // atan(+...,-INF)
				default:
					return -pi - tiny; // atan(-...,-INF)
			}
		}
	}
	// when y is INF
	if (iy == 0x7ff00000)
		return hy < 0 ? -pi_o_2 - tiny : pi_o_2 + tiny;

	// compute y/x
	const int32_t k = (iy - ix) >> 20;
	if (k > 60) // |y/x| > 2**60
		z = pi_o_2 + 0.5 * pi_lo;
	else if (hx < 0 && k < -60) // |y|/x < -2**60
		z = 0.0;
	else // safe to do y/x
		z = atan(std::fabs(y / x));
	switch (m) {
		case 0:
			return z; // atan(+,+)
		case 1:
			return withHighWord(z, static_cast<uint32_t>(highWord(z)) ^ 0x80000000u); // atan(-,+)
		case 2:
			return pi - (z - pi_lo); // atan(+,-)
		default:
			return (z - pi_lo) - pi; // atan(-,-)
	}
}

} // namespace aion::gameserver::geoEngine::math
