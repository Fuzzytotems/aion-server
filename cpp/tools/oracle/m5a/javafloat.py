"""Java float (IEEE 754 binary32) arithmetic for the oracles: every intermediate result is rounded to float like the JVM does."""

from __future__ import annotations

import math
import struct


def f32(value: float) -> float:
	"""Rounds a Python float (binary64) to the nearest binary32 value (Java `(float) d`)."""
	return struct.unpack("<f", struct.pack("<f", value))[0]


def parse_float(text: str) -> float:
	"""Java Float.parseFloat for the decimal forms of the static data (JAXB float attributes)."""
	return f32(float(text.strip()))


def to_int(value: float) -> int:
	"""Java `(int) f`: truncation toward zero, NaN 0, saturation at the int range."""
	if math.isnan(value):
		return 0
	if value >= 2147483647:
		return 2147483647
	if value <= -2147483648:
		return -2147483648
	return int(value)


def in_range(x1: float, y1: float, z1: float, x2: float, y2: float, z2: float, rng: float) -> bool:
	"""PositionUtil.isInRange(x1, y1, z1, x2, y2, z2, range): float differences and squares, strict comparison."""
	dx = f32(x1 - x2)
	dy = f32(y1 - y2)
	dz = f32(z1 - z2)
	return f32(f32(f32(dx * dx) + f32(dy * dy)) + f32(dz * dz)) < f32(rng * rng)


def distance(x1: float, y1: float, z1: float, x2: float, y2: float, z2: float) -> float:
	"""Euclidean distance in 3D (for reports only, not for range decisions)."""
	return math.sqrt((x1 - x2) ** 2 + (y1 - y2) ** 2 + (z1 - z2) ** 2)
