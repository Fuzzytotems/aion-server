"""m5c-craft: what the M5c gate needs to predict a craft or a gather exactly (docs/design/m5c-plan.md §2.6, §2.10, G-01).

Three reports, one JSON format (`aion-m5c-craft`):
- `--recipe ID`: the recipe's materials (every <components_data> alternative), product and count, combo (proc) products with the chance of each
  outcome, the craft skill and minimum level, DP, the craft cooldown, the recipe items that teach it, and, for a crafter at `--skill-level N`
  (default: the recipe's skillpoint), the task: interval and first-tick delay, every bar's step ranges, crit-blue and failure thresholds,
  execution speed and bar delay, tick and time bounds, the packets, and the skill-up (skill xp, the level-up threshold, the player exp);
- `--skill ID --level N`: a profession skill at level N: the xp a level needs, the caps, the master's upgrade cost, the autolearn recipes per race
  (crafting and morph skills), the xp per object level, and the gatherables of a gathering skill (with `--map`, those spawned on that map);
- `--gatherable ID`: one gatherable template: the material roll, the gather task at `--skill-level N` and the skill-up.

Java rules (every body is checked whole by m5c/craft_java.py, which lists each method with its lines in the report's `java` array):
- CraftService.startCrafting / checkCraft / finishCrafting / getBonusReqItem / sendCancelCraft, CM_CRAFT.runImpl;
- CraftingTask (constructor, analyzeInteraction, calculateCrit, onSuccessFinish, onFailureFinish, onInteractionStart, sendInteractionUpdate,
  onInteractionAbort), AbstractCraftTask.onInteraction, AbstractInteractionTask.start/stop/abort and its interval/delay fields;
- GatherableController.startGathering / checkPlayerSkill / getMaterials / completeInteraction / rewardPlayer, GatheringTask (constructor,
  analyzeInteraction, onSuccessFinish, onFailureFinish, onInteractionStart, onInteractionAbort, onInteractionFinish, createGathererObserver);
- PlayerSkillList.addSkillXp, PlayerSkillEntry.is*Skill, SkillLearnService.onLearnSkill, RecipeService.autoLearnRecipes, RecipeData
  (afterUnmarshal, getAutolearnRecipes), RecipeTemplate.getComboProduct(Size), GatherableData.afterUnmarshal, Material.compareTo;
- Profession (constants, getUpgradeCost, getMaxUpgradableLevel, isCrafting), CraftSkillUpdateService (the professionByNpc table, learnSkill),
  StatEnum.getModifier, Rates (SKILL_XP_*, XP_CRAFTING, XP_GATHERING, GATHERING_COUNT, get, calcXpRate, calcResult(int)),
  PlayerCommonData.addExp/setDp/addDp, the bound radii (PlayerAccountData, BoundRadius.DEFAULT, VisibleObjectTemplate.getBoundRadius),
  SM_CRAFT_UPDATE and SM_GATHER_UPDATE constructors; the config fields of CraftConfig, RatesConfig, SecurityConfig and EventsConfig;
- the task constructors (AbstractInteractionTask, AbstractCraftTask), CraftingTask.onInteractionFinish, AbstractInteractionTask.isInProgress/
  setInterval, the RecipeTemplate/ComponentsData/Component/ComboProduct/GatherableTemplate/Material/Materials/ExMaterials getters the model
  reads, PositionUtil.isInRange (3, 4 and 7 arguments), PacketSendUtility.sendPacket/broadcastPacket (2 and 3 arguments), the material
  consumption (Player.getInventory, PlayerStorage/Storage.decreaseByItemId, Storage.decreaseItemCount/getItemCountByItemId,
  ItemStorage.getItemsById, Item.decreaseItemCount), SkillLearnService.sendPacket, SkillTemplate.isPassive and setDp's
  PlayerGameStats.updateStatsAndSpeedVisually chain;
- and WHOLE classes (craft_java.CLASS_PINS): CraftService, CM_CRAFT, CraftingTask, GatheringTask, AbstractCraftTask, AbstractInteractionTask,
  GatherableController and the recipe and gatherable template classes - every member is a checked template or pinned by its digest.

Randomness: Rnd.chance() is taken as uniform on [0, 100) (Rnd.java:32-34) - a probability `P(chance() < t)` is clamp(t, 0, 100) / 100 of the
EXACT float t, reported as an ideal value (the JDK generator's 2^-24 lattice is not modelled); Rnd.nextFloat(1f, 2f) is reported by its extremes
(1f and Math.nextDown(2f)), so every step range is [step at 1f, step at nextDown(2f)]; Rnd.nextInt(10000000) is exact (integer counts); the gather
start delay Rnd.get(200, 600) is reported as its range. What stays exact whatever the rolls: the product and count, the materials, the interval,
the packets' fixed fields, execution speed and bar delay of a NORMAL/CRIT_BLUE update, the skill xp, the level-up and the player exp.

Assumptions (the character a gate creates): no BOOST_*_XP_RATE stat modifier (getStat(stat, 100).getCurrent() == 100), no legion bonus, no
active house (the ESTATE/PALACE crit bonus), account membership `--membership` (default 0), every event disabled (craft_config), the player's
bound radius from PlayerAccountData, and a StaticObject/Gatherable bound radius of 0 (neither ItemTemplate nor GatherableTemplate overrides
VisibleObjectTemplate.getBoundRadius). The player exp is PlayerCommonData.addExp's reward BEFORE the repose and salvation bonuses (0 below level 10
and 15; database state above), and none in world 301200000.

Refused (OracleError, exit 2): an unknown recipe or gatherable id (startCrafting dereferences the recipe first: NullPointerException), a product,
combo or component item without a template, a product or combo without a quality (the quality switches throw), a <comboproduct> without itemid, an
empty <components_data> (get(0) throws), a component of quantity <= 0, an autolearn recipe with max_production_count (deleted, then relearnt on
a level-up), craft type 1 when the bonus item is also a component, duplicate recipe ids, unknown attributes or child elements on the reported
template, craft type 1 for a skill without a bonus item, a skill level below the gatherable's or out of 0..9999, a gatherable whose material roll
can select nothing (then Java passes a null material on) or whose material (or exmaterial) has no item template (ItemService.addItem throws on
success), negative rates or cumulative rates that overflow, a level-up of a profession skill whose activation is not NONE (a passive skill's
effects are applied), an int attribute that is not ASCII decimal, CAPTCHA enabled (its roll and ban are not modelled), an instance map for
--map, and every Java body, class or config shape craft_java / craft_config do not model.
"""

from __future__ import annotations

import math
import re
import struct
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from fractions import Fraction
from pathlib import Path

from staticdata_oracle import OracleError

from m5a.creation import enum_constants
from m5a.data import StaticData, java_int
from m5a.javafloat import f32, round_to_int, to_int, to_long
from m5a.spawns import GameClock, evaluate, is_gatherable, load_groups
from m5c.craft_config import CraftConfigValue, load_craft_config, membership_rate
from m5c.craft_java import BASE, JavaCraftRules

FORMAT = "aion-m5c-craft"
RACES = ("ELYOS", "ASMODIANS")
# packet audiences: PacketSendUtility.sendPacket, broadcastPacket(player, packet, true) and the two-argument broadcastPacket (the known players only)
SELF = "self"
EVERYONE = "self and known players"
OTHERS = "known players only, not the actor"
MAX_SKILL_LEVEL = 9999

RECIPE_ATTRIBUTES = {"id", "nameid", "skillid", "race", "skillpoint", "dp", "autolearn", "productid", "quantity", "max_production_count",
                     "craft_delay_time", "craft_delay_id", "itemid"}
GATHERABLE_ATTRIBUTES = {"id", "name", "nameId", "sourceType", "harvestCount", "skillLevel", "harvestSkill", "successAdj", "failureAdj",
                         "aerialAdj", "captcha", "lvlLimit", "reqItem", "reqItemNameId", "checkType", "eraseValue"}
# `desc` is in the XSD but GatherableTemplate has no field for it: JAXB drops it, and the oracle reports it as ignored, not as unknown
GATHERABLE_IGNORED = {"desc"}


# ------------------------------------------------------------------------------------------------------------------------------------------
# Java arithmetic

def jf(value: int) -> float:
	"""Java's int (or long) to float conversion."""
	return f32(float(value))


def next_down(value: float) -> float:
	"""Math.nextDown(float) of a positive float: the largest float below it."""
	bits = struct.unpack("<I", struct.pack("<f", value))[0]
	return struct.unpack("<f", struct.pack("<I", bits - 1))[0]


def _int32(value: int) -> int:
	return (value + 2**31) % 2**32 - 2**31


def _jdiv(a: int, b: int) -> int:
	"""Java int division: truncation toward zero."""
	q = abs(a) // abs(b)
	return q if (a >= 0) == (b > 0) else -q


def chance_below(threshold: float) -> Fraction:
	"""P(Rnd.chance() < threshold) for Rnd.chance() uniform on [0, 100): clamp(threshold, 0, 100) / 100, of the exact float value."""
	return min(max(Fraction(threshold), Fraction(0)), Fraction(100)) / 100


def _p(value: Fraction | None) -> float | None:
	return None if value is None else float(value)


def rates_calc_result_int(value: int, rate: float) -> int:
	"""Rates.<X>.calcResult(player, int) for the `(long) (value * rate)` constants: a long times a float is float arithmetic, then
	Math.toIntExact, which logs and answers the input when the result leaves the int range (Rates.java:153-161)."""
	result = to_long(f32(jf(value) * rate))
	return result if -2**31 <= result < 2**31 else value


def boost_factor(base: int, divisor: float) -> float:
	"""`getStat(boostStat, 100).getCurrent() / 100f` for a character without a modifier on the stat: the current value is its base."""
	return f32(jf(base) / divisor)


def gained_skill_xp(xp: int, rate: float, boosted: bool, base: int, divisor: float, minimum: int) -> int:
	"""finishCrafting / rewardPlayer: Rates.SKILL_XP_*.calcResult, `gained *= getStat(...).getCurrent() / 100f` (a compound assignment: the
	product in float, cast back to int) when StatEnum.getModifier names a stat, then Math.max(1, gained)."""
	gained = rates_calc_result_int(xp, rate)
	if boosted:
		gained = to_int(f32(jf(gained) * boost_factor(base, divisor)))
	return max(minimum, gained)


def player_exp(xp: int, rate: float, base: int, divisor: float) -> int:
	"""Rates.XP_CRAFTING / XP_GATHERING.calcResult: (long) (xp * calcXpRate), calcXpRate = rate * (boost current / 100f), no legion bonus."""
	end_rate = f32(rate * boost_factor(base, divisor))
	return to_long(f32(jf(xp) * end_rate))


def required_exp(level: int, factor: float, offset: float) -> int:
	"""PlayerSkillList.addSkillXp: (int) (0.23 * (skillLvl + 17.2) * (skillLvl + 17.2)), in double arithmetic."""
	return to_int(factor * (level + offset) * (level + offset))


def add_skill_xp(rules: JavaCraftRules, skill_id: int, level: int, current_xp: int, xp: int, object_level: int, disable_tapping_cap: bool) -> dict:
	"""PlayerSkillList.addSkillXp (the fallthrough switch included): whether the xp is granted, and the level and current xp after it."""
	s = rules.skill
	if level - object_level > s["maxGap"]:
		return {"granted": False, "refusal": f"skill level - object level > {s['maxGap']} (no message; the caller sends "
		                                     "STR_MSG_DONT_GET_PRODUCTION_EXP)", "levelAfter": level, "currentXpAfter": current_xp, "requiredExp": None, "levelsUp": False}
	block = None
	if skill_id == s["humanSkill"]:
		if level == s["humanCap"]:
			return {"granted": False, "refusal": f"human gathering is capped at {s['humanCap']} (the caller sends STR_MSG_DONT_GET_PRODUCTION_EXP)",
			        "levelAfter": level, "currentXpAfter": current_xp, "requiredExp": None, "levelsUp": False}
		block = "tapping"
	if skill_id in (s["tapA"], s["tapB"]) or block == "tapping":
		block = "free" if level == s["tapFree"] or (level >= s["tapCap"] and disable_tapping_cap) else "crafting"
	if (skill_id in s["craftCapSkills"] or block == "crafting") and level in s["capLevels"]:
		return {"granted": False, "refusal": f"level {level} is a cap level: STR_CRAFT_INFO_MAXPOINT_UP, the next grade is bought at a master "
		                                     "or granted by a quest (then the caller sends STR_MSG_DONT_GET_PRODUCTION_EXP)",
		        "levelAfter": level, "currentXpAfter": current_xp, "requiredExp": None, "levelsUp": False, "maxPointMessage": True}
	needed = required_exp(level, s["lvlFactor"], s["lvlOffset"])
	total = _int32(current_xp + xp)
	if total >= needed:
		return {"granted": True, "refusal": None, "requiredExp": needed, "levelAfter": level + 1, "currentXpAfter": 0, "levelsUp": True}
	return {"granted": True, "refusal": None, "requiredExp": needed, "levelAfter": level, "currentXpAfter": total, "levelsUp": False}


def craft_xp_reward(c: dict, skillpoint: int, bonus: int) -> tuple[int, int]:
	"""CraftService.finishCrafting: (int) (0.008 * (skillLvl + 100) * (skillLvl + 100) + 60) in double arithmetic, then + xpReward * bonus / 100."""
	base = to_int(c["xpFactor"] * (skillpoint + c["xpOffset"]) * (skillpoint + c["xpOffset"]) + c["xpBase"])
	return base, _int32(base + _jdiv(_int32(base * bonus), c["bonusDivisor"]))


def gather_xp_reward(g: dict, skill_level: int) -> int:
	"""GatherableController.rewardPlayer: (int) ((0.0031 * (skillLvl + 5.3) * (skillLvl + 1592.8) + 60)) in double arithmetic."""
	return to_int(g["rewardXpFactor"] * (skill_level + g["rewardXpOffset"]) * (skill_level + g["rewardXpOffset2"]) + g["rewardXpBase"])


def fail_reduction(c: dict, diff: int) -> float:
	"""Math.max(1 - skillLvlDiff * 0.015f, 0.25f), float arithmetic."""
	return max(f32(jf(1) - f32(jf(diff) * c["failReductionStep"])), c["failReductionMin"])


def craft_bonus_modifier(c: dict, quality: str) -> float:
	"""CraftingTask.analyzeInteraction's switch over the CURRENT item template's quality (the product, or the combo product after a proc)."""
	return {"LEGEND": c["bmLegend"], "UNIQUE": c["bmUnique"], "EPIC": c["bmEpic"], "MYTHIC": c["bmMythic"]}.get(quality, 1.0)


def _lvl_boni(t: dict, diff: int) -> int:
	return (diff - t["lvlBoniFrom"]) * t["lvlBoniFactor"] if diff > t["lvlBoniFrom"] else 0


def _step_base(t: dict, diff: int, blue: bool) -> float:
	"""(craftType == CRIT_BLUE ? 100 : 0) + (((skillLvlDiff + 1) / 2f) + lvlBoni) * 10, float arithmetic."""
	inner = f32(f32(f32(jf(diff + 1) / t["halfDivisor"]) + jf(_lvl_boni(t, diff))) * jf(t["stepFactor"]))
	return f32(jf(t["blueBonus"] if blue else 0) + inner)


def craft_success_step(c: dict, diff: int, blue: bool, multi: float, modifier: float) -> int:
	"""Math.round(minStep + ((int) (base * multi)) * bonusModifier)."""
	bonus = to_int(f32(_step_base(c, diff, blue) * multi))
	return round_to_int(f32(jf(c["successMinStep"]) + f32(jf(bonus) * modifier)))


def craft_failure_step(c: dict, diff: int, limited: bool, multi: float, modifier: float) -> int:
	"""minStep (70 for a max_production_count recipe, else 120) + (int) (((skillLvlDiff + 1) / 1.5f * 10) * multi) * bonusModifier, rounded."""
	minimum = c["failMinStepLimited"] if limited else c["failMinStep"]
	bonus = to_int(f32(f32(f32(jf(diff + 1) / c["failDivisor"]) * jf(c["failFactor"])) * multi))
	return round_to_int(f32(jf(minimum) + f32(jf(bonus) * modifier)))


def craft_speed(c: dict, diff: int, modifier: float) -> tuple[int, int]:
	"""(executionSpeed, showBarDelay) of analyzeInteraction: both depend only on the level difference and the quality modifier."""
	if modifier < 1:
		speed = round_to_int(f32(jf(c["speedBase"]) * f32(jf(2) - modifier)))
	else:
		speed = c["speedBase"] - diff * c["speedStep"]
	delay = c["delayBase"] if modifier < 1 else max(c["delayMin"], c["delayBase"] - diff * c["delayStep"])
	return max(speed, c["speedMin"]), delay


def gather_success_step(g: dict, diff: int, blue: bool, multi: float) -> int:
	"""Math.round(70 + base * multi) - no (int) cast and no quality modifier, unlike crafting."""
	return round_to_int(f32(jf(g["successMinStep"]) + f32(_step_base(g, diff, blue) * multi)))


def gather_failure_step(g: dict, diff: int, multi: float) -> int:
	"""Math.round(120 + (((skillLvlDiff + 1) / 2f * 10) * multi))."""
	return round_to_int(f32(jf(g["failMinStep"]) + f32(f32(f32(jf(diff + 1) / g["failDivisor"]) * jf(g["failFactor"])) * multi)))


def steps_to_fill(full: int, smallest: int, largest: int) -> tuple[int, int]:
	"""(fewest, most) analyze ticks that bring a bar from 0 to `full` with steps between `smallest` and `largest`."""
	if smallest <= 0:
		raise OracleError(f"a bar step of {smallest} never fills the bar (Java would tick forever)")
	return math.ceil(full / largest), math.ceil(full / smallest)


def material_rolls(materials: list[dict], bound: int) -> tuple[list[int], int]:
	"""
	GatherableController.startGathering: chance = Rnd.nextInt(bound); the first material (in GatherableData's order) whose running sum of rates is
	>= chance. Returns the number of chance values [0, bound) each material wins, and how many select nothing. The first material also wins
	chance 0, and the last one loses chance == bound: with rates summing to exactly `bound` that is (r0 + 1, ..., r_last - 1).
	"""
	counts, previous, current = [], -1, 0
	for material in materials:
		if material["rate"] < 0:
			raise OracleError(f"material {material['itemId']} has a negative rate, which the running sum does not model")
		current += material["rate"]
		if current >= 2**31:
			raise OracleError("the running sum of the material rates overflows an int (Java wraps it), which is not modelled")
		top = min(current, bound - 1)
		counts.append(max(0, top - previous))
		previous = max(previous, top)
	return counts, bound - sum(counts)


# ------------------------------------------------------------------------------------------------------------------------------------------
# Static data

@dataclass
class RecipeInfo:
	id: int
	name_id: int
	skill_id: int
	race: str | None
	skillpoint: int
	dp: int
	autolearn: int
	product_id: int
	quantity: int
	max_production_count: int | None
	craft_delay_time: int | None
	craft_delay_id: int | None
	components: list[list[tuple[int, int]] | None]  # per <components_data>: (itemid, quantity); None: no <component> (getComponent() is null)
	combos: list[int | None]                         # <comboproduct itemid>, None when the attribute is missing
	unknown: list[str] = field(default_factory=list)


_DECIMAL = re.compile(r"[+-]?[0-9]+")


def _int(text: str | None, what: str, default: int | None = None) -> int:
	"""m5a.data.java_int restricted to ASCII decimal digits: Python's int() also reads '1_0' as 10 and takes non-ASCII digits, which an xs:int
	attribute does not allow - such a value is refused, not read."""
	if text is not None and not _DECIMAL.fullmatch(text.strip(" \t\r\n")):
		raise OracleError(f"{what}={text!r} is not a decimal int (ASCII digits with an optional sign)")
	return java_int(text, what, default)


def _opt_int(element: ET.Element, name: str, what: str) -> int | None:
	value = element.get(name)
	return None if value is None else _int(value, f"{what} {name}")


def load_recipes(data: StaticData, races: set[str]) -> dict[int, RecipeInfo]:
	"""RecipeData: every <recipe_template> by id; a second template of one id is refused (HashMap.put keeps the last one but autoLearnRecipes
	keeps both, which is not modelled)."""
	recipes: dict[int, RecipeInfo] = {}
	for element in data.stream("recipe_templates", "recipe_template"):
		what = f"recipe_template {element.get('id')}"
		recipe_id = _int(element.get("id"), f"{what} id", 0)
		race = element.get("race")
		if race is not None and race not in races:
			raise OracleError(f"{what}: race {race!r} is not a Race constant (JAXB leaves it null)")
		components, combos, unknown = [], [], sorted(f"@{a}" for a in element.attrib if a not in RECIPE_ATTRIBUTES)
		for child in element:
			if child.tag == "components_data":
				rows = []
				for component in child:
					if component.tag != "component":
						unknown.append(f"components_data/{component.tag}")
						continue
					rows.append((_int(component.get("itemid"), f"{what} component itemid", 0),
					             _int(component.get("quantity"), f"{what} component quantity", 0)))
					unknown.extend(f"component@{a}" for a in component.attrib if a not in ("itemid", "quantity"))
				components.append(rows or None)
			elif child.tag == "comboproduct":
				combos.append(_opt_int(child, "itemid", what))
				unknown.extend(f"comboproduct@{a}" for a in child.attrib if a != "itemid")
			else:
				unknown.append(child.tag)
		info = RecipeInfo(recipe_id, _int(element.get("nameid"), f"{what} nameid", 0), _int(element.get("skillid"), f"{what} skillid", 0),
		                  race, _int(element.get("skillpoint"), f"{what} skillpoint", 0), _int(element.get("dp"), f"{what} dp", 0),
		                  _int(element.get("autolearn"), f"{what} autolearn", 0), _int(element.get("productid"), f"{what} productid", 0),
		                  _int(element.get("quantity"), f"{what} quantity", 0), _opt_int(element, "max_production_count", what),
		                  _opt_int(element, "craft_delay_time", what), _opt_int(element, "craft_delay_id", what), components, combos, unknown)
		if recipe_id in recipes:
			raise OracleError(f"two recipe_templates with id {recipe_id}: RecipeData would keep the last one in its map but both in its autolearn list, "
			                  "which the oracle does not model")
		recipes[recipe_id] = info
	return recipes


def autolearn_recipes(recipes: dict[int, RecipeInfo], race: str, skill_id: int, max_level: int) -> list[RecipeInfo]:
	"""RecipeData.getAutolearnRecipes: autolearn != 0, the skill, skillpoint <= maxLevel, race PC_ALL or the player's race, in document order."""
	return [r for r in recipes.values() if r.autolearn != 0 and r.skill_id == skill_id and r.skillpoint <= max_level
	        and (r.race == "PC_ALL" or r.race == race)]


def load_item_index(data: StaticData) -> tuple[dict[int, tuple], dict[int, list[int]]]:
	"""Every item template as (name, quality attribute, price attribute, race attribute) - raw, checked when an item is used - and, per recipe id,
	the items whose <actions><craftlearn recipeid> teaches it (CraftLearnAction). A later template of one id replaces the earlier one (ItemData)."""
	items: dict[int, tuple] = {}
	teachers: dict[int, list[int]] = {}
	for element in data.stream("item_templates", "item_template"):
		item_id = _int(element.get("id"), "item_template id", 0)
		items[item_id] = (element.get("name"), element.get("quality"), element.get("price"), element.get("race"))
		actions = element.find("actions")
		if actions is not None:
			for action in actions.findall("craftlearn"):
				teachers.setdefault(_int(action.get("recipeid"), f"item {item_id} craftlearn recipeid", 0), []).append(item_id)
	return items, teachers


def item_info(items: dict[int, tuple], item_id: int, qualities: set[str]) -> dict:
	if item_id not in items:
		raise OracleError(f"item {item_id} has no item_template")
	name, quality, price, race = items[item_id]
	if quality is not None and quality not in qualities:
		raise OracleError(f"item {item_id}: quality {quality!r} is not an ItemQuality constant")
	return {"itemId": item_id, "name": name, "quality": quality, "templatePrice": _int(price, f"item {item_id} price", 0),
	        "race": race or "PC_ALL"}


def load_skill_names(data: StaticData, low: int, high: int) -> tuple[dict[int, str | None], dict[int, str | None]]:
	"""The names and `activation` attributes of the skill templates with low <= id < high (the profession skills of
	PlayerSkillEntry.isProfessionSkill)."""
	names: dict[int, str | None] = {}
	activations: dict[int, str | None] = {}
	for element in data.stream("skill_data", "skill_template"):
		skill_id = _int(element.get("skill_id"), "skill_template skill_id")
		if low <= skill_id < high:
			names[skill_id] = element.get("name")
			activations[skill_id] = element.get("activation")
	return names, activations


@dataclass
class GatherableInfo:
	id: int
	name: str | None
	name_id: int
	source_type: str | None
	harvest_count: int
	skill_level: int
	harvest_skill: int
	lvl_limit: int
	captcha: int
	req_item: int
	req_item_name_id: int
	check_type: int
	erase_value: int
	materials: list[dict] | None     # GatherableData order (stable descending rate); None: no <materials>
	exmaterials: list[dict] | None
	unknown: list[str] = field(default_factory=list)
	ignored: list[str] = field(default_factory=list)


def _materials(container: ET.Element | None, what: str, unknown: list[str]) -> list[dict] | None:
	if container is None:
		return None
	rows = []
	for position, material in enumerate(container):
		if material.tag != "material":
			unknown.append(f"{container.tag}/{material.tag}")
			continue
		unknown.extend(f"material@{a}" for a in material.attrib if a not in ("name", "itemid", "nameid", "rate"))
		rows.append({"itemId": _int(material.get("itemid"), f"{what} material itemid", 0), "name": material.get("name"),
		             "nameId": _int(material.get("nameid"), f"{what} material nameid", 0),
		             "rate": _int(material.get("rate"), f"{what} material rate", 0), "documentPosition": position})
	return sorted(rows, key=lambda m: -m["rate"])  # List.sort(null) with Material.compareTo = o.rate - rate: stable, descending


def load_gatherables(data: StaticData) -> dict[int, GatherableInfo]:
	gatherables: dict[int, GatherableInfo] = {}
	for element in data.stream("gatherable_templates", "gatherable_template"):
		what = f"gatherable_template {element.get('id')}"
		gid = _int(element.get("id"), f"{what} id", 0)
		unknown = sorted(f"@{a}" for a in element.attrib if a not in GATHERABLE_ATTRIBUTES | GATHERABLE_IGNORED)
		ignored = sorted(f"@{a}" for a in element.attrib if a in GATHERABLE_IGNORED)
		containers: dict[str, list[ET.Element]] = {}
		for child in element:
			if child.tag in ("materials", "exmaterials"):
				containers.setdefault(child.tag, []).append(child)
			else:
				unknown.append(child.tag)
		for tag, found in containers.items():
			if len(found) > 1:
				unknown.append(f"{len(found)} <{tag}> elements")
		get = lambda name: _int(element.get(name), f"{what} {name}", 0)  # noqa: E731
		info = GatherableInfo(gid, element.get("name"), get("nameId"), element.get("sourceType"), get("harvestCount"), get("skillLevel"),
		                      get("harvestSkill"), get("lvlLimit"), get("captcha"), get("reqItem"), get("reqItemNameId"), get("checkType"),
		                      get("eraseValue"), _materials((containers.get("materials") or [None])[-1], what, unknown),
		                      _materials((containers.get("exmaterials") or [None])[-1], what, unknown), unknown, ignored)
		gatherables[gid] = info  # GatherableData: HashMap.put, a later template of the same id replaces the earlier one
	return gatherables


def skill_tree_rows(data: StaticData, skill_id: int) -> list[dict]:
	return [dict(row.attrib) for row in data.children("skill_tree", "skill") if _int(row.get("skillId"), "skill skillId", 0) == skill_id]


# ------------------------------------------------------------------------------------------------------------------------------------------
# Context

@dataclass
class CraftContext:
	data: StaticData
	java_src: Path
	rules: JavaCraftRules
	config: dict[tuple[str, str], CraftConfigValue]
	membership: int
	races: set[str]
	qualities: set[str]
	_recipes: dict[int, RecipeInfo] | None = None
	_gatherables: dict[int, GatherableInfo] | None = None
	_items: tuple[dict[int, tuple], dict[int, list[int]]] | None = None
	_skill_names: tuple[dict[int, str | None], dict[int, str | None]] | None = None

	@staticmethod
	def create(data: StaticData, java_src: Path, config_dir: Path | None, profile: Path | None, overrides: list[str] = (),
	           membership: int = 0) -> "CraftContext":
		if not 0 <= membership <= 127:
			raise OracleError(f"membership {membership}: an account's membership is a byte of 0..127")
		rules = JavaCraftRules.read(java_src)
		config = load_craft_config(rules.config_fields, config_dir, profile, overrides)
		base = Path(java_src).joinpath(*BASE)
		races = {name for name, _ in enum_constants(base / "model" / "Race.java", "Race")}
		qualities = {name for name, _ in enum_constants(base / "model" / "templates" / "item" / "ItemQuality.java", "ItemQuality")}
		return CraftContext(data, Path(java_src), rules, config, membership, races, qualities)

	def value(self, cls: str, field_name: str):
		return self.config[(cls, field_name)].value

	def rate(self, field_name: str) -> float:
		return membership_rate(self.value("RatesConfig", field_name), self.membership)

	@property
	def recipes(self) -> dict[int, RecipeInfo]:
		if self._recipes is None:
			self._recipes = load_recipes(self.data, self.races)
		return self._recipes

	@property
	def gatherables(self) -> dict[int, GatherableInfo]:
		if self._gatherables is None:
			self._gatherables = load_gatherables(self.data)
		return self._gatherables

	def item(self, item_id: int) -> dict:
		if self._items is None:
			self._items = load_item_index(self.data)
		return item_info(self._items[0], item_id, self.qualities)

	def recipe_items(self, recipe_id: int) -> list[dict]:
		if self._items is None:
			self._items = load_item_index(self.data)
		return [self.item(item_id) for item_id in sorted(self._items[1].get(recipe_id, []))]

	def skill_names(self) -> dict[int, str | None]:
		if self._skill_names is None:
			self._skill_names = load_skill_names(self.data, self.rules.skill["profMin"], self.rules.skill["profMax"])
		return self._skill_names[0]

	def skill_activations(self) -> dict[int, str | None]:
		self.skill_names()
		return self._skill_names[1]

	def skill_kind(self, skill_id: int) -> str:
		s = self.rules.skill
		if s["tapMin"] <= skill_id <= s["tapMax"]:
			return "gathering"
		if skill_id == s["morphSkill"]:
			return "morph"
		if s["craftMin"] <= skill_id <= s["craftMax"]:
			return "crafting"
		return "profession" if s["profMin"] <= skill_id < s["profMax"] else "other"

	def profession(self, skill_id: int) -> str | None:
		"""Profession.getBySkillId."""
		for name, profession_skill in self.rules.professions.items():
			if profession_skill == skill_id:
				return name
		return None

	def header(self, mode: str) -> dict:
		return {
			"format": FORMAT,
			"version": 2,
			"mode": mode,
			"membership": self.membership,
			"config": {key: value.as_json() for key, value in ((v.key, v) for v in self.config.values())},
			"assumptions": [
				"no BOOST_*_XP_RATE stat modifier on the character (the current value is the base 100)",
				"no legion bonus (Rates.calcXpRate) and no active ESTATE or PALACE house (CraftingTask.calculateCrit)",
				"every timed event disabled (gameserver.event.service.disabled_events = *)",
				"Rnd.chance() uniform on [0, 100); probabilities are ideal values of the exact float thresholds",
				f"player bound radius {self.rules.player_bound} (PlayerAccountData), crafting station and gatherable bound radius "
				f"{self.rules.object_bound} (BoundRadius.DEFAULT)",
			],
			"java": [v.as_json() for v in self.rules.verified],
		}


def _skill_block(ctx: CraftContext, skill_id: int, names: dict[int, str | None]) -> dict:
	return {"skillId": skill_id, "name": names.get(skill_id), "kind": ctx.skill_kind(skill_id), "profession": ctx.profession(skill_id),
	        "boostStat": ctx.rules.boost_stats.get(skill_id)}


def _check_level(level: int, what: str) -> None:
	if not 0 <= level <= MAX_SKILL_LEVEL:
		raise OracleError(f"{what} {level} is outside 0..{MAX_SKILL_LEVEL}")


def skill_up(ctx: CraftContext, skill_id: int, skill_level: int, current_xp: int, object_level: int, xp_reward: int, kind: str) -> dict:
	"""finishCrafting (kind 'crafting') or rewardPlayer (kind 'gathering') from the skill xp on: gained xp, addSkillXp, the player exp."""
	rules = ctx.rules
	t = rules.craft if kind == "crafting" else {"boostBase": rules.gather["rewardBoostBase"], "boostDivisor": rules.gather["rewardBoostDivisor"],
	                                            "minXp": rules.gather["rewardMinXp"]}
	skill_rate = ctx.rate("SKILL_XP_CRAFTING_RATES" if kind == "crafting" else "SKILL_XP_GATHERING_RATES")
	boosted = skill_id in rules.boost_stats
	gained = gained_skill_xp(xp_reward, skill_rate, boosted, t["boostBase"], t["boostDivisor"], t["minXp"])
	result = add_skill_xp(rules, skill_id, skill_level, current_xp, gained, object_level,
	                      ctx.value("CraftConfig", "DISABLE_AETHER_AND_ESSENCE_TAPPING_CAP"))
	exp_rate = ctx.rate("XP_CRAFTING_RATES" if kind == "crafting" else "XP_GATHERING_RATES")
	exp = player_exp(xp_reward, exp_rate, rules.rates["boostBase"], rules.rates["boostDivisor"]) if result["granted"] else None
	learned = None
	if result["granted"] and result["levelAfter"] > skill_level and ctx.skill_kind(skill_id) in ("crafting", "morph"):
		learned = {race: [r.id for r in autolearn_recipes(ctx.recipes, race, skill_id, result["levelAfter"]) if r.skillpoint > skill_level]
		           for race in RACES}
	return {
		"skillLevel": skill_level,
		"currentXp": current_xp,
		"objectLevel": object_level,
		"skillXpRate": skill_rate,
		"boostStat": rules.boost_stats.get(skill_id),
		"gainedSkillXp": gained,
		**result,
		"playerExp": None if exp is None else {"value": xp_reward, "rate": exp_rate, "reward": exp,
		                                       "message": "STR_GET_EXP2(reward) without repose and salvation energy"},
		"autolearnedOnLevelUp": learned,
		"packets": skill_up_packets(ctx, skill_id, kind, result, exp),
	}


def skill_up_packets(ctx: CraftContext, skill_id: int, kind: str, result: dict, exp: int | None) -> list[dict]:
	"""
	The packets of addSkillXp and its caller, in order: at a cap level STR_CRAFT_INFO_MAXPOINT_UP, then STR_MSG_DONT_GET_PRODUCTION_EXP when no
	xp is granted; on a level-up SkillLearnService.onLearnSkill (the CRAFT_LEVEL_UP animation at its levels, SM_SKILL_LIST with the level-up
	message while the player is spawned, updateNearbyQuests at 399 and 499, the autolearn recipes); then rewardPlayer's
	STR_EXTRACT_GATHERING_SUCCESS_GETEXP and addExp's STR_GET_EXP2.
	"""
	s = ctx.rules.skill
	if not result["granted"]:
		cap = [{"packet": "SM_SYSTEM_MESSAGE", "to": SELF, "message": "STR_CRAFT_INFO_MAXPOINT_UP"}] if result.get("maxPointMessage") else []
		return cap + [{"packet": "SM_SYSTEM_MESSAGE", "to": SELF, "message": "STR_MSG_DONT_GET_PRODUCTION_EXP", "skillId": skill_id}]
	packets = []
	if result["levelsUp"]:
		level = result["levelAfter"]
		activation = ctx.skill_activations().get(skill_id)
		if activation != "NONE":
			raise OracleError(f"skill {skill_id} has activation {activation!r}: onLearnSkill applies a passive skill's effects on a level-up "
			                  "(SkillTemplate.isPassive), which the oracle does not model")
		if level in s["levelUpAnimation"] and (level != s["levelUpAnimationCraftingOnly"] or ctx.skill_kind(skill_id) == "crafting"):
			packets.append({"packet": "SM_ACTION_ANIMATION", "to": EVERYONE, "animation": "CRAFT_LEVEL_UP"})
		message = s["skillListMessage"]["tapping" if ctx.skill_kind(skill_id) == "gathering" else "other"]
		packets.append({"packet": "SM_SKILL_LIST", "to": SELF, "skillId": skill_id, "level": level, "message": message,
		                "when": "the player is spawned"})
		if level in s["nearbyQuestLevels"]:
			packets.append({"note": "PlayerController.updateNearbyQuests (its quest packets are not modelled)"})
		if ctx.skill_kind(skill_id) in ("crafting", "morph"):
			packets.append({"note": "RecipeService.autoLearnRecipes: RecipeList.addRecipe for every autolearnedOnLevelUp recipe (its packets are not "
			                        "modelled)"})
	if kind == "gathering":
		packets.append({"packet": "SM_SYSTEM_MESSAGE", "to": SELF, "message": "STR_EXTRACT_GATHERING_SUCCESS_GETEXP"})
	packets.append({"packet": "SM_SYSTEM_MESSAGE", "to": SELF, "message": "STR_GET_EXP2", "exp": exp,
	                "when": f"no repose or salvation energy (else a VITAL/MAKEUP variant), not in world {ctx.rules.rates['noExpWorld']}"})
	return packets


# ------------------------------------------------------------------------------------------------------------------------------------------
# Crafting

# checkCraft's refusals in order: (the check, the system message checkCraft sends first or None, whether AuditLogger logs it). Each one returns
# false to startCrafting, which then sends the cancel pair (sendCancelCraft, packets.refused) - so a refusal without a message still sends
# packets. A missing recipe or product template never gets that far: startCrafting and sendCancelCraft dereference it (NullPointerException).
CRAFT_CHECKS = [
	("a CraftingTask of the player in progress", None, False),
	("no StaticObject target (not for the morph skill)", None, True),
	("the station out of range (not for the morph skill)", "STR_COMBINE_TOO_FAR_FROM_TOOL", False),
	("DP below the recipe's dp (a starting class has 0 DP: every dp > 0 recipe)", None, True),
	("riding or hidden", "STR_SKILL_CAN_NOT_COMBINE_WHILE_IN_CURRENT_STANCE", False),
	("inventory full", "STR_COMBINE_INVENTORY_IS_FULL", False),
	("recipe not learned", "STR_COMBINE_CAN_NOT_FIND_RECIPE", False),
	("craft cooldown running", "STR_ITEM_CANT_USE_UNTIL_DELAY_TIME", False),
	("craft skill missing", "STR_COMBINE_CANT_USE", False),
	("skill level below the skillpoint", "STR_COMBINE_OUT_OF_SKILL_POINT", False),
	("an item of the selected alternative held below one of its component quantities", "STR_COMBINE_NO_COMPONENT_ITEM_SINGLE / _MULTIPLE", False),
	("craft type 1 without the bonus item (it is consumed here, before the materials)", "STR_COMBINE_NO_COMPONENT_ITEM_SINGLE", False),
]
NOT_STARTING_CLASS = "the crafter is not a starting class (on every craft that starts, dp 0 included; a starting class gets none of these)"


def material_alternatives(components: list[list[tuple[int, int]]]) -> list[dict]:
	"""
	checkCraft over the <components_data> alternatives: the FIRST one whose first item id is a key of the CM_CRAFT materials map is selected, so an
	alternative whose first item an earlier one already has can never be selected (shadowedBy). Each component of the selected alternative is
	compared on its own with the whole stack (getItemCountByItemId), so an item named twice needs only its largest quantity (`required`); then
	each component gets its own decreaseByItemId, which takes what is left up to the quantity and whose false return is ignored, so an item
	named twice loses min(held, the sum of its quantities) (`consumed` is that sum).
	"""
	first_seen: dict[int, int] = {}
	result = []
	for index, rows in enumerate(components):
		first = rows[0][0]
		shadowed = first_seen.get(first)
		first_seen.setdefault(first, index)
		items: dict[int, dict] = {}
		for item_id, quantity in rows:
			entry = items.setdefault(item_id, {"itemId": item_id, "required": 0, "consumed": 0, "components": 0})
			entry["required"] = max(entry["required"], quantity)
			entry["consumed"] += quantity
			entry["components"] += 1
		result.append({"index": index, "firstItemId": first, "selectable": shadowed is None, "shadowedBy": shadowed,
		               "components": [{"itemId": i, "quantity": q} for i, q in rows], "items": list(items.values())})
	return result


def _bar(ctx: CraftContext, recipe: RecipeInfo, diff: int, item: dict, crit_count: int, morph: bool) -> dict:
	c = ctx.rules.craft
	full = c["fullBar"]
	if morph:
		return {"critCount": crit_count, "itemId": item["itemId"], "quality": item["quality"], "instant": True,
		        "note": "the morph skill fills the success bar on the first tick without a roll; that update carries speed 0 and delay "
		                f"{c['packetMorphDelay']} (SM_CRAFT_UPDATE writes it for the morph skill)",
		        "steps": {"fewest": 1, "most": 1, "mostToAnyEnd": 1}, "executionSpeed": 0, "showBarDelay": c["packetMorphDelay"]}
	modifier = craft_bonus_modifier(c, item["quality"])
	top = next_down(c["multiMax"])
	threshold = f32(jf(ctx.value("CraftConfig", "MAX_CRAFT_FAILURE_CHANCE")) * fail_reduction(c, diff))
	always = diff >= c["alwaysSuccessDiff"]
	success = Fraction(1) if always else 1 - chance_below(threshold)
	blue_threshold = f32(jf(c["blueBase"]) + f32(jf(diff) / c["blueDivisor"]))
	blue = chance_below(blue_threshold)
	normal_steps = [craft_success_step(c, diff, False, c["multiMin"], modifier), craft_success_step(c, diff, False, top, modifier)]
	blue_steps = [craft_success_step(c, diff, True, c["multiMin"], modifier), craft_success_step(c, diff, True, top, modifier)]
	failure_steps = [craft_failure_step(c, diff, recipe.max_production_count is not None, c["multiMin"], modifier),
	                 craft_failure_step(c, diff, recipe.max_production_count is not None, top, modifier)]
	possible = ([normal_steps] if blue < 1 else []) + ([blue_steps] if blue > 0 else [])
	fewest, most = steps_to_fill(full, min(s[0] for s in possible), max(s[1] for s in possible))
	speed, delay = craft_speed(c, diff, modifier)
	steps = {"fewest": fewest, "most": most, "mostToAnyEnd": most}
	if success < 1:
		fail_fewest, fail_most = steps_to_fill(full, failure_steps[0], failure_steps[1])
		steps.update({"fewestToFailure": fail_fewest, "mostToAnyEnd": (most - 1) + (fail_most - 1) + 1})
	return {
		"critCount": crit_count,
		"itemId": item["itemId"],
		"name": item["name"],
		"quality": item["quality"],
		"bonusModifier": modifier,
		"failureThreshold": threshold,
		"alwaysSuccess": always,
		"successPerTick": _p(success),
		"critBlueThreshold": blue_threshold,
		"critBluePerSuccess": _p(blue),
		"successStep": {"normal": normal_steps, "critBlue": blue_steps},
		"failureStep": failure_steps,
		"multi": [c["multiMin"], top],
		"executionSpeed": speed,
		"showBarDelay": delay,
		"steps": steps,
	}


def recipe_report(ctx: CraftContext, recipe_id: int, skill_level: int | None = None, craft_type: int = 0, current_xp: int = 0) -> dict:
	recipe = ctx.recipes.get(recipe_id)
	if recipe is None:
		raise OracleError(f"no recipe_template {recipe_id} (CraftService.startCrafting dereferences the missing template: NullPointerException)")
	if recipe.unknown:
		raise OracleError(f"recipe {recipe_id}: {', '.join(recipe.unknown)} - JAXB ignores them, the oracle does not model what they mean")
	for index, rows in enumerate(recipe.components):
		if rows is None:
			raise OracleError(f"recipe {recipe_id}: <components_data> #{index} has no <component> (checkCraft's getComponent().get(0) throws)")
	if any(combo is None for combo in recipe.combos):
		raise OracleError(f"recipe {recipe_id}: a <comboproduct> without itemid (a proc would switch to item 0, which has no template)")
	if craft_type not in (0, 1):
		raise OracleError(f"craft type {craft_type}: CM_CRAFT sends a byte, and checkCraft/startCrafting only distinguish 1 from the rest; pass 0 or 1")
	if recipe.craft_delay_id is not None and recipe.craft_delay_time is None:
		raise OracleError(f"recipe {recipe_id}: craft_delay_id without craft_delay_time (finishCrafting unboxes null: NullPointerException)")
	for rows in recipe.components:
		for item_id, quantity in rows:
			if quantity <= 0:
				raise OracleError(f"recipe {recipe_id}: component {item_id} has quantity {quantity} (Item.decreaseItemCount takes nothing for a count "
				                  "<= 0, and the packets of such a decrease are not modelled)")
	if recipe.autolearn != 0 and recipe.max_production_count is not None:
		raise OracleError(f"recipe {recipe_id}: autolearn with max_production_count - finishCrafting deletes the recipe, and a level-up in the same "
		                  "craft relearns it (onLearnSkill -> autoLearnRecipes), which the oracle does not model")
	c = ctx.rules.craft
	morph = recipe.skill_id == c["morphSkill"]
	bonus_item = ctx.rules.bonus_items.get(recipe.skill_id, 0)
	if craft_type == c["bonusCraftType"] and bonus_item == 0:
		raise OracleError(f"craft type 1 for skill {recipe.skill_id}: getBonusReqItem answers 0, and the refusal message dereferences item 0's "
		                  "template (NullPointerException)")
	if craft_type == c["bonusCraftType"] and any(item_id == bonus_item for rows in recipe.components for item_id, _ in rows):
		raise OracleError(f"recipe {recipe_id}: the bonus item {bonus_item} is also a component - craft type 1 takes it between the component check "
		                  "and the consumption, which the oracle does not model")
	wanted = {recipe.product_id, *recipe.combos, *(i for rows in recipe.components for i, _ in rows)} | ({bonus_item} if bonus_item else set())
	items = {}
	for item_id in sorted(wanted):
		try:
			items[item_id] = ctx.item(item_id)
		except OracleError as e:
			raise OracleError(f"recipe {recipe_id}: {e}") from e
	for item_id in (recipe.product_id, *recipe.combos):
		if items[item_id]["quality"] is None:
			raise OracleError(f"recipe {recipe_id}: item {item_id} has no quality (the quality switches of startCrafting/analyzeInteraction throw)")
	names = ctx.skill_names()
	if recipe.skill_id not in names:
		raise OracleError(f"recipe {recipe_id}: skill {recipe.skill_id} has no skill_template (the refusal messages dereference it)")

	level = max(1, recipe.skillpoint) if skill_level is None else skill_level  # level 0 would be "skill not learned"
	_check_level(level, "--skill-level")
	product = items[recipe.product_id]
	report = ctx.header("recipe")
	report["recipe"] = {
		"id": recipe.id,
		"nameId": recipe.name_id,
		"skill": _skill_block(ctx, recipe.skill_id, names),
		"race": recipe.race,
		"skillpoint": recipe.skillpoint,
		"dp": recipe.dp,
		"autolearn": recipe.autolearn,
		"maxProductionCount": recipe.max_production_count,
		"craftDelayId": recipe.craft_delay_id,
		"craftDelayTime": recipe.craft_delay_time,
		"components": [[{"itemId": i, "quantity": q, "name": items[i]["name"]} for i, q in rows] for rows in recipe.components],
		"product": {"itemId": recipe.product_id, "quantity": recipe.quantity, "name": product["name"], "quality": product["quality"]},
		"comboProducts": [{"critCount": k + 1, "itemId": item_id, "quantity": recipe.quantity, "name": items[item_id]["name"],
		                   "quality": items[item_id]["quality"]} for k, item_id in enumerate(recipe.combos)],
		"kinahCost": 0,
	}
	report["learn"] = {
		"autolearn": recipe.autolearn != 0,
		"autolearnRaces": (list(RACES) if recipe.race == "PC_ALL" else [recipe.race] if recipe.race in RACES else []) if recipe.autolearn else [],
		"autolearnAtSkillLevel": recipe.skillpoint if recipe.autolearn else None,
		"rule": "SkillLearnService.onLearnSkill -> RecipeService.autoLearnRecipes(player, skillId, level) whenever a crafting or morph skill is "
		        "learnt or levels up: every autolearn recipe of the skill with skillpoint <= level and race PC_ALL or the player's",
		"recipeItems": ctx.recipe_items(recipe_id),
		"recipeItemRule": "CraftLearnAction: RecipeService.validateNewRecipe (1600 recipes, race, not known, skill present, skillpoint <= level); "
		                  "the kinah of buying the item is PricesService arithmetic (the m5c-trade oracle), not a craft cost",
	}
	crit_rates = [ctx.rate("CRAFT_CRIT_CHANCES"), ctx.rate("CRAFT_COMBO_CHANCES")]
	report["craft"] = _craft_block(ctx, recipe, level, craft_type, bonus_item, items, morph, crit_rates)
	xp, xp_bonus = craft_xp_reward(c, recipe.skillpoint, c["bonusPercent"] if craft_type == c["bonusCraftType"] else 0)
	report["skillUp"] = {"xpReward": xp, "xpRewardWithBonus": xp_bonus,
	                     **skill_up(ctx, recipe.skill_id, level, current_xp, recipe.skillpoint, xp_bonus, "crafting")} \
		if report["craft"]["refusal"] is None else None
	report["afterCraft"] = {
		"productAddedAfterSkillUp": True,
		"recipeDeleted": recipe.max_production_count is not None,
		"cooldownId": recipe.craft_delay_id,
		"cooldownMillis": _int32(recipe.craft_delay_time * c["delayMillis"]) if recipe.craft_delay_id is not None and recipe.craft_delay_time is not None
		else None,
		"creatorNameSet": "only on a weapon or armor product (ItemUpdatePredicate.changeItem)",
	}
	return report


def _craft_block(ctx: CraftContext, recipe: RecipeInfo, level: int, craft_type: int, bonus_item: int, items: dict, morph: bool,
                 crit_rates: list[float]) -> dict:
	c = ctx.rules.craft
	block = {
		"skillLevel": level,
		"skillLvlDiff": level - recipe.skillpoint,
		"craftType": craft_type,
		"bonusPercent": c["bonusPercent"] if craft_type == c["bonusCraftType"] else 0,
		"bonusItem": {"itemId": bonus_item, "name": items[bonus_item]["name"], "consumed": craft_type == c["bonusCraftType"]} if bonus_item else None,
		"checks": [{"check": check, "message": message, "auditLog": audit, "then": "the cancel pair (packets.refused)"}
		           for check, message, audit in CRAFT_CHECKS],
		"checksNote": "a missing recipe or product template is no refusal: startCrafting and sendCancelCraft dereference it (NullPointerException, "
		              "no packet); the oracle refuses such a recipe",
		"station": None if morph else {
			"range": c["stationRange"], "centerToCenter": False, "playerBoundRadius": ctx.rules.player_bound, "stationBoundRadius": ctx.rules.object_bound,
			"effectiveRange": f32(f32(jf(c["stationRange"]) + ctx.rules.player_bound) + ctx.rules.object_bound),
			"packetRange": c["packetRange"], "packetCenterToCenter": True,
			"comparison": "PositionUtil.isInRange: same world and instance, then dx*dx + dy*dy + dz*dz < range*range in float arithmetic (strict), "
			              "range = effectiveRange (checkCraft, isInRange(player, target, range, false)) or packetRange (CM_CRAFT, center to center)",
			"stationTypeChecked": False,
			"note": "any StaticObject works: CM_CRAFT only compares the client's template id with the target's, checkCraft only wants a StaticObject",
		},
		"morphPacketUnk": c["morphUnk"] if morph else None,
		"materials": {
			"rule": "checkCraft takes the FIRST <components_data> whose first component's item id is a key of the CM_CRAFT materials map (the counts "
			        "the client sends are ignored; a later alternative with the same first item is never taken). The craft goes on when every item "
			        "of it is held at least `required` times (each component is compared on its own; the first short one, in document order, "
			        "names the message), and after the other checks each item loses min(held, `consumed`); when no alternative's first item is "
			        "sent, nothing is checked or consumed",
			"alternatives": material_alternatives(recipe.components),
		},
		"dp": {"required": recipe.dp, "spent": recipe.dp, "refusedForStartingClass": recipe.dp > 0,
		       "note": "getDp() is never null (RecipeTemplate.getDp boxes an int): startCrafting calls addDp(-dp) on every craft that passes "
		               "checkCraft. setDp returns at once for a starting class (no DP change, no packet); PlayerDAO loads the dp through setDp too, "
		               "so a starting class has 0 DP and checkCraft refuses every dp > 0 recipe: an AuditLogger line and no system message, but the "
		               "cancel pair (packets.refused) is still sent. For any other class setDp sends the DP packets of packets.start, dp 0 included"},
		"refusal": None,
	}
	if level == 0:
		block["refusal"] = "STR_COMBINE_CANT_USE (checkCraft: skill level 0 means the craft skill is not learned)"
		return block
	if level < recipe.skillpoint:
		block["refusal"] = "STR_COMBINE_OUT_OF_SKILL_POINT (checkCraft)"
		return block
	diff = level - recipe.skillpoint
	product = items[recipe.product_id]
	cap = c["capUniqueEpic"] if product["quality"] in ("UNIQUE", "EPIC") else c["capMythic"] if product["quality"] == "MYTHIC" else c["capDefault"]
	interval = c["morphInterval"] if morph else max(cap, c["intervalBase"] - diff * c["intervalStep"])
	chain = [items[recipe.product_id]] + [items[i] for i in recipe.combos]
	bars = [_bar(ctx, recipe, diff, item, k, morph) for k, item in enumerate(chain)]
	block["timing"] = {"firstTickDelay": c["delay"], "interval": interval, "intervalCap": None if morph else cap,
	                   "rule": "AbstractInteractionTask.start: scheduleAtFixedRate(delay, interval); each tick analyzes once, the tick after the bar "
	                           "is full ends it (or starts the proc bar); the craft ends at delay + (analyze ticks + procs) * interval"}
	block["bars"] = bars
	crit, combo = crit_rates
	chances = [chance_below(crit)] + [chance_below(combo)] * max(0, len(recipe.combos) - 1)
	outcomes = []
	reach = Fraction(1)
	can_fail = any(bar.get("successPerTick", 1) < 1 for bar in bars)
	for k in range(len(recipe.combos) + 1):
		stop = 1 - chances[k] if k < len(recipe.combos) else Fraction(1)
		item_id = recipe.product_id if k == 0 else recipe.combos[k - 1]
		fewest = sum(b["steps"]["fewest"] for b in bars[:k + 1]) + k
		most = sum(b["steps"]["mostToAnyEnd"] for b in bars[:k + 1]) + k
		outcomes.append({"critCount": k, "itemId": item_id, "quantity": recipe.quantity,
		                 "probability": None if can_fail else _p(reach * stop),
		                 "finishMillis": {"fewest": c["delay"] + fewest * interval, "most": c["delay"] + most * interval}})
		reach *= chances[k] if k < len(recipe.combos) else 0
	block["crit"] = {"critChance": crit, "comboChance": crit_rates[1], "houseBonus": c["houseBonus"],
	                 "rule": "CraftingTask.calculateCrit when the bar is full: crit_chances for the first proc, combo_crit_chances after it, while a "
	                         "<comboproduct> is left; a proc restarts the bar with the combo product (its quality decides the next bar)"}
	block["outcomes"] = outcomes
	if can_fail:
		block["outcomeNote"] = ("a bar can fail (failureThreshold > 0 and the level difference below alwaysSuccess): then nothing is crafted and the "
		                        "materials are lost; the failure probability depends on the whole step walk and is not modelled")
	actions, anims = c["actions"], c["animations"]
	morph_delay = c["packetMorphDelay"] if morph else 0
	current_item = "the current bar's item: the product, or the combo product after a proc"
	block["packets"] = {
		"scope": "the task's own packets, checkCraft's system messages, startCrafting's DP packets and the skill-up packets (skillUp.packets); "
		         "not ItemService's inventory packets (materials, bonus item, product), RecipeList's (a deleted limited recipe, the autolearnt "
		         "recipes), the quest engine's or setExp's",
		"start": [
			{"packet": "SM_DP_INFO", "to": EVERYONE, "when": NOT_STARTING_CLASS, "dp": "the DP after addDp(-dp), capped at the max DP"},
			{"packet": "SM_STATS_INFO", "to": SELF, "when": NOT_STARTING_CLASS,
			 "note": "PlayerGameStats.updateStatsAndSpeedVisually; its max-HP/MP and speed packets only when those values changed (not modelled)"},
			{"packet": "SM_STATUPDATE_DP", "to": SELF, "when": NOT_STARTING_CLASS, "dp": "the same DP"},
			{"packet": "SM_CRAFT_UPDATE", "to": SELF, "skillId": recipe.skill_id, "itemId": recipe.product_id, "success": c["fullBar"],
			 "failure": c["fullBar"], "action": actions["init"], "executionSpeed": 0, "delay": morph_delay},
			{"packet": "SM_CRAFT_UPDATE", "to": SELF, "skillId": recipe.skill_id, "itemId": recipe.product_id, "success": 0, "failure": 0,
			 "action": actions["start"], "executionSpeed": 0, "delay": morph_delay},
			{"packet": "SM_CRAFT_ANIMATION", "to": EVERYONE, "skillId": recipe.skill_id, "action": anims["init"]},
			{"packet": "SM_CRAFT_ANIMATION", "to": EVERYONE, "skillId": recipe.skill_id, "action": anims["start"]},
		],
		"tick": {"packet": "SM_CRAFT_UPDATE", "to": SELF, "action": {"NORMAL": c["progress"]["NORMAL"], "CRIT_BLUE": c["progress"]["CRIT_BLUE"]},
		         "executionSpeed": "the bar's executionSpeed", "delay": "the bar's showBarDelay (the morph skill: always "
		                                                                 f"{c['packetMorphDelay']})",
		         "success": "the running success value", "failure": "the running failure value"},
		"proc": {"packet": "SM_CRAFT_UPDATE", "to": SELF, "action": actions["proc"], "note": "the start sequence again, with the combo item and this "
		                                                                                    "action instead of the first one"},
		"success": [{"packet": "SM_CRAFT_UPDATE", "to": SELF, "action": actions["success"], "success": c["fullBar"], "executionSpeed": 0,
		             "delay": morph_delay},
		            {"packet": "SM_CRAFT_ANIMATION", "to": EVERYONE, "skillId": 0, "action": anims["success"]}],
		"failure": [{"packet": "SM_CRAFT_UPDATE", "to": SELF, "action": actions["failure"], "failure": c["fullBar"], "executionSpeed": 0,
		             "delay": morph_delay},
		            {"packet": "SM_CRAFT_ANIMATION", "to": EVERYONE, "skillId": 0, "action": anims["failure"]}],
		"refused": [{"packet": "SM_CRAFT_UPDATE", "to": SELF, "skillId": recipe.skill_id, "itemId": recipe.product_id, "action": actions["cancel"],
		             "success": 0, "failure": 0, "executionSpeed": 0, "delay": morph_delay},
		            {"packet": "SM_CRAFT_ANIMATION", "to": EVERYONE, "skillId": 0, "action": anims["cancel"]}],
		"refusedNote": "after checkCraft's system message, if any: every refusal in `checks` (sendCancelCraft), also to a player whose own craft is "
		               "still in progress",
		"aborted": [{"packet": "SM_CRAFT_UPDATE", "to": SELF, "skillId": recipe.skill_id, "itemId": current_item, "action": actions["abort"],
		             "success": 0, "failure": 0, "executionSpeed": 0, "delay": morph_delay},
		            {"packet": "SM_CRAFT_ANIMATION", "to": EVERYONE, "skillId": 0, "action": anims["abort"]}],
	}
	return block


# ------------------------------------------------------------------------------------------------------------------------------------------
# Gathering

def _gather_checks(ctx: CraftContext, gatherable: GatherableInfo) -> None:
	if gatherable.unknown:
		raise OracleError(f"gatherable {gatherable.id}: {', '.join(gatherable.unknown)} - the oracle does not model what they mean")
	if ctx.value("SecurityConfig", "CAPTCHA_ENABLE"):
		raise OracleError("gameserver.security.captcha.enable is true: startGathering's CAPTCHA roll and gathering ban are not modelled")
	if gatherable.materials is None:
		raise OracleError(f"gatherable {gatherable.id} has no <materials> (getMaterials().getMaterial() throws NullPointerException)")
	if gatherable.req_item > 0 and gatherable.check_type in (ctx.rules.gather["checkEquipped"], ctx.rules.gather["checkInventory"]) \
		and gatherable.exmaterials is None:
		raise OracleError(f"gatherable {gatherable.id}: checkType {gatherable.check_type} without <exmaterials> (getExtraMaterials() is null)")


def _material_block(ctx: CraftContext, materials: list[dict] | None, what: str) -> list[dict] | None:
	if materials is None:
		return None
	for material in materials:
		try:
			ctx.item(material["itemId"])
		except OracleError as e:
			raise OracleError(f"{what}: {e} (GatheringTask.onSuccessFinish -> ItemService.addItem: Objects.requireNonNull throws, so no item and no "
			                  "skill xp - rewardPlayer is never reached)") from e
	bound = ctx.rules.gather["rollBound"]
	counts, nothing = material_rolls(materials, bound)
	if nothing:
		raise OracleError(f"{what}: {nothing} of the {bound} Rnd.nextInt values select no material (Java passes a null material to GatheringTask "
		                  "and SM_GATHER_UPDATE dereferences it)")
	return [{"itemId": m["itemId"], "name": m["name"], "rate": m["rate"], "rolls": n, "probability": _p(Fraction(n, bound)),
	         "documentPosition": m["documentPosition"]} for m, n in zip(materials, counts)]


def gather_task(ctx: CraftContext, gatherable: GatherableInfo, level: int) -> dict:
	"""GatheringTask at player skill level `level` (>= the template's skillLevel)."""
	g = ctx.rules.gather
	full = ctx.rules.craft["fullBar"]
	diff = level - gatherable.skill_level
	interval = max(g["intervalMin"], g["intervalBase"] - diff * g["intervalStep"])
	timing = {"firstTickDelay": [g["startDelayMin"], g["startDelayMax"]], "interval": interval}
	if diff >= g["instantDiff"]:
		return {"skillLvlDiff": diff, "instant": True, "timing": timing, "steps": {"fewest": 1, "most": 1},
		        "executionSpeed": g["fastSpeed"], "showBarDelay": g["fastDelay"], "successPerTick": 1.0,
		        "finishMillis": {"fewest": g["startDelayMin"] + interval, "most": g["startDelayMax"] + interval}}
	top = next_down(g["multiMax"])
	threshold = f32(jf(ctx.value("CraftConfig", "MAX_GATHER_FAILURE_CHANCE")) * fail_reduction(g, diff))
	success = 1 - chance_below(threshold)
	purple_threshold = f32(jf(g["purpleBase"]) + f32(jf(diff) / g["purpleDivisor"]))
	blue_threshold = f32(jf(g["blueBase"]) + f32(jf(diff) / g["blueDivisor"]))
	purple = chance_below(purple_threshold)
	blue = max(Fraction(0), chance_below(blue_threshold) - purple)
	normal = 1 - purple - blue
	normal_steps = [gather_success_step(g, diff, False, g["multiMin"]), gather_success_step(g, diff, False, top)]
	blue_steps = [gather_success_step(g, diff, True, g["multiMin"]), gather_success_step(g, diff, True, top)]
	failure_steps = [gather_failure_step(g, diff, g["multiMin"]), gather_failure_step(g, diff, top)]
	stepping = ([normal_steps] if normal > 0 else []) + ([blue_steps] if blue > 0 else [])
	if stepping:
		fewest, most = steps_to_fill(full, min(s[0] for s in stepping), max(s[1] for s in stepping))
	else:
		fewest = most = 1
	if purple > 0:
		fewest = 1
	speed = max(g["speedMin"], g["speedBase"] - diff * g["speedStep"])
	delay = max(g["delayMin"], g["delayBase"] - diff * g["delayStep"])
	steps = {"fewest": fewest, "most": most, "mostToAnyEnd": most}
	if success < 1:
		fail_fewest, fail_most = steps_to_fill(full, failure_steps[0], failure_steps[1])
		steps.update({"fewestToFailure": fail_fewest, "mostToAnyEnd": (most - 1) + (fail_most - 1) + 1})
	return {
		"skillLvlDiff": diff,
		"instant": False,
		"timing": timing,
		"failureThreshold": threshold,
		"successPerTick": _p(success),
		"critPurpleThreshold": purple_threshold,
		"critBlueThreshold": blue_threshold,
		"perSuccess": {"critPurple": _p(purple), "critBlue": _p(blue), "normal": _p(normal)},
		"successStep": {"normal": normal_steps, "critBlue": blue_steps, "critPurple": "the bar jumps to full"},
		"failureStep": failure_steps,
		"multi": [g["multiMin"], top],
		"executionSpeed": speed,
		"showBarDelay": delay,
		"critPurpleUpdate": {"executionSpeed": g["fastSpeed"], "showBarDelay": g["fastDelay"]},
		"steps": steps,
		"finishMillis": {"fewest": g["startDelayMin"] + fewest * interval, "most": g["startDelayMax"] + steps["mostToAnyEnd"] * interval},
	}


def gatherable_entry(ctx: CraftContext, gatherable: GatherableInfo, level: int | None, character_level: int, current_xp: int,
                     detailed: bool) -> dict:
	_gather_checks(ctx, gatherable)
	g = ctx.rules.gather
	level = max(1, gatherable.skill_level) if level is None else level  # level 0 would be "skill not learned"
	_check_level(level, "skill level")
	materials = _material_block(ctx, gatherable.materials, f"gatherable {gatherable.id} materials")
	extra = _material_block(ctx, gatherable.exmaterials, f"gatherable {gatherable.id} exmaterials")
	refusal = None
	if character_level < gatherable.lvl_limit:
		refusal = "STR_MSG_CANT_GATHERING_B_LEVEL_CHECK (character level below lvlLimit)"
	elif level == 0:
		refusal = (f"STR_GATHER_INCORRECT_SKILL (skill level 0: {g['humanSkill']} not learned)" if gatherable.harvest_skill == g["humanSkill"]
		           else "STR_GATHER_LEARN_SKILL (skill level 0: the harvest skill is not learned)")
	elif level < gatherable.skill_level:
		refusal = "STR_GATHER_OUT_OF_SKILL_POINT (skill level below the template's)"
	xp = gather_xp_reward(g, gatherable.skill_level)
	count = rates_calc_result_int(g["gatherCount"], ctx.rate("GATHERING_COUNT_RATES"))
	entry = {
		"id": gatherable.id,
		"name": gatherable.name,
		"nameId": gatherable.name_id,
		"sourceType": gatherable.source_type,
		"harvestSkill": gatherable.harvest_skill,
		"skillLevel": gatherable.skill_level,
		"lvlLimit": gatherable.lvl_limit,
		"harvestCount": gatherable.harvest_count,
		"materials": materials,
		"canGather": refusal is None,
		"refusal": refusal,
		"count": count,
		"xpReward": xp,
		"skillUp": None if refusal else skill_up(ctx, gatherable.harvest_skill, level, current_xp, gatherable.skill_level, xp, "gathering"),
	}
	if not detailed:
		return entry
	entry.update({
		"captcha": gatherable.captcha,
		"requiredItem": {"itemId": gatherable.req_item, "nameId": gatherable.req_item_name_id, "checkType": gatherable.check_type,
		                 "eraseValue": gatherable.erase_value} if gatherable.req_item > 0 else None,
		"exMaterials": extra,
		"materialRule": f"GatherableController.getMaterials: <exmaterials> when reqItem > 0 and (checkType {g['checkEquipped']} and the item "
		                f"equipped, or checkType {g['checkInventory']} and at least eraseValue of it in the inventory - fewer is "
		                "STR_MSG_CANT_GATHERING_B_ITEM_CHECK), else <materials>; then chance = Rnd.nextInt(rollBound) picks the first material "
		                "(stable order of descending rate) whose running rate sum is >= chance",
		"rollBound": g["rollBound"],
		"erasedOnSuccess": gatherable.erase_value if gatherable.erase_value > 0 else 0,
		"range": {"range": g["gatherRange"], "centerToCenter": False, "gatherableBoundRadius": ctx.rules.object_bound,
		          "playerBoundRadius": ctx.rules.player_bound,
		          "effectiveRange": f32(f32(jf(g["gatherRange"]) + ctx.rules.object_bound) + ctx.rules.player_bound),
		          "comparison": "PositionUtil.isInRange(gatherable, player, range, false): same world and instance, then dx*dx + dy*dy + dz*dz < "
		                        "effectiveRange^2 in float arithmetic (strict)"},
		"checks": ["character level < lvlLimit: STR_MSG_CANT_GATHERING_B_LEVEL_CHECK", "riding without the membership: STR_MSG_GATHER_RESTRICTION_RIDE",
		           "inventory full: STR_GATHER_INVENTORY_IS_FULL", "under a stance: STR_SKILL_CAN_NOT_GATHER_WHILE_IN_CURRENT_STANCE",
		           "out of range: STR_GATHER_TOO_FAR_FROM_GATHER_SOURCE", "no line of sight: STR_GATHER_OBSTACLE_EXIST",
		           "gather-restricted: STR_MSG_CAPTCHA_REMAIN_RESTRICT_TIME",
		           f"skill missing: STR_GATHER_INCORRECT_SKILL for {g['humanSkill']}, else STR_GATHER_LEARN_SKILL",
		           "skill level below skillLevel: STR_GATHER_OUT_OF_SKILL_POINT", "exmaterials item short: STR_MSG_CANT_GATHERING_B_ITEM_CHECK",
		           f"someone else gathering it: SM_GATHER_UPDATE action {g['actions']['occupied']}"],
		"task": None if refusal else gather_task(ctx, gatherable, level),
		"node": {"despawnAfterInteractions": gatherable.harvest_count if gatherable.harvest_count > 0 else None,
		         "rule": "completeInteraction runs from GatheringTask.onInteractionFinish, i.e. after EVERY ended interaction (success, failure or "
		                 "abort); at harvestCount the node is deleted (instance) or deleted with a respawn"},
		"aborts": "any move, skill cast, attack, being attacked or a damage-over-time tick aborts (GatheringTask.createGathererObserver)",
		"packets": _gather_packets(ctx, gatherable),
	})
	return entry


def _gather_packets(ctx: CraftContext, gatherable: GatherableInfo) -> dict:
	g = ctx.rules.gather
	actions, anims, full = g["actions"], g["animations"], ctx.rules.craft["fullBar"]
	skill = gatherable.harvest_skill
	return {
		"scope": "the task's own packets; on success they are followed by ItemService.addItem's inventory packets (not modelled) and rewardPlayer's "
		         "(skillUp.packets); the eraseValue decrease's inventory packets are not modelled either",
		"start": [{"packet": "SM_GATHER_UPDATE", "to": SELF, "skillId": skill, "success": full, "failure": full, "action": actions["init"],
		           "executionSpeed": 0, "delay": 0},
		          {"packet": "SM_GATHER_UPDATE", "to": SELF, "skillId": skill, "success": 0, "failure": 0, "action": actions["start"],
		           "executionSpeed": 0, "delay": 0},
		          {"packet": "SM_GATHER_ANIMATION", "to": EVERYONE, "skillId": skill, "action": anims["init"]},
		          {"packet": "SM_GATHER_ANIMATION", "to": EVERYONE, "skillId": skill, "action": anims["start"]}],
		"tick": {"packet": "SM_GATHER_UPDATE", "to": SELF, "action": dict(ctx.rules.craft["progress"])},
		"success": [{"packet": "SM_GATHER_ANIMATION", "to": EVERYONE, "skillId": skill, "action": anims["success"]},
		            {"packet": "SM_GATHER_UPDATE", "to": SELF, "action": actions["success"], "executionSpeed": 0, "delay": 0}],
		"failure": [{"packet": "SM_GATHER_UPDATE", "to": SELF, "action": actions["failurePre"], "executionSpeed": 0, "delay": 0},
		            {"packet": "SM_GATHER_UPDATE", "to": SELF, "action": actions["failure"], "executionSpeed": 0, "delay": 0},
		            {"packet": "SM_GATHER_ANIMATION", "to": EVERYONE, "skillId": skill, "action": anims["failure"]}],
		"aborted": [{"packet": "SM_GATHER_ANIMATION", "to": OTHERS, "skillId": skill, "action": anims["abort"]},
		            {"packet": "SM_GATHER_UPDATE", "to": SELF, "action": actions["abort"], "success": 0, "failure": 0}],
		"itemId": "every SM_GATHER_UPDATE carries the ROLLED material's item id; the skill id is the template's harvestSkill",
	}


def gatherable_report(ctx: CraftContext, gatherable_id: int, skill_level: int | None = None, character_level: int = 1, current_xp: int = 0) -> dict:
	gatherable = ctx.gatherables.get(gatherable_id)
	if gatherable is None:
		raise OracleError(f"no gatherable_template {gatherable_id}")
	names = ctx.skill_names()
	if gatherable.harvest_skill not in names:
		raise OracleError(f"gatherable {gatherable_id}: harvest skill {gatherable.harvest_skill} has no skill_template (the refusal messages "
		                  "dereference it)")
	report = ctx.header("gatherable")
	report["skill"] = _skill_block(ctx, gatherable.harvest_skill, names)
	report["gatherable"] = gatherable_entry(ctx, gatherable, skill_level, character_level, current_xp, detailed=True)
	return report


# ------------------------------------------------------------------------------------------------------------------------------------------
# Skills

def _upgrade(ctx: CraftContext, skill_id: int, level: int) -> dict | None:
	"""CraftSkillUpdateService.learnSkill at a master for a character whose skill is at `level` (0: not learned)."""
	profession = ctx.profession(skill_id)
	if profession is None:
		return None
	crafting = ctx.rules.crafting_range[0] <= skill_id <= ctx.rules.crafting_range[1]
	cost = ctx.rules.upgrade_costs.get(level)
	special_level, special_cost = ctx.rules.upgrade_cost_crafting_only
	if level == special_level:
		cost = special_cost if crafting else None
	max_level = ctx.rules.max_upgradable[0] if crafting else ctx.rules.max_upgradable[1]
	refusal = None
	if cost is None:
		refusal = ("STR_MSG_DONT_RANK_UP_GATHERING" if level > max_level else "STR_CRAFT_CANT_EXTEND_MONEY" if level == 399
		           else "STR_CRAFT_CANT_EXTEND_GRAND_MASTER" if level == 499 else "STR_MSG_DONT_RANK_UP")
	masters = sorted(npc for npc, name in ctx.rules.profession_by_npc.items() if name == profession)
	return {"profession": profession, "cost": cost, "newLevel": level + 1 if cost is not None else None, "refusal": refusal,
	        "question": "SM_QUESTION_WINDOW STR_CRAFT_ADDSKILL_CONFIRM with the price; yes: tryDecreaseKinah(DEC_KINAH_LEARN), addSkill(level + 1)",
	        "minCharacterLevel": ctx.rules.skill["masterMinLevel"], "masters": masters,
	        "costTable": {str(k): v for k, v in sorted(ctx.rules.upgrade_costs.items())} | {str(special_level): special_cost if crafting else None},
	        "maxUpgradableLevel": max_level}


def _xp_cap(ctx: CraftContext, skill_id: int, level: int) -> str | None:
	probe = add_skill_xp(ctx.rules, skill_id, level, 0, 0, level, ctx.value("CraftConfig", "DISABLE_AETHER_AND_ESSENCE_TAPPING_CAP"))
	return probe["refusal"]


def skill_report(ctx: CraftContext, skill_id: int, level: int, map_id: int | None = None, character_level: int = 1, current_xp: int = 0) -> dict:
	_check_level(level, "--level")
	kind = ctx.skill_kind(skill_id)
	if kind in ("other", "profession"):
		raise OracleError(f"skill {skill_id} is no gathering, crafting or morph skill (PlayerSkillEntry.isTappingSkill/isCraftingSkill/isMorphSkill)")
	names = ctx.skill_names()
	if skill_id not in names:
		raise OracleError(f"skill {skill_id} has no skill_template")
	report = ctx.header("skill")
	report["skill"] = {**_skill_block(ctx, skill_id, names), "skillTree": skill_tree_rows(ctx.data, skill_id)}
	report["level"] = level
	s = ctx.rules.skill
	report["skillUp"] = {
		"requiredExp": required_exp(level, s["lvlFactor"], s["lvlOffset"]) if level > 0 else None,
		"cap": _xp_cap(ctx, skill_id, level) if level > 0 else None,
		"maxObjectLevelGap": s["maxGap"],
		"rule": "addSkillXp: no xp when skill level - object level > maxGap or at a cap level; a level up when current xp + gained >= requiredExp, "
		        "and the rest of the xp is dropped (current xp 0)",
	}
	report["upgrade"] = _upgrade(ctx, skill_id, level)
	if kind in ("crafting", "morph"):
		recipes = [r for r in ctx.recipes.values() if r.skill_id == skill_id]
		report["recipes"] = {}
		for race in RACES:
			learned = autolearn_recipes(ctx.recipes, race, skill_id, level)
			craftable = [r for r in recipes if r.skillpoint <= level and (r.race == "PC_ALL" or r.race == race)]
			report["recipes"][race] = {
				"autolearn": [{"id": r.id, "skillpoint": r.skillpoint, "productId": r.product_id, "quantity": r.quantity} for r in learned],
				"newAtLevel": [r.id for r in learned if r.skillpoint == level],
				"craftableCount": len(craftable),
			}
		points = sorted({r.skillpoint for r in recipes if level - s["maxGap"] <= r.skillpoint <= level})
		report["xpByRecipeSkillpoint"] = [_xp_row(ctx, skill_id, level, current_xp, point, craft_xp_reward(ctx.rules.craft, point, 0)[0], "crafting")
		                                  for point in points]
	else:
		report["gatherables"] = _gatherables_of(ctx, skill_id, level, map_id, character_level, current_xp)
	return report


def _xp_row(ctx: CraftContext, skill_id: int, level: int, current_xp: int, object_level: int, xp: int, kind: str) -> dict:
	up = skill_up(ctx, skill_id, level, current_xp, object_level, xp, kind)
	return {"objectLevel": object_level, "xpReward": xp, "gainedSkillXp": up["gainedSkillXp"], "granted": up["granted"],
	        "levelsUp": up.get("levelsUp", False), "playerExp": up["playerExp"]["reward"] if up["playerExp"] else None}


def _gatherables_of(ctx: CraftContext, skill_id: int, level: int, map_id: int | None, character_level: int, current_xp: int) -> dict:
	templates = {gid: t for gid, t in ctx.gatherables.items() if t.harvest_skill == skill_id}
	spots: dict[int, list[dict]] = {}
	if map_id is not None:
		world = [m for m in ctx.data.children("world_maps", "map") if _int(m.get("id"), "map id") == map_id]
		if not world:
			raise OracleError(f"map {map_id} is not in world_maps")
		if world[0].get("instance") not in (None, "false", "0"):
			raise OracleError(f"map {map_id} is an instance: the node is deleted without respawn and the instance handler's onGather may act, "
			                  "which the oracle does not model")
		# evaluate() needs npc templates only for rows that are not gatherables (their existence, level and FLAG type)
		for row in evaluate([g for g in load_groups(ctx.data, map_id) if is_gatherable(g.npc_id)], {}, GameClock()):
			spots.setdefault(row["npcId"], []).append(row)
		missing = sorted(gid for gid in spots if gid not in ctx.gatherables)
		if missing:
			raise OracleError(f"map {map_id} spawns gatherables without a template: {missing}")
		selected = [ctx.gatherables[gid] for gid in sorted(spots) if ctx.gatherables[gid].harvest_skill == skill_id]
	else:
		selected = [t for gid, t in sorted(templates.items()) if t.skill_level <= level]
	entries = []
	for template in selected:
		entry = gatherable_entry(ctx, template, max(level, 0), character_level, current_xp, detailed=False)
		if map_id is not None:
			rows = spots[template.id]
			entry["spots"] = {"count": len(rows), "spawned": sorted({r["spawned"] for r in rows}, key=str),
			                  "respawnTimes": sorted({r["respawnTime"] for r in rows})}
		entries.append(entry)
	return {"map": map_id, "characterLevel": character_level, "entries": entries,
	        "selection": "every gatherable of the skill spawned on the map" if map_id is not None
	        else "every gatherable template of the skill with skillLevel <= the level"}


def craft_report(ctx: CraftContext, recipe: int | None = None, skill: int | None = None, level: int | None = None, gatherable: int | None = None,
                 skill_level: int | None = None, craft_type: int = 0, map_id: int | None = None, character_level: int = 1, current_xp: int = 0) -> dict:
	modes = [name for name, value in (("--recipe", recipe), ("--skill", skill), ("--gatherable", gatherable)) if value is not None]
	if len(modes) != 1:
		raise OracleError("pass exactly one of --recipe, --skill (with --level) and --gatherable")
	if current_xp < 0:
		raise OracleError("--skill-xp must be >= 0")
	if recipe is not None:
		return recipe_report(ctx, recipe, skill_level, craft_type, current_xp)
	if gatherable is not None:
		return gatherable_report(ctx, gatherable, skill_level, character_level, current_xp)
	if level is None:
		raise OracleError("--skill needs --level")
	return skill_report(ctx, skill, level, map_id, character_level, current_xp)
