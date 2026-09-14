"""V2 count oracle: reproduces the 90 "Loaded N ..." lines (92 numbers) of StaticData.afterUnmarshal (StaticData.java:313-405).

Each holder rule below restates what the Java holder's size() counts after its afterUnmarshal hook ran, written from the holder source
(dataholders/*.java and the template classes named in the comments). Keys use the XML attribute names of the bound template fields.
Where Java would crash at startup (a list field iterated without a null check, a duplicate that throws, a missing single element), the
rule raises OracleError instead of guessing a number.

JAXB list semantics used throughout: a List property is null when no matching child element exists, and one list collects all matching
children of the merged holder (all files of a singleRootTag directory). A single-valued element property keeps the last occurrence.
"""

from __future__ import annotations

from collections import Counter

from . import OracleError
from . import enums
from .values import (attr_bool, attr_byte, attr_enum, attr_float, attr_int, attr_int_list, attr_str, wrap_int32)


def _npe(holder, what):
	raise OracleError(f"<{holder.tag}> ({holder.java}): {what} would throw NullPointerException at startup")


def _children(element, tag):
	return [c for c in element if c.tag == tag]


class Holder:
	java = "?"

	def __init__(self, tag):
		self.tag = tag
		self.child_counts = Counter()

	def accept(self, element):
		self.child_counts[element.tag] += 1
		self.child(element)

	def child(self, element):
		pass

	def finish(self):
		pass

	def require(self, *tags):
		if not any(self.child_counts[t] for t in tags):
			_npe(self, f"no <{'|'.join(tags)}> element: iterating the null list")


class Distinct(Holder):
	"""size() == number of distinct keys over the list elements (HashMap put, last or first wins: same size)."""

	def __init__(self, tag, java, tags, key, required=True):
		super().__init__(tag)
		self.java, self.tags, self.key, self.required = java, tuple(tags), key, required
		self.keys = set()

	def child(self, element):
		if element.tag in self.tags:
			self.keys.add(self.key(element))

	def finish(self):
		if self.required:
			self.require(*self.tags)

	def size(self):
		return len(self.keys)


class Count(Holder):
	"""size() == list size."""

	def __init__(self, tag, java, tags, required=True):
		super().__init__(tag)
		self.java, self.tags, self.required = java, tuple(tags), required
		self.n = 0

	def child(self, element):
		if element.tag in self.tags:
			self.n += 1

	def finish(self):
		if self.required:
			self.require(*self.tags)

	def size(self):
		return self.n


def distinct(java, tags, key, required=True):
	return lambda tag: Distinct(tag, java, tags, key, required)


def count(java, tags, required=True):
	return lambda tag: Count(tag, java, tags, required)


def int_key(name, default=0):
	return lambda e: attr_int(e, name, default)


def str_key(name):
	return lambda e: attr_str(e, name)


# ---------------------------------------------------------------------------------------------------------------------------------
# Holders with more than a key per element


class NestedRequired(Distinct):
	"""Distinct key, and every element must have a child list the hook iterates without a null check."""

	def __init__(self, tag, java, tags, key, nested):
		super().__init__(tag, java, tags, key)
		self.nested = nested

	def child(self, element):
		if element.tag in self.tags:
			if not _children(element, self.nested):
				_npe(self, f"<{element.tag}> without <{self.nested}>")
			super().child(element)


class ItemGroupsData(Holder):
	"""ItemGroupsData.java: bonusSize() sums 19 group lists (medals excluded), petFoodSize() sums distinct item ids per FoodType
	except EXCLUDES and STINKY. All groups are single elements (last occurrence wins) that the hook dereferences unconditionally."""
	java = "ItemGroupsData"
	BONUS_GROUPS = ("craft_materials", "craft_shop", "craft_bundles", "craft_recipes", "manastones_common", "manastones_rare", "food",
	                "medicine_common", "medicine_rare", "medicine_legendary", "ores_rare", "ores_legendary", "ores_unique", "ores_epic",
	                "gather_rare", "enchants", "events", "boss_rare", "boss_legendary")
	FOOD_GROUPS = {  # ItemGroupsData.getPetFood
		"AETHER_CRYSTAL_BISCUIT": "feed_crystal_biscuit", "AETHER_GEM_BISCUIT": "feed_gem_biscuit",
		"AETHER_POWDER_BISCUIT": "feed_powder_biscuit", "AETHER_CHERRY": "feed_aether_cherry", "ARMOR": "feed_armor",
		"BALAUR_SCALES": "feed_balaur_material", "BONES": "feed_bone", "FLUIDS": "feed_fluid", "SOULS": "feed_soul", "THORNS": "feed_thorn",
		"HEALTHY_FOOD_ALL": "feed_healthy_all", "HEALTHY_FOOD_SPICY": "feed_healthy_spicy", "POPPY_SNACK": "poppy_snack",
		"POPPY_SNACK_TASTY": "tasty_poppy_snack", "POPPY_SNACK_NUTRITIOUS": "nutritious_poppy_snack",
		"SHUGO_EVENT_COIN": "feed_shugo_event_coin", "STINKY": "stinking_junk", "EXCLUDES": "feed_exclude", "MISCELLANEOUS": None,
	}

	def __init__(self, tag):
		super().__init__(tag)
		self.groups = {}  # tag -> (item count, distinct ids)

	def child(self, element):
		items = _children(element, "item")
		self.groups[element.tag] = (len(items), {attr_int(i, "id") for i in items})

	def finish(self):
		for food_type in enums.FOOD_TYPE:
			group = self.FOOD_GROUPS[food_type]
			if group is not None and group not in self.groups:
				_npe(self, f"missing <{group}> (FoodType.{food_type})")
		for group in self.BONUS_GROUPS:
			if group not in self.groups:
				_npe(self, f"missing <{group}> in bonusSize()")

	def bonus_size(self):
		return sum(self.groups[g][0] for g in self.BONUS_GROUPS)

	def pet_food_size(self):
		return sum(len(self.groups[g][1]) for t, g in self.FOOD_GROUPS.items() if g is not None and t not in ("EXCLUDES", "STINKY"))


class NpcData(Holder):
	"""NpcData.init: distinct npc_id; stats of non-PET tribes are dereferenced (NpcTemplate @XmlElement stats)."""
	java = "NpcData"

	def __init__(self, tag):
		super().__init__(tag)
		self.keys = set()

	def child(self, element):
		if element.tag != "npc_template":
			return
		self.keys.add(attr_int(element, "npc_id"))
		if attr_enum(element, "tribe") not in ("PET", "PET_DARK") and not _children(element, "stats"):
			_npe(self, f"npc {element.get('npc_id')} without <stats>")

	def finish(self):
		self.require("npc_template")

	def size(self):
		return len(self.keys)


class NpcShoutData(Holder):
	"""NpcShoutData.afterUnmarshal: count += shoutList.getNpcShouts().size() for every shout_npcs of every shout_group."""
	java = "NpcShoutData"

	def __init__(self, tag):
		super().__init__(tag)
		self.n = 0

	def child(self, element):
		if element.tag == "shout_group":
			for shout_list in _children(element, "shout_npcs"):
				self.n += len(_children(shout_list, "shout"))

	def finish(self):
		self.require("shout_group")

	def size(self):
		return self.n


class SkillTreeData(Holder):
	"""SkillTreeData: size() sums the lists; a template without classId is added once per PlayerClass constant."""
	java = "SkillTreeData"

	def __init__(self, tag):
		super().__init__(tag)
		self.n = 0

	def child(self, element):
		if element.tag == "skill":
			attr_enum(element, "race", enums.RACE, "PC_ALL")
			self.n += len(enums.PLAYER_CLASS) if attr_enum(element, "classId", enums.PLAYER_CLASS) is None else 1

	def finish(self):
		self.require("skill")

	def size(self):
		return self.n


class GuideHtmlData(Holder):
	"""GuideHtmlData: distinct makeHash(classId or CLASS_ALL=255, race ordinal (null = PC_ALL), level) in Java int arithmetic."""
	java = "GuideHtmlData"

	def __init__(self, tag):
		super().__init__(tag)
		self.keys = set()

	def child(self, element):
		if element.tag != "guide":
			return
		cls = attr_enum(element, "classType", enums.PLAYER_CLASS)
		class_id = 255 if cls is None else enums.PLAYER_CLASS.index(cls)
		race = enums.RACE.index(attr_enum(element, "race", enums.RACE, "PC_ALL"))
		level = attr_int(element, "level")
		self.keys.add(wrap_int32(wrap_int32(wrap_int32(class_id << 8) | race) << 8) | level)

	def finish(self):
		self.require("guide")

	def size(self):
		return len(self.keys)


class SiegeLocationData(Holder):
	"""SiegeLocationData: only FORTRESS, ARTIFACT, OUTPOST and AGENT_FIGHT enter siegeLocations; a null type throws in the switch."""
	java = "SiegeLocationData"

	def __init__(self, tag):
		super().__init__(tag)
		self.keys = set()

	def child(self, element):
		if element.tag != "siege_location":
			return
		siege_type = attr_enum(element, "type", enums.SIEGE_TYPE)
		if siege_type is None:
			_npe(self, "siege_location without type (switch on null)")
		if siege_type in ("FORTRESS", "ARTIFACT", "OUTPOST", "AGENT_FIGHT"):
			self.keys.add(attr_int(element, "id"))

	def finish(self):
		self.require("siege_location")

	def size(self):
		return len(self.keys)


class ZoneData(Holder):
	"""ZoneData: count of zones that produce an Area (area_type default POLYGON; a SPHERE with r <= 0 produces none)."""
	java = "ZoneData"
	SHAPE_ELEMENT = {"POLYGON": "points", "CYLINDER": "cylinder", "SPHERE": "sphere", "SEMISPHERE": "semisphere"}

	def __init__(self, tag):
		super().__init__(tag)
		self.n = 0

	def child(self, element):
		if element.tag != "zone":
			return
		area_type = attr_enum(element, "area_type", enums.AREA_TYPE, "POLYGON")
		shapes = _children(element, self.SHAPE_ELEMENT[area_type])
		if not shapes:
			_npe(self, f"{area_type} zone {element.get('name')!r} without <{self.SHAPE_ELEMENT[area_type]}>")
		if area_type == "SPHERE":
			r = attr_float(shapes[-1], "r")
			if r is None:
				_npe(self, f"sphere zone {element.get('name')!r} without r")
			if r <= 0:
				return
		self.n += 1

	def finish(self):
		self.require("zone")

	def size(self):
		return self.n


class Portal2Data(Holder):
	"""Portal2Data: distinct portal_scroll names + portal_dialog npc ids + portal_use npc ids (all lists null-checked)."""
	java = "Portal2Data"

	def __init__(self, tag):
		super().__init__(tag)
		self.scrolls, self.dialogs, self.uses = set(), set(), set()

	def child(self, element):
		if element.tag == "portal_scroll":
			self.scrolls.add(attr_str(element, "name"))
		elif element.tag == "portal_dialog":
			self.dialogs.add(attr_int(element, "npc_id"))
		elif element.tag == "portal_use":
			self.uses.add(attr_int(element, "npc_id"))

	def size(self):
		return len(self.scrolls) + len(self.dialogs) + len(self.uses)


class GoodsListData(Holder):
	"""GoodsListData: distinct ids of list + in_list + purchase_list; all three lists are iterated without null checks."""
	java = "GoodsListData"
	TAGS = ("list", "in_list", "purchase_list")

	def __init__(self, tag):
		super().__init__(tag)
		self.ids = {t: set() for t in self.TAGS}

	def child(self, element):
		if element.tag in self.ids:
			self.ids[element.tag].add(attr_int(element, "id"))

	def finish(self):
		for t in self.TAGS:
			self.require(t)

	def size(self):
		return sum(len(s) for s in self.ids.values())


class TradeListData(Holder):
	"""TradeListData: size() is the distinct npc_id count of tradelist_template; all three lists are iterated without null checks."""
	java = "TradeListData"
	TAGS = ("tradelist_template", "trade_in_list_template", "purchase_template")

	def __init__(self, tag):
		super().__init__(tag)
		self.ids = set()

	def child(self, element):
		if element.tag == "tradelist_template":
			self.ids.add(attr_int(element, "npc_id"))

	def finish(self):
		for t in self.TAGS:
			self.require(t)

	def size(self):
		return len(self.ids)


class ExpandData(Holder):
	"""CubeExpandData / WarehouseExpandData: distinct npc ids over all expansion_npc ids lists (StorageExpansionTemplate @ids)."""

	def __init__(self, tag, java):
		super().__init__(tag)
		self.java = java
		self.ids = set()

	def child(self, element):
		if element.tag == "expansion_npc":
			ids = attr_int_list(element, "ids")
			if ids is None:
				_npe(self, "expansion_npc without ids")
			self.ids.update(ids)

	def finish(self):
		self.require("expansion_npc")

	def size(self):
		return len(self.ids)


class NpcSkillData(Holder):
	"""NpcSkillData: distinct npc ids over all npc_skills @npc_ids (@XmlList attribute), first list wins."""
	java = "NpcSkillData"

	def __init__(self, tag):
		super().__init__(tag)
		self.ids = set()

	def child(self, element):
		if element.tag == "npc_skills":
			ids = attr_int_list(element, "npc_ids")
			if ids is None:
				_npe(self, "npc_skills without npc_ids")
			self.ids.update(ids)

	def finish(self):
		self.require("npc_skills")

	def size(self):
		return len(self.ids)


class WalkerVersionsData(Holder):
	"""WalkerVersionsData: distinct version ids over all walk_parent/version elements (String keys, absent = null)."""
	java = "WalkerVersionsData"

	def __init__(self, tag):
		super().__init__(tag)
		self.ids = set()

	def child(self, element):
		if element.tag == "walk_parent":
			self.ids.update(attr_str(v, "id") for v in _children(element, "version"))

	def finish(self):
		self.require("walk_parent")

	def size(self):
		return len(self.ids)


class Mails(Holder):
	"""Mails (model/templates/mail/Mails.java): distinct name.toLowerCase()."""
	java = "Mails"

	def __init__(self, tag):
		super().__init__(tag)
		self.names = set()

	def child(self, element):
		if element.tag == "mail":
			name = attr_str(element, "name")
			if name is None:
				_npe(self, "mail without name")
			if not name.isascii():
				raise OracleError(f"<mails>: non-ASCII mail name {name!r}; Java's locale-dependent toLowerCase is not modelled")
			self.names.add(name.lower())

	def finish(self):
		self.require("mail")

	def size(self):
		return len(self.names)


class UniqueDistinct(Distinct):
	"""Distinct key where a duplicate throws IllegalArgumentException (StaticDoorData, HouseBuildingData)."""

	def child(self, element):
		if element.tag in self.tags:
			key = self.key(element)
			if key in self.keys:
				raise OracleError(f"<{self.tag}> ({self.java}): duplicate key {key!r} throws IllegalArgumentException at startup")
			self.keys.add(key)


class HouseData(Holder):
	"""HouseData: size() == number of land elements; address ids (wrapper addresses/address) must be unique."""
	java = "HouseData"

	def __init__(self, tag):
		super().__init__(tag)
		self.n = 0
		self.addresses = set()

	def child(self, element):
		if element.tag != "land":
			return
		self.n += 1
		wrappers = _children(element, "addresses")
		if not wrappers:
			_npe(self, "land without <addresses> wrapper")
		for address in _children(wrappers[-1], "address"):
			key = attr_int(address, "id")
			if key in self.addresses:
				raise OracleError(f"<house_lands>: duplicate house address {key} throws IllegalArgumentException at startup")
			self.addresses.add(key)

	def finish(self):
		self.require("land")

	def size(self):
		return self.n


class HouseNpcsData(Holder):
	"""HouseNpcsData: sum of spawn list sizes over distinct house addresses (last wins); duplicate spawn types per house throw."""
	java = "HouseNpcsData"

	def __init__(self, tag):
		super().__init__(tag)
		self.by_address = {}

	def child(self, element):
		if element.tag != "house":
			return
		spawns = _children(element, "spawn")
		types = [attr_enum(s, "type") for s in spawns]
		if len(set(types)) != len(types):
			raise OracleError(f"<house_npcs>: duplicate spawn type for house {element.get('address')} throws IllegalArgumentException")
		self.by_address[attr_int(element, "address")] = len(spawns)

	def finish(self):
		self.require("house")

	def size(self):
		return sum(self.by_address.values())


class TownSpawnsData(Holder):
	"""TownSpawnsData.getSpawnsCount: maps by map_id (last wins), TownSpawnMap maps town_spawn by town_id, TownSpawn maps town_level
	by level (both last wins, both lists iterated without null checks), TownLevel.getSpawns().size() (null list throws)."""
	java = "TownSpawnsData"

	def __init__(self, tag):
		super().__init__(tag)
		self.by_map = {}

	def child(self, element):
		if element.tag != "spawn_map":
			return
		towns = _children(element, "town_spawn")
		if not towns:
			_npe(self, f"spawn_map {element.get('map_id')} without town_spawn")
		by_town = {}
		for town in towns:
			levels = _children(town, "town_level")
			if not levels:
				_npe(self, f"town_spawn {town.get('town_id')} without town_level")
			by_level = {}
			for level in levels:
				spawns = _children(level, "spawn")
				if not spawns:
					_npe(self, f"town_level {level.get('level')} of town {town.get('town_id')} without spawn")
				by_level[attr_int(level, "level")] = len(spawns)
			by_town[attr_int(town, "town_id")] = sum(by_level.values())
		self.by_map[attr_int(element, "map_id")] = sum(by_town.values())

	def finish(self):
		self.require("spawn_map")

	def spawns_count(self):
		return sum(self.by_map.values())


class DecomposableItemsData(Holder):
	"""DecomposableItemsData: size() counts distinct item_id of non-selectable decomposables that have <items> elements."""
	java = "DecomposableItemsData"

	def __init__(self, tag):
		super().__init__(tag)
		self.ids = set()

	def child(self, element):
		if element.tag != "decomposable":
			return
		selectable = attr_bool(element, "selectable", False)
		item_id = attr_int(element, "item_id")
		if _children(element, "items") and not selectable:
			self.ids.add(item_id)

	def finish(self):
		self.require("decomposable")

	def size(self):
		return len(self.ids)


class GlobalNpcExclusionData(Holder):
	"""GlobalNpcExclusionData.isEmpty: none of the five @XmlList elements is present."""
	java = "GlobalNpcExclusionData"
	TAGS = ("npc_ids", "npc_names", "npc_types", "npc_tribes", "npc_abyss_types")

	def is_empty(self):
		return not any(self.child_counts[t] for t in self.TAGS)

	def suffix(self):
		return "" if self.is_empty() else " with global drop npc exclusions"


class ExperienceTable(Count):
	"""PlayerExperienceTable.getMaxLevel: length of the exp array (null = 0)."""

	def __init__(self, tag):
		super().__init__(tag, "PlayerExperienceTable", ("exp",), required=False)


class XmlQuests(Holder):
	"""XMLQuests (not logged by Java): distinct ids over the 16 @XmlElements choices. Reported as an extra."""
	java = "XMLQuests"
	TAGS = ("report_to", "monster_hunt", "xml_quest", "item_collecting", "relic_rewards", "crafting_rewards", "report_to_many",
	        "kill_in_world", "kill_in_zone", "skill_use", "kill_spawned", "mentor_monster_hunt", "fountain_rewards", "item_order",
	        "work_order", "report_on_levelup")

	def __init__(self, tag):
		super().__init__(tag)
		self.ids = set()

	def child(self, element):
		if element.tag in self.TAGS:
			self.ids.add(attr_int(element, "id"))

	def size(self):
		return len(self.ids)


def _tribe_key(element):
	return attr_enum(element, "name")


HOUSING_OBJECT_TAGS = ("postbox", "use_item", "move_item", "chair", "picture", "passive", "npc", "storage", "jukebox", "moviejukebox",
                       "emblem")

# StaticData @XmlElement name -> holder rule (StaticData.java:28-302, 92 fields)
HOLDERS = {
	"world_maps": distinct("WorldMapsData", ["map"], int_key("id")),  # WorldMapTemplate @id
	"weather": distinct("MapWeatherData", ["map"], int_key("id")),  # WeatherTable @id
	"npc_trade_list": TradeListData,
	"npc_teleporter": distinct("TeleporterData", ["teleporter_template"], int_key("teleportId")),
	"teleport_location": distinct("TeleLocationData", ["teleloc_template"], int_key("loc_id")),
	"bind_points": distinct("BindPointData", ["bind_point"], int_key("npcid")),
	"quests": distinct("QuestsData", ["quest"], int_key("id")),
	"quest_scripts": XmlQuests,
	"player_experience_table": ExperienceTable,
	"absolute_stats": distinct("AbsoluteStatsData", ["stats_set"], int_key("id")),
	"item_templates": distinct("ItemData", ["item_template"], int_key("id")),  # ItemTemplate.setXmlUid(@id)
	"random_bonuses": distinct("ItemRandomBonusData", ["random_bonus"],
	                           lambda e: (attr_enum(e, "type", enums.STAT_BONUS_TYPE) or _raise_null(e, "type"), attr_int(e, "id"))),
	"npc_templates": NpcData,
	"custom_drop": distinct("CustomDrop", ["npc_drop"], int_key("npc_id")),
	"npc_shouts": NpcShoutData,
	"player_initial_data": distinct("PlayerInitialData", ["player_data"], lambda e: attr_enum(e, "class", enums.PLAYER_CLASS)),
	"skill_data": distinct("SkillData", ["skill_template"], int_key("skill_id")),
	"motion_times": distinct("MotionData", ["motion_time"], str_key("name")),
	"skill_tree": SkillTreeData,
	"cube_expander": lambda tag: ExpandData(tag, "CubeExpandData"),
	"warehouse_expander": lambda tag: ExpandData(tag, "WarehouseExpandData"),
	"player_titles": distinct("TitleData", ["title"], int_key("id")),
	"gatherable_templates": distinct("GatherableData", ["gatherable_template"], int_key("id")),
	"npc_walker": distinct("WalkerData", ["walker_template"], str_key("route_id")),
	"zones": ZoneData,
	"goodslists": GoodsListData,
	"tribe_relations": distinct("TribeRelationsData", ["tribe"], _tribe_key),
	"recipe_templates": distinct("RecipeData", ["recipe_template"], int_key("id")),
	"chest_templates": distinct("ChestData", ["chest"], int_key("npc_id")),
	"staticdoor_templates": lambda tag: UniqueDistinct(tag, "StaticDoorData", ["world"], int_key("world")),
	"item_sets": lambda tag: NestedRequired(tag, "ItemSetData", ["itemset"], int_key("id"), "itempart"),
	"npc_factions": distinct("NpcFactionsData", ["npc_faction"], int_key("id")),
	"npc_skill_templates": NpcSkillData,
	"pet_skill_templates": distinct("PetSkillData", ["pet_skill"], int_key("order_skill")),
	"siege_locations": SiegeLocationData,
	"dimensional_vortex": distinct("VortexData", ["vortex_location"], int_key("id")),
	"rift_locations": distinct("RiftData", ["rift_location"], int_key("id")),
	"base_locations": count("BaseData", ["base_location"]),
	"fly_rings": count("FlyRingData", ["fly_ring"], required=False),
	"shields": count("ShieldData", ["shield"], required=False),
	"pets": distinct("PetData", ["pet"], int_key("id")),
	"pet_feed": distinct("PetFeedData", ["flavour"], int_key("id"), required=False),
	"dopings": distinct("PetDopingData", ["doping"], int_key("id")),
	"pet_buffs": distinct("PetBuffsData", ["buff"], int_key("id"), required=False),
	"guides": GuideHtmlData,
	"roads": count("RoadData", ["road"], required=False),
	"instance_cooltimes": distinct("InstanceCooltimeData", ["instance_cooltime"], int_key("worldId")),
	"decomposable_items": DecomposableItemsData,
	"ai_templates": distinct("AIData", ["ai"], int_key("npcId")),
	"flypath_template": distinct("FlyPathData", ["flypath_location"], int_key("id")),
	"windstreams": distinct("WindstreamData", ["windstream"], int_key("mapid")),
	"item_restriction_cleanups": count("ItemRestrictionCleanupData", ["cleanup"], required=False),
	"assembled_npcs": distinct("AssembledNpcsData", ["assembled_npc"], int_key("nr")),
	"cosmetic_items": distinct("CosmeticItemsData", ["cosmetic_item"], str_key("cosmetic_name")),
	"auto_groups": distinct("AutoGroupData", ["auto_group"], int_key("id")),
	"timed_events": count("EventData", ["event"], required=False),
	"spawns": distinct("SpawnsData", ["spawn_map"], int_key("map_id")),
	"item_groups": ItemGroupsData,
	"polymorph_panels": distinct("PanelSkillsData", ["panel"], lambda e: attr_byte(e, "panel_id")),
	"instance_bonusattrs": distinct("InstanceBuffData", ["instance_bonusattr"], int_key("buff_id")),
	"housing_objects": distinct("HousingObjectData", HOUSING_OBJECT_TAGS, int_key("id")),  # AbstractHouseObject @id
	"rides": distinct("RideData", ["ride_info"], int_key("id")),
	"instance_exits": count("InstanceExitData", ["instance_exit"]),
	"portal_locs": distinct("PortalLocData", ["portal_loc"], int_key("loc_id")),
	"portal_templates2": Portal2Data,
	"house_lands": HouseData,
	"buildings": lambda tag: UniqueDistinct(tag, "HouseBuildingData", ["building"], int_key("id"), required=False),
	"house_parts": distinct("HousePartsData", ["house_part"], int_key("id"), required=False),
	"curing_objects": count("CuringObjectsData", ["curing_object"]),
	"house_npcs": HouseNpcsData,
	"assembly_items": count("AssemblyItemsData", ["item"]),
	"mails": Mails,
	"material_templates": distinct("MaterialData", ["material"], int_key("id"), required=False),
	"challenge_tasks": distinct("ChallengeData", ["task"], int_key("id")),
	"conqueror_protector_ranks": count("ConquerorAndProtectorData", ["rank"]),
	"town_spawns_data": TownSpawnsData,
	"skill_charge": lambda tag: NestedRequired(tag, "SkillChargeData", ["charge"], int_key("id"), "skill"),
	"walker_versions": WalkerVersionsData,
	"tempering_templates": lambda tag: NestedRequired(tag, "TemperingData", ["tempering_list"], str_key("item_group"), "tempering_data"),
	"enchant_templates": lambda tag: NestedRequired(tag, "EnchantData", ["enchant_list"], str_key("item_group"), "enchant_data"),
	"global_rules": count("GlobalDropData", ["gd_rule"]),
	"global_npc_exclusions": GlobalNpcExclusionData,
	"multi_return_item": distinct("MultiReturnItemData", ["return_item"], int_key("id")),
	"hotspot_template": count("HotspotData", ["hotspot_location"], required=False),
	"item_purifications": lambda tag: NestedRequired(tag, "ItemPurificationData", ["item_purification"], int_key("base_item_id"),
	                                                 "purification_result"),
	"arcadelist": count("UpgradeArcadeData", ["rewards"]),
	"login_events": distinct("AtreianPassportData", ["login_event"], int_key("id")),
	"world_raid_locations": distinct("WorldRaidData", ["world_raid_location"], int_key("location_id")),
	"kill_bounties": count("KillBountyData", ["kill_bounty"]),
	"legion_dominion_template": count("LegionDominionData", ["legion_dominion_location"]),
	"alias_locations": distinct("SkillAliasLocationData", ["alias_location"], str_key("name")),
	"signet_data_templates": distinct("SignetDataTemplates", ["signet_data_template"],
	                                  lambda e: attr_enum(e, "signet_skill", enums.SIGNET_ENUM)),
}


def _raise_null(element, name):
	raise OracleError(f"<{element.tag}> without @{name}: EnumMap.get(null) result is dereferenced at startup")


# StaticData.afterUnmarshal, in order: (Java line, message template, [(holder tag, method)])
LINES = [
	(315, "Loaded {} maps", [("world_maps", "size")]),
	(316, "Loaded {} material ids", [("material_templates", "size")]),
	(317, "Loaded weather for {} maps", [("weather", "size")]),
	(318, "Loaded {} player experience table entries", [("player_experience_table", "size")]),
	(319, "Loaded {} absolute stat templates", [("absolute_stats", "size")]),
	(320, "Loaded {} item cleanup entries", [("item_restriction_cleanups", "size")]),
	(321, "Loaded {} item templates", [("item_templates", "size")]),
	(322, "Loaded {} item bonus templates", [("random_bonuses", "size")]),
	(323, "Loaded {} bonus item group templates and {} pet food items", [("item_groups", "bonus_size"), ("item_groups", "pet_food_size")]),
	(324, "Loaded {} npc templates", [("npc_templates", "size")]),
	(325, "Loaded {} custom npc drops", [("custom_drop", "size")]),
	(326, "Loaded {} system mail templates", [("mails", "size")]),
	(327, "Loaded {} npc shout templates", [("npc_shouts", "size")]),
	(328, "Loaded {} pet templates and {} food flavours", [("pets", "size"), ("pet_feed", "size")]),
	(329, "Loaded {} pet doping templates", [("dopings", "size")]),
	(330, "Loaded {} pet buffs templates", [("pet_buffs", "size")]),
	(331, "Loaded {} initial player templates", [("player_initial_data", "size")]),
	(332, "Loaded {} trade lists", [("npc_trade_list", "size")]),
	(333, "Loaded {} npc teleporter templates", [("npc_teleporter", "size")]),
	(334, "Loaded {} teleport locations", [("teleport_location", "size")]),
	(335, "Loaded {} skill templates", [("skill_data", "size")]),
	(336, "Loaded {} skill charge entries", [("skill_charge", "size")]),
	(337, "Loaded {} motion times", [("motion_times", "size")]),
	(338, "Loaded {} skill learn entries", [("skill_tree", "size")]),
	(339, "Loaded {} cube expand entries", [("cube_expander", "size")]),
	(340, "Loaded {} warehouse expand entries", [("warehouse_expander", "size")]),
	(341, "Loaded {} bind point entries", [("bind_points", "size")]),
	(342, "Loaded {} quest data entries", [("quests", "size")]),
	(343, "Loaded {} gatherable entries", [("gatherable_templates", "size")]),
	(344, "Loaded {} title entries", [("player_titles", "size")]),
	(345, "Loaded {} walker routes", [("npc_walker", "size")]),
	(346, "Loaded {} walker group variants", [("walker_versions", "size")]),
	(347, "Loaded {} zone entries", [("zones", "size")]),
	(348, "Loaded {} goodslist entries", [("goodslists", "size")]),
	(349, "Loaded {} tribe relation entries", [("tribe_relations", "size")]),
	(350, "Loaded {} recipe entries", [("recipe_templates", "size")]),
	(351, "Loaded {} chest locations", [("chest_templates", "size")]),
	(352, "Loaded {} static door locations", [("staticdoor_templates", "size")]),
	(353, "Loaded {} item set entries", [("item_sets", "size")]),
	(354, "Loaded {} npc factions", [("npc_factions", "size")]),
	(355, "Loaded {} npc skill list entries", [("npc_skill_templates", "size")]),
	(356, "Loaded {} pet skill list entries", [("pet_skill_templates", "size")]),
	(357, "Loaded {} siege location entries", [("siege_locations", "size")]),
	(358, "Loaded {} vortex entries", [("dimensional_vortex", "size")]),
	(359, "Loaded {} rift entries", [("rift_locations", "size")]),
	(360, "Loaded {} base entries", [("base_locations", "size")]),
	(361, "Loaded {} fly ring entries", [("fly_rings", "size")]),
	(362, "Loaded {} shield entries", [("shields", "size")]),
	(363, "Loaded {} pet entries", [("pets", "size")]),
	(364, "Loaded {} guide entries", [("guides", "size")]),
	(365, "Loaded {} road entries", [("roads", "size")]),
	(366, "Loaded {} instance cooltime entries", [("instance_cooltimes", "size")]),
	(367, "Loaded {} decomposable items entries", [("decomposable_items", "size")]),
	(368, "Loaded {} ai templates", [("ai_templates", "size")]),
	(369, "Loaded {} flypath templates", [("flypath_template", "size")]),
	(370, "Loaded {} windstream entries", [("windstreams", "size")]),
	(371, "Loaded {} assembled npcs entries", [("assembled_npcs", "size")]),
	(372, "Loaded {} cosmetic items entries", [("cosmetic_items", "size")]),
	(373, "Loaded {} auto group entries", [("auto_groups", "size")]),
	(374, "Loaded {} spawn maps entries", [("spawns", "size")]),
	(375, "Loaded {} events", [("timed_events", "size")]),
	(376, "Loaded {} skill panel entries", [("polymorph_panels", "size")]),
	(377, "Loaded {} instance Buffs entries", [("instance_bonusattrs", "size")]),
	(378, "Loaded {} housing object entries", [("housing_objects", "size")]),
	(379, "Loaded {} ride info entries", [("rides", "size")]),
	(380, "Loaded {} instance exit entries", [("instance_exits", "size")]),
	(381, "Loaded {} portal loc entries", [("portal_locs", "size")]),
	(382, "Loaded {} portal templates2 entries", [("portal_templates2", "size")]),
	(383, "Loaded {} housing lands", [("house_lands", "size")]),
	(384, "Loaded {} house building styles", [("buildings", "size")]),
	(385, "Loaded {} house parts", [("house_parts", "size")]),
	(386, "Loaded {} house spawns", [("house_npcs", "size")]),
	(387, "Loaded {} curing object entries", [("curing_objects", "size")]),
	(388, "Loaded {} assembly items entries", [("assembly_items", "size")]),
	(389, "Loaded {} challenge tasks entries", [("challenge_tasks", "size")]),
	(390, "Loaded {} conqueror and protector entries", [("conqueror_protector_ranks", "size")]),
	(391, "Loaded {} town spawns", [("town_spawns_data", "spawns_count")]),
	(392, "Loaded {} temperings", [("tempering_templates", "size")]),
	(393, "Loaded {} enchants", [("enchant_templates", "size")]),
	(394, "Loaded {} global drop rules{}", [("global_rules", "size"), ("global_npc_exclusions", "suffix")]),
	(395, "Loaded {} multi return item entries", [("multi_return_item", "size")]),
	(396, "Loaded {} hotspot entries", [("hotspot_template", "size")]),
	(397, "Loaded {} item purifications entries", [("item_purifications", "size")]),
	(398, "Loaded {} upgrade arcade entries", [("arcadelist", "size")]),
	(399, "Loaded {} atreian passports", [("login_events", "size")]),
	(400, "Loaded {} world raid locations", [("world_raid_locations", "size")]),
	(401, "Loaded {} kill bounty templates", [("kill_bounties", "size")]),
	(402, "Loaded {} legion dominion locations", [("legion_dominion_template", "size")]),
	(403, "Loaded {} skill alias locations", [("alias_locations", "size")]),
	(404, "Loaded {} signet data templates", [("signet_data_templates", "size")]),
]

# Not logged by Java; reported for the C++ loader's own checks.
EXTRAS = [
	("xml_quests", "XMLQuests distinct quest ids", ("quest_scripts", "size")),
]


class CountVisitor:
	"""Builds one holder per import; a later import with the same root tag replaces the earlier holder (JAXB field assignment)."""

	def __init__(self, holders=None):
		self.holders = dict(holders or {})
		self.replaced = []
		self.current = None

	def begin_holder(self, imp, file, tag, attrib):
		factory = HOLDERS.get(tag)
		if factory is None:
			raise OracleError(f"import {imp.file_attribute!r}: root <{tag}> is not a StaticData element (JAXB: unexpected element)")
		self.current = factory(tag)

	def skipped_root(self, imp, file, tag, attrib):
		pass

	def holder_child(self, element):
		self.current.accept(element)

	def end_holder(self, imp):
		self.current.finish()
		if self.current.tag in self.holders:
			self.replaced.append((self.current.tag, imp.file_attribute))
		self.holders[self.current.tag] = self.current
		self.current = None


def evaluate(holders):
	"""Returns the count lines as dicts: {javaLine, line, values, holders}."""
	missing = sorted(set(HOLDERS) - set(holders))
	if missing:
		raise OracleError(f"StaticData.afterUnmarshal would throw NullPointerException: holders never imported: {missing}")
	result = []
	for java_line, template, calls in LINES:
		args, values = [], []
		for tag, method in calls:
			v = getattr(holders[tag], method)()
			args.append(v)
			if isinstance(v, int):
				values.append(v)
		result.append({"javaLine": java_line, "line": template.format(*args), "values": values, "holders": sorted({t for t, _ in calls})})
	return result


def evaluate_extras(holders):
	return {key: {"description": desc, "value": getattr(holders[tag], method)()} for key, desc, (tag, method) in EXTRAS}
