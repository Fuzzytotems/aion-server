"""M4 gate comparison (handlers-and-porting-plan.md §2.7 items 5 and 6): the report files of `aion_game_server --check-static-data` against
the independent geo oracle and a zone prediction from the XML and geo data.

	python -m geo m4-probes --out FILE          the getZ probes of expected/geo_expected.json as the server's probe input
	python -m geo m4-compare --dir DIR          compares the report files in DIR (exit 1 on a difference)

Checks of m4-compare:
- static_data_extras.txt: the counts Java does not log equal expected/static_data_counts.json "extras" (XMLQuests distinct quest ids).
- geo_statistics.txt: every GeoWorldLoader count equals expected/geo_expected.json "counts" (and worldMaps the map count of world_maps.xml).
- material_zone_names.txt: count, distinct count, FNV-1a digest and the 50 sampled names of geo_expected.json "zoneNames".
- geo_probes_actual.txt: one line per probe in the order of geo_expected.json with the same inputs, z equal bit for bit (NaN equals NaN).
- world_zones.txt: every map of world_maps.xml exists with WorldMap.getInstanceCount() instances, and every instance holds exactly the zone
  names ZoneService.getZoneInstancesByWorldId gives it after GeoService.init (ZoneService.java:71-145, 194-247; ZoneData.java:34-70):
  the whole map zone (the map id), the zones of the map's XML zone files that produce an area (a SPHERE with r <= 0 has none), and the
  material zones of the map's geometries (the oracle loader's names) whose material has a template in material_templates.xml, or material
  11 with gameserver.geodata.shields.enable and a geometry not in ShieldService.IGNORED_SHIELDS_BY_MAP_ID (ShieldService.java:31-65).
  Names are upper case (ZoneName.createOrGet). The instance count follows WorldMapTemplate.getTwinCount/getBeginnerTwinCount with the
  gameserver.world.max.twincount.* properties of config/ (Java defaults 1 and -1).
Nothing here reads the C++ sources; the geo part is geo/loader.py, the XML part reads the files with xml.etree.
"""

from __future__ import annotations

import json
import math
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

from . import GeoOracleError
from . import loader
from . import run

sys.path.insert(0, str(run.TOOL_DIR))
from staticdata_oracle.imports import resolve_imports  # noqa: E402

JAVA_DIR = run.TOOL_DIR.parents[2] / "game-server"
IGNORED_SHIELDS_BY_MAP_ID = {  # ShieldService.java:31-34
	310100000: {"BU_AB_CASTLESHIELD_SAMJUNG_03C_TYPE2_487543"},
	400010000: {"BU_AB_SAMJUNG_BASE_01_SHIELD_313626", "BU_AB_SAMJUNG_BASE_01_SHIELD_299314", "BU_AB_SAMJUNG_BASE_01_SHIELD_137227"},
}
SHIELD_MATERIAL_ID = 11  # ZoneService.createMaterialZoneTemplate
COUNT_KEYS = ["meshEntries", "meshes", "meshNames", "geoFiles", "placements", "missingMeshPlacements", "placementGeometries", "attachedNodes",
	"geometries", "materialGeometries", "terrainMaps"]


def write_probes(expected: dict, out: Path) -> int:
	lines = ["# mapId instanceId xBits yBits zMaxBits zMinBits (python -m geo m4-probes, from geo_expected.json)"]
	for probe in expected["probes"]:
		lines.append(" ".join(str(v) for v in (probe["map"], probe["instanceId"], probe["x"]["bits"], probe["y"]["bits"], probe["zMax"]["bits"],
			probe["zMin"]["bits"])))
	out.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
	return len(expected["probes"])


def read_properties(config_dir: Path) -> dict[str, str]:
	"""Config.load: the default directories (administration, main, network), then mygs.properties; `key = value` lines, # and ! comments"""
	properties: dict[str, str] = {}
	files = []
	for sub in ("administration", "main", "network"):
		directory = config_dir / sub
		if directory.is_dir():
			files += sorted(directory.glob("*.properties"))
	if (config_dir / "mygs.properties").is_file():
		files.append(config_dir / "mygs.properties")
	for file in files:
		for raw in file.read_text(encoding="utf-8", errors="replace").splitlines():
			line = raw.strip()
			if not line or line[0] in "#!":
				continue
			match = re.match(r"([^=:\s]+)\s*[=:]?\s*(.*)$", line)
			if match:
				properties[match.group(1)] = match.group(2).strip()
	return properties


def java_boolean(value: str) -> bool:
	return value.strip().lower() == "true"


def instance_count(attributes: dict[str, str], max_usual: int, max_beginner: int) -> int:
	"""WorldMap.getInstanceCount with WorldMapTemplate.getTwinCount/getBeginnerTwinCount"""
	twin = int(attributes.get("twin_count", "0"))
	beginner = int(attributes.get("beginner_twin_count", "0"))
	twin = twin if max_usual == 0 else min(max_usual, twin)
	beginner = beginner if max_beginner == 0 else (0 if max_beginner == -1 else min(max_beginner, beginner))
	return (1 if twin == 0 else twin) + beginner


def xml_zone_names_by_map(static_data_dir: Path, country_code: int) -> dict[int, set[str]]:
	"""ZoneData.afterUnmarshal: zones with an area, per map, upper case names"""
	imports = [imp for imp in resolve_imports(static_data_dir / "static_data.xml", country_code) if imp.file_attribute == "zones"]
	if len(imports) != 1:
		raise GeoOracleError(f"expected one zones import, found {len(imports)}")
	result: dict[int, set[str]] = {}
	for file in imports[0].files:
		for zone in ET.parse(file.path).getroot().iter("zone"):
			area_type = zone.get("area_type", "POLYGON")
			if area_type == "SPHERE":
				sphere = zone.find("sphere")
				if sphere is None or float(sphere.get("r")) <= 0:
					continue
			result.setdefault(int(zone.get("mapid")), set()).add(zone.get("name").upper())
	return result


def material_template_ids(static_data_dir: Path) -> set[int]:
	root = ET.parse(static_data_dir / "mesh_materials" / "material_templates.xml").getroot()
	return {int(material.get("id")) for material in root.iter("material")}


def read_key_values(file: Path) -> dict[str, int]:
	values = {}
	for line in file.read_text(encoding="utf-8").splitlines():
		if line.strip():
			key, value = line.split()
			values[key] = int(value)
	return values


def read_world_zones(file: Path) -> dict[int, list[tuple[int, list[str]]]]:
	"""world_zones.txt: map -> [(instance id, zone names)]"""
	maps: dict[int, list[tuple[int, list[str]]]] = {}
	current = None
	for line in file.read_text(encoding="utf-8").splitlines():
		parts = line.split(" ")
		if parts[0] == "map":
			if parts[2] == "missing":
				maps[int(parts[1])] = None
				current = None
			else:
				current = maps.setdefault(int(parts[1]), [])
		elif parts[0] == "instance":
			current.append((int(parts[1]), []))
		elif parts[0] == "zone":
			current[-1][1].append(line[len("zone "):])
	return maps


def float_bits_equal(actual: int, expected: int) -> bool:
	def is_nan(bits):
		return (bits & 0x7F800000) == 0x7F800000 and (bits & 0x007FFFFF) != 0
	return actual == expected or (is_nan(actual) and is_nan(expected))


def compare(directory: Path, expected: dict, geo_dir: Path, static_data_dir: Path, config_dir: Path, country_code: int) -> tuple[list[str], list[str]]:
	diffs: list[str] = []
	notes: list[str] = []
	world_maps_xml = static_data_dir / "world_maps.xml"
	map_ids = loader.read_world_map_ids(world_maps_xml)

	counts_document = json.loads((run.TOOL_DIR / "expected" / "static_data_counts.json").read_text(encoding="utf-8"))
	extras = read_key_values(directory / "static_data_extras.txt")
	for key, extra in counts_document["extras"].items():
		if extras.get(key) != extra["value"]:
			diffs.append(f"static data {key} ({extra['description']}): {extras.get(key)}, expected {extra['value']}")
	notes.append("static data extras: " + ", ".join(f"{key} {extras.get(key)}" for key in counts_document["extras"]))

	statistics = read_key_values(directory / "geo_statistics.txt")
	for key in COUNT_KEYS:
		if statistics.get(key) != expected["counts"][key]:
			diffs.append(f"geo {key}: {statistics.get(key)}, expected {expected['counts'][key]}")
	if statistics.get("worldMaps") != len(map_ids):
		diffs.append(f"geo worldMaps: {statistics.get('worldMaps')}, expected {len(map_ids)}")
	notes.append("geo counts: " + ", ".join(f"{key} {statistics.get(key)}" for key in COUNT_KEYS + ["worldMaps"]))

	names = [line for line in (directory / "material_zone_names.txt").read_text(encoding="utf-8").splitlines() if line]
	zones = expected["zoneNames"]
	if len(names) != zones["count"]:
		diffs.append(f"material zone names: {len(names)}, expected {zones['count']}")
	if len(set(names)) != zones["distinct"]:
		diffs.append(f"distinct material zone names: {len(set(names))}, expected {zones['distinct']}")
	digest = run.zone_names_digest(sorted(names))
	if digest != zones["fnv1a64"]:
		diffs.append(f"material zone names digest {digest}, expected {zones['fnv1a64']}")
	missing_samples = [name for name in zones["sample"] if name not in set(names)]
	if missing_samples:
		diffs.append(f"sampled material zone names missing: {missing_samples}")
	notes.append(f"material zone names: {len(names)} ({len(set(names))} distinct, digest {digest}, {len(zones['sample']) - len(missing_samples)} "
		f"of {len(zones['sample'])} samples)")

	probes = expected["probes"]
	actual_lines = [line.split() for line in (directory / "geo_probes_actual.txt").read_text(encoding="utf-8").splitlines() if line.strip()]
	if len(actual_lines) != len(probes):
		diffs.append(f"getZ probes: {len(actual_lines)} results, expected {len(probes)}")
	hits = 0
	for i, (probe, fields) in enumerate(zip(probes, actual_lines)):
		inputs = [probe["map"], probe["instanceId"], probe["x"]["bits"], probe["y"]["bits"], probe["zMax"]["bits"], probe["zMin"]["bits"]]
		if [int(v) for v in fields[:6]] != inputs:
			diffs.append(f"probe {i + 1}: inputs {fields[:6]}, expected {inputs}")
			continue
		z = int(fields[6])
		if not float_bits_equal(z, probe["z"]["bits"]):
			diffs.append(f"probe {i + 1} ({probe['kind']}/{probe['source']} on map {probe['map']}): z bits {z}, expected {probe['z']['bits']} "
				f"({probe['z']['value']})")
		if not math.isnan(struct_float(z)):
			hits += 1
	notes.append(f"getZ probes: {len(actual_lines)} evaluated, {hits} surface hits")

	properties = read_properties(config_dir)
	max_usual = int(properties.get("gameserver.world.max.twincount.usual", "1"))
	max_beginner = int(properties.get("gameserver.world.max.twincount.beginner", "-1"))
	shields = java_boolean(properties.get("gameserver.geodata.shields.enable", "false"))
	geo_enabled = java_boolean(properties.get("gameserver.geodata.enable", "false"))
	attributes = loader.read_world_map_attributes(world_maps_xml)
	xml_zones = xml_zone_names_by_map(static_data_dir, country_code)
	material_ids = material_template_ids(static_data_dir)
	material_zones: dict[int, set[str]] = {}
	if geo_enabled:
		for zone_name, map_id, geometry_name, material_id in loader.load(geo_dir, world_maps_xml).material_zones:
			if material_id == SHIELD_MATERIAL_ID:
				created = shields and geometry_name not in IGNORED_SHIELDS_BY_MAP_ID.get(map_id, set())
			else:
				created = material_id in material_ids
			if created:
				material_zones.setdefault(map_id, set()).add(zone_name.upper())
	world = read_world_zones(directory / "world_zones.txt")
	if sorted(world) != sorted(map_ids) or len(world) != len(map_ids):
		diffs.append(f"world maps: {len(world)} reported, expected {len(map_ids)} (missing {sorted(set(map_ids) - set(world))[:10]})")
	total_instances = 0
	total_zones = 0
	for map_id in map_ids:
		instances = world.get(map_id)
		if instances is None:
			diffs.append(f"map {map_id}: no WorldMap")
			continue
		expected_instances = instance_count(attributes[map_id], max_usual, max_beginner)
		if len(instances) != expected_instances:
			diffs.append(f"map {map_id}: {len(instances)} instances, expected {expected_instances}")
		predicted = {str(map_id)} | xml_zones.get(map_id, set()) | material_zones.get(map_id, set())
		for instance_id, zone_names in instances:
			total_instances += 1
			total_zones += len(zone_names)
			actual = set(zone_names)
			if len(actual) != len(zone_names) or actual != predicted:
				diffs.append(f"map {map_id} instance {instance_id}: {len(zone_names)} zones, expected {len(predicted)} (unexpected "
					f"{sorted(actual - predicted)[:5]}, missing {sorted(predicted - actual)[:5]})")
	notes.append(f"world: {len(world)} maps, {total_instances} map instances, {total_zones} zone instances (XML zone names "
		f"{sum(len(v) for v in xml_zones.values())}, material zones {sum(len(v) for v in material_zones.values())})")
	return diffs, notes


def struct_float(bits: int) -> float:
	import struct
	return struct.unpack(">f", struct.pack(">I", bits))[0]


def main(argv: list[str]) -> int:
	import argparse
	parser = argparse.ArgumentParser(prog="python -m geo m4-probes|m4-compare")
	parser.add_argument("command", choices=["m4-probes", "m4-compare"])
	parser.add_argument("--out", type=Path)
	parser.add_argument("--dir", type=Path)
	parser.add_argument("--expected", type=Path, default=run.DEFAULT_EXPECTED)
	parser.add_argument("--geo-dir", type=Path, default=run.DEFAULT_GEO_DIR)
	parser.add_argument("--static-data", type=Path, default=JAVA_DIR / "data" / "static_data")
	parser.add_argument("--config", type=Path, default=JAVA_DIR / "config")
	parser.add_argument("--country-code", type=int, default=99)
	args = parser.parse_args(argv)
	expected = json.loads(args.expected.read_text(encoding="utf-8"))
	if args.command == "m4-probes":
		if args.out is None:
			parser.error("m4-probes needs --out")
		print(f"wrote {write_probes(expected, args.out)} probes to {args.out}")
		return 0
	if args.dir is None:
		parser.error("m4-compare needs --dir")
	diffs, notes = compare(args.dir, expected, args.geo_dir, args.static_data, args.config, args.country_code)
	for note in notes:
		print(note)
	for diff in diffs:
		print(diff)
	print("geo and world equal" if not diffs else f"{len(diffs)} difference(s)")
	return 1 if diffs else 0
