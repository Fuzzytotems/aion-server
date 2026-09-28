"""The game server configuration the m5c-trade oracle reads, loaded the way Java loads it.

Java rules, each with the method it comes from:
- Config.loadProperties (Config.java:81-101): every regular `*.properties` file directly in config/administration, config/main and
  config/network (PropertiesUtils.loadFromDirectory, not recursive) fills one `defaults` table, then config/mygs.properties is loaded over it
  (PropertiesUtils.load(file, defaults): a missing file is no error). Files.find enumerates a directory in file system order, so a key that two
  default files give different values is order dependent: the oracle refuses it. Config.load then puts the config properties of every ACTIVE
  event on top (Config.java:48, EventService.getActiveEventConfigProperties); the oracle does not model the event calendar and refuses when any
  <event> of the timed_events holder names a key it reads;
- java.util.Properties.load: logical lines (a line ending in an odd number of backslashes continues on the next one, whose leading white space is
  dropped), `#`/`!` comment lines, the key up to the first unescaped `=`, `:` or white space, one separator, the value with its TRAILING white
  space kept, and the \\t \\n \\r \\f \\uXXXX escapes; files are read as ISO-8859-1;
- ConfigurableProcessor.getValue (ConfigurableProcessor.java:174-183): the property, else the @Property defaultValue of the Config class field
  (read from the Java source); `""` means the empty string; ${name} placeholders are replaced by that property or "";
- the transformers: NumberTransformer (Integer.decode for an int), BooleanTransformer (true/false in any case, or 1/0), ArrayTransformer over
  CommaSeparatedValueTransformer.splitAndTrimValues (commas outside double quotes, each token String.trim'med - chars <= ' ' only - and
  unquoted, a trailing empty token dropped) with Float.valueOf per element for a float[];
- the oracle's own inputs: `--set KEY=VALUE` is read as one line of the profile (so its trailing white space stays, as a profile line's
  would), and a `--profile` named explicitly must exist. gameserver.country.code (GSConfig.SERVER_COUNTRY_CODE) is loaded too: the caller
  reads the static data with it (XmlMerger.applyCountryOverride picks goodslists_<region>.xml).

What the oracle does NOT model, and raises OracleError for: a value Java would reject at startup (Integer.decode of " 20", "abc"; a missing
default folder, on which Files.find throws), and the forms Java accepts but this reader does not implement - hexadecimal or octal ints ("0x14",
"#14", "024"), floats with a type suffix, hexadecimal floats, NaN and Infinity, and a @Property defaultValue with a Java string escape.
"""

from __future__ import annotations

import re
import struct
from dataclasses import dataclass
from fractions import Fraction
from pathlib import Path

from staticdata_oracle import OracleError

from m5a.data import StaticData

# The keys the trade oracle reads: key -> (Config class file below configs/main, field name, Java type)
TRADE_KEYS = {
	"gameserver.prices.default.prices": ("PricesConfig", "DEFAULT_PRICES", "int"),
	"gameserver.prices.default.modifier": ("PricesConfig", "DEFAULT_MODIFIER", "int"),
	"gameserver.prices.default.taxes": ("PricesConfig", "DEFAULT_TAXES", "int"),
	"gameserver.prices.vendor.buymod": ("PricesConfig", "VENDOR_BUY_MODIFIER", "int"),
	"gameserver.prices.vendor.sellmod": ("PricesConfig", "VENDOR_SELL_MODIFIER", "int"),
	"gameserver.siege.enable": ("SiegeConfig", "SIEGE_ENABLED", "boolean"),
	"gameserver.limits.enable": ("CustomConfig", "LIMITS_ENABLED", "boolean"),
	"gameserver.limits.enable_dynamic_cap": ("CustomConfig", "LIMITS_ENABLE_DYNAMIC_CAP", "boolean"),
	"gameserver.selling.apitems.enabled": ("CustomConfig", "SELLING_APITEMS_ENABLED", "boolean"),
	"gameserver.rates.sell_limit": ("RatesConfig", "SELL_LIMIT_RATES", "float[]"),
	# XmlMerger.applyCountryOverride: the region variant of an imported static data file (goodslists_<region>.xml)
	"gameserver.country.code": ("GSConfig", "SERVER_COUNTRY_CODE", "int"),
}

DEFAULT_FOLDERS = ("administration", "main", "network")  # Config.loadProperties' defaultsFolders, in order
WHITESPACE = " \t\f"


def _logical_lines(text: str, what: str) -> list[str]:
	"""java.util.Properties.LineReader: the logical lines of a properties text, comments and blank lines removed."""
	lines = re.split(r"\r\n|\r|\n", text)
	result: list[str] = []
	current: str | None = None
	for natural in lines:
		stripped = natural.lstrip(WHITESPACE)
		if current is None:
			if not stripped or stripped[0] in "#!":
				continue
			current = ""
		trailing = len(stripped) - len(stripped.rstrip("\\"))
		if trailing % 2 == 1:
			current += stripped[:-1]  # a continuation: the backslash goes, the next line's leading white space too
			continue
		result.append(current + stripped)
		current = None
	if current is not None:
		result.append(current)  # Java: a continuation at the end of the input ends the logical line
	return result


def _load_convert(text: str, what: str) -> str:
	"""Properties.loadConvert: the backslash escapes of a key or a value."""
	out = []
	i = 0
	while i < len(text):
		c = text[i]
		i += 1
		if c != "\\":
			out.append(c)
			continue
		if i >= len(text):
			break  # a lone trailing backslash of the last line is dropped
		c = text[i]
		i += 1
		if c == "u":
			digits = text[i:i + 4]
			if len(digits) != 4 or not re.fullmatch(r"[0-9a-fA-F]{4}", digits):
				raise OracleError(f"{what}: malformed \\uxxxx encoding (Java: IllegalArgumentException in Properties.load)")
			out.append(chr(int(digits, 16)))
			i += 4
		else:
			out.append({"t": "\t", "r": "\r", "n": "\n", "f": "\f"}.get(c, c))
	return "".join(out)


def parse_properties(text: str, what: str) -> list[tuple[str, str]]:
	"""Properties.load: (key, value) pairs in file order (a later pair of the same key replaces the earlier one in the table)."""
	pairs = []
	for line in _logical_lines(text, what):
		key_len, value_start, has_sep, backslash = 0, len(line), False, False
		while key_len < len(line):
			c = line[key_len]
			if c in "=:" and not backslash:
				value_start, has_sep = key_len + 1, True
				break
			if c in WHITESPACE and not backslash:
				value_start = key_len + 1
				break
			backslash = (not backslash) if c == "\\" else False
			key_len += 1
		while value_start < len(line):
			c = line[value_start]
			if c not in WHITESPACE:
				if not has_sep and c in "=:":
					has_sep = True
				else:
					break
			value_start += 1
		pairs.append((_load_convert(line[:key_len], what), _load_convert(line[value_start:], what)))
	return pairs


def _read_properties_file(path: Path) -> list[tuple[str, str]]:
	try:
		return parse_properties(path.read_bytes().decode("iso-8859-1"), str(path))
	except OSError as e:
		raise OracleError(f"{path}: {e}") from e


def java_float(text: str, what: str) -> float:
	"""Float.valueOf for a plain decimal (optionally signed, optionally with an exponent): the decimal CORRECTLY rounded to binary32 (round half
	to even), not rounded to a double first. Every other form Float.valueOf accepts is refused."""
	if not re.fullmatch(r"[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?", text):
		raise OracleError(f"{what}={text!r}: only plain decimal floats are modelled (Float.valueOf would accept or reject it)")
	exact = Fraction(text)
	try:
		bits = struct.unpack("<I", struct.pack("<f", float(exact)))[0]  # nearest double, then nearest float: at most one ulp off
	except (OverflowError, struct.error) as e:
		raise OracleError(f"{what}={text!r} is out of the float range (Float.valueOf answers Infinity, which is not modelled)") from e
	best = None
	for neighbour_bits in {bits, max(bits - 1, 0) if bits & 0x7FFFFFFF else bits, bits + 1}:
		if neighbour_bits & 0x7F800000 == 0x7F800000:
			continue  # infinity or NaN
		value = struct.unpack("<f", struct.pack("<I", neighbour_bits))[0]
		key = (abs(Fraction(value) - exact), neighbour_bits & 1)  # nearest, then the even significand
		if best is None or key < best[0]:
			best = (key, value)
	return best[1]


def java_decode_int(text: str, what: str) -> int:
	"""Integer.decode for a canonical decimal int; the hexadecimal and octal forms it also accepts are refused, as is what it rejects."""
	if not re.fullmatch(r"[+-]?(0|[1-9][0-9]*)", text):
		raise OracleError(f"{what}={text!r}: not a canonical decimal int (Integer.decode would throw or read hexadecimal/octal, which is not "
		                  "modelled)")
	value = int(text, 10)
	if not -2**31 <= value < 2**31:
		raise OracleError(f"{what}={text!r} is out of the int range (Integer.decode throws)")
	return value


def java_config_boolean(text: str, what: str) -> bool:
	"""BooleanTransformer.parseObject."""
	if text.lower() == "true" or text == "1":
		return True
	if text.lower() == "false" or text == "0":
		return False
	raise OracleError(f"{what}={text!r}: BooleanTransformer allows only true, false, 1 and 0 (Java fails at startup)")


def _java_trim(text: str) -> str:
	"""String.trim: removes every leading and trailing char <= ' ' (and nothing else: U+00A0 stays)."""
	start, end = 0, len(text)
	while start < end and ord(text[start]) <= 32:
		start += 1
	while end > start and ord(text[end - 1]) <= 32:
		end -= 1
	return text[start:end]


def split_and_trim_values(value: str) -> list[str]:
	"""CommaSeparatedValueTransformer.splitAndTrimValues."""
	tokens: list[str] = []
	in_quotes = False
	current = ""

	def trim(s: str) -> str:
		s = _java_trim(s)
		if len(s) > 1 and s[0] == '"' and s[-1] == '"':
			s = s[1:-1]
		return s

	for c in value:
		if c == "," and not in_quotes:
			tokens.append(trim(current))
			current = ""
			continue
		if c == '"':
			in_quotes = not in_quotes
		current += c
	last = trim(current)
	if last:
		tokens.append(last)
	return tokens


def transform(value: str, java_type: str, what: str):
	if java_type == "int":
		return java_decode_int(value, what)
	if java_type == "boolean":
		return java_config_boolean(value, what)
	if java_type == "float[]":
		return [java_float(token, what) for token in split_and_trim_values(value)]
	raise OracleError(f"{what}: Java type {java_type} is not modelled")


def property_defaults(java_src: Path, keys: dict[str, tuple[str, str, str]] = TRADE_KEYS) -> dict[str, str]:
	"""The @Property(key = ..., defaultValue = ...) of each key's field, read from configs/main/<Class>.java and checked against the field's
	name and type."""
	base = Path(java_src) / "com" / "aionemu" / "gameserver" / "configs" / "main"
	texts: dict[str, str] = {}
	defaults: dict[str, str] = {}
	pattern = re.compile(r'@Property\(\s*key\s*=\s*"([^"]+)"\s*(?:,\s*defaultValue\s*=\s*"((?:[^"\\]|\\.)*)"\s*)?\)\s*'
	                     r"public\s+static\s+([\w\[\]<>]+)\s+(\w+)\s*;")
	for key, (cls, field_name, java_type) in keys.items():
		if cls not in texts:
			path = base / f"{cls}.java"
			try:
				texts[cls] = path.read_text(encoding="utf-8")
			except OSError as e:
				raise OracleError(f"{path}: {e}") from e
		found = [m for m in pattern.finditer(texts[cls]) if m.group(1) == key]
		if len(found) != 1:
			raise OracleError(f"{cls}.java: expected one @Property(key = \"{key}\") on a `public static` field, found {len(found)}")
		match = found[0]
		if match.group(4) != field_name or match.group(3) != java_type:
			raise OracleError(f"{cls}.java: {key} is bound to {match.group(3)} {match.group(4)}, the oracle expects {java_type} {field_name}")
		if match.group(2) is None:
			raise OracleError(f"{cls}.java: {key} has no defaultValue (the field initializer would decide, which is not modelled)")
		if "\\" in match.group(2):
			raise OracleError(f"{cls}.java: the defaultValue of {key} has a Java string escape, which the oracle does not unescape")
		defaults[key] = match.group(2)
	return defaults


def refuse_event_keys(data: StaticData, keys: dict[str, tuple[str, str, str]] = TRADE_KEYS) -> None:
	"""Config.load puts the config properties of every ACTIVE event on top (Config.java:48): refused for any key the oracle reads."""
	events = event_config_keys(data)
	touched = sorted(k for k in keys if k in events)
	if touched:
		raise OracleError(f"timed events set {', '.join(f'{k} ({events[k]})' for k in touched)}; whether an event is active is the event "
		                  "calendar, which the oracle does not model")


def event_config_keys(data: StaticData) -> dict[str, list[str]]:
	"""key -> names of the timed_events <event>s whose <config_properties> set it (EventTemplate.loadConfigProperties)."""
	keys: dict[str, list[str]] = {}
	for event in data.children("timed_events", "event"):
		container = event.find("config_properties")
		if container is None:
			continue
		text = "\n".join((p.text or "") for p in container.findall("property"))
		for key, _ in parse_properties(text, f"event {event.get('name')!r}"):
			keys.setdefault(key, []).append(event.get("name") or "?")
	return keys


@dataclass(frozen=True)
class ConfigValue:
	value: object
	raw: str
	source: str

	def as_json(self) -> dict:
		return {"value": self.value, "raw": self.raw, "source": self.source}


def load_config(java_src: Path, config_dir: Path | None, profile: Path | None, overrides: list[str] = (), data: StaticData | None = None,
                keys: dict[str, tuple[str, str, str]] = TRADE_KEYS, require_profile: bool = False) -> dict[str, ConfigValue]:
	"""
	The typed value of every key, with where it came from: `--set KEY=VALUE` (read as a line of the profile: Properties.load's separators,
	escapes and trailing white space), the profile (config/mygs.properties by default, Config.java:91, where a missing file is no error; one
	named explicitly, `require_profile`, must exist), a default folder file, or the @Property defaultValue. `config_dir` None means no default
	folder files at all. With `data`, a key a timed event sets is refused (refuse_event_keys).
	"""
	java_defaults = property_defaults(java_src, keys)
	table: dict[str, tuple[str, str]] = {}  # every property: key -> (value, source)
	if config_dir is not None:
		config_dir = Path(config_dir)
		if not config_dir.is_dir():
			raise OracleError(f"{config_dir} is not a directory (pass --config)")
		seen: dict[str, tuple[str, str]] = {}
		for folder in DEFAULT_FOLDERS:
			directory = config_dir / folder
			if not directory.is_dir():
				raise OracleError(f"{directory} does not exist: Files.find throws NoSuchFileException and the game server does not start")
			for path in sorted(p for p in directory.iterdir() if p.is_file() and p.name.endswith(".properties")):
				for key, value in _read_properties_file(path):
					source = str(path.relative_to(config_dir.parent)).replace("\\", "/")
					if key in keys and key in seen and seen[key][0] != value and seen[key][1] != source:
						raise OracleError(f"{key} is set to {seen[key][0]!r} in {seen[key][1]} and to {value!r} in {source}: which one wins depends on "
						                  "the file system order of Files.find, which the oracle does not model")
					seen[key] = (value, source)
					table[key] = (value, source)
	if require_profile and (profile is None or not Path(profile).is_file()):
		raise OracleError(f"--profile {profile}: no such file (only the default config/mygs.properties may be missing)")
	if profile is not None and Path(profile).is_file():
		profile = Path(profile)
		for key, value in _read_properties_file(profile):
			table[key] = (value, str(profile.name))
	for text in overrides:
		pairs = parse_properties(text, f"--set {text!r}")
		if len(pairs) != 1 or not pairs[0][0] or "\n" in text or "\r" in text:
			raise OracleError(f"--set {text!r}: expected one KEY=VALUE line")
		key, value = pairs[0]
		table[key] = (value, "--set")
	if data is not None:
		refuse_event_keys(data, keys)

	result: dict[str, ConfigValue] = {}
	for key, (cls, field_name, java_type) in keys.items():
		raw, source = table.get(key, (java_defaults[key], f"{cls}.java @Property defaultValue"))
		value = raw
		if _java_trim(value) == '""':
			value = ""
		else:
			value = re.sub(r"\$\{([^}]+)\}", lambda m: table.get(m.group(1), ("", ""))[0], value)
		result[key] = ConfigValue(transform(value, java_type, key), raw, source)
	return result
