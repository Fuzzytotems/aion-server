"""m5b2-skills: the skills a character casts in the M5b-2 gate and every template constant the gate asserts exactly (m5b2-plan.md G-01, D8).

Java rules, each with the method the value comes from:
- the skill set: SkillLearnService.learnNewSkills(player, 1, level) of a character created with `class` (m5a/creation.py learn_new_skills,
  which m5a-creation shares). Only a starting class can be created (CM_CREATE_CHARACTER.java:92) and a character that is not a daeva cannot pass
  level 9 (PlayerCommonData.java:276-281: the level is capped at maxLevel - 1 with maxLevel 10), so the levels are 1..9;
- the template: SkillTemplate.java:34-113 with its JAXB field defaults (skilltype NONE, skill_category NONE, dispel_category NONE,
  chain_skill_prob 100, tslot null, every int 0, every boolean false);
- castDuration, the short SM_CASTSPELL.java:75 writes: Skill.updateCastDurationAndSpeed (Skill.java:338-348). For a Player effector that is
  calculateCastDuration (:372-381): the base `duration` unless the skill is cast (Skill.initializeSkillMethod: not PASSIVE, not PROVOKED, no
  item) with apply_casting_time_bonus, and then calculateMagicalCastDuration (:383-399), which with no casting time stat function on the
  character reduces to the base as well (every getPositiveReverseStat answers its base, buffDelta is 0 and the 25 % cap is below the base).
  The oracle does NOT model a casting time stat function: when the character's equipped starting gear carries a BOOST_CASTING_TIME* modifier,
  when an equipped item belongs to an item set, or when one of its passive skills changes a BOOST_CASTING_TIME* stat or is a
  BoostSkillCastingTimeEffect, castDuration and castSpeed are null and `notModelled` says why. A CHARGE skill (calculateChargeCastDuration) is
  null as well. For an npc it is Math.round(baseCastDuration * (NpcTemplate.cast_speed / 1000f)) (Skill.java:339-341), cast_speed default 1000;
- castSpeed, the float SM_CASTSPELL.java:77 writes: 1 - getStat(BOOST_CASTING_TIME, 1000).getBonus() / 1000f (Skill.java:347), i.e. 1.0 with no
  casting time function; allowAnimationBoost, the byte of :78: SkillTemplate.isApplyCastingTimeBonus (Skill.java:1049-1051);
- cooldown, the int SM_CASTSPELL_RESULT.java:81 writes, in units of 100 ms: Skill.getCooldown (Skill.java:330-336), the template's cooldown plus
  cooldown_delta_lv * skillLevel when the cooldown is not 0 (Player.getSkillCooldown answers the template value outside the
  NO_SKILL_COOLDOWN_MODE state). cooldownMillis is what SM_SKILL_COOLDOWN.java:62 writes as the duration: the TEMPLATE cooldown * 100, without
  the level delta;
- costs, per condition section: MpCondition.getCost / HpCondition.getCost (MpCondition.java:48-58, HpCondition.java:49-54): value + delta *
  skillLevel, null when `ratio` makes it a percentage of the current max MP/HP; the MP cost also assumes Skill.getBoostSkillCost() == 0, which a
  BoostSkillCostEffect among the passives would break (then null, `notModelled`). DpCondition only checks (DpCondition.java:22-24). The end
  section is what payCastCosts pays (m5b2-plan.md X4); an <mp> in the START section is paid by Conditions.validate at CAST_START as well;
- the effects: every child of <effects> in document order, the class Effects.java's @XmlElements binds its tag to, and that class's `extends`
  chain up to EffectTemplate (read from skillengine/effect/*.java). A tag Effects.java does not bind is dropped by JAXB and listed under
  `ignoredEffectElements`. The EffectTemplate attributes with their JAXB defaults (EffectTemplate.java:37-95);
- effectDuration: Effect.calculateTemplateDuration (Effect.java:899-910) when every template succeeds: the first template in successEffects -
  a ConcurrentHashMap keyed by position, so ascending positions while they are below its 16 bins - whose duration2 + duration1 * skillLevel
  is positive, minus Rnd.get(0, randomtime), which the oracle reports as effectDurationRandomTime instead of rolling. 0 means no timed effect.
  Neither the PvP percentage nor the cumulative resist of calculateEffectsDuration (:884-897) applies to an npc target or to the effector
  itself, which are the gate's cases;
- targetSlot: the template's tslot as SkillTargetSlot's name, ordinal (what SM_ABNORMAL_STATE.java:36 and SM_ABNORMAL_EFFECT.java:54 write
  per effect) and id (the slot mask EffectController.broadCastEffects passes, SM_ABNORMAL_EFFECT.java:45);
- the soul sickness: PlayerController.updateSoulSickness (PlayerController.java:712-734) casts skill 8291 at skill level deathCount, which it
  first raises by one while it is below 10; the skill id and the cap are read from the Java source;
- npc skills: NpcSkillData.afterUnmarshal (the first <npc_skills> list naming an npc id wins, NpcSkillData.java:28-36) and the NpcSkillTemplate
  attributes with their JAXB defaults (NpcSkillTemplate.java:12-47).
"""

from __future__ import annotations

import re
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path

from staticdata_oracle import OracleError

from m5a.creation import RACES, JavaEnums, creation_report, enum_constants, learn_new_skills
from m5a.data import StaticData, java_boolean, java_int
from m5a.javafloat import f32, round_to_int

CASTING_TIME_STAT_PREFIX = "BOOST_CASTING_TIME"
CASTING_TIME_EFFECT = "BoostSkillCastingTimeEffect"
SKILL_COST_EFFECT = "BoostSkillCostEffect"
EFFECT_TEMPLATE = "EffectTemplate"
CONDITION_SECTIONS = (("start", "startconditions"), ("use", "useconditions"), ("end", "endconditions"))
MAX_LEVEL_WITHOUT_DAEVA = 9


def _read(source: Path) -> str:
	try:
		return source.read_text(encoding="utf-8")
	except OSError as e:
		raise OracleError(f"{source}: {e}") from e


def _strip_comments(text: str) -> str:
	return re.sub(r"//[^\n]*", "", re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL))


@dataclass(frozen=True)
class JavaSkillRules:
	"""The tables the report needs from the Java sources."""

	effect_classes: dict[str, str]         # Effects.java @XmlElements: XML tag -> effect class
	superclasses: dict[str, str]           # effect class -> the class it extends (skillengine/effect/*.java)
	target_slots: dict[str, tuple[int, int]]  # SkillTargetSlot name -> (ordinal, id)
	soul_sickness_skill: int               # PlayerController.updateSoulSickness: `if (skillId == 0) skillId = N;`
	soul_sickness_max_death_count: int     # ... `if (deathCount < N) deathCount++;`
	cast_duration_cap: float               # Skill.calculateMagicalCastDuration: `Math.round(baseCastDuration * Nf)`

	@staticmethod
	def read(java_src: Path) -> "JavaSkillRules":
		base = Path(java_src) / "com" / "aionemu" / "gameserver"
		skill_java = _strip_comments(_read(base / "skillengine" / "model" / "Skill.java"))
		cap_match = re.search(r"private int calculateMagicalCastDuration\(\)\s*\{\s*int baseDurationCap\s*=\s*Math\.round\(baseCastDuration\s*\*\s*"
		                      r"([0-9.]+)f\);", skill_java)
		if not cap_match:
			raise OracleError("Skill.calculateMagicalCastDuration does not have the shape this oracle was written against")
		effect_dir = base / "skillengine" / "effect"
		effects_java = _strip_comments(_read(effect_dir / "Effects.java"))
		block = re.search(r"@XmlElements\(\s*\{(.*?)\}\s*\)", effects_java, re.DOTALL)
		if not block:
			raise OracleError(f"{effect_dir / 'Effects.java'}: no @XmlElements block")
		effect_classes: dict[str, str] = {}
		for tag, cls in re.findall(r'@XmlElement\(\s*name\s*=\s*"([^"]+)"\s*,\s*type\s*=\s*(\w+)\.class\s*\)', block.group(1)):
			if tag in effect_classes:
				raise OracleError(f"Effects.java binds <{tag}> twice")
			effect_classes[tag] = cls

		superclasses: dict[str, str] = {}
		for source in sorted(effect_dir.glob("*.java")):
			match = re.search(r"\bclass\s+(\w+)(?:<[^>]*>)?\s+extends\s+(\w+)", _strip_comments(_read(source)))
			if match and match.group(1) == source.stem:
				superclasses[match.group(1)] = match.group(2)

		slots: dict[str, tuple[int, int]] = {}
		for ordinal, (name, args) in enumerate(enum_constants(base / "skillengine" / "model" / "SkillTargetSlot.java", "SkillTargetSlot")):
			slots[name] = (ordinal, java_int((args or "").strip(), f"SkillTargetSlot.{name} id"))

		controller = _strip_comments(_read(base / "controllers" / "PlayerController.java"))
		body = re.search(r"public void updateSoulSickness\(int skillId\)\s*\{(.*?)\n\t\}", controller, re.DOTALL)
		if not body:
			raise OracleError("PlayerController.java: updateSoulSickness(int) not found")
		skill = re.search(r"if\s*\(skillId\s*==\s*0\)\s*skillId\s*=\s*(\d+);", body.group(1))
		cap = re.search(r"if\s*\(deathCount\s*<\s*(\d+)\)\s*\{?\s*deathCount\+\+;", body.group(1))
		if not skill or not cap:
			raise OracleError("PlayerController.updateSoulSickness does not have the shape this oracle was written against")
		return JavaSkillRules(effect_classes, superclasses, slots, int(skill.group(1)), int(cap.group(1)), f32(float(cap_match.group(1))))

	def class_chain(self, cls: str) -> list[str]:
		"""The class and its superclasses up to and including EffectTemplate."""
		chain = [cls]
		while chain[-1] != EFFECT_TEMPLATE:
			parent = self.superclasses.get(chain[-1])
			if parent is None:
				raise OracleError(f"effect class {chain[-1]} has no `extends` the oracle can follow to EffectTemplate")
			if parent in chain:
				raise OracleError(f"effect class {cls}: cyclic extends chain {chain + [parent]}")
			chain.append(parent)
		return chain


@dataclass
class SkillTemplateInfo:
	"""The attributes and child elements of one <skill_template>, read while the streamed element is still alive."""

	skill_id: int
	attrs: dict[str, str]
	properties: dict[str, str] | None
	conditions: dict[str, list[tuple[str, dict[str, str]]]]
	effects: list[tuple[str, dict[str, str], list[dict[str, str]], list[str]]]  # tag, attributes, <change> attributes, other child tags
	actions: list[tuple[str, dict[str, str]]] = field(default_factory=list)

	@staticmethod
	def of(element: ET.Element) -> "SkillTemplateInfo":
		skill_id = java_int(element.get("skill_id"), "skill_template skill_id")
		properties = element.find("properties")
		conditions = {}
		for key, tag in CONDITION_SECTIONS:
			section = element.find(tag)
			conditions[key] = [(c.tag, dict(c.attrib)) for c in section] if section is not None else []
		effects = []
		container = element.find("effects")
		if container is not None:
			for child in container:
				changes = [dict(c.attrib) for c in child.findall("change")]
				others = [c.tag for c in child if c.tag != "change"]
				effects.append((child.tag, dict(child.attrib), changes, others))
		actions_element = element.find("actions")
		actions = [(a.tag, dict(a.attrib)) for a in actions_element] if actions_element is not None else []
		return SkillTemplateInfo(skill_id, dict(element.attrib), dict(properties.attrib) if properties is not None else None, conditions, effects,
		                         actions)

	def attr_int(self, name: str, default: int = 0) -> int:
		return java_int(self.attrs.get(name), f"skill {self.skill_id} {name}", default)

	def attr_bool(self, name: str) -> bool:
		return java_boolean(self.attrs.get(name))


def load_skill_templates(data: StaticData, wanted: set[int]) -> dict[int, SkillTemplateInfo]:
	"""The <skill_template>s of the wanted ids (SkillData: a later template of the same id replaces the earlier one)."""
	templates: dict[int, SkillTemplateInfo] = {}
	for element in data.stream("skill_data", "skill_template"):
		if java_int(element.get("skill_id"), "skill_template skill_id") in wanted:
			info = SkillTemplateInfo.of(element)
			templates[info.skill_id] = info
	missing = sorted(wanted - templates.keys())
	if missing:
		raise OracleError(f"no skill_template for skill id(s) {missing}")
	return templates


def _condition_cost(tag: str, attrs: dict[str, str], level: int, skill_id: int) -> dict:
	"""MpCondition.getCost / HpCondition.getCost without the boost term: value + delta * skillLevel, null for a ratio (a percentage)."""
	value = java_int(attrs.get("value"), f"skill {skill_id} <{tag}> value")
	if tag == "dp":
		return {"value": value}  # DpCondition.validate only compares the DP (DpCondition.java:22-24)
	delta = java_int(attrs.get("delta"), f"skill {skill_id} <{tag}> delta", 0)
	ratio = java_boolean(attrs.get("ratio"))
	return {"value": value, "delta": delta, "ratio": ratio, "cost": None if ratio else value + delta * level}


def effect_duration(effects: list[dict], level: int) -> tuple[int, int] | None:
	"""
	Effect.calculateTemplateDuration for an effect whose every template succeeded: (duration, randomtime) of the first template in
	successEffects order with a positive duration2 + duration1 * skillLevel, (0, 0) when there is none. successEffects is a ConcurrentHashMap
	keyed by position that a later template of the same position overwrites; the ascending order only holds below its 16 initial bins, so a
	position outside 0..15 answers None.
	"""
	by_position: dict[int, dict] = {}
	for effect in effects:
		by_position[effect["position"]] = effect
	if any(p < 0 or p > 15 for p in by_position):
		return None
	for position in sorted(by_position):
		effect = by_position[position]
		duration = effect["duration2"] + effect["duration1"] * level
		if duration > 0:
			return duration, effect["randomTime"]
	return 0, 0


class SkillReporter:
	"""Builds the per skill entries; `casting_time_sources` and `cost_boost_sources` are the unmodelled stat sources of the character."""

	def __init__(self, rules: JavaSkillRules, templates: dict[int, SkillTemplateInfo], casting_time_sources: list[str],
	             cost_boost_sources: list[str]):
		self.rules = rules
		self.templates = templates
		self.casting_time_sources = casting_time_sources
		self.cost_boost_sources = cost_boost_sources

	def target_slot(self, name: str | None) -> dict | None:
		if name is None:
			return None
		if name not in self.rules.target_slots:
			raise OracleError(f"tslot {name} is not a SkillTargetSlot constant")
		ordinal, slot_id = self.rules.target_slots[name]
		return {"name": name, "ordinal": ordinal, "id": slot_id}

	def effects_of(self, info: SkillTemplateInfo) -> tuple[list[dict], list[str]]:
		effects, ignored = [], []
		for tag, attrs, changes, others in info.effects:
			cls = self.rules.effect_classes.get(tag)
			if cls is None:
				ignored.append(tag)  # JAXB drops an element @XmlElements does not name
				continue
			what = f"skill {info.skill_id} <{tag}>"
			pre_effects = [java_int(p, f"{what} preeffect") for p in attrs.get("preeffect", "").split()]
			effects.append({
				"tag": tag,
				"class": cls,
				"classChain": self.rules.class_chain(cls),
				"position": java_int(attrs.get("e"), f"{what} e", 0),
				"duration1": java_int(attrs.get("duration1"), f"{what} duration1", 0),
				"duration2": java_int(attrs.get("duration2"), f"{what} duration2", 0),
				"randomTime": java_int(attrs.get("randomtime"), f"{what} randomtime", 0),
				"preEffects": pre_effects,
				"noResist": java_boolean(attrs.get("noresist")),
				"effectId": java_int(attrs.get("effectid"), f"{what} effectid", 0),
				"value": java_int(attrs.get("value"), f"{what} value", 0),
				"delta": java_int(attrs.get("delta"), f"{what} delta", 0),
				"skillElement": attrs.get("element", "NONE"),
				"changes": changes,
				"childTags": others,
				"attributes": attrs,
			})
		return effects, ignored

	def player_cast_duration(self, info: SkillTemplateInfo, method: str) -> tuple[int | None, str | None]:
		"""Skill.updateCastDurationAndSpeed's player arms (Skill.java:342-347) and calculateCastDuration (:372-381) for a skill without an item
		and outside a multicast (getMultiCastCount() is 0 on a first cast)."""
		base = info.attr_int("duration")
		if info.attrs.get("activation") == "CHARGE":
			return None, "a CHARGE skill takes Skill.calculateChargeCastDuration, which the oracle does not model"
		if not (method == "CAST" and info.attr_bool("apply_casting_time_bonus")):
			return base, None  # isCastDurationAffectedByCastSpeed (:1054-1056) is false: baseCastDuration
		if self.casting_time_sources:
			return None, "casting time stat functions are not modelled: " + ", ".join(self.casting_time_sources)
		return magical_cast_duration_without_functions(base, info.attrs.get("skillsubtype"), self.rules.cast_duration_cap), None

	def entry(self, skill_id: int, level: int, sources: list[str]) -> dict:
		info = self.templates[skill_id]
		activation = info.attrs.get("activation")
		if activation is None:
			raise OracleError(f"skill {skill_id} has no activation (a required attribute)")
		method = "PASSIVE" if activation == "PASSIVE" else "PROVOKED" if activation == "PROVOKED" else "CAST"  # initializeSkillMethod, no item
		not_modelled: list[str] = []

		cast_duration, reason = self.player_cast_duration(info, method)
		if reason:
			not_modelled.append(f"castDuration: {reason}")
		cast_speed = None if self.casting_time_sources or activation == "CHARGE" else 1.0

		template_cooldown = info.attr_int("cooldown")
		cooldown = template_cooldown
		if cooldown != 0 and info.attr_int("cooldown_delta_lv") != 0:
			cooldown = cooldown + info.attr_int("cooldown_delta_lv") * level

		costs: dict[str, dict] = {}
		for key, _ in CONDITION_SECTIONS:
			section = {}
			for tag, attrs in info.conditions[key]:
				if tag in ("mp", "hp", "dp"):
					section[tag] = _condition_cost(tag, attrs, level, skill_id)
			costs[key] = section
		mp = costs["end"].get("mp")
		mp_cost = mp["cost"] if mp else 0
		if mp and mp["ratio"]:
			not_modelled.append("mpCost: <mp ratio=true> is a percentage of the current max MP")
		if mp and self.cost_boost_sources:
			mp_cost = None
			not_modelled.append("mpCost: Skill.getBoostSkillCost is not modelled: " + ", ".join(self.cost_boost_sources))

		effects, ignored = self.effects_of(info)
		duration = effect_duration(effects, level)
		if duration is None:
			not_modelled.append("effectDuration: an effect position outside 0..15 leaves the ConcurrentHashMap order of successEffects")

		chain = [attrs.get("category") for tag, attrs in info.conditions["start"] if tag == "chain"]
		return {
			"skillId": skill_id,
			"level": level,
			"sources": sorted(set(sources)),
			"name": info.attrs.get("name"),
			"lvl": info.attr_int("lvl"),
			"stack": info.attrs.get("stack"),
			"group": info.attrs.get("group"),
			"skillType": info.attrs.get("skilltype", "NONE"),
			"subType": info.attrs.get("skillsubtype"),
			"category": info.attrs.get("skill_category", "NONE"),
			"activation": activation,
			"method": method,
			"targetSlot": self.target_slot(info.attrs.get("tslot")),
			"targetSlotLevel": info.attr_int("tslot_level"),
			"dispelCategory": info.attrs.get("dispel_category", "NONE"),
			"reqDispelLevel": info.attr_int("req_dispel_level"),
			"baseCastDuration": info.attr_int("duration"),
			"castDuration": cast_duration,
			"castSpeed": cast_speed,
			"allowAnimationBoost": info.attr_bool("apply_casting_time_bonus"),
			"cooldownId": info.attr_int("cooldownId"),
			"cooldown": cooldown,
			"cooldownDeltaLv": info.attr_int("cooldown_delta_lv"),
			"cooldownMillis": template_cooldown * 100,
			"chainCategory": chain[0] if chain else None,
			"chainSkillProb": info.attr_int("chain_skill_prob", 100),
			"cancelRate": info.attr_int("cancel_rate"),
			"pvpDuration": info.attr_int("pvp_duration"),
			"mpCost": mp_cost,
			"costs": costs,
			"properties": info.properties,
			"conditions": {key: [{"tag": tag, "attributes": attrs} for tag, attrs in info.conditions[key]] for key, _ in CONDITION_SECTIONS},
			"actions": [{"tag": tag, "attributes": attrs} for tag, attrs in info.actions],
			"effects": effects,
			"ignoredEffectElements": ignored,
			"effectDuration": duration[0] if duration else None,
			"effectDurationRandomTime": duration[1] if duration else None,
			"notModelled": not_modelled,
		}


def magical_cast_duration_without_functions(base: int, sub_type: str | None, cap_factor: float) -> int:
	"""
	Skill.calculateMagicalCastDuration (Skill.java:383-399) for an effector without a stat function on any BOOST_CASTING_TIME* stat: then
	CreatureGameStats.getPositiveReverseStat(stat, value) is max(0, value), because a ReverseStat without functions answers its base
	(Stat2.getExactCurrent with bonus 0, baseRate 1, fixedBonusRate 0, finalRate 1). getSkillCastBoostStat (:401-410) names a stat for the
	SUMMON, SUMMONHOMING, SUMMONTRAP, HEAL and ATTACK sub types, and isSummonType (:412-414) exempts the three summon types from the final cap.
	"""
	positive_reverse = lambda value: max(0, value)  # noqa: E731
	base_duration_cap = round_to_int(f32(base * cap_factor))
	cast_duration = max(positive_reverse(base), base_duration_cap)
	boost_value = positive_reverse(base)
	if sub_type in ("SUMMON", "SUMMONHOMING", "SUMMONTRAP", "HEAL", "ATTACK"):
		boost_value = positive_reverse(boost_value)
	buff_delta = base - boost_value
	cast_duration -= buff_delta
	if sub_type not in ("SUMMON", "SUMMONHOMING", "SUMMONTRAP"):
		cast_duration = max(cast_duration, base_duration_cap)
	return max(cast_duration, 0)


def npc_cast_duration(base: int, cast_speed: int) -> int:
	"""Skill.updateCastDurationAndSpeed, the Npc arm: Math.round(baseCastDuration * (npc.getGameStats().getCastSpeed() / 1000f))."""
	return round_to_int(f32(base * f32(cast_speed / f32(1000.0))))


def _npc_template(data: StaticData, npc_id: int) -> dict:
	for element in data.stream("npc_templates", "npc_template"):
		if java_int(element.get("npc_id"), "npc_template npc_id") == npc_id:
			return {"level": java_int(element.get("level"), f"npc_template {npc_id} level", 0),
			        "castSpeed": java_int(element.get("cast_speed"), f"npc_template {npc_id} cast_speed", 1000),
			        "name": element.get("name")}
	raise OracleError(f"no npc_template with npc_id {npc_id}")


def npc_skill_lists(data: StaticData, npc_ids: set[int]) -> dict[int, list[dict]]:
	"""NpcSkillData.afterUnmarshal: npc id -> the <npc_skill> rows of the FIRST <npc_skills> list whose npc_ids names it."""
	lists: dict[int, list[dict]] = {}
	for element in data.children("npc_skill_templates", "npc_skills"):
		ids = [java_int(i, "npc_skills npc_ids") for i in (element.get("npc_ids") or "").split()]
		for npc_id in ids:
			if npc_id not in npc_ids or npc_id in lists:
				continue
			rows = []
			for skill in element.findall("npc_skill"):
				what = f"npc {npc_id} npc_skill"
				rows.append({
					"skillId": java_int(skill.get("id"), f"{what} id", 0),
					"level": java_int(skill.get("lv"), f"{what} lv", 0),
					"prob": java_int(skill.get("prob"), f"{what} prob", 0),
					"minHp": java_int(skill.get("min_hp"), f"{what} min_hp", 0),
					"maxHp": java_int(skill.get("max_hp"), f"{what} max_hp", 100),
					"cd": java_int(skill.get("cd"), f"{what} cd", 0),
					"prio": java_int(skill.get("prio"), f"{what} prio", 0),
					"nextSkillTime": java_int(skill.get("next_skill_time"), f"{what} next_skill_time", -1),
					"isPostSpawn": java_boolean(skill.get("is_post_spawn")),
					"target": skill.get("target", "MOST_HATED"),
				})
			lists[npc_id] = rows
	return lists


def _casting_and_cost_sources(data: StaticData, rules: JavaSkillRules, templates: dict[int, SkillTemplateInfo], equipped: list[int],
                              passives: list[int]) -> tuple[list[str], list[str]]:
	"""The stat sources of a fresh character that the cast duration and the MP cost formulas would have to model."""
	casting, cost = [], []
	wanted = set(equipped)
	for element in data.stream("item_templates", "item_template"):
		item_id = java_int(element.get("id"), "item_template id")
		if item_id not in wanted:
			continue
		modifiers = element.find("modifiers")
		for modifier in modifiers if modifiers is not None else []:
			if (modifier.get("name") or "").startswith(CASTING_TIME_STAT_PREFIX):
				casting.append(f"item {item_id} modifier {modifier.get('name')}")
	for itemset in data.children("item_sets", "itemset"):
		for part in itemset.findall("itempart"):
			if java_int(part.get("itemid"), "itempart itemid", 0) in wanted:
				casting.append(f"item {part.get('itemid')} is part of item set {itemset.get('id')}")
	for skill_id in passives:
		for tag, _, changes, _ in templates[skill_id].effects:
			cls = rules.effect_classes.get(tag)
			if cls == CASTING_TIME_EFFECT:
				casting.append(f"passive skill {skill_id} <{tag}>")
			if cls == SKILL_COST_EFFECT:
				cost.append(f"passive skill {skill_id} <{tag}>")
			for change in changes:
				if (change.get("stat") or "").startswith(CASTING_TIME_STAT_PREFIX):
					casting.append(f"passive skill {skill_id} changes {change.get('stat')}")
	return casting, cost


def _parse_skill_argument(text: str) -> tuple[int, int | None]:
	skill, _, level = text.partition(":")
	return java_int(skill, "--skill id"), java_int(level, "--skill level") if level else None


def skills_report(data: StaticData, java_src: Path, race: str, player_class: str, level: int = 1, extra_skills: list[str] = (),
                  npc_ids: list[int] = (), death_count: int = 1) -> dict:
	if race not in RACES:
		raise OracleError(f"race must be one of {RACES}")
	java_src = Path(java_src)
	enums = JavaEnums(java_src)
	if player_class not in enums.classes:
		raise OracleError(f"unknown player class {player_class}")
	if not enums.classes[player_class][0]:
		raise OracleError(f"{player_class} is not a starting class: CM_CREATE_CHARACTER.java:92 refuses to create it")
	if not 1 <= level <= MAX_LEVEL_WITHOUT_DAEVA:
		raise OracleError(f"level {level}: a character that is not a daeva is capped at level {MAX_LEVEL_WITHOUT_DAEVA} "
		                  "(PlayerCommonData.java:276-281), and the daeva state is quest state, not static data")
	rules = JavaSkillRules.read(java_src)
	if not 1 <= death_count <= rules.soul_sickness_max_death_count:
		raise OracleError(f"death count {death_count}: updateSoulSickness raises it to at most {rules.soul_sickness_max_death_count}")

	creation = creation_report(data, java_src, race, player_class)
	learned = learn_new_skills(data, enums, race, player_class, 1, level)

	extras: list[tuple[int, int | None]] = [_parse_skill_argument(text) for text in extra_skills]
	npc_lists = npc_skill_lists(data, set(npc_ids))
	wanted = set(learned) | {s for s, _ in extras} | {rules.soul_sickness_skill}
	for rows in npc_lists.values():
		wanted |= {row["skillId"] for row in rows}
	templates = load_skill_templates(data, wanted)

	passives = sorted(s for s in learned if templates[s].attrs.get("activation") == "PASSIVE")
	equipped = [item["itemId"] for item in creation["items"] if item["equipped"]]
	casting_sources, cost_sources = _casting_and_cost_sources(data, rules, templates, equipped, passives)
	reporter = SkillReporter(rules, templates, casting_sources, cost_sources)

	requested: dict[tuple[int, int], list[str]] = {}
	for skill_id, skill_level in learned.items():
		requested.setdefault((skill_id, skill_level), []).append("autolearn")
	for skill_id, skill_level in extras:
		requested.setdefault((skill_id, skill_level if skill_level is not None else templates[skill_id].attr_int("lvl")), []).append("extra")
	requested.setdefault((rules.soul_sickness_skill, death_count), []).append("soulSickness")
	npcs = []
	for npc_id in npc_ids:
		template = _npc_template(data, npc_id)
		rows = npc_lists.get(npc_id, [])
		for row in rows:
			requested.setdefault((row["skillId"], row["level"]), []).append(f"npc:{npc_id}")
			row["castDuration"] = npc_cast_duration(templates[row["skillId"]].attr_int("duration"), template["castSpeed"])
		npcs.append({"npcId": npc_id, "name": template["name"], "level": template["level"], "castSpeed": template["castSpeed"], "skills": rows})

	entries = [reporter.entry(skill_id, skill_level, sources) for (skill_id, skill_level), sources in sorted(requested.items())]
	leaves = sorted({effect["class"] for entry in entries for effect in entry["effects"]})
	closed = sorted({cls for entry in entries for effect in entry["effects"] for cls in effect["classChain"]})
	return {
		"format": "aion-m5b2-skills",
		"version": 1,
		"race": race,
		"playerClass": player_class,
		"level": level,
		"character": {
			"skills": [{"skillId": skill_id, "level": skill_level} for skill_id, skill_level in sorted(learned.items())],
			"passives": passives,
			"equippedItems": equipped,
			"castingTimeSources": casting_sources,
			"skillCostSources": cost_sources,
		},
		"soulSickness": {"skillId": rules.soul_sickness_skill, "deathCount": death_count, "maxDeathCount": rules.soul_sickness_max_death_count},
		"npcs": npcs,
		"skills": entries,
		"effectClasses": {"leaves": leaves, "withBases": closed},
	}
