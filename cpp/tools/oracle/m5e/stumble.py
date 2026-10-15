"""m5e-stumble: what GeoService.getClosestCollision gives a stumbled npc on open ground (m5e-plan.md §10.3 X9g, W-27), from the geo data.

StumbleEffect.calculate (skillengine/effect/StumbleEffect.java:66-70) asks GeoService.getClosestCollision(effected, x + 2 cos, y + 2 sin, z),
which is GeoMap.getClosestCollision(x, y, z, tx, ty, z, atNearGroundZ = true, ..., CollisionIntention.DEFAULT_COLLISIONS, ...)
(world/geo/GeoService.java:124-131, geoEngine/models/GeoMap.java:137-155):
- the ray from (x, y, z + COLLISION_CHECK_Z_OFFSET) to (tx, ty, z + COLLISION_CHECK_Z_OFFSET), COLLISION_CHECK_Z_OFFSET = 1 (GeoMap.java:32);
- no collision on it: the end (tx, ty, z) with z = getZ(tx, ty, z + 1, z - 2) where that finds a surface (GeoMap.java:141-148);
- a collision: the origin itself when it is within 0.55 m, else the contact point set back 0.5 m (GeoMap.java:149-154).

This module answers the first case only - where it can say the segment is OPEN, the end's z is getZ's (tools/oracle/geo/probes.py, the
float32 emulation GeoRealDataTest checks the C++ engine against). "Open" is decided conservatively, so a segment it calls open has no
collision in Java:
- no geometry with a DEFAULT_COLLISIONS intention crosses the ray: the geometries whose bounds contain a point of the segment (sampled every
  0.1 m) are tested triangle by triangle with the ray itself (probes.MapScene._collide_geometry);
- the terrain cannot cross it: at every sample the topmost PHYSICAL surface found from 5 m above the ray down is at least CLEARANCE below the
  ray (a heightmap is piecewise planar between 2 m samples, so between two 0.1 m samples it cannot rise by more than the slope allows; the
  clearance covers it). A surface above the ray (a roof, a tree) makes the segment not open as well;
- any probe near a rounding boundary (probes.Ambiguous) makes it "ambiguous", never open.
"""

from __future__ import annotations

import math
from pathlib import Path

from staticdata_oracle import OracleError

from geo import GeoOracleError
from geo import loader as geo_loader
from geo.javamath import add, f32, sub
from geo.jgeo import v_normalize, v_sub
from geo.probes import Ambiguous, MapScene

COLLISION_CHECK_Z_OFFSET = 1.0  # GeoMap.java:32
# CollisionIntention.DEFAULT_COLLISIONS = PHYSICAL | DOOR | PHYSICAL_SEE_THROUGH (CollisionIntention.java)
DEFAULT_COLLISIONS = 1 | (1 << 4) | (1 << 7)
SAMPLE_STEP = 0.1
CLEARANCE = 0.3
PROBE_ABOVE = 5.0


def load_scene(geo_dir: Path, world_maps_xml: Path, map_id: int) -> MapScene:
	placed: dict[int, list] = {}
	try:
		result = geo_loader.load(geo_dir, world_maps_xml, {map_id}, placed)
	except GeoOracleError as e:
		raise OracleError(str(e)) from e
	if map_id not in result.map_ids:
		raise OracleError(f"map {map_id} is not a world map of {world_maps_xml}")
	return MapScene(map_id, placed.get(map_id, []), geo_loader.read_heightmap(result.terrains.get(map_id)))


def _samples(fx: float, fy: float, tx: float, ty: float) -> list[tuple[float, float]]:
	length = math.hypot(tx - fx, ty - fy)
	count = max(1, math.ceil(length / SAMPLE_STEP))
	return [(fx + (tx - fx) * i / count, fy + (ty - fy) * i / count) for i in range(count + 1)]


def stumble_end(scene: MapScene, fx: float, fy: float, fz: float, tx: float, ty: float) -> dict:
	"""the segment from the npc's position (fx, fy, fz) to the stumble's end (tx, ty): open or not, and getZ's z at the end"""
	fx, fy, fz, tx, ty = f32(fx), f32(fy), f32(fz), f32(tx), f32(ty)
	report: dict = {"from": [fx, fy, fz], "to": [tx, ty], "open": False, "reason": None, "groundZ": None,
	                "length": math.hypot(tx - fx, ty - fy)}
	ray_z = add(fz, COLLISION_CHECK_Z_OFFSET)
	samples = _samples(fx, fy, tx, ty)
	report["samples"] = len(samples)
	try:
		# the ray of getCollisions (GeoMap.java:206-217) against every geometry whose bound holds a point of the segment
		origin = (fx, fy, ray_z)
		target = (tx, ty, ray_z)
		limit = math.dist(origin, target)
		if limit > 0:
			direction = v_normalize(v_sub(target, origin))
			seen: set[int] = set()
			hits: list = []
			for x, y in samples:
				for indexed in scene.candidates(x, y):
					if id(indexed) in seen:
						continue
					seen.add(id(indexed))
					scene._collide_geometry(indexed, origin, direction, f32(limit), 0, hits, intentions=DEFAULT_COLLISIONS)
			if hits:
				report["reason"] = f"a geometry crosses the ray {min(h[0] for h in hits):.3f} m from its origin"
				return report
		# the ground under the ray: no surface within CLEARANCE of it, none above it
		highest = -math.inf
		for x, y in samples:
			surface, _source = scene.get_z(x, y, add(ray_z, PROBE_ABOVE), sub(fz, 3.0))
			if math.isnan(surface):
				report["reason"] = f"no ground at ({x:.3f}, {y:.3f})"
				return report
			highest = max(highest, surface)
		report["highestSurface"] = highest
		if highest > ray_z - CLEARANCE:
			report["reason"] = f"a surface at {highest:.3f} reaches the ray at {ray_z:.3f} (clearance {CLEARANCE})"
			return report
		# GeoMap.getClosestCollision without a collision: getZ(end.x, end.y, end.z + 1, end.z - 2) (GeoMap.java:142-147)
		ground, source = scene.get_z(tx, ty, add(fz, 1.0), sub(fz, 2.0))
	except Ambiguous as e:
		report["reason"] = f"ambiguous: {e}"
		return report
	report["open"] = True
	report["groundZ"] = None if math.isnan(ground) else ground
	report["groundSource"] = source
	return report


def stumble_report(geo_dir: Path, world_maps_xml: Path, map_id: int, stumbles: list[tuple[float, float, float, float, float]]) -> dict:
	scene = load_scene(geo_dir, world_maps_xml, map_id)
	return {"format": "aion-m5e-stumble", "version": 1, "map": map_id,
	        "stumbles": [stumble_end(scene, *stumble) for stumble in stumbles]}
