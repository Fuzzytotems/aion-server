"""Strict readers for the attribute values the count rules depend on.

JAXB is more lenient than these readers (it trims numbers, and its enum and boolean handling of odd literals is not verifiable without a
JDK). The readers accept only canonical forms and raise OracleError otherwise, so a count can never silently depend on a JAXB edge case.
The census (V4) reports where the data uses non-canonical forms.
"""

from __future__ import annotations

import re

from . import OracleError

_INT = re.compile(r"-?[0-9]+\Z")
_FLOAT = re.compile(r"-?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?\Z")
_IDENTIFIER = re.compile(r"[A-Za-z_][A-Za-z0-9_]*\Z")

INT_MIN, INT_MAX = -(2 ** 31), 2 ** 31 - 1


def java_int(value: str, what: str) -> int:
	if not _INT.match(value):
		raise OracleError(f"{what}: {value!r} is not a canonical int")
	v = int(value)
	if not INT_MIN <= v <= INT_MAX:
		raise OracleError(f"{what}: {value!r} is out of int range")
	return v


def java_byte(value: str, what: str) -> int:
	v = java_int(value, what)
	if not -128 <= v <= 127:
		raise OracleError(f"{what}: {value!r} is out of byte range (JAXB would narrow it)")
	return v


def java_float(value: str, what: str) -> float:
	if not _FLOAT.match(value):
		raise OracleError(f"{what}: {value!r} is not a plain decimal float")
	return float(value)


def java_bool(value: str, what: str) -> bool:
	if value in ("true", "1"):
		return True
	if value in ("false", "0"):
		return False
	raise OracleError(f"{what}: {value!r} is not a canonical xs:boolean")


def wrap_int32(v: int) -> int:
	v &= 0xFFFFFFFF
	return v - 0x100000000 if v & 0x80000000 else v


def where(element, name: str) -> str:
	return f"<{element.tag}> @{name}"


def attr_int(element, name: str, default: int = 0) -> int:
	v = element.get(name)
	return default if v is None else java_int(v, where(element, name))


def attr_byte(element, name: str, default: int = 0) -> int:
	v = element.get(name)
	return default if v is None else java_byte(v, where(element, name))


def attr_float(element, name: str):
	"""Float (boxed): None when absent."""
	v = element.get(name)
	return None if v is None else java_float(v, where(element, name))


def attr_bool(element, name: str, default: bool = False) -> bool:
	v = element.get(name)
	return default if v is None else java_bool(v, where(element, name))


def attr_str(element, name: str):
	return element.get(name)


def attr_enum(element, name: str, constants=None, default=None):
	"""Enum attribute: the constant name, or default when absent. constants=None accepts any identifier (ordinal-free enums)."""
	v = element.get(name)
	if v is None:
		return default
	if not _IDENTIFIER.match(v):
		raise OracleError(f"{where(element, name)}: {v!r} is not an enum constant literal")
	if constants is not None and v not in constants:
		raise OracleError(f"{where(element, name)}: {v!r} is not a constant of the Java enum")
	return v


def attr_int_list(element, name: str):
	"""List<Integer>/int[] attribute (xs:list): None when absent, whitespace-separated tokens otherwise."""
	v = element.get(name)
	if v is None:
		return None
	return [java_int(t, where(element, name)) for t in v.split()]
