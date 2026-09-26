"""m5b3-drops: everything a gate needs about what a killed npc can drop (m5b3-plan.md G-01, D3, D4, D10, §2.4).

Java rules, each with the method the value comes from (paths below game-server/src/com/aionemu/gameserver unless noted):
- whether a drop is registered at all: NpcController.onDie (controllers/NpcController.java:136-170) asks the npc's AI REWARD_AP_XP_DP_LOOT before
  doReward and ALLOW_DECAY for the corpse, and doReward (:206-247) calls DropRegistrationService.registerDrop(npc, player, player.getLevel(), null)
  for the most-damage attacker only if the AI answers REWARD_LOOT true (:244-245), after adding the kill's XP (:233: --player-level is the
  level at registerDrop, after a level-up from this kill). The AI is AIEngine.newAI(name) (ai/AIEngine.java:64-86) of the spot's `ai` attribute,
  else the template's (model/gameobjects/Creature.java:64-66; SpawnTemplate.NO_AI means no AI: DummyAI, which answers every question false),
  where NpcTemplate.afterUnmarshal (model/templates/npc/NpcTemplate.java:122-127) has already replaced the template's ai with
  "siege_teleporter" when level > 1, the ai is not "noaction" and abyss_type is TELEPORTER. The answers are read from the `ask(AIQuestion)`
  overrides along the AI class's `extends` chain (NpcAI.ask, ai/NpcAI.java:147-160: REWARD_AP_XP_DP_LOOT, REWARD_LOOT and ALLOW_DECAY true);
  an AI overriding handleDropRegistered (AbstractAI.java:314) is refused, since it may rewrite the drop
  (data/handlers/ai/events/HalloweenPumpkinAI.java:60). An AI class that calls registerDrop itself (ChestAI.java:68, QuestItemNpcAI.java:68
  through AIActions.registerDrop, NightmareCrateAI.java:75, all from handleUseItemFinish, i.e. on use) is reported as such: when and whether
  that call runs (key items, a quest dialog) is not modelled; for a solo user it registers the same drop set (its groupMembers holds only the
  user: QuestService.getQuestDrop and addDropItems take the solo arm, an each_member custom drop becomes the user's distributed item);
- registerDrop (services/drop/DropRegistrationService.java:59-109), solo (no team: initDropNpc :122-165 allows only the killer, winnerObj 0):
  index 1; the custom drop (DataManager.CUSTOM_NPC_DROP.getNpcDrop, the FIRST <npc_drop> of an id, dataholders/CustomDrop.java:34-40) through
  NpcDrop.dropCalculator (model/drop/NpcDrop.java:39-48: a group applies if its race is PC_ALL or the player's) and DropGroup.tryAddDropItems
  (model/drop/DropGroup.java:57-83: max_items rolls of Rnd.chance(); the drops whose final chance is above the roll and nearest to it tie, one of
  them is taken uniformly and removed); then QuestService.getQuestDrop (services/QuestService.java:666-733, the solo arm :726-730 with
  isQuestDrop :754-796); then, unless the AI is "quest_use_item" (:87), the global rules of GlobalDropData (after processRules, see below)
  when the npc has no global npc exclusion (hasGlobalNpcExclusions :287-296) and its map's drop_type is not NONE (:92), each through
  addGlobalDrops (:183-196): skipped unless isAllowedDefaultGlobalDropNpc (:167-181) or the rule has gd_npcs; calculateEffectiveChance (:221-227,
  the dynamic factor getRankModifier * getRatingModifier :455-474) through DropModifiers.calculateDropChance (model/drop/DropModifiers.java:53-57);
  fired unless Rnd.chance() >= chance; addDropItems (:229-258, the solo arm :251-254) over collectDrops (:417-430: Chance.selectElement(drops,
  true) max_drop_rule times when there are more candidates) and collectAllowedDrops (:432-445: checkRuleRestrictions :298-415 in Java's order,
  then item race PC_ALL or the player's and min_diff <= npcLevel - itemLevel <= max_diff), each entry regDropItem(index++, ...) with
  getItemCount (:447-453: Rnd.get(min, max), for kinah `count *= npc.getLevel() * Math.pow(rank * rating, 6)`);
- the modifiers: createDropModifiers (:111-120: chest = the AI name "chest" or a group_drop starting with "treasure" or ending with "box"),
  calculateBoostDropRate (:203-219: Rates.get(killer, RatesConfig.DROP_RATES) * boost / 100f, model/gameobjects/player/Rates.java:166-173) and
  getReductionDropRate (:198-201, utils/stats/DropRewardEnum.java:30-43);
- GlobalDropData.processRules (dataholders/GlobalDropData.java:30-71, called at DataManager.java:231): a rule with gd_npc_names gets gd_npcs =
  its own gd_npcs plus every npc template whose name matches (CONTAINS/START_WITH/END_WITH against value.toLowerCase(), EQUALS ignoring case)
  when that list is not empty; otherwise the rule keeps gd_npcs as it was, which may be null - the names are never checked again;
- the wire values per item: DropItem(Drop) (model/drop/DropItem.java:28-33: optionalSocket -1 when the template has option_slot_bonus),
  DropItem.getLootEffectId (:204-213, what SM_LOOT_STATUS(LOOT_ENABLE) carries when such an item is in the drop: SM_LOOT_STATUS.getLootEffect,
  network/aion/serverpackets/SM_LOOT_STATUS.java:33-36, any non-zero id of the drop set, so the id is not determined when two differ; the
  report gives the exact probability that it is not 0: rules and custom groups draw independently, no quest drop as `entries` assumes), and
  SM_LOOT_ITEMLIST.writeImpl (:37-58: showLootConfirmation 0 for a solo looter);
- the global rule order (ruleIndex, and so which rule an entry index belongs to) is GlobalDropData's: the files of the global_rules import in
  the order XmlUtil.listFiles (Files.find) returns them, which staticdata_oracle/imports.py models as NTFS order.

Randomness: Rnd.chance() is RandomGenerator.nextFloat(100f) (commons/src/com/aionemu/commons/utils/Rnd.java:32-34), which the JDK implements as
`(nextInt() >>> 8) * 0x1.0p-24f * bound`, corrected to Math.nextDown(bound) when it rounds up to bound (RandomGenerator.nextFloat,
RandomSupport.boundedNextFloat - JDK code, not in this repository). The probabilities below are exact over those 2^24 float values; `certain`
(effective chance >= 100f) and `never` (<= 0) hold for any nextFloat in [0, 100). Rnd.get(min, max) and Rnd.get(List) are uniform.

What the oracle does NOT model, and raises OracleError for instead of guessing: a rule whose zone predicate decides (gd_zones: isInsideZone needs
the npc's position and the zone shapes; a rule that adds no entry either way is reported in rulesZoneUndecided instead: it never fires -
addGlobalDrops rolls before collectAllowedDrops checks the restrictions - or it has no candidate), an AI whose ask() is not a
`return switch (question) { case ... -> true|false|super.ask(question); }` or `return true|false;` for the three questions, an AI overriding
handleDropRegistered, a map with an @InstanceID handler (onDropRegistered), an npc without group_drop (Java: NullPointerException in
createDropModifiers) or without rank/rating where a dynamic rule or the kinah count needs them, unknown enum values (JAXB leaves them null and
Java throws where they are compared), a float product beyond the float range (Java: Infinity; 0 * Infinity is NaN, and `Rnd.chance() >= NaN`
is false, so such a rule would fire), data on which Java fails at startup (a gd_item without an item template, min_count <= 0, max_count < min_count, a custom
<drop> that Drop.afterUnmarshal rejects - in every <npc_drop>, also a repeated one, a gd_npc_names rule while an npc template has no name),
and event drop rules: the oracle assumes gameserver.event.service.disabled_events = * (every gate profile; the Java default is empty, which
keeps the permanent "Beyond Aion Server Buffs" event and every dated event in its period active). It computes for a solo killer (no group or
alliance) with no BOOST_DROP_RATE/DR_BOOST modifier, no Energy of Repose or Salvation and no palace, membership 0. Repose is 0 below level 10
unless a seeded database row or a command set it (PlayerCommonData.updateMaxRepose, PlayerCommonData.java:236-238, 248-256, clears it on a
level change and a later login) and assumed 0 from level 10 on, where it grows offline (the report says so; a character with repose energy
gets +5). Salvation (getCurrentSalvationPercent, :535-544, no level gate) is 0 unless salvation points
were set, which only the //energybuff admin command (data/handlers/admincommands/EnergyBuff.java:47) and the set_vitalpoint console command
(consolecommands/Set_vitalpoint.java:35) do - PlayerDAO does not load them.
"""

from __future__ import annotations

import math
import re
import struct
import xml.etree.ElementTree as ET
import xml.parsers.expat as expat
from dataclasses import dataclass, field
from fractions import Fraction
from pathlib import Path

from staticdata_oracle import OracleError

from m5a.creation import RACES, enum_constants
from m5a.data import StaticData, java_boolean, java_int
from m5a.javafloat import f32, to_long
from m5a.spawns import GameClock, NpcInfo, evaluate, is_gatherable, load_groups
from m5b.monster import RACE_OF_WORLD_TYPE, _instance_handler_class, _map_template

LATTICE = 1 << 24  # RandomGenerator.nextFloat(): 24 random bits
AI_QUESTIONS = ("REWARD_AP_XP_DP_LOOT", "REWARD_LOOT", "ALLOW_DECAY")
MAX_SELECTION_STATES = 4000
MAX_CUSTOM_STATES = 20000
MAX_CUSTOM_ROLLS = 200
MAX_COUNT_VALUES = 100000
RULE_CHILDREN = ("gd_items", "gd_maps", "gd_races", "gd_tribes", "gd_ratings", "gd_worlds", "gd_npcs", "gd_npc_names", "gd_npc_groups",
                 "gd_excluded_npcs", "gd_zones")
RULE_ATTRIBUTES = ("rule_name", "chance", "min_diff", "max_diff", "restriction_race", "level_based_chance_reduction", "member_limit",
                   "max_drop_rule", "dynamic_chance")
# GlobalRule's restriction wrappers: XML child, entry tag, entry attribute, Java enum (None: int)
RESTRICTION_LISTS = {
	"gd_maps": ("gd_map", "map_id", None),
	"gd_worlds": ("gd_world", "wd_type", "WorldDropType"),
	"gd_ratings": ("gd_rating", "rating", "NpcRating"),
	"gd_races": ("gd_race", "race", "Race"),
	"gd_tribes": ("gd_tribe", "tribe", "TribeClass"),
	"gd_zones": ("gd_zone", "zone", "zone"),
	"gd_npcs": ("gd_npc", "npc_id", None),
	"gd_npc_groups": ("gd_npc_group", "group", "GroupDropType"),
}
# checkRuleRestrictions (DropRegistrationService.java:298-320), in Java's order
PREDICATES = ("restrictionRace", "maps", "worlds", "ratings", "races", "tribes", "zones", "npcs", "npcGroups", "excludedNpcs")


# ---------------------------------------------------------------------------------------------------------------------------------------------
# Java float and Rnd arithmetic

def _f32_bits(value: float) -> int:
	return struct.unpack("<I", struct.pack("<f", value))[0]


def _f32_from_bits(bits: int) -> float:
	return struct.unpack("<f", struct.pack("<I", bits))[0]


def checked_f32(value: float, what: str) -> float:
	"""A float product or sum of the drop arithmetic. Beyond the float range Java gets Infinity, and a chance of 0 times Infinity is NaN, which
	`Rnd.chance() >= chance` never reaches (such a rule would fire): not carried through, refused."""
	try:
		return f32(value)
	except OverflowError as e:
		raise OracleError(f"{what}: {value!r} is beyond the float range (Java: Infinity), which the oracle does not model") from e


def next_down(value: float) -> float:
	"""Math.nextDown(float) for a positive finite float."""
	if value <= 0:
		raise OracleError(f"next_down({value}) is only modelled for positive values")
	return _f32_from_bits(_f32_bits(value) - 1)


def java_float(text: str | None, what: str, default: float | None = None) -> float:
	"""A JAXB float (Float.parseFloat): the decimal rounded ONCE to the nearest binary32, ties to even (not through a double)."""
	if text is None:
		if default is None:
			raise OracleError(f"missing required value {what}")
		return default
	value = text.strip()
	if not re.fullmatch(r"[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?", value):
		raise OracleError(f"{what}={text!r} is not a decimal float the oracle models")
	exact = Fraction(value)
	if exact == 0:
		return 0.0
	magnitude = abs(exact)
	try:
		candidate = f32(float(magnitude))
	except OverflowError as e:
		raise OracleError(f"{what}={text!r} is outside the float range") from e
	best = None
	bits = _f32_bits(candidate)
	for neighbour in (bits - 1, bits, bits + 1):
		if neighbour < 0 or neighbour >= 0x7F800000:
			continue
		value32 = _f32_from_bits(neighbour)
		key = (abs(Fraction(value32) - magnitude), neighbour & 1)
		if best is None or key < best[0]:
			best = (key, value32)
	return best[1] if exact > 0 else -best[1]


def next_float_value(k: int, bound: float) -> float:
	"""RandomGenerator.nextFloat(bound) for the k-th 24-bit value: (k * 2^-24f) * bound in float, corrected to nextDown(bound) at bound."""
	value = f32(k * (1.0 / LATTICE) * bound)  # both factors are exact float values, the product is rounded once to float
	return next_down(bound) if value >= bound else value


def lattice_count(bound: float, threshold: float, inclusive: bool = False) -> int:
	"""How many of the 2^24 values of nextFloat(bound) are < threshold (<= with inclusive): values are monotonic in k, so a binary search."""
	lo, hi = 0, LATTICE  # the answer is the first k whose value fails the test
	while lo < hi:
		mid = (lo + hi) // 2
		value = next_float_value(mid, bound)
		if value < threshold or (inclusive and value == threshold):
			lo = mid + 1
		else:
			hi = mid
	return lo


def chance_probability(threshold: float) -> Fraction:
	"""P(Rnd.chance() < threshold): the probability that a `if (Rnd.chance() >= chance) skip` test passes."""
	if threshold <= 0:
		return Fraction(0)
	if threshold >= 100.0:
		return Fraction(1)
	return Fraction(lattice_count(f32(100.0), threshold), LATTICE)


def chance_between(low: float, high: float) -> Fraction:
	"""P(low <= Rnd.chance() < high)."""
	return chance_probability(high) - chance_probability(low) if high > low else Fraction(0)


def select_element_probabilities(weights: list[float]) -> list[Fraction] | None:
	"""
	Chance.selectElement (model/Chance.java:26-45): the float sum of the weights, randomChance = Rnd.nextFloat(sum), and the first element whose
	float running sum `luck` is >= randomChance. None when the sum is not positive (Java returns null and collectDrops adds nothing).
	"""
	total = 0.0
	running = []
	for weight in weights:
		total = checked_f32(total + weight, "Chance.selectElement: the sum of the weights")
		running.append(total)
	if not total > 0:
		return None
	probabilities = []
	previous = 0
	for luck in running:
		count = lattice_count(total, luck, inclusive=True)
		probabilities.append(Fraction(count - previous, LATTICE))
		previous = count
	if previous != LATTICE:
		raise OracleError("Chance.selectElement: the running sum does not cover every random value")  # cannot happen with non-negative weights
	return probabilities


def selection_inclusion(weights: list[float], picks: int) -> list[Fraction] | None:
	"""
	collectDrops' loop `for (i < maxDrops && !drops.isEmpty()) selectElement(drops, true)`: the probability that each candidate is among the picks.
	Exact over every sequence of picks; None when the remaining-set states exceed MAX_SELECTION_STATES (the caller reports it as not modelled).
	"""
	n = len(weights)
	memo: dict[tuple[int, ...], list[Fraction]] = {}

	def solve(remaining: tuple[int, ...], left: int) -> list[Fraction]:
		if left == 0 or not remaining:
			return [Fraction(0)] * n
		key = remaining + (-left,)
		if key in memo:
			return memo[key]
		if len(memo) > MAX_SELECTION_STATES:
			raise _TooManyStates()
		probabilities = select_element_probabilities([weights[i] for i in remaining])
		result = [Fraction(0)] * n
		if probabilities is not None:
			for position, p in enumerate(probabilities):
				if p == 0:
					continue
				chosen = remaining[position]
				result[chosen] += p
				rest = solve(remaining[:position] + remaining[position + 1:], left - 1)
				for i in range(n):
					result[i] += p * rest[i]
		memo[key] = result
		return result

	try:
		return solve(tuple(range(n)), picks)
	except _TooManyStates:
		return None


def selection_avoidance(weights: list[float], picks: int, avoided: set[int]) -> Fraction | None:
	"""The same collectDrops loop: the probability that none of the candidates at the positions `avoided` is among the picks (None beyond
	MAX_SELECTION_STATES)."""
	memo: dict[tuple[int, ...], Fraction] = {}

	def solve(remaining: tuple[int, ...], left: int) -> Fraction:
		if left <= 0 or not remaining:
			return Fraction(1)
		key = remaining + (-left,)
		if key in memo:
			return memo[key]
		if len(memo) > MAX_SELECTION_STATES:
			raise _TooManyStates()
		probabilities = select_element_probabilities([weights[i] for i in remaining])
		result = Fraction(1) if probabilities is None else Fraction(0)  # null: nothing is picked, the loop picks nothing again
		for position, p in enumerate(probabilities or []):
			if p and remaining[position] not in avoided:
				result += p * solve(remaining[:position] + remaining[position + 1:], left - 1)
		memo[key] = result
		return result

	try:
		return solve(tuple(range(len(weights))), picks)
	except _TooManyStates:
		return None


class _TooManyStates(Exception):
	pass


def fraction_out(value: Fraction) -> float:
	return float(value)


# ---------------------------------------------------------------------------------------------------------------------------------------------
# Java sources

def _read(source: Path) -> str:
	try:
		return source.read_text(encoding="utf-8")
	except OSError as e:
		raise OracleError(f"{source}: {e}") from e


def _strip_comments(text: str) -> str:
	return re.sub(r"//[^\n]*", "", re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL))


def _normalized(text: str) -> str:
	return re.sub(r"\s+", " ", _strip_comments(text))


def _search(text: str, pattern: str, source: Path, what: str) -> re.Match:
	match = re.search(pattern, text, re.DOTALL)
	if not match:
		raise OracleError(f"{source}: cannot read {what} (the Java source does not have the shape this oracle was written against)")
	return match


# Statements the evaluator models, checked on the whitespace-normalized sources: if one is gone the Java code changed and the oracle refuses.
REQUIRED_STATEMENTS = {
	"services/drop/DropRegistrationService.java": [
		"NpcDrop npcDrop = DataManager.CUSTOM_NPC_DROP.getNpcDrop(npc.getNpcId());",
		"int index = 1;",
		"if (npcDrop != null) index = npcDrop.dropCalculator(droppedItems, index, dropModifiers, groupMembers);",
		"index = QuestService.getQuestDrop(droppedItems, index, npc, groupMembers, looter);",
		"if (!hasGlobalNpcExclusions && npc.getWorldDropType() != WorldDropType.NONE) { index = addGlobalDrops(index, dropModifiers, looter, npc, "
		"isAllowedDefaultGlobalDropNpc, DataManager.GLOBAL_DROP_DATA.getAllRules(), droppedItems, groupMembers, winnerObj); }",
		"if (!hasGlobalNpcExclusions || dropModifiers.isDropNpcChest()) addGlobalDrops(index, dropModifiers, looter, npc, "
		"isAllowedDefaultGlobalDropNpc, EventService.getInstance().getActiveEventDropRules(),",
		"dropModifiers.setDropRace(player.getRace());",
		"dropModifiers.setBoostDropRate(calculateBoostDropRate(player, npc));",
		"dropModifiers.setReductionDropRate(getReductionDropRate(npc, highestLevel));",
		"if (npc.getSpawn() instanceof SiegeSpawnTemplate && npc.getAbyssNpcType() != AbyssNpcType.DEFENDER) return false;",
		"if (isChest || npc.getAbyssNpcType() != AbyssNpcType.NONE && npc.getAbyssNpcType() != AbyssNpcType.DEFENDER) return false;",
		"if (isAllowedDefaultGlobalDropNpc || rule.getGlobalRuleNpcs() != null) { float chance = calculateEffectiveChance(rule, npc, dropModifiers); "
		"if (Rnd.chance() >= chance) continue; index = addDropItems(",
		"int dropChance = DropRewardEnum.dropRewardFrom(npc.getLevel() - highestLevel);",
		"return dropChance == 100 ? null : dropChance / 100f;",
		"if (killer.getCommonData().getCurrentReposeEnergy() > 0) boostDropRate += 5;",
		"return Rates.get(killer, RatesConfig.DROP_RATES) * boostDropRate / 100f;",
		"float chance = rule.getChance(); if (rule.isDynamicChance()) chance *= getRankModifier(npc) * getRatingModifier(npc); "
		"return dropModifiers.calculateDropChance(chance, rule.isUseLevelBasedChanceReduction());",
		"List<GlobalDropItem> drops = collectDrops(rule, npc, dropModifiers); if (!drops.isEmpty()) { if (rule.getMemberLimit() > 1 && "
		"player.isInTeam()) {",
		"for (GlobalDropItem drop : drops) { droppedItems.add(regDropItem(index++, winnerObj, npc.getObjectId(), drop.getId(), "
		"getItemCount(drop, npc))); }",
		"if (gde.getNpcIds().contains(npc.getNpcId()) || gde.getNpcNames().contains(npc.getName()) || gde.getNpcTemplateTypes().contains("
		"npc.getNpcTemplateType()) || npc.getTribe() != null && gde.getNpcTribes().contains(npc.getTribe()) || gde.getNpcAbyssTypes().contains("
		"npc.getAbyssNpcType())) return true;",
		"if (race == Race.ASMODIANS && rule.getRestrictionRace() == GlobalRule.RestrictionRace.ELYOS || race == Race.ELYOS && "
		"rule.getRestrictionRace() == GlobalRule.RestrictionRace.ASMODIANS) return false;",
		"if (gdMap.getMapId() == npc.getPosition().getMapId()) return true;",
		"if (gdWorld.getWorldDropType().equals(npc.getWorldDropType())) return true;",
		"if (gdRating.getRating().equals(npc.getRating())) return true;",
		"if (gdRace.getRace().equals(npc.getRace())) return true;",
		"if (gdTribe.getTribe().equals(npc.getTribe())) return true;",
		"if (npc.isInsideZone(ZoneName.get(gdZone.getZone()))) return true;",
		"if (gdNpc.getNpcId() == npc.getNpcId()) return true;",
		"if (gdGroup.getGroup().equals(npc.getGroupDrop())) return true;",
		"return !rule.getGlobalRuleExcludedNpcs().getNpcIds().contains(npc.getNpcId());",
		"int maxDrops = dropModifiers.getMaxDropsPerGroup() == null ? rule.getMaxDropRule() : dropModifiers.getMaxDropsPerGroup();",
		"if (drops.size() > maxDrops) { List<GlobalDropItem> allowedItems = new ArrayList<>(); for (int i = 0; i < maxDrops && !drops.isEmpty(); "
		"i++) { GlobalDropItem item = Chance.selectElement(drops, true); if (item != null) allowedItems.add(item); } return allowedItems; } return drops;",
		"if (!checkRuleRestrictions(rule, dropModifiers.getDropRace(), npc)) return Collections.emptyList();",
		"if (itemTemplate.getRace() == Race.PC_ALL || itemTemplate.getRace() == dropModifiers.getDropRace()) { int diff = npc.getLevel() - "
		"itemTemplate.getLevel(); if (diff >= rule.getMinDiff() && diff <= rule.getMaxDiff()) tempItems.add(globalItem); }",
		"long count = Rnd.get(item.getMinCount(), item.getMaxCount()); if (item.getId() == ItemId.KINAH) count *= npc.getLevel() * "
		"Math.pow(getRankModifier(npc) * getRatingModifier(npc),",
	],
	"model/drop/DropModifiers.java": [
		"if (allowReductionDropRate && reductionDropRate != null) chance *= reductionDropRate; return chance * boostDropRate;",
	],
	"model/drop/NpcDrop.java": [
		"if (dg.getRace() == Race.PC_ALL || dg.getRace() == dropModifiers.getDropRace()) { index = dg.tryAddDropItems(result, index, dropModifiers, "
		"groupMembers); }",
	],
	"model/drop/DropGroup.java": [
		"Set<Drop> remainingDrops = new HashSet<>(drops); for (int i = 0; i < maxItems && !remainingDrops.isEmpty(); i++) { float chance = "
		"Rnd.chance();",
		"float finalChance = dropModifiers.calculateDropChance(drop.getChance(), isUseLevelBasedChanceReduction()); if (chance < finalChance) {",
		"Drop drop = Rnd.get(nearestDropsOfSameChance); if (drop != null) { index = addDropItem(index, result, drop, groupMembers); "
		"remainingDrops.remove(drop); }",
		"DropItem dropitem = new DropItem(drop); dropitem.calculateCount(); dropitem.setIndex(index++); result.add(dropitem);",
	],
	"model/drop/DropItem.java": [
		"if (DataManager.ITEM_DATA.getItemTemplate(dropTemplate.getItemId()).getOptionSlotBonus() != 0) optionalSocket = -1;",
		"count = Rnd.get(dropTemplate.getMinAmount(), dropTemplate.getMaxAmount());",
	],
	"model/Chance.java": [
		"for (T element : elements) sumOfChances += element.getChance(); if (sumOfChances > 0) { float randomChance = Rnd.nextFloat(sumOfChances); "
		"float luck = 0;",
		"luck += element.getChance(); if (randomChance <= luck) {",
	],
	"controllers/NpcController.java": [
		"allowDecay = owner.getAi().ask(AIQuestion.ALLOW_DECAY);",
		"if (owner.getAi().ask(AIQuestion.REWARD_AP_XP_DP_LOOT)) doReward();",
		"if (attacker.equals(winner) && getOwner().getAi().ask(AIQuestion.REWARD_LOOT)) DropRegistrationService.getInstance().registerDrop("
		"getOwner(), player, player.getLevel(), null);",
	],
	"dataholders/GlobalDropData.java": [
		"if (gr.getGlobalRuleNpcNames() != null) { List<GlobalDropNpc> allowedNpcs = getAllowedNpcs(gr, npcList); if (!allowedNpcs.isEmpty()) { "
		"gr.setNpcs(new GlobalDropNpcs()); gr.getGlobalRuleNpcs().addNpcs(allowedNpcs);",
		"npc.getName().contains(gdNpcName.getValue().toLowerCase())",
		"npc.getName().endsWith(gdNpcName.getValue().toLowerCase())",
		"npc.getName().startsWith(gdNpcName.getValue().toLowerCase())",
		"npc.getName().equalsIgnoreCase(gdNpcName.getValue())",
	],
	"services/QuestService.java": [
		"if (Rnd.chance() >= drop.getChance()) continue;",
		"} else { if (isQuestDrop(player, drop)) { dropItems.add(regQuestDropItem(drop, index++, player.getObjectId())); } }",
	],
}


@dataclass(frozen=True)
class JavaDropRules:
	"""The literals and enum tables the drop evaluator needs, read from the Java sources."""

	rank_modifiers: dict[str, float]      # DropRegistrationService.getRankModifier
	rating_modifiers: dict[str, float]    # DropRegistrationService.getRatingModifier
	kinah_exponent: int                   # getItemCount: Math.pow(rank * rating, N)
	kinah_item: int                       # ItemId.KINAH
	chest_ai: str                         # createDropModifiers: npc.getAi().getName().equals("chest")
	chest_prefix: str                     # ... dropType.startsWith("treasure")
	chest_suffix: str                     # ... dropType.endsWith("box")
	quest_ai: str                         # registerDrop: equals("quest_use_item")
	min_default_level: int                # isAllowedDefaultGlobalDropNpc: npc.getLevel() < 2
	default_level_maps: tuple[int, ...]   # ... WorldMapType.POETA / ISHALGEN ids
	base_boost: int                       # calculateBoostDropRate: getStat(StatEnum.BOOST_DROP_RATE, 100)
	drop_reward: dict[int, int]           # DropRewardEnum(levelDifference, dropRewardPercent)
	loot_effects: dict[int, int]          # DropItem.getLootEffectId
	no_ai: str                            # SpawnTemplate.NO_AI
	drop_rates_default: str               # RatesConfig.DROP_RATES @Property defaultValue
	rule_defaults: dict[str, int]         # GlobalRule minDiff, maxDiff, memberLimit, maxDropRule
	enums: dict[str, tuple[str, ...]]     # Race, TribeClass, NpcRating, NpcRank, GroupDropType, WorldDropType, NpcTemplateType, AbyssNpcType, ...
	teleporter_rewrite: tuple[int, str, str, str]  # NpcTemplate.afterUnmarshal: level > N && !"noaction".equals(ai) && TELEPORTER -> ai
	repose_level: int                     # PlayerCommonData.isReadyForReposeEnergy: getLevel() >= N

	@staticmethod
	def read(java_src: Path) -> "JavaDropRules":
		base = Path(java_src) / "com" / "aionemu" / "gameserver"
		for relative, statements in REQUIRED_STATEMENTS.items():
			text = _normalized(_read(base / relative))
			for statement in statements:
				if statement not in text:
					raise OracleError(f"{relative}: `{statement[:90]}...` not found (the Java source does not have the shape this oracle was "
					                  "written against)")
		service_path = base / "services" / "drop" / "DropRegistrationService.java"
		service = _normalized(_read(service_path))

		def switch_table(method: str, subject: str) -> dict[str, float]:
			body = _search(service, rf"private float {method}\(Npc npc\) \{{ return switch \(npc\.{subject}\(\)\) \{{(.*?)\}};", service_path, method)
			arms = re.findall(r"case (\w+) -> ([0-9.]+)f;", body.group(1))
			if not arms or len(arms) != body.group(1).count("case "):
				raise OracleError(f"{service_path}: {method} is not a table of `case X -> Nf;` arms")
			return {name: java_float(value, f"{method} {name}") for name, value in arms}

		kinah = _search(service, r"Math\.pow\(getRankModifier\(npc\) \* getRatingModifier\(npc\), (\d+)\);", service_path, "the kinah exponent")
		chest = _search(service, r'String dropType = npc\.getGroupDrop\(\)\.name\(\)\.toLowerCase\(\); boolean isChest = npc\.getAi\(\)\.getName\(\)'
		                         r'\.equals\("(\w+)"\) \|\| dropType\.startsWith\("(\w+)"\) \|\| dropType\.endsWith\("(\w+)"\);', service_path,
		                "the chest test of createDropModifiers")
		quest = _search(service, r'boolean isNpcQuest = npc\.getAi\(\)\.getName\(\)\.equals\("(\w+)"\);', service_path, "the quest_use_item test")
		level = _search(service, r"if \(npc\.getLevel\(\) < (\d+) && !isChest && npc\.getWorldId\(\) != WorldMapType\.(\w+)\.getId\(\) && "
		                         r"npc\.getWorldId\(\) != WorldMapType\.(\w+)\.getId\(\)\) return false;", service_path, "the level exception")
		boost = _search(service, r"int boostDropRate = npc\.getGameStats\(\)\.getStat\(StatEnum\.BOOST_DROP_RATE, (\d+)\)\.getCurrent\(\);", service_path,
		                "the base boost")
		world_types = {name: java_int((args or "").split(",")[0].strip(), f"WorldMapType.{name}")  # WorldMapType(int worldId[, boolean ...])
		               for name, args in enum_constants(base / "world" / "WorldMapType.java", "WorldMapType")}
		for name in (level.group(2), level.group(3)):
			if name not in world_types:
				raise OracleError(f"WorldMapType has no {name}")

		item_id = _normalized(_read(base / "model" / "items" / "ItemId.java"))
		kinah_item = int(_search(item_id, r"public static final int KINAH = (\d+);", base / "model" / "items" / "ItemId.java", "ItemId.KINAH").group(1))

		drop_reward: dict[int, int] = {}
		reward_path = base / "utils" / "stats" / "DropRewardEnum.java"
		for name, args in enum_constants(reward_path, "DropRewardEnum"):
			parts = [p.strip() for p in (args or "").split(",")]
			if len(parts) != 2:
				raise OracleError(f"DropRewardEnum.{name}: expected (levelDifference, dropRewardPercent)")
			drop_reward[java_int(parts[0], f"DropRewardEnum.{name}")] = java_int(parts[1], f"DropRewardEnum.{name}")
		reward_text = _normalized(_read(reward_path))
		if "if (levelDifference <= MINUS_10.levelDifference) return MINUS_10.dropRewardPercent; else if (levelDifference >= " \
		   "MINUS_5.levelDifference) return MINUS_5.dropRewardPercent;" not in reward_text:
			raise OracleError(f"{reward_path}: dropRewardFrom does not clamp at MINUS_10 and MINUS_5")

		item_path = base / "model" / "drop" / "DropItem.java"
		effect_body = _search(_normalized(_read(item_path)), r"public int getLootEffectId\(\) \{ return switch \(dropTemplate\.getItemId\(\)\) \{(.*?)\};",
		                      item_path, "getLootEffectId")
		loot_effects: dict[int, int] = {}
		arms = re.findall(r"case ([0-9, ]+) -> (\d+);", effect_body.group(1))
		if "default -> 0;" not in effect_body.group(1) or len(arms) != effect_body.group(1).count("case "):
			raise OracleError(f"{item_path}: getLootEffectId is not a table of `case ids -> N;` arms with `default -> 0`")
		for ids, effect in arms:
			for item in ids.split(","):
				loot_effects[int(item.strip())] = int(effect)

		spawn_path = base / "model" / "templates" / "spawns" / "SpawnTemplate.java"
		no_ai = _search(_read(spawn_path), r'public static final String NO_AI = "([^"]*)";', spawn_path, "SpawnTemplate.NO_AI").group(1)
		rates_path = base / "configs" / "main" / "RatesConfig.java"
		rates = _search(_normalized(_read(rates_path)), r'@Property\(key = "gameserver\.rates\.drop", defaultValue = "([^"]*)"\) public static float\[\] '
		                                                r'DROP_RATES;', rates_path, "RatesConfig.DROP_RATES")
		rule_path = base / "model" / "templates" / "globaldrops" / "GlobalRule.java"
		rule_text = _normalized(_read(rule_path))
		defaults = {}
		for field_name in ("minDiff", "maxDiff", "memberLimit", "maxDropRule"):
			defaults[field_name] = int(_search(rule_text, rf"private int {field_name} = (-?\d+);", rule_path, f"GlobalRule.{field_name}").group(1))

		enum_files = {
			"Race": base / "model" / "Race.java",
			"TribeClass": base / "model" / "TribeClass.java",
			"NpcRating": base / "model" / "templates" / "npc" / "NpcRating.java",
			"NpcRank": base / "model" / "templates" / "npc" / "NpcRank.java",
			"GroupDropType": base / "model" / "templates" / "npc" / "GroupDropType.java",
			"NpcTemplateType": base / "model" / "templates" / "npc" / "NpcTemplateType.java",
			"AbyssNpcType": base / "model" / "templates" / "npc" / "AbyssNpcType.java",
			"WorldDropType": base / "world" / "WorldDropType.java",
			"RestrictionRace": rule_path,
			"StringFunction": base / "model" / "templates" / "globaldrops" / "StringFunction.java",
		}
		enums = {name: enum_names(path, name) for name, path in enum_files.items()}
		rank_modifiers = switch_table("getRankModifier", "getRank")
		rating_modifiers = switch_table("getRatingModifier", "getRating")
		if set(rank_modifiers) != set(enums["NpcRank"]) or set(rating_modifiers) != set(enums["NpcRating"]):
			raise OracleError(f"{service_path}: the rank/rating modifier switches do not cover NpcRank/NpcRating")

		template_path = base / "model" / "templates" / "npc" / "NpcTemplate.java"
		template_text = _normalized(_read(template_path))
		rewrite = _search(template_text, r'protected void afterUnmarshal\(Unmarshaller u, Object parent\) \{ if \(level > (\d+) && !"(\w+)"\.equals\(ai\) '
		                                 r'&& getAbyssNpcType\(\)\.equals\(AbyssNpcType\.(\w+)\)\) ai = "(\w+)"; if \(ai != null\) ai = ai\.intern\(\); \}',
		                  template_path, "the ai rewrite of NpcTemplate.afterUnmarshal")
		if "return abyssNpcType != null ? abyssNpcType : AbyssNpcType.NONE;" not in template_text:
			raise OracleError(f"{template_path}: getAbyssNpcType no longer defaults to NONE")
		if rewrite.group(3) not in enums["AbyssNpcType"]:
			raise OracleError(f"{template_path}: AbyssNpcType has no {rewrite.group(3)}")
		common_path = base / "model" / "gameobjects" / "player" / "PlayerCommonData.java"
		common = _normalized(_read(common_path))
		repose = _search(common, r"public boolean isReadyForReposeEnergy\(\) \{ return getLevel\(\) >= (\d+); \}", common_path, "isReadyForReposeEnergy")
		if "if (!isReadyForReposeEnergy()) { reposeCurrent = 0; reposeMax = 0; }" not in common:
			raise OracleError(f"{common_path}: updateMaxRepose no longer clears the repose energy below the repose level")
		return JavaDropRules(rank_modifiers, rating_modifiers, int(kinah.group(1)), kinah_item, chest.group(1), chest.group(2), chest.group(3),
		                     quest.group(1), int(level.group(1)), (world_types[level.group(2)], world_types[level.group(3)]), int(boost.group(1)),
		                     drop_reward, loot_effects, no_ai, rates.group(1), defaults, enums,
		                     (int(rewrite.group(1)), rewrite.group(2), rewrite.group(3), rewrite.group(4)), int(repose.group(1)))

	def drop_reward_from(self, level_difference: int) -> int:
		"""DropRewardEnum.dropRewardFrom: MINUS_10 at or below -10, MINUS_5 (100 %) at or above -5, else the exact constant."""
		lowest, highest = min(self.drop_reward), max(self.drop_reward)
		if level_difference <= lowest:
			return self.drop_reward[lowest]
		if level_difference >= highest:
			return self.drop_reward[highest]
		if level_difference not in self.drop_reward:
			raise OracleError(f"DropRewardEnum has no constant for {level_difference} (Java: NoSuchElementException)")
		return self.drop_reward[level_difference]

	def enum_value(self, enum: str, text: str | None, what: str) -> str | None:
		if text is None:
			return None
		value = text.strip()
		if value not in self.enums[enum]:
			raise OracleError(f"{what}={text!r} is not a {enum} constant (JAXB would leave it null)")
		return value


def enum_names(source: Path, enum_name: str) -> tuple[str, ...]:
	"""The constant names of `enum enum_name { ... }` in declaration order; the constant list ends at the first top-level `;` or at the
	enum's closing brace (GroupDropType has no `;`)."""
	text = _strip_comments(_read(source))
	head = re.search(r"\benum\s+" + enum_name + r"\b[^{]*\{", text)
	if not head:
		raise OracleError(f"{source}: enum {enum_name} not found")
	names, current, depth = [], "", 0
	for ch in text[head.end():]:
		if ch in "({":
			depth += 1
		elif ch in ")}":
			if depth == 0:
				break
			depth -= 1
		if depth == 0 and ch in ",;":
			if current.strip():
				names.append(current)
			current = ""
			if ch == ";":
				break
			continue
		current += ch
	else:
		raise OracleError(f"{source}: enum {enum_name} is not closed")
	if current.strip():
		names.append(current)
	result = []
	for item in names:
		match = re.match(r"^\s*(?:@\w+(?:\([^)]*\))?\s*)*([A-Za-z_][A-Za-z0-9_]*)\s*(?:\(.*\))?\s*(?:\{.*\})?\s*$", item, re.DOTALL)
		if not match:
			raise OracleError(f"{source}: cannot parse the enum constant {item.strip()!r}")
		result.append(match.group(1))
	return tuple(result)


def java_float_rates(text: str, what: str) -> list[float]:
	"""A float[] config property: comma separated Float.parseFloat values."""
	return [java_float(part, what) for part in text.split(",")]


# ---------------------------------------------------------------------------------------------------------------------------------------------
# AI classes

@dataclass
class AiClass:
	name: str
	path: Path
	extends: str | None
	answers: dict[str, str] | str | None  # question -> "true" | "false" | "super"; "unparsed" when ask() has another shape; None: no ask()
	drop_hook: bool                        # declares handleDropRegistered()
	calls_register_drop: bool = False      # calls DropRegistrationService.registerDrop / AIActions.registerDrop itself (ChestAI: on use)


class AiCatalog:
	"""@AIName -> class and the `extends` chains of every AI class under data/handlers/ai and src/.../ai."""

	def __init__(self, java_src: Path, handlers_dir: Path):
		self.by_class: dict[str, list[AiClass]] = {}
		self.by_ai_name: dict[str, AiClass] = {}
		self.nested_ai_names: dict[str, Path] = {}
		roots = [Path(java_src) / "com" / "aionemu" / "gameserver" / "ai", Path(handlers_dir) / "ai"]
		for root in roots:
			if not root.is_dir():
				raise OracleError(f"{root} does not exist: the AI classes decide whether a kill registers a drop")
			for path in sorted(root.rglob("*.java")):
				self._add(path)
		for required in ("NpcAI", "AITemplate"):
			if len(self.by_class.get(required, [])) != 1:
				raise OracleError(f"the AI class {required} is not found exactly once")

	def _add(self, path: Path) -> None:
		text = _strip_comments(_read(path))
		stem = path.stem
		head = re.search(r"\bclass\s+" + re.escape(stem) + r"\s*(<(?:[^<>]|<[^<>]*>)*>)?\s*(?:extends\s+(\w+))?", text)
		if not head:
			return  # an interface or enum
		for match in re.finditer(r'@AIName\(\s*"([^"]+)"\s*\)\s*(?:@\w+(?:\([^)]*\))?\s*)*(?:(?:public|protected|private|abstract|final|static)\s+)*'
		                         r'class\s+(\w+)', text):
			if match.group(2) != stem:
				self.nested_ai_names[match.group(1)] = path
		answers = _ask_answers(text)
		drop_hook = re.search(r"\bvoid\s+handleDropRegistered\s*\(\s*\)", text) is not None
		if stem == "AITemplate":
			if not re.search(r"protected void handleDropRegistered\(\)\s*\{\s*\}", text):
				raise OracleError(f"{path}: AITemplate.handleDropRegistered is no longer empty")
			drop_hook = False
		cls = AiClass(stem, path, head.group(2), answers, drop_hook, re.search(r"\.\s*registerDrop\s*\(", text) is not None)
		self.by_class.setdefault(stem, []).append(cls)
		for match in re.finditer(r'@AIName\(\s*"([^"]+)"\s*\)', text):
			if self.nested_ai_names.get(match.group(1)) == path:
				continue
			if match.group(1) in self.by_ai_name:
				raise OracleError(f"two AI classes are named {match.group(1)!r} (Java: IllegalArgumentException in AIEngine.registerAI)")
			self.by_ai_name[match.group(1)] = cls

	def chain(self, cls: AiClass) -> list[AiClass]:
		chain = [cls]
		while chain[-1].name != "AITemplate":
			parent = chain[-1].extends
			if parent is None:
				raise OracleError(f"AI class {chain[-1].name} does not extend a class the oracle can follow to AITemplate")
			candidates = self.by_class.get(parent, [])
			if len(candidates) != 1:
				raise OracleError(f"AI class {chain[-1].name} extends {parent}, which is {'ambiguous' if candidates else 'not an AI class'} here")
			if candidates[0] in chain:
				raise OracleError(f"AI class {cls.name}: cyclic extends chain")
			chain.append(candidates[0])
		return chain

	def drop_behaviour(self, ai_name: str | None) -> dict:
		"""What the AI answers to the three questions of NpcController.onDie / doReward, and the classes that decided."""
		if ai_name is None:  # AIEngine.newAI(null): DummyAI extends AITemplate, whose ask() answers false
			template = self.by_class["AITemplate"][0]
			answers = {q: self._answer([template], q) for q in AI_QUESTIONS}
			return {"name": "noname", "aiName": None, "classChain": ["DummyAI", "AITemplate"], "registersDropItself": [], **answers}
		if ai_name in self.nested_ai_names:
			raise OracleError(f"AI {ai_name!r} is a nested class ({self.nested_ai_names[ai_name].name}), which the oracle does not model")
		cls = self.by_ai_name.get(ai_name)
		if cls is None:
			raise OracleError(f"no AI class is named {ai_name!r} (Java: IllegalArgumentException in AIEngine.newAI, the npc never spawns)")
		chain = self.chain(cls)
		hooks = [c.name for c in chain if c.drop_hook]
		if hooks:
			raise OracleError(f"AI {ai_name!r}: {hooks[0]} overrides handleDropRegistered, which may rewrite the drop; the oracle does not model it")
		answers = {q: self._answer(chain, q) for q in AI_QUESTIONS}
		return {"name": ai_name, "aiName": ai_name, "classChain": [c.name for c in chain],
		        "registersDropItself": [c.name for c in chain if c.calls_register_drop], **answers}

	@staticmethod
	def _answer(chain: list[AiClass], question: str) -> bool:
		for cls in chain:
			if cls.answers is None:
				continue
			if cls.answers == "unparsed":
				raise OracleError(f"{cls.path.name}: ask(AIQuestion) is not `return switch (question) {{ case ... -> true|false|super.ask(question); }}` "
				                  f"or `return true|false;`, so the oracle cannot tell what it answers to {question}")
			answer = cls.answers.get(question)
			if answer is None:
				raise OracleError(f"{cls.path.name}: ask(AIQuestion) answers {question} with an expression the oracle does not model")
			if answer != "super":
				return answer == "true"
		raise OracleError(f"no class of the AI chain {[c.name for c in chain]} answers {question}")


def _method_body(text: str, head: re.Match) -> str | None:
	start = text.find("{", head.end() - 1)
	if start < 0:
		return None
	depth = 0
	for position in range(start, len(text)):
		if text[position] == "{":
			depth += 1
		elif text[position] == "}":
			depth -= 1
			if depth == 0:
				return text[start + 1:position]
	return None


def _top_level_arms(body: str) -> list[str]:
	"""The arms of a switch body, split where `case` or `default` starts outside any nested (), {} (a nested switch expression stays inside
	its arm). Text before the first arm makes the whole body unparseable (returned as an arm that matches nothing)."""
	starts, depth = [], 0
	for match in re.finditer(r"[(){}]|\b(?:case|default)\b", body):
		token = match.group(0)
		if token in "({":
			depth += 1
		elif token in ")}":
			depth -= 1
		elif depth == 0:
			starts.append(match.start())
	if not starts:
		return [body] if body.strip() else []
	arms = [body[:starts[0]]] if body[:starts[0]].strip() else []
	arms += [body[start:end].strip() for start, end in zip(starts, starts[1:] + [len(body)])]
	return arms


def _ask_answers(text: str) -> dict[str, str] | str | None:
	"""The answers of `boolean ask(AIQuestion x)` to AI_QUESTIONS: "true"/"false"/"super"; None for an unmodelled arm; "unparsed" for a body shape
	the oracle does not model; None (the whole result) when the class does not declare ask()."""
	heads = list(re.finditer(r"\bboolean\s+ask\s*\(\s*AIQuestion\s+(\w+)\s*\)\s*\{", text))
	if not heads:
		return None
	if len(heads) > 1:
		return "unparsed"
	variable = heads[0].group(1)
	body = _method_body(text, heads[0])
	if body is None:
		return "unparsed"
	simple = re.fullmatch(r"\s*return\s+(true|false|super\.ask\(\s*" + variable + r"\s*\))\s*;\s*", body)
	if simple:
		answer = "super" if simple.group(1).startswith("super") else simple.group(1)
		return {q: answer for q in AI_QUESTIONS}
	switch = re.fullmatch(r"\s*return\s+switch\s*\(\s*" + variable + r"\s*\)\s*\{(.*)\}\s*;\s*", body, re.DOTALL)
	if not switch:
		return "unparsed"
	labelled: dict[str, str | None] = {}
	default_value: str | None = "missing"
	for arm in _top_level_arms(switch.group(1)):
		head = re.match(r"(?:case\s+([A-Z_][A-Z0-9_]*(?:\s*,\s*[A-Z_][A-Z0-9_]*)*)|default)\s*->\s*(.*)$", arm, re.DOTALL)
		if not head:
			return "unparsed"  # an old-style `case X:` arm, a pattern label, ...
		expression = re.fullmatch(r"\s*(true|false|super\.ask\(\s*" + variable + r"\s*\))\s*;\s*", head.group(2))
		value = None if not expression else "super" if expression.group(1).startswith("super") else expression.group(1)
		if head.group(1) is None:
			default_value = value
		else:
			for label in head.group(1).split(","):
				labelled[label.strip()] = value
	answers: dict[str, str | None] = {}
	for question in AI_QUESTIONS:
		if question in labelled:
			answers[question] = labelled[question]
		elif default_value == "missing":
			answers[question] = None
		else:
			answers[question] = default_value
	return answers


# ---------------------------------------------------------------------------------------------------------------------------------------------
# Static data

@dataclass(frozen=True)
class NpcTemplateInfo:
	npc_id: int
	name: str | None
	level: int
	rank: str | None
	rating: str | None
	race: str
	tribe: str | None
	group_drop: str | None
	ai: str | None             # after NpcTemplate.afterUnmarshal's siege_teleporter rewrite
	npc_type: str
	abyss_type: str
	xml_ai: str | None = None  # the ai attribute as written


@dataclass(frozen=True)
class GdItem:
	item_id: int
	min_count: int
	max_count: int
	chance: float


@dataclass
class GdRule:
	index: int
	file: str
	ordinal: int
	name: str
	chance: float
	min_diff: int
	max_diff: int
	restriction_race: str | None
	level_based: bool
	member_limit: int
	max_drop_rule: int
	dynamic: bool
	items: list[GdItem]
	lists: dict[str, list | None]              # RESTRICTION_LISTS keys -> values (None: no element, so no restriction)
	npc_names: list[tuple[str, str]] | None    # gd_npc_names (function, value)
	excluded: set[int] | None                  # gd_excluded_npcs npc_ids
	npcs_from_names: list[int] = field(default_factory=list)  # processRules additions


@dataclass(frozen=True)
class ItemInfo:
	item_id: int
	name: str | None
	level: int
	race: str
	quality: str | None
	max_stack_count: int
	option_slot_bonus: int


@dataclass(frozen=True)
class CustomDropEntry:
	item_id: int
	min_amount: int
	max_amount: int
	chance: float
	each_member: bool


@dataclass(frozen=True)
class CustomGroup:
	name: str | None
	race: str
	level_based: bool
	max_items: int
	drops: tuple[CustomDropEntry, ...]


@dataclass(frozen=True)
class QuestDropInfo:
	quest_id: int
	npc_id: int
	item_id: int
	chance: int
	drop_each_member: int
	collecting_step: int
	handler_side: bool
	needed_amount: int | None
	target: str
	mentor_type: str
	collect_items: tuple[tuple[int, int], ...] | None


def _rule_from_element(element: ET.Element, rules: JavaDropRules, index: int, file: str, ordinal: int) -> GdRule:
	where = f"{file} gd_rule #{ordinal}"
	if element.tag != "gd_rule":
		raise OracleError(f"{file}: <{element.tag}> is not a gd_rule")
	for attribute in element.attrib:
		if attribute not in RULE_ATTRIBUTES:
			raise OracleError(f"{where}: unknown attribute {attribute!r}")
	name = element.get("rule_name")
	if name is None:
		raise OracleError(f"{where}: missing required rule_name")
	where = f"{where} ({name})"
	children = {}
	for child in element:
		if child.tag not in RULE_CHILDREN:
			raise OracleError(f"{where}: unknown element <{child.tag}>")
		if child.tag in children:
			raise OracleError(f"{where}: <{child.tag}> twice (JAXB keeps the last one)")
		children[child.tag] = child
	items_element = children.get("gd_items")
	if items_element is None:
		raise OracleError(f"{where}: no <gd_items> (Java: NullPointerException in collectAllowedDrops)")
	items = []
	for item in items_element:
		if item.tag != "gd_item" or set(item.attrib) - {"id", "min_count", "max_count", "chance"}:
			raise OracleError(f"{where}: unexpected <{item.tag} {sorted(item.attrib)}> in gd_items")
		item_id = java_int(item.get("id"), f"{where} gd_item id")
		minimum = java_int(item.get("min_count"), f"{where} gd_item {item_id} min_count", 1)
		maximum = java_int(item.get("max_count"), f"{where} gd_item {item_id} max_count", 0)
		if minimum <= 0:  # GlobalDropItem.afterUnmarshal (model/templates/globaldrops/GlobalDropItem.java:31-43)
			raise OracleError(f"{where}: gd_item {item_id} min_count {minimum} (Java: IllegalArgumentException at startup)")
		if maximum == 0:
			maximum = minimum
		elif maximum < minimum:
			raise OracleError(f"{where}: gd_item {item_id} max_count < min_count (Java: IllegalArgumentException at startup)")
		items.append(GdItem(item_id, minimum, maximum, java_float(item.get("chance"), f"{where} gd_item {item_id} chance", 100.0)))
	lists: dict[str, list | None] = {}
	for tag, (entry_tag, attribute, enum) in RESTRICTION_LISTS.items():
		wrapper = children.get(tag)
		if wrapper is None:
			lists[tag] = None
			continue
		values = []
		for entry in wrapper:
			if entry.tag != entry_tag or set(entry.attrib) != {attribute}:
				raise OracleError(f"{where}: unexpected <{entry.tag} {sorted(entry.attrib)}> in <{tag}>")
			text = entry.get(attribute)
			if enum is None:
				values.append(java_int(text, f"{where} {entry_tag} {attribute}"))
			elif enum == "zone":
				values.append(text)
			else:
				values.append(rules.enum_value(enum, text, f"{where} {entry_tag} {attribute}"))
		lists[tag] = values
	npc_names = None
	if "gd_npc_names" in children:
		npc_names = []
		for entry in children["gd_npc_names"]:
			if entry.tag != "gd_npc_name" or set(entry.attrib) != {"function", "value"}:
				raise OracleError(f"{where}: unexpected <{entry.tag}> in gd_npc_names")
			function = rules.enum_value("StringFunction", entry.get("function"), f"{where} gd_npc_name function")
			if not entry.get("value").isascii():
				raise OracleError(f"{where}: gd_npc_name value {entry.get('value')!r} is not ASCII (String.toLowerCase() depends on the locale)")
			npc_names.append((function, entry.get("value")))
	excluded = None
	if "gd_excluded_npcs" in children:
		ids = children["gd_excluded_npcs"].get("npc_ids")
		if ids is None:
			raise OracleError(f"{where}: gd_excluded_npcs without npc_ids (Java: NullPointerException in checkGlobalRuleExcludedNpcs)")
		excluded = {java_int(i, f"{where} gd_excluded_npcs") for i in ids.split()}
	restriction = element.get("restriction_race")
	return GdRule(
		index, file, ordinal, name,
		java_float(element.get("chance"), f"{where} chance"),
		java_int(element.get("min_diff"), f"{where} min_diff", rules.rule_defaults["minDiff"]),
		java_int(element.get("max_diff"), f"{where} max_diff", rules.rule_defaults["maxDiff"]),
		rules.enum_value("RestrictionRace", restriction, f"{where} restriction_race"),
		java_boolean(element.get("level_based_chance_reduction")),
		java_int(element.get("member_limit"), f"{where} member_limit", rules.rule_defaults["memberLimit"]),
		java_int(element.get("max_drop_rule"), f"{where} max_drop_rule", rules.rule_defaults["maxDropRule"]),
		java_boolean(element.get("dynamic_chance")),
		items, lists, npc_names, excluded)


def _scan_attributes(paths: list[Path], tag: str) -> list[dict[str, str]]:
	"""The attributes of every `tag` element directly below the root of each file, in document order (expat without building the elements:
	the two biggest holders, item and npc templates, need only their root children's attributes here)."""
	result: list[dict[str, str]] = []
	for path in paths:
		depth = 0

		def start(name: str, attributes: dict[str, str]) -> None:
			nonlocal depth
			depth += 1
			if depth == 2 and name == tag:
				result.append(attributes)

		def end(name: str) -> None:
			nonlocal depth
			depth -= 1

		parser = expat.ParserCreate()
		parser.StartElementHandler = start
		parser.EndElementHandler = end
		try:
			with open(path, "rb") as stream:
				parser.ParseFile(stream)
		except (OSError, expat.ExpatError) as e:
			raise OracleError(f"{path}: {e}") from e
	return result


class DropData:
	"""Every static data table the drop evaluator reads, loaded once (a real-data report reuses it for several npcs)."""

	def __init__(self, data: StaticData, java_src: Path, handlers_dir: Path | None = None):
		self.data = data
		self.java_src = Path(java_src)
		self.handlers_dir = Path(handlers_dir) if handlers_dir is not None else self.java_src.parent / "data" / "handlers"
		self.rules = JavaDropRules.read(self.java_src)
		self.ai = AiCatalog(self.java_src, self.handlers_dir)
		self.config_dir = self.java_src.parent / "config"
		self.npcs = self._load_npcs()
		self.global_rules = self._load_rules()
		self._process_rules()
		self.exclusions = self._load_exclusions()
		self.custom_drops = self._load_custom_drops()
		self.quests = self._load_quests()
		self.events = self._load_events()
		wanted = {item.item_id for rule in self.global_rules for item in rule.items}
		wanted |= {item.item_id for rules in self.events.values() for rule in rules for item in rule.items}
		wanted |= {d.item_id for groups in self.custom_drops.values() for g in groups for d in g.drops}
		wanted |= {q.item_id for drops in self.quests.values() for q in drops}
		self.items = self._load_items(wanted)
		for rule in self.global_rules + [r for rules in self.events.values() for r in rules]:
			for item in rule.items:
				if item.item_id not in self.items:  # GlobalDropItem.afterUnmarshal: "Global drop item ID ... is invalid"
					raise OracleError(f"{rule.file} rule {rule.name!r}: gd_item {item.item_id} has no item template (Java: IllegalArgumentException at "
					                  "startup)")
		self.max_player_level = sum(1 for _ in data.children("player_experience_table", "exp"))
		self._spawn_maps: dict[int, set[int]] | None = None
		self._spawn_rows: dict[int, list[dict]] = {}
		self._handlers: dict[int, str | None] = {}

	def _load_npcs(self) -> dict[int, NpcTemplateInfo]:
		"""NpcData.init: a later template of the same id replaces the earlier one. NpcTemplate.afterUnmarshal (NpcTemplate.java:122-127) replaces
		the ai of a TELEPORTER above level 1 with siege_teleporter unless it is noaction."""
		npcs = {}
		min_level, kept_ai, abyss, new_ai = self.rules.teleporter_rewrite
		for element in _scan_attributes(self.data.files("npc_templates"), "npc_template"):
			npc_id = java_int(element.get("npc_id"), "npc_template npc_id")
			level = java_int(element.get("level"), f"npc_template {npc_id} level", 0)
			if not -128 <= level <= 127:
				raise OracleError(f"npc_template {npc_id} level {level} is out of the byte range")
			# enum attributes are validated when an evaluation reads them (NpcTemplate.java:25-114; type and abyss_type default to NONE in
			# their getters, race to NONE in the field)
			xml_ai = element.get("ai")
			ai = new_ai if level > min_level and xml_ai != kept_ai and (element.get("abyss_type") or "").strip() == abyss else xml_ai
			npcs[npc_id] = NpcTemplateInfo(
				npc_id, element.get("name"), level,
				element.get("rank"), element.get("rating"), element.get("race", "NONE"), element.get("tribe"), element.get("group_drop"),
				ai, element.get("type", "NONE"), element.get("abyss_type", "NONE"), xml_ai)
		return npcs

	def _load_rules(self) -> list[GdRule]:
		"""GlobalDropData.globalDropRules: the gd_rule children of every file of the global_rules import, in merged order."""
		rules = []
		for path in self.data.files("global_rules"):
			try:
				root = ET.parse(path).getroot()
			except ET.ParseError as e:
				raise OracleError(f"{path}: {e}") from e
			relative = path.relative_to(self.data.dir).as_posix()
			for ordinal, element in enumerate(root):
				rules.append(_rule_from_element(element, self.rules, len(rules), relative, ordinal))
		return rules

	def _process_rules(self) -> None:
		"""GlobalDropData.processRules over NpcData.getNpcData() (every template, the last one per id)."""
		named = [rule for rule in self.global_rules if rule.npc_names is not None]
		if not named:
			return
		for template in self.npcs.values():
			if template.name is None:
				raise OracleError(f"npc_template {template.npc_id} has no name while gd_npc_names rules exist (Java: NullPointerException in "
				                  "GlobalDropData.processRules at startup)")
		for rule in named:
			matched = []
			for function, value in rule.npc_names:
				lower = value.lower()
				for template in self.npcs.values():
					name = template.name
					if function == "CONTAINS":
						hit = lower in name
					elif function == "END_WITH":
						hit = name.endswith(lower)
					elif function == "START_WITH":
						hit = name.startswith(lower)
					else:  # EQUALS: equalsIgnoreCase
						if not name.isascii():
							raise OracleError(f"rule {rule.name!r}: EQUALS against the non-ASCII npc name {name!r} is not modelled")
						hit = name.lower() == lower
					if hit:
						matched.append(template.npc_id)
			allowed = (rule.lists["gd_npcs"] or []) + matched
			if allowed:
				rule.lists["gd_npcs"] = allowed
				rule.npcs_from_names = matched
				rule.npc_names = []

	def _load_exclusions(self) -> dict[str, set | None]:
		"""GlobalNpcExclusionData (dataholders/GlobalNpcExclusionData.java): five @XmlList sets, isEmpty when all five are absent."""
		result: dict[str, set | None] = {"npc_ids": None, "npc_names": None, "npc_types": None, "npc_tribes": None, "npc_abyss_types": None}
		enums = {"npc_types": "NpcTemplateType", "npc_tribes": "TribeClass", "npc_abyss_types": "AbyssNpcType"}
		for element in self.data.children("global_npc_exclusions"):
			if element.tag not in result:
				raise OracleError(f"global_npc_exclusions: unknown element <{element.tag}>")
			tokens = (element.text or "").split()
			if element.tag == "npc_ids":
				values = {java_int(t, "global_npc_exclusions npc_ids") for t in tokens}
			elif element.tag == "npc_names":
				values = set(tokens)
			else:
				values = {self.rules.enum_value(enums[element.tag], t, f"global_npc_exclusions {element.tag}") for t in tokens}
			result[element.tag] = values  # JAXB: a repeated element replaces the set
		return result

	def _load_custom_drops(self) -> dict[int, list[CustomGroup]]:
		"""CustomDrop.afterUnmarshal: the FIRST <npc_drop> of an npc id wins (putIfAbsent), after JAXB has run Drop.afterUnmarshal on the <drop>s
		of every <npc_drop>, a repeated one too."""
		drops: dict[int, list[CustomGroup]] = {}
		for element in self.data.children("custom_drop", "npc_drop"):
			npc_id = java_int(element.get("npc_id"), "npc_drop npc_id")
			entries_per_group = [self._custom_entries(group, npc_id) for group in element.findall("drop_group")]
			if npc_id in drops:
				continue
			drops[npc_id] = [CustomGroup(group.get("name"), self.rules.enum_value("Race", group.get("race", "PC_ALL"), f"npc_drop {npc_id} race"),
			                             java_boolean(group.get("level_based_chance_reduction")),
			                             java_int(group.get("max_items"), f"npc_drop {npc_id} max_items", 1), entries)
			                 for group, entries in zip(element.findall("drop_group"), entries_per_group)]
		return drops

	@staticmethod
	def _custom_entries(group: ET.Element, npc_id: int) -> tuple[CustomDropEntry, ...]:
		"""The <drop>s of a drop_group through Drop.afterUnmarshal (model/drop/Drop.java:41-50)."""
		entries = []
		for drop in group.findall("drop"):
			what = f"npc_drop {npc_id} drop {drop.get('item_id')}"
			entry = CustomDropEntry(java_int(drop.get("item_id"), f"{what} item_id", 0),
			                        java_int(drop.get("min_amount"), f"{what} min_amount", 1),
			                        java_int(drop.get("max_amount"), f"{what} max_amount", 0),
			                        java_float(drop.get("chance"), f"{what} chance", 100.0), java_boolean(drop.get("each_member")))
			if entry.chance <= 0 or entry.min_amount <= 0 or (entry.max_amount != 0 and entry.max_amount < entry.min_amount):
				raise OracleError(f"{what}: Drop.afterUnmarshal throws IllegalArgumentException at startup")
			if entry.max_amount == 0:
				entry = CustomDropEntry(entry.item_id, entry.min_amount, entry.min_amount, entry.chance, entry.each_member)
			entries.append(entry)
		return tuple(entries)

	def _load_quests(self) -> dict[int, list[QuestDropInfo]]:
		"""QuestEngine.init (questEngine/QuestEngine.java:86-91) over QuestsData (the last <quest> of an id) plus the handler side drops that
		quest handlers register with addHandlerSideQuestDrop (:901-909)."""
		templates: dict[int, dict] = {}
		for element in self.data.stream("quests", "quest"):
			quest_id = java_int(element.get("id"), "quest id")
			collect = element.find("collect_items")
			templates[quest_id] = {
				"target": element.get("target", "NONE"),
				"mentor": element.get("mentor_type", "NONE"),
				"collect": tuple((java_int(c.get("item_id"), f"quest {quest_id} collect_item", 0), java_int(c.get("count"), f"quest {quest_id} count", 0))
				                 for c in collect.findall("collect_item")) if collect is not None else None,
				"drops": [(java_int(d.get("npc_id"), f"quest {quest_id} quest_drop npc_id"), java_int(d.get("item_id"), f"quest {quest_id} item_id"),
				           java_int(d.get("chance"), f"quest {quest_id} chance", 100), java_int(d.get("drop_each_member"), "drop_each_member", 0),
				           java_int(d.get("collecting_step"), "collecting_step", 0)) for d in element.findall("quest_drop")],
			}
		drops: dict[int, list[QuestDropInfo]] = {}
		for quest_id, template in templates.items():
			for npc_id, item_id, chance, each, step in template["drops"]:
				drops.setdefault(npc_id, []).append(QuestDropInfo(quest_id, npc_id, item_id, chance, each, step, False, None, template["target"],
				                                                  template["mentor"], template["collect"]))
		quest_dir = self.handlers_dir / "quest"
		if not quest_dir.is_dir():
			raise OracleError(f"{quest_dir} does not exist: quest handlers register handler side quest drops")
		call = re.compile(r"addHandlerSideQuestDrop\s*\(([^;]*)\)\s*;")
		for path in sorted(quest_dir.rglob("*.java")):
			text = _read(path)
			if "addHandlerSideQuestDrop" not in text:
				continue
			text = _strip_comments(text)
			quest = re.findall(r"\bsuper\s*\(\s*(\d+)\s*\)", text)
			for match in call.finditer(text):
				args = [a.strip() for a in match.group(1).split(",")]
				if len(quest) != 1 or args[0] != "questId" or len(args) not in (5, 6) or not all(re.fullmatch(r"\d+", a) for a in args[1:]):
					raise OracleError(f"{path.name}: addHandlerSideQuestDrop({match.group(1)}) is not a call with literal arguments in a handler with "
					                  "one super(questId); the oracle cannot tell which npc it registers a drop for")
				quest_id = int(quest[0])
				template = templates.get(quest_id)
				if template is None:
					raise OracleError(f"{path.name}: quest {quest_id} has no template (Java: NullPointerException in HandlerSideDrop)")
				npc_id, item_id, amount, chance = (int(a) for a in args[1:5])
				step = int(args[5]) if len(args) == 6 else 0
				each = next((d[3] for d in template["drops"] if d[0] == npc_id and d[1] == item_id), 0)  # HandlerSideDrop copies dropEachMember
				drops.setdefault(npc_id, []).append(QuestDropInfo(quest_id, npc_id, item_id, chance, each, step, True, amount, template["target"],
				                                                  template["mentor"], template["collect"]))
		return drops

	def _load_events(self) -> dict[str, list[GdRule]]:
		"""EventTemplate.eventDropRules of every timed event (model/templates/event/EventTemplate.java:46-48): listed, never evaluated."""
		events: dict[str, list[GdRule]] = {}
		for event in self.data.children("timed_events", "event"):
			wrapper = event.find("event_drops")
			if wrapper is None:
				continue
			name = event.get("name")
			events[name] = [_rule_from_element(r, self.rules, i, f"timed event {name}", i) for i, r in enumerate(wrapper.findall("gd_rule"))]
		return events

	def _load_items(self, wanted: set[int]) -> dict[int, ItemInfo]:
		items = {}
		for element in _scan_attributes(self.data.files("item_templates"), "item_template"):
			item_id = java_int(element.get("id"), "item_template id")
			if item_id not in wanted:
				continue
			items[item_id] = ItemInfo(item_id, element.get("name"), java_int(element.get("level"), f"item {item_id} level", 0),
			                          self.rules.enum_value("Race", element.get("race", "PC_ALL"), f"item {item_id} race"), element.get("quality"),
			                          java_int(element.get("max_stack_count"), f"item {item_id} max_stack_count", 1),
			                          java_int(element.get("option_slot_bonus"), f"item {item_id} option_slot_bonus", 0))
		return items

	def item(self, item_id: int, what: str) -> ItemInfo:
		info = self.items.get(item_id)
		if info is None:
			raise OracleError(f"{what}: item {item_id} has no item template (Java: NullPointerException in new DropItem)")
		return info

	def map_info(self, map_id: int) -> dict:
		element = _map_template(self.data, map_id)
		return {"id": map_id, "name": element.get("name"), "worldType": element.get("world_type", "NONE"),
		        "dropType": self.rules.enum_value("WorldDropType", element.get("drop_type", "NONE"), f"map {map_id} drop_type"),
		        "instance": java_boolean(element.get("instance"))}

	def spawn_maps(self, npc_id: int) -> list[int]:
		"""The maps with a regular <spawn> of the npc id."""
		if self._spawn_maps is None:
			self._spawn_maps = {}
			for spawn_map in self.data.children("spawns", "spawn_map"):
				map_id = java_int(spawn_map.get("map_id"), "spawn_map map_id")
				for spawn in spawn_map.findall("spawn"):
					self._spawn_maps.setdefault(java_int(spawn.get("npc_id"), "spawn npc_id"), set()).add(map_id)
		return sorted(self._spawn_maps.get(npc_id, ()))

	def spawn_rows(self, map_id: int) -> list[dict]:
		"""m5a/spawns.py evaluate over SpawnsData.getSpawnsByWorldId, with an unknown game clock (a temporary spawn may be spawned)."""
		if map_id not in self._spawn_rows:
			infos = {npc_id: NpcInfo(t.level, t.npc_type) for npc_id, t in self.npcs.items()}
			self._spawn_rows[map_id] = evaluate(load_groups(self.data, map_id), infos, GameClock())
		return self._spawn_rows[map_id]

	def instance_handler(self, map_id: int) -> str | None:
		"""The @InstanceID(map) handler class under data/handlers (InstanceEngine), whose onDropRegistered may change the drop."""
		if map_id not in self._handlers:
			self._handlers[map_id] = _instance_handler_class(self.handlers_dir, map_id)
		return self._handlers[map_id]


# ---------------------------------------------------------------------------------------------------------------------------------------------
# Evaluation

@dataclass(frozen=True)
class Killer:
	race: str
	level: int
	drop_rate: float


class DropEvaluator:
	"""registerDrop for one npc id on one map, killed by a solo player (race, level) at a drop rate."""

	def __init__(self, drop_data: DropData, npc_id: int, map_id: int, killer: Killer, rows: list[dict] | None = None):
		self.d = drop_data
		self.rules = drop_data.rules
		self.killer = killer
		self.npc_id = npc_id
		self.map_id = map_id
		self.npc = drop_data.npcs.get(npc_id)
		if self.npc is None:
			raise OracleError(f"no npc_template with npc_id {npc_id}")
		if is_gatherable(npc_id):
			raise OracleError(f"{npc_id} is a gatherable id, not an npc")
		self.map = drop_data.map_info(map_id)
		rows = rows if rows is not None else drop_data.spawn_rows(map_id)
		self.spots = [r for r in rows if r["npcId"] == npc_id and r["spawned"] is not False]
		if not self.spots:
			raise OracleError(f"npc {npc_id} has no regular spawn spot on map {map_id} that can be spawned (the oracle models regular spawns)")
		self.not_modelled: list[str] = []

	# -- the npc
	def _enum(self, enum: str, value: str | None, what: str) -> str | None:
		return self.rules.enum_value(enum, value, f"npc {self.npc_id} {what}")

	def ai_behaviour(self) -> dict:
		names = sorted({(r["spotAi"] if r["spotAi"] is not None else self.npc.ai) for r in self.spots}, key=lambda n: (n is None, n or ""))
		behaviours = []
		for name in names:
			effective = None if name == self.rules.no_ai else name
			behaviours.append(self.d.ai.drop_behaviour(effective))
		decisive = {tuple(b[q] for q in AI_QUESTIONS) + (b["name"] == self.rules.chest_ai, b["name"] == self.rules.quest_ai,
		                                                 tuple(b["registersDropItself"])) for b in behaviours}
		if len(decisive) != 1:
			raise OracleError(f"npc {self.npc_id}: its spots on map {self.map_id} carry AIs that answer the loot questions differently "
			                  f"({[b['name'] for b in behaviours]}); the spot decides, which the oracle does not model")
		return {"aiNames": [b["aiName"] for b in behaviours], "behaviours": behaviours}

	def modifiers(self, ai_name: str) -> dict:
		"""createDropModifiers (DropRegistrationService.java:111-120) for the solo killer."""
		group = self._enum("GroupDropType", self.npc.group_drop, "group_drop")
		if group is None:
			raise OracleError(f"npc {self.npc_id} has no group_drop (Java: NullPointerException in createDropModifiers, nothing is registered)")
		drop_type = group.lower()
		chest = ai_name == self.rules.chest_ai or drop_type.startswith(self.rules.chest_prefix) or drop_type.endswith(self.rules.chest_suffix)
		boost_int = self.rules.base_boost  # no BOOST_DROP_RATE / DR_BOOST function, no repose, no salvation, no palace
		boost = f32(checked_f32(self.killer.drop_rate * boost_int, "boostDropRate: rate * boost") / f32(100.0))
		level_difference = self.npc.level - self.killer.level
		percent = self.rules.drop_reward_from(level_difference)
		reduction = None if percent == 100 else f32(percent / f32(100.0))
		return {"isChest": chest, "dropRace": self.killer.race, "boostDropRateInt": boost_int, "boostDropRate": boost,
		        "levelDifference": level_difference, "dropRewardPercent": percent, "reductionDropRate": reduction}

	def global_exclusion(self) -> str | None:
		"""hasGlobalNpcExclusions (DropRegistrationService.java:287-296): which exclusion list names the npc, None when none does."""
		e = self.d.exclusions
		if all(v is None for v in e.values()):
			return None
		if self.npc_id in (e["npc_ids"] or set()):
			return "npc_ids"
		if self.npc.name in (e["npc_names"] or set()):
			return "npc_names"
		if self._enum("NpcTemplateType", self.npc.npc_type, "type") in (e["npc_types"] or set()):
			return "npc_types"
		tribe = self._enum("TribeClass", self.npc.tribe, "tribe")
		if tribe is not None and tribe in (e["npc_tribes"] or set()):
			return "npc_tribes"
		if self._enum("AbyssNpcType", self.npc.abyss_type, "abyss_type") in (e["npc_abyss_types"] or set()):
			return "npc_abyss_types"
		return None

	def default_exclusion(self, chest: bool) -> str | None:
		"""isAllowedDefaultGlobalDropNpc (DropRegistrationService.java:167-181) for a regular spawn: why it answers false, None when true."""
		if self.npc.level < self.rules.min_default_level and not chest and self.map_id not in self.rules.default_level_maps:
			return f"level {self.npc.level} < {self.rules.min_default_level} outside Poeta and Ishalgen"
		abyss = self._enum("AbyssNpcType", self.npc.abyss_type, "abyss_type")
		if chest:
			return "a chest"
		if abyss not in ("NONE", "DEFENDER"):
			return f"abyss_type {abyss}"
		return None

	# -- the rules
	def rule_failure(self, rule: GdRule) -> tuple[str | None, bool]:
		"""
		checkRuleRestrictions: (the first failing predicate in Java's order or None when all pass, whether a zone list is left to decide). The
		predicates have no side effects and are ANDed, so a non-empty gd_zones list is looked at last: a later failing predicate decides without
		it (and is the one reported); with every other predicate passing, the zones decide (the caller refuses unless the rule adds no entry
		either way). An empty wrapper element (e.g. <gd_maps/>) matches nothing, as Java's loop over an empty list does.
		"""
		npc = self.npc
		lists = rule.lists
		zone_pending = False
		for predicate in PREDICATES:
			if predicate == "restrictionRace":
				failed = rule.restriction_race is not None and rule.restriction_race != self.killer.race
			elif predicate == "maps":
				failed = lists["gd_maps"] is not None and self.map_id not in lists["gd_maps"]
			elif predicate == "worlds":
				failed = lists["gd_worlds"] is not None and self.map["dropType"] not in lists["gd_worlds"]
			elif predicate == "ratings":
				failed = lists["gd_ratings"] is not None and self._enum("NpcRating", npc.rating, "rating") not in lists["gd_ratings"]
			elif predicate == "races":
				failed = lists["gd_races"] is not None and self._enum("Race", npc.race, "race") not in lists["gd_races"]
			elif predicate == "tribes":
				failed = lists["gd_tribes"] is not None and self._enum("TribeClass", npc.tribe, "tribe") not in lists["gd_tribes"]
			elif predicate == "zones":
				failed = lists["gd_zones"] is not None and not lists["gd_zones"]
				zone_pending = bool(lists["gd_zones"])
			elif predicate == "npcs":
				failed = lists["gd_npcs"] is not None and self.npc_id not in lists["gd_npcs"]
			elif predicate == "npcGroups":
				failed = lists["gd_npc_groups"] is not None and self._enum("GroupDropType", npc.group_drop, "group_drop") not in lists["gd_npc_groups"]
			else:
				failed = rule.excluded is not None and self.npc_id in rule.excluded
			if failed:
				return predicate, False
		return None, zone_pending

	def effective_chance(self, rule: GdRule, modifiers: dict) -> tuple[float, dict]:
		"""calculateEffectiveChance + DropModifiers.calculateDropChance, in float arithmetic."""
		chance = rule.chance
		detail = {"baseChance": rule.chance, "dynamicChance": rule.dynamic, "rankModifier": None, "ratingModifier": None}
		what = f"rule {rule.name!r}: the effective chance"
		if rule.dynamic:
			rank, rating = self._rank_rating("a dynamic_chance rule")
			factor = f32(self.rules.rank_modifiers[rank] * self.rules.rating_modifiers[rating])
			detail.update(rankModifier=self.rules.rank_modifiers[rank], ratingModifier=self.rules.rating_modifiers[rating])
			chance = checked_f32(chance * factor, what)
		if rule.level_based and modifiers["reductionDropRate"] is not None:
			chance = f32(chance * modifiers["reductionDropRate"])
		return checked_f32(chance * modifiers["boostDropRate"], what), detail

	def _rank_rating(self, what: str) -> tuple[str, str]:
		rank = self._enum("NpcRank", self.npc.rank, "rank")
		rating = self._enum("NpcRating", self.npc.rating, "rating")
		if rank is None or rating is None:
			raise OracleError(f"npc {self.npc_id} has no {'rank' if rank is None else 'rating'} but {what} needs getRankModifier * getRatingModifier "
			                  "(Java: NullPointerException in the switch; registerDrop stops there)")
		return rank, rating

	def candidates(self, rule: GdRule) -> list[tuple[GdItem, ItemInfo]]:
		"""collectAllowedDrops' item filter: race PC_ALL or the killer's, min_diff <= npcLevel - itemLevel <= max_diff, in gd_items order."""
		result = []
		for item in rule.items:
			info = self.d.item(item.item_id, f"rule {rule.name!r}")
			if info.race == "PC_ALL" or info.race == self.killer.race:
				diff = self.npc.level - info.level
				if rule.min_diff <= diff <= rule.max_diff:
					result.append((item, info))
		return result

	def count_distribution(self, item: GdItem) -> tuple[list[dict] | None, str | None]:
		"""getItemCount: Rnd.get(min, max), uniform; for kinah `count *= level * Math.pow(rank * rating, 6)` (a long times a double, truncated).
		Math.pow is taken as exact when rank * rating raised to the exponent is a double, else as either neighbouring double: a count that
		depends on which is not modelled."""
		values = item.max_count - item.min_count + 1
		if item.item_id != self.rules.kinah_item:
			return None, None  # uniform over countRange
		rank, rating = self._rank_rating("the kinah count")
		factor = f32(self.rules.rank_modifiers[rank] * self.rules.rating_modifiers[rating])
		exact_power = Fraction(factor) ** self.rules.kinah_exponent
		candidates = {float(exact_power)} if Fraction(float(exact_power)) == exact_power else _double_neighbours(exact_power)
		counts: dict[int, int] = {}
		if values > MAX_COUNT_VALUES:
			return None, f"kinah count: {values} base values are more than the oracle enumerates"
		for base in range(item.min_count, item.max_count + 1):
			results = {to_long(float(base) * (float(self.npc.level) * power)) for power in candidates}
			if len(results) != 1:
				return None, f"kinah count of base {base}: Math.pow's last bit decides the truncation"
			result = results.pop()
			counts[result] = counts.get(result, 0) + 1
		return [{"count": c, "probability": fraction_out(Fraction(n, values))} for c, n in sorted(counts.items())], None

	def candidate_entry(self, item: GdItem, info: ItemInfo, pick: Fraction | None) -> dict:
		distribution, reason = self.count_distribution(item)
		if reason:
			self.not_modelled.append(reason)
		kinah = item.item_id == self.rules.kinah_item
		return {
			"itemId": item.item_id,
			"name": info.name,
			"level": info.level,
			"race": info.race,
			"quality": info.quality,
			"maxStackCount": info.max_stack_count,
			"weight": item.chance,
			"minCount": item.min_count,
			"maxCount": item.max_count,
			"kinah": kinah,
			"countValues": distribution,
			"countRange": [distribution[0]["count"], distribution[-1]["count"]] if distribution else [item.min_count, item.max_count],
			"pickProbability": fraction_out(pick) if pick is not None else None,
			"optionalSocket": -1 if info.option_slot_bonus != 0 else 0,
			"lootEffectId": self.rules.loot_effects.get(item.item_id, 0),
		}

	def evaluate_rules(self, modifiers: dict, allowed_default: bool) -> tuple[list[dict], list[dict], list[dict], dict]:
		applicable, without_candidates, zone_undecided = [], [], []
		stats = {"total": len(self.d.global_rules), "skippedNotAllowedDefault": 0, "blockedBy": {p: 0 for p in PREDICATES}, "zoneUndecided": 0}
		for rule in self.d.global_rules:
			npc_rule = rule.lists["gd_npcs"] is not None
			if not (allowed_default or npc_rule):
				stats["skippedNotAllowedDefault"] += 1
				continue
			chance, detail = self.effective_chance(rule, modifiers)  # Java computes it (and may throw) before the restrictions
			failure, zone_pending = self.rule_failure(rule)
			if failure is not None:
				stats["blockedBy"][failure] += 1
				continue
			candidates = self.candidates(rule)  # the item filter does not depend on the zone
			reference = {"ruleIndex": rule.index, "file": rule.file, "ordinalInFile": rule.ordinal, "ruleName": rule.name}
			fire = chance_probability(chance)
			picks = max(0, min(rule.max_drop_rule, len(candidates)))  # collectDrops' loop runs max_drop_rule times: none when it is <= 0
			if zone_pending:
				# addGlobalDrops rolls first and collectAllowedDrops checks the restrictions after: a rule that never fires, or that would add
				# nothing when its zone matches, adds no entry whatever isInsideZone answers
				if fire != 0 and picks != 0:
					raise OracleError(f"rule {rule.name!r} ({rule.file} #{rule.ordinal}) passes every predicate but its zones {rule.lists['gd_zones']}: "
					                  "npc.isInsideZone decides, which needs the npc's position and the zone shapes; the oracle does not model it")
				zone_undecided.append(dict(reference, zones=rule.lists["gd_zones"], effectiveChance=chance,
				                           addsNoEntry="it never fires" if fire == 0 else "no candidate" if not candidates else "max_drop_rule <= 0"))
				stats["zoneUndecided"] += 1
				continue
			if not candidates:
				without_candidates.append(reference)
				continue
			if rule.max_drop_rule <= 0:
				inclusion = [Fraction(0)] * len(candidates)
			elif len(candidates) > rule.max_drop_rule:
				weights = [item.chance for item, _ in candidates]
				if any(not w > 0 for w in weights):
					raise OracleError(f"rule {rule.name!r}: a candidate weight <= 0 in a Chance.selectElement draw is not modelled")
				inclusion = selection_inclusion(weights, rule.max_drop_rule)
				if inclusion is None:
					self.not_modelled.append(f"rule {rule.name!r}: pick probabilities of {rule.max_drop_rule} of {len(candidates)} candidates")
			else:
				inclusion = [Fraction(1)] * len(candidates)
			entry = dict(reference)
			entry.update(detail)
			entry.update({
				"npcSpecific": npc_rule,
				"npcsFromNames": self.npc_id in rule.npcs_from_names,
				"levelBasedChanceReduction": rule.level_based,
				"effectiveChance": chance,
				"fireProbability": fraction_out(fire),
				"certain": fire == 1,
				"never": fire == 0,
				"maxDropRule": rule.max_drop_rule,
				"memberLimit": rule.member_limit,
				"minDiff": rule.min_diff,
				"maxDiff": rule.max_diff,
				"restrictionRace": rule.restriction_race,
				"restrictions": sorted(k for k, v in rule.lists.items() if v is not None) + (["gd_excluded_npcs"] if rule.excluded is not None else []),
				"entriesIfFired": picks,
				"allCandidatesDrop": picks == len(candidates),
				"candidates": [self.candidate_entry(item, info, inclusion[i] if inclusion else None) for i, (item, info) in enumerate(candidates)],
				"_fire": fire,
				"_noEffect": self._rule_no_effect(rule, candidates, picks),
			})
			applicable.append(entry)
		return applicable, without_candidates, zone_undecided, stats

	def _rule_no_effect(self, rule: GdRule, candidates: list[tuple[GdItem, ItemInfo]], picks: int) -> Fraction | None:
		"""P(no candidate with a loot effect is among the rule's entries | it fires); None when the draw is beyond MAX_SELECTION_STATES."""
		effects = {i for i, (item, _) in enumerate(candidates) if self.rules.loot_effects.get(item.item_id, 0)}
		if not effects or picks == 0:
			return Fraction(1)
		if picks == len(candidates):
			return Fraction(0)
		avoid = selection_avoidance([item.chance for item, _ in candidates], rule.max_drop_rule, effects)
		if avoid is None:
			self.not_modelled.append(f"rule {rule.name!r}: the loot effect probability of {rule.max_drop_rule} of {len(candidates)} candidates")
		return avoid

	# -- custom drops
	def custom_groups(self, modifiers: dict) -> list[dict]:
		groups = self.d.custom_drops.get(self.npc_id)
		if groups is None:
			return []
		result = []
		for number, group in enumerate(groups):
			applies = group.race == "PC_ALL" or group.race == self.killer.race
			drops = []
			finals = []
			for drop in group.drops:
				info = self.d.item(drop.item_id, f"custom drop of npc {self.npc_id}")
				chance = drop.chance
				if group.level_based and modifiers["reductionDropRate"] is not None:
					chance = f32(chance * modifiers["reductionDropRate"])
				final = checked_f32(chance * modifiers["boostDropRate"], f"npc {self.npc_id} custom drop group {number}: the final chance")
				finals.append(final)
				drops.append({"itemId": drop.item_id, "name": info.name, "minAmount": drop.min_amount, "maxAmount": drop.max_amount,
				              "baseChance": drop.chance, "finalChance": final, "eachMember": drop.each_member,
				              "optionalSocket": -1 if info.option_slot_bonus != 0 else 0, "lootEffectId": self.rules.loot_effects.get(drop.item_id, 0)})
			entry = {"group": number, "name": group.name, "race": group.race, "applies": applies, "maxItems": group.max_items,
			         "levelBasedChanceReduction": group.level_based, "drops": drops}
			nothing = {"minEntries": 0, "maxEntries": 0, "entryDistribution": [{"count": 0, "probability": 1.0}], "_distribution": {0: Fraction(1)},
			           "_noEffect": Fraction(1)}
			if not drops:
				if applies:
					raise OracleError(f"npc {self.npc_id} custom drop group {number} has no <drop> (Java: NullPointerException in tryAddDropItems)")
				entry.update(nothing)
			elif not applies:
				entry.update(nothing)
			else:
				rolls = max(0, group.max_items)  # `for (int i = 0; i < maxItems && ...)`: no roll when max_items <= 0
				certain = sum(1 for f in finals if f >= 100.0)
				possible = sum(1 for f in finals if f > 0)
				distribution, inclusion = _custom_group_distribution(finals, rolls)
				entry["minEntries"] = min(rolls, certain)
				entry["maxEntries"] = min(rolls, possible)
				effects = [d["lootEffectId"] != 0 for d in drops]
				entry["_noEffect"] = _custom_group_avoidance(finals, effects, rolls) if any(effects) else Fraction(1)
				if entry["_noEffect"] is None:
					self.not_modelled.append(f"custom drop group {number}: the loot effect probability needs more states than the oracle enumerates")
				if distribution is None:
					self.not_modelled.append(f"custom drop group {number}: more chance-level states than the oracle enumerates")
					entry["entryDistribution"] = None
					entry["_distribution"] = None
				else:
					entry["entryDistribution"] = [{"count": c, "probability": fraction_out(p)} for c, p in sorted(distribution.items())]
					entry["_distribution"] = distribution
					for drop, p in zip(drops, inclusion):
						drop["pickProbability"] = fraction_out(p)
				self.not_modelled.append(f"custom drop group {number}: the float rounding of finalChance - chance (DropGroup.java:66) is taken as exact "
				                         "- two chance levels whose differences round to one float would tie")
			result.append(entry)
		return result

	# -- quest drops
	def quest_drops(self) -> list[dict]:
		result = []
		for drop in self.d.quests.get(self.npc_id, []):
			info = self.d.item(drop.item_id, f"quest {drop.quest_id} drop")
			conditions = [f"quest {drop.quest_id} is in state START"]
			if drop.collecting_step:
				conditions.append(f"its quest var 0 equals the collecting step {drop.collecting_step}")
			solo_never = None
			if drop.target == "ALLIANCE":
				solo_never = "the quest's target is ALLIANCE"
			elif drop.mentor_type == "MENTE":
				solo_never = "the quest's mentor_type is MENTE (needs a group with a mentor)"
			if drop.handler_side:
				conditions.append(f"the inventory holds fewer than {drop.needed_amount} of item {drop.item_id}")
			elif drop.collect_items is not None:
				needed = [count for item_id, count in drop.collect_items if item_id == drop.item_id]
				conditions.append(f"the inventory holds fewer than {needed[0]} of item {drop.item_id}" if needed else
				                  "never: the quest's collect_items do not name the item")
			result.append({"questId": drop.quest_id, "itemId": drop.item_id, "name": info.name, "chance": drop.chance,
			               "chanceProbability": fraction_out(chance_probability(float(drop.chance))), "collectingStep": drop.collecting_step,
			               "dropEachMember": drop.drop_each_member, "handlerSide": drop.handler_side, "neededAmount": drop.needed_amount,
			               "soloNever": solo_never, "conditions": conditions, "count": 1,
			               "optionalSocket": -1 if info.option_slot_bonus != 0 else 0, "lootEffectId": self.rules.loot_effects.get(drop.item_id, 0)})
		return result

	# -- the whole report
	def report(self) -> dict:
		ai = self.ai_behaviour()
		behaviour = ai["behaviours"][0]
		# NpcController.onDie: doReward only on REWARD_AP_XP_DP_LOOT; doReward: registerDrop only on REWARD_LOOT; the corpse on ALLOW_DECAY
		registers = behaviour["REWARD_AP_XP_DP_LOOT"] and behaviour["REWARD_LOOT"]
		by_ai = behaviour["registersDropItself"]
		npc = self.npc
		npc_entry = {"npcId": self.npc_id, "name": npc.name, "level": npc.level, "rank": self._enum("NpcRank", npc.rank, "rank"),
		             "rating": self._enum("NpcRating", npc.rating, "rating"), "race": self._enum("Race", npc.race, "race"),
		             "tribe": self._enum("TribeClass", npc.tribe, "tribe"), "groupDrop": self._enum("GroupDropType", npc.group_drop, "group_drop"),
		             "templateAi": npc.ai, "templateAiInXml": npc.xml_ai, "type": self._enum("NpcTemplateType", npc.npc_type, "type"),
		             "abyssType": self._enum("AbyssNpcType", npc.abyss_type, "abyss_type"), "spots": len(self.spots)}
		report = {
			"format": "aion-m5b3-drops",
			"version": 1,
			"npcId": self.npc_id,
			"map": self.map,
			"killer": {"race": self.killer.race, "level": self.killer.level, "levelMeaning": "the level when registerDrop runs: on a kill after the "
			           "kill's XP (NpcController.java:233 before :244), so after a level-up from this kill", "solo": True, "membership": 0,
			           "reposeEnergy": 0, "reposeEnergyAssumed": self.killer.level >= self.rules.repose_level, "salvationPercent": 0},
			"config": self.config(),
			"npc": npc_entry,
			"ai": ai,
			"registerDrop": registers,
			"registerDropByAi": by_ai,
			"dropTrigger": "kill" if not by_ai else "the AI's own registerDrop call",
			"allowDecay": behaviour["ALLOW_DECAY"],
		}
		if by_ai:
			self.not_modelled.append(f"{', '.join(by_ai)} call(s) registerDrop itself (ChestAI, QuestItemNpcAI and NightmareCrateAI on use, from "
			                         "handleUseItemFinish): when and whether that runs (key items, a quest dialog, QuestItemNpcAI only with quest "
			                         "drops) is not modelled; the report is the drop it registers for a solo user, whose level is the killer level")
		if not registers and not by_ai:
			report.update({"lootEnable": None, "modifiers": None, "globalDrops": None, "customDrop": [], "questDrops": [], "rules": [],
			               "rulesWithoutCandidates": [], "rulesZoneUndecided": [], "ruleStatistics": None, "kinah": None,
			               "entries": {"min": 0, "max": 0, "expected": 0.0, "deterministic": True, "pNoDrop": 1.0, "minCertainEffectiveChance": None,
			                           "distribution": [{"count": 0, "probability": 1.0}]},
			               "notModelled": [], "gateAssertions": [f"no SM_LOOT_STATUS(LOOT_ENABLE) and no loot: the AI answers REWARD_AP_XP_DP_LOOT="
			                                                     f"{behaviour['REWARD_AP_XP_DP_LOOT']} and REWARD_LOOT={behaviour['REWARD_LOOT']}"]})
			return report
		ai_name = behaviour["name"]
		modifiers = self.modifiers(ai_name)
		custom = self.custom_groups(modifiers)
		quests = self.quest_drops()
		quest_ai = ai_name == self.rules.quest_ai
		exclusion = self.global_exclusion()
		world_none = self.map["dropType"] == "NONE"
		default_exclusion = self.default_exclusion(modifiers["isChest"])
		evaluated = not quest_ai and exclusion is None and not world_none
		rules, without, zone_undecided, stats = self.evaluate_rules(modifiers, default_exclusion is None) if evaluated else ([], [], [], None)
		if quests:
			self.not_modelled.append("quest drops: whether one registers depends on the killer's quest state; `entries` and the indexes assume none "
			                         "does (a character with none of the listed quests in START)")
		entries = self.entries(custom, rules)
		kinah = [dict(ruleName=r["ruleName"], ruleIndex=r["ruleIndex"], fireProbability=r["fireProbability"], certain=r["certain"], **{
			k: c[k] for k in ("itemId", "countValues", "countRange")}) for r in rules for c in r["candidates"] if c["kinah"]]
		# the loot effect ids of every item that can be in the drop set (a rule that never fires and a group that does not apply add none)
		effects = sorted({0} | {c["lootEffectId"] for r in rules if r["_fire"] > 0 for c in r["candidates"]}
		                 | {d["lootEffectId"] for g in custom if g["maxEntries"] > 0 for d in g["drops"] if d["finalChance"] > 0}
		                 | {q["lootEffectId"] for q in quests})
		# P(the drop set holds an item with a loot effect): rules and groups draw independently; no quest drop, as `entries` assumes
		no_effect: Fraction | None = Fraction(1)
		for part in [1 - r["_fire"] * (1 - r["_noEffect"]) if r["_noEffect"] is not None else None for r in rules] + [g["_noEffect"] for g in custom]:
			no_effect = None if no_effect is None or part is None else no_effect * part
		report.update({
			"lootEnable": {"sent": True, "to": "the killer" if not by_ai else "the user", "lootEffectIds": effects,
			               "pLootEffect": None if no_effect is None else fraction_out(1 - no_effect)},
			"modifiers": modifiers,
			"globalDrops": {"questUseItemAi": quest_ai, "globalNpcExclusion": exclusion, "worldDropTypeNone": world_none,
			                "allowedDefaultGlobalDropNpc": default_exclusion is None, "defaultExclusion": default_exclusion, "evaluated": evaluated},
			"customDrop": [{k: v for k, v in g.items() if not k.startswith("_")} for g in custom],
			"questDrops": quests,
			"rules": [{k: v for k, v in r.items() if not k.startswith("_")} for r in rules],
			"rulesWithoutCandidates": without,
			"rulesZoneUndecided": zone_undecided,
			"ruleStatistics": stats,
			"kinah": kinah,
			"entries": entries,
			"notModelled": sorted(set(self.not_modelled)),
		})
		report["gateAssertions"] = gate_assertions(report)
		return report

	def entries(self, custom: list[dict], rules: list[dict]) -> dict:
		"""The number of loot entries: custom groups, then (for a character without the quests) no quest entry, then every applicable rule, which
		adds entriesIfFired entries with its fire probability, independently. Indexes follow registration order from 1."""
		distribution = {0: Fraction(1)}
		minimum = maximum = 0
		exact_index = True
		next_index = 1
		for group in custom:
			if group["_distribution"] is None:
				distribution = None
			elif distribution is not None:
				distribution = _convolve(distribution, group["_distribution"])
			minimum += group["minEntries"]
			maximum += group["maxEntries"]
			if group["minEntries"] != group["maxEntries"]:
				exact_index = False
			next_index += group["minEntries"]
		for rule in rules:
			fire = rule["_fire"]
			k = rule["entriesIfFired"]
			if distribution is not None:
				step: dict[int, Fraction] = {}
				for count, p in ((0, 1 - fire), (k, fire)):
					if p:
						step[count] = step.get(count, Fraction(0)) + p
				distribution = _convolve(distribution, step)
			minimum += k if fire == 1 else 0
			maximum += k if fire > 0 else 0
			if exact_index:
				rule["indexes"] = list(range(next_index, next_index + k)) if fire == 1 else [] if fire == 0 else None
				if 0 < fire < 1:
					exact_index = False
				else:
					next_index += k if fire == 1 else 0
			else:
				rule["indexes"] = None
		certain = [rule["effectiveChance"] for rule in rules if rule["_fire"] == 1]
		result = {"min": minimum, "max": maximum, "deterministic": minimum == maximum,
		          "minCertainEffectiveChance": min(certain) if certain else None}
		if distribution is None:
			result.update(expected=None, pNoDrop=None, distribution=None)
		else:
			cleaned = {c: p for c, p in distribution.items() if p != 0}
			result["expected"] = fraction_out(sum(c * p for c, p in cleaned.items()))
			result["pNoDrop"] = fraction_out(cleaned.get(0, Fraction(0)))
			result["distribution"] = [{"count": c, "probability": fraction_out(p)} for c, p in sorted(cleaned.items())]
		return result

	def config(self) -> dict:
		shipped = None
		rates_file = self.d.config_dir / "main" / "rates.properties"
		if rates_file.is_file():
			match = re.search(r"^\s*gameserver\.rates\.drop\s*=\s*(.*?)\s*$", _read(rates_file), re.MULTILINE)
			shipped = match.group(1) if match else None
		events_shipped = None
		events_file = self.d.config_dir / "main" / "events.properties"
		if events_file.is_file():
			match = re.search(r"^\s*gameserver\.event\.service\.disabled_events\s*=[ \t]*(.*?)\s*$", _read(events_file), re.MULTILINE)
			events_shipped = match.group(1) if match else None
		return {
			"dropRate": self.killer.drop_rate,
			"dropRatesDefault": self.rules.drop_rates_default,
			"dropRatesShipped": shipped,
			"membership": 0,
			"disabledEvents": "*",
			"disabledEventsShipped": events_shipped,
			"eventsWithDropRules": sorted(self.d.events),
			"assumptions": [
				"gameserver.rates.drop: the value at the killer's membership (Rates.get: membershipRates[min(length - 1, membership)]); "
				"default 1.0 (RatesConfig.DROP_RATES = \"" + self.rules.drop_rates_default + "\", membership 0)",
				"gameserver.event.service.disabled_events = * (every gate profile): no event drop rule and no event buff; the Java default (empty) "
				"activates the permanent 'Beyond Aion Server Buffs' event (a 0.1 % chance per PvE kill of a +10 % drop boost buff) and every "
				"dated event in its period - not modelled",
				"a solo killer: no group or alliance (initDropNpc allows only the killer, winnerObj 0, the member_limit arm is not taken)",
				"no BOOST_DROP_RATE / DR_BOOST stat function on the npc or the killer, no Salvation (only the //energybuff admin and the "
				"set_vitalpoint console commands set salvation points), no palace: boost = " + str(self.rules.base_boost),
				(f"Energy of Repose 0 at level {self.killer.level} (below {self.rules.repose_level} only a seeded database row or the //energybuff "
				 "and set_makeup_bonus commands give it; PlayerCommonData.updateMaxRepose clears it on a level change and on a later login)"
				 if self.killer.level < self.rules.repose_level else
				 f"Energy of Repose 0 ASSUMED at level {self.killer.level} (>= {self.rules.repose_level}: it grows while offline, "
				 "PlayerEnterWorldService.updateEnergyOfRepose; a character with repose energy gets boost + 5 in calculateBoostDropRate)"),
				"--player-level is the killer's level when registerDrop runs: NpcController.doReward adds the kill's XP first (NpcController.java:233 "
				"before :244), so a level-up from this kill counts",
				"the killer dealt the most damage (NpcController.doReward registers the drop for the winner only)",
				"ruleIndex (and so the rule each entry index belongs to) follows GlobalDropData's order: the global_rules files in the order Java "
				"lists them (XmlUtil.listFiles, Files.find), taken as NTFS order - another file system may order the files differently",
			],
		}


def _double_neighbours(value: Fraction) -> set[float]:
	"""The two doubles around a value that is not a double: what a Math.pow within 1 ulp may answer."""
	nearest = float(value)
	return {nearest, math.nextafter(nearest, math.inf) if Fraction(nearest) < value else math.nextafter(nearest, -math.inf)}


def _convolve(a: dict[int, Fraction], b: dict[int, Fraction]) -> dict[int, Fraction]:
	result: dict[int, Fraction] = {}
	for x, p in a.items():
		for y, q in b.items():
			result[x + y] = result.get(x + y, Fraction(0)) + p * q
	return result


def _custom_group_distribution(finals: list[float], max_items: int) -> tuple[dict[int, Fraction] | None, list[Fraction]]:
	"""
	DropGroup.tryAddDropItems over the drops' final chances: each of max_items rolls c = Rnd.chance() picks, among the remaining drops whose final
	chance is above c, one of those with the smallest final chance (all equal, so uniform), and removes it; a roll above every remaining final
	chance picks nothing. The state is the number of remaining drops per distinct final chance; returns (entry count distribution, per-drop
	inclusion probability), or (None, []) beyond MAX_CUSTOM_STATES.
	"""
	levels = sorted(set(finals))
	counts = tuple(sum(1 for f in finals if f == level) for level in levels)
	memo: dict[tuple, tuple[dict[int, Fraction], list[Fraction]]] = {}

	def solve(state: tuple[int, ...], left: int) -> tuple[dict[int, Fraction], list[Fraction]]:
		if left <= 0 or sum(state) == 0:
			return {0: Fraction(1)}, [Fraction(0)] * len(levels)
		key = state + (-left,)
		if key in memo:
			return memo[key]
		if len(memo) > MAX_CUSTOM_STATES:
			raise _TooManyStates()
		distribution: dict[int, Fraction] = {}
		picks = [Fraction(0)] * len(levels)
		below = 0.0
		covered = Fraction(0)
		for j, level in enumerate(levels):
			if state[j] == 0:
				continue
			p = chance_between(below, level) if level > 0 else Fraction(0)
			below = level
			if p == 0:
				continue
			covered += p
			next_state = state[:j] + (state[j] - 1,) + state[j + 1:]
			sub_distribution, sub_picks = solve(next_state, left - 1)
			for count, q in sub_distribution.items():
				distribution[count + 1] = distribution.get(count + 1, Fraction(0)) + p * q
			picks[j] += p
			for i in range(len(levels)):
				picks[i] += p * sub_picks[i]
		nothing = 1 - covered
		if nothing:
			sub_distribution, sub_picks = solve(state, left - 1)
			for count, q in sub_distribution.items():
				distribution[count] = distribution.get(count, Fraction(0)) + nothing * q
			for i in range(len(levels)):
				picks[i] += nothing * sub_picks[i]
		memo[key] = (distribution, picks)
		return memo[key]

	if max_items > MAX_CUSTOM_ROLLS:  # the recursion is one level deep per roll
		return None, []
	try:
		distribution, level_picks = solve(counts, max_items)
	except _TooManyStates:
		return None, []
	inclusion = [level_picks[levels.index(f)] / counts[levels.index(f)] for f in finals]
	return distribution, inclusion


def _custom_group_avoidance(finals: list[float], marked: list[bool], max_items: int) -> Fraction | None:
	"""The same draw: the probability that no drop with `marked` set is picked. A roll that lands on a chance level takes one of its remaining
	drops uniformly; the state is the number of unmarked drops left per level (a marked pick ends the walk). None beyond MAX_CUSTOM_STATES."""
	levels = sorted(set(finals))
	marked_per_level = tuple(sum(1 for f, m in zip(finals, marked) if f == level and m) for level in levels)
	plain = tuple(sum(1 for f, m in zip(finals, marked) if f == level and not m) for level in levels)
	memo: dict[tuple, Fraction] = {}

	def solve(state: tuple[int, ...], left: int) -> Fraction:
		if left <= 0:
			return Fraction(1)
		key = state + (-left,)
		if key in memo:
			return memo[key]
		if len(memo) > MAX_CUSTOM_STATES:
			raise _TooManyStates()
		result = Fraction(0)
		below = 0.0
		covered = Fraction(0)
		for j, level in enumerate(levels):
			remaining = state[j] + marked_per_level[j]
			if remaining == 0:
				continue
			p = chance_between(below, level) if level > 0 else Fraction(0)
			below = level
			if p == 0:
				continue
			covered += p
			if state[j]:
				result += p * Fraction(state[j], remaining) * solve(state[:j] + (state[j] - 1,) + state[j + 1:], left - 1)
		if covered != 1:
			result += (1 - covered) * solve(state, left - 1)
		memo[key] = result
		return result

	if max_items > MAX_CUSTOM_ROLLS:
		return None
	try:
		return solve(plain, max_items)
	except _TooManyStates:
		return None


def gate_assertions(report: dict) -> list[str]:
	"""What a gate can assert exactly for this npc, derived from the report."""
	again = ("DropService.scheduleFreeForAll (DropService.java:54-70) broadcasts it again at 240 s while the corpse stands; a corpse with drops "
	         "decays after 300 s, RespawnService.WITH_DROP_DECAY")
	if report["registerDropByAi"]:
		kill = (" - a kill by damage would register it too (the AI answers REWARD_AP_XP_DP_LOOT and REWARD_LOOT true)" if report["registerDrop"]
		        else "")
		lines = [f"registerDrop runs when the AI calls it ({', '.join(report['registerDropByAi'])}; the shipped AIs on use, from handleUseItemFinish)"
		         f"{kill}: then SM_LOOT_STATUS(npc, LOOT_ENABLE) to the user, once within 240 s ({again}); when it runs is not modelled"]
	else:
		lines = [f"SM_LOOT_STATUS(corpse, LOOT_ENABLE) to the killer exactly once within 240 s of the kill (registerDrop runs to its end, even with "
		         f"an empty drop set; {again})"]
	if not report["allowDecay"]:
		lines.append("the AI answers ALLOW_DECAY false: NpcController.onDie deletes the corpse at once, so nothing can be looted")
	effects = [e for e in report["lootEnable"]["lootEffectIds"] if e]
	p_effect = report["lootEnable"]["pLootEffect"]
	without_quest = " without a quest drop" if report["questDrops"] else ""
	if not effects:
		lines.append("its lootEffectId is 0")
	elif p_effect == 1:
		lines.append(f"its lootEffectId is not 0: {effects[0]}" if len(effects) == 1 else f"its lootEffectId is not 0: one of {effects} (any of the set)")
	else:
		lines.append(f"its lootEffectId is 0 unless an item with a loot effect ({effects}) is drawn, and one of those ids then (SM_LOOT_STATUS."
		             f"getLootEffect: any non-zero id of the set)" + (f"; P(not 0) = {p_effect}{without_quest}" if p_effect is not None else ""))
	entries = report["entries"]
	if entries["deterministic"] and entries["min"] == 0:
		lines.append("the drop set is empty: no entry (RespawnService.scheduleDecayTask keeps IMMEDIATE_DECAY)")
	elif entries["deterministic"]:
		lines.append(f"SM_LOOT_ITEMLIST lists exactly {entries['min']} entries with the indexes 1..{entries['min']} (in any order: HashSet, plan D10)")
		edge = entries.get("minCertainEffectiveChance")
		if edge is not None and edge < 101.0:
			lines.append(f"caution: the smallest certain effective chance is {edge}, within 1 % of 100f - certain only while the port's float "
			             "arithmetic lands on or above 100f; a gate rate with margin avoids depending on it")
	else:
		lines.append(f"the entry count is random: {entries['min']}..{entries['max']} (P(no drop) = {entries['pNoDrop']}); raise the drop rate until "
		             "every applicable rule is certain to make it exact")
	for rule in report["rules"]:
		ids = [c["itemId"] for c in rule["candidates"]]
		where = f"indexes {rule['indexes']}" if rule["indexes"] else "its entries"
		if rule["never"]:
			continue
		picks = rule["entriesIfFired"]
		if rule["allCandidatesDrop"]:
			what = f"exactly the items {ids} in gd_items order" if len(ids) > 1 else f"exactly item {ids[0]}"
		elif picks == 1:
			what = f"1 entry drawn from the {len(ids)} candidates {ids}"
		else:
			what = (f"{picks} entries drawn without replacement from the {len(ids)} candidate positions {ids}"
			        + (" (an id listed twice can drop twice)" if len(set(ids)) != len(ids) else ""))
		counts = ", ".join(f"{c['itemId']}: {c['countRange'][0]}..{c['countRange'][1]}" for c in rule["candidates"]
		                   if c["countRange"][0] != 1 or c["countRange"][1] != 1)
		certain = "" if rule["certain"] else f" (fires with probability {rule['fireProbability']})"
		lines.append(f"rule {rule['ruleIndex']} '{rule['ruleName']}'{certain}: {where} hold {what}" + (f"; counts {counts}" if counts else "; count 1"))
	files = sorted({r["file"] for r in report["rules"] if r["indexes"]})
	if len(files) > 1:
		lines.append(f"which rule each index belongs to assumes Java lists the global_rules files {files} in NTFS order (Files.find); a gate that "
		             "does not depend on it asserts the entry count, the indexes and a one-to-one match of entries to rules by candidate membership")
	for kinah in report["kinah"] or []:
		values = [v["count"] for v in kinah["countValues"]] if kinah["countValues"] else None
		lines.append(f"kinah (rule {kinah['ruleIndex']}): count in {values}" if values else f"kinah: count in {kinah['countRange']}")
	sockets = sorted({c["itemId"] for r in report["rules"] for c in r["candidates"] if c["optionalSocket"]})
	lines.append("SM_LOOT_ITEMLIST optionalSocket is 0 for every candidate" if not sockets else f"optionalSocket is -1 for {sockets}, else 0")
	lines.append("showLootConfirmation is 0 for every entry (a solo looter)")
	if report["questDrops"]:
		lines.append(f"no quest item unless the killer has one of the quests {sorted({q['questId'] for q in report['questDrops']})} in START")
	return lines


# ---------------------------------------------------------------------------------------------------------------------------------------------
# Entry points

def resolve_map(drop_data: DropData, npc_id: int, map_id: int | None) -> int:
	if map_id is not None:
		return map_id
	maps = drop_data.spawn_maps(npc_id)
	if len(maps) != 1:
		raise OracleError(f"npc {npc_id} has regular spawns on {len(maps)} maps {maps}: pass --map")
	return maps[0]


def _killer(drop_data: DropData, map_info: dict, race: str | None, level: int, drop_rate: str | None) -> Killer:
	if race is None:
		if map_info["worldType"] not in RACE_OF_WORLD_TYPE:
			raise OracleError(f"map {map_info['id']} has world_type {map_info['worldType']}: pass --race")
		race = RACE_OF_WORLD_TYPE[map_info["worldType"]]
	if race not in RACES:
		raise OracleError(f"race must be one of {RACES}")
	if not 1 <= level <= drop_data.max_player_level:
		raise OracleError(f"player level {level} is outside 1..{drop_data.max_player_level} (player_experience_table)")
	rates = java_float_rates(drop_rate if drop_rate is not None else drop_data.rules.drop_rates_default, "--drop-rate")
	if len(rates) != 1 and drop_rate is not None:
		raise OracleError("--drop-rate takes the one value of gameserver.rates.drop that applies to the killer's membership")
	rate = rates[0]  # membership 0
	if not 0 <= rate < float("inf"):
		raise OracleError(f"drop rate {rate} is not a finite non-negative float")
	return Killer(race, level, rate)


def _check_handler(drop_data: DropData, map_id: int) -> None:
	handler = drop_data.instance_handler(map_id)
	if handler is not None:
		raise OracleError(f"map {map_id} has the instance handler {handler}, whose onDropRegistered may change the drop; the oracle does not model it")


def drops_report(drop_data: DropData, npc_id: int, map_id: int | None = None, player_level: int = 1, race: str | None = None,
                 drop_rate: str | None = None) -> dict:
	map_id = resolve_map(drop_data, npc_id, map_id)
	map_info = drop_data.map_info(map_id)
	_check_handler(drop_data, map_id)
	killer = _killer(drop_data, map_info, race, player_level, drop_rate)
	return DropEvaluator(drop_data, npc_id, map_id, killer).report()


def map_survey(drop_data: DropData, map_id: int, player_level: int = 1, race: str | None = None, drop_rate: str | None = None) -> dict:
	"""One row per npc id with a regular spot on the map that may be spawned; an npc the oracle refuses is listed with the reason."""
	return map_survey_with_items(drop_data, map_id, player_level, race, drop_rate)[0]


def map_survey_with_items(drop_data: DropData, map_id: int, player_level: int = 1, race: str | None = None,
                          drop_rate: str | None = None) -> tuple[dict, set[int]]:
	"""map_survey and the ids its `distinctDroppableItems` counts (rule and custom drop candidates, quest drop items)."""
	map_info = drop_data.map_info(map_id)
	_check_handler(drop_data, map_id)
	killer = _killer(drop_data, map_info, race, player_level, drop_rate)
	rows = drop_data.spawn_rows(map_id)
	npc_ids = sorted({r["npcId"] for r in rows if r["spawned"] is not False and not r["flags"]["gatherable"]})
	summaries, refused = [], []
	items: set[int] = set()
	for npc_id in npc_ids:
		try:
			report = DropEvaluator(drop_data, npc_id, map_id, killer, rows).report()
		except OracleError as e:
			refused.append({"npcId": npc_id, "reason": str(e)})
			continue
		for rule in report["rules"]:
			items |= {c["itemId"] for c in rule["candidates"]}
		for group in report["customDrop"]:
			items |= {d["itemId"] for d in group["drops"] if group["applies"]}
		template = drop_data.npcs[npc_id]
		summaries.append({
			"npcId": npc_id, "name": template.name, "level": template.level, "race": template.race, "rating": template.rating,
			"rank": template.rank, "groupDrop": template.group_drop, "aiNames": report["ai"]["aiNames"], "registerDrop": report["registerDrop"],
			"registerDropByAi": report["registerDropByAi"],
			"globalNpcExclusion": (report["globalDrops"] or {}).get("globalNpcExclusion"),
			"rules": [f"{r['ruleIndex']}:{r['ruleName']}" for r in report["rules"]],
			"kinah": [k["countRange"] for k in report["kinah"] or []],
			"customDrop": bool(report["customDrop"]),
			"questDropItems": sorted({q["itemId"] for q in report["questDrops"]}),
			"entriesMin": report["entries"]["min"], "entriesMax": report["entries"]["max"], "pNoDrop": report["entries"]["pNoDrop"],
		})
	droppable = items | {i for s in summaries for i in s["questDropItems"]}
	return {
		"format": "aion-m5b3-drop-survey",
		"version": 1,
		"map": map_info,
		"killer": {"race": killer.race, "level": killer.level, "dropRate": killer.drop_rate},
		"npcIds": len(npc_ids),
		"withApplicableRule": sum(1 for s in summaries if s["rules"]),
		"withKinah": sum(1 for s in summaries if s["kinah"]),
		"withCustomDrop": sum(1 for s in summaries if s["customDrop"]),
		"questDropItems": len({i for s in summaries for i in s["questDropItems"]}),
		# rule and custom drop candidates, and with the quest drop items (the m5b3-plan.md §2.4 count)
		"distinctRuleItems": len(items),
		"distinctDroppableItems": len(droppable),
		"npcs": summaries,
		"refused": refused,
	}, droppable
