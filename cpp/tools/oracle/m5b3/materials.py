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
from geo.jgeo import transform_box
from geo.pngread import read_png
from m5a.data import StaticData, java_int

SHIELD_MATERIAL = 11  # ZoneService.createMaterialZoneTemplate: `if (geometry.getMaterialId() == 11)` -> ShieldService


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


def material_report(data: StaticData, geo_dir: Path, world_maps_xml: Path, map_id: int, near: tuple[float, float, float] | None,
                    radius: float | None = None, limit: int | None = None) -> dict:
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
		"notModelled": ["which positions pass the TOUCH check of ZoneCollisionMaterialActor on a zone's mesh (m5b3-plan.md risk 9, G-04)",
		                "the conditions of a material skill (SUNNY, NIGHT: the weather and the game time decide them)"],
	}
