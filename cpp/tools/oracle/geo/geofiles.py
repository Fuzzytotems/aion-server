"""Readers of models.mesh and <mapId>.geo (big endian, GeoWorldLoader.loadMeshes and loadWorld) and of the world map ids."""

from __future__ import annotations

import re
import struct
import sys
from array import array
from dataclasses import dataclass, field
from pathlib import Path

from . import GeoOracleError

_SHORT = struct.Struct(">h")
_USHORT = struct.Struct(">H")
_PLACEMENT = struct.Struct(">15fbhb")  # loc (3), rotation (9), scale (3), type, id, level


@dataclass
class MeshModel:
	"""One model (Mesh) of an entry: float32 vertices (x, y, z per vertex), triangle vertex indices, material and collision intentions."""

	vertices: array
	indices: array
	index_size: int
	material_id: int  # unsigned (Mesh.getMaterialId)
	intentions: int  # signed byte (Mesh.getCollisionIntentions)
	aabb: tuple | None = None  # (center, extents) of BoundingBox.containAABB, computed on demand


@dataclass
class MeshEntry:
	name: str
	models: list[MeshModel] = field(default_factory=list)


@dataclass
class Placement:
	name: str
	loc: tuple
	rotation: tuple
	scale: tuple
	type: int
	id: int
	level: int


def read_meshes(path: Path) -> list[MeshEntry]:
	data = path.read_bytes()
	entries = []
	pos = 0
	little = sys.byteorder == "little"
	while pos < len(data):
		if pos + 2 > len(data):
			raise GeoOracleError("models.mesh: truncated entry")
		(name_length,) = _SHORT.unpack_from(data, pos)
		pos += 2
		if name_length < 0:
			raise GeoOracleError("models.mesh: negative name length")
		name = data[pos:pos + name_length].decode("utf-8")
		pos += name_length
		entry = MeshEntry(name)
		model_count = data[pos]
		pos += 1
		for _ in range(model_count):
			(vertex_count,) = _USHORT.unpack_from(data, pos)
			pos += 2
			vertices = array("f")
			vertices.frombytes(data[pos:pos + vertex_count * 12])
			if little:
				vertices.byteswap()
			pos += vertex_count * 12
			(faces,) = _USHORT.unpack_from(data, pos)
			pos += 2
			index_size = struct.unpack_from(">b", data, pos)[0]
			pos += 1
			if index_size == 1:
				indices = array("B", data[pos:pos + faces * 3])
			elif index_size == 2:
				indices = array("H")
				indices.frombytes(data[pos:pos + faces * 6])
				if little:
					indices.byteswap()
			else:
				raise GeoOracleError(f"models.mesh: index size {index_size}")  # Java: IOException -> GameServerError
			pos += faces * 3 * index_size
			material_id = data[pos]
			intentions = struct.unpack_from(">b", data, pos + 1)[0]
			pos += 2
			entry.models.append(MeshModel(vertices, indices, index_size, material_id, intentions))
		entries.append(entry)
	if pos != len(data):
		raise GeoOracleError("models.mesh: trailing bytes")
	return entries


def read_placements(path: Path) -> list[Placement]:
	data = path.read_bytes()
	placements = []
	pos = 0
	while pos < len(data):
		(name_length,) = _SHORT.unpack_from(data, pos)
		pos += 2
		if name_length < 0:
			raise GeoOracleError(f"{path.name}: negative name length")
		name = data[pos:pos + name_length].decode("utf-8")
		pos += name_length
		values = _PLACEMENT.unpack_from(data, pos)
		pos += _PLACEMENT.size
		placements.append(Placement(name, values[0:3], values[3:12], values[12:15], values[15], values[16], values[17]))
	if pos != len(data):
		raise GeoOracleError(f"{path.name}: trailing bytes")
	return placements


def read_world_map_ids(world_maps_xml: Path) -> list[int]:
	"""the ids of <map> in world_maps.xml, in file order (GeoService creates one GeoMap per world map template)"""
	text = world_maps_xml.read_text(encoding="utf-8")
	return [int(value) for value in re.findall(r"<map\s[^>]*?\bid=\"(\d+)\"", text)]


def read_world_map_attributes(world_maps_xml: Path) -> dict[int, dict[str, str]]:
	"""id -> attributes of <map>, for the missing .geo file warning rule of GeoWorldLoader.loadWorld"""
	text = world_maps_xml.read_text(encoding="utf-8")
	result = {}
	for tag in re.findall(r"<map\s[^>]*>", text):
		attributes = dict(re.findall(r"(\w+)=\"([^\"]*)\"", tag))
		result[int(attributes["id"])] = attributes
	return result
