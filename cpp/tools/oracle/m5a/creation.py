"""m5a-creation: what a new level 1 character gets from PlayerService.newPlayer and storeNewPlayer.

Java rules:
- spawn point: PlayerInitialData.getSpawnLocation(race) (<elyos_spawn_location>, <asmodian_spawn_location>);
- skills: SkillLearnService.learnNewSkills(player, 1, 1) through learn_new_skills: the autolearn templates of
  SkillTreeData.getTemplatesFor(class, 1, race) (race specific first, then PC_ALL; a template without classId belongs to every class), without
  skill 30001 for a class that is not a starting class - but a non-starting class first learns its starting class's templates, and that arm
  does teach 30001 (only starting classes can be created, CM_CREATE_CHARACTER.java:92, so the M5a characters never take it); the level is the
  skill template's lvl; PlayerSkillList.addSkill keeps the higher level of a skill id added twice;
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
		# name -> PlayerClass.getStartingClass(): the class itself for a starting class (`this.startingClass = this`), the named constant for
		# the others (the PlayerClass(..., PlayerClass startingClass, ...) constructor)
		self.starting_classes: dict[str, str] = {}
		for name, args in enum_constants(base / "model" / "PlayerClass.java", "PlayerClass"):
			parts = [p.strip() for p in (args or "").split(",")]
			if len(parts) != 12:
				raise OracleError(f"PlayerClass.{name}: expected 12 constructor arguments")
			self.classes[name] = (parts[2] == "true", int(parts[4]), int(parts[8]), int(parts[9]), int(parts[10]))
			if parts[2] == "true":
				self.starting_classes[name] = name
			elif re.fullmatch(r"[A-Z][A-Z0-9_]*", parts[2]):
				self.starting_classes[name] = parts[2]
			else:
				raise OracleError(f"PlayerClass.{name}: cannot read the starting class argument {parts[2]!r}")

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


def learn_new_skills(data: StaticData, enums: JavaEnums, race: str, player_class: str, from_level: int, to_level: int) -> dict[int, int]:
	"""
	SkillLearnService.learnNewSkills(player, fromLevel, toLevel) on an EMPTY PlayerSkillList (SkillLearnService.java:60-93): skill id -> level.

	- the levels run from toLevel down to fromLevel, and for a level below 10 a character whose class is not a starting class first learns
	  the level's autolearn templates of its starting class (:63-66);
	- autoLearnSkills(level, class, race) walks SkillTreeData.getTemplatesFor(class, level, race): the templates of `minLevel == level` whose
	  classId is the class or absent (SkillTreeData.afterUnmarshal adds a class-less row to every class) and whose race is the player's -
	  race specific first, then PC_ALL, each in document order (SkillTreeData.java:71-83) - skipping the non-autolearn ones and skill 30001
	  when THE CLASS PASSED IN is not a starting class (:88), which is why the starting class arm of a daeva class still teaches 30001;
	- PlayerSkillList.addSkill keeps the higher level of a skill id added twice (PlayerSkillList.java:57-64), and the level is the skill
	  template's `lvl` (SkillLearnTemplate.getSkillLevel), which throws NullPointerException for a skill id without a template;
	- the daeva branch (:69-74, 30001 becomes 30002) needs toLevel >= 10 and PlayerCommonData.isDaeva(), which this model refuses to answer for.
	"""
	if to_level >= 10:
		raise OracleError(f"level {to_level}: from level 10 on learnNewSkills depends on PlayerCommonData.isDaeva() (SkillLearnService.java:70), "
		                  "which is quest state, not static data")
	if from_level < 1 or from_level > to_level:
		raise OracleError(f"learnNewSkills({from_level}, {to_level}): the level range is empty")
	if player_class not in enums.classes:
		raise OracleError(f"unknown player class {player_class}")

	# (classId, race, minLevel, autolearn, skillId) of every <skill> in document order, which is the order SkillTreeData.afterUnmarshal fills
	# its per (class, race, minLevel) lists in
	rows = []
	for element in data.children("skill_tree", "skill"):
		rows.append((element.get("classId"), element.get("race", "PC_ALL"), java_int(element.get("minLevel"), "minLevel"),
		             java_boolean(element.get("autolearn")), java_int(element.get("skillId"), "skillId")))

	def templates_for(cls: str, level: int) -> list[tuple[bool, int]]:
		specific = [(auto, skill) for class_id, skill_race, min_level, auto, skill in rows
		            if (class_id is None or class_id == cls) and skill_race == race and min_level == level]
		generic = [(auto, skill) for class_id, skill_race, min_level, auto, skill in rows
		           if (class_id is None or class_id == cls) and skill_race == "PC_ALL" and min_level == level]
		return specific + generic

	def auto_learn(level: int, cls: str, learned: list[int]) -> None:
		cls_starting = enums.classes[cls][0]
		for autolearn, skill_id in templates_for(cls, level):
			if not autolearn:
				continue
			if skill_id == 30001 and not cls_starting:
				continue
			learned.append(skill_id)

	starting_class = None if enums.classes[player_class][0] else enums.starting_classes[player_class]
	learned: list[int] = []
	for level in range(to_level, from_level - 1, -1):
		if level < 10 and starting_class is not None:
			auto_learn(level, starting_class, learned)
		auto_learn(level, player_class, learned)

	levels = _skill_levels(data, set(learned))
	skills: dict[int, int] = {}
	for skill_id in learned:
		if skill_id not in levels:
			raise OracleError(f"skill {skill_id} has no skill template (SkillLearnTemplate.getSkillLevel throws NullPointerException)")
		skills[skill_id] = max(skills.get(skill_id, 0), levels[skill_id])
	return skills


def creation_report(data: StaticData, java_src: Path, race: str, player_class: str) -> dict:
	if race not in RACES:
		raise OracleError(f"race must be one of {RACES}")
	enums = JavaEnums(java_src)
	if player_class not in enums.classes:
		raise OracleError(f"unknown player class {player_class}")
	_starting, health, will, health_multiplier, will_multiplier = enums.classes[player_class]

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

	# skills: PlayerService.newPlayer calls learnNewSkills(newPlayer, 1, newPlayer.getLevel()) and a new character is level 1
	# (PlayerService.java:202, CM_CREATE_CHARACTER.java:62)
	skills = learn_new_skills(data, enums, race, player_class, 1, 1)

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
