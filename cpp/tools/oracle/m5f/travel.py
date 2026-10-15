"""m5f-travel (m5f-plan.md G-01, §2.1-§2.5, §2.9, §10): the travel data the M5f gate asserts, re-derived from the static data with Java's
arithmetic. Every rule is cited at its use; the Java paths are below game-server/src/com/aionemu/gameserver (handlers: game-server/data/handlers).

- teleporter npcs (TeleporterData.getTeleporterTemplateByNpcId, TeleportLocation, TelelocationTemplate, TeleportService.teleport and
  checkKinahForTransportation, PricesService.getPriceForService through the m5c helpers) and DialogService's Daeva gate of AIRLINE_SERVICE;
- hotspots (BindPointTeleportService.calculateTeleportationPrice with PositionUtil.getDistance's float differences and float sum);
- obelisks (ResurrectAI: BindPointData's raw price);
- portals (PortalAI -> ActionItemNpcAI's use bar, Portal2Data.getPortalUsePath, PortalLocData, PortalService.port's maxPlayers and
  checkEnterLevel, InstanceCooltimeData.calculateInstanceEntranceCooltime, InstanceExitData.getInstanceExit);
- SpawnEngine.spawnInstance's spots (the m5a-spawns rules with the instance's difficulty);
- PlayerExperienceTable.getStartExpForLevel.

Not modelled (reported where it matters): HiPass (price 1), siege influence (prices are computed with sieges off, influence 0), the membership
instance cooldown rate (InstanceService.getInstanceRate is taken as 1: no INSTANCES_COOLDOWN permission), walker formations' positions,
static doors and houses of spawnInstance, a DST gap or overlap exactly at an instance reset time.
"""

from __future__ import annotations

import datetime as dt
import math
import re
import time
from dataclasses import dataclass, replace
from pathlib import Path

from staticdata_oracle import OracleError

from m5a.data import StaticData, java_boolean, java_int
from m5a.javafloat import distance as report_distance
from m5a.javafloat import f32, in_range, parse_float, to_long
from m5a.spawns import GameClock, Group, Spot, TemporarySpawn, evaluate

RACES = ("ELYOS", "ASMODIANS")


def _int32(value: int) -> int:
	return (value + 2**31) % 2**32 - 2**31


def java_byte(value: int) -> int:
	"""Java `(byte) i`"""
	return (value + 128) % 256 - 128


def _byte_attr(text: str | None, what: str) -> int:
	"""a JAXB byte attribute (default 0); out of range is refused (JAXB would fail the unmarshal)"""
	value = java_int(text, what, 0)
	if not -128 <= value <= 127:
		raise OracleError(f"{what}={text!r} is out of the byte range")
	return value


def _float_attr(text: str | None, what: str, default: float = 0.0) -> float:
	if text is None:
		return default
	try:
		return parse_float(text)
	except ValueError as e:
		raise OracleError(f"{what}={text!r} is not a float") from e


def _race(text: str | None, default: str) -> str:
	return default if text is None else text.strip()


# ---- static data -------------------------------------------------------------------------------------------------------------------------

@dataclass(frozen=True)
class NpcRow:
	npc_id: int
	name: str | None
	ai: str | None
	race: str           # NpcTemplate.race, default Race.NONE (NpcTemplate.java:98-99)
	tribe: str | None
	npc_type: str
	level: int
	talk_distance: int  # NpcTemplate.getTalkDistance: talk_info's distance (TalkInfo default 2), 2 without talk_info (NpcTemplate.java:267-269)
	talk_delay: int     # NpcTemplate.getTalkDelay: talk_info's delay in seconds, 0 without (NpcTemplate.java:271-273)


@dataclass
class TeleLoc:
	loc_id: int
	teleportid: int
	price: int
	price_pvp: int
	required_quest: int
	loc_type: str


@dataclass
class PortalPathRow:
	attrs: dict
	quest_req: list
	item_req: list


class TravelData:
	"""The holders m5f-travel reads, parsed once and on demand, with the Java holders' key rules (a later entry of a map key replaces the
	earlier one unless noted)."""

	def __init__(self, data: StaticData, java_src: Path | None = None):
		self.data = data
		self.java_src = java_src
		self._cache: dict[str, object] = {}

	def _once(self, key, fn):
		if key not in self._cache:
			self._cache[key] = fn()
		return self._cache[key]

	# npc_templates (NpcData: the last template of an id is the one kept, as m5a's loader)
	def npcs(self) -> dict[int, NpcRow]:
		def load():
			result = {}
			for e in self.data.stream("npc_templates", "npc_template"):
				npc_id = java_int(e.get("npc_id"), "npc_template npc_id")
				talk = e.find("talk_info")
				distance = 2 if talk is None else java_int(talk.get("distance"), f"npc {npc_id} talk_info distance", 2)
				delay = 0 if talk is None else java_int(talk.get("delay"), f"npc {npc_id} talk_info delay", 0)
				result[npc_id] = NpcRow(npc_id, e.get("name"), e.get("ai"), _race(e.get("race"), "NONE"), e.get("tribe"), e.get("type", "NONE"),
				                        java_int(e.get("level"), f"npc {npc_id} level", 0), distance, delay)
			return result
		return self._once("npcs", load)

	def npc(self, npc_id: int) -> NpcRow:
		row = self.npcs().get(npc_id)
		if row is None:
			raise OracleError(f"npc {npc_id} has no npc_template")
		return row

	# npc_teleporter.xml: TeleporterData.afterUnmarshal keys the templates by teleportId (a later one replaces the earlier), and
	# getTeleporterTemplateByNpcId walks the HashMap's values (TeleporterData.java)
	def teleporters(self) -> dict[int, tuple[list[int], list[TeleLoc]]]:
		def load():
			result = {}
			for e in self.data.children("npc_teleporter", "teleporter_template"):
				tid = java_int(e.get("teleportId"), "teleporter_template teleportId")
				npc_ids = [java_int(v, "teleporter_template npc_ids") for v in (e.get("npc_ids") or "").split()]
				locs = []
				for block in e.findall("locations"):  # @XmlElement(name = "locations") on a single field: JAXB keeps the last block
					locs = []
					for t in block.findall("telelocation"):
						what = f"teleporter {tid} telelocation"
						locs.append(TeleLoc(java_int(t.get("loc_id"), f"{what} loc_id"), java_int(t.get("teleportid"), f"{what} teleportid", 0),
						                    java_int(t.get("price"), f"{what} price", 0), java_int(t.get("pricePvp"), f"{what} pricePvp", 0),
						                    java_int(t.get("required_quest"), f"{what} required_quest", 0), t.get("type")))
				result[tid] = (npc_ids, locs)
			return result
		return self._once("teleporters", load)

	def teleporter_of(self, npc_id: int) -> tuple[int, list[TeleLoc]] | None:
		found = [(tid, locs) for tid, (ids, locs) in self.teleporters().items() if npc_id in ids]
		if len(found) > 1:
			raise OracleError(f"npc {npc_id} is in teleporter templates {[f[0] for f in found]}: getTeleporterTemplateByNpcId returns the first "
			                  "of a HashMap's iteration, which this oracle does not model")
		return found[0] if found else None

	# teleport_location.xml: TeleLocationData keyed by loc_id
	def telelocs(self) -> dict[int, dict]:
		def load():
			result = {}
			for e in self.data.children("teleport_location", "teleloc_template"):
				loc_id = java_int(e.get("loc_id"), "teleloc_template loc_id")
				what = f"teleloc {loc_id}"
				result[loc_id] = {"map": java_int(e.get("mapid"), f"{what} mapid"), "name": e.get("name"),
				                  "x": _float_attr(e.get("posX"), f"{what} posX"), "y": _float_attr(e.get("posY"), f"{what} posY"),
				                  "z": _float_attr(e.get("posZ"), f"{what} posZ"), "heading": java_int(e.get("heading"), f"{what} heading", 0),
				                  "hasPosition": e.get("posX") is not None}
			return result
		return self._once("telelocs", load)

	# hotspot_template.xml: HotspotData.getHotspotTemplateById returns the FIRST template of the id (a list walk)
	def hotspots(self) -> dict[int, dict]:
		def load():
			result = {}
			for e in self.data.children("hotspot_template", "hotspot_location"):
				hid = java_int(e.get("id"), "hotspot_location id")
				if hid in result:
					continue
				what = f"hotspot {hid}"
				price = e.get("price")
				if price is None or not re.fullmatch(r"-?\d+", price.strip()):
					raise OracleError(f"{what} price={price!r} is not a long")
				result[hid] = {"map": java_int(e.get("worldId"), f"{what} worldId"), "race": _race(e.get("race"), "PC_ALL"),
				               "x": _float_attr(e.get("x"), f"{what} x"), "y": _float_attr(e.get("y"), f"{what} y"),
				               "z": _float_attr(e.get("z"), f"{what} z"), "basePrice": int(price.strip())}
			return result
		return self._once("hotspots", load)

	# bind_points/bind_points.xml: BindPointData keyed by npcid
	def bind_points(self) -> dict[int, dict]:
		def load():
			result = {}
			for e in self.data.children("bind_points", "bind_point"):
				npc_id = java_int(e.get("npcid"), "bind_point npcid", 0)
				result[npc_id] = {"npcId": npc_id, "name": e.get("name"), "price": java_int(e.get("price"), f"bind_point {npc_id} price", 0)}
			return result
		return self._once("bind_points", load)

	# portals/portal_template2.xml: Portal2Data keyed by npc_id, portal_use and portal_dialog separately
	def portal_templates(self) -> tuple[dict[int, list[PortalPathRow]], dict[int, tuple[int, list[PortalPathRow]]]]:
		def paths(e):
			rows = []
			for p in e.findall("portal_path"):
				rows.append(PortalPathRow(dict(p.attrib), [dict(q.attrib) for q in p.findall("quest_req")], [dict(i.attrib) for i in p.findall("item_req")]))
			return rows

		def load():
			uses, dialogs = {}, {}
			for e in self.data.children("portal_templates2"):
				if e.tag == "portal_use":
					uses[java_int(e.get("npc_id"), "portal_use npc_id", 0)] = paths(e)
				elif e.tag == "portal_dialog":
					dialogs[java_int(e.get("npc_id"), "portal_dialog npc_id", 0)] = (java_int(e.get("teleport_dialog_id"), "teleport_dialog_id", 1011), paths(e))
			return uses, dialogs
		return self._once("portals", load)

	# portals/portal_loc.xml: PortalLocData keyed by loc_id
	def portal_locs(self) -> dict[int, dict]:
		def load():
			result = {}
			for e in self.data.children("portal_locs", "portal_loc"):
				loc_id = java_int(e.get("loc_id"), "portal_loc loc_id", 0)
				what = f"portal_loc {loc_id}"
				result[loc_id] = {"locId": loc_id, "map": java_int(e.get("world_id"), f"{what} world_id", 0), "x": _float_attr(e.get("x"), f"{what} x"),
				                  "y": _float_attr(e.get("y"), f"{what} y"), "z": _float_attr(e.get("z"), f"{what} z"),
				                  "heading": _byte_attr(e.get("h"), f"{what} h")}
			return result
		return self._once("portal_locs", load)

	# instance_cooltimes: InstanceCooltimeData keyed by worldId (InstanceCooltimeData.java:41-47)
	def cooltimes(self) -> dict[int, dict]:
		def load():
			result = {}
			for e in self.data.children("instance_cooltimes", "instance_cooltime"):
				world = java_int(e.get("worldId"), "instance_cooltime worldId")
				what = f"instance_cooltime of {world}"

				def text(tag):
					child = e.find(tag)
					return None if child is None or child.text is None else child.text.strip()
				result[world] = {
					"id": java_int(e.get("id"), f"{what} id"), "worldId": world, "race": e.get("race"),
					"syncId": java_int(e.get("sync_id"), f"{what} sync_id", 0),
					"type": text("type"), "typeValue": text("typevalue"),
					"entCoolTime": java_int(text("ent_cool_time"), f"{what} ent_cool_time", 0),
					"maxCount": java_int(text("maxcount"), f"{what} maxcount", 0),
					"maxMemberLight": java_int(text("max_member_light"), f"{what} max_member_light", 0),
					"maxMemberDark": java_int(text("max_member_dark"), f"{what} max_member_dark", 0),
					"enterMinLevelLight": java_int(text("enter_min_level_light"), f"{what} enter_min_level_light", 0),
					"enterMaxLevelLight": java_int(text("enter_max_level_light"), f"{what} enter_max_level_light", 0),
					"enterMinLevelDark": java_int(text("enter_min_level_dark"), f"{what} enter_min_level_dark", 0),
					"enterMaxLevelDark": java_int(text("enter_max_level_dark"), f"{what} enter_max_level_dark", 0),
					"canEnterMentor": java_boolean(text("can_enter_mentor")),
				}
			return result
		return self._once("cooltimes", load)

	# instance_exit: InstanceExitData, a list per instance world in document order (InstanceExitData.java)
	def instance_exits(self) -> dict[int, list[dict]]:
		def load():
			result = {}
			for e in self.data.children("instance_exits", "instance_exit"):
				world = java_int(e.get("instance_id"), "instance_exit instance_id", 0)
				what = f"instance_exit of {world}"
				result.setdefault(world, []).append({
					"instance": world, "map": java_int(e.get("exit_world"), f"{what} exit_world", 0), "race": _race(e.get("race"), "PC_ALL"),
					"x": _float_attr(e.get("x"), f"{what} x"), "y": _float_attr(e.get("y"), f"{what} y"), "z": _float_attr(e.get("z"), f"{what} z"),
					"heading": _byte_attr(e.get("h"), f"{what} h")})
			return result
		return self._once("instance_exits", load)

	def world_maps(self) -> dict[int, dict]:
		def load():
			result = {}
			for e in self.data.children("world_maps", "map"):
				map_id = java_int(e.get("id"), "map id")
				result[map_id] = {"name": e.get("name"), "instance": java_boolean(e.get("instance"))}  # WorldMapTemplate.instance, default false
			return result
		return self._once("world_maps", load)

	def experience(self) -> list[int]:
		def load():
			values = []
			for e in self.data.children("player_experience_table", "exp"):
				text = (e.text or "").strip()
				if not re.fullmatch(r"-?\d+", text):
					raise OracleError(f"player_experience_table <exp>{text}</exp> is not a long")
				values.append(int(text))
			if not values:
				raise OracleError("player_experience_table.xml has no <exp>")
			return values
		return self._once("experience", load)

	def spawn_groups(self) -> dict[int, list[Group]]:
		"""SpawnsData.getSpawnsByWorldId for every map: m5a.spawns.load_groups' rules (custom spawns, groups per npc id), in one pass."""
		def load():
			by_map: dict[int, dict[int, list[Group]]] = {}
			for spawn_map in self.data.children("spawns", "spawn_map"):
				map_id = java_int(spawn_map.get("map_id"), "spawn_map map_id")
				by_npc = by_map.setdefault(map_id, {})
				customs: list[int] = []
				for spawn in spawn_map.findall("spawn"):
					npc_id = java_int(spawn.get("npc_id"), "spawn npc_id")
					if npc_id in customs:
						continue
					if java_boolean(spawn.get("custom")):
						by_npc.pop(npc_id, None)
						customs.append(npc_id)
					temporary = spawn.find("temporary_spawn")
					group = Group(npc_id, java_int(spawn.get("pool"), f"spawn {npc_id} pool", 0), java_int(spawn.get("difficult_id"), f"spawn {npc_id}", 0),
					              spawn.get("handler"), TemporarySpawn.parse(temporary) if temporary is not None else None,
					              respawn_time=java_int(spawn.get("respawn_time"), f"spawn {npc_id} respawn_time", 0))
					for spot in spawn.findall("spot"):
						spot_temporary = spot.find("temporary_spawn")
						group.spots.append(Spot(npc_id, parse_float(spot.get("x")), parse_float(spot.get("y")), parse_float(spot.get("z")),
						                        _byte_attr(spot.get("h"), f"spot h of npc {npc_id}"), spot.get("walker_id"),
						                        java_int(spot.get("random_walk"), "random_walk", 0),
						                        TemporarySpawn.parse(spot_temporary) if spot_temporary is not None else None,
						                        java_int(spot.get("static_id"), f"spot static_id of npc {npc_id}", 0), spot.get("ai")))
					by_npc.setdefault(npc_id, []).append(group)
			return {m: [g for groups in by_npc.values() for g in groups] for m, by_npc in by_map.items()}
		return self._once("spawn_groups", load)

	def npc_spots(self, npc_id: int) -> list[dict]:
		rows = []
		for map_id, groups in sorted(self.spawn_groups().items()):
			for g in groups:
				if g.npc_id != npc_id:
					continue
				for s in g.spots:
					rows.append({"map": map_id, "x": s.x, "y": s.y, "z": s.z, "heading": s.h, "staticId": s.static_id,
					             "difficultId": g.difficult_id, "pool": 0 < g.pool < len(g.spots),
					             "temporary": g.temporary is not None or s.temporary is not None, "handler": g.handler, "spotAi": s.ai})
		return rows


# ---- prices ------------------------------------------------------------------------------------------------------------------------------

def load_prices(java_src: Path, config_dir: Path, profile: Path | None, require_profile: bool, race: str) -> dict:
	"""PricesService's factors for `race` with sieges off (influence 0): m5c-trade's race_prices over the profile's price keys."""
	from m5c.trade import race_prices
	from m5c.trade_config import load_config
	config = load_config(java_src, config_dir, profile, [], require_profile=require_profile)
	# with gameserver.siege.enable the influence comes from the database; the gate runs with sieges off, whose influence is 0 (m5c §2.10)
	influence = 0 if config["gameserver.siege.enable"].value else None
	prices = race_prices(config, race, influence)
	prices["siegeEnabledInConfig"] = bool(config["gameserver.siege.enable"].value)
	return prices


def service_price(base: int, prices: dict) -> int:
	"""PricesService.getPriceForService (PricesService.java:86-90): the m5c economy helper (three truncating `(long) (x * f / 100D)`)."""
	from m5c.economy import service_price as m5c_service_price
	return m5c_service_price(base, prices)


def daeva_only_npcs(java_src: Path) -> list[int]:
	"""DialogService.onDialogSelect's AIRLINE_SERVICE arm (DialogService.java:187-197): the npc ids of the switch that sends NO_RIGHT to a
	non-Daeva before TeleportService.showMap, read from the source."""
	path = Path(java_src) / "com" / "aionemu" / "gameserver" / "services" / "DialogService.java"
	try:
		text = path.read_text(encoding="utf-8")
	except OSError as e:
		raise OracleError(f"{path}: {e}") from e
	text = re.sub(r"//[^\n]*", "", text)
	m = re.search(r"case AIRLINE_SERVICE:\s*\{\s*switch \(npc\.getNpcId\(\)\) \{((?:\s*case \d+:)+)\s*if \(!player\.getCommonData\(\)\.isDaeva\(\)\) \{"
	              r"\s*PacketSendUtility\.sendPacket\(player, new SM_DIALOG_WINDOW\(npc\.getObjectId\(\), DialogPage\.NO_RIGHT\.id\(\)\)\);\s*return;",
	              text)
	if not m:
		raise OracleError(f"{path}: the AIRLINE_SERVICE arm does not have the shape this oracle was written against (DialogService.java:187-197)")
	return [int(v) for v in re.findall(r"case (\d+):", m.group(1))]


# ---- selectors ---------------------------------------------------------------------------------------------------------------------------

def npc_report(td: TravelData, npc_id: int, race: str | None, prices_for, daeva_only: list[int]) -> dict:
	npc = td.npc(npc_id)
	price_race = race or npc.race
	if price_race not in RACES:
		raise OracleError(f"npc {npc_id} has race {npc.race}: pass --race ELYOS|ASMODIANS for the prices")
	prices = prices_for(price_race)
	found = td.teleporter_of(npc_id)
	locations = []
	if found is not None:
		telelocs = td.telelocs()
		for loc in found[1]:
			tpl = telelocs.get(loc.loc_id)
			row = {"locId": loc.loc_id, "type": loc.loc_type, "teleportId": loc.teleportid, "price": loc.price, "pricePvp": loc.price_pvp,
			       # TeleportService.checkKinahForTransportation (TeleportService.java:158-177): no HiPass -> getPriceForService(price, race)
			       "servicePrice": service_price(loc.price, prices), "requiredQuest": loc.required_quest}
			if tpl is None:
				# TeleportService.teleport: "Missing teleloc_template" -> STR_CANNOT_MOVE_TO_AIRPORT_NO_ROUTE (TeleportService.java:73-78)
				row.update({"map": None, "x": None, "y": None, "z": None, "heading": None, "headingByte": None, "name": None, "missingTemplate": True})
			else:
				# REGULAR: sendLoc(mapId, ..., x, y, z, (byte) heading) (TeleportService.java:123-131); FLIGHT: no position, the client flies
				row.update({"map": tpl["map"], "x": tpl["x"], "y": tpl["y"], "z": tpl["z"], "heading": tpl["heading"],
				            "headingByte": java_byte(tpl["heading"]), "name": tpl["name"], "hasPosition": tpl["hasPosition"]})
			locations.append(row)
	return {"npc": npc_id, "name": npc.name, "ai": npc.ai, "race": npc.race, "tribe": npc.tribe, "talkDistance": npc.talk_distance,
	        "spots": td.npc_spots(npc_id), "daevaOnly": npc_id in daeva_only, "priceRace": price_race,
	        "prices": {k: prices[k] for k in ("globalPrices", "globalPricesModifier", "taxes", "influence", "siegeEnabledInConfig")},
	        "teleporter": None if found is None else {"teleportId": found[0], "type": _teleporter_type(found[1])},
	        "locations": locations}


def _teleporter_type(locs: list[TeleLoc]) -> str:
	kinds = sorted({l.loc_type for l in locs})
	return kinds[0] if len(kinds) == 1 else "MIXED" if kinds else "NONE"


def hotspot_distance(px: float, py: float, pz: float, hx: float, hy: float, hz: float) -> float:
	"""PositionUtil.getDistance(float x1, y1, z1, x2, y2, z2) (PositionUtil.java:223-230): float differences, float squares and float sum
	(`dx * dx + dy * dy + dz * dz` is float arithmetic), then Math.sqrt of that float widened to double."""
	dx = f32(px - hx)
	dy = f32(py - hy)
	dz = f32(pz - hz)
	return math.sqrt(f32(f32(f32(dx * dx) + f32(dy * dy)) + f32(dz * dz)))


def hotspot_distance_double(px: float, py: float, pz: float, hx: float, hy: float, hz: float) -> float:
	"""the same distance in double arithmetic (what a port must NOT do; for the unit test and the mutation proof only)"""
	return math.sqrt((px - hx) ** 2 + (py - hy) ** 2 + (pz - hz) ** 2)


def hotspot_price(base: int, distance: float) -> int:
	"""BindPointTeleportService.calculateTeleportationPrice (BindPointTeleportService.java:81-85): `(long) (basePrice * distance / 1000d)`
	(long * double -> double), then Math.max(1, basePrice + distanceCost)."""
	distance_cost = to_long(float(base) * distance / 1000.0)
	return max(1, base + distance_cost)


def hotspot_report(td: TravelData, hotspot_id: int, origin: tuple[float, float, float]) -> dict:
	h = td.hotspots().get(hotspot_id)
	if h is None:
		raise OracleError(f"hotspot {hotspot_id}: no hotspot_location (BindPointTeleportService.teleport audits and refuses)")
	px, py, pz = (f32(v) for v in origin)
	dist = hotspot_distance(px, py, pz, h["x"], h["y"], h["z"])
	return {"hotspot": hotspot_id, "map": h["map"], "race": h["race"], "x": h["x"], "y": h["y"], "z": h["z"],
	        # BindPointTeleportService.java:67 -> teleportTo(player, worldId, x, y, z) = teleportTo(..., player.getHeading(), NONE)
	        # (TeleportService.java:249-251): the player keeps its own heading
	        "heading": None,
	        "basePrice": h["basePrice"], "from": [px, py, pz], "distance": dist, "price": hotspot_price(h["basePrice"], dist),
	        "doublePrice": hotspot_price(h["basePrice"], hotspot_distance_double(px, py, pz, h["x"], h["y"], h["z"]))}


def obelisk_report(td: TravelData, npc_id: int) -> dict:
	npc = td.npc(npc_id)
	bind = td.bind_points().get(npc_id)
	if bind is None:
		# ResurrectAI.handleDialogStart: "There is no bind point template for npc" and nothing else (ResurrectAI.java:45-50)
		raise OracleError(f"npc {npc_id} has no bind_point template (ResurrectAI logs and does nothing)")
	return {"npc": npc_id, "name": npc.name, "ai": npc.ai, "race": npc.race, "tribe": npc.tribe, "talkDistance": npc.talk_distance,
	        "spots": td.npc_spots(npc_id),
	        # ResurrectAI.bindHere: the question carries the raw price and acceptRequest decreases it raw, no PricesService (ResurrectAI.java:75-106)
	        "bindPoint": {"id": bind["npcId"], "npcId": bind["npcId"], "name": bind["name"], "price": bind["price"], "race": npc.race,
	                      "tribe": npc.tribe}}


# -- portals and the instance cooltime

class Zone:
	"""ServerTime's zone (GSConfig.TIME_ZONE_ID: gameserver.timezone, empty = ZoneId.systemDefault(), ZoneIdTransformer). 'local' uses the
	machine's own rules through time.localtime/mktime (Windows ships no IANA database for zoneinfo); 'UTC' and '+HH:MM' are fixed offsets;
	any other name needs zoneinfo's database."""

	def __init__(self, name: str):
		self.name = name
		self.tz = None
		if name == "local":
			return
		m = re.fullmatch(r"(?:UTC|GMT)?([+-])(\d{1,2})(?::?(\d{2}))?", name)
		if name in ("UTC", "GMT", "Z"):
			self.tz = dt.timezone.utc
		elif m:
			sign = 1 if m.group(1) == "+" else -1
			self.tz = dt.timezone(sign * dt.timedelta(hours=int(m.group(2)), minutes=int(m.group(3) or 0)))
		else:
			try:
				import zoneinfo
				self.tz = zoneinfo.ZoneInfo(name)
			except Exception as e:  # ZoneInfoNotFoundError, or no tz database on this machine
				raise OracleError(f"--tz {name}: not a zone this Python can resolve ({e}); use 'local', 'UTC' or a fixed offset like +02:00") from e

	def local_date(self, epoch_ms: int) -> dt.date:
		seconds = epoch_ms // 1000
		if self.tz is None:
			t = time.localtime(seconds)
			return dt.date(t.tm_year, t.tm_mon, t.tm_mday)
		return dt.datetime.fromtimestamp(seconds, self.tz).date()

	def epoch_seconds(self, day: dt.date, hour: int, minute: int) -> int:
		"""ZonedDateTime of the local date and time (seconds 0) -> toEpochSecond"""
		if self.tz is None:
			return int(time.mktime((day.year, day.month, day.day, hour, minute, 0, 0, 0, -1)))
		return int(dt.datetime(day.year, day.month, day.day, hour, minute, tzinfo=self.tz).timestamp())


DAYS = {"Mon": 1, "Tue": 2, "Wed": 3, "Thu": 4, "Fri": 5, "Sat": 6, "Sun": 7}  # InstanceCooltimeData.getDay (InstanceCooltimeData.java:112-128)


def days_until_reset(type_value: str | None, day_of_week: int) -> int:
	"""InstanceCooltimeData.calculateDaysUntilReset (InstanceCooltimeData.java:103-110)"""
	if type_value is None:
		raise OracleError("a WEEKLY instance cooltime without <typevalue>: NullPointerException in calculateDaysUntilReset")
	days = []
	for name in type_value.split(","):
		if name not in DAYS:
			raise OracleError(f"typevalue day {name!r}: IllegalArgumentException(\"Invalid Day\")")
		days.append(DAYS[name])
	days.sort()
	for reset in days:
		if reset >= day_of_week:
			return reset - day_of_week
	return (7 - day_of_week) + days[0]


def entrance_cooltime_ms(clt: dict | None, now_ms: int, zone: Zone) -> int:
	"""InstanceCooltimeData.calculateInstanceEntranceCooltime (InstanceCooltimeData.java:70-101) with getInstanceRate == 1."""
	if clt is None or clt["maxCount"] == 0:
		return 0
	kind = clt["type"]
	if kind in ("DAILY", "WEEKLY"):
		ect = clt["entCoolTime"]
		hour, minute = int(ect / 100), _int32(ect - int(ect / 100) * 100)  # Java int / and % truncate toward zero
		if not (0 <= hour <= 23 and 0 <= minute <= 59):
			raise OracleError(f"ent_cool_time {ect}: LocalTime.of({hour}, {minute}) throws DateTimeException")
		day = zone.local_date(now_ms)
		repeat = zone.epoch_seconds(day, hour, minute)  # now.with(LocalTime.of(hour, minute)): today at HH:MM:00.000
		if now_ms > repeat * 1000:  # now.isAfter(repeatDate)
			day = day + dt.timedelta(days=1)
			repeat = zone.epoch_seconds(day, hour, minute)  # plusDays(1): the same local time the next day
		if kind == "WEEKLY":
			day = day + dt.timedelta(days=days_until_reset(clt["typeValue"], day.isoweekday()))
			repeat = zone.epoch_seconds(day, hour, minute)
		return repeat * 1000
	if kind == "RELATIVE":
		minutes = clt["entCoolTime"]
		if minutes == 0:
			return 0
		return now_ms + _int32(minutes * 60 * 1000)  # int arithmetic, then widened to long
	return 0  # "Unhandled InstanceCoolTimeType" (a warning), instanceCoolTime stays 0


def _path_out(td: TravelData, path: PortalPathRow, race: str, selected: bool) -> dict:
	a = path.attrs
	what = "portal_path"
	loc_id = java_int(a.get("loc_id"), f"{what} loc_id", 0)
	loc = td.portal_locs().get(loc_id)
	row = {"locId": loc_id, "selected": selected, "race": a.get("race", "PC_ALL"), "dialog": java_int(a.get("dialog"), f"{what} dialog", 0),
	       "siegeId": java_int(a.get("siege_id"), f"{what} siege_id", 0), "minLevel": java_int(a.get("min_level"), f"{what} min_level", 0),
	       "minRank": java_int(a.get("min_rank"), f"{what} min_rank", 0), "kinah": java_int(a.get("kinah"), f"{what} kinah", 0),
	       "titleId": java_int(a.get("title_id"), f"{what} title_id", 0), "errGroup": java_int(a.get("err_group"), f"{what} err_group", 0),
	       "errLevel": java_int(a.get("err_level"), f"{what} err_level", 0), "questReq": path.quest_req, "itemReq": path.item_req}
	if loc is None:
		# PortalService.port: "No portal loc for locId" and return (PortalService.java:50-54)
		row.update({"map": None, "instance": None, "x": None, "y": None, "z": None, "heading": None, "missingLoc": True})
		return row
	world = td.world_maps().get(loc["map"])
	row.update({"map": loc["map"], "instance": bool(world and world["instance"]), "x": loc["x"], "y": loc["y"], "z": loc["z"], "heading": loc["heading"]})
	clt = td.cooltimes().get(loc["map"])
	# PortalService.port's maxPlayers (PortalService.java:58-59) and checkEnterLevel's limits (PortalService.java:204-215)
	row["maxPlayers"] = 0 if clt is None else clt["maxMemberLight"] if race == "ELYOS" else clt["maxMemberDark"]
	enter_min, enter_max = row["minLevel"], 0
	if clt is not None:
		if enter_min == 0:
			enter_min = clt["enterMinLevelLight"] if race == "ELYOS" else clt["enterMinLevelDark"]
		enter_max = clt["enterMaxLevelLight"] if race == "ELYOS" else clt["enterMaxLevelDark"]
	row["enterMinLevel"] = enter_min
	row["enterMaxLevel"] = enter_max
	return row


def instance_exit(td: TravelData, world: int, race: str) -> dict | None:
	"""InstanceExitData.getInstanceExit: the first exit of the world whose race is PC_ALL or the player's"""
	for e in td.instance_exits().get(world, []):
		if e["race"] == "PC_ALL" or e["race"] == race:
			return dict(e)
	return None


def portal_report(td: TravelData, npc_id: int, race: str, now_ms: int, zone: Zone) -> dict:
	npc = td.npc(npc_id)
	uses, dialogs = td.portal_templates()
	paths_rows = []
	selected_index = None
	use = uses.get(npc_id)
	if use is not None:
		# Portal2Data.getPortalUsePath: the first path of the player's race or PC_ALL, else the last path (for the race error)
		for i, p in enumerate(use):
			if p.attrs.get("race", "PC_ALL") in (race, "PC_ALL"):
				selected_index = i
				break
		if selected_index is None and use:
			selected_index = len(use) - 1
		paths_rows = [_path_out(td, p, race, i == selected_index) for i, p in enumerate(use)]
	dialog = dialogs.get(npc_id)
	dialog_rows = [] if dialog is None else [_path_out(td, p, race, False) for p in dialog[1]]
	selected = paths_rows[selected_index] if selected_index is not None else None
	cooltime = None
	reuse = None
	exit_row = None
	if selected is not None and selected.get("map") is not None:
		clt = td.cooltimes().get(selected["map"])
		if clt is not None:
			cooltime = {k: clt[k] for k in ("id", "worldId", "type", "typeValue", "maxCount", "entCoolTime", "maxMemberLight", "maxMemberDark",
			                                "enterMinLevelLight", "enterMaxLevelLight", "enterMinLevelDark", "enterMaxLevelDark", "canEnterMentor",
			                                "syncId", "race")}
		reuse = entrance_cooltime_ms(clt, now_ms, zone) if clt is not None else None
		exit_row = instance_exit(td, selected["map"], race)
	return {"npc": npc_id, "name": npc.name, "ai": npc.ai, "race": race, "spots": td.npc_spots(npc_id),
	        # ActionItemNpcAI.getTalkDelayInMs: getTalkDelay() * 1000 (talk_info delay in seconds; ActionItemNpcAI.java:83-85)
	        "talkDelayMs": _int32(npc.talk_delay * 1000), "talkDistance": npc.talk_distance,
	        # PositionUtil.isInTalkRange: talk distance + 1, bound radii considered (PositionUtil.java:306-309)
	        "talkRange": npc.talk_distance + 1,
	        "paths": paths_rows, "dialogPaths": dialog_rows,
	        "teleportDialogId": None if dialog is None else dialog[0],
	        "teleporterFallback": use is None and td.teleporter_of(npc_id) is not None,
	        "cooltime": cooltime, "nowMs": now_ms, "timeZone": zone.name, "reuseTimeMs": reuse,
	        # SM_INSTANCE_INFO writes (int) (reuseTime - now) / 1000 (SM_INSTANCE_INFO.java:47)
	        "reuseRemainingSeconds": None if not reuse else int(_int32(reuse - now_ms) / 1000),
	        "exit": exit_row}


def instance_exit_report(td: TravelData, world: int, race: str) -> dict:
	row = instance_exit(td, world, race)
	return {"world": world, "race": race, "exit": row}


def instance_spawns_report(td: TravelData, world: int, difficulty: int, near: tuple[float, float, float], radius: float, clock: GameClock) -> dict:
	"""SpawnEngine.spawnInstance (SpawnEngine.java:136-180): a group with difficult_id != 0 spawns only for its difficulty, a temporary group only
	in spawn time, a handler group spawns no npc, a pool a random subset, else every spot (a spot's own temporary_spawn checked) - m5a-spawns'
	evaluate() with the instance's difficulty mapped to 0."""
	groups = td.spawn_groups().get(world)
	if groups is None:
		raise OracleError(f"no spawn_map for world {world}")
	adjusted = [replace(g, difficult_id=0) if g.difficult_id == difficulty else g for g in groups]
	from m5a.spawns import NpcInfo
	npcs = td.npcs()
	infos = {k: NpcInfo(v.level, v.npc_type) for k, v in npcs.items()}
	rows = evaluate(adjusted, infos, clock)
	cx, cy, cz = (f32(v) for v in near)
	spots = []
	for r in rows:
		if not in_range(cx, cy, cz, r["x"], r["y"], r["z"], radius):
			continue
		npc = npcs.get(r["npcId"])
		ai = r["spotAi"] if r["spotAi"] is not None else (npc.ai if npc else None)
		spots.append({"npcId": r["npcId"], "name": npc.name if npc else None, "x": r["x"], "y": r["y"], "z": r["z"], "heading": r["h"], "ai": ai,
		              "spawned": r["spawned"],
		              "fixed": r["spawned"] is True and not r["flags"]["walker"] and not r["flags"]["randomWalk"],
		              "temporary": r["flags"]["temporary"], "pool": r["flags"]["pool"], "walker": r["flags"]["walker"],
		              "randomWalk": r["flags"]["randomWalk"], "handler": r["flags"]["handler"], "staticId": r["staticId"],
		              "distance": round(report_distance(cx, cy, cz, r["x"], r["y"], r["z"]), 3)})
	spots.sort(key=lambda s: (s["distance"], s["npcId"]))
	placed = [r for r in rows if r["spawned"] is not False]
	return {"world": world, "difficulty": difficulty, "near": [cx, cy, cz], "radius": radius,
	        # the whole instance: spots spawnInstance may place (spawned true or unknown) and their npc ids
	        "total": {"spots": len(placed), "npcIds": len({r["npcId"] for r in placed}),
	                  "certain": sum(1 for r in rows if r["spawned"] is True),
	                  # without the gatherables (ids 400001..499998, VisibleObjectSpawner: not an Npc)
	                  "npcSpots": sum(1 for r in placed if not r["flags"]["gatherable"]),
	                  "npcNpcIds": len({r["npcId"] for r in placed if not r["flags"]["gatherable"]})},
	        "gameTime": {"hour": clock.hour, "day": clock.day, "month": clock.month, "weekday": clock.weekday}, "spots": spots}


def exp_report(td: TravelData, level: int) -> dict:
	"""PlayerExperienceTable.getStartExpForLevel (PlayerExperienceTable.java:29-34)"""
	table = td.experience()
	if level > len(table):
		raise OracleError(f"level {level} is above the table's max level {len(table)} (IllegalArgumentException)")
	if level < 0:
		raise OracleError(f"level {level}: experience[{level - 1}] throws ArrayIndexOutOfBoundsException")
	return {"level": level, "exp": 0 if level == 0 else table[level - 1], "maxLevel": len(table)}
