"""getZ probes: GeoMap.getZ(x, y, zMax, zMin, instanceId) evaluated by brute force over the triangles of the geometries on the ray.

The Java path (GeoMap.getZ, Node/DespawnableNode/Geometry.collideWith, BIHTree.collideWithRay, BIHNode.intersectWhere, Terrain.collideAtOrigin)
is emulated in float32 for every triangle a geometry's gates let through; the BIH tree only prunes triangles that cannot be hit, so the brute
force finds the same collisions. The node bounds above the geometries are merged boxes that contain the geometry bounds and are not
emulated. A probe is drawn again when its result could depend on rounding at a boundary (a hit near a triangle edge or a geometry bound,
two hits at the same distance with different heights) or when a geometry on the ray has a singular world matrix (Java throws).
"""

from __future__ import annotations

import math
import random
from dataclasses import dataclass

from .javamath import INF, NAN, add, f32, float_compare, float_to_int, float_to_raw_uint_bits, mul, sub
from .jgeo import (box_collide_ray_count, box_contains, box_intersects_ray, m_invert, m_mult, m_mult_normal, ray_intersect_where, ray_intersects_t,
	transform_box, v_distance, v_normalize, v_sub)
from .loader import PlacedGeometry, mesh_aabb

PHYSICAL = 1
CELL = 64.0


class Ambiguous(Exception):
	"""the probe result may depend on rounding at a boundary, or Java would throw"""


@dataclass
class IndexedGeometry:
	geometry: PlacedGeometry
	center: tuple
	extents: tuple


class MapScene:
	"""the geometries of one map with their world bounds in a 2D grid, and the map's heightmap"""

	def __init__(self, map_id: int, geometries: list[PlacedGeometry], heightmap):
		self.map_id = map_id
		self.heightmap = heightmap
		self.flat = heightmap is not None and len(heightmap[2]) > 0 and min(heightmap[2]) == max(heightmap[2])
		self.cells: dict[tuple[int, int], list[IndexedGeometry]] = {}
		self.geometries = []
		for geometry in geometries:
			center, extents = mesh_aabb(geometry.mesh)
			world_center, world_extents = transform_box(center, extents, geometry.matrix)
			indexed = IndexedGeometry(geometry, world_center, world_extents)
			self.geometries.append(indexed)
			if not all(math.isfinite(v) for v in world_center + world_extents):
				raise Ambiguous(f"{map_id}: non-finite geometry bound")
			x0 = math.floor((world_center[0] - world_extents[0] - 1) / CELL)
			x1 = math.floor((world_center[0] + world_extents[0] + 1) / CELL)
			y0 = math.floor((world_center[1] - world_extents[1] - 1) / CELL)
			y1 = math.floor((world_center[1] + world_extents[1] + 1) / CELL)
			if (x1 - x0 + 1) * (y1 - y0 + 1) > 4096:
				raise Ambiguous(f"{map_id}: a geometry bound too large for the grid")
			for cx in range(x0, x1 + 1):
				for cy in range(y0, y1 + 1):
					self.cells.setdefault((cx, cy), []).append(indexed)

	def candidates(self, x: float, y: float) -> list[IndexedGeometry]:
		result = self.cells.get((math.floor(x / CELL), math.floor(y / CELL)), [])
		return [g for g in result if abs(x - g.center[0]) <= g.extents[0] + 0.01 and abs(y - g.center[1]) <= g.extents[1] + 0.01]

	# ---- Terrain ------------------------------------------------------------------------------------------------------------------------------

	def terrain_z_at(self, x_index: int, y_index: int) -> float:
		"""Terrain.getZ(xIndex, yIndex)"""
		width, height, samples = self.heightmap
		if x_index < 0 or y_index < 0 or x_index > width or y_index > height:
			return NAN
		if x_index == 0 or y_index == 0 or x_index == width or y_index == height:
			return 0.0
		value = samples[0] if self.flat else samples[y_index + x_index * height]
		return NAN if value == 0xFFFF else f32((value * 2048) / 65536.0)

	def terrain_hit(self, origin, direction, limit):
		"""Terrain.collideAtOrigin -> collideNearXY(origin.x, origin.y, ...): (contact point, distance) or None"""
		if self.heightmap is None:
			return None
		x, y = origin[0], origin[1]
		x_north = float_to_int(f32(x / 2.0))
		y_west = float_to_int(f32(y / 2.0))
		y_east = y_west + 1
		z2 = self.terrain_z_at(x_north, y_east)
		if math.isnan(z2):
			return None
		x_south = x_north + 1
		z3 = self.terrain_z_at(x_south, y_west)
		if math.isnan(z3):
			return None
		z1 = self.terrain_z_at(x_north, y_west)
		z4 = self.terrain_z_at(x_south, y_east)
		xn, yw = x_north * 2, y_west * 2
		ye, xs = yw + 2, xn + 2
		p2 = (f32(float(xn)), f32(float(ye)), z2)
		p3 = (f32(float(xs)), f32(float(yw)), z3)
		contact = None
		if not math.isnan(z1):
			contact = ray_intersect_where(origin, direction, (f32(float(xn)), f32(float(yw)), z1), p2, p3)
		if contact is None and not math.isnan(z4):
			contact = ray_intersect_where(origin, direction, (f32(float(xs)), f32(float(ye)), z4), p2, p3)
		if contact is None:
			return None
		distance = v_distance(contact, origin)
		if distance > limit:
			return None
		# a contact near the diagonal or the cell border could switch triangles or cells under rounding
		fx, fy = x / 2.0 - x_north, y / 2.0 - y_west
		if min(fx, fy, 1 - fx, 1 - fy, abs(1 - fx - fy)) < 1e-4:
			raise Ambiguous("terrain cell border")
		return contact, distance

	# ---- getZ ---------------------------------------------------------------------------------------------------------------------------------

	def get_z(self, x: float, y: float, z_max: float, z_min: float, theme_id: int = 0) -> tuple[float, str]:
		origin = (x, y, z_max)
		direction = v_normalize(v_sub((x, y, z_min), origin))
		limit = sub(z_max, z_min)
		hits = []  # (distance, contact z, source)
		for indexed in self.candidates(x, y):
			self._collide_geometry(indexed, origin, direction, limit, theme_id, hits)
		terrain = self.terrain_hit(origin, direction, limit)
		if terrain is not None:
			hits.append((terrain[1], terrain[0][2], "terrain"))
		hits = [hit for hit in hits if not math.isnan(hit[0])]
		if not hits:
			return NAN, "none"
		best = hits[0]
		for hit in hits[1:]:
			if float_compare(hit[0], best[0]) < 0:
				best = hit
		for hit in hits:
			if hit is not best and abs(hit[0] - best[0]) <= 2e-3 * max(1.0, best[0]) and hit[1] != best[1]:
				raise Ambiguous("two collisions at nearly the same distance")
		return best[1], best[2]

	def _collide_geometry(self, indexed: IndexedGeometry, origin, direction, limit, theme_id, hits, through_map: bool = True):
		"""the collisions of one geometry with the ray. `through_map` False is a direct Geometry.collideWith on the geometry itself - what
		AbstractCollisionObserver's TOUCH check does with a material zone's geometry (m5b3/materials.py stand_report) - which asks neither the
		node's type nor any intention (Geometry.java:118-131, Mesh.java:102-109); True is the map's collideWith of getZ, which does"""
		geometry = indexed.geometry
		node_type = geometry.node_type
		if through_map:
			if node_type == 1:  # EVENT
				if theme_id != geometry.node_id:
					return
			elif node_type != 0 and node_type != 3:  # not HOUSE: inactive until setActive (instances start empty)
				return
			if geometry.node_intentions & PHYSICAL == 0 or geometry.mesh.intentions & PHYSICAL == 0:
				return
		if not box_intersects_ray(indexed.center, indexed.extents, origin, direction):
			return
		if box_collide_ray_count(indexed.center, indexed.extents, origin, direction, limit) == 0 and not box_contains(indexed.center, indexed.extents, origin):
			return
		inverse = m_invert(geometry.matrix)
		if inverse is None:
			raise Ambiguous("singular world matrix (Java: ArithmeticException)")
		local_origin = m_mult(inverse, origin)
		local_direction = v_normalize(m_mult_normal(inverse, direction))
		near_bound = (abs(abs(origin[0] - indexed.center[0]) - indexed.extents[0]) < 1e-3
			or abs(abs(origin[1] - indexed.center[1]) - indexed.extents[1]) < 1e-3)
		vertices = geometry.mesh.vertices
		indices = geometry.mesh.indices
		lox, loy, loz = local_origin
		ldx, ldy, ldz = local_direction
		matrix = geometry.matrix
		for triangle in range(len(indices) // 3):
			i0, i1, i2 = indices[triangle * 3] * 3, indices[triangle * 3 + 1] * 3, indices[triangle * 3 + 2] * 3
			ax, ay, az = vertices[i0], vertices[i0 + 1], vertices[i0 + 2]
			bx, by, bz = vertices[i1], vertices[i1 + 1], vertices[i1 + 2]
			cx, cy, cz = vertices[i2], vertices[i2 + 1], vertices[i2 + 2]
			# double precision Möller-Trumbore prefilter with a generous margin
			e1x, e1y, e1z = bx - ax, by - ay, bz - az
			e2x, e2y, e2z = cx - ax, cy - ay, cz - az
			px, py, pz = ldy * e2z - ldz * e2y, ldz * e2x - ldx * e2z, ldx * e2y - ldy * e2x
			det = e1x * px + e1y * py + e1z * pz
			if abs(det) < 1e-12:  # Java treats |dirDotNorm| <= FLT_EPSILON as parallel
				continue
			sx, sy, sz = lox - ax, loy - ay, loz - az
			u = (sx * px + sy * py + sz * pz) / det
			if u < -1e-3 or u > 1 + 1e-3:
				continue
			qx, qy, qz = sy * e1z - sz * e1y, sz * e1x - sx * e1z, sx * e1y - sy * e1x
			v = (ldx * qx + ldy * qy + ldz * qz) / det
			if v < -1e-3 or u + v > 1 + 1e-3:
				continue
			margin = min(u, v, 1 - u - v)
			if abs(margin) < 1e-5 or (near_bound and margin >= 0):
				raise Ambiguous("ray near a triangle edge or a geometry bound")
			v0, v1, v2 = (ax, ay, az), (bx, by, bz), (cx, cy, cz)
			t = ray_intersects_t(local_origin, local_direction, v0, v1, v2)
			if t == INF:
				continue
			w0, w1, w2 = m_mult(matrix, v0), m_mult(matrix, v1), m_mult(matrix, v2)
			t_world = ray_intersects_t(origin, direction, w0, w1, w2)
			contact = (add(mul(direction[0], t_world), origin[0]), add(mul(direction[1], t_world), origin[1]), add(mul(direction[2], t_world), origin[2]))
			distance = v_distance(origin, contact)
			if distance > limit:
				continue
			hits.append((distance, contact[2], "mesh"))


def float_record(value: float) -> dict:
	return {"bits": float_to_raw_uint_bits(value), "value": repr(value)}


def draw_probes(scenes: dict[int, MapScene], seed: int, mesh_probes: int, terrain_probes: int, random_probes: int) -> list[dict]:
	rng = random.Random(seed)
	map_ids = sorted(scenes)
	terrain_maps = [m for m in map_ids if scenes[m].heightmap is not None]
	probes = []

	def evaluate(kind: str, map_id: int, x: float, y: float, z_max: float, z_min: float) -> bool:
		scene = scenes[map_id]
		try:
			z, source = scene.get_z(x, y, z_max, z_min)
		except Ambiguous:
			return False
		probes.append({"kind": kind, "map": map_id, "x": float_record(x), "y": float_record(y), "zMax": float_record(z_max),
			"zMin": float_record(z_min), "instanceId": 1, "z": float_record(z), "source": source})
		return True

	attempts = 0
	while sum(1 for p in probes if p["kind"] == "mesh") < mesh_probes:
		attempts += 1
		if attempts > mesh_probes * 50:
			raise Ambiguous("too many rejected mesh probes")
		scene = scenes[rng.choice(map_ids)]
		physical = [g for g in scene.geometries if g.geometry.mesh.intentions & PHYSICAL and g.geometry.node_type in (0, 3)]
		if not physical:
			continue
		indexed = rng.choice(physical)
		mesh = indexed.geometry.mesh
		triangle = rng.randrange(len(mesh.indices) // 3)
		corners = [m_mult(indexed.geometry.matrix, tuple(mesh.vertices[mesh.indices[triangle * 3 + k] * 3 + c] for c in range(3))) for k in range(3)]
		wx = sum(c[0] for c in corners) / 3
		wy = sum(c[1] for c in corners) / 3
		wz = sum(c[2] for c in corners) / 3
		evaluate("mesh", scene.map_id, f32(wx), f32(wy), f32(wz + 4.0), f32(wz - 16.0))

	attempts = 0
	while sum(1 for p in probes if p["kind"] == "terrain") < terrain_probes:
		attempts += 1
		if attempts > terrain_probes * 50:
			raise Ambiguous("too many rejected terrain probes")
		scene = scenes[rng.choice(terrain_maps)]
		width, height, _ = scene.heightmap
		x = f32(round(rng.uniform(2.0, width * 2.0 - 2.0), 2))
		y = f32(round(rng.uniform(2.0, height * 2.0 - 2.0), 2))
		ground = scene.terrain_z_at(int(x / 2), int(y / 2))
		base = 0.0 if math.isnan(ground) else ground
		z_max = f32(round(base + rng.uniform(1.0, 40.0), 2))
		evaluate("terrain", scene.map_id, x, y, z_max, f32(z_max - 200.0))

	attempts = 0
	while sum(1 for p in probes if p["kind"] == "random") < random_probes:
		attempts += 1
		if attempts > random_probes * 50:
			raise Ambiguous("too many rejected random probes")
		scene = scenes[rng.choice(map_ids)]
		x = f32(round(rng.uniform(0.0, 4096.0), 2))
		y = f32(round(rng.uniform(0.0, 4096.0), 2))
		z_max = f32(round(rng.uniform(0.0, 3000.0), 2))
		evaluate("random", scene.map_id, x, y, z_max, f32(z_max - rng.uniform(1.0, 60.0)))
	return probes
