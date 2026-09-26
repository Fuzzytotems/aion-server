"""Generates and checks expected/geo_expected.json."""

from __future__ import annotations

import json
import random
from pathlib import Path

from . import loader
from .probes import MapScene, draw_probes

TOOL_DIR = Path(__file__).resolve().parent.parent
DEFAULT_GEO_DIR = TOOL_DIR.parents[2] / "game-server" / "data" / "geo"
DEFAULT_WORLD_MAPS = TOOL_DIR.parents[2] / "game-server" / "data" / "static_data" / "world_maps.xml"
DEFAULT_EXPECTED = TOOL_DIR / "expected" / "geo_expected.json"

SEED = 4804
PROBE_MAPS_WITH_TERRAIN = 8
PROBE_MAPS_WITHOUT_TERRAIN = 4
MESH_PROBES = 130
TERRAIN_PROBES = 50
RANDOM_PROBES = 20
ZONE_NAME_SAMPLES = 50
FNV1A64_OFFSET = 0xCBF29CE484222325
FNV1A64_PRIME = 0x100000001B3


def fnv1a64(data: bytes) -> int:
	"""64-bit FNV-1a: a drift digest (not a cryptographic hash) that the C++ real-data test recomputes without a crypto library"""
	value = FNV1A64_OFFSET
	for byte in data:
		value = ((value ^ byte) * FNV1A64_PRIME) & 0xFFFFFFFFFFFFFFFF
	return value


def zone_names_digest(sorted_names: list[str]) -> str:
	"""FNV-1a 64 of the sorted zone names (code point order) joined with LF, UTF-8, as 16 lower-case hex digits"""
	return format(fnv1a64("\n".join(sorted_names).encode("utf-8")), "016x")


def select_probe_maps(geo_dir: Path, world_maps_xml: Path) -> list[int]:
	map_ids = loader.read_world_map_ids(world_maps_xml)
	terrains, _ = loader.terrain_files(geo_dir, map_ids)
	with_geo = sorted(m for m in map_ids if (geo_dir / f"{m}.geo").is_file())
	with_terrain = [m for m in with_geo if terrains.get(m) is not None and terrains[m].heightmap is not None]
	without_terrain = [m for m in with_geo if m not in with_terrain]
	rng = random.Random(SEED)
	return sorted(rng.sample(with_terrain, PROBE_MAPS_WITH_TERRAIN) + rng.sample(without_terrain, PROBE_MAPS_WITHOUT_TERRAIN))


def generate(geo_dir: Path = DEFAULT_GEO_DIR, world_maps_xml: Path = DEFAULT_WORLD_MAPS) -> dict:
	probe_maps = select_probe_maps(geo_dir, world_maps_xml)
	placed: dict[int, list] = {}
	result = loader.load(geo_dir, world_maps_xml, set(probe_maps), placed)
	scenes = {map_id: MapScene(map_id, placed[map_id], loader.read_heightmap(result.terrains.get(map_id))) for map_id in probe_maps}
	probes = draw_probes(scenes, SEED, MESH_PROBES, TERRAIN_PROBES, RANDOM_PROBES)
	names = sorted(result.zone_names)
	sample = [names[i * len(names) // ZONE_NAME_SAMPLES] for i in range(ZONE_NAME_SAMPLES)] if names else []
	return {
		"format": "aion-geo-oracle",
		"version": 1,
		"counts": {
			"meshEntries": result.mesh_entries,
			"meshes": result.meshes,
			"meshNames": result.mesh_names,
			"geoFiles": result.geo_files,
			"placements": result.placements,
			"missingMeshPlacements": result.missing_mesh_placements,
			"placementGeometries": result.placement_geometries,
			"attachedNodes": result.attached_nodes,
			"geometries": result.geometries,
			"materialGeometries": result.material_geometries,
			"terrainMaps": result.terrain_maps,
			"mapsWithEntities": len(result.maps_with_entities),
			"worldMaps": len(result.map_ids),
		},
		"despawnableNodes": dict(sorted(result.despawnable_nodes.items())),
		"zoneNames": {"count": len(names), "distinct": len(set(names)), "fnv1a64": zone_names_digest(names), "sample": sample},
		"probeMaps": probe_maps,
		"probes": probes,
	}


def dumps(document: dict) -> str:
	return json.dumps(document, indent="\t", sort_keys=False) + "\n"
