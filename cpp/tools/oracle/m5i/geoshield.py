"""m5i-spots, the geo half: the points of C5's shield crossing when geo shields are on (lane-b/m5i-rest, the geo variant).

Java rules (paths below game-server/src/com/aionemu/gameserver):
- every placed geometry with the MATERIAL collision intention and material 11 becomes a SiegeShield (ZoneService.createMaterialZoneTemplate:
  `if (geometry.getMaterialId() == 11)` -> ShieldService.tryRegisterShield), and ShieldService.attachShield gives the shields inside a fortress
  to it and removes the fortress's sphere (ShieldService.java:70-85), so with geo shields on the sphere of siege_shields.xml is not used;
- the shield's zone is the MaterialZoneTemplate of the geometry's world bound: a sphere of radius |extents| + 1 around the bound's center
  (tools/oracle/m5b3/materials.py zone_area, MaterialZoneTemplate.java), entering it adds a CollisionDieActor for an enemy of the owner
  (SiegeShield.onEnterZone);
- CollisionDieActor is a PASS check (AbstractCollisionObserver.moved): a ray from the new position + COLLISION_CHECK_Z_OFFSET (1) back to
  the previous one, limited to their distance, against the shield geometry's triangles; any hit kills while the fortress is under shield.

The crossing is taken on a horizontal line at the height of the geometry's world bound center, from S1 towards that center: P is the first
triangle hit; M1 lies 3 m before P, M2 3 m after it. The player walks at z = P.z - 1 so the observer's rays run at P.z. A step of at most 1 m
just before P lies inside the zone sphere (the mesh is inside its bound box, the sphere is the box's corner distance + 1).
"""

from __future__ import annotations

import math
from pathlib import Path

from geo import loader as geo_loader
from geo.jgeo import contain_aabb, m_mult, ray_intersects_t, transform_box

RESHANTA = 400010000
SHIELD_MATERIAL = 11
COLLISION_CHECK_Z_OFFSET = 1.0


def _world_triangles(geometry):
	mesh = geometry.mesh
	vertices = mesh.vertices
	points = [m_mult(geometry.matrix, (vertices[i], vertices[i + 1], vertices[i + 2])) for i in range(0, len(vertices), 3)]
	indices = mesh.indices
	return [(points[indices[i]], points[indices[i + 1]], points[indices[i + 2]]) for i in range(0, len(indices) - 2, 3)]


def geo_shield_report(geo_dir: Path, world_maps_xml: Path, s1: tuple, sphere_center: tuple, max_distance: float = 150.0) -> dict | None:
	"""The 1131 shield geometry nearest to the sphere center (within max_distance), its zone sphere and the crossing M1/M2 from S1; None
	without a geo shield there or without a hit."""
	placed: dict = {}
	geo_loader.load(Path(geo_dir), Path(world_maps_xml), {RESHANTA}, placed)
	return shield_crossing(placed.get(RESHANTA, []), s1, sphere_center, max_distance)


def shield_crossing(geometries, s1: tuple, sphere_center: tuple, max_distance: float = 150.0) -> dict | None:
	"""geo_shield_report over placed geometries (tools/oracle/geo/loader.py PlacedGeometry)."""
	best = None
	for geometry in geometries:
		if geometry.mesh.material_id != SHIELD_MATERIAL or not geometry.mesh.intentions & geo_loader.MATERIAL:
			continue
		center, extents = geo_loader.mesh_aabb(geometry.mesh)
		world_center, world_extents = transform_box(center, extents, geometry.matrix)
		distance = math.dist(world_center[:2], sphere_center[:2])
		if distance <= max_distance and (best is None or distance < best[0]):
			best = (distance, geometry, world_center, world_extents)
	if best is None:
		return None
	_, geometry, center, extents = best
	radius = math.sqrt(extents[0] ** 2 + extents[1] ** 2 + extents[2] ** 2) + 1.0
	height = center[2]
	origin = (s1[0], s1[1], height)
	dx, dy = center[0] - s1[0], center[1] - s1[1]
	length = math.hypot(dx, dy)
	direction = (dx / length, dy / length, 0.0)
	hits = [t for v0, v1, v2 in _world_triangles(geometry) if math.isfinite(t := ray_intersects_t(origin, direction, v0, v1, v2)) and t > 0]
	if not hits:
		return None
	t = min(hits)
	point = (origin[0] + direction[0] * t, origin[1] + direction[1] * t, height)

	def along(offset):
		return [round(point[0] + direction[0] * offset, 3), round(point[1] + direction[1] * offset, 3), round(height - COLLISION_CHECK_Z_OFFSET, 3)]

	return {"geometry": geometry.name, "center": [round(c, 3) for c in center], "extents": [round(e, 3) for e in extents],
	        "zoneRadius": round(radius, 3), "hit": [round(c, 3) for c in point], "distanceFromS1": round(t, 3),
	        "M1": along(-3.0), "M2": along(3.0),
	        "M1inZone": math.dist((point[0] - direction[0] * 3, point[1] - direction[1] * 3, height), center) < radius,
	        "stepBeforeHitInZone": math.dist((point[0] - direction[0], point[1] - direction[1], height), center) < radius}
