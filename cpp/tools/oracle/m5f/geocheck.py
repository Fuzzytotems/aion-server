"""m5f-travel --geo-check (m5f-plan.md §10.5 G3): the data z of every M5f destination against the geo data's surface there.

GeoService.getZ(worldId, x, y, z, instanceId) = GeoMap.getZ(x, y, z + 2, z - 2) (world/geo/GeoService.java:57-59), evaluated with the geo oracle's
float32 emulation (tools/oracle/geo/probes.py MapScene.get_z). When that window finds no surface, a wide probe (z + 50 .. z - 50) says where the
nearest surface below z + 50 is. A point the emulation calls ambiguous (on a terrain cell border, as every hotspot at whole coordinates is) is
probed 1 cm away diagonally instead and marked `nudged`. A destination is listed in `outside` when |data z - geo z| > 1.0 or no surface is found at all.

Destinations (§2.9): the REGULAR locations of the teleporters 203194, 203679, 203091, 203581, 203726, 204191; the FLIGHT locations of 203070 and
203083 (no position in teleport_location.xml: the client flies the path and the landing point is client-given - listed under notModelled); every
hotspot of the four start maps; Haramel's portal locations (730318, 730319) and its instance exits. This proves nothing about the port; it is a
data check for the real client (risk 9).
"""

from __future__ import annotations

import math
from pathlib import Path

from staticdata_oracle import OracleError

TELEPORTERS = (203194, 203679, 203091, 203581, 203726, 204191)
FLIGHT_MASTERS = (203070, 203083)
START_MAPS = (210010000, 220010000, 210030000, 220030000)
HARAMEL_PORTALS = (730318, 730319)
HARAMEL = 300200000
THRESHOLD = 1.0
WIDE = 50.0


def destinations(td) -> tuple[list[dict], list[dict]]:
	rows, not_modelled = [], []
	telelocs = td.telelocs()
	for npc in TELEPORTERS + FLIGHT_MASTERS:
		found = td.teleporter_of(npc)
		if found is None:
			not_modelled.append({"what": f"npc {npc}", "reason": "no teleporter template"})
			continue
		for loc in found[1]:
			tpl = telelocs.get(loc.loc_id)
			what = f"npc {npc} loc {loc.loc_id}"
			if tpl is None:
				not_modelled.append({"what": what, "reason": "no teleloc_template"})
			elif loc.loc_type != "REGULAR" or not tpl["hasPosition"]:
				not_modelled.append({"what": what, "reason": f"{loc.loc_type} location without a position: the landing is client-given"})
			else:
				rows.append({"what": what, "name": tpl["name"], "map": tpl["map"], "x": tpl["x"], "y": tpl["y"], "z": tpl["z"]})
	for hid, h in sorted(td.hotspots().items()):
		if h["map"] in START_MAPS:
			rows.append({"what": f"hotspot {hid}", "name": None, "map": h["map"], "x": h["x"], "y": h["y"], "z": h["z"]})
	uses, _ = td.portal_templates()
	seen_locs = set()
	for npc in HARAMEL_PORTALS:
		for path in uses.get(npc, []):
			loc_id = int(path.attrs.get("loc_id", "0"))
			loc = td.portal_locs().get(loc_id)
			if loc is None or loc_id in seen_locs:
				continue
			seen_locs.add(loc_id)
			rows.append({"what": f"portal {npc} loc {loc_id}", "name": None, "map": loc["map"], "x": loc["x"], "y": loc["y"], "z": loc["z"]})
	for e in td.instance_exits().get(HARAMEL, []):
		rows.append({"what": f"instance exit {HARAMEL} {e['race']}", "name": None, "map": e["map"], "x": e["x"], "y": e["y"], "z": e["z"]})
	return rows, not_modelled


# the probe point itself first, then 1 cm away diagonally (see geo_check)
NUDGES = ((0.0, 0.0), (0.01, 0.01), (-0.01, -0.01), (0.01, -0.01), (-0.01, 0.01))


def _probe(scene, x: float, y: float, z: float, add, sub) -> tuple[float, str, str]:
	"""GeoService.getZ(worldId, x, y, z) = getZ(x, y, z + 2, z - 2); without a surface there, the wide window"""
	found, source = scene.get_z(x, y, add(z, 2.0), sub(z, 2.0))
	if not math.isnan(found):
		return found, source, "z+2..z-2"
	found, source = scene.get_z(x, y, add(z, WIDE), sub(z, WIDE))
	return found, source, f"z+{WIDE:g}..z-{WIDE:g}"


def geo_check(td, geo_dir: Path, world_maps_xml: Path, only_maps: list[int] | None = None) -> dict:
	from geo.javamath import add, sub
	from geo.probes import Ambiguous
	from m5e.stumble import load_scene

	rows, not_modelled = destinations(td)
	if only_maps:
		skipped = [r for r in rows if r["map"] not in only_maps]
		rows = [r for r in rows if r["map"] in only_maps]
		for r in skipped:
			not_modelled.append({"what": r["what"], "reason": f"map {r['map']} not in --geo-map"})
	if not Path(geo_dir).is_dir():
		raise OracleError(f"{geo_dir}: no geo data directory (pass --geo-dir)")
	scenes = {}
	checked = []
	for map_id in sorted({r["map"] for r in rows}):
		if not (Path(geo_dir) / f"{map_id}.geo").is_file():
			scenes[map_id] = None
			continue
		scenes[map_id] = load_scene(Path(geo_dir), world_maps_xml, map_id)
	for r in rows:
		scene = scenes.get(r["map"])
		out = dict(r)
		if scene is None:
			out.update({"geoZ": None, "dz": None, "source": None, "note": "no geo file for the map"})
			not_modelled.append({"what": r["what"], "reason": f"no {r['map']}.geo"})
			checked.append(out)
			continue
		probe = None
		reasons = []
		for nudge in NUDGES:
			try:
				probe = _probe(scene, add(r["x"], nudge[0]), add(r["y"], nudge[1]), r["z"], add, sub) + (nudge,)
				break
			except Ambiguous as e:
				reasons.append(str(e))
		if probe is None:
			out.update({"geoZ": None, "dz": None, "source": None, "note": f"ambiguous: {'; '.join(sorted(set(reasons)))}"})
			checked.append(out)
			continue
		z, source, window, nudge = probe
		out.update({"geoZ": None if math.isnan(z) else z, "dz": None if math.isnan(z) else r["z"] - z, "source": source, "window": window})
		if nudge != (0.0, 0.0):
			# the exact point is on a terrain cell border or a triangle edge, where the float result depends on rounding the emulation cannot
			# settle; for a 1-m data check the surface 1 cm away is the answer
			out["nudged"] = list(nudge)
		checked.append(out)
	outside = [c for c in checked if c.get("note") is None and (c["dz"] is None or abs(c["dz"]) > THRESHOLD)]
	return {"threshold": THRESHOLD, "checked": checked, "outside": outside, "notModelled": not_modelled,
	        "maps": sorted(m for m, s in scenes.items() if s is not None)}
