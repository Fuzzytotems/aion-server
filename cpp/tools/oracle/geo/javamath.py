"""Java float (IEEE 754 single) and int arithmetic in Python.

Python floats are doubles. The sum, difference, product and quotient of two float32 values and the square root of one, computed in double
and rounded once to float32, equal the float32 result: the exact results of + - * need at most 53 significant bits, and a quotient or root
that is not exact cannot lie within one double ulp of a float32 rounding boundary. Every helper therefore rounds after one operation.
"""

from __future__ import annotations

import math
import struct

_FLOAT = struct.Struct("<f")
_INT = struct.Struct("<i")
_UINT = struct.Struct("<I")

INF = math.inf
NAN = math.nan
FLT_EPSILON = 1.1920928955078125e-07


def f32(value: float) -> float:
	"""Rounds a double to float32 (Java (float) cast): overflow gives an infinity, NaN stays NaN."""
	try:
		return _FLOAT.unpack(_FLOAT.pack(value))[0]
	except OverflowError:
		return math.copysign(INF, value)


def add(a: float, b: float) -> float:
	return f32(a + b)


def sub(a: float, b: float) -> float:
	return f32(a - b)


def mul(a: float, b: float) -> float:
	return f32(a * b)


def div(a: float, b: float) -> float:
	"""IEEE division (Python raises ZeroDivisionError for x / 0.0)."""
	if b == 0.0:
		if a == 0.0 or math.isnan(a):
			return NAN
		negative = (math.copysign(1.0, a) < 0) != (math.copysign(1.0, b) < 0)
		return -INF if negative else INF
	return f32(a / b)


def sqrt(value: float) -> float:
	"""FastMath.sqrt: (float) Math.sqrt(value)"""
	if math.isnan(value) or value < 0:
		return NAN
	return f32(math.sqrt(value))


def float_to_int_bits(value: float) -> int:
	"""Float.floatToIntBits: the canonical NaN 0x7fc00000"""
	if math.isnan(value):
		return 0x7FC00000
	return _INT.unpack(_FLOAT.pack(value))[0]


def float_to_raw_uint_bits(value: float) -> int:
	return _UINT.unpack(_FLOAT.pack(value))[0]


def float_to_int(value: float) -> int:
	"""Java (int) floatValue: NaN is 0, values outside the int range saturate, otherwise truncation toward zero"""
	if math.isnan(value):
		return 0
	if value >= 2147483647.0:
		return 2147483647
	if value <= -2147483648.0:
		return -2147483648
	return int(value)


def java_int(value: int) -> int:
	"""Wraps a Python int to Java int"""
	value &= 0xFFFFFFFF
	return value - 0x100000000 if value >= 0x80000000 else value


def java_long_rem(a: int, b: int) -> int:
	"""Java long remainder: the sign of the dividend"""
	r = abs(a) % abs(b)
	return -r if a < 0 else r


def java_max(a: float, b: float) -> float:
	"""Math.max(float, float)"""
	if math.isnan(a):
		return a
	if a == 0.0 and b == 0.0 and math.copysign(1.0, a) < 0:
		return b
	return a if a >= b else b


def java_min(a: float, b: float) -> float:
	"""Math.min(float, float)"""
	if math.isnan(a):
		return a
	if a == 0.0 and b == 0.0 and math.copysign(1.0, b) < 0:
		return b
	return a if a <= b else b


def float_compare(a: float, b: float) -> int:
	"""Float.compare: -0.0 < 0.0, NaN greater than everything and equal to itself"""
	if a < b:
		return -1
	if a > b:
		return 1
	bits_a = float_to_int_bits(a)
	bits_b = float_to_int_bits(b)
	return 0 if bits_a == bits_b else (-1 if bits_a < bits_b else 1)


def fabs(value: float) -> float:
	"""FastMath.abs: `fValue < 0 ? -fValue : fValue` (keeps -0.0)"""
	return -value if value < 0 else value
