"""The game server configuration the m5c-craft oracle reads, loaded the way Java loads it (Config.loadProperties, Config.java:81-101).

Layers, lowest first: the @Property defaultValue of the Config class field (read from configs/main/*.java by craft_java.JavaCraftRules), every
`*.properties` file directly in config/administration, config/main and config/network, the profile (config/mygs.properties, like Java; none with
--no-profile), then every `--set KEY=VALUE` (what a gate passes on top of the profile). ConfigurableProcessor.getValue then turns `""` into the
empty string and replaces ${name} placeholders; the transformers type the value. The Properties syntax and the transformers are the ones of
m5c/trade_config.py (the m5c-trade lane's reader of the same files), reused, not copied.

Refused (OracleError): a key two default files set to different values (Files.find order decides in Java), a key without a defaultValue that no
file sets (the field keeps its initializer, which is not modelled), every value Java rejects or the reader does not implement (trade_config), and
EVENTS: unless gameserver.event.service.disabled_events is exactly "*" (EventService.isAllEvents), an active timed event can put config
properties on top of everything (Config.java:48 - custom_events.xml sets gameserver.rates.crafting.crit_chances and
gameserver.rates.gathering.count in some weeks) and give BOOST_*_XP_RATE buffs (skills 10823/10824), which is the event calendar and is not
modelled.
"""

from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path

from staticdata_oracle import OracleError

from m5c.trade_config import (DEFAULT_FOLDERS, java_config_boolean, java_decode_int, java_float, parse_properties,
                              split_and_trim_values)

EVENTS_KEY_FIELD = ("EventsConfig", "DISABLED_EVENTS")


@dataclass(frozen=True)
class CraftConfigValue:
	cls: str
	field: str
	key: str
	java_type: str
	java_default: str | None
	raw: str
	source: str
	value: object

	def as_json(self) -> dict:
		value = sorted(self.value) if isinstance(self.value, (set, frozenset)) else self.value
		return {"field": f"{self.cls}.{self.field}", "type": self.java_type, "javaDefault": self.java_default, "raw": self.raw,
		        "source": self.source, "value": value}


def _read(path: Path) -> list[tuple[str, str]]:
	try:
		return parse_properties(path.read_bytes().decode("iso-8859-1"), str(path))
	except OSError as e:
		raise OracleError(f"{path}: {e}") from e


def _java_trim(text: str) -> str:
	start, end = 0, len(text)
	while start < end and ord(text[start]) <= 32:
		start += 1
	while end > start and ord(text[end - 1]) <= 32:
		end -= 1
	return text[start:end]


def _transform(value: str, java_type: str, what: str):
	if java_type == "int":
		return java_decode_int(value, what)
	if java_type == "boolean":
		return java_config_boolean(value, what)
	if java_type == "float[]":
		return [java_float(token, what) for token in split_and_trim_values(value)]
	if java_type == "Set<String>":
		return frozenset(split_and_trim_values(value))
	raise OracleError(f"{what}: Java type {java_type} is not modelled")


def load_craft_config(config_fields: dict, config_dir: Path | None, profile: Path | None, overrides: list[str] = ()) -> dict[tuple[str, str],
                                                                                                                           CraftConfigValue]:
	"""(Config class, field) -> the typed value and where it came from, for every field of JavaCraftRules.config_fields."""
	keys = {key: cls_field for cls_field, (key, _, _) in config_fields.items()}
	table: dict[str, tuple[str, str]] = {}
	if config_dir is not None:
		config_dir = Path(config_dir)
		if not config_dir.is_dir():
			raise OracleError(f"{config_dir} is not a directory (pass --config-dir)")
		seen: dict[str, tuple[str, str]] = {}
		for folder in DEFAULT_FOLDERS:
			directory = config_dir / folder
			if not directory.is_dir():
				raise OracleError(f"{directory} does not exist (Java's Files.find throws, the server does not start)")
			for path in sorted(p for p in directory.iterdir() if p.is_file() and p.name.endswith(".properties")):
				source = f"{folder}/{path.name}"
				for key, value in _read(path):
					if key in keys and key in seen and seen[key][1] != source and seen[key][0] != value:
						raise OracleError(f"{key} is {seen[key][0]!r} in {seen[key][1]} and {value!r} in {source}: which one wins is the file system "
						                  "order of Files.find, which the oracle does not model")
					seen[key] = (value, source)
					table[key] = (value, source)
	if profile is not None:
		profile = Path(profile)
		if profile.is_file():  # PropertiesUtils.load: a missing profile is no error
			for key, value in _read(profile):
				table[key] = (value, profile.name)
	for text in overrides:
		key, sep, value = text.partition("=")
		if not sep or not key.strip():
			raise OracleError(f"--set {text!r}: expected KEY=VALUE")
		table[key.strip()] = (value.strip(), "--set")

	result: dict[tuple[str, str], CraftConfigValue] = {}
	for (cls, field_name), (key, java_default, java_type) in config_fields.items():
		if key in table:
			raw, source = table[key]
		elif java_default is not None:
			raw, source = java_default, f"{cls}.java @Property defaultValue"
		else:
			raise OracleError(f"{key}: no file sets it and {cls}.{field_name} has no defaultValue (the field initializer decides, not modelled)")
		value = "" if _java_trim(raw) == '""' else re.sub(r"\$\{([^}]+)\}", lambda m: table.get(m.group(1), ("", ""))[0], raw)
		result[(cls, field_name)] = CraftConfigValue(cls, field_name, key, java_type, java_default, raw, source, _transform(value, java_type, key))
	events = result.get(EVENTS_KEY_FIELD)
	if events is None or events.value != frozenset({"*"}):
		shown = events.raw if events is not None else None
		raise OracleError(f"gameserver.event.service.disabled_events is {shown!r}, not '*': an active timed event can override the rate keys and "
		                  "give BOOST_*_XP_RATE buffs (Config.java:48, EventService), which is the event calendar and is not modelled - use a profile "
		                  "that disables every event or pass --set gameserver.event.service.disabled_events=*")
	return result


def membership_rate(values: list[float], membership: int) -> float:
	"""Rates.get (Rates.java:166-173): values[min(length - 1, membership)], 1 for an empty array (Java logs "Missing rates")."""
	if not values:
		return 1.0
	return values[min(len(values) - 1, membership)]
