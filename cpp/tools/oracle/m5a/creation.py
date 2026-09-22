"""m5a-creation: what a new level 1 character gets from PlayerService.newPlayer and storeNewPlayer.

Java rules:
- spawn point: PlayerInitialData.getSpawnLocation(race) (<elyos_spawn_location>, <asmodian_spawn_location>);
- skills: SkillLearnService.learnNewSkills(player, 1, 1): the autolearn templates of SkillTreeData.getTemplatesFor(class, 1, race) (race specific
  first, then PC_ALL; a template without classId belongs to every class), without skill 30001 for non-starting classes; the level is the skill
  template's lvl; PlayerSkillList.addSkill keeps the higher level of a skill id added twice;
- items: the <player_data class=...> items; ItemFactory.newItem caps the count at max_stack_count (kinah 182400001 is never capped) and throws
  NullPointerException for an unknown item id; every armor or weapon is equipped (the new Equipment is empty, so isSlotEquipped is always false)
  with ItemSlot.getSlotFor(itemGroup.validEquipmentSlots).slotIdMask, the first non-combo slot of the mask; kinah is the inventory's kinah item;
- base stats: PlayerStatCalculator.calculateMaxHp/calculateMaxMp with PlayerClass.healthMultiplier/willMultiplier in float arithmetic.
The enum constructor data (ItemGroup, ItemSubType, ItemSlot, PlayerClass) is read from the Java sources.
"""

from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path

from staticdata_oracle import OracleError

from .data import StaticData, java_boolean, java_int
from .javafloat import f32, parse_float, to_int

KINAH = 182400001
RACES = ("ELYOS", "ASMODIANS")


def enum_constants(source: Path, enum_name: str) -> list[tuple[str, str | None]]:
	"""(name, argument text) of every constant of `enum enum_name { ... }` (comments removed, one constant per line)."""
	try:
		text = source.read_text(encoding="utf-8")
	except OSError as e:
		raise OracleError(f"{source}: {e}") from e
	text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
	text = re.sub(r"//[^\n]*", "", text)
	match = re.search(r"\benum\s+" + enum_name + r"\b[^{]*\{(.*?);", text, re.DOTALL)
	if not match:
		raise OracleError(f"{source}: enum {enum_name} not found")
	body = match.group(1) + ";"
	constants = []
	depth = 0
	current = ""
	for ch in body:
		if ch == "(":
			depth += 1
		elif ch == ")":
			depth -= 1
		if depth == 0 and ch in ",;":
			item = current.strip()
			if item:
				name_match = re.match(r"^([A-Za-z_][A-Za-z0-9_]*)\s*(?:\((.*)\))?$", item, re.DOTALL)
				if not name_match:
					raise OracleError(f"{source}: cannot parse enum constant {item!r}")
				constants.append((name_match.group(1), name_match.group(2)))
			current = ""
		else:
			current += ch
	return constants


class JavaEnums:
	"""Constructor data of ItemSlot, ItemSubType, ItemGroup and PlayerClass read from the Java source tree."""

	def __init__(self, java_src: Path):
		base = Path(java_src) / "com" / "aionemu" / "gameserver"
		self.slots: list[tuple[str, int, bool]] = []  # name, mask, combo (ordinal order)
		masks: dict[str, int] = {}
		for name, args in enum_constants(base / "model" / "items" / "ItemSlot.java", "ItemSlot"):
			parts = [p.strip() for p in (args or "").split(",")]
			combo = len(parts) > 1 and parts[1] == "true"
			mask = 0
			for term in parts[0].split("|"):
				term = term.strip()
				shift = re.fullmatch(r"1L(?:\s*<<\s*(\d+))?", term)
				ref = re.fullmatch(r"([A-Z][A-Z0-9_]*)\.slotIdMask", term)
				if shift:
					mask |= 1 << int(shift.group(1) or 0)
				elif ref:
					mask |= masks[ref.group(1)]
				else:
					raise OracleError(f"ItemSlot.{name}: cannot evaluate {term!r}")
			masks[name] = mask
			self.slots.append((name, mask, combo))
		self.sub_type_equip: dict[str, str] = {}
		for name, args in enum_constants(base / "model" / "templates" / "item" / "enums" / "ItemSubType.java", "ItemSubType"):
			arg = (args or "").strip()
			self.sub_type_equip[name] = "ARMOR" if arg.startswith("ArmorType.") else arg.removeprefix("EquipType.")
		self.item_groups: dict[str, tuple[int, str]] = {}  # name -> (valid slots, equip type)
		for name, args in enum_constants(base / "model" / "templates" / "item" / "enums" / "ItemGroup.java", "ItemGroup"):
			if args is None:
				self.item_groups[name] = (0, "NONE")  # ItemGroup(): no slots, ItemSubType.NONE
				continue
			first, _, rest = args.partition(",")
			slots = 0
			for term in first.split("|"):
				term = term.strip()
				ref = re.fullmatch(r"ItemSlot\.([A-Z][A-Z0-9_]*)\.getSlotIdMask\(\)", term)
				if ref:
					slots |= masks[ref.group(1)]
				elif re.fullmatch(r"\d+", term):
					slots |= int(term)
				else:
					raise OracleError(f"ItemGroup.{name}: cannot evaluate {term!r}")
			second = rest.split(",")[0].strip()
			if second.startswith("ArmorType."):
				equip = "ARMOR"
			elif second.startswith("ItemSubType."):
				equip = self.sub_type_equip[second.removeprefix("ItemSubType.")]
			else:
				raise OracleError(f"ItemGroup.{name}: unexpected argument {second!r}")
			self.item_groups[name] = (slots, equip)
		# name -> (starting class, health, will, health multiplier, will multiplier); the constructor is
		# PlayerClass(classId, nameId, isStartingClass|startingClass, power, health, agility, accuracy, knowledge, will, healthMultiplier,
		# willMultiplier, magicalCriticalResist)
		self.classes: dict[str, tuple[bool, int, int, int, int]] = {}
		for name, args in enum_constants(base / "model" / "PlayerClass.java", "PlayerClass"):
			parts = [p.strip() for p in (args or "").split(",")]
			if len(parts) != 12:
				raise OracleError(f"PlayerClass.{name}: expected 12 constructor arguments")
			self.classes[name] = (parts[2] == "true", int(parts[4]), int(parts[8]), int(parts[9]), int(parts[10]))

	def slot_for(self, slot_mask: int) -> int:
		"""ItemSlot.getSlotFor(mask).getSlotIdMask(): the first non-combo slot fully contained in the mask."""
		if slot_mask == 0:
			raise OracleError("ItemSlot.getSlotsFor: slotIdMask cannot be 0 (IllegalArgumentException)")
		for _, mask, combo in self.slots:
			if not combo and (slot_mask & mask) == mask:
				return mask
		raise OracleError(f"ItemSlot.getSlotFor({slot_mask}): no slot (ArrayIndexOutOfBoundsException)")


def max_hp(health_multiplier: int, level: int) -> int:
	"""PlayerStatCalculator.calculateMaxHp"""
	base = health_multiplier // 2
	mod1 = f32(f32(0.1075) * health_multiplier)
	mod2 = f32(f32(0.002875) * health_multiplier)
	return to_int(f32(f32(base + f32(level * mod1)) + f32(f32(level * level) * mod2)))


def max_mp(will_multiplier: int, level: int) -> int:
	"""PlayerStatCalculator.calculateMaxMp"""
	base = f32(will_multiplier * f32(0.35))
	mod1 = f32(f32(level * base) / f32(2.0))
	mod2 = f32(f32(f32(level * level * will_multiplier) * f32(0.125)) / 10000)
	return to_int(f32(f32(base + mod1) + mod2))


def base_stat_dependent_additional_value(base_stat: int, multiplier: int) -> int:
	"""PlayerGameStats.calculateBaseStatDependentAdditionalValue: (int) ((baseStat.getCurrent() - 100) / 100f * multiplier)"""
	return to_int(f32(f32(f32(base_stat - 100) / f32(100.0)) * multiplier))


def stats_info_base_max_hp(health: int, health_multiplier: int, level: int) -> int:
	"""
	The value SM_STATS_INFO writes as [base hp]: pgs.getMaxHp().getBase(), i.e. the stats template's maxHp
	(PlayerStatCalculator.calculateMaxHp through PlayerClass.createStatsTemplate) plus what MaxHpFunction adds to the BASE - not the bonus -
	in PlayerStatFunctions (getHealthDependentAdditionalHp). A fresh character has no other MAXHP stat function: the starting gear carries no
	MAXHP modifier, passive skill effects are not applied, and the HEALTH stat is the class value, so its Stat2 current is the class health.
	"""
	return max_hp(health_multiplier, level) + base_stat_dependent_additional_value(health, health_multiplier)


def stats_info_base_max_mp(will: int, will_multiplier: int, level: int) -> int:
	"""The value SM_STATS_INFO writes as [base mana]: calculateMaxMp plus MaxMpFunction's getWillDependentAdditionalMp (see above)."""
	return max_mp(will_multiplier, level) + base_stat_dependent_additional_value(will, will_multiplier)


@dataclass(frozen=True)
class ItemInfo:
	item_group: str
	max_stack_count: int


def _item_templates(data: StaticData, wanted: set[int]) -> dict[int, ItemInfo]:
	items: dict[int, ItemInfo] = {}
	for element in data.stream("item_templates", "item_template"):
		item_id = java_int(element.get("id"), "item_template id")
		if item_id in wanted:
			items[item_id] = ItemInfo(element.get("item_group", "NONE"), java_int(element.get("max_stack_count"), "max_stack_count", 1))
	return items


def _skill_levels(data: StaticData, wanted: set[int]) -> dict[int, int]:
	levels: dict[int, int] = {}
	for element in data.stream("skill_data", "skill_template"):
		skill_id = java_int(element.get("skill_id"), "skill_template skill_id")
		if skill_id in wanted:
			levels[skill_id] = java_int(element.get("lvl"), f"skill {skill_id} lvl", 0)
	return levels


def creation_report(data: StaticData, java_src: Path, race: str, player_class: str) -> dict:
	if race not in RACES:
		raise OracleError(f"race must be one of {RACES}")
	enums = JavaEnums(java_src)
	if player_class not in enums.classes:
		raise OracleError(f"unknown player class {player_class}")
	starting, health, will, health_multiplier, will_multiplier = enums.classes[player_class]

	location = None
	class_items: list[tuple[int, int]] | None = None
	for element in data.children("player_initial_data"):
		if element.tag == ("elyos_spawn_location" if race == "ELYOS" else "asmodian_spawn_location"):
			location = {"mapId": java_int(element.get("map_id"), "map_id"), "x": parse_float(element.get("x")), "y": parse_float(element.get("y")),
			            "z": parse_float(element.get("z")), "heading": java_int(element.get("heading"), "heading", 0)}
		elif element.tag == "player_data" and element.get("class") == player_class:  # afterUnmarshal: data.put, the last one wins
			class_items = [(java_int(i.get("id"), "item id"), java_int(i.get("count"), "item count", 0)) for i in element.findall("items/item")]
	if location is None:
		raise OracleError(f"no spawn location for {race}")

	# skills
	templates = []
	for element in data.children("skill_tree", "skill"):
		if java_int(element.get("minLevel"), "minLevel") != 1 or not java_boolean(element.get("autolearn")):
			continue
		class_id = element.get("classId")
		if class_id is not None and class_id != player_class:
			continue
		skill_race = element.get("race", "PC_ALL")
		if skill_race not in (race, "PC_ALL"):
			continue
		templates.append((0 if skill_race == race else 1, java_int(element.get("skillId"), "skillId")))
	templates.sort(key=lambda t: t[0])  # SkillTreeData.getTemplatesFor: race specific first (stable document order inside)
	skill_ids = [skill_id for _, skill_id in templates if not (skill_id == 30001 and not starting)]
	levels = _skill_levels(data, set(skill_ids))
	skills: dict[int, int] = {}
	for skill_id in skill_ids:
		if skill_id not in levels:
			raise OracleError(f"skill {skill_id} has no skill template (SkillLearnTemplate.getSkillLevel throws NullPointerException)")
		skills[skill_id] = max(skills.get(skill_id, 0), levels[skill_id])

	# items
	items_out = []
	if class_items is not None:
		infos = _item_templates(data, {item_id for item_id, _ in class_items})
		for item_id, count in class_items:
			info = infos.get(item_id)
			if info is None:
				raise OracleError(f"item {item_id} has no template (ItemFactory.newItem returns null, newItem(id, count) throws NullPointerException)")
			if count > info.max_stack_count and item_id != KINAH:
				count = info.max_stack_count
			if info.item_group not in enums.item_groups:
				raise OracleError(f"item {item_id}: unknown item_group {info.item_group}")
			slots, equip = enums.item_groups[info.item_group]
			equipped = equip in ("ARMOR", "WEAPON")
			items_out.append({"itemId": item_id, "count": count, "kinah": item_id == KINAH, "equipped": equipped,
			                  "slot": enums.slot_for(slots) if equipped else 0})

	return {
		"format": "aion-m5a-creation",
		"version": 1,
		"race": race,
		"playerClass": player_class,
		"spawn": location,
		"items": items_out,
		"skills": [{"skillId": skill_id, "level": level} for skill_id, level in sorted(skills.items())],
		# What SM_STATS_INFO writes as [base hp] / [base mana] for a fresh level 1 character, which is what scenario check V9 compares.
		# statsTemplate is the PlayerStatCalculator half alone (PlayerClass.createStatsTemplate), kept separate because the two differ.
		"baseStats": {"maxHp": stats_info_base_max_hp(health, health_multiplier, 1), "maxMp": stats_info_base_max_mp(will, will_multiplier, 1)},
		"statsTemplate": {"maxHp": max_hp(health_multiplier, 1), "maxMp": max_mp(will_multiplier, 1)},
	}
