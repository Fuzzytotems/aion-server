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
  `ignoredEffectElements`. The EffectTemplate attributes with their JAXB defaults (EffectTemplate.java:37-95), and effectiveDuration2, what the
  VIRTUAL getDuration2() answers: the attribute plus the constant of the nearest override in the class chain, read from the Java sources -
  AbstractOverTimeEffect.java:64-67 `return duration2 + 1000;` ("on retail these effects last one sec more than their template value"), so
  every damage and heal over time (BleedEffect, PoisonEffect, SpellAttackEffect, HealOverTimeEffect and its HealEffect/MPHealEffect/...) lasts
  1,000 ms longer than its duration2 attribute says;
- effectDuration: Effect.calculateTemplateDuration (Effect.java:899-910) when every template succeeds: the first template in successEffects -
  a ConcurrentHashMap keyed by position, so ascending positions while they are below its 16 bins - whose et.getDuration2() + duration1 *
  skillLevel is positive, minus Rnd.get(0, randomtime), which the oracle reports as effectDurationRandomTime instead of rolling. 0 means no
  timed effect. getDuration2() is the virtual one, so an over time template counts its extra second there, and one with duration2 = 0 is
  still positive (1000) and still decides. The sum is a long (`(long) et.getDuration1()`, "some event skills would produce an int overflow"),
  and calculateEffectsDuration (:884-897) ends in `(int) Math.min(Integer.MAX_VALUE, duration)`: the xpboost templates (duration1 2000000000)
  last Integer.MAX_VALUE ms at every level. Neither the PvP percentage nor the cumulative resist of calculateEffectsDuration applies to an npc
  target or to the effector itself, which are the gate's cases;
- targetSlot: the template's tslot as SkillTargetSlot's name, ordinal (what SM_ABNORMAL_STATE.java:36 and SM_ABNORMAL_EFFECT.java:54 write
  per effect) and id (the slot mask EffectController.broadCastEffects passes, SM_ABNORMAL_EFFECT.java:45);
- the soul sickness: PlayerController.updateSoulSickness (PlayerController.java:712-734) casts skill 8291 at skill level deathCount, which it
  first raises by one while it is below 10; the skill id and the cap are read from the Java source;
- npc skills: NpcSkillData.afterUnmarshal (the first <npc_skills> list naming an npc id wins, NpcSkillData.java:28-36) and the NpcSkillTemplate
  attributes with their JAXB defaults (NpcSkillTemplate.java:12-47);
- the skills an effect or a skill template launches (m5b2-plan.md §10 "what remains", m5e-plan.md §2.4 lesson 2), followed from every
  reported skill to a fixpoint over (skill id, level, whether the skill runs Skill.endCast there), so a cycle ends. None of them goes through
  a start condition or a cooldown. Each launched skill is reported like any other, with `launchedBy` (the skill, the effect - null for a
  penalty - and the chance), and each skill lists its `launches`:
  * <subeffect skill_id chance> under any effect: EffectTemplate.calculateSubEffect (EffectTemplate.java:398-433), at skill level 1 (:419),
    when Rnd.chance() < chance (:415; SubEffect.java:18, default 100), and only while Effect.isLaunchSubEffect() (Effect.java:528), the
    template's <modifiers> match (:401-407) and its <subconditions> hold (:409-412) - the last two are reported as `gates`, as is a class
    that rolls launchSubEffect itself (SignetBurstEffect.java:44);
  * provoker skill_id (ProvokerEffect.java:31-33, :43, :62, :69-80): on each ATTACK (hittype NMLATK or BACKATK) or ATTACKED (any other) of
    the effected, with chance hittypeprob2 (EffectTemplate.java:53-54, default 100), at the launched template's lvl (SkillEngine.java:121-124);
  * delayedskill skill_id (DelayedSkillEffect.java:13-14, :24-25): when the effect ends by time, at the launched template's lvl
    (SkillEngine.applyEffectsDirectly, SkillEngine.java:154-163);
  * carvesignet signet_id (CarveSignetEffect.java:19-44): with chance prob (default 100), skill signet_id + nextSignetLevel - 1 at its lvl
    (SkillEngine.java:169-172), where nextSignetLevel is signet_increment on a target without the signet stack and
    min(carved + signet_increment, max(signet_cap, carved)) on one that carries it. `carved` is any level a <carvesignet> of the same stack
    anywhere in skill_data can carve (another player may have carved the target), or 0 (Effect.java:94) for a signet no carve applied, so the
    levels are the fixpoint `carve_levels` computes; a level this template reaches only through another carver is `viaOtherCarvers`;
  * skilllauncher (SkillLauncherEffect.java:18-24), condskilllauncher (CondSkillLauncherEffect.java:22-23, :46: while the effected's HP is
    at or below `value` % of its max) and aura (AuraEffect.java:31-32, :71) skill_id, each at the launched template's lvl;
  * the template's penalty_skill_id (SkillTemplate.java:82-85), kind `penalty`: Skill.endCast starts it after the skill's own effects
    (Skill.java:646-648, startPenaltySkill :478-489) with no roll (chance 100), unless every effected resisted or dodged (blockedPenaltySkill,
    :603-606, reported as a gate). Without penalty_skill_send_msg it is applied from the first target on the caster at its lvl
    (applyEffectDirectly, :488; SkillEngine.java:121-124); with it the caster casts it on itself at level 1 through a PenaltySkill (:483,
    PenaltySkill.java:7-14), which runs endCast in turn (useWithoutPropSkill), so its own penalty skill follows. Only a skill that runs
    endCast launches its penalty skill: a cast one (an npc_skills row, the soul sickness, an autolearnt or --skill skill that is not PASSIVE;
    _runs_end_cast) or one a penalty with the message casts - never a skill an effect launches, a penalty skill applied without the message
    or a passive, which are all applied directly (Effect.applyEffect / SkillEngine.applyEffect).
  Which effect classes launch a skill is read from the Java sources (an attribute skill_id, signet_id or npc_id, a SkillEngine call, a
  hand-made Effect, a useSkill, PlayerReviveService, PET_SKILL_DATA), and so is every shape above, the penalty's included. Of the launches an
  effect class or a skill template makes, the oracle refuses (OracleError) every one it does not model rather than under-report: a resurrect
  / resurrectbase / resurrectpos / rebirth skill_id (cast on revive through PlayerReviveService), a petorderuseultraskill (the pet's skill
  from pet_skill_templates), a summon (the summoned npc casts its own skills), a <subeffect addeffect="true"> (launched at the signet burst
  count, EffectTemplate.java:421-424), more than one <subeffect> on an effect, more than one effect with a <subeffect> in a skill (an Effect
  keeps one sub effect, Effect.java:479-481, which each calculateSubEffect overwrites, EffectTemplate.java:431, and one <subconditions>
  abort flag for all of them, :409-412 and Effect.java:1001-1003, so they are no independent launches), a launch without its skill id, a
  launched id without a template, a class that overrides calculateSubEffect, a modelled launcher whose source launches in more ways than the
  modelled one, and any modelled shape the Java no longer has.
  Out of scope, as neither a skill template nor an effect class makes it: the critical hit proc. After a Player's critical hit - a skill
  attack that is not MAGICAL and has no sub effect (Effect.java:533-541) or an auto-attack (CreatureController.java:341-345) -
  SkillEngine.createCriticalProcEffect
  (SkillEngine.java:193-225) launches, with chance 10, 8218 (StumbleEffect) with a POLEARM, STAFF or GREATSWORD main hand or 8217
  (StaggerEffect) with a BOW, at its lvl. The equipped weapon decides, not the skills, so `launchedSkills` and `addedByLaunches` hold for the
  character's starting gear, whose weapons (SWORD, SPELLBOOK, MACE, DAGGER, GUN, HARP; player_initial_data.xml) launch nothing this way.
  What an AI script, an item or a chain or charge skill casts is a cast of its own, not a launch, and is not followed either.
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
JAVA_INT_MAX = 2**31 - 1
SUB_EFFECT = "SubEffect"
CARVE_SIGNET_EFFECT = "CarveSignetEffect"
PROVOKE_TARGETS = ("ME", "OPPONENT")  # ProvokerEffect.getProvokeTarget's exhaustive switch (ProvokerEffect.java:83-87)


def _read(source: Path) -> str:
	try:
		return source.read_text(encoding="utf-8")
	except OSError as e:
		raise OracleError(f"{source}: {e}") from e


def _strip_comments(text: str) -> str:
	return re.sub(r"//[^\n]*", "", re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL))


def _int32(value: int) -> int:
	"""Java int arithmetic: the value wrapped to 32 bits two's complement."""
	return (value + 2**31) % 2**32 - 2**31


@dataclass(frozen=True)
class JavaSkillRules:
	"""The tables the report needs from the Java sources."""

	effect_classes: dict[str, str]         # Effects.java @XmlElements: XML tag -> effect class
	superclasses: dict[str, str]           # effect class -> the class it extends (skillengine/effect/*.java)
	target_slots: dict[str, tuple[int, int]]  # SkillTargetSlot name -> (ordinal, id)
	soul_sickness_skill: int               # PlayerController.updateSoulSickness: `if (skillId == 0) skillId = N;`
	soul_sickness_max_death_count: int     # ... `if (deathCount < N) deathCount++;`
	cast_duration_cap: float               # Skill.calculateMagicalCastDuration: `Math.round(baseCastDuration * Nf)`
	duration2_bonuses: dict[str, int]      # effect class -> N of its `int getDuration2() { return duration2 [+ N]; }` (EffectTemplate: 0)
	launch: "LaunchRules | None" = None    # which effect classes launch a skill, and how (read() always sets it)

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
		duration2_bonuses: dict[str, int] = {}
		texts: dict[str, str] = {}
		for source in sorted(effect_dir.glob("*.java")):
			text = _strip_comments(_read(source))
			texts[source.stem] = text
			match = re.search(r"\bclass\s+(\w+)(?:<[^>]*>)?\s+extends\s+(\w+)", text)
			if match and match.group(1) == source.stem:
				superclasses[match.group(1)] = match.group(2)
			# Effect.calculateTemplateDuration reads the three duration getters virtually. getDuration2 has one override today
			# (AbstractOverTimeEffect.java:64-67); an override of another shape, or of the other two, is a rule this oracle does not know.
			# The method is found by its head alone, as getDuration1/getRandomTime below are, and only then held to the one shape: a body with
			# braces of its own (an if, a nested block) or with anything else in it is refused, never skipped.
			for head in re.finditer(r"\bint\s+getDuration2\s*\(\s*\)\s*\{", text):
				getter = re.compile(r"\bint\s+getDuration2\s*\(\s*\)\s*\{([^{}]*)\}").match(text, head.start())
				body = re.fullmatch(r"\s*return\s+duration2\s*(?:\+\s*(\d+)\s*)?;\s*", getter.group(1)) if getter else None
				if not body:
					raise OracleError(f"{source.name}: getDuration2() is not `return duration2 [+ N];`, the shape this oracle was written against")
				if source.stem in duration2_bonuses:
					raise OracleError(f"{source.name}: a second getDuration2() (a nested class?), which this oracle does not model")
				duration2_bonuses[source.stem] = int(body.group(1) or 0)
			for other in ("getDuration1", "getRandomTime"):
				if source.stem != EFFECT_TEMPLATE and re.search(rf"\bint\s+{other}\s*\(\s*\)\s*\{{", text):
					raise OracleError(f"{source.name} overrides {other}(), which Effect.calculateTemplateDuration reads and this oracle does not model")
		if duration2_bonuses.get(EFFECT_TEMPLATE) != 0:
			raise OracleError(f"{effect_dir / 'EffectTemplate.java'}: no getDuration2() answering `return duration2;`")

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
		return JavaSkillRules(effect_classes, superclasses, slots, int(skill.group(1)), int(cap.group(1)), f32(float(cap_match.group(1))),
		                      duration2_bonuses, _read_launch_rules(base, texts, superclasses, skill_java))

	def duration2_bonus(self, cls: str) -> int:
		"""What the virtual EffectTemplate.getDuration2() adds to the duration2 attribute of a template of class `cls`: the override nearest to
		the class in its `extends` chain - 1000 below AbstractOverTimeEffect (AbstractOverTimeEffect.java:64-67), 0 through EffectTemplate's."""
		for ancestor in self.class_chain(cls):
			if ancestor in self.duration2_bonuses:
				return self.duration2_bonuses[ancestor]
		raise OracleError(f"effect class {cls}: no getDuration2() in its extends chain")  # read() guarantees EffectTemplate's

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


# ---- which effect classes launch a skill, read from the Java sources ---------------------------------------------------------------------

# How an effect class's own source launches a skill (EffectTemplate's <subeffect> is read on its own): an attribute naming a skill, a signet
# chain or a summoned npc (which casts its own skills), a SkillEngine call, a hand-made Effect, a creature made to use a skill, the revive
# service (which casts a resurrection's skill_id through updateSoulSickness) and the pet order table.
LAUNCH_EVIDENCE = (
	(re.compile(r'@XmlAttribute\(\s*name\s*=\s*"(skill_id|signet_id|npc_id)"'), "binds the attribute {0}"),
	(re.compile(r"\bSkillEngine\.getInstance\(\)\s*\.\s*(\w+)\s*\("), "calls SkillEngine.{0}"),
	(re.compile(r"\bnew\s+Effect\s*\("), "creates an Effect"),
	(re.compile(r"\.\s*useSkill\s*\("), "makes a creature use a skill"),
	(re.compile(r"\bPlayerReviveService\s*\.\s*(\w+)\s*\("), "calls PlayerReviveService.{0}"),
	(re.compile(r"\bPET_SKILL_DATA\b"), "reads PET_SKILL_DATA"),
)

# The SkillEngine entry points the modelled launches call, by the head of their declaration (SkillEngine.java:121-172). The report gives a
# launched skill its template's lvl, so each must apply it at `skillTemplate.getLvl()`.
SKILL_ENGINE_ENTRY_POINTS = {
	"applyEffectDirectly(int, Creature, Creature)":
		r"public\s+Effect\s+applyEffectDirectly\(\s*int\s+skillId\s*,\s*Creature\s+effector\s*,\s*Creature\s+effected\s*\)\s*\{",
	"applyEffectDirectly(int, Creature, Creature, Integer, ForceType)":
		r"public\s+Effect\s+applyEffectDirectly\(\s*int\s+skillId\s*,\s*Creature\s+effector\s*,\s*Creature\s+effected\s*,\s*Integer\s+duration\s*,"
		r"\s*ForceType\s+forceType\s*\)\s*\{",
	"applyEffectsDirectly(int, Creature, Creature, float, float, float)":
		r"public\s+List<Creature>\s+applyEffectsDirectly\(\s*int\s+skillId\s*,\s*Creature\s+effector\s*,\s*Creature\s+firstTarget\s*,\s*float\s+x\s*,"
		r"\s*float\s+y\s*,\s*float\s+z\s*\)\s*\{",
	"applyEffect(int, Creature, Creature)":
		r"public\s+Effect\s+applyEffect\(\s*int\s+skillId\s*,\s*Creature\s+effector\s*,\s*Creature\s+effected\s*\)\s*\{",
}


@dataclass(frozen=True)
class LaunchModel:
	"""One modelled way an effect class launches a skill: the report's kind, the SkillEngine entry point, and what the Java must say."""

	kind: str
	engine: str                                        # a key of SKILL_ENGINE_ENTRY_POINTS
	shapes: tuple[str, ...]                            # regular expressions the class's own (comment-stripped) source must match
	defaults: tuple[tuple[str, str, str], ...] = ()    # (XML attribute, class declaring it, regex whose group is its JAXB default)
	attribute: str = "skill_id"                        # the attribute naming the launched skill

	def evidence(self) -> list[str]:
		"""What LAUNCH_EVIDENCE finds in a class that launches this way and no other: its attribute and its one SkillEngine call."""
		return sorted([f"binds the attribute {self.attribute}", f"calls SkillEngine.{self.engine.partition('(')[0]}"])


_SKILL_ID_FIELD = r'@XmlAttribute\(\s*name\s*=\s*"skill_id"\s*\)\s*protected\s+int\s+skillId\s*;'
LAUNCH_MODELS = {
	# ProvokerEffect.java:31-33, :43, :62, :69-80: per qualifying hit, the skill at its lvl on the provoker's effector or on the opponent
	"ProvokerEffect": LaunchModel("provoker", "applyEffectDirectly(int, Creature, Creature)", (
		_SKILL_ID_FIELD,
		r'@XmlAttribute\(\s*name\s*=\s*"provoke_target"\s*\)\s*protected\s+ProvokeTarget\s+provokeTarget\s*;',
		r"ObserverType\s+observerType\s*=\s*hitType\s*==\s*HitType\.NMLATK\s*\|\|\s*hitType\s*==\s*HitType\.BACKATK\s*\?\s*ObserverType\.ATTACK\s*:"
		r"\s*ObserverType\.ATTACKED\s*;",
		r"if\s*\(\s*Rnd\.chance\(\)\s*>=\s*hitTypeProb\s*\)\s*return\s+false\s*;",
		r"SkillEngine\.getInstance\(\)\.applyEffectDirectly\(\s*skillId\s*,\s*effector\s*,\s*getProvokeTarget\(\s*effector\s*,\s*target\s*\)\s*\)\s*;"),
		(("hittype", EFFECT_TEMPLATE, r'@XmlAttribute\(\s*name\s*=\s*"hittype"\s*\)\s*protected\s+HitType\s+hitType\s*=\s*HitType\.(\w+)\s*;'),
		 ("hittypeprob2", EFFECT_TEMPLATE, r'@XmlAttribute\(\s*name\s*=\s*"hittypeprob2"\s*\)\s*protected\s+int\s+hitTypeProb\s*=\s*(\d+)\s*;'),
		 ("radius", "ShieldEffect", r"@XmlAttribute\s+protected\s+int\s+radius\s*=\s*(\d+)\s*;"))),
	# DelayedSkillEffect.java:13-14, :24-25: when the effect ends by time, the skill at its lvl on the targets of its own properties
	"DelayedSkillEffect": LaunchModel("delayedskill", "applyEffectsDirectly(int, Creature, Creature, float, float, float)", (
		_SKILL_ID_FIELD,
		r"if\s*\(\s*effect\.isEndedByTime\(\)\s*\)\s*SkillEngine\.getInstance\(\)\.applyEffectsDirectly\(\s*skillId\s*,")),
	# CarveSignetEffect.java:19-28, :34-44: with chance prob, signet_id + nextSignetLevel - 1 at its lvl
	CARVE_SIGNET_EFFECT: LaunchModel("carvesignet", "applyEffect(int, Creature, Creature)", (
		r'@XmlAttribute\(\s*name\s*=\s*"signet_cap"[^)]*\)\s*protected\s+int\s+signetCap\s*;',
		r'@XmlAttribute\(\s*name\s*=\s*"signet_id"[^)]*\)\s*protected\s+int\s+signetId\s*;',
		r"@XmlAttribute(?:\([^)]*\))?\s*protected\s+String\s+signet\s*;",
		r"if\s*\(\s*Rnd\.chance\(\)\s*>=\s*prob\s*\)\s*return\s*;",
		r"int\s+nextSignetLevel\s*=\s*signetIncrement\s*;",
		r"getAbnormalEffect\(\s*signet\s*\)",
		r"nextSignetLevel\s*=\s*Math\.min\(\s*activeSignet\.getCarvedSignet\(\)\s*\+\s*signetIncrement\s*,\s*Math\.max\(\s*signetCap\s*,"
		r"\s*activeSignet\.getCarvedSignet\(\)\s*\)\s*\)\s*;",
		r"SkillEngine\.getInstance\(\)\.applyEffect\(\s*signetId\s*\+\s*nextSignetLevel\s*-\s*1\s*,",
		r"\.setCarvedSignet\(\s*nextSignetLevel\s*\)"),
		(("signet_increment", CARVE_SIGNET_EFFECT,
		  r'@XmlAttribute\(\s*name\s*=\s*"signet_increment"[^)]*\)\s*protected\s+int\s+signetIncrement\s*=\s*(\d+)\s*;'),
		 ("prob", CARVE_SIGNET_EFFECT, r"@XmlAttribute(?:\([^)]*\))?\s*protected\s+int\s+prob\s*=\s*(\d+)\s*;")), "signet_id"),
	# SkillLauncherEffect.java:18-24: on apply, the skill at its lvl from the effector on the effected
	"SkillLauncherEffect": LaunchModel("skilllauncher", "applyEffect(int, Creature, Creature)", (
		_SKILL_ID_FIELD,
		r"SkillEngine\.getInstance\(\)\.applyEffect\(\s*skillId\s*,\s*effect\.getEffector\(\)\s*,\s*effect\.getEffected\(\)\s*\)\s*;")),
	# CondSkillLauncherEffect.java:22-23, :46: while the effected's HP is at or below `value` % of its max, the skill at its lvl on itself
	"CondSkillLauncherEffect": LaunchModel("condskilllauncher", "applyEffectDirectly(int, Creature, Creature, Integer, ForceType)", (
		_SKILL_ID_FIELD,
		r"hpValue\s*<=\s*value\s*\*\s*effect\.getEffected\(\)\.getLifeStats\(\)\.getMaxHp\(\)\s*/\s*100",
		r"SkillEngine\.getInstance\(\)\.applyEffectDirectly\(\s*skillId\s*,\s*effect\.getEffected\(\)\s*,\s*effect\.getEffected\(\)\s*,\s*duration\s*,"
		r"\s*null\s*\)\s*;")),
	# AuraEffect.java:31-32, :71, :76: on every period, the skill at its lvl on each creature the aura reaches, by itself
	"AuraEffect": LaunchModel("aura", "applyEffect(int, Creature, Creature)", (
		_SKILL_ID_FIELD, r"SkillEngine\.getInstance\(\)\.applyEffect\(\s*skillId\s*,\s*effected\s*,\s*effected\s*\)\s*;")),
}

# EffectTemplate.calculateSubEffect (EffectTemplate.java:398-433) and SubEffect (SubEffect.java:15-20) as the closure models them
SUBEFFECT_SHAPES = (
	(EFFECT_TEMPLATE, r'@XmlElement\(\s*name\s*=\s*"subeffect"\s*\)\s*protected\s+SubEffect\s+subEffect\s*;'),
	(SUB_EFFECT, r'@XmlAttribute\(\s*name\s*=\s*"skill_id"[^)]*\)\s*private\s+int\s+skillId\s*;'),
	(SUB_EFFECT, r'@XmlAttribute\(\s*name\s*=\s*"addeffect"\s*\)\s*private\s+boolean\s+addEffect\b'),
)
SUBEFFECT_BODY_SHAPES = (
	r"if\s*\(\s*Rnd\.chance\(\)\s*>=\s*subEffect\.getChance\(\)\s*\)\s*return\s*;",
	r"DataManager\.SKILL_DATA\.getSkillTemplate\(\s*subEffect\.getSkillId\(\)\s*\)",
	r"if\s*\(\s*subEffect\.isAddEffect\(\)\s*\)",
	r"new\s+Effect\([^;]*,\s*template\s*,\s*level\s*,",
)

# The penalty skill as the closure models it, by file below skillengine/: the attribute (SkillTemplate.java:82-85, :265-271), when
# Skill.endCast starts it (Skill.java:603-606, :646-648), the PenaltySkill the message arm casts (PenaltySkill.java:7-14: on the caster
# itself, through useWithoutPropSkill, so it runs endCast in turn) and SkillEngine.getPenaltySkill (SkillEngine.java:109-115)
PENALTY_SHAPES = (
	("model/SkillTemplate.java", r'@XmlAttribute\(\s*name\s*=\s*"penalty_skill_id"\s*\)\s*private\s+int\s+penaltySkillId\s*;'),
	("model/SkillTemplate.java", r"public\s+int\s+getPenaltySkillId\(\)\s*\{\s*return\s+penaltySkillId\s*;\s*\}"),
	("model/SkillTemplate.java", r"public\s+boolean\s+shouldPenaltySkillSendMsg\(\)\s*\{\s*return\s+penaltySkillSendMsg\s*;\s*\}"),
	("model/Skill.java", r"if\s*\(\s*resistCount\s*==\s*effectedList\.size\(\)\s*\)\s*\{[^{}]*\bblockedPenaltySkill\s*=\s*true\s*;"),
	("model/Skill.java", r"if\s*\(\s*!\s*blockedPenaltySkill\s*\)\s*startPenaltySkill\(\)\s*;"),
	("model/PenaltySkill.java", r"super\(\s*skillTemplate\s*,\s*effector\s*,\s*skillLevel\s*,\s*effector\s*,\s*null\s*\)\s*;"),
	("model/PenaltySkill.java", r"public\s+boolean\s+useSkill\(\)\s*\{\s*super\.useWithoutPropSkill\(\)\s*;"),
	("SkillEngine.java", r"return\s+new\s+PenaltySkill\(\s*template\s*,\s*effector\s*,\s*skillLevel\s*\)\s*;"),
)
PENALTY_SEND_MSG = r'@XmlAttribute\(\s*name\s*=\s*"penalty_skill_send_msg"\s*\)\s*private\s+boolean\s+penaltySkillSendMsg\s*(?:=\s*(true|false)\s*)?;'
# Skill.startPenaltySkill (Skill.java:478-489), the whole body: group 2 is the level of the message arm
PENALTY_BODY = (
	r"\s*int\s+penaltySkill\s*=\s*skillTemplate\.getPenaltySkillId\(\)\s*;"
	r"\s*if\s*\(\s*penaltySkill\s*==\s*0\s*\)\s*return\s*;"
	r"\s*if\s*\(\s*getSkillTemplate\(\)\.shouldPenaltySkillSendMsg\(\)\s*\)\s*\{"
	r"\s*PenaltySkill\s+(\w+)\s*=\s*SkillEngine\.getInstance\(\)\.getPenaltySkill\(\s*effector\s*,\s*penaltySkill\s*,\s*(\d+)\s*\)\s*;"
	r"\s*if\s*\(\s*\1\s*!=\s*null\s*\)\s*\{?\s*\1\.useSkill\(\)\s*;\s*\}?"
	r"\s*\}\s*else\s*\{?"
	r"\s*SkillEngine\.getInstance\(\)\.applyEffectDirectly\(\s*penaltySkill\s*,\s*firstTarget\s*,\s*effector\s*\)\s*;"
	r"\s*\}?\s*"
)
PENALTY_ENGINE = "applyEffectDirectly(int, Creature, Creature)"


def _method_body(text: str, head: str) -> str | None:
	"""The body of the first method whose declaration matches `head` (a regex ending in its opening brace), found by counting braces."""
	match = re.search(head, text)
	if not match:
		return None
	depth = 1
	for i in range(match.end(), len(text)):
		if text[i] == "{":
			depth += 1
		elif text[i] == "}":
			depth -= 1
			if depth == 0:
				return text[match.end():i]
	return None


def _evidence_all(text: str) -> list[str]:
	"""Every LAUNCH_EVIDENCE match in `text`, a label per match: two calls of one SkillEngine method are two entries."""
	found = []
	for pattern, label in LAUNCH_EVIDENCE:
		found.extend(label.format(*match.groups()) for match in pattern.finditer(text))
	return found


def _evidence(text: str) -> list[str]:
	return list(dict.fromkeys(_evidence_all(text)))


@dataclass(frozen=True)
class LaunchRules:
	"""What the launched-skill closure reads from the Java sources."""

	evidence: dict[str, str]                      # effect class -> how its own source launches a skill (EffectTemplate's subeffect aside)
	kinds: dict[str, str]                         # modelled launcher class whose Java has the modelled shape -> the report's kind
	unmodelled: dict[str, str]                    # modelled launcher class whose Java does not -> why
	defaults: dict[str, dict[str, int | str]]     # kind -> XML attribute -> its JAXB default
	subeffect_error: str | None                   # why calculateSubEffect / SubEffect is not the modelled shape; None when it is
	subeffect_level: int                          # calculateSubEffect: `int level = N;`
	subeffect_chance: int                         # SubEffect: `private int chance = N;`
	subeffect_overrides: frozenset[str]           # effect classes that override calculateSubEffect or startSubEffect
	launch_rolls: frozenset[str]                  # effect classes that roll Effect.setLaunchSubEffect themselves
	template_error: str | None                    # EffectTemplate launching a skill outside calculateSubEffect: every effect would
	penalty_error: str | None = None              # why the penalty skill is not the modelled shape (PENALTY_*); None when it is
	penalty_level: int = 0                        # startPenaltySkill: `getPenaltySkill(effector, penaltySkill, N)`, the message arm's level
	penalty_send_msg_default: bool = False        # SkillTemplate: the JAXB default of penalty_skill_send_msg

	def launcher(self, chain: list[str]) -> tuple[str | None, str | None]:
		"""(kind, None) for a modelled launcher, (None, why) for a class that launches in a shape the oracle does not model, (None, None) for
		a class that launches nothing. `chain` is the class and its superclasses: a class inherits the launches of its superclasses."""
		found = [cls for cls in chain if cls in self.evidence]
		if not found:
			return None, None
		leaf = chain[0]
		if found == [leaf] and leaf in self.kinds:
			return self.kinds[leaf], None
		if found == [leaf] and leaf in self.unmodelled:
			return None, self.unmodelled[leaf]
		return None, "; ".join(f"{cls} {self.evidence[cls]}" for cls in found)


def _read_launch_rules(base: Path, texts: dict[str, str], superclasses: dict[str, str], skill_java: str) -> LaunchRules:
	engine = _strip_comments(_read(base / "skillengine" / "SkillEngine.java"))
	engine_errors: dict[str, str] = {}
	for name, head in SKILL_ENGINE_ENTRY_POINTS.items():
		body = _method_body(engine, head)
		if body is None:
			engine_errors[name] = f"SkillEngine.java has no {name}"
		elif not re.search(r"\bskillTemplate\.getLvl\(\)", body):
			engine_errors[name] = f"SkillEngine.{name} does not apply the skill at skillTemplate.getLvl()"

	def is_effect(cls: str) -> bool:
		seen = set()
		while cls in superclasses and cls not in seen:
			seen.add(cls)
			cls = superclasses[cls]
		return cls == EFFECT_TEMPLATE

	evidence = {}
	for cls in sorted(superclasses):
		found = _evidence(texts[cls]) if is_effect(cls) else []
		if found:
			evidence[cls] = ", ".join(found)

	kinds, unmodelled, defaults = {}, {}, {}
	for cls, model in LAUNCH_MODELS.items():
		text = texts.get(cls)
		problem = None
		if text is None:
			problem = f"{cls}.java not found"
		else:
			missing = next((shape for shape in model.shapes if not re.search(shape, text)), None)
			if missing:
				problem = f"{cls}.java does not have the launch shape this oracle models (/{missing}/)"
			elif sorted(_evidence_all(text)) != model.evidence():
				problem = (f"{cls}.java launches in more ways than this oracle models ({', '.join(_evidence_all(text))}; the model: "
				           f"{', '.join(model.evidence())}), and the closure would follow only the modelled one")
			elif model.engine in engine_errors:
				problem = engine_errors[model.engine]
		values: dict[str, int | str] = {}
		for attribute, owner, pattern in model.defaults:
			match = re.search(pattern, texts.get(owner, ""))
			if match:
				values[attribute] = int(match.group(1)) if match.group(1).isdigit() else match.group(1)
			elif problem is None:
				problem = f"{owner}.java: the JAXB default of {attribute} is not /{pattern}/"
		if problem:
			unmodelled[cls] = problem
		else:
			kinds[cls] = model.kind
			defaults[model.kind] = values

	template = texts.get(EFFECT_TEMPLATE, "")
	problems = [f"{owner}.java does not match /{shape}/" for owner, shape in SUBEFFECT_SHAPES if not re.search(shape, texts.get(owner, ""))]
	body = _method_body(template, r"public\s+void\s+calculateSubEffect\(\s*Effect\s+effect\s*\)\s*\{")
	level = re.search(r"\bint\s+level\s*=\s*(\d+)\s*;", body or "")
	chance = re.search(r"@XmlAttribute\s+private\s+int\s+chance\s*=\s*(\d+)\s*;", texts.get(SUB_EFFECT, ""))
	if body is None:
		problems.append("EffectTemplate.java has no calculateSubEffect(Effect)")
	else:
		problems += [f"EffectTemplate.calculateSubEffect does not match /{shape}/" for shape in SUBEFFECT_BODY_SHAPES if not re.search(shape, body)]
	if not level:
		problems.append("EffectTemplate.calculateSubEffect has no `int level = N;`")
	if not chance:
		problems.append("SubEffect.java has no `private int chance = N;`")
	stray = _evidence(template.replace(body, "", 1) if body else template)
	overrides = frozenset(cls for cls in superclasses if is_effect(cls)
	                      and re.search(r"\bvoid\s+(?:calculateSubEffect|startSubEffect)\s*\(", texts[cls]))
	rolls = frozenset(cls for cls in superclasses if is_effect(cls) and re.search(r"\bsetLaunchSubEffect\s*\(", texts[cls]))
	penalty_error, penalty_level, penalty_send_msg_default = _read_penalty_rules(base / "skillengine", skill_java, engine, engine_errors)
	return LaunchRules(
		evidence, kinds, unmodelled, defaults, "; ".join(problems) or None, int(level.group(1)) if level else 0,
		int(chance.group(1)) if chance else 0, overrides, rolls,
		f"EffectTemplate.java launches a skill outside calculateSubEffect ({', '.join(stray)}), which every effect class inherits" if stray else None,
		penalty_error, penalty_level, penalty_send_msg_default)


def _read_penalty_rules(skill_dir: Path, skill_java: str, engine: str, engine_errors: dict[str, str]) -> tuple[str | None, int, bool]:
	"""(why the penalty skill is not the modelled shape or None, the level of the message arm, the JAXB default of penalty_skill_send_msg).
	A missing file is a reason, not an error: the oracle refuses only when a closure reaches a penalty skill."""
	texts = {"model/Skill.java": skill_java, "SkillEngine.java": engine}
	for relative in ("model/SkillTemplate.java", "model/PenaltySkill.java"):
		try:
			texts[relative] = _strip_comments((skill_dir / relative).read_text(encoding="utf-8"))
		except OSError:
			texts[relative] = None
	problems = [f"{relative} not found" for relative, text in texts.items() if text is None]
	problems += [f"{relative} does not match /{shape}/" for relative, shape in PENALTY_SHAPES
	             if texts[relative] is not None and not re.search(shape, texts[relative])]
	send_msg = re.search(PENALTY_SEND_MSG, texts["model/SkillTemplate.java"] or "")
	if not send_msg:
		problems.append(f"model/SkillTemplate.java does not match /{PENALTY_SEND_MSG}/")
	body = _method_body(skill_java, r"private\s+void\s+startPenaltySkill\(\s*\)\s*\{")
	shape = re.fullmatch(PENALTY_BODY, body) if body is not None else None
	if not shape:
		problems.append("Skill.startPenaltySkill is not the body this oracle models (PENALTY_BODY)")
	if len(re.findall(r"\bstartPenaltySkill\s*\(\s*\)\s*;", skill_java)) != 1 or len(re.findall(r"\bblockedPenaltySkill\s*=\s*true\b", skill_java)) != 1:
		problems.append("Skill.java calls startPenaltySkill() or blocks it in more than the one place this oracle models")
	if PENALTY_ENGINE in engine_errors:
		problems.append(engine_errors[PENALTY_ENGINE])
	return "; ".join(problems) or None, int(shape.group(2)) if shape else 0, bool(send_msg) and send_msg.group(1) == "true"


@dataclass
class SkillTemplateInfo:
	"""The attributes and child elements of one <skill_template>, read while the streamed element is still alive."""

	skill_id: int
	attrs: dict[str, str]
	properties: dict[str, str] | None
	conditions: dict[str, list[tuple[str, dict[str, str]]]]
	effects: list[tuple[str, dict[str, str], list[dict[str, str]], list[str]]]  # tag, attributes, <change> attributes, other child tags
	actions: list[tuple[str, dict[str, str]]] = field(default_factory=list)
	subeffects: list[list[dict[str, str]]] = field(default_factory=list)  # per entry of `effects`: the attributes of its <subeffect>s

	@staticmethod
	def of(element: ET.Element) -> "SkillTemplateInfo":
		skill_id = java_int(element.get("skill_id"), "skill_template skill_id")
		properties = element.find("properties")
		conditions = {}
		for key, tag in CONDITION_SECTIONS:
			section = element.find(tag)
			conditions[key] = [(c.tag, dict(c.attrib)) for c in section] if section is not None else []
		effects, subeffects = [], []
		container = element.find("effects")
		if container is not None:
			for child in container:
				changes = [dict(c.attrib) for c in child.findall("change")]
				others = [c.tag for c in child if c.tag != "change"]
				effects.append((child.tag, dict(child.attrib), changes, others))
				subeffects.append([dict(c.attrib) for c in child.findall("subeffect")])
		actions_element = element.find("actions")
		actions = [(a.tag, dict(a.attrib)) for a in actions_element] if actions_element is not None else []
		return SkillTemplateInfo(skill_id, dict(element.attrib), dict(properties.attrib) if properties is not None else None, conditions, effects,
		                         actions, subeffects)

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
	successEffects order with a positive et.getDuration2() + duration1 * skillLevel, (0, 0) when there is none. getDuration2() is virtual, so it
	is each effect's effectiveDuration2, not its duration2 attribute: an over time template adds its 1,000 ms before the `> 0` test.
	successEffects is a ConcurrentHashMap keyed by position that a later template of the same position overwrites; the ascending order only
	holds below its 16 initial bins, so a position outside 0..15 answers None.
	"""
	by_position: dict[int, dict] = {}
	for effect in effects:
		by_position[effect["position"]] = effect
	if any(p < 0 or p > 15 for p in by_position):
		return None
	for position in sorted(by_position):
		effect = by_position[position]
		duration = effect["effectiveDuration2"] + effect["duration1"] * level  # Effect.java:902, a long: `(long) et.getDuration1()`
		if duration > 0:
			return duration, effect["randomTime"]
	return 0, 0


def effects_duration(template_duration: tuple[int, int]) -> tuple[int, int] | None:
	"""
	Effect.calculateEffectsDuration (Effect.java:884-897) over calculateTemplateDuration's long, without the PvP percentage and the cumulative
	resist (neither applies to the gate's targets): `(int) Math.min(Integer.MAX_VALUE, duration)`, where the random part was already subtracted.
	(duration, randomtime) as the report writes them: a duration above Integer.MAX_VALUE that every roll leaves above it is the clamp with no
	random part left (the xpboost templates of skill_templates.xml, 2000000000 per level); None when the roll decides whether the clamp
	applies, which no single pair can say.
	"""
	duration, random_time = template_duration
	if duration <= JAVA_INT_MAX:
		return duration, random_time
	if duration - random_time >= JAVA_INT_MAX:
		return JAVA_INT_MAX, 0
	return None


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
			duration2 = java_int(attrs.get("duration2"), f"{what} duration2", 0)
			effects.append({
				"tag": tag,
				"class": cls,
				"classChain": self.rules.class_chain(cls),
				"position": java_int(attrs.get("e"), f"{what} e", 0),
				"duration1": java_int(attrs.get("duration1"), f"{what} duration1", 0),
				"duration2": duration2,
				"effectiveDuration2": _int32(duration2 + self.rules.duration2_bonus(cls)),  # the virtual getDuration2(), an int
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
		else:
			duration = effects_duration(duration)
			if duration is None:
				not_modelled.append("effectDuration: the randomtime roll decides whether calculateEffectsDuration's Integer.MAX_VALUE clamp applies")

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


# ---- the skills an effect launches, followed to a fixpoint -------------------------------------------------------------------------------

def carve_levels(increment: int, cap: int, carved: set[int]) -> set[int]:
	"""CarveSignetEffect.applyEffect's nextSignetLevel (CarveSignetEffect.java:37-41) for every state of the target: signetIncrement without
	the signet stack, min(carved + signetIncrement, max(signetCap, carved)) with it, carved being a level in `carved` or 0 (Effect.java:94, a
	signet no carve applied)."""
	return {increment} | {min(level + increment, max(cap, level)) for level in carved | {0}}


def carvable_levels(carvers: list[tuple[int, int]]) -> set[int]:
	"""The levels a signet stack can carry: the fixpoint of carve_levels over every (signet_increment, signet_cap) that carves the stack."""
	for increment, _ in carvers:
		if increment < 1:
			raise OracleError(f"<carvesignet signet_increment={increment}>: below 1 the carved level has no bound, which this oracle does not model")
	levels: set[int] = set()
	while True:
		grown = set(levels)
		for increment, cap in carvers:
			grown |= carve_levels(increment, cap, levels)
		if grown == levels:
			return levels
		levels = grown


class LaunchClosure:
	"""
	The launch edges of the reported skills, followed to a fixpoint. `edges` maps a skill id to its launches (they do not depend on the level
	the skill runs at): the launching effect (None for the template's penalty skill), the launched skill and the level it runs at, the chance
	and what else decides the launch. An edge of chance 0 or less is reported but not followed (Rnd.chance() is never below 0), and a penalty
	edge is taken only from a skill that runs Skill.endCast (states). `templates` grows with every template the closure loads, so it is the
	dict the SkillReporter reads.
	"""

	def __init__(self, data: StaticData, rules: JavaSkillRules, templates: dict[int, SkillTemplateInfo]):
		if rules.launch is None:
			raise OracleError("JavaSkillRules without launch rules: build them with JavaSkillRules.read")
		if rules.launch.template_error:
			raise OracleError(rules.launch.template_error)
		self.data = data
		self.rules = rules
		self.launch = rules.launch
		self.templates = templates
		self.edges: dict[int, list[dict]] = {}
		self._carvers: dict[str, list[tuple[int, int]]] | None = None

	def close(self, roots: set[tuple[int, int]], cast: set[tuple[int, int]]) -> set[tuple[int, int]]:
		"""Every (skill id, level) the roots reach, the roots included; `cast` are the roots that run Skill.endCast (see states)."""
		return {pair for pair, _ in self.states(roots, cast)}

	def states(self, roots: set[tuple[int, int]], cast: set[tuple[int, int]]) -> set[tuple[tuple[int, int], bool]]:
		"""
		((skill id, level), runs Skill.endCast) of every skill the roots reach, the fixpoint over these states, so a cycle ends. A root runs
		endCast when it is in `cast` (_runs_end_cast), a launched skill when a penalty with the message casts it (runsEndCast: PenaltySkill.useSkill
		-> useWithoutPropSkill); only a state that runs endCast takes a penalty edge (Skill.java:646-648). A skill an effect launches never does.
		"""
		start = {(pair, pair in cast) for pair in roots}
		reached = set(start)
		frontier = sorted(start)
		while frontier:
			self._resolve({skill_id for (skill_id, _), _ in frontier})
			following = []
			for (skill_id, _), casts in frontier:
				for edge in self.edges[skill_id]:
					target = ((edge["skillId"], edge["level"]), edge.get("runsEndCast", False))
					if self.takes(edge, casts) and target not in reached:
						reached.add(target)
						following.append(target)
			frontier = sorted(following)
		return reached

	@staticmethod
	def takes(edge: dict, casts: bool) -> bool:
		"""Whether a skill takes the launch edge: a followed one (chance above 0), and a penalty only when the skill runs endCast (`casts`)."""
		return edge["followed"] and (casts or edge["kind"] != "penalty")

	def launched_by(self, states: set[tuple[tuple[int, int], bool]]) -> dict[tuple[int, int], list[dict]]:
		"""(skill id, level) -> the edges taken into it from `states`, which must be closed: one link per launching (skill id, level), which
		takes every edge it takes in any of its states (one that runs endCast takes all the other one does, and the penalty)."""
		incoming: dict[tuple[int, int], list[dict]] = {}
		casting = {pair for pair, casts in states if casts}
		for skill_id, level in sorted({pair for pair, _ in states}):
			for edge in self.edges[skill_id]:
				if self.takes(edge, (skill_id, level) in casting):
					link = {"skillId": skill_id, "level": level, "kind": edge["kind"], "effect": edge["effect"], "chance": edge["chance"]}
					if "carvedLevel" in edge:
						link["carvedLevel"] = edge["carvedLevel"]
					incoming.setdefault((edge["skillId"], edge["level"]), []).append(link)
		return incoming

	def _resolve(self, skill_ids: set[int]) -> None:
		pending = {skill_id: self._launches_of(self.templates[skill_id]) for skill_id in sorted(skill_ids - self.edges.keys())}
		missing: dict[int, str] = {}
		for skill_id, launches in pending.items():
			for launch in launches:
				if launch["skillId"] not in self.templates:
					where = f"<{launch['effect']['tag']}>" if launch["effect"] else "penalty_skill_id"
					missing.setdefault(launch["skillId"], f"skill {skill_id} {where} ({launch['kind']})")
		if missing:
			self._load(missing)
		for skill_id, launches in pending.items():
			edges = []
			for launch in launches:
				level = launch["level"] if launch["level"] is not None else self.templates[launch["skillId"]].attr_int("lvl")
				edges.append({"kind": launch["kind"], "effect": launch["effect"], "skillId": launch["skillId"], "level": level,
				              "chance": launch["chance"], "followed": launch["chance"] > 0, **launch["details"]})
			self.edges[skill_id] = edges

	def _load(self, missing: dict[int, str]) -> None:
		found = {}
		for element in self.data.stream("skill_data", "skill_template"):
			skill_id = java_int(element.get("skill_id"), "skill_template skill_id")
			if skill_id in missing:
				found[skill_id] = SkillTemplateInfo.of(element)  # a later template of the id replaces the earlier one
		absent = sorted(missing.keys() - found.keys())
		if absent:
			raise OracleError("; ".join(f"{missing[s]} launches skill {s}, which has no skill_template" for s in absent))
		self.templates.update(found)

	def _launches_of(self, info: SkillTemplateInfo) -> list[dict]:
		if len(info.subeffects) != len(info.effects):
			raise OracleError(f"skill {info.skill_id}: a SkillTemplateInfo without its <subeffect>s (build it with SkillTemplateInfo.of)")
		bearing = sum(1 for (tag, _, _, _), subeffects in zip(info.effects, info.subeffects) if subeffects and tag in self.rules.effect_classes)
		if bearing > 1:
			raise OracleError(f"skill {info.skill_id} has {bearing} effects with a <subeffect>: an Effect keeps one sub effect (Effect.setSubEffect, "
			                  "Effect.java:479-481), which each calculateSubEffect overwrites (EffectTemplate.java:431), and one <subconditions> abort "
			                  "flag for all of them (:409-412, Effect.java:1001-1003), so they are no independent launches; this oracle does not "
			                  "model them")
		launches = []
		for index, ((tag, attrs, _, others), subeffects) in enumerate(zip(info.effects, info.subeffects)):
			cls = self.rules.effect_classes.get(tag)
			if cls is None:
				continue  # JAXB drops an element @XmlElements does not name, and its <subeffect> with it
			what = f"skill {info.skill_id} <{tag}>"
			effect = {"index": index, "position": java_int(attrs.get("e"), f"{what} e", 0), "tag": tag, "class": cls}
			chain = self.rules.class_chain(cls)
			kind, refusal = self.launch.launcher(chain)
			if refusal:
				raise OracleError(f"{what}: {cls} launches a skill in a shape this oracle does not model ({refusal})")
			if kind:
				launches.extend(getattr(self, "_" + kind)(info, attrs, effect, what))
			if subeffects:
				launches.append(self._subeffect(chain, subeffects, others, effect, what))
		penalty = java_int(info.attrs.get("penalty_skill_id"), f"skill {info.skill_id} penalty_skill_id", 0)
		if penalty != 0:  # Skill.startPenaltySkill: `if (penaltySkill == 0) return;`
			launches.append(self._penalty(info, penalty))
		return launches

	def _penalty(self, info: SkillTemplateInfo, skill_id: int) -> dict:
		"""Skill.startPenaltySkill (Skill.java:478-489): with penalty_skill_send_msg the caster casts it on itself at level 1 through a
		PenaltySkill, which runs endCast in turn; without, it is applied from the first target on the caster at its lvl."""
		what = f"skill {info.skill_id} penalty_skill_id {skill_id}"
		if self.launch.penalty_error:
			raise OracleError(f"{what}: {self.launch.penalty_error}")
		send_msg = java_boolean(info.attrs.get("penalty_skill_send_msg"), self.launch.penalty_send_msg_default)
		return self._launch("penalty", None, skill_id, self.launch.penalty_level if send_msg else None, 100, trigger="endCast", sendMsg=send_msg,
		                    effector="caster" if send_msg else "firstTarget", runsEndCast=send_msg, gates=["Skill.blockedPenaltySkill"])

	@staticmethod
	def _target(attrs: dict[str, str], name: str, what: str) -> int:
		skill_id = java_int(attrs.get(name), f"{what} {name}", 0)
		if skill_id <= 0:
			raise OracleError(f"{what} has no {name}: a launch of skill {skill_id}, which has no template (this oracle does not model it)")
		return skill_id

	@staticmethod
	def _launch(kind: str, effect: dict | None, skill_id: int, level: int | None, chance: int, **details) -> dict:
		return {"kind": kind, "effect": effect, "skillId": skill_id, "level": level, "chance": chance, "details": details}

	def _subeffect(self, chain: list[str], subeffects: list[dict[str, str]], others: list[str], effect: dict, what: str) -> dict:
		if self.launch.subeffect_error:
			raise OracleError(f"{what} <subeffect>: {self.launch.subeffect_error}")
		if len(subeffects) > 1:
			raise OracleError(f"{what} has {len(subeffects)} <subeffect>s: EffectTemplate binds one (EffectTemplate.java:57-58), and which one JAXB "
			                  "keeps is not modelled")
		overriding = [cls for cls in chain if cls in self.launch.subeffect_overrides]
		if overriding:
			raise OracleError(f"{what} <subeffect>: {overriding[0]} overrides calculateSubEffect or startSubEffect, which this oracle does not model")
		attrs = subeffects[0]
		if java_boolean(attrs.get("addeffect")):
			raise OracleError(f"{what} <subeffect addeffect=\"true\">: calculateSubEffect launches it at Effect.getSignetBurstedCount(), the target's "
			                  "signet level (EffectTemplate.java:421-424), which this oracle does not model")
		skill_id = self._target(attrs, "skill_id", f"{what} <subeffect>")
		chance = java_int(attrs.get("chance"), f"{what} <subeffect> chance", self.launch.subeffect_chance)
		gates = [tag for tag in ("modifiers", "subconditions") if tag in others]
		gates += [f"{cls}.launchSubEffect" for cls in chain if cls in self.launch.launch_rolls]
		return self._launch("subeffect", effect, skill_id, self.launch.subeffect_level, chance, gates=gates)

	def _provoker(self, info, attrs, effect, what) -> list[dict]:
		defaults = self.launch.defaults["provoker"]
		provoke_target = attrs.get("provoke_target")
		if provoke_target not in PROVOKE_TARGETS:
			raise OracleError(f"{what}: provoke_target {provoke_target!r} is none of {PROVOKE_TARGETS}, the arms of ProvokerEffect.getProvokeTarget "
			                  "(ProvokerEffect.java:83-88); anything else throws, which this oracle does not model")
		hit_type = attrs.get("hittype", defaults["hittype"])
		return [self._launch("provoker", effect, self._target(attrs, "skill_id", what), None,
		                     java_int(attrs.get("hittypeprob2"), f"{what} hittypeprob2", defaults["hittypeprob2"]),
		                     observer="ATTACK" if hit_type in ("NMLATK", "BACKATK") else "ATTACKED", hitType=hit_type, provokeTarget=provoke_target,
		                     radius=java_int(attrs.get("radius"), f"{what} radius", defaults["radius"]))]

	def _delayedskill(self, info, attrs, effect, what) -> list[dict]:
		return [self._launch("delayedskill", effect, self._target(attrs, "skill_id", what), None, 100, trigger="endedByTime")]

	def _skilllauncher(self, info, attrs, effect, what) -> list[dict]:
		return [self._launch("skilllauncher", effect, self._target(attrs, "skill_id", what), None, 100, trigger="apply")]

	def _condskilllauncher(self, info, attrs, effect, what) -> list[dict]:
		return [self._launch("condskilllauncher", effect, self._target(attrs, "skill_id", what), None, 100, trigger="hpAtOrBelow",
		                     hpPercent=java_int(attrs.get("value"), f"{what} value", 0), permanent=info.attrs.get("activation") == "PASSIVE")]

	def _aura(self, info, attrs, effect, what) -> list[dict]:
		return [self._launch("aura", effect, self._target(attrs, "skill_id", what), None, 100, trigger="periodic")]

	def _carvesignet(self, info, attrs, effect, what) -> list[dict]:
		signet_id = self._target(attrs, "signet_id", what)
		signet = attrs.get("signet")
		if signet is None:
			raise OracleError(f"{what} has no signet: getAbnormalEffect(null) throws (CarveSignetEffect.java:38), which this oracle does not model")
		increment, cap, prob = self._carver(attrs, what)
		if increment < 1:
			raise OracleError(f"{what} signet_increment {increment}: below 1 the carved level has no bound, which this oracle does not model")
		own = carve_levels(increment, cap, carvable_levels([(increment, cap)]))
		levels = carve_levels(increment, cap, carvable_levels(self._signet_carvers().get(signet, []) + [(increment, cap)]))
		return [self._launch("carvesignet", effect, signet_id + level - 1, None, prob, signet=signet, carvedLevel=level, signetCap=cap,
		                     signetIncrement=increment, viaOtherCarvers=level not in own) for level in sorted(levels)]

	def _carver(self, attrs: dict[str, str], what: str) -> tuple[int, int, int]:
		defaults = self.launch.defaults["carvesignet"]
		return (java_int(attrs.get("signet_increment"), f"{what} signet_increment", defaults["signet_increment"]),
		        java_int(attrs.get("signet_cap"), f"{what} signet_cap", 0), java_int(attrs.get("prob"), f"{what} prob", defaults["prob"]))

	def _signet_carvers(self) -> dict[str, list[tuple[int, int]]]:
		"""signet stack -> (signet_increment, signet_cap) of every <carvesignet> in skill_data that can carve it (prob above 0), the last
		template of an id winning: any of them may have carved the target before."""
		if self._carvers is None:
			tags = {tag for tag, cls in self.rules.effect_classes.items() if cls == CARVE_SIGNET_EFFECT}
			by_skill: dict[int, list[tuple[str, int, int]]] = {}
			for element in self.data.stream("skill_data", "skill_template"):
				skill_id = java_int(element.get("skill_id"), "skill_template skill_id")
				container = element.find("effects")
				rows = []
				for child in container if container is not None else []:
					if child.tag in tags and child.get("signet") is not None:
						increment, cap, prob = self._carver(dict(child.attrib), f"skill {skill_id} <{child.tag}>")
						if prob > 0:
							rows.append((child.get("signet"), increment, cap))
				by_skill[skill_id] = rows
			self._carvers = {}
			for rows in by_skill.values():
				for signet, increment, cap in rows:
					self._carvers.setdefault(signet, []).append((increment, cap))
		return self._carvers


def _pairs(pairs: set[tuple[int, int]]) -> list[dict]:
	return [{"skillId": skill_id, "level": level} for skill_id, level in sorted(pairs)]


def _effect_classes(entries: dict[tuple[int, int], dict], direct: set[tuple[int, int]], closed: set[tuple[int, int]]) -> dict:
	"""The leaf effect classes of `closed` and their closure under `extends`, and what the launched skills add to those of `direct`."""
	def classes(pairs):
		effects = [effect for pair in pairs for effect in entries[pair]["effects"]]
		return {effect["class"] for effect in effects}, {cls for effect in effects for cls in effect["classChain"]}
	leaves, bases = classes(closed)
	direct_leaves, direct_bases = classes(direct)
	return {"leaves": sorted(leaves), "withBases": sorted(bases),
	        "addedByLaunches": {"leaves": sorted(leaves - direct_leaves), "withBases": sorted(bases - direct_bases)}}


def _runs_end_cast(sources: list[str], info: SkillTemplateInfo) -> bool:
	"""Whether a requested skill is used through Skill.useSkill and so runs Skill.endCast, where its penalty skill starts (Skill.java:646-648):
	an npc casts every row of its npc_skills list (CreatureController.useSkill, CreatureController.java:455-460; the post-spawn rows through
	useWithoutPropSkill, SpawnEventHandler.java:22), updateSoulSickness casts its skill (PlayerController.java:732), and the character casts an
	autolearnt or --skill skill unless it is PASSIVE: CM_CASTSPELL.java:92 refuses a passive, which SkillLearnService.java:35-36 and
	PlayerEnterWorldService.java:410-411 apply directly instead."""
	if any(source.startswith("npc:") or source == "soulSickness" for source in sources):
		return True
	return info.attrs.get("activation") != "PASSIVE"


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

	# the skills the requested ones launch, to a fixpoint; `templates` (the reporter's dict) grows with them
	direct = set(requested)
	cast = {pair for pair, sources in requested.items() if _runs_end_cast(sources, templates[pair[0]])}
	closure = LaunchClosure(data, rules, templates)
	states = closure.states(direct, cast)
	reached = {pair for pair, _ in states}
	incoming = closure.launched_by(states)
	for pair in incoming:
		requested.setdefault(pair, []).append("launch")

	by_pair: dict[tuple[int, int], dict] = {}
	for (skill_id, skill_level), sources in sorted(requested.items()):
		entry = reporter.entry(skill_id, skill_level, sources)
		entry["launches"] = closure.edges[skill_id]
		entry["launchedBy"] = incoming.get((skill_id, skill_level), [])
		by_pair[(skill_id, skill_level)] = entry
	learned_pairs = set(learned.items())
	learned_closure = closure.close(learned_pairs, {pair for pair in learned_pairs if _runs_end_cast(["autolearn"], templates[pair[0]])})
	for npc in npcs:
		roots = {(row["skillId"], row["level"]) for row in npc["skills"]}
		npc_closure = closure.close(roots, roots)  # the npc casts every row (_runs_end_cast)
		npc["launchedSkills"] = _pairs(npc_closure - roots)
		npc["effectClasses"] = _effect_classes(by_pair, roots, npc_closure)
	return {
		"format": "aion-m5b2-skills",
		"version": 2,
		"race": race,
		"playerClass": player_class,
		"level": level,
		"character": {
			"skills": [{"skillId": skill_id, "level": skill_level} for skill_id, skill_level in sorted(learned.items())],
			"passives": passives,
			"equippedItems": equipped,
			"castingTimeSources": casting_sources,
			"skillCostSources": cost_sources,
			"launchedSkills": _pairs(learned_closure - learned_pairs),
			"effectClasses": _effect_classes(by_pair, learned_pairs, learned_closure),
		},
		"soulSickness": {"skillId": rules.soul_sickness_skill, "deathCount": death_count, "maxDeathCount": rules.soul_sickness_max_death_count},
		"npcs": npcs,
		"skills": list(by_pair.values()),
		"effectClasses": _effect_classes(by_pair, direct, reached),
	}
