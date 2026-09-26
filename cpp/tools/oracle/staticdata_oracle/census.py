"""V4 lexical census: the shape of every attribute value and element text in the merged document, per element path.

The census is type-agnostic (it does not read the generator's IR): it classifies literals and flags forms where JAXB's lenient parsing
and a strict C++ parser could disagree, or where Java code can observe a difference (present-empty values). Consumers join the paths with
the IR types; a flag only matters if the bound property's type makes it matter (e.g. LEADING_ZERO on an int, EMPTY on a String/list).

Path syntax: holder-root/child/.../element@attribute, and holder-root/.../element#text for element text (whitespace-only text is not part
of the merged document and is not counted).
"""

from __future__ import annotations

import re
from collections import Counter

FORMAT = "aion-staticdata-census"
VERSION = 1
VALUE_LIMIT = 24  # distinct literals recorded per path
EXAMPLE_LIMIT = 3
EXAMPLE_LENGTH = 80  # longer examples are cut and end with "..."
LITERAL_LENGTH = 80  # paths with a longer literal record no literal set

_INT = re.compile(r"-?(?:0|[1-9][0-9]*)\Z")
_INT_NONCANONICAL = re.compile(r"[+-]?[0-9]+\Z")
_DECIMAL = re.compile(r"-?[0-9]+\.[0-9]+\Z")
_DECIMAL_EDGE = re.compile(r"[+-]?(?:[0-9]+\.|\.[0-9]+)\Z")
_EXPONENT = re.compile(r"[+-]?(?:[0-9]+\.?[0-9]*|\.[0-9]+)[eE][+-]?[0-9]+\Z")
_FLOAT_SUFFIX = re.compile(r"[+-]?(?:[0-9]+\.?[0-9]*|\.[0-9]+)(?:[eE][+-]?[0-9]+)?[fFdD]\Z")
_FLOAT_SPECIAL = frozenset({"NaN", "Infinity", "-Infinity", "+Infinity", "INF", "-INF"})
_HEX = re.compile(r"[+-]?0[xX][0-9a-fA-F]+\Z")
_ENUM = re.compile(r"[A-Z][A-Z0-9_]*\Z")
_IDENTIFIER = re.compile(r"[A-Za-z_][A-Za-z0-9_]*\Z")
_DATETIME = re.compile(r"[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}(?::[0-9]{2}(?:\.[0-9]+)?)?\Z")
_DATE = re.compile(r"[0-9]{4}-[0-9]{2}-[0-9]{2}\Z")
_TIME = re.compile(r"[0-9]{1,2}:[0-9]{2}(?::[0-9]{2})?\Z")
_XML_WS = " \t\r\n"

INT_MIN, INT_MAX = -(2 ** 31), 2 ** 31 - 1
LONG_MIN, LONG_MAX = -(2 ** 63), 2 ** 63 - 1

NUMERIC_SHAPES = frozenset({"int", "long", "bigint", "int-noncanonical", "decimal", "decimal-edge", "exponent", "float-suffix",
                            "float-special", "hex", "int-list", "decimal-list"})
TEXT_SHAPES = frozenset({"enum", "identifier", "text", "enum-list", "comma-list", "datetime", "date", "time", "bool",
                         "bool-noncanonical"})

FLAG_DESCRIPTIONS = {
	"EMPTY": "present but empty value (String absent vs empty, xs:list empty vs null, number/enum parse of \"\")",
	"BLANK": "whitespace-only value",
	"WHITESPACE_EDGE": "leading or trailing whitespace (JAXB trims numbers/enums, keeps Strings)",
	"MULTI_SPACE": "consecutive spaces inside the value (xs:list collapse vs split(\" \"))",
	"PLUS_SIGN": "explicit + sign on a number",
	"LEADING_ZERO": "integer with leading zeros",
	"NEGATIVE_ZERO": "-0",
	"INT_OVERFLOW": "integer outside int32",
	"LONG_OVERFLOW": "integer outside int64",
	"DECIMAL_EDGE": "decimal like \"5.\" or \".5\"",
	"EXPONENT": "number with exponent",
	"FLOAT_SUFFIX": "Java float literal suffix (f/d)",
	"FLOAT_SPECIAL": "NaN/Infinity/INF literal",
	"HEX": "hexadecimal literal",
	"BOOL_NONCANONICAL": "boolean literal in non-canonical case (True, FALSE, ...)",
	"NON_ASCII": "non-ASCII characters",
	"CONTROL_CHAR": "tab, newline or other control character",
	"MIXED_SHAPES": "the same path has numeric and non-numeric literals",
	"BOOL_MIXED_NUMERIC": "the same path uses true/false and 0/1",
	"ENUM_CASE_VARIANTS": "the same path has literals differing only in case",
	"MIXED_CONTENT": "element has both child elements and non-whitespace text",
}


def classify(value: str):
	"""Returns (shape, flags tuple, int value or None)."""
	flags = []
	if value == "":
		return "empty", ("EMPTY",), None
	stripped = value.strip(_XML_WS)
	if stripped == "":
		return "blank", ("BLANK",), None
	if stripped != value:
		flags.append("WHITESPACE_EDGE")
	if not value.isascii():
		flags.append("NON_ASCII")
	if any(ord(c) < 0x20 for c in value):
		flags.append("CONTROL_CHAR")
	v = stripped
	if _INT.match(v):
		n = int(v)
		if v == "-0":
			flags.append("NEGATIVE_ZERO")
			return "int-noncanonical", tuple(flags), 0
		if INT_MIN <= n <= INT_MAX:
			return "int", tuple(flags), n
		if LONG_MIN <= n <= LONG_MAX:
			flags.append("INT_OVERFLOW")
			return "long", tuple(flags), n
		flags += ["INT_OVERFLOW", "LONG_OVERFLOW"]
		return "bigint", tuple(flags), n
	if _INT_NONCANONICAL.match(v):
		if v[0] == "+":
			flags.append("PLUS_SIGN")
		digits = v.lstrip("+-")
		if len(digits) > 1 and digits[0] == "0":
			flags.append("LEADING_ZERO")
		if int(v) == 0 and v[0] == "-":
			flags.append("NEGATIVE_ZERO")
		n = int(v)
		if not INT_MIN <= n <= INT_MAX:
			flags.append("INT_OVERFLOW")
		return "int-noncanonical", tuple(flags), n
	if _DECIMAL.match(v):
		return "decimal", tuple(flags), None
	if _DECIMAL_EDGE.match(v):
		flags.append("DECIMAL_EDGE")
		if v[0] == "+":
			flags.append("PLUS_SIGN")
		return "decimal-edge", tuple(flags), None
	if _EXPONENT.match(v):
		flags.append("EXPONENT")
		return "exponent", tuple(flags), None
	if _FLOAT_SUFFIX.match(v):
		flags.append("FLOAT_SUFFIX")
		return "float-suffix", tuple(flags), None
	if v in _FLOAT_SPECIAL:
		flags.append("FLOAT_SPECIAL")
		return "float-special", tuple(flags), None
	if _HEX.match(v):
		flags.append("HEX")
		return "hex", tuple(flags), None
	if v in ("true", "false"):
		return "bool", tuple(flags), None
	if v.lower() in ("true", "false"):
		flags.append("BOOL_NONCANONICAL")
		return "bool-noncanonical", tuple(flags), None
	if _ENUM.match(v):
		return "enum", tuple(flags), None
	if _IDENTIFIER.match(v):
		return "identifier", tuple(flags), None
	if _DATETIME.match(v):
		return "datetime", tuple(flags), None
	if _DATE.match(v):
		return "date", tuple(flags), None
	if _TIME.match(v):
		return "time", tuple(flags), None
	tokens = v.split()
	if len(tokens) > 1:
		if "  " in v:
			flags.append("MULTI_SPACE")
		if all(_INT.match(t) for t in tokens):
			return "int-list", tuple(flags), None
		if all(_INT.match(t) or _DECIMAL.match(t) for t in tokens):
			return "decimal-list", tuple(flags), None
		if all(_IDENTIFIER.match(t) for t in tokens):
			return "enum-list", tuple(flags), None
	if "," in v and all(_INT.match(p.strip()) or _IDENTIFIER.match(p.strip()) for p in v.split(",")):
		return "comma-list", tuple(flags), None
	if "  " in v and "MULTI_SPACE" not in flags:
		flags.append("MULTI_SPACE")
	return "text", tuple(flags), None


class _Stat:
	__slots__ = ("count", "shapes", "flags", "values", "overflow", "int_min", "int_max", "max_len", "examples")

	def __init__(self):
		self.count = 0
		self.shapes = Counter()
		self.flags = Counter()
		self.values = Counter()
		self.overflow = False
		self.int_min = None
		self.int_max = None
		self.max_len = 0
		self.examples = {}

	def add(self, value, classified):
		shape, flags, n = classified
		self.count += 1
		self.shapes[shape] += 1
		if len(value) > self.max_len:
			self.max_len = len(value)
		for f in flags:
			self.flags[f] += 1
			ex = self.examples.setdefault(f, [])
			example = value if len(value) <= EXAMPLE_LENGTH else value[:EXAMPLE_LENGTH] + "..."
			if len(ex) < EXAMPLE_LIMIT and example not in ex:
				ex.append(example)
		if n is not None:
			if self.int_min is None or n < self.int_min:
				self.int_min = n
			if self.int_max is None or n > self.int_max:
				self.int_max = n
		if not self.overflow:
			if len(value) > LITERAL_LENGTH:
				self.overflow = "not recorded (literal longer than %d)" % LITERAL_LENGTH
				self.values.clear()
			elif value in self.values or len(self.values) < VALUE_LIMIT:
				self.values[value] += 1
			else:
				self.overflow = f">{VALUE_LIMIT}"
				self.values.clear()

	def path_flags(self):
		flags = dict(self.flags)
		shapes = set(self.shapes) - {"empty", "blank"}
		if shapes & NUMERIC_SHAPES and shapes & (TEXT_SHAPES - {"bool", "bool-noncanonical"}):
			flags["MIXED_SHAPES"] = 1
		if not self.overflow:
			lits = set(self.values)
			if lits & {"true", "false"} and lits & {"0", "1"}:
				flags["BOOL_MIXED_NUMERIC"] = 1
			folded = Counter(v.lower() for v in lits)
			if any(c > 1 for c in folded.values()):
				flags["ENUM_CASE_VARIANTS"] = 1
		return flags

	def to_json(self, element_count):
		doc = {"count": self.count, "elementCount": element_count, "shapes": dict(sorted(self.shapes.items()))}
		flags = self.path_flags()
		if flags:
			doc["flags"] = dict(sorted(flags.items()))
			examples = {f: self.examples[f] for f in sorted(self.examples)}
			if examples:
				doc["examples"] = examples
		if self.int_min is not None:
			doc["intRange"] = [self.int_min, self.int_max]
		doc["maxLength"] = self.max_len
		if self.overflow:
			doc["distinct"] = self.overflow
		else:
			doc["distinct"] = len(self.values)
			doc["values"] = dict(sorted(self.values.items()))
		return doc


class CensusVisitor:

	def __init__(self):
		self.stats = {}  # (path string, attribute or "#text") -> _Stat
		self.elements = Counter()  # path string -> element count
		self.mixed = Counter()
		self._root = None
		self._keys = {}
		self._cache = {}

	def _classify(self, value):
		c = self._cache.get(value)
		if c is None:
			c = classify(value)
			if len(value) <= 40 and len(self._cache) < 500_000:
				self._cache[value] = c
		return c

	def _key(self, path):
		key = self._keys.get((self._root, path))
		if key is None:
			key = "/".join((self._root,) + path)
			self._keys[(self._root, path)] = key
		return key

	def _attrs(self, key, attrib):
		self.elements[key] += 1
		stats = self.stats
		for name, value in attrib.items():
			if name[0] == "{":
				continue
			s = stats.get((key, name))
			if s is None:
				s = stats[(key, name)] = _Stat()
			s.add(value, self._classify(value))

	def begin_holder(self, imp, file, tag, attrib):
		self._root = tag
		self._attrs(tag, attrib)

	def start(self, path, tag, attrib):
		self._attrs(self._key(path), attrib)

	def end(self, path, element):
		text = element.text
		has_text = text is not None and text.strip(_XML_WS) != ""
		if has_text:
			key = self._key(path)
			s = self.stats.get((key, "#text"))
			if s is None:
				s = self.stats[(key, "#text")] = _Stat()
			s.add(text, self._classify(text))
		if len(element) and (has_text or any(c.tail is not None and c.tail.strip(_XML_WS) for c in element)):
			self.mixed[self._key(path)] += 1

	def end_holder(self, imp):
		self._root = None

	def result(self):
		paths = {}
		flag_paths = Counter()
		for (key, name) in sorted(self.stats):
			sep = "#text" if name == "#text" else "@" + name
			doc = self.stats[(key, name)].to_json(self.elements[key])
			if key in self.mixed and name == "#text":
				doc.setdefault("flags", {})["MIXED_CONTENT"] = self.mixed[key]
				doc["flags"] = dict(sorted(doc["flags"].items()))
			paths[key + sep] = doc
			for f in doc.get("flags", {}):
				flag_paths[f] += 1
		for key in sorted(self.mixed):
			if (key, "#text") not in self.stats:
				paths[key + "#text"] = {"count": 0, "elementCount": self.elements[key], "flags": {"MIXED_CONTENT": self.mixed[key]}}
				flag_paths["MIXED_CONTENT"] += 1
		values = sum(s.count for (k, n), s in self.stats.items() if n != "#text")
		texts = sum(s.count for (k, n), s in self.stats.items() if n == "#text")
		return {
			"format": FORMAT,
			"version": VERSION,
			"valueLimit": VALUE_LIMIT,
			"summary": {
				"elementPaths": len(self.elements),
				"valuePaths": len(paths),
				"attributeValues": values,
				"textValues": texts,
				"pathsPerFlag": dict(sorted(flag_paths.items())),
			},
			"flagDescriptions": FLAG_DESCRIPTIONS,
			"paths": dict(sorted(paths.items())),
		}


def markdown_report(census: dict, title: str = "Static data lexical census (V4)") -> str:
	lines = [f"# {title}", "", "Generated by `cpp/tools/oracle/oracle.py generate`; do not edit.", ""]
	s = census["summary"]
	lines += [f"- element paths: {s['elementPaths']}", f"- value paths: {s['valuePaths']}", f"- attribute values: {s['attributeValues']}",
	          f"- element text values: {s['textValues']}", "", "## Flags", "", "| Flag | Paths | Meaning |", "|---|---|---|"]
	for flag, n in s["pathsPerFlag"].items():
		lines.append(f"| {flag} | {n} | {census['flagDescriptions'][flag]} |")
	lines += ["", "## Flagged paths", ""]
	for path, doc in census["paths"].items():
		flags = doc.get("flags")
		if not flags:
			continue
		shapes = ", ".join(f"{k} {v}" for k, v in doc.get("shapes", {}).items())
		lines.append(f"### `{path}`")
		lines.append("")
		lines.append(f"{doc['count']} values on {doc['elementCount']} elements; shapes: {shapes or '-'}")
		lines.append("")
		for flag, n in flags.items():
			examples = doc.get("examples", {}).get(flag)
			ex = "" if not examples else " e.g. " + ", ".join(f"`{e!r}`" for e in examples)
			lines.append(f"- {flag}: {n}{ex}")
		if "values" in doc and doc["distinct"] <= 12:
			lines.append("- literals: " + ", ".join(f"`{v!r}` {c}" for v, c in doc["values"].items()))
		lines.append("")
	return "\n".join(lines).rstrip() + "\n"
