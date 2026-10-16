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
- base stats: PlayerStatCalculator.calculateMaxHp/calculateMaxMp with PlayerClass.healthMultiplier/willMultiplier in float arithmetic;
- the passive skill effects (passive_stat_functions): PlayerEnterWorldService.activatePassiveSkillEffects applies every PASSIVE skill of the
  list through SkillEngine.applyEffectDirectly(template, level, player, player) (PlayerEnterWorldService.java:407-413; closed in the C++ server
  by M5b-2 part 3, m5b2-plan.md D2), and the stat functions each effect class's startEffect registers are reported with whether they change
  the character's stats for its starting equipment. They never touch the BASE of MAXHP or MAXMP (checked), so baseStats does not move.
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
		# name -> ItemGroup.getItemSubType(): the ItemSubType argument, NONE for the ArmorType constructors and ItemGroup() (ItemGroup.java:114-139)
		self.item_group_sub_types: dict[str, str] = {}
		for name, args in enum_constants(base / "model" / "templates" / "item" / "enums" / "ItemGroup.java", "ItemGroup"):
			if args is None:
				self.item_groups[name] = (0, "NONE")  # ItemGroup(): no slots, ItemSubType.NONE
				self.item_group_sub_types[name] = "NONE"
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
				self.item_group_sub_types[name] = "NONE"
			elif second.startswith("ItemSubType."):
				equip = self.sub_type_equip[second.removeprefix("ItemSubType.")]
				self.item_group_sub_types[name] = second.removeprefix("ItemSubType.")
			else:
				raise OracleError(f"ItemGroup.{name}: unexpected argument {second!r}")
			self.item_groups[name] = (slots, equip)
		# name -> (starting class, health, will, health multiplier, will multiplier); the constructor is
		# PlayerClass(classId, nameId, isStartingClass|startingClass, power, health, agility, accuracy, knowledge, will, healthMultiplier,
		# willMultiplier, magicalCriticalResist)
		self.classes: dict[str, tuple[bool, int, int, int, int]] = {}
		# name -> the power argument, which PlayerStatsTemplate.getPower() answers (PlayerClass.java:200-205)
		self.powers: dict[str, int] = {}
		# name -> PlayerClass.getStartingClass(): the class itself for a starting class (`this.startingClass = this`), the named constant for
		# the others (the PlayerClass(..., PlayerClass startingClass, ...) constructor)
		self.starting_classes: dict[str, str] = {}
		for name, args in enum_constants(base / "model" / "PlayerClass.java", "PlayerClass"):
			parts = [p.strip() for p in (args or "").split(",")]
			if len(parts) != 12:
				raise OracleError(f"PlayerClass.{name}: expected 12 constructor arguments")
			self.classes[name] = (parts[2] == "true", int(parts[4]), int(parts[8]), int(parts[9]), int(parts[10]))
			self.powers[name] = int(parts[3])
			if parts[2] == "true":
				self.starting_classes[name] = name
			elif re.fullmatch(r"[A-Z][A-Z0-9_]*", parts[2]):
				self.starting_classes[name] = parts[2]
			else:
				raise OracleError(f"PlayerClass.{name}: cannot read the starting class argument {parts[2]!r}")
		# name -> ItemAttackType.isMagical(), the first constructor argument (ItemAttackType(boolean magic, SkillElement elem))
		self.attack_type_magical: dict[str, bool] = {}
		for name, args in enum_constants(base / "model" / "templates" / "item" / "ItemAttackType.java", "ItemAttackType"):
			first = (args or "").split(",")[0].strip()
			if first not in ("true", "false"):
				raise OracleError(f"ItemAttackType.{name}: cannot read the magic argument {first!r}")
			self.attack_type_magical[name] = first == "true"

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
	in PlayerStatFunctions (getHealthDependentAdditionalHp). A fresh character has no other function on the base of MAXHP: the starting gear
	carries no MAXHP modifier, the functions of its passive skill effects are bonus functions or leave MAXHP alone (passive_stat_functions
	refuses a character for which that is not true), and the HEALTH stat is the class value, so its Stat2 current is the class health.
	"""
	return max_hp(health_multiplier, level) + base_stat_dependent_additional_value(health, health_multiplier)


def stats_info_base_max_mp(will: int, will_multiplier: int, level: int) -> int:
	"""The value SM_STATS_INFO writes as [base mana]: calculateMaxMp plus MaxMpFunction's getWillDependentAdditionalMp (see above)."""
	return max_mp(will_multiplier, level) + base_stat_dependent_additional_value(will, will_multiplier)


@dataclass(frozen=True)
class ItemInfo:
	item_group: str
	max_stack_count: int
	# ItemTemplate.getAttackType (attack_type; the field has no default, so a template without the attribute answers null) and WeaponStats
	# min_damage/max_damage (0 without <weapon_stats>)
	attack_type: str | None = None
	min_damage: int = 0
	max_damage: int = 0
	has_weapon_stats: bool = False
	# the stat names of the template's <modifiers> children (ItemTemplate.getModifiers), for the models that refuse an item which touches them
	modifier_stats: frozenset[str] = frozenset()
	# the <modifiers> children themselves: (tag, stat name, value, bonus) in document order
	modifiers: tuple[tuple[str, str, int, bool], ...] = ()


def _item_templates(data: StaticData, wanted: set[int]) -> dict[int, ItemInfo]:
	items: dict[int, ItemInfo] = {}
	for element in data.stream("item_templates", "item_template"):
		item_id = java_int(element.get("id"), "item_template id")
		if item_id in wanted:
			weapon = element.find("weapon_stats")
			modifiers = element.find("modifiers")
			items[item_id] = ItemInfo(
				element.get("item_group", "NONE"), java_int(element.get("max_stack_count"), "max_stack_count", 1),
				attack_type=element.get("attack_type"),
				min_damage=java_int(weapon.get("min_damage"), f"item {item_id} min_damage", 0) if weapon is not None else 0,
				max_damage=java_int(weapon.get("max_damage"), f"item {item_id} max_damage", 0) if weapon is not None else 0,
				has_weapon_stats=weapon is not None,
				modifier_stats=frozenset(child.get("name", "") for child in modifiers) if modifiers is not None else frozenset(),
				modifiers=tuple((child.tag, child.get("name", ""), java_int(child.get("value"), f"item {item_id} modifier value", 0),
				                 java_boolean(child.get("bonus"))) for child in modifiers) if modifiers is not None else ())
	return items


# ---- SM_STATS_INFO's current max HP and MP (m5b2-plan.md §10.3 X1, X10) --------------------------------------------------------------------------

def stats_info_current_max(stat: str, base: int, equipped_items: list[ItemInfo], passives: list[dict]) -> dict:
	"""
	What SM_STATS_INFO writes as the CURRENT [max hp] / [max mana] (pgs.getMaxHp().getCurrent(), getMaxMp()) of a fresh character, beside the
	base baseStats reports: the equipped items' <modifiers><add name="MAXHP|MAXMP" bonus="true"/> are bonus StatAddFunctions
	(ModifiersTemplate binds <add> to StatAddFunction, StatAddFunction.apply: `stat.addToBonus(value)`), a passive's bonus StatAddFunction on
	the stat adds the same way, and Stat2.getCurrent() is `(int) (base * 1 + bonus * 1 + base * 0) * 1` in float. `bonus` is reported too:
	a later bonus function - the soul sickness's MAXHP/MAXMP PERCENT - adds to the same float (m5b2-plan.md X10).
	Refused: any other modifier on the stat (a <rate>, a <sub>, a non-bonus <add> that would move the base baseStats reports) and any passive
	on it that is not a bonus StatAddFunction.
	"""
	bonus = 0.0
	for item in equipped_items:
		for tag, name, value, is_bonus in item.modifiers:
			if name != stat:
				continue
			if tag != "add" or not is_bonus:
				raise OracleError(f"an equipped {item.item_group} carries <{tag} name={stat} bonus={is_bonus}>, which the max {stat} model does not model")
			bonus = f32(bonus + value)
	for function in passives:
		if not function["applies"] or function["stat"] != stat:
			continue
		if function["function"] != "StatAddFunction" or not function["bonus"]:
			raise OracleError(f"skill {function['skillId']}: a {function['function']} on {stat} is not modelled")
		bonus = f32(bonus + function["value"])
	return {"base": base, "bonus": bonus, "current": to_int(f32(f32(base) + f32(bonus)))}


# ---- SM_STATS_INFO's main hand physical attack (m5b2-plan.md §10.3 X1) ------------------------------------------------------------------------

# the stats whose functions reach PlayerGameStats.getMainHandPAttack: PHYSICAL_ATTACK (getStat), MAIN_HAND_POWER (applyStatFunctions) and
# POWER, which PhysicalAttackFunction reads for the base rate
MAIN_HAND_P_ATTACK_STATS = ("PHYSICAL_ATTACK", "MAIN_HAND_POWER", "POWER")


def stats_info_main_hand_p_attack(enums: JavaEnums, power: int, main_hand: ItemInfo | None, equipped_items: list[ItemInfo],
                                  passives: list[dict]) -> dict:
	"""
	What SM_STATS_INFO writes as [base main hand attack] and [current main hand attack] (SM_STATS_INFO.java:79, :157):
	pgs.getMainHandPAttack(CalculationType.DISPLAY).getBase() and .getCurrent() (PlayerGameStats.java:151-170), for a fresh character with
	its passive skill effects applied (m5b2-plan.md D2, the X1 assertion of §10.3):
	- a main hand weapon whose ItemTemplate.getAttackType().isMagical() answers `new AdditionStat(PHYSICAL_ATTACK, 0, owner)`: 0 and 0;
	- otherwise the DISPLAY base is WeaponStats.getMeanDamage() = (minDamage + maxDamage) / 2f, and getStat(PHYSICAL_ATTACK, base) applies
	  PhysicalAttackFunction (PlayerStatFunctions.java:51-78: with a main hand weapon `stat.setBaseRate(power * 0.01f)`, power being
	  getPower().getCurrent(), i.e. PlayerStatsTemplate.getPower() = the PlayerClass power) and the passives' PHYSICAL_ATTACK functions - a
	  bonus StatAddFunction is `stat.addToBonus(value)` (StatAddFunction.java:22-27);
	- applyStatFunctions(MAIN_HAND_POWER, stat) applies the weapon mastery: a bonus StatWeaponMasteryFunction whose group is the main hand's
	  is `stat.setFixedBonusRate(value / 100f)` (StatWeaponMasteryFunction.java:45-53);
	- Stat2.getBase() is `(int) (base * baseRate)` and getCurrent() `(int) ((base * baseRate + bonus * bonusRate + base * fixedBonusRate) *
	  finalRate)` in float arithmetic (Stat2.java:30-31, 64-69), bonusRate and finalRate staying 1. StatCapUtil caps PHYSICAL_ATTACK at
	  [0, unlimited] and POWER at [80, 999], which a starting class does not reach.
	Refused with OracleError instead of guessed: no main hand weapon (the no-weapon power multiplier arm), an equipped item whose <modifiers>
	touch PHYSICAL_ATTACK, MAIN_HAND_POWER or POWER, a passive on POWER, a passive on PHYSICAL_ATTACK that is not a bonus StatAddFunction, a
	passive on MAIN_HAND_POWER that is not a bonus StatWeaponMasteryFunction, more than one applying weapon mastery (their order would decide),
	and a power outside the cap.
	"""
	if main_hand is None or not main_hand.has_weapon_stats:
		raise OracleError("no main hand weapon: PhysicalAttackFunction's no-weapon arm is not modelled")
	for item in equipped_items:
		touched = sorted(item.modifier_stats & set(MAIN_HAND_P_ATTACK_STATS))
		if touched:
			raise OracleError(f"an equipped {item.item_group} carries <modifiers> on {touched}, which the main hand attack model does not model")
	# ItemAttackType.isMagical() (the constructor's first argument, read from ItemAttackType.java); a weapon without the attribute throws
	if main_hand.attack_type not in enums.attack_type_magical:
		raise OracleError(f"main hand attack_type {main_hand.attack_type!r}: Java's getAttackType().isMagical() does not answer for it")
	if enums.attack_type_magical[main_hand.attack_type]:
		return {"base": 0, "current": 0}
	if not 80 <= power <= 999:
		raise OracleError(f"power {power} is outside StatCapUtil's [80, 999]")
	bonus = 0.0
	fixed_bonus_rate = 0.0
	masteries = 0
	for function in passives:
		if not function["applies"] or function["stat"] not in MAIN_HAND_P_ATTACK_STATS:
			continue
		if function["stat"] == "POWER":
			raise OracleError(f"skill {function['skillId']}: a passive on POWER is not modelled")
		if function["stat"] == "PHYSICAL_ATTACK":
			if function["function"] != "StatAddFunction" or not function["bonus"]:
				raise OracleError(f"skill {function['skillId']}: a {function['function']} on PHYSICAL_ATTACK is not modelled")
			bonus = f32(bonus + function["value"])
		else:
			if function["function"] != "StatWeaponMasteryFunction" or not function["bonus"]:
				raise OracleError(f"skill {function['skillId']}: a {function['function']} on MAIN_HAND_POWER is not modelled")
			masteries += 1
			fixed_bonus_rate = f32(f32(function["value"]) / f32(100.0))
	if masteries > 1:
		raise OracleError("more than one weapon mastery applies to the main hand; their order is not modelled")
	mean = f32(f32(main_hand.min_damage + main_hand.max_damage) / f32(2.0))
	base_rate = f32(power * f32(0.01))
	based = f32(mean * base_rate)
	current = f32(f32(based + f32(bonus * f32(1.0))) + f32(mean * fixed_bonus_rate))
	return {"base": to_int(based), "current": to_int(f32(current * f32(1.0)))}


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


# ---- the passive skill effects of a fresh character (m5b2-plan.md D2, X1) ---------------------------------------------------------------------

# BufEffect.getModifiers (BufEffect.java:48-71): the function each <change func> becomes and whether it is a bonus function
BUF_FUNCTIONS = {"ADD": ("StatAddFunction", True), "PERCENT": ("StatRateFunction", True), "REPLACE": ("StatSetFunction", False)}
# the effect classes whose startEffect this model follows; every one of them inherits BufEffect.applyEffect (addToEffectedController)
MODELLED_PASSIVE_STARTS = ("BufEffect", "WeaponMasteryEffect", "ArmorMasteryEffect", "ShieldMasteryEffect")
# a function on these moves the value SM_STATS_INFO writes as [base hp] / [base mana]: MAXHP and MAXMP through a non-bonus function, HEALTH
# and WILL through any, because MaxHpFunction/MaxMpFunction add a value computed from their current to the base (PlayerStatFunctions.java:81-115)
BASE_STAT_INPUTS = {"MAXHP": False, "MAXMP": False, "HEALTH": True, "WILL": True}


def _java_int_div(a: int, b: int) -> int:
	"""Java int division: truncated toward zero."""
	q = abs(a) // abs(b)
	return q if (a >= 0) == (b > 0) else -q


class PassiveRules:
	"""What the passive model reads from the Java sources: the effect classes (m5b2 JavaSkillRules), which class of a chain overrides
	startEffect/applyEffect/getModifiers, and StatArmorMasteryFunction.getEquipmentFactor's slot table."""

	def __init__(self, java_src: Path):
		from m5b2.skills import JavaSkillRules  # m5b2.skills imports this module, so not at module level

		self.skill_rules = JavaSkillRules.read(java_src)
		base = Path(java_src) / "com" / "aionemu" / "gameserver"
		self.effect_dir = base / "skillengine" / "effect"
		self._overrides: dict[str, set[str]] = {}
		source = base / "model" / "stats" / "calc" / "functions" / "StatArmorMasteryFunction.java"
		try:
			text = source.read_text(encoding="utf-8")
		except OSError as e:
			raise OracleError(f"{source}: {e}") from e
		body = re.search(r"int getEquipmentFactor\(ItemSlot itemSlot\)\s*\{(.*?)\n\t\}", text, re.DOTALL)
		if not body or not re.search(r"default\s*->\s*0\s*;", body.group(1)):
			raise OracleError(f"{source}: getEquipmentFactor is not the `switch (itemSlot) {{ case ... -> N; default -> 0; }}` this oracle reads")
		self.armor_factors: dict[str, int] = {}
		for slots, factor in re.findall(r"case\s+([A-Z_,\s]+?)\s*->\s*(\d+)\s*;", body.group(1)):
			for slot in slots.split(","):
				self.armor_factors[slot.strip()] = int(factor)

	def overrides(self, cls: str) -> set[str]:
		if cls not in self._overrides:
			source = self.effect_dir / f"{cls}.java"
			try:
				text = source.read_text(encoding="utf-8")
			except OSError as e:
				raise OracleError(f"{source}: {e}") from e
			text = re.sub(r"//[^\n]*", "", re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL))
			self._overrides[cls] = {m for m in ("startEffect", "applyEffect", "getModifiers", "endEffect")
			                        if re.search(rf"\b{m}\s*\(\s*Effect\s+\w+\s*\)\s*\{{", text)}
		return self._overrides[cls]

	def nearest(self, cls: str, method: str) -> str:
		"""The class of the chain whose `method` a template of class `cls` runs."""
		for ancestor in self.skill_rules.class_chain(cls):
			if method in self.overrides(ancestor):
				return ancestor
		raise OracleError(f"effect class {cls}: no {method} in its extends chain")


def _passive_templates(data: StaticData, wanted: set[int]) -> dict[int, tuple[str | None, list[tuple[str, dict, list[tuple[dict, bool]]]]]]:
	"""skill id -> (activation, [(effect tag, attributes, [(<change> attributes, has <conditions>)])]) of the wanted skill templates."""
	templates = {}
	for element in data.stream("skill_data", "skill_template"):
		skill_id = java_int(element.get("skill_id"), "skill_template skill_id")
		if skill_id not in wanted:
			continue
		effects = []
		container = element.find("effects")
		if container is not None:
			for child in container:
				changes = [(dict(c.attrib), c.find("conditions") is not None) for c in child.findall("change")]
				effects.append((child.tag, dict(child.attrib), changes))
		templates[skill_id] = (element.get("activation"), effects)
	return templates


def passive_stat_functions(data: StaticData, enums: JavaEnums, rules: PassiveRules, skills: dict[int, int], equipped: dict[str, str]) -> list[dict]:
	"""
	The stat functions the passive skills of `skills` (id -> level) register at enter world, for a character wearing `equipped` (ItemSlot name
	-> item group), in skill id order and then in document order:
	- SkillTemplate.isPassive is activation="PASSIVE"; EffectTemplate.calculate succeeds unconditionally for a passive skill
	  (EffectTemplate.java:298-301), so every effect Effects.java binds starts. A tag it does not bind is dropped by JAXB;
	- BufEffect.getModifiers: per <change>, value + delta * skillLevel as StatAddFunction / StatRateFunction (bonus) or StatSetFunction (base);
	  a <change> without a stat is skipped with a warning;
	- WeaponMasteryEffect.startEffect: nothing without <change>; a TWO_HAND weapon group keeps the stat, otherwise PHYSICAL_ATTACK and
	  MAGICAL_ATTACK become MAIN_HAND_POWER and OFF_HAND_POWER and every other stat is dropped. StatWeaponMasteryFunction.apply changes
	  MAIN_HAND_POWER (and any other stat) only when the main hand item's group is the mastery's, OFF_HAND_POWER only when the off hand holds a
	  weapon of that group (Equipment.getMainHandWeaponType / getOffHandWeaponType);
	- ArmorMasteryEffect.startEffect: nothing without <change>; the fixed bonus is calculateBaseValue (value + delta * skillLevel of the effect);
	  StatArmorMasteryFunction's equipment factor sums getEquipmentFactor(slot) over the equipped items of the armor's ItemSubType, getValue()
	  is value * factor / 100 in int arithmetic, and apply adds fixedBonus * factor / 100f to the bonus when both are non-zero;
	- ShieldMasteryEffect.startEffect: StatShieldMasteryFunction changes the stat only with a SHIELD in the sub hand (Equipment.isShieldEquipped).
	`applies` is whether the function changes the stat for this equipment. A passive effect whose class starts or applies differently, a
	<change> with <conditions>, or a function that would move SM_STATS_INFO's base max HP/MP is not modelled and raises OracleError.
	"""
	templates = _passive_templates(data, set(skills))
	main_group = equipped.get("MAIN_HAND")
	off_group = equipped.get("SUB_HAND")
	off_weapon = off_group if off_group is not None and enums.item_groups[off_group][1] == "WEAPON" else None
	shield = off_group is not None and enums.item_group_sub_types[off_group] == "SHIELD"
	functions = []
	for skill_id in sorted(skills):
		if skill_id not in templates:
			raise OracleError(f"skill {skill_id} has no skill template")
		activation, effects = templates[skill_id]
		if activation != "PASSIVE":
			continue
		level = skills[skill_id]
		for tag, attrs, changes in effects:
			cls = rules.skill_rules.effect_classes.get(tag)
			if cls is None:
				continue
			starter = rules.nearest(cls, "startEffect")
			if starter not in MODELLED_PASSIVE_STARTS or rules.nearest(cls, "applyEffect") != "BufEffect" \
					or rules.nearest(cls, "getModifiers") != "BufEffect":
				raise OracleError(f"skill {skill_id}: the passive <{tag}> ({cls}) starts in {starter}, which this oracle does not model")
			modifiers = []  # BufEffect.getModifiers: (stat, function, value, bonus)
			for change, has_conditions in changes:
				if has_conditions:
					raise OracleError(f"skill {skill_id}: a <change> of <{tag}> carries <conditions>, which this oracle does not model")
				stat = change.get("stat")
				if stat is None:
					continue
				func = change.get("func")
				if func not in BUF_FUNCTIONS:
					raise OracleError(f"skill {skill_id}: <change func={func!r}> is not ADD, PERCENT or REPLACE")
				value = java_int(change.get("value"), f"skill {skill_id} change value", 0) \
					+ java_int(change.get("delta"), f"skill {skill_id} change delta", 0) * level
				modifiers.append((stat, *BUF_FUNCTIONS[func], value))

			def add(function: str, stat: str, value: int, bonus: bool, applies: bool, **extra) -> None:
				functions.append({"skillId": skill_id, "level": level, "effect": tag, "effectClass": cls, "function": function, "stat": stat,
				                  "value": value, "bonus": bonus, "applies": applies, **extra})

			if starter == "BufEffect":
				for stat, function, bonus, value in modifiers:
					add(function, stat, value, bonus, True)
			elif starter == "WeaponMasteryEffect":
				weapon = attrs.get("weapon")
				if weapon not in enums.item_group_sub_types:
					raise OracleError(f"skill {skill_id}: <{tag} weapon={weapon!r}> is not an ItemGroup")
				for stat, _function, bonus, value in modifiers:
					if enums.item_group_sub_types[weapon] == "TWO_HAND":
						add("StatWeaponMasteryFunction", stat, value, bonus, main_group == weapon, weapon=weapon)
					elif stat in ("PHYSICAL_ATTACK", "MAGICAL_ATTACK"):
						add("StatWeaponMasteryFunction", "MAIN_HAND_POWER", value, bonus, main_group == weapon, weapon=weapon)
						add("StatWeaponMasteryFunction", "OFF_HAND_POWER", value, bonus, off_weapon == weapon, weapon=weapon)
			elif starter == "ArmorMasteryEffect":
				armor = attrs.get("armor")
				if armor is None:
					raise OracleError(f"skill {skill_id}: <{tag}> without armor (Java: NullPointerException comparing the item sub types)")
				fixed_bonus = java_int(attrs.get("value"), f"skill {skill_id} value", 0) + java_int(attrs.get("delta"), f"skill {skill_id} delta", 0) * level
				factor = sum(rules.armor_factors.get(slot, 0) for slot, group in equipped.items() if enums.item_group_sub_types[group] == armor)
				for stat, _function, bonus, value in modifiers:
					rate = _java_int_div(value * factor, 100)
					bonus_add = f32(f32(fixed_bonus * factor) / f32(100.0)) if fixed_bonus != 0 and factor != 0 else 0.0
					add("StatArmorMasteryFunction", stat, rate, bonus, rate != 0 or bonus_add != 0, armor=armor, equipmentFactor=factor,
					    fixedBonus=fixed_bonus, bonusAdded=bonus_add)
			else:  # ShieldMasteryEffect
				for stat, _function, bonus, value in modifiers:
					add("StatShieldMasteryFunction", stat, value, bonus, shield)
	for function in functions:
		moves_base = BASE_STAT_INPUTS.get(function["stat"])
		if function["applies"] and moves_base is not None and (moves_base or not function["bonus"]):
			raise OracleError(f"skill {function['skillId']}: its passive {function['function']} on {function['stat']} moves the base max HP/MP "
			                  "SM_STATS_INFO writes, which this oracle does not model")
	return functions


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
	equipped: dict[str, str] = {}  # ItemSlot name -> item group of what is equipped there, for the passive model
	equipped_infos: dict[str, ItemInfo] = {}  # ItemSlot name -> the template of what is equipped there, for the main hand attack model
	slot_names = {mask: name for name, mask, combo in enums.slots if not combo}
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
			is_equipped = equip in ("ARMOR", "WEAPON")
			slot = enums.slot_for(slots) if is_equipped else 0
			items_out.append({"itemId": item_id, "count": count, "kinah": item_id == KINAH, "equipped": is_equipped, "slot": slot})
			if is_equipped:
				equipped[slot_names[slot]] = info.item_group
				equipped_infos[slot_names[slot]] = info

	# the passive skill effects of enter world (PlayerEnterWorldService.activatePassiveSkillEffects)
	passives = passive_stat_functions(data, enums, PassiveRules(java_src), skills, equipped)

	# SM_STATS_INFO's main hand physical attack with those passives applied (m5b2-plan.md X1); null with the reason where the model refuses
	stats_info: dict = {"mainHandPAttack": None, "maxHp": None, "maxMp": None, "notModelled": []}
	try:
		stats_info["mainHandPAttack"] = stats_info_main_hand_p_attack(enums, enums.powers[player_class], equipped_infos.get("MAIN_HAND"),
		                                                              list(equipped_infos.values()), passives)
	except OracleError as e:
		stats_info["notModelled"].append(f"mainHandPAttack: {e}")
	for key, stat, base in (("maxHp", "MAXHP", stats_info_base_max_hp(health, health_multiplier, 1)),
	                        ("maxMp", "MAXMP", stats_info_base_max_mp(will, will_multiplier, 1))):
		try:
			stats_info[key] = stats_info_current_max(stat, base, list(equipped_infos.values()), passives)
		except OracleError as e:
			stats_info["notModelled"].append(f"{key}: {e}")

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
		# the stat functions the passive skills register at enter world, and whether each changes a stat for the starting equipment
		"passiveStatFunctions": passives,
		# what SM_STATS_INFO writes as [base main hand attack] / [current main hand attack] with those functions applied (m5b2-plan.md X1)
		"statsInfo": stats_info,
	}
