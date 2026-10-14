"""m5i-spots: the gate's positions and the npc sets they see (m5i-plan.md 10.2, G-01), derived from the static data and the Java it names.

- Visibility: VisibleObject.getVisibleDistance() is 95 m (VisibleObject.java:247-249), checked in 3D (PositionUtil.isInRange); a FLAG npc is
  seen on the whole map (FlagKnownList.java:25).
- Aggro: an npc template's `arange` (aggro range); a spot is safe from a set when every npc of it is farther than its arange + 3 m.
- Fortress 1131: its FORT zone polygon AB1_1131_CASTLE_AREA_400010000 (zones_400010000.xml) and its shield sphere (siege_shields.xml);
  its siege spawns per (race, mod) (spawns/Sieges/400010000_Reshanta.xml). A FortressAssault's teleport wave picks siege npcs that are not
  LEGENDARY and not ARTIFACT (FortressAssault.java:67-73).
- Base 2120: the base spawns per occupier and handler (spawns/Bases/210020000_Eltnen.xml); base_locations.xml has no default_occupier for
  it, so it starts as BALAUR (BaseTemplate's default) and Base.handleStart spawns its FLAG, MERCHANT and SENTINEL (Base.java:64-70).
- World raid location 31 (Reshanta, world_raids.xml): WorldRaid spawns its flag 832819 at the location's own coordinates (WorldRaid.java:155).
- Rift 2120 (ELTNEN_AM): RiftEnum's master/slave anchors, entries and levels (RiftEnum.java:11), the master npc 700137 at the anchor spot
  (spawns/Rifts/210020000_Eltnen.xml).

The positions are chosen here by a grid search under those rules (S1, S2, S3 with M1/M2) or next to the target spawn (S4, S6, S7); each comes
with the checks that justify it, so a data change that breaks a rule fails the oracle instead of the gate.
"""
import math
import xml.etree.ElementTree as ET
from pathlib import Path

VISIBLE = 95.0
AGGRO_MARGIN = 3.0
RESHANTA, ELTNEN = 400010000, 210020000


def _npc_templates(data_dir: Path) -> dict:
	npcs = {}
	for path in sorted((data_dir / "npcs").glob("*.xml")):
		for event, element in ET.iterparse(path, events=("end",)):
			if element.tag == "npc_template":
				npcs[int(element.get("npc_id"))] = {"arange": int(element.get("arange", "0")), "rating": element.get("rating", "NORMAL"),
				                                    "abyssType": element.get("abyss_type", "NONE"), "type": element.get("type", "NONE"),
				                                    "tribe": element.get("tribe", "")}
				element.clear()
	return npcs


def _dist(a, b) -> float:
	return math.sqrt((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2 + (a[2] - b[2]) ** 2)


def _in_polygon(x: float, y: float, points) -> bool:
	inside = False
	j = len(points) - 1
	for i in range(len(points)):
		xi, yi = points[i]
		xj, yj = points[j]
		if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / (yj - yi) + xi:
			inside = not inside
		j = i
	return inside


def _zone(data_dir: Path, name: str):
	for path in (data_dir / "zones").glob("*.xml"):
		for zone in ET.parse(path).getroot().iter("zone"):
			if zone.get("name") == name:
				pts = zone.find("points")
				return {"points": [(float(p.get("x")), float(p.get("y"))) for p in pts.findall("point")],
				        "bottom": float(pts.get("bottom")), "top": float(pts.get("top"))}
	raise ValueError(f"zone {name} not found")


def _in_zone(zone, p) -> bool:
	return _in_polygon(p[0], p[1], zone["points"]) and zone["bottom"] <= p[2] <= zone["top"]


def _shield(data_dir: Path, shield_id: int):
	for shield in ET.parse(data_dir / "siege" / "siege_shields.xml").getroot().iter("shield"):
		if int(shield.get("id")) == shield_id:
			c = shield.find("center")
			return {"center": (float(c.get("x")), float(c.get("y")), float(c.get("z"))), "radius": float(shield.get("radius"))}
	raise ValueError(f"shield {shield_id} not found")


def _siege_sets(data_dir: Path, siege_id: int) -> dict:
	sets = {}
	root = ET.parse(data_dir / "spawns" / "Sieges" / "400010000_Reshanta.xml").getroot()
	for siege_spawn in root.iter("siege_spawn"):
		if int(siege_spawn.get("siege_id")) != siege_id:
			continue
		for race in siege_spawn.findall("siege_race"):
			for mod in race.findall("siege_mod"):
				key = f"{race.get('race')}/{mod.get('mod')}"
				for spawn in mod.findall("spawn"):
					for spot in spawn.findall("spot"):
						sets.setdefault(key, []).append({"npcId": int(spawn.get("npc_id")),
						                                 "pos": (float(spot.get("x")), float(spot.get("y")), float(spot.get("z")))})
	return sets


def _base_sets(data_dir: Path, base_id: int) -> dict:
	sets = {}
	root = ET.parse(data_dir / "spawns" / "Bases" / "210020000_Eltnen.xml").getroot()
	for base_spawn in root.iter("base_spawn"):
		if int(base_spawn.get("id")) != base_id:
			continue
		for occ in base_spawn.findall("occupier_template"):
			for spawn in occ.findall("spawn"):
				for spot in spawn.findall("spot"):
					sets.setdefault(f"{occ.get('occupier')}/{spawn.get('handler')}", []).append(
						{"npcId": int(spawn.get("npc_id")), "pos": (float(spot.get("x")), float(spot.get("y")), float(spot.get("z")))})
	return sets


def _aggro_margin(pos, npcs, templates) -> float:
	"""the smallest distance - (arange + margin) over the npcs (positive: outside every aggro range)"""
	worst = math.inf
	for npc in npcs:
		arange = templates.get(npc["npcId"], {}).get("arange", 0)
		if arange <= 0:
			continue
		worst = min(worst, _dist(pos, npc["pos"]) - arange - AGGRO_MARGIN)
	return worst


def _seen(pos, npcs):
	return [n for n in npcs if _dist(pos, n["pos"]) <= VISIBLE]


def _grid(points, step):
	xs = [p[0] for p in points]
	ys = [p[1] for p in points]
	x = math.floor(min(xs))
	while x <= max(xs):
		y = math.floor(min(ys))
		while y <= max(ys):
			yield (float(x), float(y))
			y += step
		x += step


def _z_near(x, y, npcs):
	nearest = min(npcs, key=lambda n: (n["pos"][0] - x) ** 2 + (n["pos"][1] - y) ** 2)
	return nearest["pos"][2]


def _round(p):
	return [round(p[0], 3), round(p[1], 3), round(p[2], 3)]


def spots_report(data_dir: Path) -> dict:
	data_dir = Path(data_dir)
	templates = _npc_templates(data_dir)
	zone = _zone(data_dir, "AB1_1131_CASTLE_AREA_400010000")
	shield = _shield(data_dir, 1131)
	sets = _siege_sets(data_dir, 1131)
	all_1131 = [n for npcs in sets.values() for n in npcs]

	def eligible(npcs):
		return [n for n in npcs if templates.get(n["npcId"], {}).get("rating") != "LEGENDARY" and
		        templates.get(n["npcId"], {}).get("abyssType") != "ARTIFACT"]

	# S1: inside the zone, outside the sphere, safe from every set of every race, in view of the most assault-eligible ELYOS SIEGE npcs
	best = None
	for x, y in _grid(zone["points"], 4):
		if not _in_polygon(x, y, zone["points"]):
			continue
		z = _z_near(x, y, all_1131)
		p = (x, y, z)
		if not _in_zone(zone, p) or _dist(p, shield["center"]) <= shield["radius"] + 5:
			continue
		margin = _aggro_margin(p, all_1131, templates)
		if margin <= 0:
			continue
		seen = len(eligible(_seen(p, sets.get("ELYOS/SIEGE", []))))
		key = (seen, margin)
		if best is None or key > best[0]:
			best = (key, p)
	if best is None:
		raise ValueError("no S1 spot satisfies the rules")
	s1 = best[1]

	# S3 with M1 and M2: S3 outside the zone, M1 inside the zone outside the sphere, M2 inside the sphere; the moves from the sphere centre outwards
	cx, cy, cz = shield["center"]
	s3 = m1 = m2 = None
	for angle in range(0, 360, 5):
		rad = math.radians(angle)
		cand = [(cx + math.cos(rad) * r, cy + math.sin(rad) * r, cz) for r in range(10, 400, 2)]
		outside = [c for c in cand if not _in_zone(zone, c)]
		if not outside:
			continue
		first_out = outside[0]
		r_out = math.hypot(first_out[0] - cx, first_out[1] - cy)
		m1c = (cx + math.cos(rad) * (shield["radius"] + 8), cy + math.sin(rad) * (shield["radius"] + 8), cz)
		m2c = (cx + math.cos(rad) * (shield["radius"] - 8), cy + math.sin(rad) * (shield["radius"] - 8), cz)
		s3c = (cx + math.cos(rad) * (r_out + 8), cy + math.sin(rad) * (r_out + 8), cz)
		if _in_zone(zone, s3c) or not _in_zone(zone, m1c) or _dist(m1c, shield["center"]) <= shield["radius"]:
			continue
		if all(_aggro_margin(c, all_1131, templates) > 0 for c in (s3c, m1c, m2c)) and r_out + 8 - (shield["radius"] + 8) <= 120:
			s3, m1, m2 = s3c, m1c, m2c
			break

	# S4: next to world raid location 31's flag spot, 10 m off
	wr = None
	for loc in ET.parse(data_dir / "world_raid" / "world_raids.xml").getroot().iter("world_raid_location"):
		if int(loc.get("location_id")) == 31:
			wr = (float(loc.get("x")), float(loc.get("y")), float(loc.get("z")))
	s4 = (wr[0] + 10, wr[1], wr[2])

	# S6: next to rift 2120's master anchor (ELTNEN_AM)
	rift = None
	for spawn_map in ET.parse(data_dir / "spawns" / "Rifts" / "210020000_Eltnen.xml").getroot().iter("spawn"):
		for spot in spawn_map.findall("spot"):
			if spot.get("anchor") == "ELTNEN_AM":
				rift = {"npcId": int(spawn_map.get("npc_id")), "pos": (float(spot.get("x")), float(spot.get("y")), float(spot.get("z")))}
	s6 = (rift["pos"][0] + 10, rift["pos"][1], rift["pos"][2])

	# S7: in view of base 2120's spawns, outside the aggro of every occupier's set
	base = _base_sets(data_dir, 2120)
	flag = base["BALAUR/FLAG"][0]["pos"]
	all_base = [n for npcs in base.values() for n in npcs]
	s7 = None
	for r in range(30, 90, 2):
		for angle in range(0, 360, 10):
			rad = math.radians(angle)
			c = (flag[0] + math.cos(rad) * r, flag[1] + math.sin(rad) * r, flag[2])
			if _aggro_margin(c, all_base, templates) > 0 and all(_dist(c, n["pos"]) <= VISIBLE for n in all_base):
				s7 = c
				break
		if s7:
			break
	if s7 is None:
		raise ValueError("no S7 spot satisfies the rules")

	def npc_list(npcs):
		return [{"npcId": n["npcId"], "pos": _round(n["pos"])} for n in npcs]

	teleport = []
	for loc in ET.parse(data_dir / "siege" / "siege_locations.xml").getroot().iter("siege_location"):
		if loc.get("id") == "1131":
			ad = loc.find("assault_data")
			for a in ad.findall("assaulter"):
				if a.get("type") == "TELEPORT":
					teleport = [int(i) for i in a.get("npc_ids").split()]
			base_delay = int(ad.get("base_delay"))
	return {
		"visibleDistance": VISIBLE,
		"spots": {"S1": _round(s1), "S3": _round(s3) if s3 else None, "M1": _round(m1) if m1 else None, "M2": _round(m2) if m2 else None,
		          "S4": _round(s4), "S6": _round(s6), "S7": _round(s7)},
		"checks": {
			"S1": {"inZone": _in_zone(zone, s1), "sphereDistance": round(_dist(s1, shield["center"]), 2),
			       "aggroMargin": round(_aggro_margin(s1, all_1131, templates), 2),
			       "eligibleElyosSiegeInView": len(eligible(_seen(s1, sets.get("ELYOS/SIEGE", []))))},
			"S7": {"aggroMargin": round(_aggro_margin(s7, all_base, templates), 2)},
		},
		"shield": {"center": _round(shield["center"]), "radius": shield["radius"]},
		"assault1131": {"teleport": teleport, "baseDelay": base_delay},
		"set1131": {key: npc_list(_seen(s1, npcs)) for key, npcs in sets.items()},
		"worldRaid31": {"pos": _round(wr), "flagNpcId": 832819},
		"rift2120": {"masterNpcId": rift["npcId"], "masterPos": _round(rift["pos"]), "entries": 12, "minLevel": 20, "maxLevel": 65},
		"base2120": {key: npc_list(npcs) for key, npcs in base.items()},
	}
