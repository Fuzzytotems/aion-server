"""m5j-items: what the M5j stage-2 gate (m5j-plan.md §10.4 Z9-Z11, §18.3 CP5 H-21) asserts about riding, kisks and toy pets, as the Java
server computes it from the static data.

Java rules, each with the method the value comes from (game-server/src/com/aionemu/gameserver):
- an item a character may use (PlayerRestrictions.canUseItem, PlayerRestrictions.java:311-367, the checks that depend on the template): the
  item race PC_ALL or the character's; no <uselimits gender>; ItemTemplate.isClassSpecific(class) (restrict[ordinal] > 0, or the starting
  class's for an advanced class); getRequiredLevel(class) (restrict[ordinal], -1 for 0) not above the level; getMaxLevelRestrict (restrict_max
  [ordinal], 0 without it) 0 or not below the level; no use area; no activation race. A missing restrict is SpaceSeparatedBytesAdapter's
  absence: every class at level 1 (m5c/economy.py's reading);
- the ride item (`--class`, `--race`): of the items with a <ride npc_id> action the character may use, the lowest required level, then the
  lowest id; RideAction.act's casting delay (`casting_delay`, 0 finishes at once) and its RideInfo (ride.xml's ride_info id = npc_id);
  finishUse broadcasts SM_EMOTION(CHANGE_SPEED, 0, 0), SM_EMOTION(RIDE, 0, npcId) and SM_ITEM_USAGE_ANIMATION(..., 0, 1, 1) (RideAction.java:
  152-155); the second use is PlayerActions.unsetPlayerMode(RIDE): SM_EMOTION(CHANGE_SPEED) and SM_EMOTION(RIDE_END) (PlayerActions.java:55-56).
  canAct's zone arm (CustomConfig.ENABLE_RIDE_RESTRICTION, `gameserver.ride.restriction.enable`) asks every zone of the spot ZoneInstance.canRide
  (below); the gate switches the restriction off, and the oracle reports the key (and refuses a profile that leaves it on, since the spot's
  zones are not modelled);
- the kisk item: of the items with a <toypetspawn npcid> action the character may use whose npc has <kisk_stats> with resurrects > 0 (a kisk
  with no resurrection is not Kisk.isActive, Kisk.java:211-213, and offers no kisk revive) and members > 1 (ToyPetSpawnAction.finishUse
  opens the bind dialog, KiskAI's question, instead of binding at once: ToyPetSpawnAction.java:110-113), the lowest required level, then the
  lowest id. KiskStatsTemplate's defaults: usemask 4, members 6, resurrects 18 (KiskStatsTemplate.java:16-23); every use mask lets the creator
  bind (Kisk.isUseAllowed). The kisk lives KISK_LIFETIME_IN_SEC = 2 h (Kisk.java:31);
- whether a kisk may be put on the map (`--map`): ToyPetSpawnAction.isPutKiskZone asks every zone the player is in ZoneInstance.canPutKisk:
  a zone without flags (-1) or with 0 answers the map's (WorldMap.canPutKisk: the map's BIND flag; no override at start), else its own BIND
  bit (ZoneAttributes.BIND = 1 << 0); the oracle reads every <zone mapid> of the zone files and answers `everywhere` when all of them allow
  (then any spot of the map does, inside a zone or outside all) - a map with a refusing zone is refused (the spot geometry is not modelled);
- the pet egg: of the items with an <adoptpet petId> action without `minutes` (no expiry) whose pet has the FOOD function, of a flavour that
  pet_feed.xml has (PetCommonData's constructor dereferences PET_FEED_DATA.getFlavourById(id): the NCSoft test flavours 1-6 are not in the
  file, so adopting such a pet throws NullPointerException after the egg was consumed), and at most two of
  the specialties SM_PET.writePetData writes (WAREHOUSE, LOOT, DOPING, FOOD), the lowest id (PetAdoptionService.validateAdoption needs the
  action's petId to be the adopted one and the pet template to exist); SM_PET's written specialties in writePetData's order (WAREHOUSE, LOOT,
  DOPING, FOOD) with their ids (PetFunctionType); NameConfig.PET_NAME_PATTERN's default `[a-zA-Z]{2,16}` (`gameserver.name.pet_pattern`, not
  read from the profile: a Pattern key) for the name the gate gives;
- the EmotionType ids of CHANGE_SPEED, RIDE and RIDE_END, the PetFunctionType ids, message and question ids by name (m5j/social.message_ids).
"""

from __future__ import annotations

from pathlib import Path

from staticdata_oracle import OracleError

from m5a.creation import JavaEnums, enum_constants
from m5a.data import StaticData, java_int
from m5c.trade_config import load_config
from m5j.social import _read, _squeeze, message_ids

ITEM_KEYS = {
	"gameserver.ride.restriction.enable": ("CustomConfig", "ENABLE_RIDE_RESTRICTION", "boolean"),
	"gameserver.kisk.restriction.enable": ("CustomConfig", "ENABLE_KISK_RESTRICTION", "boolean"),
	"gameserver.periodicsave.player.pets": ("PeriodicSaveConfig", "PLAYER_PETS", "int"),
}

# statements the rules above were written against (whitespace-insensitive): a change in the Java source fails the oracle
STATEMENTS = (
	("model/templates/item/actions/RideAction.java", "PacketSendUtility.broadcastPacket(player, new SM_EMOTION(player, EmotionType.CHANGE_SPEED, 0, 0), true);"),
	("model/templates/item/actions/RideAction.java", "PacketSendUtility.broadcastPacket(player, new SM_EMOTION(player, EmotionType.RIDE, 0, getRideInfo().getNpcId()), true);"),
	("model/templates/item/actions/RideAction.java", "new SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem.getObjectId(), parentItem.getItemId(), 0, 1, 1), true);"),
	("model/templates/item/actions/RideAction.java", "if (CustomConfig.ENABLE_RIDE_RESTRICTION) { for (ZoneInstance zone : player.findZones()) { if (!zone.canRide()) {"),
	("model/actions/PlayerActions.java", "PacketSendUtility.broadcastPacket(player, new SM_EMOTION(player, EmotionType.RIDE_END), true);"),
	("model/templates/item/actions/ToyPetSpawnAction.java", "if (kisk.getMaxMembers() > 1) kisk.getController().onDialogRequest(player); else KiskService.getInstance().onBind(kisk, player);"),
	("model/templates/item/actions/ToyPetSpawnAction.java", "if (!zone.canPutKisk()) return false;"),
	("world/zone/ZoneInstance.java", "public boolean canPutKisk() { if (template.getZoneTemplate().getFlags() == -1 || template.getZoneTemplate().getFlags() == 0 "
	                                 "|| World.getInstance().getWorldMap(mapId).hasOverridenOption(ZoneAttributes.BIND)) return World.getInstance()"
	                                 ".getWorldMap(mapId).canPutKisk(); return (template.getZoneTemplate().getFlags() & ZoneAttributes.BIND.getId()) != 0; }"),
	("model/gameobjects/Kisk.java", "private static final long KISK_LIFETIME_IN_SEC = TimeUnit.HOURS.toSeconds(2);"),
	("model/gameobjects/Kisk.java", "return !isDead() && getRemainingResurrects() > 0;"),
	("model/templates/stats/KiskStatsTemplate.java", "@XmlAttribute(name = \"usemask\") private int useMask = 4;"),
	("model/templates/stats/KiskStatsTemplate.java", "@XmlAttribute(name = \"members\") private int maxMembers = 6;"),
	("model/templates/stats/KiskStatsTemplate.java", "@XmlAttribute(name = \"resurrects\") private int maxResurrects = 18;"),
	("services/toypet/PetAdoptionService.java", "|| template.getActions().getAdoptPetAction().getPetId() != petId) {"),
	("configs/main/NameConfig.java", "@Property(key = \"gameserver.name.pet_pattern\", defaultValue = \"[a-zA-Z]{2,16}\")"),
	("restrictions/PlayerRestrictions.java", "if (item.getItemTemplate().getRace() != Race.PC_ALL && item.getItemTemplate().getRace() != player.getRace()) {"),
	("restrictions/PlayerRestrictions.java", "int requiredLevel = item.getItemTemplate().getRequiredLevel(player.getPlayerClass()); if (requiredLevel > player.getLevel()) {"),
)

# SM_PET.writePetData's specialties in its order (SM_PET.java:283-309)
WRITTEN_SPECIALTIES = ("WAREHOUSE", "LOOT", "DOPING", "FOOD")
ZONE_BIND = 1 << 0  # ZoneAttributes.BIND
ZONE_RIDE = 1 << 4  # ZoneAttributes.RIDE


def check_statements(java_src: Path) -> None:
	base = Path(java_src) / "com" / "aionemu" / "gameserver"
	cache: dict[str, str] = {}
	for relative, statement in STATEMENTS:
		if relative not in cache:
			cache[relative] = _squeeze(_read(base / relative))
		if _squeeze(statement) not in cache[relative]:
			raise OracleError(f"{relative} no longer contains `{statement}`: the Java source does not have the shape this oracle was written against")


def _bytes(text: str | None, what: str) -> tuple[int, ...] | None:
	if text is None:
		return None
	values = tuple(java_int(t, what) for t in text.split())
	for value in values:
		if not -128 <= value <= 127:
			raise OracleError(f"{what}: {value} is not a Java byte")
	return values


def race_activation_targets(java_src: Path) -> set[str]:
	"""ItemActivationTarget's constants constructed with a Race (ItemActivationTarget.java): getActivationRace() is not null for them"""
	source = Path(java_src) / "com" / "aionemu" / "gameserver" / "model" / "templates" / "item" / "ItemActivationTarget.java"
	return {name for name, args in enum_constants(source, "ItemActivationTarget") if args and args.strip().startswith("Race.")}


def usable(element, enums: JavaEnums, player_class: str, race: str, level: int, race_targets: set[str] = frozenset()) -> int | None:
	"""PlayerRestrictions.canUseItem's template checks: the required level for the class when the item passes them at `level`, else None"""
	item_id = java_int(element.get("id"), "item_template id")
	what = f"item_template {item_id}"
	classes = list(enums.classes)
	ordinal = classes.index(player_class)
	restrict = _bytes(element.get("restrict"), f"{what} restrict") or (1,) * len(classes)
	restrict_max = _bytes(element.get("restrict_max"), f"{what} restrict_max")
	if ordinal >= len(restrict):
		raise OracleError(f"{what}: restrict has {len(restrict)} values, PlayerClass.{player_class} is ordinal {ordinal}")
	if element.get("race", "PC_ALL") not in ("PC_ALL", race) or element.get("activate_target") in race_targets:
		return None
	limits = element.find("uselimits")
	if limits is not None and (limits.get("gender") is not None or limits.get("usearea") is not None):
		return None
	related = restrict[ordinal] > 0
	starting = enums.starting_classes[player_class]
	if not related and starting != player_class:
		related = restrict[classes.index(starting)] > 0
	if not related:
		return None
	required = restrict[ordinal] if restrict[ordinal] != 0 else -1
	if required > level:
		return None
	max_level = restrict_max[ordinal] if restrict_max is not None else 0
	if max_level != 0 and level > max_level:
		return None
	return required


def _action(element, tag: str):
	actions = element.find("actions")
	return None if actions is None else actions.find(tag)


def ride_block(data: StaticData, enums: JavaEnums, player_class: str, race: str, level: int, race_targets: set[str]) -> dict:
	# StaticData.stream clears each element after it was yielded: read what is kept now
	rides = {java_int(e.get("id"), "ride_info id"): {"type": java_int(e.get("type"), "ride type", 0), "moveSpeed": float(e.get("move_speed", "0")),
	                                                   "flySpeed": float(e.get("fly_speed", "0"))} for e in data.stream("rides", "ride_info")}
	best = None
	for element in data.stream("item_templates", "item_template"):
		action = _action(element, "ride")
		if action is None:
			continue
		required = usable(element, enums, player_class, race, level, race_targets)
		if required is None:
			continue
		item_id = java_int(element.get("id"), "item_template id")
		npc_id = java_int(action.get("npc_id"), f"item {item_id} ride npc_id")
		if npc_id not in rides:
			continue  # RideAction.getRideInfo() would be null (a NullPointerException in finishUse)
		key = (required, item_id)
		if best is None or key < best[0]:
			best = (key, java_int(element.get("casting_delay"), f"item {item_id} casting_delay", 0), java_int(element.get("desc"), f"item {item_id} desc", 0), npc_id)
	if best is None:
		raise OracleError(f"no ride item a level-{level} {race} {player_class} may use")
	(required, item_id), casting_delay, name_id, npc_id = best
	return {"itemId": item_id, "requiredLevel": required, "castingDelay": casting_delay, "nameId": name_id, "npcId": npc_id, "rideInfo": rides[npc_id]}


def kisk_block(data: StaticData, enums: JavaEnums, player_class: str, race: str, level: int, race_targets: set[str]) -> dict:
	kisks = {}
	for npc in data.stream("npc_templates", "npc_template"):
		stats = npc.find("kisk_stats")
		if stats is not None:
			npc_id = java_int(npc.get("npc_id"), "npc_template npc_id")
			kisks[npc_id] = {"npcId": npc_id, "nameId": java_int(npc.get("name_id"), f"npc {npc_id} name_id", 0), "ai": npc.get("ai"),
			                 "useMask": java_int(stats.get("usemask"), f"npc {npc_id} usemask", 4),
			                 "maxMembers": java_int(stats.get("members"), f"npc {npc_id} members", 6),
			                 "maxResurrects": java_int(stats.get("resurrects"), f"npc {npc_id} resurrects", 18)}
	best = None
	for element in data.stream("item_templates", "item_template"):
		action = _action(element, "toypetspawn")
		if action is None:
			continue
		item_id = java_int(element.get("id"), "item_template id")
		kisk = kisks.get(java_int(action.get("npcid"), f"item {item_id} toypetspawn npcid"))
		if kisk is None or kisk["maxResurrects"] <= 0 or kisk["maxMembers"] <= 1:
			continue
		required = usable(element, enums, player_class, race, level, race_targets)
		if required is None:
			continue
		key = (required, item_id)
		if best is None or key < best[0]:
			best = (key, java_int(element.get("casting_delay"), f"item {item_id} casting_delay", 0), java_int(element.get("desc"), f"item {item_id} desc", 0), kisk)
	if best is None:
		raise OracleError(f"no kisk item with resurrections and a bind dialog that a level-{level} {race} {player_class} may use")
	(required, item_id), casting_delay, name_id, kisk = best
	return {"itemId": item_id, "requiredLevel": required, "castingDelay": casting_delay, "nameId": name_id, "lifetimeSeconds": 2 * 60 * 60, **kisk}


def map_zones(data: StaticData, map_id: int) -> dict:
	"""ZoneInstance.canPutKisk and canRide of every zone of the map, the map's own options"""
	world_map = next((m for m in data.stream("world_maps", "map") if java_int(m.get("id"), "map id") == map_id), None)
	if world_map is None:
		raise OracleError(f"map {map_id} is not in world_maps.xml")
	map_flags = (world_map.get("flags") or "").split()
	map_bind, map_ride = "BIND" in map_flags, "RIDE" in map_flags
	zones = []
	for zone in data.stream("zones", "zone"):
		if java_int(zone.get("mapid"), "zone mapid") != map_id:
			continue
		flags = java_int(zone.get("flags"), f"zone {zone.get('name')} flags", -1)
		own = flags not in (-1, 0)
		zones.append({"name": zone.get("name"), "flags": flags, "canPutKisk": bool(flags & ZONE_BIND) if own else map_bind,
		              "canRide": bool(flags & ZONE_RIDE) if own else map_ride})
	return {"mapId": map_id, "mapFlags": map_flags, "mapCanPutKisk": map_bind, "mapCanRide": map_ride, "zones": zones,
	        "kiskEverywhere": map_bind and all(z["canPutKisk"] for z in zones), "rideEverywhere": map_ride and all(z["canRide"] for z in zones)}


def pet_block(data: StaticData) -> dict:
	# StaticData.stream clears each element after it was yielded: read what is kept now
	pets = {java_int(p.get("id"), "pet id"): (java_int(p.get("nameid"), "pet nameid", 0),
	                                          [(f.get("type"), java_int(f.get("id"), "petfunction id", 0)) for f in p.findall("petfunction")])
	        for p in data.stream("pets", "pet")}
	# PetCommonData's constructor (PetCommonData.java:50-54) dereferences PET_FEED_DATA.getFlavourById(the FOOD function's id): a flavour
	# pet_feed.xml does not have ("Flavours with id 1-6 are NCSoft tests, not included") is a NullPointerException after the egg was consumed
	flavours = {java_int(f.get("id"), "flavour id"): {"id": java_int(f.get("id"), "flavour id"),
	                                                  "fullCount": java_int(f.get("full_count"), "flavour full_count", 1),
	                                                  "lovedLimit": java_int(f.get("loved_limit"), "flavour loved_limit", 0),
	                                                  "cooldown": java_int(f.get("cd"), "flavour cd", 0),
	                                                  "foodGroups": [g.get("group") for g in f.findall("food")]}
	            for f in data.stream("pet_feed", "flavour")}
	for element in data.stream("item_templates", "item_template"):
		action = _action(element, "adoptpet")
		if action is None or action.get("minutes") is not None:
			continue
		item_id = java_int(element.get("id"), "item_template id")
		pet_id = java_int(action.get("petId"), f"item {item_id} adoptpet petId")
		pet = pets.get(pet_id)
		if pet is None:
			continue
		name_id, functions = pet
		types = [t for t, _ in functions]
		written = [t for t in WRITTEN_SPECIALTIES if t in types]
		food = next(i for t, i in functions if t == "FOOD") if "FOOD" in types else None
		if food not in flavours or len(written) > 2:
			continue
		return {"eggItemId": item_id, "petId": pet_id, "nameId": name_id,
		        "functions": types, "writtenSpecialties": written, "expires": False, "namePattern": "[a-zA-Z]{2,16}",
		        "flavour": flavours[food]}
	raise OracleError("no non-expiring pet egg whose pet has the FOOD function of a pet_feed.xml flavour and at most two written specialties")


def items_report(data: StaticData, java_src: Path, config_dir: Path | None, profile: Path | None, overrides: list[str], player_class: str,
                 race: str, level: int, map_id: int, messages: list[str], questions: list[str], require_profile: bool = False) -> dict:
	check_statements(java_src)
	config = load_config(java_src, config_dir, profile, overrides, keys=ITEM_KEYS, require_profile=require_profile)
	enums = JavaEnums(java_src)
	if player_class not in enums.classes:
		raise OracleError(f"unknown PlayerClass {player_class}")
	if race not in ("ELYOS", "ASMODIANS"):
		raise OracleError(f"--race {race}: ELYOS or ASMODIANS")
	base = Path(java_src) / "com" / "aionemu" / "gameserver"
	emotions = {name: java_int(args, f"EmotionType.{name}") for name, args in enum_constants(base / "model" / "EmotionType.java", "EmotionType")
	            if name in ("CHANGE_SPEED", "RIDE", "RIDE_END")}
	functions = {}
	for name, args in enum_constants(base / "model" / "templates" / "pet" / "PetFunctionType.java", "PetFunctionType"):
		functions[name] = java_int((args or "").split(",")[0].strip(), f"PetFunctionType.{name}")
	targets = race_activation_targets(java_src)
	if not targets:
		raise OracleError("ItemActivationTarget: no constant with a race")
	zones = map_zones(data, map_id)
	if config["gameserver.ride.restriction.enable"].value and not zones["rideEverywhere"]:
		raise OracleError(f"gameserver.ride.restriction.enable is on and map {map_id} has zones without RIDE: the spot's zones are not modelled")
	if not zones["kiskEverywhere"]:
		raise OracleError(f"map {map_id} has zones that refuse a kisk: the spot's zones are not modelled")
	found_messages, found_questions = message_ids(java_src, messages, questions)
	pet = pet_block(data)
	pet["writtenSpecialtyIds"] = [functions[t] for t in pet["writtenSpecialties"]]
	return {
		"format": "aion-m5j-items",
		"version": 1,
		"character": {"class": player_class, "race": race, "level": level},
		"config": {key: value.as_json() for key, value in config.items()},
		"emotions": emotions,
		"petFunctionIds": functions,
		"messages": found_messages,
		"questions": found_questions,
		"ride": ride_block(data, enums, player_class, race, level, targets),
		"kisk": kisk_block(data, enums, player_class, race, level, targets),
		"map": zones,
		"pet": pet,
	}
