"""m5d-quests --registration-order and --census: the registry QuestEngine.init builds from the XML quests, and the quest census of
m5d-plan.md §2.3-§2.5 (G-01, §18.3; T-04's registration-order case compares the C++ registry against the first).

The registration order (Java rules):
- QuestEngine.init (QuestEngine.java:86-110) registers `DataManager.XML_QUESTS.getAllQuests()` (:104-105), the values of XMLQuests.questsById,
  a `new HashMap<>()` that afterUnmarshal fills in document order (XMLQuests.java:30-43; a later id replaces the value and keeps the place). So
  the XML quests register in that map's iteration order (javasrc.hash_iteration_order), each quest's register() whole before the next
  (templates.registration);
- QuestNpc.addOnTalkEvent and addOnKillEvent (QuestNpc.java:59-75) append the quest once to an ArrayList: an npc's onTalkEvent and onKillEvent
  lists are the quests registering there, in registration order. QuestNpc.onQuestStart is a `new HashSet<>(0)` (QuestNpc.java:27, 39-43),
  iterated in its bucket order over that insertion order (the order the follow-up dialog walks, AbstractQuestHandler.sendQuestEndDialog);
- each list's flags say whether it is also the ascending order or the document order, and a start set's whether it is also its insertion
  order: a list with a true flag cannot tell Java's order from a sorted container, from an insertion-ordered registry filled in document order,
  or from an insertion-ordered set (the C++ runtime::HashSet iterates in insertion order). `talkLists`, `killLists` and `startLists` count
  the npcs whose list is none of those and name them (mires 203057's onQuestStart [1104, 1102, 1103] against its insertion [1102, 1103, 1104]);
- QuestEngine.registerOnEnterWorld (QuestEngine.java:765-768) appends the quest once to the ArrayList questOnEnterWorld, which onEnterWorld
  walks (:340-350); registerOnLevelChanged (:738-754) appends it to the list of the template's race_permitted, or to ASMODIANS' and then ELYOS'
  when the template has none. A PC_ALL quest goes to a PC_ALL list that onLevelChanged (:230-244), which reads the player's race, never reads;
- m5d-plan.md D9: the C++ registry holds the XML quests only. The Java server loads its handlers (data/handlers/quest) first, in an order this
  oracle does not know, so in the Java server their quests come before the XML quests in every list (`javaStartQuests` names those that start
  at an npc).

The census (m5d-plan.md §2.3-§2.5 rev 2, re-derived from the data and the Java rules):
- §2.3: the quest_data templates by category; the XML quests (XMLQuests), the Java handlers (the text scan of data/handlers/quest), both, and
  neither by category; the XML quests with restricted="true" (QuestTemplate.isRestricted has no caller);
- §2.4, where a player can start each XML quest: a partition, the first row that matches:
  1. `disabled`: minlevel_permitted 99;
  2. `noStartNpc`: register() puts the quest into no npc's onQuestStart (started by an item, a zone, a level, a kill or another handler);
  3. a start npc spawned at startup: SpawnEngine.spawnAll (SpawnEngine.java:119-189) spawns the regular spawns (a <spawn> directly under
     <spawn_map>) of every map that is not an instance (WorldMap.isInstanceType), for difficulty 0 and without a handler (a RIFT spawn goes to
     RiftManager, a STATIC one is a StaticObject), as an Npc when the npc has a template and is not a gatherable; a temporary spawn counts (it
     spawns in its time). The ai is the spot's `ai` when it has one (Creature.java:64-66, "__NO_AI__" is none), else the template's. By the
     ais of those spawns: TALKING_AIS (GeneralNpcAI, whose handleDialogStart runs TalkEventHandler.onTalk, and AggressiveNpcAI, which extends
     it) make `talkingAi`, else D5_AIS (AbyssGuardSimpleAI, m5d-plan.md D5 and A-01) `abyssGuardAi`, else `otherAi`. A talkingAi or
     abyssGuardAi quest is `...BlockedByJava` when fewer of its <start_conditions> groups can pass than QuestTemplate.getRequiredConditionCount
     asks (QuestTemplate.java:170-188, QuestService.java:365-372), a group failing when it names a quest with a Java handler under <finished>
     or <acquired> (phase 6; <unfinished> and <noacquired> of a quest never started pass). Only these direct names count, not a chain;
  4. `serviceSpawned`: a start npc is spawned by a service (the nested siege/base/rift/vortex/mercenary/ahserion spawns, which SpawnsData
     keeps for their services, SpawnsData.java:56-77, 101-118, 185-199; the regular spawns of instance maps, spawned with the instance; a
     RIFT regular spawn, which spawnInstance hands to RiftManager), counted under the first kind of SERVICE_KINDS that holds one of its start
     npcs (`inSeveralKinds` lists the quests with more than one);
  5. `neverSpawned`: no spawn of the spawns holder makes a start npc an Npc. Sub-partition, the first that matches: `townSpawns`, a start npc
     of town_spawns, which TownService spawns at startup at each town's level (GameServer.java:126, TownService.java:40-66,
     Town.spawnNewObjects; a new town is level 1) - rev 2 counted them here; `houseSpawns`, a start npc a house spawns at startup (spawnAll ->
     spawnInstance -> HousingService.spawnHouses, HousingService.java:149-174, brings in every house of an open-world map whose land's default
     building is not a PERSONAL_INS studio; HouseController.onAfterSpawn -> updateSpawns, HouseController.java:52-89, spawns per house_npcs
     spawn of the address the land's manager_npc (MANAGER), teleport_npc (TELEPORT) or sign npc (SIGN: one of the four, by the house's owner
     and bids)) - rev 2 counted them here too; `eventSpawns`, a start npc of a timed event's spawns (spawned while the event runs);
     `inertSpawns`, a start npc whose regular spawns spawn no Npc (INERT_KINDS: a STATIC spawn is a StaticObject, spawnInstance's switch does
     nothing for another handler, an open-world spawn of a difficulty other than 0, an npc without template or a gatherable, a map world_maps
     does not have, a spawn without spots); `noSpawnData`, a start npc in no spawn, town, house or event data: Java code spawns it or nothing
     does. Its `namedInJava` is a text scan, a name and not a proof of a spawn: `outsideQuestHandlers`, a start npc id written as an int
     literal in data/handlers outside quest (instance, ai, ... handlers: phase 5 and M5f) or in game-server/src; `questHandlersOnly`, only in
     data/handlers/quest (phase 6); `nowhere`.
  `reachable` is talkingAi + abyssGuardAi (after M5d's D5); `serviceOtherwiseReachable` the serviceSpawned quests that a talking or D5 ai
  would start without a Java-handled precondition, per spawn kind.
- §2.4, what completing a reachable quest needs, by kind: `dialogsAndKills` (report_to, monster_hunt, report_to_many, xml_quest);
  item_collecting as `questObjects` (a <quest_drop> from a quest object, npc id / 100000 == 7, AbstractQuestHandler.loadActionItems),
  `questLoot` (quest drops from other npcs only) or `itemsFromElsewhere` (no quest drop); `crafting` (work_order, crafting_rewards);
  `turnIn` (relic_rewards, fountain_rewards); `skillUse`; `pvpKills` (kill_in_zone, kill_in_world); `other` (the kinds without a start npc);
- E-09's reward bodies: a <bonus> (QuestService.getRewardItems calls BonusService.getQuestBonus, QuestService.java:197-203), AP, GP, a cube or
  warehouse extension (giveReward's `!= 0` and `extend_inventory` 1/2 arms, QuestService.java:220-244) in any <rewards> or
  <extended_rewards>, over all XML quests and over the reachable ones;
- the CHALLENGE_TASK XML quests by row (QuestService's ChallengeTaskService calls, QuestService.java:105-106, 438-439);
- §2.5: the quests of the two start zones by handler (quest_zone, which JAXB does not bind: read from the element).
"""

from __future__ import annotations

import re

from m5a.data import java_boolean, java_int
from m5a.spawns import is_gatherable
from staticdata_oracle import OracleError

from .javasrc import hash_iteration_order
from .quests import QuestWorld, required_condition_count

TALKING_AIS = ("general", "aggressive")
D5_AIS = ("simple_abyssguard",)
NO_AI = "__NO_AI__"  # SpawnTemplate.NO_AI: a spot ai that means no ai at all
BOTH_RACES = ("ASMODIANS", "ELYOS")  # QuestEngine.registerOnLevelChanged without race_permitted: ASMODIANS' list first, then ELYOS'
SERVICE_KINDS = ("siege", "instance", "base", "vortex", "ahserion", "rift", "mercenary")
INERT_KINDS = ("static", "handler", "difficulty", "notAnNpc", "noWorldMap", "noSpots")
NESTED_SPAWNS = {"siege_spawn": "siege", "base_spawn": "base", "rift_spawn": "rift", "vortex_spawn": "vortex", "mercenary_spawn": "mercenary",
                 "ahserion_spawn": "ahserion"}
START_ROWS = ("disabled", "noStartNpc", "talkingAi", "abyssGuardAi", "talkingAiBlockedByJava", "abyssGuardAiBlockedByJava", "otherAi",
              "serviceSpawned", "neverSpawned")
NEVER_SPAWNED_ROWS = ("townSpawns", "houseSpawns", "eventSpawns", "inertSpawns", "noSpawnData")
REACHABLE_ROWS = ("talkingAi", "abyssGuardAi")
COMPLETION_KINDS = {"report_to": "dialogsAndKills", "monster_hunt": "dialogsAndKills", "report_to_many": "dialogsAndKills",
                    "xml_quest": "dialogsAndKills", "work_order": "crafting", "crafting_rewards": "crafting", "relic_rewards": "turnIn",
                    "fountain_rewards": "turnIn", "skill_use": "skillUse", "kill_in_zone": "pvpKills", "kill_in_world": "pvpKills"}
COMPLETION_ROWS = ("dialogsAndKills", "questLoot", "crafting", "itemsFromElsewhere", "questObjects", "turnIn", "skillUse", "pvpKills", "other")
QUEST_OBJECT_PREFIX = 7  # AbstractQuestHandler.loadActionItems: npcId / 100000 == 7
DISABLED_LEVEL = 99
START_ZONES = ("Poeta", "Ishalgen")  # m5d-plan.md §2.5: the two start maps' quest_zone names
HOUSE_SPAWN_NPCS = {"MANAGER": ("manager_npc",), "TELEPORT": ("teleport_npc",),  # HouseController.updateSpawns: the land's npc per spawn type
                    "SIGN": ("sign_nosale", "sign_sale", "sign_waiting", "sign_home")}  # getCurrentSignNpcId: by the owner and the bids
STUDIO_BUILDING = "PERSONAL_INS"  # HousingService.spawnHouses skips the studios
JAVA_INT_LITERAL = re.compile(r"(?<![\w.])\d+(?![\w.])")  # a decimal token that is not part of a name or a floating literal


# --- the registration order --------------------------------------------------------------------------------------------------------------


def _order_flags(ids: list[int], position: dict[int, int], insertion: list[int] | None = None) -> dict:
	"""Whether a list is also the ascending order or the document order (then a sorted or an insertion-ordered registry passes it too), and
	with `insertion` (a start set's) whether it is also that insertion order (then an insertion-ordered set passes it too)."""
	flags = {"ascending": ids == sorted(ids), "documentOrder": ids == sorted(ids, key=position.__getitem__)}
	if insertion is not None:
		flags["insertionOrder"] = ids == insertion
	return flags


def _list_summary(by_npc: dict[int, list[int]], position: dict[int, int], neither: str,
                  insertion: dict[int, list[int]] | None = None) -> dict:
	"""The npcs of one kind of list, those with several quests, how many of these are not ascending, not in document order (and not in
	insertion order), and under `neither` the npcs whose list is none of those orders."""
	several = {n: ids for n, ids in by_npc.items() if len(ids) > 1}
	flags = {n: _order_flags(ids, position, insertion[n] if insertion is not None else None) for n, ids in several.items()}
	summary = {"npcs": len(by_npc), "withSeveralQuests": len(several),
	           "notAscending": sum(1 for f in flags.values() if not f["ascending"]),
	           "notDocumentOrder": sum(1 for f in flags.values() if not f["documentOrder"])}
	if insertion is not None:
		summary["notInsertionOrder"] = sum(1 for f in flags.values() if not f["insertionOrder"])
	summary[neither] = sorted(n for n, f in flags.items() if not any(f.values()))
	return summary


def registry_lists(world: QuestWorld) -> dict:
	"""QuestEngine's lists after the XML registration: the order, questOnEnterWorld, questOnLevelUp per race and per npc the onQuestStart
	(iteration order), onTalkEvent and onKillEvent lists."""
	regs = world.registrations()
	enter_world = []
	level_up: dict[str, list[int]] = {}
	talk: dict[int, list[int]] = {}
	start: dict[int, list[int]] = {}
	kill: dict[int, list[int]] = {}
	for quest_id, reg in regs.items():
		if reg.enter_world and quest_id not in enter_world:
			enter_world.append(quest_id)
		if reg.level_changed:
			race = world.template(quest_id)["racePermitted"]
			for key in (race,) if race is not None else BOTH_RACES:
				ids = level_up.setdefault(key, [])
				if quest_id not in ids:
					ids.append(quest_id)
		for npc in reg.quest_start:
			start.setdefault(npc, []).append(quest_id)
		for npc in reg.talk:
			talk.setdefault(npc, []).append(quest_id)
		for npc in reg.kill:
			kill.setdefault(npc, []).append(quest_id)
	return {"order": list(regs), "questOnEnterWorld": enter_world, "questOnLevelUp": level_up,
	        "onQuestStart": {npc: hash_iteration_order(ids, initial_capacity=0) for npc, ids in start.items()}, "onQuestStartInsertion": start,
	        "onTalkEvent": talk, "onKillEvent": kill}


def _npc_view(lists: dict, npc: int, position: dict[int, int], java_start: dict[int, list[int]]) -> dict:
	view = {"registered": any(npc in lists[name] for name in ("onQuestStart", "onTalkEvent", "onKillEvent"))}
	insertion = lists["onQuestStartInsertion"].get(npc, [])
	for name in ("onQuestStart", "onTalkEvent", "onKillEvent"):
		ids = lists[name].get(npc, [])
		view[name] = ids
		view[name + "Flags"] = _order_flags(ids, position, insertion if name == "onQuestStart" else None)
	view["onQuestStartInsertion"] = insertion
	view["javaStartQuests"] = sorted(java_start.get(npc, []))
	return view


def registration_order_report(world: QuestWorld, npc: int | None = None) -> dict:
	"""m5d-quests --registration-order [--npc ID] (aion-m5d-registration-order): with `npc` that npc's lists with their order flags, else
	every npc's lists."""
	lists = registry_lists(world)
	position = {quest_id: i for i, quest_id in enumerate(world.xml_insertion)}
	if npc is not None:
		npcs = {"npc": dict(_npc_view(lists, npc, position, world.java_start_npcs()), npcId=npc)}
	else:
		npcs = {name: {str(n): ids for n, ids in sorted(lists[name].items())} for name in ("onQuestStart", "onTalkEvent", "onKillEvent")}
	return {
		"format": "aion-m5d-registration-order",
		"version": 1,
		"registry": "xml",
		"xmlQuests": len(lists["order"]),
		"order": lists["order"],
		"orderFlags": _order_flags(lists["order"], position),
		"questOnEnterWorld": lists["questOnEnterWorld"],
		"questOnEnterWorldFlags": _order_flags(lists["questOnEnterWorld"], position),
		"questOnLevelUp": {race: ids for race, ids in sorted(lists["questOnLevelUp"].items())},
		**npcs,
		"talkLists": _list_summary(lists["onTalkEvent"], position, "neitherAscendingNorDocumentOrder"),
		"killLists": _list_summary(lists["onKillEvent"], position, "neitherAscendingNorDocumentOrder"),
		"startLists": _list_summary(lists["onQuestStart"], position, "neitherAscendingNorDocumentNorInsertionOrder", lists["onQuestStartInsertion"]),
		"notModelled": {
			"javaHandlers": "the Java server registers the data/handlers/quest handlers before the XML quests, in the order ScriptManager "
			                "loads them: their quests precede these lists there (m5d-plan.md D9: the C++ registry has the XML quests only)",
		},
	}


# --- the census ---------------------------------------------------------------------------------------------------------------------------


def _by_count(values) -> dict:
	counts: dict[str, int] = {}
	for value in values:
		counts[str(value)] = counts.get(str(value), 0) + 1
	return dict(sorted(counts.items(), key=lambda kv: (-kv[1], kv[0])))


def _instance_maps(world: QuestWorld) -> dict[int, bool]:
	"""world_maps: map id -> WorldMapTemplate.isInstance."""
	return {java_int(e.get("id"), "map id"): java_boolean(e.get("instance")) for e in world.data.children("world_maps", "map")}


def spawn_index(world: QuestWorld) -> tuple[dict[int, set[str]], dict[tuple[int, str], set[str | None]]]:
	"""(npc -> the kinds of its spawns: "startup", a SERVICE_KINDS or an INERT_KINDS name, (npc, kind) -> the ai names of those spawns' spots:
	the spot's ai when it has one, else the npc template's)."""
	instance = _instance_maps(world)
	templates = world.npc_index()[0]
	kinds: dict[int, set[str]] = {}
	ais: dict[tuple[int, str], set[str | None]] = {}

	def add(spawn, kind: str) -> None:
		npc = java_int(spawn.get("npc_id"), "spawn npc_id")
		kinds.setdefault(npc, set()).add(kind)
		template_ai = templates.get(npc, {}).get("ai")
		for spot in spawn.findall("spot"):
			ai = spot.get("ai")
			ais.setdefault((npc, kind), set()).add(template_ai if ai is None else (None if ai == NO_AI else ai))

	for spawn_map in world.spawn_maps():
		map_id = java_int(spawn_map.get("map_id"), "spawn_map map_id")
		for child in spawn_map:
			if child.tag != "spawn":
				kind = NESTED_SPAWNS.get(child.tag)
				for spawn in child.iter("spawn") if kind is not None else ():
					add(spawn, kind)
				continue
			npc = java_int(child.get("npc_id"), "spawn npc_id")
			handler = child.get("handler")
			if map_id not in instance:
				kind = "noWorldMap"
			elif instance[map_id]:
				kind = "instance"
			elif java_int(child.get("difficult_id"), f"spawn {npc} difficult_id", 0) != 0:
				kind = "difficulty"
			elif handler == "RIFT":
				kind = "rift"
			elif handler == "STATIC":
				kind = "static"
			elif handler is not None:
				kind = "handler"
			elif npc not in templates or is_gatherable(npc):
				kind = "notAnNpc"
			else:
				kind = "startup" if child.find("spot") is not None else "noSpots"  # spawnInstance spawns one object per spot
			add(child, kind)
	return kinds, ais


def blocked_by_java(world: QuestWorld, quest_id: int) -> bool:
	"""Fewer <start_conditions> groups can pass than the template requires, a group failing when it names a Java-handled quest under
	<finished> or <acquired>."""
	t = world.template(quest_id)
	passing = 0
	for group in t["startConds"] or []:
		names = [f["questId"] for f in group["finished"] or []] + list(group["acquired"] or [])
		if not any(q in world.java for q in names):
			passing += 1
	return passing < required_condition_count(world, t)


def reward_bodies(t) -> dict[str, bool]:
	"""E-09's reward bodies a template reaches: bonus, AP, GP, cube and warehouse extension."""
	blocks = list(t["rewards"] or []) + ([t["extendedRewards"]] if t["extendedRewards"] is not None else [])
	flags = {"bonus": t["bonus"] is not None, "ap": any(b["abyssPoints"] != 0 for b in blocks), "gp": any(b["gloryPoints"] != 0 for b in blocks),
	         "cube": any(b["extendInventory"] == 1 for b in blocks), "warehouse": any(b["extendInventory"] == 2 for b in blocks)}
	flags["any"] = any(flags.values())
	return flags


def _reward_counts(world: QuestWorld, quest_ids) -> dict[str, int]:
	counts = {"bonus": 0, "ap": 0, "gp": 0, "cube": 0, "warehouse": 0, "any": 0}
	for quest_id in quest_ids:
		for key, value in reward_bodies(world.template(quest_id)).items():
			counts[key] += 1 if value else 0
	return counts


def start_rows(world: QuestWorld, kinds: dict[int, set[str]], ais: dict[tuple[int, str], set[str | None]]) \
		-> tuple[dict[int, str], dict[int, str], dict[int, list[str]]]:
	"""(quest -> its §2.4 start row, serviceSpawned quest -> its spawn kind, otherAi quest -> the ais), over spawn_index's answer."""
	rows: dict[int, str] = {}
	service: dict[int, str] = {}
	other_ais: dict[int, list[str]] = {}
	for quest_id, reg in world.registrations().items():
		t = world.template(quest_id)
		if t["minlevelPermitted"] == DISABLED_LEVEL:
			rows[quest_id] = "disabled"
			continue
		if not reg.quest_start:
			rows[quest_id] = "noStartNpc"
			continue
		startup = [npc for npc in reg.quest_start if "startup" in kinds.get(npc, ())]
		if startup:
			names = set().union(*(ais[(npc, "startup")] for npc in startup))
			if names & set(TALKING_AIS):
				row = "talkingAi"
			elif names & set(D5_AIS):
				row = "abyssGuardAi"
			else:
				rows[quest_id] = "otherAi"
				other_ais[quest_id] = sorted(str(n) for n in names)
				continue
			rows[quest_id] = row + "BlockedByJava" if blocked_by_java(world, quest_id) else row
			continue
		spawned = set().union(*(kinds.get(npc, set()) for npc in reg.quest_start)) & set(SERVICE_KINDS)
		if spawned:
			rows[quest_id] = "serviceSpawned"
			service[quest_id] = next(kind for kind in SERVICE_KINDS if kind in spawned)
			continue
		rows[quest_id] = "neverSpawned"
	return rows, service, other_ais


def completion_row(world: QuestWorld, quest_id: int) -> str:
	tag = world.xml[quest_id].tag
	if tag == "item_collecting":
		drops = world.template(quest_id)["questDrop"] or []
		if any(d["npcId"] // 100000 == QUEST_OBJECT_PREFIX for d in drops):
			return "questObjects"
		return "questLoot" if drops else "itemsFromElsewhere"
	return COMPLETION_KINDS.get(tag, "other")


def _event_npcs(world: QuestWorld) -> set[int]:
	npcs = set()
	for event in world.data.children("timed_events", "event"):
		for spawn in event.iter("spawn"):
			if spawn.get("npc_id") is not None:
				npcs.add(java_int(spawn.get("npc_id"), "event spawn npc_id"))
	return npcs


def _town_npcs(world: QuestWorld) -> tuple[set[int], set[int]]:
	"""(every town spawn's npc, the npcs of level 1 town spawns)."""
	every, level1 = set(), set()
	for spawn_map in world.data.children("town_spawns_data", "spawn_map"):
		for town_level in spawn_map.iter("town_level"):
			for spawn in town_level.iter("spawn"):
				if spawn.get("npc_id") is None:
					continue
				npc = java_int(spawn.get("npc_id"), "town spawn npc_id")
				every.add(npc)
				if java_int(town_level.get("level"), "town_level level") == 1:
					level1.add(npc)
	return every, level1


def _house_npcs(world: QuestWorld) -> set[int]:
	"""The npcs the houses spawn at startup: per address of an open-world map (spawnAll spawns only those) whose land's default building
	(the first with default="true", else the first, HousingLand.getDefaultBuilding; its type from the buildings holder when the land's
	<building> has none, Building.getType) is not a studio, the land's npc of each house_npcs spawn type of the address."""
	instance = _instance_maps(world)
	types = {java_int(b.get("id"), "building id"): b.get("type") for b in world.data.children("buildings", "building")}
	spawn_types: dict[int, list[str]] = {}
	for house in world.data.children("house_npcs", "house"):
		spawn_types.setdefault(java_int(house.get("address"), "house address"), []).extend(s.get("type") for s in house.findall("spawn"))
	npcs = set()
	for land in world.data.children("house_lands", "land"):
		buildings = land.findall("buildings/building")
		if not buildings:
			raise OracleError(f"house land {land.get('id')} has no <building> (HousingLand.getDefaultBuilding throws)")
		default = next((b for b in buildings if java_boolean(b.get("default"))), buildings[0])
		building_type = default.get("type") or types.get(java_int(default.get("id"), "building id"))
		if building_type == STUDIO_BUILDING:
			continue
		for address in land.findall("addresses/address"):
			if instance.get(java_int(address.get("map"), "house address map"), True):  # an instance map, or none: spawnAll skips it
				continue
			for spawn_type in spawn_types.get(java_int(address.get("id"), "house address id"), []):
				npcs.update(java_int(land.get(attr), f"house land {attr}") for attr in HOUSE_SPAWN_NPCS.get(spawn_type, ()))
	return npcs


def java_named_npcs(world: QuestWorld, npcs: set[int]) -> dict[int, set[str]]:
	"""npc -> where Java text writes its id as an int literal: the name of a directory of data/handlers (the parent of the quest handlers:
	instance, ai, quest, ...) or "src" (game-server/src). A text scan: a name, not a proof of a spawn."""
	if not npcs:
		return {}
	roots = []
	handlers = world.handlers_dir.parent
	if handlers.is_dir():
		roots += [(sub.name, sub) for sub in sorted(handlers.iterdir()) if sub.is_dir()]
	roots.append(("src", world.java_src))
	found: dict[int, set[str]] = {}
	for area, root in roots:
		for path in sorted(root.rglob("*.java")):
			for literal in JAVA_INT_LITERAL.findall(path.read_text(encoding="utf-8", errors="replace")):
				if int(literal) in npcs:
					found.setdefault(int(literal), set()).add(area)
	return found


def census_report(world: QuestWorld) -> dict:
	"""m5d-quests --census (aion-m5d-census)."""
	regs = world.registrations()
	kinds, ais = spawn_index(world)
	rows, service, other_ais = start_rows(world, kinds, ais)
	by_row = {row: sorted(q for q, r in rows.items() if r == row) for row in START_ROWS}
	unknown = set(rows.values()) - set(START_ROWS)
	if unknown:
		raise OracleError(f"census rows {sorted(unknown)} are not in START_ROWS")
	reachable = sorted(q for q, r in rows.items() if r in REACHABLE_ROWS)
	otherwise = []
	for quest_id in by_row["serviceSpawned"]:
		names = set().union(*(ais.get((npc, kind), set()) for npc in regs[quest_id].quest_start for kind in SERVICE_KINDS))
		if names & set(TALKING_AIS + D5_AIS) and not blocked_by_java(world, quest_id):
			otherwise.append(quest_id)
	town, town_level1 = _town_npcs(world)
	events = _event_npcs(world)

	def start_kinds(quest_id: int) -> set[str]:
		return set().union(*(kinds.get(npc, set()) for npc in regs[quest_id].quest_start))

	houses = _house_npcs(world)
	never_sub: dict[str, list[int]] = {}
	for quest_id in by_row["neverSpawned"]:
		npcs = regs[quest_id].quest_start
		if any(npc in town for npc in npcs):
			sub = "townSpawns"
		elif any(npc in houses for npc in npcs):
			sub = "houseSpawns"
		elif any(npc in events for npc in npcs):
			sub = "eventSpawns"
		elif start_kinds(quest_id):
			sub = "inertSpawns"
		else:
			sub = "noSpawnData"
		never_sub.setdefault(sub, []).append(quest_id)
	never_town, never_house, never_event, never_inert, no_data = (never_sub.get(sub, []) for sub in NEVER_SPAWNED_ROWS)
	named = java_named_npcs(world, {npc for q in no_data for npc in regs[q].quest_start})

	def named_in(quest_id: int) -> set[str]:
		return set().union(*(named.get(npc, set()) for npc in regs[quest_id].quest_start))

	quest_area = world.handlers_dir.name
	outside_quest = [q for q in no_data if named_in(q) - {quest_area}]
	quest_only = [q for q in no_data if named_in(q) == {quest_area}]
	completion = {row: [q for q in reachable if completion_row(world, q) == row] for row in COMPLETION_ROWS}
	templates = world.template_ids
	neither = [q for q in templates if q not in world.xml and q not in world.java]
	challenge = [q for q in regs if world.template(q)["category"] == "CHALLENGE_TASK"]
	zones = {}
	for zone in START_ZONES:
		ids = sorted(q for q in templates if world.template(q).element.get("quest_zone") == zone)
		zones[zone] = {"total": len(ids), **{registry: [q for q in ids if world.registry_of(q) == registry] for registry in ("xml", "java", "none")}}
	start = {}
	for row in START_ROWS:
		entry = {"count": len(by_row[row]), "quests": by_row[row]}
		if row == "serviceSpawned":
			entry["bySpawn"] = {kind: sum(1 for q in by_row[row] if service[q] == kind) for kind in SERVICE_KINDS
			                    if any(service[q] == kind for q in by_row[row])}
			entry["inSeveralKinds"] = [q for q in by_row[row] if len(start_kinds(q) & set(SERVICE_KINDS)) > 1]
		if row == "otherAi":
			entry["ais"] = {str(q): other_ais[q] for q in by_row[row]}
		if row == "neverSpawned":
			entry["townSpawns"] = {"count": len(never_town), "quests": never_town,
			                       "atTownLevel1": sum(1 for q in never_town if any(n in town_level1 for n in regs[q].quest_start))}
			entry["houseSpawns"] = {"count": len(never_house), "quests": never_house}
			entry["eventSpawns"] = {"count": len(never_event), "quests": never_event}
			entry["inertSpawns"] = {"count": len(never_inert), "quests": never_inert,
			                        "kinds": {str(q): [k for k in INERT_KINDS if k in start_kinds(q)] for q in never_inert}}
			entry["noSpawnData"] = {"count": len(no_data), "quests": no_data, "namedInJava": {
				"outsideQuestHandlers": {"count": len(outside_quest), "quests": outside_quest,
				                         "where": {str(q): sorted(named_in(q)) for q in outside_quest}},
				"questHandlersOnly": {"count": len(quest_only), "quests": quest_only},
				"nowhere": {"count": len(no_data) - len(outside_quest) - len(quest_only), "quests": [q for q in no_data if not named_in(q)]}}}
		start[row] = entry
	return {
		"format": "aion-m5d-census",
		"version": 1,
		"registry": world.census(),
		"templates": {"total": len(templates), "byCategory": _by_count(world.template(q)["category"] for q in templates)},
		"neither": {"total": len(neither), "byCategory": _by_count(world.template(q)["category"] for q in neither)},
		"xmlRestricted": sum(1 for q in regs if world.template(q)["restricted"]),
		"start": start,
		"startTotal": sum(entry["count"] for entry in start.values()),
		"reachable": {"count": len(reachable), "rows": list(REACHABLE_ROWS), "byKind": _by_count(world.xml[q].tag for q in reachable)},
		"serviceOtherwiseReachable": {"count": len(otherwise), "bySpawn": _by_count(service[q] for q in otherwise), "quests": otherwise},
		"completion": {row: {"count": len(ids), "byKind": _by_count(world.xml[q].tag for q in ids), "quests": ids} for row, ids in completion.items()},
		"dialogsAndKillsRewards": _reward_counts(world, completion["dialogsAndKills"]),
		"rewards": {"xml": _reward_counts(world, regs), "reachable": _reward_counts(world, reachable)},
		"challengeTasks": {"xml": len(challenge), "byRow": _by_count(rows[q] for q in challenge),
		                   "reachable": sum(1 for q in challenge if rows[q] in REACHABLE_ROWS)},
		"startZones": zones,
		"ais": {"talking": list(TALKING_AIS), "d5": list(D5_AIS)},
	}
