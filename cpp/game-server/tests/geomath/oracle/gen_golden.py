#!/usr/bin/env python3
"""Float32 oracle for the geo math port (P4-03): an independent Python model of the Java semantics of the jME math classes.

The Java code cannot be run, so this script re-implements the used methods of Vector3f, Vector2f, Matrix3f, Matrix4f, Ray, FastMath and
StrictMath (fdlibm) from the Java sources, emulating Java float arithmetic: every float operation is evaluated in double and rounded to
float32 (exact for +, -, * since the exact results fit into a double; / and sqrt are correctly rounded doubles rounded again), int operands are
promoted to float, float/double mixes are evaluated in double, and / follows IEEE 754 for zero divisors.

It writes ../GoldenVectors.gen.h (committed). The C++ tests compare the C++ results bit for bit.

Usage (Python 3.12 stdlib only, deterministic):  python gen_golden.py            regenerate the header
                                                python gen_golden.py --check    fail if the committed header differs
"""

import ctypes
import math
import pathlib
import random
import struct
import sys

INF = float("inf")
NAN = float("nan")


def f32(x: float) -> float:
    """Rounds a double to float32 (round to nearest even, overflow to infinity)."""
    return ctypes.c_float(x).value


def jdiv(a: float, b: float) -> float:
    """IEEE 754 double division (Python raises on zero divisors)."""
    if b == 0.0:
        if a != a or a == 0.0:
            return NAN
        return math.copysign(INF, a) * math.copysign(1.0, b)
    return a / b


def jsqrt(d: float) -> float:
    if d != d or d < 0.0:
        return NAN
    return math.sqrt(d)


class F:
    """A Java float. F op F and F op int give F; F op Python float (a Java double) gives a Python float."""

    __slots__ = ("v",)

    def __init__(self, v):
        self.v = f32(float(v))

    @staticmethod
    def _other(o):
        if isinstance(o, F):
            return o.v, True
        if isinstance(o, bool):
            raise TypeError("bool operand")
        if isinstance(o, int):
            return f32(float(o)), True
        if isinstance(o, float):
            return o, False
        raise TypeError(type(o))

    def _bin(self, o, op, reverse=False):
        ov, is_float = F._other(o)
        a, b = (ov, self.v) if reverse else (self.v, ov)
        r = op(a, b)
        return F(r) if is_float else r

    def __add__(self, o):
        return self._bin(o, lambda a, b: a + b)

    def __radd__(self, o):
        return self._bin(o, lambda a, b: a + b, True)

    def __sub__(self, o):
        return self._bin(o, lambda a, b: a - b)

    def __rsub__(self, o):
        return self._bin(o, lambda a, b: a - b, True)

    def __mul__(self, o):
        return self._bin(o, lambda a, b: a * b)

    def __rmul__(self, o):
        return self._bin(o, lambda a, b: a * b, True)

    def __truediv__(self, o):
        return self._bin(o, jdiv)

    def __rtruediv__(self, o):
        return self._bin(o, jdiv, True)

    def __neg__(self):
        return F(-self.v)

    def __pos__(self):
        return self

    def __lt__(self, o):
        return self.v < F._other(o)[0]

    def __le__(self, o):
        return self.v <= F._other(o)[0]

    def __gt__(self, o):
        return self.v > F._other(o)[0]

    def __ge__(self, o):
        return self.v >= F._other(o)[0]

    def __eq__(self, o):
        return self.v == F._other(o)[0]

    def __ne__(self, o):
        return self.v != F._other(o)[0]

    __hash__ = None


def fbits(x) -> int:
    v = x.v if isinstance(x, F) else f32(x)
    return struct.unpack("<I", struct.pack("<f", v))[0]


def float_to_int_bits(x: F) -> int:
    if x.v != x.v:
        return 0x7FC00000
    return struct.unpack("<i", struct.pack("<f", x.v))[0]


def dbits(d: float) -> int:
    return struct.unpack("<Q", struct.pack("<d", d))[0]


def from_dbits(bits: int) -> float:
    return struct.unpack("<d", struct.pack("<Q", bits))[0]


def java_int(x: int) -> int:
    x &= 0xFFFFFFFF
    return x - (1 << 32) if x >= (1 << 31) else x


# ------------------------------------------------------------------------------------------------------------------------------------------
# StrictMath (fdlibm 5.3 as in java.lang.FdLibm)
# ------------------------------------------------------------------------------------------------------------------------------------------

def hi(x: float) -> int:
    return java_int(dbits(x) >> 32)


def lo(x: float) -> int:
    return dbits(x) & 0xFFFFFFFF


def with_lo(x: float, low: int) -> float:
    return from_dbits((dbits(x) & 0xFFFFFFFF00000000) | low)


def with_hi(x: float, high: int) -> float:
    return from_dbits((dbits(x) & 0xFFFFFFFF) | ((high & 0xFFFFFFFF) << 32))


def hexd(h, l):
    return from_dbits((h << 32) | l)


PI_D = hexd(0x400921FB, 0x54442D18)
PIO2_HI = hexd(0x3FF921FB, 0x54442D18)
PIO2_LO = hexd(0x3C91A626, 0x33145C07)
PIO4_HI = hexd(0x3FE921FB, 0x54442D18)
PS = [1.66666666666666657415e-01, -3.25565818622400915405e-01, 2.01212532134862925881e-01, -4.00555345006794114027e-02,
      7.91534994289814532176e-04, 3.47933107596021167570e-05]
QS = [None, -2.40339491173441421878e+00, 2.02094576023350569471e+00, -6.88283971605453293030e-01, 7.70381505559019352791e-02]
ATANHI = [4.63647609000806093515e-01, 7.85398163397448278999e-01, 9.82793723247329054082e-01, 1.57079632679489655800e+00]
ATANLO = [2.26987774529616870924e-17, 3.06161699786838301793e-17, 1.39033110312309984516e-17, 6.12323399573676603587e-17]
AT = [3.33333333333329318027e-01, -1.99999999998764832476e-01, 1.42857142725034663711e-01, -1.11111104054623557880e-01,
      9.09088713343650656196e-02, -7.69187620504482999495e-02, 6.66107313738753120669e-02, -5.83357013379057348645e-02,
      4.97687799461593236017e-02, -3.65315727442169155270e-02, 1.62858201153657823623e-02]
PI_O_4 = 7.8539816339744827900E-01
PI_O_2 = 1.5707963267948965580E+00
PI_LO = 1.2246467991473531772E-16
HUGE = 1.0e300
TINY = 1.0e-300


def _pq(t):
    p = t * (PS[0] + t * (PS[1] + t * (PS[2] + t * (PS[3] + t * (PS[4] + t * PS[5])))))
    q = 1.0 + t * (QS[1] + t * (QS[2] + t * (QS[3] + t * QS[4])))
    return p, q


def s_asin(x: float) -> float:
    hx = hi(x)
    ix = hx & 0x7FFFFFFF
    if ix >= 0x3FF00000:
        if ((ix - 0x3FF00000) | lo(x)) == 0:
            return x * PIO2_HI + x * PIO2_LO
        return NAN
    elif ix < 0x3FE00000:
        if ix < 0x3E400000:
            if HUGE + x > 1.0:
                return x
        t = x * x
        p, q = _pq(t)
        w = p / q
        return x + x * w
    w = 1.0 - abs(x)
    t = w * 0.5
    p, q = _pq(t)
    s = math.sqrt(t)
    if ix >= 0x3FEF3333:
        w = p / q
        t = PIO2_HI - (2.0 * (s + s * w) - PIO2_LO)
    else:
        w = with_lo(s, 0)
        c = (t - w * w) / (s + w)
        r = p / q
        p = 2.0 * s * r - (PIO2_LO - 2.0 * c)
        q = PIO4_HI - 2.0 * w
        t = PIO4_HI - (p - q)
    return t if hx > 0 else -t


def s_acos(x: float) -> float:
    hx = hi(x)
    ix = hx & 0x7FFFFFFF
    if ix >= 0x3FF00000:
        if ((ix - 0x3FF00000) | lo(x)) == 0:
            if hx > 0:
                return 0.0
            return PI_D + 2.0 * PIO2_LO
        return NAN
    if ix < 0x3FE00000:
        if ix <= 0x3C600000:
            return PIO2_HI + PIO2_LO
        z = x * x
        p, q = _pq(z)
        r = p / q
        return PIO2_HI - (x - (PIO2_LO - x * r))
    elif hx < 0:
        z = (1.0 + x) * 0.5
        p, q = _pq(z)
        s = math.sqrt(z)
        r = p / q
        w = r * s - PIO2_LO
        return PI_D - 2.0 * (s + w)
    else:
        z = (1.0 - x) * 0.5
        s = math.sqrt(z)
        df = with_lo(s, 0)
        c = (z - df * df) / (s + df)
        p, q = _pq(z)
        r = p / q
        w = r * s + c
        return 2.0 * (df + w)


def s_atan(x: float) -> float:
    hx = hi(x)
    ix = hx & 0x7FFFFFFF
    if ix >= 0x44100000:
        if ix > 0x7FF00000 or (ix == 0x7FF00000 and lo(x) != 0):
            return x + x
        if hx > 0:
            return ATANHI[3] + ATANLO[3]
        return -ATANHI[3] - ATANLO[3]
    if ix < 0x3FDC0000:
        if ix < 0x3E200000:
            if HUGE + x > 1.0:
                return x
        idx = -1
    else:
        x = abs(x)
        if ix < 0x3FF30000:
            if ix < 0x3FE60000:
                idx = 0
                x = (2.0 * x - 1.0) / (2.0 + x)
            else:
                idx = 1
                x = (x - 1.0) / (x + 1.0)
        else:
            if ix < 0x40038000:
                idx = 2
                x = (x - 1.5) / (1.0 + 1.5 * x)
            else:
                idx = 3
                x = -1.0 / x
    z = x * x
    w = z * z
    s1 = z * (AT[0] + w * (AT[2] + w * (AT[4] + w * (AT[6] + w * (AT[8] + w * AT[10])))))
    s2 = w * (AT[1] + w * (AT[3] + w * (AT[5] + w * (AT[7] + w * AT[9]))))
    if idx < 0:
        return x - x * (s1 + s2)
    z = ATANHI[idx] - ((x * (s1 + s2) - ATANLO[idx]) - x)
    return -z if hx < 0 else z


def s_atan2(y: float, x: float) -> float:
    hx = hi(x)
    ix = hx & 0x7FFFFFFF
    lx = lo(x)
    hy = hi(y)
    iy = hy & 0x7FFFFFFF
    ly = lo(y)
    if x != x or y != y:
        return x + y
    if ((hx - 0x3FF00000) & 0xFFFFFFFF | lx) == 0:
        return s_atan(y)
    m = ((hy >> 31) & 1) | ((hx >> 30) & 2)
    if (iy | ly) == 0:
        if m in (0, 1):
            return y
        return PI_D + TINY if m == 2 else -PI_D - TINY
    if (ix | lx) == 0:
        return -PI_O_2 - TINY if hy < 0 else PI_O_2 + TINY
    if ix == 0x7FF00000:
        if iy == 0x7FF00000:
            return [PI_O_4 + TINY, -PI_O_4 - TINY, 3.0 * PI_O_4 + TINY, -3.0 * PI_O_4 - TINY][m]
        return [0.0, -0.0, PI_D + TINY, -PI_D - TINY][m]
    if iy == 0x7FF00000:
        return -PI_O_2 - TINY if hy < 0 else PI_O_2 + TINY
    k = (iy - ix) >> 20
    if k > 60:
        z = PI_O_2 + 0.5 * PI_LO
    elif hx < 0 and k < -60:
        z = 0.0
    else:
        z = s_atan(abs(y / x))
    if m == 0:
        return z
    if m == 1:
        return with_hi(z, hi(z) ^ 0x80000000)
    if m == 2:
        return PI_D - (z - PI_LO)
    return (z - PI_LO) - PI_D


def next_up(d):
    return math.nextafter(d, INF)


def within_one_ulp(a: float, b: float) -> bool:
    if a != a or b != b:
        return a != a and b != b
    return a == b or math.nextafter(a, INF) == b or math.nextafter(a, -INF) == b


# ------------------------------------------------------------------------------------------------------------------------------------------
# FastMath
# ------------------------------------------------------------------------------------------------------------------------------------------

PI = F(math.pi)
TWO_PI = F(2) * PI
HALF_PI = F(0.5) * PI
QUARTER_PI = F(0.25) * PI
INV_PI = F(1) / PI
INV_TWO_PI = F(1) / TWO_PI
DEG_TO_RAD = PI / F(180)
RAD_TO_DEG = F(180) / PI
ONE_THIRD = F(1) / F(3)
FLT_EPSILON = F(1.1920928955078125E-7)
ZERO_TOLERANCE = F(0.0001)


def fm_abs(v: F) -> F:
    return -v if v < 0 else v


def fm_sqrt(v: F) -> F:
    return F(jsqrt(v.v))


def fm_inv_sqrt(v: F) -> F:
    return F(jdiv(1.0, jsqrt(v.v)))


def fm_fast_inv_sqrt(x: F) -> F:
    xhalf = F(0.5) * x
    i = float_to_int_bits(x)
    i = java_int(0x5F375A86 - (i >> 1))
    x = F(struct.unpack("<f", struct.pack("<i", i))[0])
    return x * (F(1.5) - xhalf * x * x)


def fm_acos(v: F) -> F:
    if F(-1) < v:
        if v < F(1):
            return F(s_acos(v.v))
        return F(0)
    return PI


def fm_asin(v: F) -> F:
    if F(-1) < v:
        if v < F(1):
            return F(s_asin(v.v))
        return HALF_PI
    return -HALF_PI


def fm_atan(v: F) -> F:
    return F(s_atan(v.v))


def fm_atan2(y: F, x: F) -> F:
    return F(s_atan2(y.v, x.v))


def java_fmod(a: float, b: float) -> float:
    if a != a or b != b or math.isinf(a) or b == 0.0:
        return NAN
    if math.isinf(b):
        return a
    return math.fmod(a, b)


def fm_reduce_sin_angle(r: F) -> F:
    r = F(java_fmod(r.v, TWO_PI.v))
    if F(abs(r.v)) > PI:
        r = r - TWO_PI
    if F(abs(r.v)) > HALF_PI:
        r = PI - r
    return r


def fm_interpolate_linear(scale: F, start: F, end: F) -> F:
    if start == end:
        return start
    if scale <= F(0):
        return start
    if scale >= F(1):
        return end
    return ((F(1) - scale) * start) + (scale * end)


def fm_catmull_rom(u: F, T: F, p0: F, p1: F, p2: F, p3: F) -> F:
    c1 = p1.v
    c2 = (-1.0 * T) * p0 + T * p2                           # double + float -> double
    c3 = (2 * T * p0 + (T - 3) * p1 + (3 - 2 * T) * p2 + -T * p3).v   # float expression
    c4 = (-T * p0 + (2 - T) * p1 + (T - 2) * p2 + T * p3).v
    return F(((c4 * u.v + c3) * u.v + c2) * u.v + c1)


def fm_determinant(m):
    m00, m01, m02, m03, m10, m11, m12, m13, m20, m21, m22, m23, m30, m31, m32, m33 = m
    det01 = m20 * m31 - m21 * m30
    det02 = m20 * m32 - m22 * m30
    det03 = m20 * m33 - m23 * m30
    det12 = m21 * m32 - m22 * m31
    det13 = m21 * m33 - m23 * m31
    det23 = m22 * m33 - m23 * m32
    return F(m00 * (m11 * det23 - m12 * det13 + m13 * det12) - m01 * (m10 * det23 - m12 * det03 + m13 * det02)
             + m02 * (m10 * det13 - m11 * det03 + m13 * det01) - m03 * (m10 * det12 - m11 * det02 + m12 * det01))


def fm_normalize(val: F, mn: F, mx: F) -> F:
    if math.isinf(val.v) or val.v != val.v:
        return F(0)
    rng = mx - mn
    while val > mx:
        val = val - rng
    while val < mn:
        val = val + rng
    return val


def fm_copysign(x: F, y: F) -> F:
    if y >= 0 and x <= 0:
        return -x
    elif y < 0 and x >= 0:
        return -x
    return x


def fm_float_to_half(flt: F) -> int:
    v = flt.v
    if v == INF:
        return 0x7C00
    if v == -INF:
        return java_int(0xFC00) & 0xFFFF
    if v == 0:
        return 0
    if v > 65504.0:
        return 0x7BFF
    if v < -65504.0:
        return 0xFBFF
    if 0 < v < f32(5.96046E-8):
        return 0x0001
    if 0 > v > -f32(5.96046E-8):
        return 0x8001
    f = float_to_int_bits(flt)
    return (((f >> 16) & 0x8000) | ((((f & 0x7F800000) - 0x38000000) >> 13) & 0x7C00) | ((f >> 13) & 0x03FF)) & 0xFFFF


def fm_half_to_float(half: int) -> F:
    if half == 0x0000:
        return F(0.0)
    if half == 0x8000:
        return F(-0.0)
    if half == 0x7C00:
        return F(INF)
    if half == 0xFC00:
        return F(-INF)
    bits = ((half & 0x8000) << 16) | (((half & 0x7C00) + 0x1C000) << 13) | ((half & 0x03FF) << 13)
    return F(struct.unpack("<f", struct.pack("<I", bits & 0xFFFFFFFF))[0])


def fm_ccw(p0, p1, p2) -> int:
    dx1 = p1[0] - p0[0]
    dy1 = p1[1] - p0[1]
    dx2 = p2[0] - p0[0]
    dy2 = p2[1] - p0[1]
    if dx1 * dy2 > dy1 * dx2:
        return 1
    if dx1 * dy2 < dy1 * dx2:
        return -1
    if dx1 * dx2 < 0 or dy1 * dy2 < 0:
        return -1
    if (dx1 * dx1 + dy1 * dy1) < (dx2 * dx2 + dy2 * dy2):
        return 1
    return 0


def fm_point_inside_triangle(t0, t1, t2, p) -> int:
    val1 = fm_ccw(t0, t1, p)
    if val1 == 0:
        return 1
    val2 = fm_ccw(t1, t2, p)
    if val2 == 0:
        return 1
    if val2 != val1:
        return 0
    val3 = fm_ccw(t2, t0, p)
    if val3 == 0:
        return 1
    if val3 != val1:
        return 0
    return val3


# ------------------------------------------------------------------------------------------------------------------------------------------
# Vector3f (lists of three F, mutated in place like Java objects)
# ------------------------------------------------------------------------------------------------------------------------------------------

def v3(x, y, z):
    return [F(x), F(y), F(z)]


def v3_dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def v3_cross(a, b):
    return [(a[1] * b[2]) - (a[2] * b[1]), (a[2] * b[0]) - (a[0] * b[2]), (a[0] * b[1]) - (a[1] * b[0])]


def v3_length_squared(a):
    return a[0] * a[0] + a[1] * a[1] + a[2] * a[2]


def v3_length(a):
    return fm_sqrt(v3_length_squared(a))


def v3_distance_squared(a, b):
    dx = (a[0] - b[0]).v
    dy = (a[1] - b[1]).v
    dz = (a[2] - b[2]).v
    return F(dx * dx + dy * dy + dz * dz)


def v3_normalize_local(a):
    length = a[0] * a[0] + a[1] * a[1] + a[2] * a[2]
    if length != F(1) and length != F(0):
        length = F(1) / fm_sqrt(length)
        a[0] = a[0] * length
        a[1] = a[1] * length
        a[2] = a[2] * length
    return a


def v3_normalize(a):
    return v3_normalize_local(list(a))


def v3_divide(a, s):
    s = F(1) / s
    return [a[0] * s, a[1] * s, a[2] * s]


def v3_scale_add(a, s, add):
    return [a[0] * s + add[0], a[1] * s + add[1], a[2] * s + add[2]]


def v3_interpolate(a, final, amnt):
    return [(1 - amnt) * a[i] + amnt * final[i] for i in range(3)]


def v3_project(a, other):
    n = v3_dot(a, other)
    d = v3_length_squared(other)
    r = v3_normalize_local(list(other))
    s = n / d
    return [r[0] * s, r[1] * s, r[2] * s]


def v3_angle_between(a, b):
    return fm_acos(v3_dot(a, b))


def v3_hash(a):
    h = 37
    for c in a:
        h = java_int(h + 37 * h + float_to_int_bits(c))
    return h


def v3_complement_basis(w):
    u = [F(0), F(0), F(0)]
    v = [F(0), F(0), F(0)]
    if fm_abs(w[0]) >= fm_abs(w[1]):
        inv = fm_inv_sqrt(w[0] * w[0] + w[2] * w[2])
        u[0] = -w[2] * inv
        u[1] = F(0)
        u[2] = +w[0] * inv
        v[0] = w[1] * u[2]
        v[1] = w[2] * u[0] - w[0] * u[2]
        v[2] = -w[1] * u[0]
    else:
        inv = fm_inv_sqrt(w[1] * w[1] + w[2] * w[2])
        u[0] = F(0)
        u[1] = +w[2] * inv
        u[2] = -w[1] * inv
        v[0] = w[1] * u[2] - w[2] * u[1]
        v[1] = -w[0] * u[2]
        v[2] = w[0] * u[1]
    return u, v


# ------------------------------------------------------------------------------------------------------------------------------------------
# Vector2f
# ------------------------------------------------------------------------------------------------------------------------------------------

def v2_length(a):
    return fm_sqrt(a[0] * a[0] + a[1] * a[1])


def v2_normalize(a):
    length = v2_length(a)
    if length != 0:
        return [a[0] / length, a[1] / length]
    return [a[0] / F(1), a[1] / F(1)]


def v2_distance_squared(a, b):
    dx = (a[0] - b[0]).v
    dy = (a[1] - b[1]).v
    return F(dx * dx + dy * dy)


def v2_hash(a):
    h = 37
    for c in a:
        h = java_int(h + 37 * h + float_to_int_bits(c))
    return h


# ------------------------------------------------------------------------------------------------------------------------------------------
# Matrix3f / Matrix4f (row-major lists of F)
# ------------------------------------------------------------------------------------------------------------------------------------------

def m3_mult(a, b):
    r = []
    for i in range(3):
        for j in range(3):
            r.append(a[i * 3] * b[j] + a[i * 3 + 1] * b[3 + j] + a[i * 3 + 2] * b[6 + j])
    return r


def m3_mult_vec(a, v):
    return [a[i * 3] * v[0] + a[i * 3 + 1] * v[1] + a[i * 3 + 2] * v[2] for i in range(3)]


def m3_determinant(m):
    m00, m01, m02, m10, m11, m12, m20, m21, m22 = m
    co00 = m11 * m22 - m12 * m21
    co10 = m12 * m20 - m10 * m22
    co20 = m10 * m21 - m11 * m20
    return m00 * co00 + m01 * co10 + m02 * co20


def m3_adjoint(m):
    m00, m01, m02, m10, m11, m12, m20, m21, m22 = m
    return [m11 * m22 - m12 * m21, m02 * m21 - m01 * m22, m01 * m12 - m02 * m11,
            m12 * m20 - m10 * m22, m00 * m22 - m02 * m20, m02 * m10 - m00 * m12,
            m10 * m21 - m11 * m20, m01 * m20 - m00 * m21, m00 * m11 - m01 * m10]


def m3_invert(m):
    det = m3_determinant(m)
    if fm_abs(det) <= FLT_EPSILON:
        return [F(0)] * 9
    s = F(1) / det
    return [e * s for e in m3_adjoint(m)]


def m3_hash(m):
    h = 37
    for e in m:
        h = java_int(37 * h + float_to_int_bits(e))
    return h


def m3_from_start_end(start, end):
    mat = [F(1), F(0), F(0), F(0), F(1), F(0), F(0), F(0), F(1)]
    v = v3_cross(start, end)
    e = v3_dot(start, end)
    f = -e if e < 0 else e
    if f > F(1) - ZERO_TOLERANCE:
        x = [start[0] if start[0].v > 0.0 else -start[0], start[1] if start[1].v > 0.0 else -start[1],
             start[2] if start[2].v > 0.0 else -start[2]]
        if x[0] < x[1]:
            if x[0] < x[2]:
                x = [F(1), F(0), F(0)]
            else:
                x = [F(0), F(0), F(1)]
        else:
            if x[1] < x[2]:
                x = [F(0), F(1), F(0)]
            else:
                x = [F(0), F(0), F(1)]
        u = [x[i] - start[i] for i in range(3)]
        v = [x[i] - end[i] for i in range(3)]
        c1 = F(2) / v3_dot(u, u)
        c2 = F(2) / v3_dot(v, v)
        c3 = c1 * c2 * v3_dot(u, v)
        for i in range(3):
            for j in range(3):
                mat[i * 3 + j] = -c1 * u[i] * u[j] - c2 * v[i] * v[j] + c3 * v[i] * u[j]
            mat[i * 3 + i] = mat[i * 3 + i] + F(1)
    else:
        h = F(1) / (F(1) + e)
        hvx = h * v[0]
        hvz = h * v[2]
        hvxy = hvx * v[1]
        hvxz = hvx * v[2]
        hvyz = hvz * v[1]
        mat = [e + hvx * v[0], hvxy - v[2], hvxz + v[1],
               hvxy + v[2], e + h * v[1] * v[1], hvyz - v[0],
               hvxz - v[1], hvyz + v[0], e + hvz * v[2]]
    return mat


def m4_mult(a, b):
    r = []
    for i in range(4):
        for j in range(4):
            r.append(a[i * 4] * b[j] + a[i * 4 + 1] * b[4 + j] + a[i * 4 + 2] * b[8 + j] + a[i * 4 + 3] * b[12 + j])
    return r


def m4_mult_vec(m, v):
    return [m[i * 4] * v[0] + m[i * 4 + 1] * v[1] + m[i * 4 + 2] * v[2] + m[i * 4 + 3] for i in range(3)]


def m4_mult_normal(m, v):
    return [m[i * 4] * v[0] + m[i * 4 + 1] * v[1] + m[i * 4 + 2] * v[2] for i in range(3)]


def m4_mult_normal_across(m, v):
    return [m[i] * v[0] + m[4 + i] * v[1] + m[8 + i] * v[2] for i in range(3)]


def m4_mult_proj(m, v):
    return m4_mult_vec(m, v), m[12] * v[0] + m[13] * v[1] + m[14] * v[2] + m[15]


def m4_mult_across(m, v):
    return [m[i] * v[0] + m[4 + i] * v[1] + m[8 + i] * v[2] + m[12 + i] * 1 for i in range(3)]


def m4_cofactors(m):
    m00, m01, m02, m03, m10, m11, m12, m13, m20, m21, m22, m23, m30, m31, m32, m33 = m
    fA0 = m00 * m11 - m01 * m10
    fA1 = m00 * m12 - m02 * m10
    fA2 = m00 * m13 - m03 * m10
    fA3 = m01 * m12 - m02 * m11
    fA4 = m01 * m13 - m03 * m11
    fA5 = m02 * m13 - m03 * m12
    fB0 = m20 * m31 - m21 * m30
    fB1 = m20 * m32 - m22 * m30
    fB2 = m20 * m33 - m23 * m30
    fB3 = m21 * m32 - m22 * m31
    fB4 = m21 * m33 - m23 * m31
    fB5 = m22 * m33 - m23 * m32
    det = fA0 * fB5 - fA1 * fB4 + fA2 * fB3 + fA3 * fB2 - fA4 * fB1 + fA5 * fB0
    s = [None] * 16
    s[0] = +m11 * fB5 - m12 * fB4 + m13 * fB3
    s[4] = -m10 * fB5 + m12 * fB2 - m13 * fB1
    s[8] = +m10 * fB4 - m11 * fB2 + m13 * fB0
    s[12] = -m10 * fB3 + m11 * fB1 - m12 * fB0
    s[1] = -m01 * fB5 + m02 * fB4 - m03 * fB3
    s[5] = +m00 * fB5 - m02 * fB2 + m03 * fB1
    s[9] = -m00 * fB4 + m01 * fB2 - m03 * fB0
    s[13] = +m00 * fB3 - m01 * fB1 + m02 * fB0
    s[2] = +m31 * fA5 - m32 * fA4 + m33 * fA3
    s[6] = -m30 * fA5 + m32 * fA2 - m33 * fA1
    s[10] = +m30 * fA4 - m31 * fA2 + m33 * fA0
    s[14] = -m30 * fA3 + m31 * fA1 - m32 * fA0
    s[3] = -m21 * fA5 + m22 * fA4 - m23 * fA3
    s[7] = +m20 * fA5 - m22 * fA2 + m23 * fA1
    s[11] = -m20 * fA4 + m21 * fA2 - m23 * fA0
    s[15] = +m20 * fA3 - m21 * fA1 + m22 * fA0
    return det, s


def m4_invert(m):
    det, s = m4_cofactors(m)
    if fm_abs(det) <= F(0):
        return None
    inv = F(1) / det
    return [e * inv for e in s]


def m4_hash(m):
    h = 37
    for e in m:
        h = java_int(37 * h + float_to_int_bits(e))
    return h


def m4_set_transform(rot, scale, loc):
    """Geometry.setTransform: loadIdentity, setRotationMatrix, scale(Vector3f), setTranslation."""
    m = [F(1), F(0), F(0), F(0), F(0), F(1), F(0), F(0), F(0), F(0), F(1), F(0), F(0), F(0), F(0), F(1)]
    for i in range(3):
        for j in range(3):
            m[i * 4 + j] = rot[i * 3 + j]
    for col in range(3):
        for row in range(4):
            m[row * 4 + col] = m[row * 4 + col] * scale[col]
    m[3], m[7], m[11] = loc
    return m


# ------------------------------------------------------------------------------------------------------------------------------------------
# Ray
# ------------------------------------------------------------------------------------------------------------------------------------------

def ray_intersects_t(origin, direction, v0, v1, v2):
    e1x, e1y, e1z = v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2]
    e2x, e2y, e2z = v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2]
    nx = (e1y * e2z) - (e1z * e2y)
    ny = (e1z * e2x) - (e1x * e2z)
    nz = (e1x * e2y) - (e1y * e2x)
    dirDotNorm = direction[0] * nx + direction[1] * ny + direction[2] * nz
    dx, dy, dz = origin[0] - v0[0], origin[1] - v0[1], origin[2] - v0[2]
    if dirDotNorm > FLT_EPSILON:
        sign = F(1)
    elif dirDotNorm < -FLT_EPSILON:
        sign = F(-1)
        dirDotNorm = -dirDotNorm
    else:
        return F(INF)
    ex = (dy * e2z) - (dz * e2y)
    ey = (dz * e2x) - (dx * e2z)
    ez = (dx * e2y) - (dy * e2x)
    dirDotDiffxEdge2 = sign * (direction[0] * ex + direction[1] * ey + direction[2] * ez)
    if dirDotDiffxEdge2 >= F(0):
        ex = (e1y * dz) - (e1z * dy)
        ey = (e1z * dx) - (e1x * dz)
        ez = (e1x * dy) - (e1y * dx)
        dirDotEdge1xDiff = sign * (direction[0] * ex + direction[1] * ey + direction[2] * ez)
        if dirDotEdge1xDiff >= F(0):
            if dirDotDiffxEdge2 + dirDotEdge1xDiff <= dirDotNorm:
                diffDotNorm = -sign * (dx * nx + dy * ny + dz * nz)
                if diffDotNorm >= F(0):
                    inv = F(1) / dirDotNorm
                    return diffDotNorm * inv
    return F(INF)


def ray_intersects_store(origin, direction, v0, v1, v2, do_planar, quad):
    """Returns (hit, store or None) of the private Ray.intersects with a non-null store."""
    diff = [origin[i] - v0[i] for i in range(3)]
    edge1 = [v1[i] - v0[i] for i in range(3)]
    edge2 = [v2[i] - v0[i] for i in range(3)]
    norm = v3_cross(edge1, edge2)
    dirDotNorm = v3_dot(direction, norm)
    if dirDotNorm > FLT_EPSILON:
        sign = F(1)
    elif dirDotNorm < -FLT_EPSILON:
        sign = F(-1)
        dirDotNorm = -dirDotNorm
    else:
        return False, None
    edge2 = v3_cross(diff, edge2)  # diff.cross(edge2, edge2)
    dirDotDiffxEdge2 = sign * v3_dot(direction, edge2)
    if dirDotDiffxEdge2 >= F(0):
        edge1 = v3_cross(edge1, diff)  # edge1.crossLocal(diff)
        dirDotEdge1xDiff = sign * v3_dot(direction, edge1)
        if dirDotEdge1xDiff >= F(0):
            ok = (dirDotDiffxEdge2 + dirDotEdge1xDiff <= dirDotNorm) if not quad else (dirDotEdge1xDiff <= dirDotNorm)
            if ok:
                diffDotNorm = -sign * v3_dot(diff, norm)
                if diffDotNorm >= F(0):
                    inv = F(1) / dirDotNorm
                    t = diffDotNorm * inv
                    if not do_planar:
                        return True, [origin[0] + direction[0] * t, origin[1] + direction[1] * t, origin[2] + direction[2] * t]
                    return True, [t, dirDotDiffxEdge2 * inv, dirDotEdge1xDiff * inv]
    return False, None


def ray_distance_squared(origin, direction, point):
    va = [point[i] - origin[i] for i in range(3)]
    rayParam = v3_dot(direction, va)
    if rayParam > 0:
        vb = [direction[i] * rayParam for i in range(3)]
        vb = [origin[i] + vb[i] for i in range(3)]
    else:
        vb = list(origin)
    va = [vb[i] - point[i] for i in range(3)]
    return v3_length_squared(va)


# ------------------------------------------------------------------------------------------------------------------------------------------
# Case generation
# ------------------------------------------------------------------------------------------------------------------------------------------

def rnd_float(r: random.Random) -> F:
    kind = r.randrange(6)
    if kind == 0:
        return F(r.uniform(-1.0, 1.0))
    if kind == 1:
        return F(r.uniform(0.0, 4096.0))
    if kind == 2:
        return F(r.uniform(-5000.0, 5000.0))
    if kind == 3:
        return F(math.copysign(10.0 ** r.uniform(-8.0, 8.0), r.uniform(-1.0, 1.0)))
    if kind == 4:
        return F(r.randint(-8, 8))
    return F(r.uniform(-50.0, 50.0))


def rnd_vec(r, n=3):
    return [rnd_float(r) for _ in range(n)]


class Emitter:
    def __init__(self):
        self.parts = []

    def struct(self, name, fields, cases, comment):
        """fields: list of (name, count, kind) with kind f (float bits), d (double bits), i (int32), b (bool)."""
        types = {"f": "uint32_t", "d": "uint64_t", "i": "int32_t", "b": "bool"}
        out = [f"// {comment}", f"struct {name} {{"]
        for fname, count, kind in fields:
            out.append(f"\t{types[kind]} {fname}" + (f"[{count}]" if count > 1 else "") + ";")
        out.append("};")
        out.append("")
        out.append(f"inline constexpr {name} {name}s[] = {{")
        for case in cases:
            vals = []
            for fname, count, kind in fields:
                v = case[fname]
                items = v if count > 1 else [v]
                if len(items) != count:
                    raise ValueError(f"{name}.{fname}: expected {count} values, got {len(items)}")
                rendered = []
                for item in items:
                    if kind == "f":
                        rendered.append(f"0x{fbits(item):08x}u")
                    elif kind == "d":
                        rendered.append(f"0x{dbits(item):016x}ull")
                    elif kind == "i":
                        rendered.append(str(int(item)) if item != -2147483648 else "(-2147483647 - 1)")
                    else:
                        rendered.append("true" if item else "false")
                vals.append(("{" + ", ".join(rendered) + "}") if count > 1 else rendered[0])
            out.append("\t{" + ", ".join(vals) + "},")
        out.append("};")
        out.append("")
        self.parts.append("\n".join(out))


def gen_vector3f(r, em):
    cases = []
    specials = [
        (v3(1, 0, 0), v3(0, 1, 0)), (v3(0, 0, 0), v3(3, 4, 12)), (v3(1, 2, 2), v3(1, 2, 2)), (v3(-0.0, 0.0, -0.0), v3(0.0, -0.0, 0.0)),
        (v3(1e30, -1e30, 1e30), v3(-1e30, 1e30, 3e38)), (v3(1e-30, 2e-30, -3e-30), v3(1e-40, 0, 1)),
    ]
    inputs = list(specials)
    while len(inputs) < 56:
        inputs.append((rnd_vec(r), rnd_vec(r)))
    for a, b in inputs:
        s = rnd_float(r)
        t = F(r.uniform(0.0, 1.0))
        na, nb = v3_normalize(a), v3_normalize(b)
        u, v = v3_complement_basis(na)
        cases.append({
            "a": a, "b": b, "s": s, "t": t,
            "dot": v3_dot(a, b), "cross": v3_cross(a, b), "length": v3_length(a), "lengthSquared": v3_length_squared(a),
            "distance": fm_sqrt(v3_distance_squared(a, b)), "distanceSquared": v3_distance_squared(a, b), "normalize": na,
            "divide": v3_divide(a, s), "scaleAdd": v3_scale_add(a, s, b), "interpolate": v3_interpolate(a, b, t),
            "project": v3_project(a, b), "angleBetween": v3_angle_between(na, nb), "hashCode": v3_hash(a), "basisU": u, "basisV": v,
        })
    em.struct("Vector3fCase", [("a", 3, "f"), ("b", 3, "f"), ("s", 1, "f"), ("t", 1, "f"), ("dot", 1, "f"), ("cross", 3, "f"),
                               ("length", 1, "f"), ("lengthSquared", 1, "f"), ("distance", 1, "f"), ("distanceSquared", 1, "f"),
                               ("normalize", 3, "f"), ("divide", 3, "f"), ("scaleAdd", 3, "f"), ("interpolate", 3, "f"), ("project", 3, "f"),
                               ("angleBetween", 1, "f"), ("hashCode", 1, "i"), ("basisU", 3, "f"), ("basisV", 3, "f")], cases,
              "Vector3f: a op b; angleBetween and the complement basis use normalize(a)/normalize(b); interpolate(a -> b, t); scaleAdd(s, b)")


def gen_vector2f(r, em):
    cases = []
    inputs = [([F(0), F(0)], [F(1), F(0)]), ([F(3), F(4)], [F(-4), F(3)]), ([F(-1), F(-0.0)], [F(-1), F(0.0)])]
    while len(inputs) < 40:
        inputs.append((rnd_vec(r, 2), rnd_vec(r, 2)))
    for a, b in inputs:
        na, nb = v2_normalize(a), v2_normalize(b)
        cases.append({
            "a": a, "b": b, "dot": a[0] * b[0] + a[1] * b[1], "determinant": (a[0] * b[1]) - (a[1] * b[0]), "length": v2_length(a),
            "distanceSquared": v2_distance_squared(a, b), "normalize": na, "getAngle": -fm_atan2(a[1], a[0]),
            "angleBetween": fm_atan2(b[1], b[0]) - fm_atan2(a[1], a[0]), "smallestAngleBetween": fm_acos(na[0] * nb[0] + na[1] * nb[1]),
            "subtractXY": [a[0] - b[0], a[1] - b[1]], "hashCode": v2_hash(a),
        })
    em.struct("Vector2fCase", [("a", 2, "f"), ("b", 2, "f"), ("dot", 1, "f"), ("determinant", 1, "f"), ("length", 1, "f"),
                               ("distanceSquared", 1, "f"), ("normalize", 2, "f"), ("getAngle", 1, "f"), ("angleBetween", 1, "f"),
                               ("smallestAngleBetween", 1, "f"), ("subtractXY", 2, "f"), ("hashCode", 1, "i")], cases,
              "Vector2f: smallestAngleBetween uses normalize(a)/normalize(b); subtractXY = a.subtract(b.x, b.y)")


def rnd_unit(r):
    while True:
        v = v3(r.uniform(-1, 1), r.uniform(-1, 1), r.uniform(-1, 1))
        if v3_length_squared(v).v > 1e-4:
            return v3_normalize(v)


def gen_matrix3f(r, em):
    cases = []
    for k in range(40):
        a = rnd_vec(r, 9)
        b = rnd_vec(r, 9)
        if k == 0:
            a = [F(2), F(0), F(0), F(0), F(2), F(0), F(0), F(0), F(2)]
        if k == 1:
            a = [F(1), F(2), F(3), F(2), F(4), F(6), F(1), F(1), F(1)]  # singular
        v = rnd_vec(r)
        start = rnd_unit(r)
        if k % 4 == 1:
            end = v3_normalize([start[i] + F(r.uniform(-1e-3, 1e-3)) for i in range(3)])  # nearly parallel
        elif k % 4 == 2:
            end = [-start[0], -start[1], -start[2]]  # opposite
        else:
            end = rnd_unit(r)
        cases.append({
            "a": a, "b": b, "v": v, "start": start, "end": end, "mult": m3_mult(a, b), "multVector": m3_mult_vec(a, v),
            "determinant": m3_determinant(a), "invert": m3_invert(a), "adjoint": m3_adjoint(a), "fromStartEnd": m3_from_start_end(start, end),
            "hashCode": m3_hash(a),
        })
    em.struct("Matrix3fCase", [("a", 9, "f"), ("b", 9, "f"), ("v", 3, "f"), ("start", 3, "f"), ("end", 3, "f"), ("mult", 9, "f"),
                               ("multVector", 3, "f"), ("determinant", 1, "f"), ("invert", 9, "f"), ("adjoint", 9, "f"),
                               ("fromStartEnd", 9, "f"), ("hashCode", 1, "i")], cases,
              "Matrix3f (row-major): a.mult(b), a.mult(v), a.determinant(), a.invert(), a.adjoint(), fromStartEndVectors(start, end)")


def gen_matrix4f(r, em):
    cases = []
    for k in range(40):
        a = rnd_vec(r, 16)
        b = rnd_vec(r, 16)
        if k % 3 == 0:  # affine matrices as used by the geo engine
            a[12], a[13], a[14], a[15] = F(0), F(0), F(0), F(1)
        v = rnd_vec(r)
        det, adj = m4_cofactors(a)
        inv = m4_invert(a)
        proj, w = m4_mult_proj(a, v)
        cases.append({
            "a": a, "b": b, "v": v, "mult": m4_mult(a, b), "multVector": m4_mult_vec(a, v), "multNormal": m4_mult_normal(a, v),
            "multNormalAcross": m4_mult_normal_across(a, v), "multProj": proj, "multProjW": w, "multAcross": m4_mult_across(a, v),
            "determinant": det, "invertible": inv is not None, "invert": inv if inv is not None else [F(0)] * 16, "adjoint": adj,
            "hashCode": m4_hash(a),
        })
    em.struct("Matrix4fCase", [("a", 16, "f"), ("b", 16, "f"), ("v", 3, "f"), ("mult", 16, "f"), ("multVector", 3, "f"),
                               ("multNormal", 3, "f"), ("multNormalAcross", 3, "f"), ("multProj", 3, "f"), ("multProjW", 1, "f"),
                               ("multAcross", 3, "f"), ("determinant", 1, "f"), ("invertible", 1, "b"), ("invert", 16, "f"),
                               ("adjoint", 16, "f"), ("hashCode", 1, "i")], cases,
              "Matrix4f (row-major): a.mult(b), a.mult(v), multNormal, multNormalAcross, multProj (+ w), multAcross, determinant, invert, adjoint")


def gen_transform(r, em):
    cases = []
    for _ in range(32):
        # rotation about z by a random heading, like the placements in the .geo files, plus a random tilt axis
        angle = r.uniform(-math.pi, math.pi)
        c, s = math.cos(angle), math.sin(angle)
        rot = [F(c), F(-s), F(0), F(s), F(c), F(0), F(0), F(0), F(1)]
        if r.random() < 0.5:
            rot = m3_mult(rot, [F(1), F(0), F(0), F(0), F(math.cos(0.1)), F(-math.sin(0.1)), F(0), F(math.sin(0.1)), F(math.cos(0.1))])
        sc = F(r.uniform(0.5, 2.0))
        scale = [sc, sc, sc] if r.random() < 0.7 else [F(r.uniform(0.5, 2.0)) for _ in range(3)]
        loc = [F(r.uniform(0, 4096)), F(r.uniform(0, 4096)), F(r.uniform(0, 600))]
        world = m4_set_transform(rot, scale, loc)
        inv = m4_invert(world)
        point = [loc[0] + F(r.uniform(-20, 20)), loc[1] + F(r.uniform(-20, 20)), loc[2] + F(r.uniform(-5, 5))]
        direction = v3_normalize([F(r.uniform(-1, 1)), F(r.uniform(-1, 1)), F(r.uniform(-1, 1))])
        local_dir = m4_mult_normal(inv, direction)
        cases.append({
            "rotation": rot, "scale": scale, "location": loc, "point": point, "direction": direction, "world": world, "inverse": inv,
            "localPoint": m4_mult_vec(inv, point), "localDirection": local_dir, "localDirectionNormalized": v3_normalize(local_dir),
            "worldPoint": m4_mult_vec(world, point),
        })
    em.struct("TransformCase", [("rotation", 9, "f"), ("scale", 3, "f"), ("location", 3, "f"), ("point", 3, "f"), ("direction", 3, "f"),
                                ("world", 16, "f"), ("inverse", 16, "f"), ("localPoint", 3, "f"), ("localDirection", 3, "f"),
                                ("localDirectionNormalized", 3, "f"), ("worldPoint", 3, "f")], cases,
              "Geometry.setTransform (loadIdentity, setRotationMatrix, scale, setTranslation) and the BIHNode ray transform (invert, mult, multNormal, normalizeLocal)")


def gen_ray(r, em):
    cases = []
    for k in range(64):
        base = [F(r.uniform(0, 4096)), F(r.uniform(0, 4096)), F(r.uniform(0, 600))]
        v0 = [base[i] + F(r.uniform(-10, 10)) for i in range(3)]
        v1 = [base[i] + F(r.uniform(-10, 10)) for i in range(3)]
        v2 = [base[i] + F(r.uniform(-10, 10)) for i in range(3)]
        origin = [base[0] + F(r.uniform(-30, 30)), base[1] + F(r.uniform(-30, 30)), base[2] + F(r.uniform(-30, 30))]
        mode = k % 4
        if mode in (0, 1):  # aim at a point inside the triangle (mode 1: vertical ray like GeoMap.getZ)
            b1, b2 = r.random(), r.random()
            if b1 + b2 > 1:
                b1, b2 = 1 - b1, 1 - b2
            target = [v0[i] + (v1[i] - v0[i]) * F(b1) + (v2[i] - v0[i]) * F(b2) for i in range(3)]
            if mode == 1:
                origin = [target[0], target[1], target[2] + F(r.uniform(1, 50))]
                direction = [F(0), F(0), F(-1)]
            else:
                direction = v3_normalize([target[i] - origin[i] for i in range(3)])
        elif mode == 2:  # random direction, mostly misses
            direction = rnd_unit(r)
        else:  # direction parallel to the triangle plane
            direction = v3_normalize([v1[i] - v0[i] for i in range(3)])
        if k == 0:
            v0, v1, v2 = v3(0, 0, 0), v3(1, 0, 0), v3(0, 1, 0)
            origin, direction = v3(0.25, 0.25, 5), v3(0, 0, -1)
        point = rnd_vec(r)
        t = ray_intersects_t(origin, direction, v0, v1, v2)
        hit, loc = ray_intersects_store(origin, direction, v0, v1, v2, False, False)
        quad_hit, quad = ray_intersects_store(origin, direction, v0, v1, v2, True, True)
        cases.append({
            "origin": origin, "direction": direction, "v0": v0, "v1": v1, "v2": v2, "point": point, "t": t, "hit": hit,
            "location": loc if hit else [F(0)] * 3, "quadHit": quad_hit, "quad": quad if quad_hit else [F(0)] * 3,
            "distanceSquared": ray_distance_squared(origin, direction, point),
        })
    em.struct("RayCase", [("origin", 3, "f"), ("direction", 3, "f"), ("v0", 3, "f"), ("v1", 3, "f"), ("v2", 3, "f"), ("point", 3, "f"),
                          ("t", 1, "f"), ("hit", 1, "b"), ("location", 3, "f"), ("quadHit", 1, "b"), ("quad", 3, "f"),
                          ("distanceSquared", 1, "f")], cases,
              "Ray: intersects(v0, v1, v2), intersectWhere (hit, location), intersectWherePlanarQuad (hit, t/u/v), distanceSquared(point)")


def gen_fastmath(r, em):
    cases = []
    xs = [F(0), F(-0.0), F(1), F(-1), F(0.5), F(-0.5), F(2), F(1e-30), F(3.4028235e38), F(INF), F(-INF), F(NAN), F(0.999999), F(-0.999999),
          F(1.0000001), F(0.975), F(0.4375), F(1e-45), F(4096), F(-4096)]
    while len(xs) < 72:
        xs.append(rnd_float(r))
    for x in xs:
        y = rnd_float(r)
        z = rnd_float(r)
        t = F(r.uniform(-0.25, 1.25))
        p = rnd_vec(r, 4)
        m16 = [rnd_float(r).v for _ in range(16)]
        unit = F(r.uniform(-1.2, 1.2))
        angle = F(r.uniform(-100.0, 100.0))
        cases.append({
            "x": x, "y": y, "z": z, "t": t, "p": p, "unit": unit, "angle": angle, "m": m16,
            "invSqrt": fm_inv_sqrt(x), "fastInvSqrt": fm_fast_inv_sqrt(x), "sqrt": fm_sqrt(x), "reduceSinAngle": fm_reduce_sin_angle(y),
            "interpolateLinear": fm_interpolate_linear(t, y, z), "catmullRom": fm_catmull_rom(t, x, p[0], p[1], p[2], p[3]),
            "acosX": fm_acos(x), "asinX": fm_asin(x), "acosUnit": fm_acos(unit), "asinUnit": fm_asin(unit), "atanY": fm_atan(y),
            "atan2YZ": fm_atan2(y, z), "atan2XY": fm_atan2(x, y), "copysign": fm_copysign(y, z), "normalize": fm_normalize(angle, -PI, PI),
            "determinant": fm_determinant(m16), "abs": fm_abs(x), "sign": x if (x.v == 0 or x.v != x.v) else F(math.copysign(1.0, x.v)),
        })
    em.struct("FastMathCase", [("x", 1, "f"), ("y", 1, "f"), ("z", 1, "f"), ("t", 1, "f"), ("p", 4, "f"), ("unit", 1, "f"), ("angle", 1, "f"), ("m", 16, "d"),
                               ("invSqrt", 1, "f"), ("fastInvSqrt", 1, "f"), ("sqrt", 1, "f"), ("reduceSinAngle", 1, "f"),
                               ("interpolateLinear", 1, "f"), ("catmullRom", 1, "f"), ("acosX", 1, "f"), ("asinX", 1, "f"), ("acosUnit", 1, "f"),
                               ("asinUnit", 1, "f"), ("atanY", 1, "f"), ("atan2YZ", 1, "f"), ("atan2XY", 1, "f"), ("copysign", 1, "f"),
                               ("normalize", 1, "f"), ("determinant", 1, "f"), ("abs", 1, "f"), ("sign", 1, "f")], cases,
              "FastMath: invSqrt/fastInvSqrt/sqrt/acos/asin/abs/sign(x), reduceSinAngle/atan(y), interpolateLinear(t, y, z), "
              "interpolateCatmullRom(t, x, p...), atan2(y, z), atan2(x, y), copysign(y, z), normalize(angle, -PI, PI), determinant(m)")


def gen_strictmath(r, em):
    xs = [0.0, -0.0, 1.0, -1.0, 0.5, -0.5, 0.4375, 0.6875, 1.1875, 2.4375, 0.975, -0.975, 2.0 ** -28, -(2.0 ** -58), 2.0 ** 66, -(2.0 ** 70),
          INF, -INF, NAN, 1e-310, math.nextafter(1.0, 0.0), math.nextafter(-1.0, 0.0), math.nextafter(1.0, 2.0), 3.0, -7.5, 1e10]
    ys = [0.0, -0.0, 1.0, -1.0, INF, -INF, NAN, 2.0, -3.0, 1e-300, 1e300, 0.1]
    cases = []
    for x in xs:
        y = ys[len(cases) % len(ys)]
        cases.append((x, y))
    for y in ys:
        for x in [0.0, -0.0, 1.0, -1.0, INF, -INF, 1e-200, -1e200]:
            cases.append((x, y))
    while len(cases) < 400:
        kind = r.randrange(4)
        if kind == 0:
            x = r.uniform(-1.0, 1.0)
        elif kind == 1:
            x = math.copysign(10.0 ** r.uniform(-20, 20), r.uniform(-1, 1))
        elif kind == 2:
            x = r.uniform(-3.0, 3.0)
        else:
            x = f32(r.uniform(-1.0, 1.0))
        y = math.copysign(10.0 ** r.uniform(-20, 20), r.uniform(-1, 1)) if r.random() < 0.5 else r.uniform(-5, 5)
        cases.append((x, y))
    out = []
    worst = 0
    for x, y in cases:
        row = {"x": x, "y": y, "asin": s_asin(x), "acos": s_acos(x), "atan": s_atan(x), "atan2": s_atan2(y, x)}
        # sanity check of the model against the C runtime used by Python: fdlibm is within 1 ulp of the true value
        for name, ref in (("asin", lambda: math.asin(x) if abs(x) <= 1 or x != x else NAN), ("acos", lambda: math.acos(x) if abs(x) <= 1 or x != x else NAN),
                          ("atan", lambda: math.atan(x)), ("atan2", lambda: math.atan2(y, x))):
            expected = ref()
            if not within_one_ulp(row[name], expected):
                raise AssertionError(f"StrictMath model {name}({x!r}, {y!r}) = {row[name]!r} is not within 1 ulp of {expected!r}")
            if row[name] != expected and not (row[name] != row[name]):
                worst += 1
        out.append(row)
    em.struct("StrictMathCase", [("x", 1, "d"), ("y", 1, "d"), ("asin", 1, "d"), ("acos", 1, "d"), ("atan", 1, "d"), ("atan2", 1, "d")], out,
              f"StrictMath (fdlibm): asin(x), acos(x), atan(x), atan2(y, x); {worst} of {4 * len(out)} results differ from the C runtime by 1 ulp")


def gen_half(r, em):
    cases = []
    values = [F(0), F(-0.0), F(1), F(-1), F(65504), F(65520), F(-70000), F(1e-8), F(-1e-8), F(0.5), F(3.14159), F(INF), F(-INF),
              F(5.96046e-8), F(6e-8), F(2.5e-4), F(-123.456)]
    while len(values) < 48:
        values.append(F(r.uniform(-70000, 70000)) if r.random() < 0.5 else F(math.copysign(10 ** r.uniform(-9, 5), r.uniform(-1, 1))))
    for v in values:
        half = fm_float_to_half(v)
        cases.append({"value": v, "half": half - 65536 if half >= 32768 else half, "back": fm_half_to_float(half)})
    em.struct("HalfCase", [("value", 1, "f"), ("half", 1, "i"), ("back", 1, "f")], cases,
              "FastMath.convertFloatToHalf(value) and convertHalfToFloat(half)")


def gen_triangle(r, em):
    cases = []
    for k in range(48):
        t0, t1, t2 = rnd_vec(r, 2), rnd_vec(r, 2), rnd_vec(r, 2)
        if k % 3 == 0:
            t0, t1, t2 = [F(0), F(0)], [F(4), F(0)], [F(0), F(4)]
        p = [F(r.uniform(-1, 5)), F(r.uniform(-1, 5))] if k % 3 == 0 else rnd_vec(r, 2)
        if k % 5 == 0:
            p = [t0[0], t0[1]]
        cases.append({"t0": t0, "t1": t1, "t2": t2, "p": p, "ccw": fm_ccw(t0, t1, p), "inside": fm_point_inside_triangle(t0, t1, t2, p)})
    em.struct("TriangleCase", [("t0", 2, "f"), ("t1", 2, "f"), ("t2", 2, "f"), ("p", 2, "f"), ("ccw", 1, "i"), ("inside", 1, "i")], cases,
              "FastMath.counterClockwise(t0, t1, p) and pointInsideTriangle(t0, t1, t2, p)")


def generate() -> str:
    r = random.Random(20260913)
    em = Emitter()
    consts = {"PI": PI, "TWO_PI": TWO_PI, "HALF_PI": HALF_PI, "QUARTER_PI": QUARTER_PI, "INV_PI": INV_PI, "INV_TWO_PI": INV_TWO_PI,
              "DEG_TO_RAD": DEG_TO_RAD, "RAD_TO_DEG": RAD_TO_DEG, "ONE_THIRD": ONE_THIRD, "FLOAT_EPSILON": FLT_EPSILON,
              "ZERO_TOLERANCE": ZERO_TOLERANCE}
    const_lines = ["// FastMath constants (float bits)"] + [f"inline constexpr uint32_t CONST_{k} = 0x{fbits(v):08x}u;" for k, v in consts.items()] + [""]
    em.parts.append("\n".join(const_lines))
    gen_vector3f(r, em)
    gen_vector2f(r, em)
    gen_matrix3f(r, em)
    gen_matrix4f(r, em)
    gen_transform(r, em)
    gen_ray(r, em)
    gen_fastmath(r, em)
    gen_strictmath(r, em)
    gen_half(r, em)
    gen_triangle(r, em)
    header = [
        "// GENERATED by oracle/gen_golden.py - do not edit. Regenerate with: python oracle/gen_golden.py",
        "// Golden values from a Python model of the Java float semantics of the jME math classes (see the generator's docstring).",
        "#pragma once",
        "",
        "#include <cstdint>",
        "",
        "namespace aion::gameserver::geoEngine::math::golden {",
        "",
    ]
    return "\n".join(header) + "\n".join(em.parts) + "} // namespace aion::gameserver::geoEngine::math::golden\n"


def main() -> int:
    target = pathlib.Path(__file__).resolve().parent.parent / "GoldenVectors.gen.h"
    text = generate()
    if "--check" in sys.argv[1:]:
        current = target.read_text(encoding="utf-8") if target.exists() else ""
        if current != text:
            print(f"{target} is out of date; run python {pathlib.Path(__file__).name}", file=sys.stderr)
            return 1
        print("up to date")
        return 0
    target.write_bytes(text.encode("utf-8"))
    print(f"wrote {target} ({len(text)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
