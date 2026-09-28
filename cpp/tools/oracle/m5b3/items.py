"""m5b3-item, and the cube-slot budget of m5b3-drops (m5b3-plan.md G-01, §2.4-§2.6, §10.1-§10.3, risk 5).

Java rules, each with the method the value comes from (paths below game-server/src/com/aionemu/gameserver unless noted):
- the item template (model/templates/item/ItemTemplate.java): the JAXB field defaults (max_stack_count 1 at :48, so `isStackable()` is
  `maxStackCount > 1`, :431-433), the mask bits of model/items/ItemMask.java (LIMIT_ONE, TRADEABLE, ..., read from the source), the `<actions>`
  children bound by ItemActions.java's @XmlElements (tag -> action class, read from the source), `<uselimits>` (ItemUseLimits: usedelay is the
  cooldown in ms Player.startCooldown/hasCooldown use under the usedelayid group), `<godstone>` (GodstoneInfo: skillid, skilllvl, probability,
  probabilityleft, breakprob, nonbreakcount), `<inventory id>` (getExtraInventoryId, :417-422: -1 without the element; an item with an id
  above 0 goes to the special cube, Storage.isFull(int), not into the cube's slots);
- a skill an item reaches (a `<skilluse skillid level>` action, a godstone's skillid/skilllvl): SkillData.afterUnmarshal
  (dataholders/SkillData.java:33-39) puts every template into skillTemplateById in merged document order, so the LAST <skill_template> of an id
  wins; its effects are bound by skillengine/effect/Effects.java (the m5b2 oracle's JavaSkillRules), and EffectTemplate.calculateBaseValue is
  `value + delta * effect.getSkillLevel()` (skillengine/effect/EffectTemplate.java:265) - `valueAtLevel` below;
- the cube-slot budget of a corpse (DropService.requestDropItem, services/drop/DropService.java:330-341: kinah and a solo looter's item both go
  through ItemService.addItem(player, itemId, count), i.e. allowInventoryOverflow false and the ITEM_COLLECT predicate, ItemService.java:37-39):
  addItem (:70-97) adds kinah to the storage's kinah item (no slot); addStackableItem (:143-173) fills every existing stack of the id up to
  max_stack_count (Item.increaseItemCount, model/gameobjects/Item.java:315-326) and then makes new stacks of at most max_stack_count each
  (ItemFactory.calculateCount, services/item/ItemFactory.java:33-38) while the cube is not full; addNonStackableItem (:102-116) makes one item per
  unit of the count. The POWER_SHARDS arm first fills equipped shards of the same id: the budget assumes none is equipped (the gate never equips
  one). A LIMIT_ONE item already in the cube or the regular warehouse is refused before any of this (DropService.java:304-310) and stays in the
  corpse. The cube's size is StorageType.CUBE's limit plus 9 slots per npc, quest and item expansion (model/items/storage/StorageType.java:7,
  Player.java:429); the budget assumes none.

What the budget is: per corpse, the number of cube slots looting every entry can take in the worst case - the sum over the entries that can be
registered of the most new stacks the entry's candidates can make at their maximum count, kinah and special-cube items excluded. It is an upper
bound: two entries that pick the same new stackable item share a stack. `deterministicMerges` are the entries that take no slot whatever is
picked (every candidate merges into the room of existing stacks at its maximum count), e.g. the Minor Power Shard of "Power Shards" once a shard
stack exists (m5b3-plan.md §2.4 "Two picks the gate can rely on").

The oracle refuses (OracleError, exit 2) what it does not model: a Java source whose modelled statements changed (REQUIRED_STATEMENTS), an item
id without a template, a skill id without a template, an effect tag Effects.java does not bind, an action tag ItemActions.java does not bind.
"""

from __future__ import annotations

import math
import re
from dataclasses import dataclass
from pathlib import Path

from staticdata_oracle import OracleError

from m5a.creation import enum_constants
from m5a.data import StaticData, java_int
from m5b2.skills import JavaSkillRules, SkillTemplateInfo

KINAH_ITEM_ID_PATTERN = r"public static final int KINAH\s*=\s*(\d+)\s*;"

# Statements the budget and the item report model, checked on the whitespace-normalized, comment-free sources: a changed one is a refusal.
REQUIRED_STATEMENTS = {
	"services/item/ItemService.java": [
		"public static long addItem(Player player, int itemId, long count) { return addItem(player, itemId, count, null, false, "
		"DEFAULT_UPDATE_PREDICATE); }",
		"if (itemTemplate.isKinah()) { inventory.increaseKinah(count); return 0; }",
		"if (itemTemplate.isStackable()) count = addStackableItem(player, itemTemplate, count, allowInventoryOverflow, predicate); else count = "
		"addNonStackableItem(player, itemTemplate, count, sourceItem, allowInventoryOverflow, predicate);",
		"while ((allowInventoryOverflow || !inventory.isFull(itemTemplate.getExtraInventoryId())) && count > 0) { Item newItem = "
		"ItemFactory.newItem(itemTemplate.getTemplateId());",
		"if (itemTemplate.getItemGroup() == ItemGroup.POWER_SHARDS) { Equipment equipment = player.getEquipment(); items = "
		"equipment.getEquippedItemsByItemId(itemTemplate.getTemplateId());",
		"items = inventory.getItemsByItemId(itemTemplate.getTemplateId()); for (Item item : items) { if (count == 0) { break; } count = "
		"inventory.increaseItemCount(item, count, predicate.getUpdateType(item, true)); }",
		"while (count > 0 && (allowInventoryOverflow || !inventory.isFull(itemTemplate.getExtraInventoryId()))) { Item newItem = "
		"ItemFactory.newItem(itemTemplate.getTemplateId(), count); count -= newItem.getItemCount(); inventory.add(newItem, predicate.getAddType()); }",
	],
	"services/item/ItemFactory.java": [
		"long maxStackCount = itemTemplate.getMaxStackCount(); if (count > maxStackCount && !itemTemplate.isKinah()) count = maxStackCount; "
		"return count;",
	],
	"model/gameobjects/Item.java": [
		"long cap = itemTemplate.getMaxStackCount(); long addCount = this.itemCount + count > cap ? cap - this.itemCount : count;",
	],
	"model/templates/item/ItemTemplate.java": [
		"private int maxStackCount = 1;",
		"public boolean isStackable() { return this.maxStackCount > 1; }",
		"if (extraInventory == null) { return -1; } return extraInventory.getId();",
	],
	"model/items/storage/Storage.java": [
		"public boolean isFull(int inventory) { if (inventory > 0) { return isFullSpecialCube(); } return isFull(); }",
	],
	"services/drop/DropService.java": [
		"if (template.hasLimitOne()) { if (player.getInventory().getFirstItemByItemId(itemId) != null || "
		"player.getStorage(StorageType.REGULAR_WAREHOUSE.getId()).getFirstItemByItemId(itemId) != null) {",
		"if (itemId == ItemId.KINAH) { var team = player.getCurrentTeam(); if (team == null) { requestedItem.setCount(ItemService.addItem(player, "
		"itemId, requestedItem.getCount()));",
		"} else if (!player.isInTeam() && !requestedItem.isItemWonNotCollected() && dropNpc.getDistributionId() == 0) { "
		"requestedItem.setCount(ItemService.addItem(player, itemId, requestedItem.getCount()));",
	],
	"model/gameobjects/player/Player.java": [
		"getInventory().setLimit(StorageType.CUBE.getLimit() + (getNpcExpands() + getQuestExpands() + getItemExpands()) * "
		"getInventory().getRowLength());",
	],
	"dataholders/SkillData.java": [
		"for (SkillTemplate skillTemplate : skillTemplates) { int skillId = skillTemplate.getSkillId(); skillTemplateById.put(skillId, skillTemplate);",
	],
	"skillengine/effect/EffectTemplate.java": [
		"return value + delta * effect.getSkillLevel();",
	],
}


def _read(source: Path) -> str:
	try:
		return source.read_text(encoding="utf-8")
	except OSError as e:
		raise OracleError(f"{source}: {e}") from e


def _strip_comments(text: str) -> str:
	return re.sub(r"//[^\n]*", "", re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL))


def _normalized(text: str) -> str:
	return re.sub(r"\s+", " ", _strip_comments(text))


@dataclass(frozen=True)
class JavaItemRules:
	"""The literals and tables the item report and the cube budget need, read from the Java sources."""

	mask_flags: tuple[tuple[str, int], ...]   # ItemMask.java: name -> bit, in declaration order
	action_classes: dict[str, str]           # ItemActions.java @XmlElements: XML tag -> action class
	cube_limit: int                          # StorageType.CUBE's limit
	cube_row_length: int                     # StorageType.CUBE's row length (slots per expansion)
	kinah_item_id: int                       # ItemId.KINAH

	@staticmethod
	def read(java_src: Path) -> "JavaItemRules":
		base = Path(java_src) / "com" / "aionemu" / "gameserver"
		for relative, statements in REQUIRED_STATEMENTS.items():
			text = _normalized(_read(base / relative))
			for statement in statements:
				if statement not in text:
					raise OracleError(f"{base / relative}: the statement `{statement}` is gone - the Java code this oracle models changed")
		mask_text = _strip_comments(_read(base / "model" / "items" / "ItemMask.java"))
		flags = []
		for name, value in re.findall(r"public static final int (\w+)\s*=\s*([^;]+);", mask_text):
			shift = re.fullmatch(r"\(?\s*1\s*<<\s*(\d+)\s*\)?", value.strip())
			if shift:
				flags.append((name, 1 << int(shift.group(1))))
			elif re.fullmatch(r"\d+", value.strip()):
				flags.append((name, int(value.strip())))
			else:
				raise OracleError(f"ItemMask.{name} = {value}: not a literal or a shift this oracle reads")
		if not flags:
			raise OracleError(f"{base / 'model/items/ItemMask.java'}: no mask constant found")
		actions_text = _strip_comments(_read(base / "model" / "templates" / "item" / "actions" / "ItemActions.java"))
		block = re.search(r"@XmlElements\(\s*\{(.*?)\}\s*\)", actions_text, re.DOTALL)
		if not block:
			raise OracleError("ItemActions.java: no @XmlElements block")
		action_classes: dict[str, str] = {}
		for tag, cls in re.findall(r'@XmlElement\(\s*name\s*=\s*"([^"]+)"\s*,\s*type\s*=\s*(\w+)\.class\s*\)', block.group(1)):
			if tag in action_classes:
				raise OracleError(f"ItemActions.java binds <{tag}> twice")
			action_classes[tag] = cls
		storage = {name: args for name, args in enum_constants(base / "model" / "items" / "storage" / "StorageType.java", "StorageType")}
		cube_args = [a.strip() for a in (storage.get("CUBE") or "").split(",")]
		if len(cube_args) < 3 or not all(re.fullmatch(r"\d+", a) for a in cube_args):
			raise OracleError(f"StorageType.CUBE({storage.get('CUBE')}): not (id, limit, rowLength[, specialLimit])")
		kinah = re.search(KINAH_ITEM_ID_PATTERN, _read(base / "model" / "items" / "ItemId.java"))
		if not kinah:
			raise OracleError("ItemId.KINAH not found")
		return JavaItemRules(tuple(flags), action_classes, int(cube_args[1]), int(cube_args[2]), int(kinah.group(1)))

	def flag_names(self, mask: int) -> list[str]:
		return [name for name, bit in self.mask_flags if mask & bit]


@dataclass
class ItemTemplateInfo:
	"""One <item_template> with what the report and the budget read."""

	item_id: int
	attrs: dict[str, str]
	actions: list[tuple[str, dict[str, str]]]
	use_limits: dict[str, str] | None
	godstone: dict[str, str] | None
	extra_inventory_id: int
	line: int | None = None

	@property
	def name(self) -> str | None:
		return self.attrs.get("name")

	@property
	def max_stack_count(self) -> int:
		return java_int(self.attrs.get("max_stack_count"), f"item {self.item_id} max_stack_count", 1)

	@property
	def mask(self) -> int:
		return java_int(self.attrs.get("mask"), f"item {self.item_id} mask", 0)

	@property
	def stackable(self) -> bool:
		return self.max_stack_count > 1  # ItemTemplate.isStackable


def load_item_templates(data: StaticData, wanted: set[int]) -> dict[int, ItemTemplateInfo]:
	"""The <item_template>s of the wanted ids (ItemData: a later template of the same id replaces the earlier one)."""
	result: dict[int, ItemTemplateInfo] = {}
	for element in data.stream("item_templates", "item_template"):
		item_id = java_int(element.get("id"), "item_template id")
		if item_id not in wanted:
			continue
		actions_element = element.find("actions")
		actions = [(a.tag, dict(a.attrib)) for a in actions_element] if actions_element is not None else []
		limits = element.find("uselimits")
		godstone = element.find("godstone")
		inventory = element.find("inventory")
		extra = java_int(inventory.get("id"), f"item {item_id} inventory id", 0) if inventory is not None else -1
		result[item_id] = ItemTemplateInfo(item_id, dict(element.attrib), actions, dict(limits.attrib) if limits is not None else None,
		                                   dict(godstone.attrib) if godstone is not None else None, extra)
	missing = sorted(wanted - result.keys())
	if missing:
		raise OracleError(f"no item_template for item id(s) {missing}")
	return result


# ---------------------------------------------------------------------------------------------------------------------------------------------
# The cube-slot budget of a corpse

def new_slots(item: ItemTemplateInfo, count: int, stacks: list[int], kinah_id: int) -> int:
	"""The cube slots ItemService.addItem(player, itemId, count) takes for `count` of `item` when the cube holds `stacks` of it (their counts)."""
	if item.item_id == kinah_id:
		return 0  # ItemService.addItem: increaseKinah
	if item.extra_inventory_id > 0:
		return 0  # the special cube (Storage.isFull(int)), not a cube slot
	if not item.stackable:
		return count  # addNonStackableItem: one item per unit
	cap = item.max_stack_count
	room = sum(max(0, cap - c) for c in stacks)  # addStackableItem fills every existing stack (Item.increaseItemCount)
	left = max(0, count - room)
	return math.ceil(left / cap)  # new stacks of at most max_stack_count (ItemFactory.calculateCount)


def cube_budget(report: dict, data: StaticData, rules: JavaItemRules, inventory: list[tuple[int, int]], expansions: int = 0) -> dict:
	"""The per-corpse slot budget of a drops report (drops_report) for a cube that holds `inventory`, one (itemId, count) pair per stack."""
	stacks: dict[int, list[int]] = {}
	for item_id, count in inventory:
		if count <= 0:
			raise OracleError(f"--inventory {item_id}:{count}: a stack holds at least one item")
		stacks.setdefault(item_id, []).append(count)
	candidates = {c["itemId"] for r in report.get("rules", []) for c in r["candidates"]}
	candidates |= {d["itemId"] for g in report.get("customDrop", []) for d in g["drops"]}
	templates = load_item_templates(data, candidates | set(stacks))
	for item_id, counts in stacks.items():
		cap = templates[item_id].max_stack_count
		if item_id == rules.kinah_item_id:
			if len(counts) != 1:
				raise OracleError(f"--inventory {item_id}: the cube has one kinah item (Storage's kinah field), not {len(counts)}")
		elif any(c > cap for c in counts):  # ItemFactory.calculateCount caps every stack but the kinah one
			raise OracleError(f"--inventory {item_id}: a stack of more than its max_stack_count {cap}")
	entries = []
	worst = best = kinah_entries = 0
	merges = []
	special = []

	def candidate_row(item_id: int, low: int, high: int) -> dict:
		"""one candidate entry of `low`..`high` of the item: the new slots at either end and how it merges into the cube"""
		template = templates[item_id]
		present = stacks.get(item_id, [])
		worst_slots = new_slots(template, high, present, rules.kinah_item_id)
		best_slots = new_slots(template, low, present, rules.kinah_item_id)
		limit_one = bool(template.mask & dict(rules.mask_flags).get("LIMIT_ONE", 0))
		if item_id == rules.kinah_item_id:
			merge = "kinah"
		elif template.extra_inventory_id > 0:
			merge = "specialCube"
			special.append(item_id)
		elif limit_one and present:
			merge, worst_slots, best_slots = "refusedLimitOne", 0, 0
		elif template.stackable and present and worst_slots == 0:
			merge = "certain"
		elif template.stackable and present:
			merge = "partial"
		else:
			merge = "never"
		return {"itemId": item_id, "name": template.name, "stackable": template.stackable, "maxStackCount": template.max_stack_count,
		        "existingStacks": present, "countRange": [low, high], "limitOne": limit_one, "extraInventoryId": template.extra_inventory_id,
		        "merge": merge, "newSlotsWorst": worst_slots, "newSlotsBest": best_slots}

	def merge_kind(rows: list[dict]) -> tuple[bool, bool]:
		"""(every candidate is kinah, every candidate takes no slot whatever is picked)"""
		is_kinah = all(r["merge"] == "kinah" for r in rows)
		return is_kinah, not is_kinah and all(r["merge"] in ("certain", "refusedLimitOne") for r in rows)

	for rule in report.get("rules", []):
		if rule["never"]:
			continue
		picks = rule["entriesIfFired"]
		rows = [candidate_row(c["itemId"], *c["countRange"]) for c in rule["candidates"]]
		ranked = sorted(r["newSlotsWorst"] for r in rows)
		rule_worst = sum(ranked[-picks:]) if picks else 0
		rule_best = sum(sorted(r["newSlotsBest"] for r in rows)[:picks]) if rule["certain"] else 0
		is_kinah, deterministic = merge_kind(rows)
		if is_kinah:
			kinah_entries += picks
		if deterministic:
			merges.append({"ruleIndex": rule["ruleIndex"], "ruleName": rule["ruleName"], "indexes": rule["indexes"],
			               "itemIds": sorted({r["itemId"] for r in rows})})
		worst += rule_worst
		best += rule_best
		entries.append({"ruleIndex": rule["ruleIndex"], "ruleName": rule["ruleName"], "indexes": rule["indexes"], "entriesIfFired": picks,
		                "certain": rule["certain"], "kinah": is_kinah, "deterministicMerge": deterministic, "newSlotsWorst": rule_worst,
		                "newSlotsBest": rule_best, "candidates": rows})
	for group in report.get("customDrop", []):
		if not group.get("applies") or group.get("maxEntries", 0) <= 0:
			continue
		# DropGroup.tryAddDropItems (model/drop/DropGroup.java:57-80): a roll picks a drop only when `chance < finalChance` (so never one at
		# finalChance 0) and removes it from remainingDrops, so the group adds minEntries..maxEntries DISTINCT drops of the ones it can pick;
		# addDropItem's calculateCount gives each Rnd.get(minAmount, maxAmount) (DropItem.java:38-40; Drop.afterUnmarshal made a max_amount of 0
		# the min_amount, which the drops report already applied)
		rows = [candidate_row(d["itemId"], d["minAmount"], d["maxAmount"]) for d in group["drops"] if d["finalChance"] > 0]
		group_worst = sum(sorted(r["newSlotsWorst"] for r in rows)[-group["maxEntries"]:])
		group_best = sum(sorted(r["newSlotsBest"] for r in rows)[:group["minEntries"]])
		is_kinah, deterministic = merge_kind(rows)
		if is_kinah:
			kinah_entries += group["maxEntries"]
		if deterministic:
			merges.append({"customGroup": group["group"], "customGroupName": group.get("name"), "itemIds": sorted({r["itemId"] for r in rows})})
		worst += group_worst
		best += group_best
		entries.append({"customGroup": group["group"], "customGroupName": group.get("name"), "minEntries": group["minEntries"],
		                "maxEntries": group["maxEntries"], "kinah": is_kinah, "deterministicMerge": deterministic, "newSlotsWorst": group_worst,
		                "newSlotsBest": group_best, "candidates": rows})
	limit = rules.cube_limit + expansions * rules.cube_row_length
	used = sum(len(v) for k, v in stacks.items() if k != rules.kinah_item_id and templates[k].extra_inventory_id <= 0)
	return {
		"limit": limit,
		"limitRule": f"StorageType.CUBE limit {rules.cube_limit} + {expansions} expansion(s) x row length {rules.cube_row_length} (Player.java:429)",
		"stacks": [{"itemId": k, "counts": v} for k, v in sorted(stacks.items())],
		"slotsUsed": used,
		"slotsFree": limit - used,
		"kinahEntries": kinah_entries,
		"deterministicMerges": merges,
		"worstCaseNewSlots": worst,
		"bestCaseNewSlots": best,
		"fitsWorstCase": worst <= limit - used,
		"specialCubeItems": sorted(set(special)),
		"entries": entries,
		"assumptions": [
			"every entry is looted by the killer alone into the cube (DropService.requestDropItem's solo arm: ItemService.addItem(player, itemId, "
			"count), allowInventoryOverflow false)",
			"no power shard is equipped (addStackableItem's POWER_SHARDS arm fills equipped shards of the same id first)",
			"worstCaseNewSlots is an upper bound: two entries that pick the same new stackable item share its first stack",
			"the cube holds exactly the --inventory stacks (kinah takes no slot) and has no npc, quest or item expansion unless --cube-expansions",
			"no quest drop registers (as the drops report's `entries` assume)",
		],
	}


# ---------------------------------------------------------------------------------------------------------------------------------------------
# m5b3-item

def load_skill_templates_counted(data: StaticData, wanted: set[int]) -> tuple[dict[int, SkillTemplateInfo], dict[int, int]]:
	"""The LAST <skill_template> of each wanted id (SkillData.afterUnmarshal, SkillData.java:33-39) and how many templates carry the id."""
	templates: dict[int, SkillTemplateInfo] = {}
	counts: dict[int, int] = {}
	for element in data.stream("skill_data", "skill_template"):
		skill_id = java_int(element.get("skill_id"), "skill_template skill_id")
		if skill_id in wanted:
			templates[skill_id] = SkillTemplateInfo.of(element)
			counts[skill_id] = counts.get(skill_id, 0) + 1
	missing = sorted(wanted - templates.keys())
	if missing:
		raise OracleError(f"no skill_template for skill id(s) {missing}")
	return templates, counts


def skill_entry(template: SkillTemplateInfo, count: int, level: int, skill_rules: JavaSkillRules) -> dict:
	effects = []
	for position, (tag, attrs, changes, others) in enumerate(template.effects, 1):
		cls = skill_rules.effect_classes.get(tag)
		if cls is None:
			raise OracleError(f"skill {template.skill_id}: effect <{tag}> is not bound by Effects.java")
		value = java_int(attrs.get("value"), f"skill {template.skill_id} <{tag}> value", 0)
		delta = java_int(attrs.get("delta"), f"skill {template.skill_id} <{tag}> delta", 0)
		effects.append({"position": position, "tag": tag, "class": cls, "classChain": skill_rules.class_chain(cls), "attributes": attrs,
		                "valueAtLevel": value + delta * level, "changes": changes, "otherChildren": others})
	return {
		"skillId": template.skill_id,
		"level": level,
		"templateCount": count,
		"kept": "the last template of the id (SkillData.afterUnmarshal: skillTemplateById.put, SkillData.java:33-39)" if count > 1 else "the only template",
		"name": template.attrs.get("name"),
		"attributes": template.attrs,
		"properties": template.properties,
		"effects": effects,
		"effectClasses": sorted({e["class"] for e in effects}),
		"effectClassChain": sorted({c for e in effects for c in e["classChain"]}),
	}


def item_report(data: StaticData, java_src: Path, item_ids: list[int]) -> dict:
	rules = JavaItemRules.read(java_src)
	skill_rules = JavaSkillRules.read(java_src)
	templates = load_item_templates(data, set(item_ids))
	wanted_skills: set[int] = set()
	for template in templates.values():
		for tag, attrs in template.actions:
			if tag not in rules.action_classes:
				raise OracleError(f"item {template.item_id}: action <{tag}> is not bound by ItemActions.java")
			if tag == "skilluse":
				wanted_skills.add(java_int(attrs.get("skillid"), f"item {template.item_id} skilluse skillid"))
		if template.godstone is not None:
			wanted_skills.add(java_int(template.godstone.get("skillid"), f"item {template.item_id} godstone skillid"))
	skills, counts = load_skill_templates_counted(data, wanted_skills) if wanted_skills else ({}, {})
	items = []
	for item_id in item_ids:
		template = templates[item_id]
		actions = []
		for tag, attrs in template.actions:
			entry = {"tag": tag, "class": rules.action_classes[tag], "attributes": attrs}
			if tag == "skilluse":
				skill_id = java_int(attrs.get("skillid"), f"item {item_id} skilluse skillid")
				level = java_int(attrs.get("level"), f"item {item_id} skilluse level", 0)
				entry["skill"] = skill_entry(skills[skill_id], counts[skill_id], level, skill_rules)
			actions.append(entry)
		godstone = None
		if template.godstone is not None:
			skill_id = java_int(template.godstone.get("skillid"), f"item {item_id} godstone skillid")
			level = java_int(template.godstone.get("skilllvl"), f"item {item_id} godstone skilllvl", 0)
			godstone = {"attributes": template.godstone, "skill": skill_entry(skills[skill_id], counts[skill_id], level, skill_rules)}
		mask = template.mask
		limits = template.use_limits
		items.append({
			"itemId": item_id,
			"name": template.name,
			"attributes": template.attrs,
			"maxStackCount": template.max_stack_count,
			"stackable": template.stackable,
			"mask": mask,
			"maskFlags": rules.flag_names(mask),
			"extraInventoryId": template.extra_inventory_id,
			"expireTimeMinutes": java_int(template.attrs.get("expire_time"), f"item {item_id} expire_time", 0),
			"useLimits": limits,
			"cooldown": None if limits is None else {
				"useDelayMillis": java_int(limits.get("usedelay"), f"item {item_id} usedelay", 0),
				"useDelayId": java_int(limits.get("usedelayid"), f"item {item_id} usedelayid", 0)},
			"actions": actions,
			"godstone": godstone,
		})
	return {
		"format": "aion-m5b3-item",
		"version": 1,
		"items": items,
		"maskBits": {name: bit for name, bit in rules.mask_flags},
	}
