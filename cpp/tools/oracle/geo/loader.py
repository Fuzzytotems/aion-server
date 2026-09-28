"""The scene GeoWorldLoader builds, reduced to what the oracle checks: counts, material zone names, the geometries and terrains of probe maps.

Rules (GeoWorldLoader.java, GeoMap.java, Node.java, DespawnableNode.java):
- loadMeshes: one node per models.mesh entry with one Geometry per model, all named like the entry. A name with `|` is stored once per alias:
  a clone of the node whose first child named like the entry (Node.getChild) is renamed to the alias; later children keep the full name.
  A later entry with the same name replaces the earlier one (HashMap.put).
- loadWorld: every record of <mapId>.geo whose model exists is cloned into the map (a DespawnableNode for type > 0). A TOWN_OBJECT record
  also attaches the models of its higher town levels (the name with `_01.cgf` replaced by `_0<level>.cgf`) that exist.
- createZone: every child geometry with the MATERIAL intention bit becomes a material zone named
  upper(name between the last '/' and the last '.') [+ "_CHILD" + (index + 1) if the node has several children] + "_" + vector hash of the
  geometry's world bound center + "_" + map id.
- loadTerrains: a PNG belongs to every map whose id is a prefix of one of the comma separated parts of its file name; 16-bit images set
  the heightmap, 8-bit images the materials (with the size checks of Terrain); a map has a terrain if a heightmap was set.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path

from . import GeoOracleError
from .geofiles import MeshEntry, MeshModel, Placement, read_meshes, read_placements, read_world_map_attributes, read_world_map_ids
from .jgeo import contain_aabb, transform_box, vector_hash, world_matrix
from .pngread import CHANNELS, read_png

MATERIAL = 1 << 1
TOWN_OBJECT = 5
DESPAWNABLE_TYPES = {1: "EVENT", 2: "PLACEABLE", 3: "HOUSE", 4: "HOUSE_DOOR", 5: "TOWN_OBJECT", 6: "DOOR_STATE1", 7: "DOOR_STATE2", 8: "SHIELD"}


@dataclass
class ModelNode:
	"""a node of the loaded models map: (geometry name, mesh) children and the node's collision intentions (OR of the meshes)"""

	children: list[tuple[str, MeshModel]]
	intentions: int


@dataclass
class PlacedGeometry:
	"""a Geometry attached to a map"""

	order: int  # attach order of its node in the map
	child_index: int
	name: str
	mesh: MeshModel
	matrix: tuple
	node_type: int  # 0 for a plain Node, otherwise the DespawnableType id
	node_id: int
	node_intentions: int
	# the name createZone gives a geometry with the MATERIAL intention, without the map id (the zone name is it + "_" + map id); None otherwise
	zone_geometry_name: str | None = None


@dataclass
class TerrainFile:
	path: Path
	width: int
	height: int
	bit_depth: int
	color_type: int


@dataclass
class MapTerrain:
	heightmap: TerrainFile | None = None
	materials: TerrainFile | None = None


@dataclass
class LoadResult:
	mesh_entries: int = 0
	meshes: int = 0
	mesh_names: int = 0
	geo_files: int = 0
	placements: int = 0
	missing_mesh_placements: int = 0
	attached_nodes: int = 0
	geometries: int = 0
	placement_geometries: int = 0  # the geometries of the records' own nodes (without the higher town level nodes)
	material_geometries: int = 0
	terrain_maps: int = 0
	zone_names: list[str] = field(default_factory=list)
	# per zone name: (map id, geometry name without the map id, Mesh.getMaterialId) - the inputs of ZoneService.createMaterialZoneTemplate
	material_zones: list[tuple[str, int, str, int]] = field(default_factory=list)
	despawnable_nodes: dict[str, int] = field(default_factory=dict)
	map_ids: list[int] = field(default_factory=list)
	maps_with_entities: list[int] = field(default_factory=list)
	terrains: dict[int, MapTerrain] = field(default_factory=dict)
	models: dict[str, ModelNode] = field(default_factory=dict)


def byte(value: int) -> int:
	value &= 0xFF
	return value - 0x100 if value >= 0x80 else value


def build_models(entries: list[MeshEntry]) -> dict[str, ModelNode]:
	models: dict[str, ModelNode] = {}
	for entry in entries:
		intentions = 0
		for model in entry.models:
			intentions = byte(intentions | model.intentions)
		children = [(entry.name, model) for model in entry.models]
		if "|" not in entry.name:
			models[entry.name] = ModelNode(children, intentions)
			continue
		aliases = entry.name.split("|")
		while aliases and aliases[-1] == "":
			aliases.pop()  # Java String.split drops trailing empty strings
		for alias in aliases:
			if not children:
				raise GeoOracleError(f"{entry.name}: Java throws NullPointerException for an alias of a node without children")
			renamed = [(alias, children[0][1])] + children[1:]
			models[alias] = ModelNode(renamed, intentions)
	return models


def mesh_aabb(mesh: MeshModel):
	if mesh.aabb is None:
		if len(mesh.vertices) <= 2:
			raise GeoOracleError("Java throws IllegalArgumentException for a mesh with fewer than 3 vertex values")
		mesh.aabb = contain_aabb(mesh.vertices)
	return mesh.aabb


def zone_base_name(geometry_name: str) -> str:
	index = geometry_name.rfind("/")
	dot_index = geometry_name.rfind(".")
	if dot_index < index + 1:
		raise GeoOracleError(f"{geometry_name}: Java throws StringIndexOutOfBoundsException in createZone")
	return geometry_name[index + 1:dot_index].upper()


def terrain_files(geo_dir: Path, map_ids: list[int]) -> tuple[dict[int, MapTerrain], list[str]]:
	"""GeoWorldLoader.loadTerrains without decoding: the heightmap and materials file of each map (the size checks use the PNG headers)"""
	import struct

	terrains: dict[int, MapTerrain] = {}
	unassociated = []
	for path in sorted(p for p in geo_dir.iterdir() if p.is_file() and p.name.lower().endswith(".png")):
		with path.open("rb") as handle:
			head = handle.read(33)
		width, height, bit_depth, color_type = struct.unpack(">IIBB", head[16:26])
		parts = path.name.split(",")
		while parts and parts[-1] == "":
			parts.pop()
		map_parts = set(parts)
		info = TerrainFile(path, width, height, bit_depth, color_type)
		for map_id in map_ids:
			if not map_parts:
				break
			matching = {part for part in map_parts if part.startswith(str(map_id))}
			if not matching:
				continue
			map_parts -= matching
			terrain = terrains.setdefault(map_id, MapTerrain())
			samples = width * height * CHANNELS.get(color_type, 1)
			if bit_depth == 16:
				if terrain.materials is not None and (width < terrain.materials.width or height < terrain.materials.height):
					raise GeoOracleError(f"{path.name}: Java: Terrain heightmap must not be smaller than terrain materials")
				if samples != width * height:
					raise GeoOracleError(f"{path.name}: Java: Expected terrain heightmap length differs")
				terrain.heightmap = info
			else:
				if terrain.heightmap is not None and (width > terrain.heightmap.width or height > terrain.heightmap.height):
					raise GeoOracleError(f"{path.name}: Java: Terrain materials need a terrain heightmap of at least the same size")
				if bit_depth != 8 or samples != width * height:
					raise GeoOracleError(f"{path.name}: Java: Expected terrain materials length differs")
				terrain.materials = info
		unassociated.extend(f"{part} of {path.name}" for part in sorted(map_parts))
	return terrains, unassociated


def load(geo_dir: Path, world_maps_xml: Path, probe_maps: set[int] | None = None,
		placed: dict[int, list[PlacedGeometry]] | None = None) -> LoadResult:
	"""Emulates GeoWorldLoader.load for the maps of world_maps.xml. For the maps in probe_maps the attached geometries are stored in `placed`."""
	result = LoadResult()
	result.map_ids = read_world_map_ids(world_maps_xml)
	attributes = read_world_map_attributes(world_maps_xml)
	result.terrains, _ = terrain_files(geo_dir, result.map_ids)
	result.terrain_maps = sum(1 for map_id in result.map_ids if result.terrains.get(map_id, MapTerrain()).heightmap is not None)

	entries = read_meshes(geo_dir / "models.mesh")
	result.mesh_entries = len(entries)
	result.meshes = sum(len(entry.models) for entry in entries)
	result.models = build_models(entries)
	result.mesh_names = len(result.models)

	for map_id in result.map_ids:
		geo_file = geo_dir / f"{map_id}.geo"
		if not geo_file.is_file():
			template = attributes[map_id]
			if template is None:
				raise GeoOracleError(f"{map_id}: no world map template (Java: NullPointerException)")
			continue
		result.geo_files += 1
		keep = probe_maps is not None and map_id in probe_maps
		if keep:
			placed[map_id] = []
		order = 0

		def attach(model: ModelNode, record: Placement, node_type: int, node_id: int, town_level_node: bool = False):
			nonlocal order
			matrix = world_matrix(record.rotation, record.loc, record.scale)
			count = len(model.children)
			for child_index, (name, mesh) in enumerate(model.children):
				result.geometries += 1
				if not town_level_node:
					result.placement_geometries += 1
				geometry_name = None
				if mesh.intentions & MATERIAL:
					center, extents = mesh_aabb(mesh)
					world_center, _ = transform_box(center, extents, matrix)
					base = zone_base_name(name)
					if count > 1:
						base += f"_CHILD{child_index + 1}"
					geometry_name = f"{base}_{vector_hash(world_center)}"
					result.zone_names.append(f"{geometry_name}_{map_id}")
					result.material_zones.append((f"{geometry_name}_{map_id}", map_id, geometry_name, mesh.material_id))
					result.material_geometries += 1
				if keep:
					placed[map_id].append(PlacedGeometry(order, child_index, name, mesh, matrix, node_type, node_id, model.intentions, geometry_name))
			result.attached_nodes += 1
			if node_type:
				label = DESPAWNABLE_TYPES[node_type]
				result.despawnable_nodes[label] = result.despawnable_nodes.get(label, 0) + 1
			order += 1

		records = read_placements(geo_file)
		result.placements += len(records)
		for record in records:
			model = result.models.get(record.name)
			if model is None:
				result.missing_mesh_placements += 1
				continue
			node_type = record.type if record.type > 0 else 0
			if node_type:
				if node_type not in DESPAWNABLE_TYPES:
					raise GeoOracleError(f"{geo_file.name}: Java: Invalid ID {record.type}")
				if node_type == TOWN_OBJECT:
					if record.level > 8:
						raise GeoOracleError(f"{geo_file.name}: Java: {record.level} doesn't fit in bit mask")
				elif record.level != 0:
					raise GeoOracleError(f"{geo_file.name}: Java: Unexpected value in town level field for non-town entity")
				if node_type == 8:
					raise GeoOracleError(f"{geo_file.name}: Java: SHIELD is not implemented (GeoMap.attachChild)")
			attach(model, record, node_type, record.id)
			if node_type == TOWN_OBJECT:
				for town_level in range(record.level + 1, 6):
					town_model = result.models.get(record.name.replace("_01.cgf", f"_0{town_level}.cgf"))
					if town_model is not None:
						attach(town_model, record, node_type, record.id, True)
		if order > 0:
			result.maps_with_entities.append(map_id)
	return result


def read_heightmap(terrain: MapTerrain):
	"""the heightmap samples of a map (unsigned 16-bit values) with its sizes, or None"""
	if terrain is None or terrain.heightmap is None:
		return None
	png = read_png(terrain.heightmap.path)
	return png.width, png.height, png.samples
