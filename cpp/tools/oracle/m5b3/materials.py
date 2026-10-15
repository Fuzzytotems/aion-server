"""m5b3-material: the skill materials of one map - the mesh placements whose material casts a skill, and the terrain materials (m5b3-plan.md G-01,
§2.6 "Material skills", §10.5, risk 9).

Java rules (paths below game-server/src/com/aionemu/gameserver):
- a material zone per placed geometry with the MATERIAL collision intention: GeoWorldLoader.attachToMapAndCreateZones / createZone
  (geoEngine/GeoWorldLoader.java:240-273), named upper(base name) [+ "_CHILD<n>" when the node has several children] + "_" + the vector hash
  of the geometry's world bound center + "_" + the map id - the name tools/oracle/geo/loader.py computes for every placed geometry
  (PlacedGeometry.zone_geometry_name, the same string as its material_zones, which the C++ geo engine is checked against in GeoRealDataTest),
  `|` mesh aliases and town levels included;
- ZoneService.createMaterialZoneTemplate (world/zone/ZoneService.java:194-235): a zone whose material has no MaterialTemplate
  (DataManager.MATERIAL_DATA, mesh_materials/material_templates.xml) is not created (material 11 is a shield instead); a second geometry with
  a zone name already registered only logs "Duplicate material mesh" - the first one's handler and area stay;
- the zone's area (model/templates/zone/MaterialZoneTemplate.java:13-38) from the geometry's world bound (BoundingBox center and extents): a
  CYLINDER when the geometry name contains "CYLINDER", "CONE" or "H_COLUME" (radius (float) sqrt(xExt^2 + yExt^2) + 1, top center.z + zExt + 1,
  bottom center.z - zExt - 1), a SEMISPHERE when it contains "SEMISPHERE" and a SPHERE otherwise (radius (float) sqrt(xExt^2 + yExt^2 + zExt^2)
  + 1), in float arithmetic (tools/oracle/geo/javamath.py);
- the skills of a material (MaterialTemplate / MaterialSkill: id, level, target, frequency, conditions); a creature inside the zone runs them
  through MaterialZoneHandler -> ZoneCollisionMaterialActor with a TOUCH check (m5b3-plan.md risk 9: which positions count as touching is
  G-04's measurement, not this oracle's);
- terrain materials: TerrainZoneCollisionMaterialActor (controllers/observer/TerrainZoneCollisionMaterialActor.java) reads
  GeoMap.getTerrainMaterialAt (Terrain.getTerrainMaterialAt) from the map's 8-bit materials PNG and runs the skills of that material id; the
  report gives the histogram of the PNG's material ids and which of them have a MaterialTemplate (m5b3-plan.md §12 left this unread).

Both need gameserver.geodata.enable and gameserver.geodata.materials.enable (GeoService.getTerrainMaterialAt, GeoDataConfig.GEO_MATERIALS_ENABLE).
"""

from __future__ import annotations

import math
from pathlib import Path

from staticdata_oracle import OracleError

from geo import GeoOracleError
from geo import loader as geo_loader
from geo.javamath import add, mul, sqrt, sub
from geo.jgeo import transform_box, v_normalize, v_sub
from geo.pngread import read_png
from geo.probes import Ambiguous, IndexedGeometry, MapScene
from m5a.data import StaticData, java_int

SHIELD_MATERIAL = 11  # ZoneService.createMaterialZoneTemplate: `if (geometry.getMaterialId() == 11)` -> ShieldService

# AbstractCollisionObserver.moved, the TOUCH arm (controllers/observer/AbstractCollisionObserver.java:47-64): a vertical ray from
# z + 0.05f + boundRadius.upper down to geoZ - 0.11f, geoZ being GeoService.getZ(worldId, x, y, z, instanceId) = GeoMap.getZ(x, y, z + 2, z - 2)
# (GeoService.java:57-59), or z - 0.11f when that is NaN
TOUCH_ABOVE = 0.05
TOUCH_BELOW = 0.11
GEO_Z_HALF_RANGE = 2.0
# a player's BoundRadius: (0.25, 0.25, PlayerAppearance.getBoundHeight()) (PlayerAccountData.java:99), getBoundHeight = height * 1.75f
# (PlayerAppearance.java:1051-1053); the scenario characters are created with height 1.0 (GameSession.h CharacterAppearance)
PLAYER_BOUND_UPPER = 1.75


def material_templates(data: StaticData) -> dict[int, list[dict]]:
	"""MaterialData: material id -> its <skill>s (a later <material> of the same id replaces the earlier one)."""
	result: dict[int, list[dict]] = {}
	for element in data.children("material_templates", "material"):
		material_id = java_int(element.get("id"), "material id")
		skills = []
		for skill in element.findall("skill"):
			skills.append({"skillId": java_int(skill.get("id"), f"material {material_id} skill id"),
			               "level": java_int(skill.get("level"), f"material {material_id} skill level", 0),
			               "target": skill.get("target"), "frequency": java_int(skill.get("frequency"), f"material {material_id} frequency", 0),
			               "conditions": (skill.get("conditions") or "").split() or None})
		result[material_id] = skills
	return result


def zone_area(geometry_name: str, center: tuple, extents: tuple) -> dict:
	"""MaterialZoneTemplate's area of a geometry (the name as createZone set it) with its world bound."""
	x_ext, y_ext, z_ext = extents
	if "CYLINDER" in geometry_name or "CONE" in geometry_name or "H_COLUME" in geometry_name:
		r = sqrt(add(mul(x_ext, x_ext), mul(y_ext, y_ext)))
		return {"type": "CYLINDER", "x": center[0], "y": center[1], "r": add(r, 1.0), "top": add(add(center[2], z_ext), 1.0),
		        "bottom": sub(sub(center[2], z_ext), 1.0)}
	corner = sqrt(add(add(mul(x_ext, x_ext), mul(y_ext, y_ext)), mul(z_ext, z_ext)))
	kind = "SEMISPHERE" if "SEMISPHERE" in geometry_name else "SPHERE"
	return {"type": kind, "x": center[0], "y": center[1], "z": center[2], "r": add(corner, 1.0)}


def terrain_materials(geo_dir: Path, world_maps_xml: Path, map_id: int, materials: dict[int, list[dict]]) -> dict:
	map_ids = geo_loader.read_world_map_ids(world_maps_xml)
	terrains, _ = geo_loader.terrain_files(geo_dir, map_ids)
	terrain = terrains.get(map_id)
	if terrain is None or terrain.materials is None:
		return {"materialsFile": None, "histogram": {}, "withSkills": {}}
	png = read_png(terrain.materials.path)
	histogram: dict[int, int] = {}
	for sample in png.samples:
		if sample:
			histogram[sample] = histogram.get(sample, 0) + 1
	return {"materialsFile": terrain.materials.path.name, "size": [png.width, png.height],
	        "histogram": {str(k): v for k, v in sorted(histogram.items())},
	        "withSkills": {str(k): {"samples": v, "skills": materials[k]} for k, v in sorted(histogram.items()) if materials.get(k)}}


def inside_area(area: dict, x: float, y: float, z: float) -> bool:
	"""Area.isInside3D of a material zone's area (ZoneInstance.revalidate, ZoneInstance.java:49-50): a SphereArea is PositionUtil.isInRange, a
	squared distance below r^2 (PositionUtil.java:257-262); a SemisphereArea also needs the center below the point (`this.z < z`,
	SemisphereArea.isInside3D); a CylinderArea is AbstractArea.isInside3D, isInsideZ and a 2D distance below r (CylinderArea.java:67-69)"""
	kind = area["type"]
	if kind == "CYLINDER":
		return area["bottom"] <= z <= area["top"] and math.dist((x, y), (area["x"], area["y"])) < area["r"]
	if (x - area["x"]) ** 2 + (y - area["y"]) ** 2 + (z - area["z"]) ** 2 >= area["r"] ** 2:
		return False
	return kind != "SEMISPHERE" or area["z"] < z


def area_depth(area: dict, x: float, y: float, z: float) -> float:
	"""how deep (x, y, z) lies inside inside_area's region (positive, the distance to its nearest boundary) or how far outside it (negative; a
	lower bound of the distance, so a margin read from it is never too generous)"""
	kind = area["type"]
	if kind == "CYLINDER":
		return min(area["r"] - math.dist((x, y), (area["x"], area["y"])), z - area["bottom"], area["top"] - z)
	depth = area["r"] - math.dist((x, y, z), (area["x"], area["y"], area["z"]))
	return min(depth, z - area["z"]) if kind == "SEMISPHERE" else depth


class TouchScene:
	"""the map's geometries for GeoMap.getZ (tools/oracle/geo/probes.py MapScene) and each skill zone's own geometry for the TOUCH check"""

	def __init__(self, map_id: int, geometries: list, heightmap, zone_geometries: dict[str, object]):
		self.scene = MapScene(map_id, geometries, heightmap)
		by_geometry = {id(indexed.geometry): indexed for indexed in self.scene.geometries}
		self.zone_geometry = {name: by_geometry[id(geometry)] for name, geometry in zone_geometries.items()}

	def surface_z(self, x: float, y: float, z_max: float, z_min: float) -> float:
		"""GeoMap.getZ(x, y, zMax, zMin): the PHYSICAL surface nearest zMax, NaN if none (GeoMap.java:114-135). @raises Ambiguous"""
		return self.scene.get_z(x, y, z_max, z_min)[0]

	def touched(self, zone_name: str, x: float, y: float, z: float, bound_upper: float) -> bool:
		"""ZoneCollisionMaterialActor's TOUCH check for a walking player at (x, y, z) (AbstractCollisionObserver.java:47-64, 71-76): the ray from
		z + 0.05 + upper down to geoZ - 0.11 hits the zone's geometry. @raises Ambiguous"""
		geo_z = self.surface_z(x, y, z + GEO_Z_HALF_RANGE, z - GEO_Z_HALF_RANGE)
		z_max = z + TOUCH_ABOVE + bound_upper
		z_min = (z if math.isnan(geo_z) else geo_z) - TOUCH_BELOW
		origin = (x, y, z_max)
		direction = v_normalize(v_sub((x, y, z_min), origin))
		hits: list = []
		self.scene._collide_geometry(self.zone_geometry[zone_name], origin, direction, z_max - z_min, 0, hits, through_map=False)
		return bool(hits)


def stand_report(touch: TouchScene, skill_zones: list[dict], target: dict, bound_upper: float = PLAYER_BOUND_UPPER, step: float = 0.02,
                 min_clearance: float = 0.05, step_off_distance: float = 4.0) -> dict:
	"""m5b3-plan.md G-04 (§10.5, §13 question 3): where a player stands so that `target` is the ONLY skill zone whose material actor acts.

	Every skill zone whose area holds the player gets its own ZoneCollisionMaterialActor (MaterialZoneHandler.onEnterZone), but a creature has
	one ZONE_MATERIAL_ACTION task: AbstractMaterialSkillActor.act schedules the 1 s MaterialSkillTask only when the creature has none
	(AbstractMaterialSkillActor.java:37-44), with the skills of the actor that was touched FIRST - and the zones' collision checks run on the
	thread pool (AbstractCollisionObserver.moved), so where two zones are touched the order, and so which zone's conditions apply, is a race.
	A point where the target zone is inside and touched and every other zone the player is inside is NOT touched makes the ticks the target's.

	The grid covers the target geometry's world bound in `step` metres. A client standing at (x, y) reports the highest PHYSICAL surface there
	(the fire meshes are PHYSICAL | MATERIAL). The chosen point is the valid one farthest from any invalid grid point (its clearance, at least
	`min_clearance`); ties go to the one nearer the zone center. The step-off point is `step_off_distance` from the target center, on the
	ground, inside no skill zone's area, and farthest from the other zones."""
	cx, cy, cz = target["center"]
	ex, ey, ez = target["extents"]
	nearby = [z for z in skill_zones if math.dist(z["center"], target["center"]) <= z["area"]["r"] + target["area"]["r"] + step_off_distance + 2]
	nx = int(math.ceil(2 * ex / step)) + 1
	ny = int(math.ceil(2 * ey / step)) + 1
	grid: dict[tuple[int, int], dict] = {}
	counts = {"points": 0, "noSurface": 0, "ambiguous": 0, "targetInside": 0, "targetTouched": 0, "targetOnly": 0, "othersTouched": 0}
	for i in range(nx):
		for j in range(ny):
			x = cx - ex + i * step
			y = cy - ey + j * step
			counts["points"] += 1
			cell = {"x": x, "y": y, "valid": False}
			grid[(i, j)] = cell
			try:
				z = touch.surface_z(x, y, cz + ez + GEO_Z_HALF_RANGE, cz - ez - GEO_Z_HALF_RANGE)
				if math.isnan(z):
					counts["noSurface"] += 1
					continue
				cell["z"] = z
				inside = [zone["zoneName"] for zone in nearby if inside_area(zone["area"], x, y, z)]
				touched = [name for name in inside if touch.touched(name, x, y, z, bound_upper)]
			except Ambiguous:
				# a ray near a triangle edge: inside the mesh that is an edge two of its own triangles share, which cannot change "touched", so
				# the cell is neither a candidate nor a reason to keep away from its neighbours
				counts["ambiguous"] += 1
				cell["unknown"] = True
				continue
			cell["inside"], cell["touched"] = inside, touched
			if target["zoneName"] in inside:
				counts["targetInside"] += 1
			if target["zoneName"] in touched:
				counts["targetTouched"] += 1
				if touched == [target["zoneName"]]:
					counts["targetOnly"] += 1
					cell["valid"] = True
				else:
					counts["othersTouched"] += 1
	best = None
	for (i, j), cell in grid.items():
		if not cell["valid"]:
			continue
		clearance = math.inf
		for (k, m), other in grid.items():
			if not other["valid"] and not other.get("unknown"):
				clearance = min(clearance, math.hypot((k - i) * step, (m - j) * step))
		# the grid's border: beyond it the target geometry is not under the point, so it counts as invalid
		clearance = min(clearance, (i + 1) * step, (nx - i) * step, (j + 1) * step, (ny - j) * step)
		key = (round(clearance, 6), -math.dist((cell["x"], cell["y"]), (cx, cy)))
		if best is None or key > best[0]:
			best = (key, cell, clearance)
	stand = None
	if best is not None and best[2] >= min_clearance:
		cell = best[1]
		stand = {"x": cell["x"], "y": cell["y"], "z": cell["z"], "clearance": best[2], "inside": cell["inside"], "touched": cell["touched"],
		         "distanceToCenter": math.dist((cell["x"], cell["y"], cell["z"]), target["center"])}
	step_off = None
	others = [z for z in nearby if z["zoneName"] != target["zoneName"]]
	for k in range(16):
		angle = 2 * math.pi * k / 16
		x, y = cx + step_off_distance * math.cos(angle), cy + step_off_distance * math.sin(angle)
		try:
			z = touch.surface_z(x, y, cz + 3, cz - 3)
		except Ambiguous:
			continue
		if math.isnan(z) or any(inside_area(zone["area"], x, y, z) for zone in nearby):
			continue
		spacing = min((math.dist((x, y), zone["center"][:2]) for zone in others), default=math.inf)
		if step_off is None or spacing > step_off["nearestOtherZone"]:
			step_off = {"x": x, "y": y, "z": z, "nearestOtherZone": spacing}
	return {"zoneName": target["zoneName"], "boundUpper": bound_upper, "step": step, "minClearance": min_clearance, "grid": counts,
	        "nearbyZones": [{"zoneName": z["zoneName"], "materialId": z["materialId"], "conditions": [c for s in z["skills"] for c in (s["conditions"] or [])],
	                         "distance": math.dist(z["center"], target["center"])} for z in nearby],
	        "point": stand, "stepOff": step_off, "untouched": untouched_point(touch, nearby, target, bound_upper)}


def untouched_point(touch: TouchScene, nearby: list[dict], target: dict, bound_upper: float = PLAYER_BOUND_UPPER, step: float = 0.05,
                    min_outside_bound: float = 0.25, max_height: float = 1.0, min_margin: float = 0.05) -> dict | None:
	"""m5b3-plan.md §18.2 (the review of stage 2): a point INSIDE the target zone's area - and no other skill zone's - whose TOUCH ray misses
	every mesh, so a correct server creates the target's ZoneCollisionMaterialActor there (MaterialZoneHandler.onEnterZone -> actor.moved())
	and never touches it, and a port that ignores the ray (isTouched = true) starts the fire's task there. The stand point cannot show that:
	it is on the fire mesh, touched either way.

	The columns are a `step` grid over the area's disc, at least `min_outside_bound` outside the target geometry's world bound horizontally
	(no vertical ray there can reach the geometry); in each, the heights are the highest PHYSICAL surface and the heights `step` apart above
	the area's center within `max_height` of that surface - a SEMISPHERE is entered only above its center, and its fire stands on a floor
	below it, so the client may have to report a z above the floor (CM_MOVE takes the client's z, CM_MOVE.java). A candidate's margin is the
	least of its depth inside the target's area and its distance outside every other nearby zone's (area_depth); the point is the candidate
	with the largest margin, at least `min_margin`, whose emulated TOUCH checks (TouchScene.touched) all miss; ties go to the lower one."""
	cx, cy, cz = target["center"]
	ex, ey, _ = target["extents"]
	r = target["area"]["r"]
	others = [z for z in nearby if z["zoneName"] != target["zoneName"]]
	n = int(math.ceil(r / step))
	counts = {"columns": 0, "ambiguous": 0, "noSurface": 0, "candidates": 0, "rejected": 0}
	candidates = []
	for i in range(-n, n + 1):
		for j in range(-n, n + 1):
			x, y = cx + i * step, cy + j * step
			outside_bound = max(abs(x - cx) - ex, abs(y - cy) - ey)
			if outside_bound < min_outside_bound or math.dist((x, y), (cx, cy)) >= r:
				continue
			counts["columns"] += 1
			try:
				surface = touch.surface_z(x, y, cz + r + GEO_Z_HALF_RANGE, cz - r - GEO_Z_HALF_RANGE)
			except Ambiguous:
				counts["ambiguous"] += 1
				continue
			if math.isnan(surface):
				counts["noSurface"] += 1
				continue
			heights = [surface] + [cz + k * step for k in range(1, n + 1) if surface < cz + k * step <= surface + max_height]
			for z in heights:
				margin = min([area_depth(target["area"], x, y, z)] + [-area_depth(o["area"], x, y, z) for o in others])
				if margin >= min_margin:
					candidates.append(((round(margin, 6), -round(z - surface, 6)), x, y, z, surface, outside_bound, margin))
	counts["candidates"] = len(candidates)
	candidates.sort(key=lambda c: c[0], reverse=True)
	for _, x, y, z, surface, outside_bound, margin in candidates:
		try:
			inside = [zone["zoneName"] for zone in nearby if inside_area(zone["area"], x, y, z)]
			touched = [name for name in inside if touch.touched(name, x, y, z, bound_upper)]
		except Ambiguous:
			counts["ambiguous"] += 1
			continue
		if inside != [target["zoneName"]] or touched:
			counts["rejected"] += 1  # a ray that hit a mesh (or, on a float edge, another zone's area)
			continue
		return {"x": x, "y": y, "z": z, "surfaceZ": surface, "height": z - surface, "margin": margin, "outsideBound": outside_bound,
		        "inside": inside, "touched": touched, "distanceToCenter": math.dist((x, y, z), target["center"]), "search": counts}
	return None


def material_report(data: StaticData, geo_dir: Path, world_maps_xml: Path, map_id: int, near: tuple[float, float, float] | None,
                    radius: float | None = None, limit: int | None = None, stand: bool = False, bound_upper: float = PLAYER_BOUND_UPPER) -> dict:
	materials = material_templates(data)
	placed: dict[int, list] = {}
	try:
		result = geo_loader.load(geo_dir, world_maps_xml, {map_id}, placed)
	except GeoOracleError as e:
		raise OracleError(str(e)) from e
	if map_id not in result.map_ids:
		raise OracleError(f"map {map_id} is not a world map of {world_maps_xml}")
	geometries = placed.get(map_id, [])
	zones: dict[str, dict] = {}
	zone_geometries: dict[str, object] = {}  # the geometry whose zone was created (the first one of a name)
	duplicates: list[str] = []
	without_template: dict[int, int] = {}
	for geometry in geometries:
		if not geometry.mesh.intentions & geo_loader.MATERIAL:
			continue
		center, extents = transform_box(*geo_loader.mesh_aabb(geometry.mesh), geometry.matrix)
		geometry_name = geometry.zone_geometry_name  # createZone's name, computed once by the loader (the names GeoRealDataTest checks)
		if geometry_name is None:
			raise OracleError(f"{geometry.name}: the loader named no material zone for a geometry with the MATERIAL intention")
		zone_name = f"{geometry_name}_{map_id}"
		material_id = geometry.mesh.material_id
		if material_id != SHIELD_MATERIAL and material_id not in materials:
			without_template[material_id] = without_template.get(material_id, 0) + 1
			continue
		if zone_name in zones:
			duplicates.append(zone_name)
			continue
		area = zone_area(geometry_name, center, extents)
		zone = {"zoneName": zone_name, "materialId": material_id, "mesh": geometry.name, "placementOrder": geometry.order,
		        "center": list(center), "extents": list(extents), "area": area,
		        "skills": materials.get(material_id, []), "shield": material_id == SHIELD_MATERIAL}
		if near is not None:
			zone["distance"] = math.dist(near, center)
			zone["distance2d"] = math.dist(near[:2], center[:2])
		zones[zone_name] = zone
		zone_geometries[zone_name] = geometry
	skill_zones = [z for z in zones.values() if z["skills"]]
	by_material: dict[int, int] = {}
	for zone in skill_zones:
		by_material[zone["materialId"]] = by_material.get(zone["materialId"], 0) + 1
	skill_ids = sorted({s["skillId"] for z in skill_zones for s in z["skills"]})
	listed = sorted(skill_zones, key=lambda z: (z.get("distance", 0.0), z["zoneName"]))
	if radius is not None:
		if near is None:
			raise OracleError("--radius needs --near")
		listed = [z for z in listed if z["distance"] <= radius]
	if limit is not None:
		listed = listed[:limit]
	unconditional = [z for z in skill_zones if any(s["conditions"] is None for s in z["skills"])]
	nearest_unconditional = min(unconditional, key=lambda z: z["distance"]) if near is not None and unconditional else None
	stand_section = None
	if stand:
		if nearest_unconditional is None:
			raise OracleError("--stand needs --near and an unconditional skill zone on the map")
		touch = TouchScene(map_id, geometries, geo_loader.read_heightmap(result.terrains.get(map_id)),
		                   {z["zoneName"]: zone_geometries[z["zoneName"]] for z in skill_zones})
		stand_section = stand_report(touch, skill_zones, nearest_unconditional, bound_upper)
	return {
		"format": "aion-m5b3-material",
		"version": 1,
		"map": map_id,
		"near": list(near) if near is not None else None,
		"meshPlacements": sum(1 for _ in {g.order for g in geometries}),
		"materialGeometries": sum(1 for g in geometries if g.mesh.intentions & geo_loader.MATERIAL),
		"geometriesWithoutMaterialTemplate": {str(k): v for k, v in sorted(without_template.items())},
		"duplicateZoneNames": duplicates,
		"skillZones": len(skill_zones),
		"skillPlacements": len({z["placementOrder"] for z in skill_zones}),
		"skillZonesByMaterial": {str(k): v for k, v in sorted(by_material.items())},
		"skillIds": skill_ids,
		"nearestUnconditional": nearest_unconditional,
		"zones": listed,
		"terrain": terrain_materials(geo_dir, world_maps_xml, map_id, materials),
		"stand": stand_section,
		"notModelled": ["without --stand: which positions pass the TOUCH check of ZoneCollisionMaterialActor on a zone's mesh (m5b3-plan.md "
		                "risk 9, G-04); --stand emulates it in double precision and keeps a clearance from every grid point it rejects",
		                "the conditions of a material skill (SUNNY, NIGHT: the weather and the game time decide them)"],
	}
