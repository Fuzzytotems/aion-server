"""m5h-housing: the constants the M5h studio cases assert (m5h-plan.md G-01, §10.2 C14-C20), read from the static data and the Java sources.

- the npcs of the studio path (the Parrine of the paid studio, the studio entrance and the studio exit): their first spawn spot
  (spawns/<dir>/<map>_*.xml) and their talk_info (npc_templates.xml: distance and delay, the use bar's time);
- the Elyos studio (HouseData.getStudioAddress: address 2001, houses.xml): its address point and exit, its land's default building, sale
  price and house npc ids (manager = the butler, teleport = the relationship crystal) and the house_npcs.xml spots of the address;
- the furniture the gate places: an item's <houseobject id>/<housedeco id> (item_templates.xml) and the house object's template
  (housing_objects.xml: its kind, use_days, and for a use_item its delay, cd, limit, talking_distance and reward);
- PartType's line numbers (PartType.java: the decor ids SM_HOUSE_RENDER writes, room by room) and HouseDoorState's ids;
- SM_SYSTEM_MESSAGE ids by name (m5h.legion.message_ids).
"""

from __future__ import annotations

import re
import xml.etree.ElementTree as ET
from pathlib import Path

from staticdata_oracle import OracleError

from m5a.creation import enum_constants
from m5h.legion import _read, message_ids

STUDIO_ADDRESS = 2001  # HouseData.getStudioAddress(ELYOS) (HouseData.java)


def _iter(path: Path, tag: str):
	try:
		for _, element in ET.iterparse(path, events=("end",)):
			if element.tag == tag:
				yield element
	except (OSError, ET.ParseError) as e:
		raise OracleError(f"{path}: {e}") from e


def _float(element: ET.Element, name: str, what: str) -> float:
	value = element.get(name)
	if value is None:
		raise OracleError(f"{what}: no {name}")
	return float(value)


def npc_spawn(data_dir: Path, map_id: int, npc_id: int) -> dict:
	"""The first spot of the first <spawn npc_id> in the map's spawn files (Npcs/ and Instances/)."""
	for path in sorted((data_dir / "spawns").glob(f"*/{map_id}_*.xml")):
		for spawn in _iter(path, "spawn"):
			if spawn.get("npc_id") == str(npc_id):
				spot = spawn.find("spot")
				if spot is None:
					raise OracleError(f"{path}: npc {npc_id} has no spot")
				what = f"{path.name} npc {npc_id}"
				return {"map": map_id, "x": _float(spot, "x", what), "y": _float(spot, "y", what), "z": _float(spot, "z", what),
				        "h": int(spot.get("h", "0"))}
	raise OracleError(f"no spawn of npc {npc_id} on map {map_id}")


def npc_talk(data_dir: Path, npc_ids: set[int]) -> dict:
	found = {}
	for template in _iter(data_dir / "npcs" / "npc_templates.xml", "npc_template"):
		npc_id = int(template.get("npc_id", "0"))
		if npc_id in npc_ids:
			talk = template.find("talk_info")
			bound = template.find("bound_radius")
			found[npc_id] = {"ai": template.get("ai"), "talkDistance": float(talk.get("distance", "0")) if talk is not None else 0.0,
			                 "talkDelayMs": int(talk.get("delay", "0")) * 1000 if talk is not None else 0,
			                 "boundRadius": float(bound.get("side", "0")) if bound is not None else 0.0}
			if len(found) == len(npc_ids):
				break
		template.clear()
	missing = npc_ids - set(found)
	if missing:
		raise OracleError(f"npc_templates.xml: no template for {sorted(missing)}")
	return found


def studio(data_dir: Path) -> dict:
	houses = data_dir / "housing" / "houses.xml"
	try:
		root = ET.parse(houses).getroot()
	except (OSError, ET.ParseError) as e:
		raise OracleError(f"{houses}: {e}") from e
	for land in root.iter("land"):
		for address in land.iter("address"):
			if address.get("id") != str(STUDIO_ADDRESS):
				continue
			what = f"houses.xml address {STUDIO_ADDRESS}"
			default = [b for b in land.iter("building") if b.get("default") == "true"]
			sale = land.find("sale")
			if not default or sale is None:
				raise OracleError(f"{what}: the land has no default building or no sale")
			result = {"address": STUDIO_ADDRESS, "map": int(address.get("map")), "x": _float(address, "x", what), "y": _float(address, "y", what),
			          "z": _float(address, "z", what), "exitMap": int(address.get("exit_map")), "exitX": _float(address, "exit_x", what),
			          "exitY": _float(address, "exit_y", what), "exitZ": _float(address, "exit_z", what), "building": int(default[0].get("id")),
			          "goldPrice": int(sale.get("gold_price")), "managerNpc": int(land.get("manager_npc")), "teleportNpc": int(land.get("teleport_npc"))}
			result["houseNpcs"] = house_npcs(data_dir, STUDIO_ADDRESS)
			return result
	raise OracleError(f"houses.xml: no address {STUDIO_ADDRESS}")


def house_npcs(data_dir: Path, address: int) -> dict:
	path = data_dir / "housing" / "house_npcs.xml"
	for house in _iter(path, "house"):
		if house.get("address") == str(address):
			return {spawn.get("type"): {"x": _float(spawn, "x", "house_npcs"), "y": _float(spawn, "y", "house_npcs"), "z": _float(spawn, "z", "house_npcs")}
			        for spawn in house.findall("spawn")}
	raise OracleError(f"house_npcs.xml: no house {address}")


def items(data_dir: Path, item_ids: list[int]) -> dict:
	wanted = set(item_ids)
	found = {}
	for template in _iter(data_dir / "items" / "item_templates.xml", "item_template"):
		item_id = int(template.get("id", "0"))
		if item_id in wanted:
			house_object = template.find("actions/houseobject")
			decoration = template.find("actions/housedeco")
			found[item_id] = {"houseObject": int(house_object.get("id")) if house_object is not None else None,
			                  "decoration": int(decoration.get("id")) if decoration is not None and decoration.get("id") else None}
			if len(found) == len(wanted):
				break
		template.clear()
	missing = wanted - set(found)
	if missing:
		raise OracleError(f"item_templates.xml: no item {sorted(missing)}")
	objects = {v["houseObject"] for v in found.values() if v["houseObject"] is not None}
	templates = {}
	path = data_dir / "housing" / "housing_objects.xml"
	try:
		root = ET.parse(path).getroot()
	except (OSError, ET.ParseError) as e:
		raise OracleError(f"{path}: {e}") from e
	for element in root:
		object_id = int(element.get("id", "0"))
		if object_id in objects:
			entry = {"kind": element.tag, "useDays": int(element.get("use_days", "0")), "talkingDistance": float(element.get("talking_distance", "0"))}
			if element.tag == "use_item":
				action = element.find("action")
				entry.update({"delayMs": int(element.get("delay", "0")), "cooldownSeconds": int(element.get("cd", "0")), "limit": element.get("limit"),
				              "owner": element.get("owner") == "true", "rewardId": int(action.get("reward_id")) if action is not None else None})
			templates[object_id] = entry
	for item_id, value in found.items():
		if value["houseObject"] is not None:
			if value["houseObject"] not in templates:
				raise OracleError(f"housing_objects.xml: no house object {value['houseObject']} (item {item_id})")
			value["template"] = templates[value["houseObject"]]
	return {str(k): v for k, v in found.items()}


def part_types(java_src: Path) -> list:
	"""PartType's constants in declaration order with their line numbers (PartType(int packetLineStart, int packetLineEnd))."""
	base = Path(java_src) / "com" / "aionemu" / "gameserver"
	result = []
	for name, args in enum_constants(base / "model" / "templates" / "housing" / "PartType.java", "PartType"):
		start, end = [int(a.strip()) for a in args.split(",")]
		result.append({"name": name, "startLine": start, "rooms": end - start + 1})
	return result


def door_states(java_src: Path) -> dict:
	base = Path(java_src) / "com" / "aionemu" / "gameserver"
	return {name: int(args.strip()) for name, args in enum_constants(base / "model" / "house" / "HouseDoorState.java", "HouseDoorState")}


def script_limits(java_src: Path) -> dict:
	text = _read(Path(java_src) / "com" / "aionemu" / "gameserver" / "network" / "aion" / "serverpackets" / "SM_HOUSE_SCRIPTS.java")
	padding = re.search(r"SCRIPT_PADDING\s*=\s*(?:new\s+byte\[\]\s*)?\{([^}]*)\}", text)
	if padding is None:
		raise OracleError("SM_HOUSE_SCRIPTS.java: SCRIPT_PADDING not found")
	return {"padding": [int(v.strip(), 0) & 0xFF for v in padding.group(1).split(",") if v.strip()]}


def housing_report(data_dir: Path, java_src: Path, messages: list[str], npc_spots: list[tuple[int, int]], item_ids: list[int]) -> dict:
	base = Path(java_src) / "com" / "aionemu" / "gameserver"
	npcs = {}
	talk = npc_talk(data_dir, {npc for _, npc in npc_spots})
	for map_id, npc_id in npc_spots:
		npcs[str(npc_id)] = {**npc_spawn(data_dir, map_id, npc_id), **talk[npc_id]}
	return {
		"messages": message_ids(base, messages),
		"npcs": npcs,
		"studio": studio(data_dir),
		"items": items(data_dir, item_ids),
		"partTypes": part_types(java_src),
		"doorStates": door_states(java_src),
		"scripts": script_limits(java_src),
	}
