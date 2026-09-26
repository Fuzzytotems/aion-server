"""M5b-2 skills oracle (m5b2/skills.py, m5b2-plan.md G-01): the formulas alone, the Java tables as they stand today, the whole report on a small
static_data tree, and the four skill ids the M5b-2 gate asserts on the real data (2864 Ferocious Strike, 1282 Flame Bolt, 1328 Root and the
revive debuff 8291), plus the class-less autolearn rows (243 Return, 245 Bandage Heal, 302 Escape) that the plan's first inventory missed.

Expected values are derived by hand from the Java sources named in m5b2/skills.py and repeated per case. Everything that reads the Java source
tree is skipped without it, like M5bFixtureReportTest.
"""

import contextlib
import io
import json
import shutil
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

from m5a.creation import JavaEnums, creation_report, learn_new_skills
from m5a.data import StaticData
from m5a.javafloat import f32
from m5b2.skills import (JavaSkillRules, LaunchClosure, _condition_cost, carvable_levels, carve_levels, effect_duration, effects_duration,
                         load_skill_templates, magical_cast_duration_without_functions, npc_cast_duration, npc_skill_lists, skills_report)
from staticdata_oracle import OracleError
from staticdata_oracle import run as runner

import oracle

from .support import Tree

JAVA_SRC = runner.TOOL_DIR.parents[2] / "game-server" / "src"
HAVE_JAVA_TREE = (JAVA_SRC / "com" / "aionemu" / "gameserver").is_dir() and runner.DEFAULT_STATIC_DATA.is_dir()


def effect(position, duration1=0, duration2=0, random_time=0, over_time=False):
	"""One effects entry as SkillReporter.effects_of writes it; `over_time` is a class below AbstractOverTimeEffect (getDuration2 adds 1000)."""
	return {"position": position, "duration1": duration1, "duration2": duration2, "effectiveDuration2": duration2 + (1000 if over_time else 0),
	        "randomTime": random_time}


class M5b2FormulaTest(unittest.TestCase):
	"""The arithmetic of the report, without any Java source or static data."""

	def test_magical_cast_duration_without_functions_is_the_base(self):
		# Skill.calculateMagicalCastDuration with every getPositiveReverseStat answering max(0, value): max(base, cap) - 0, capped again
		self.assertEqual(magical_cast_duration_without_functions(2000, "ATTACK", f32(0.25)), 2000, "Flame Bolt: the template value")
		self.assertEqual(magical_cast_duration_without_functions(2000, "HEAL", f32(0.25)), 2000)
		self.assertEqual(magical_cast_duration_without_functions(0, "DEBUFF", f32(0.25)), 0, "an instant skill stays instant")
		self.assertEqual(magical_cast_duration_without_functions(1, "SUMMON", f32(0.25)), 1)
		# a cap factor above 1 (not Java's, but it pins that the cap is applied at all): max(base, round(base * 2f)) twice
		self.assertEqual(magical_cast_duration_without_functions(1000, "ATTACK", f32(2.0)), 2000, "the cap of the first max()")
		self.assertEqual(magical_cast_duration_without_functions(1000, "SUMMON", f32(2.0)), 2000, "a summon skips only the second cap")

	def test_npc_cast_duration_rounds_the_float_product(self):
		# Skill.java:340: Math.round(baseCastDuration * (castSpeed / 1000f))
		self.assertEqual(npc_cast_duration(2500, 1000), 2500, "16419 Brandish of npc 210133, cast_speed default 1000")
		self.assertEqual(npc_cast_duration(2500, 1100), 2750)
		self.assertEqual(npc_cast_duration(5, 1100), 6, "5 * 1.1f is 5.5f, and Math.round rounds half up")
		self.assertEqual(npc_cast_duration(5, 900), 5, "5 * 0.9f is 4.5f: half up again")
		self.assertEqual(npc_cast_duration(0, 1500), 0)
		self.assertEqual(npc_cast_duration(45, 1300), 58, "1.3f is below 1.3, so 45 * 1.3f is 58.499998f: float, not the exact 58.5")

	def test_effect_duration_takes_the_first_positive_position(self):
		# Effect.calculateTemplateDuration: duration2 + duration1 * skillLevel of the first success template (ascending position) that is > 0
		self.assertEqual(effect_duration([effect(1, duration2=20000)], 1), (20000, 0), "1328 Root")
		self.assertEqual(effect_duration([effect(1, 20000, 40000)] * 3, 1), (60000, 0), "8291 at deathCount 1: 40000 + 20000 * 1")
		self.assertEqual(effect_duration([effect(1, 20000, 40000)], 3), (100000, 0), "... and at deathCount 3")
		self.assertEqual(effect_duration([effect(2, duration2=50000), effect(1, duration2=5000)], 1), (5000, 0),
		                 "successEffects is keyed by position, so position 1 comes first whatever the document order")
		self.assertEqual(effect_duration([effect(1), effect(2, duration2=7000, random_time=500)], 1), (7000, 500),
		                 "a template of duration 0 is skipped, and the randomtime is reported, not rolled")
		self.assertEqual(effect_duration([effect(1, duration2=100), effect(1, duration2=200)], 1), (200, 0),
		                 "a later template of the same position overwrites the earlier one in successEffects")
		self.assertEqual(effect_duration([effect(1, duration1=-10, duration2=5)], 1), (0, 0), "a non-positive sum is no duration")
		self.assertEqual(effect_duration([], 1), (0, 0))
		self.assertIsNone(effect_duration([effect(16, duration2=1)], 1), "outside the 16 bins the ConcurrentHashMap order is not modelled")

	def test_effect_duration_reads_the_virtual_get_duration2(self):
		# Effect.java:902 calls et.getDuration2(), which AbstractOverTimeEffect.java:64-67 overrides with `duration2 + 1000`
		self.assertEqual(effect_duration([effect(1), effect(2, duration2=15000, over_time=True)], 1), (16000, 0),
		                 "1447 Erosion's <spellatk duration2=15000>: a damage over time lasts one second more than its attribute")
		self.assertEqual(effect_duration([effect(1), effect(2, duration1=300, duration2=12100, over_time=True)], 1), (13400, 0),
		                 "17018 Bite's <bleed duration2=12100 duration1=300>: 12100 + 1000 + 300 * 1")
		self.assertEqual(effect_duration([effect(1, over_time=True), effect(2, duration2=20000)], 1), (1000, 0),
		                 "the extra second is part of getDuration2(), so it counts before the `> 0` test: an over time template of duration2 0 "
		                 "is positive and decides, however long the later template is")
		self.assertEqual(effect_duration([effect(1, duration1=-1000, over_time=True), effect(2, duration2=20000)], 1), (20000, 0),
		                 "... and 1000 - 1000 * level 1 is 0, which is not positive: the next position decides")

	def test_effect_duration_is_a_long_that_calculate_effects_duration_clamps(self):
		# Effect.java:902 `et.getDuration2() + ((long) et.getDuration1()) * getSkillLevel()`: a long, "some event skills would produce an int
		# overflow" - the xpboost templates of skill_templates.xml (duration2 1184000000, duration1 2000000000)
		xpboost = [effect(1, duration1=2000000000, duration2=1184000000), effect(2, duration2=5000)]
		self.assertEqual(effect_duration(xpboost, 1), (3184000000, 0), "in int arithmetic -1110967296, not positive, and position 2 would decide")
		self.assertEqual(effect_duration(xpboost, 2), (5184000000, 0), "in int arithmetic 889032704")
		# Effect.java:896 `(int) Math.min(Integer.MAX_VALUE, duration)`, after calculateTemplateDuration subtracted the roll
		self.assertEqual(effects_duration((5184000000, 0)), (2**31 - 1, 0))
		self.assertEqual(effects_duration((2**31 - 1, 0)), (2**31 - 1, 0), "Integer.MAX_VALUE itself is not clamped")
		self.assertEqual(effects_duration((20000, 300)), (20000, 300), "below the clamp the pair is calculateTemplateDuration's")
		self.assertEqual(effects_duration((2**31 + 499, 500)), (2**31 - 1, 0), "every roll stays above the clamp: no random part is left")
		self.assertIsNone(effects_duration((2**31 + 499, 501)), "a roll of 501 would end 1 ms below Integer.MAX_VALUE: the roll decides")

	def test_condition_costs(self):
		# MpCondition.getCost / HpCondition.getCost: value + delta * skillLevel; a ratio is a percentage of the max, which is not static data
		self.assertEqual(_condition_cost("mp", {"value": "19", "delta": "0"}, 1, 1282), {"value": 19, "delta": 0, "ratio": False, "cost": 19})
		self.assertEqual(_condition_cost("mp", {"value": "10", "delta": "3"}, 4, 1), {"value": 10, "delta": 3, "ratio": False, "cost": 22})
		self.assertEqual(_condition_cost("hp", {"value": "5"}, 2, 1), {"value": 5, "delta": 0, "ratio": False, "cost": 5})
		self.assertEqual(_condition_cost("mp", {"value": "10", "ratio": "true"}, 1, 1)["cost"], None)
		self.assertEqual(_condition_cost("dp", {"value": "7"}, 1, 1), {"value": 7}, "DpCondition only compares")
		with self.assertRaises(OracleError):
			_condition_cost("mp", {}, 1, 1)  # value is required

	def test_carve_levels_follow_next_signet_level(self):
		# CarveSignetEffect.java:37-41: signetIncrement without the stack, min(carved + signetIncrement, max(signetCap, carved)) with it
		self.assertEqual(carve_levels(1, 3, set()), {1}, "a first carve, or a signet no carve applied (carved 0): the increment")
		self.assertEqual(carve_levels(1, 3, {1, 2, 3}), {1, 2, 3}, "min(3 + 1, max(3, 3)) is 3: the cap holds")
		self.assertEqual(carve_levels(1, 3, {5}), {1, 5}, "a target carved to 5 by a deeper carver keeps 5: min(5 + 1, max(3, 5))")
		self.assertEqual(carve_levels(2, 5, {1}), {2, 3}, "min(1 + 2, max(5, 1)) is 3")
		self.assertEqual(carve_levels(5, 3, set()), {3, 5}, "an increment above the cap: 5 on a fresh target, min(0 + 5, max(3, 0)) = 3 on an "
		                                                    "uncarved signet")
		# the levels a stack can carry: the fixpoint over every carver
		self.assertEqual(carvable_levels([(1, 3)]), {1, 2, 3}, "1, then 2, then 3, then the cap")
		self.assertEqual(carvable_levels([(2, 5)]), {2, 4, 5}, "2, 4, then min(6, 5): 1 and 3 are never carved by this carver alone")
		self.assertEqual(carvable_levels([(1, 3), (2, 5)]), {1, 2, 3, 4, 5}, "together every level: 1 + 2 is 3, and 3 + 2 is 5")
		self.assertEqual(carvable_levels([]), set())
		with self.assertRaises(OracleError):
			carvable_levels([(0, 3)])  # an increment below 1 never ends (or runs below 1): not modelled


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5b2JavaRulesTest(unittest.TestCase):
	"""The tables the report reads from the Java sources, as they stand today."""

	@classmethod
	def setUpClass(cls):
		cls.rules = JavaSkillRules.read(JAVA_SRC)

	def test_effects_java_binds_170_tags(self):
		self.assertEqual(len(self.rules.effect_classes), 170, "m5b2-plan.md §2.3: Effects.java maps 170 XML element names")
		self.assertEqual(self.rules.effect_classes["skillatk"], "SkillAttackInstantEffect")
		self.assertEqual(self.rules.effect_classes["spellatkinstant"], "SpellAttackInstantEffect")
		self.assertEqual(self.rules.effect_classes["root"], "RootEffect")
		self.assertEqual(self.rules.effect_classes["statdown"], "StatdownEffect")
		self.assertEqual(self.rules.effect_classes["return"], "ReturnEffect")
		self.assertEqual(self.rules.effect_classes["escape"], "EscapeEffect")
		self.assertEqual(self.rules.effect_classes["healinstant"], "HealInstantEffect")

	def test_extends_chains(self):
		self.assertEqual(self.rules.class_chain("SpellAttackInstantEffect"), ["SpellAttackInstantEffect", "DamageEffect", "EffectTemplate"])
		self.assertEqual(self.rules.class_chain("StatdownEffect"), ["StatdownEffect", "BufEffect", "EffectTemplate"])
		self.assertEqual(self.rules.class_chain("HealInstantEffect"), ["HealInstantEffect", "AbstractHealEffect", "EffectTemplate"])
		self.assertEqual(self.rules.class_chain("RootEffect"), ["RootEffect", "EffectTemplate"])
		self.assertEqual(self.rules.class_chain("EffectTemplate"), ["EffectTemplate"])
		with self.assertRaises(OracleError):
			self.rules.class_chain("NoSuchEffect")

	def test_get_duration2_overrides(self):
		# EffectTemplate.java:116-118 `return duration2;` and the one override, AbstractOverTimeEffect.java:64-67 `return duration2 + 1000;`
		self.assertEqual(self.rules.duration2_bonuses, {"EffectTemplate": 0, "AbstractOverTimeEffect": 1000})
		for over_time in ("SpellAttackEffect", "BleedEffect", "PoisonEffect", "HealOverTimeEffect", "AbstractOverTimeEffect"):
			self.assertEqual(self.rules.duration2_bonus(over_time), 1000, f"{over_time} inherits the override")
		self.assertEqual(self.rules.class_chain("HealEffect"), ["HealEffect", "HealOverTimeEffect", "AbstractOverTimeEffect", "EffectTemplate"])
		self.assertEqual(self.rules.duration2_bonus("HealEffect"), 1000, "two levels below the override (the post-spawn <heal> of plan §2.4(c))")
		for plain in ("RootEffect", "StatdownEffect", "AlwaysDodgeEffect", "SpellAttackInstantEffect", "HealInstantEffect", "EffectTemplate"):
			self.assertEqual(self.rules.duration2_bonus(plain), 0, f"{plain} answers the attribute")
		with self.assertRaises(OracleError):
			self.rules.duration2_bonus("NoSuchEffect")

	def test_target_slots_soul_sickness_and_the_cast_cap(self):
		# SkillTargetSlot(id) in declaration order: the ordinal is what SM_ABNORMAL_STATE writes, the id is the slot mask
		self.assertEqual(self.rules.target_slots, {"BUFF": (0, 1), "DEBUFF": (1, 2), "CHANT": (2, 4), "SPEC": (3, 8), "SPEC2": (4, 16),
		                                           "BOOST": (5, 32), "NOSHOW": (6, 64), "NONE": (7, 128)})
		self.assertEqual((self.rules.soul_sickness_skill, self.rules.soul_sickness_max_death_count), (8291, 10))
		self.assertEqual(self.rules.cast_duration_cap, f32(0.25))

	def test_the_launchers_the_java_declares(self):
		launch = self.rules.launch
		# every effect class whose own source launches a skill (m5e-plan.md §2.4 lesson 2): the six modelled ones, the revive family (the
		# skill_id is cast on revive), the pet order (PET_SKILL_DATA), the summons (npc_id; SummonSkillAreaEffect also makes its servant use one)
		self.assertEqual(sorted(launch.evidence), ["AuraEffect", "CarveSignetEffect", "CondSkillLauncherEffect", "DelayedSkillEffect",
		                                           "PetOrderUseUltraSkillEffect", "ProvokerEffect", "RebirthEffect", "ResurrectBaseEffect",
		                                           "ResurrectEffect", "SkillLauncherEffect", "SummonEffect", "SummonSkillAreaEffect"])
		self.assertEqual(launch.kinds, {"ProvokerEffect": "provoker", "DelayedSkillEffect": "delayedskill", "CarveSignetEffect": "carvesignet",
		                                "SkillLauncherEffect": "skilllauncher", "CondSkillLauncherEffect": "condskilllauncher", "AuraEffect": "aura"})
		self.assertEqual(launch.unmodelled, {}, "every modelled class still has the shape the closure models")
		# EffectTemplate.java:52-54, ShieldEffect.java:26, CarveSignetEffect.java:20, :28; calculateSubEffect `int level = 1;` (:419);
		# SubEffect.java:18 `chance = 100`
		self.assertEqual(launch.defaults["provoker"], {"hittype": "EVERYHIT", "hittypeprob2": 100, "radius": 0})
		self.assertEqual(launch.defaults["carvesignet"], {"signet_increment": 1, "prob": 100})
		self.assertEqual((launch.subeffect_error, launch.subeffect_level, launch.subeffect_chance, launch.template_error), (None, 1, 100, None))
		self.assertEqual((launch.subeffect_overrides, launch.launch_rolls), (frozenset(), frozenset({"SignetBurstEffect"})),
		                 "nothing overrides calculateSubEffect; SignetBurstEffect.java:44 rolls launchSubEffect from the signet data")
		# Skill.startPenaltySkill (Skill.java:478-489): getPenaltySkill(effector, penaltySkill, 1) with the message; SkillTemplate.java:84-85
		# `penaltySkillSendMsg = false`
		self.assertEqual((launch.penalty_error, launch.penalty_level, launch.penalty_send_msg_default), (None, 1, False))
		launcher = lambda cls: launch.launcher(self.rules.class_chain(cls))  # noqa: E731
		self.assertEqual(launcher("ProvokerEffect"), ("provoker", None))
		self.assertEqual(launcher("RootEffect"), (None, None))
		self.assertEqual(launcher("SignetBurstEffect"), (None, None), "a burst launches only through its <subeffect>")
		self.assertIn("ResurrectEffect binds the attribute skill_id", launcher("ResurrectPositionalEffect")[1], "inherited from ResurrectEffect")
		self.assertIn("SummonEffect binds the attribute npc_id", launcher("SummonTotemEffect")[1], "two classes up the chain")
		self.assertIn("PET_SKILL_DATA", launcher("PetOrderUseUltraSkillEffect")[1])


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5b2JavaRulesShapeTest(unittest.TestCase):
	"""JavaSkillRules.read on a copy of the Java files it reads, one of them changed: the duration getters are read, not assumed."""

	BASE = ("com", "aionemu", "gameserver")

	def rules_with(self, relative: str, old: str, new: str) -> JavaSkillRules:
		with tempfile.TemporaryDirectory() as root:
			source, target = JAVA_SRC.joinpath(*self.BASE), Path(root).joinpath(*self.BASE)
			for part in ("skillengine/effect", "skillengine/model", "controllers"):
				(target / part).mkdir(parents=True)
			for path in [*(source / "skillengine" / "effect").glob("*.java"), source / "skillengine" / "model" / "Skill.java",
			             source / "skillengine" / "model" / "SkillTargetSlot.java", source / "controllers" / "PlayerController.java",
			             source / "skillengine" / "SkillEngine.java", source / "skillengine" / "model" / "SkillTemplate.java",
			             source / "skillengine" / "model" / "PenaltySkill.java"]:
				shutil.copyfile(path, target / path.relative_to(source))
			changed = target / relative
			text = changed.read_text(encoding="utf-8")
			self.assertIn(old, text, f"{relative} no longer contains the text this case changes")
			changed.write_text(text.replace(old, new, 1), encoding="utf-8")
			return JavaSkillRules.read(Path(root))

	def test_the_constant_is_read_from_the_override(self):
		rules = self.rules_with("skillengine/effect/AbstractOverTimeEffect.java", "return duration2 + 1000;", "return duration2 + 1500;")
		self.assertEqual(rules.duration2_bonus("BleedEffect"), 1500)
		self.assertEqual(rules.duration2_bonus("RootEffect"), 0)

	def test_a_getter_the_oracle_does_not_model_is_refused(self):
		cases = [
			("skillengine/effect/AbstractOverTimeEffect.java", "return duration2 + 1000;", "return duration2 * 2;"),
			("skillengine/effect/EffectTemplate.java", "return duration2;", "return duration2 + 1;"),
			("skillengine/effect/RootEffect.java", "public void applyEffect(", "public int getDuration1() { return 5; }\n\tpublic void applyEffect("),
			("skillengine/effect/BleedEffect.java", "public void calculate(", "public int getRandomTime() { return 5; }\n\tpublic void calculate("),
			# a body with braces of its own: the getter must be found by its head and refused, not skipped as if there were no override (which
			# would leave the over time classes at EffectTemplate's `return duration2;` without a word)
			("skillengine/effect/AbstractOverTimeEffect.java", "return duration2 + 1000;",
			 "if (duration2 > 0) {\n\t\t\treturn duration2 + 1000;\n\t\t}\n\t\treturn duration2;"),
			("skillengine/effect/RootEffect.java", "public void applyEffect(",
			 "public int getDuration2() {\n\t\t{ return duration2; }\n\t}\n\tpublic void applyEffect("),
		]
		for relative, old, new in cases:
			with self.subTest(file=relative, new=new), self.assertRaises(OracleError):
				self.rules_with(relative, old, new)

	def test_the_launch_shapes_are_read(self):
		# the constants of the closure come from the Java: SubEffect's default chance and calculateSubEffect's level
		rules = self.rules_with("skillengine/effect/SubEffect.java", "private int chance = 100;", "private int chance = 40;")
		self.assertEqual(rules.launch.subeffect_chance, 40)
		rules = self.rules_with("skillengine/effect/EffectTemplate.java", "int level = 1;", "int level = 2;")
		self.assertEqual(rules.launch.subeffect_level, 2)
		rules = self.rules_with("skillengine/effect/CarveSignetEffect.java", "protected int prob = 100;", "protected int prob = 90;")
		self.assertEqual(rules.launch.defaults["carvesignet"]["prob"], 90)
		# a modelled launcher whose Java changed is no longer modelled, and says why; the other launchers stay
		rules = self.rules_with("skillengine/effect/DelayedSkillEffect.java", "if (effect.isEndedByTime())", "if (true)")
		self.assertNotIn("DelayedSkillEffect", rules.launch.kinds)
		self.assertIn("DelayedSkillEffect.java does not have the launch shape", rules.launch.unmodelled["DelayedSkillEffect"])
		self.assertEqual(rules.launch.launcher(rules.class_chain("DelayedSkillEffect"))[0], None)
		self.assertIn("ProvokerEffect", rules.launch.kinds)
		# a SkillEngine entry point that stops applying the template's lvl unmodels every launcher calling it
		rules = self.rules_with("skillengine/SkillEngine.java", "applyEffect(effector, effected, skillTemplate, skillTemplate.getLvl(), null, null)",
		                        "applyEffect(effector, effected, skillTemplate, 1, null, null)")
		self.assertEqual(sorted(rules.launch.unmodelled), ["AuraEffect", "CarveSignetEffect", "SkillLauncherEffect"])
		# a new launcher is found by what its source does; a subeffect override or a launch in EffectTemplate itself is recorded
		rules = self.rules_with("skillengine/effect/RootEffect.java", "public void applyEffect(",
		                        "public void calculateSubEffect(Effect effect) {\n\t\tSkillEngine.getInstance().applyEffect(1, null, null);\n\t}\n\t"
		                        "public void applyEffect(")
		self.assertEqual(rules.launch.evidence["RootEffect"], "calls SkillEngine.applyEffect")
		self.assertEqual(rules.launch.subeffect_overrides, frozenset({"RootEffect"}))
		rules = self.rules_with("skillengine/effect/EffectTemplate.java", "public void startSubEffect(Effect effect) {",
		                        "public void startSubEffect(Effect effect) {\n\t\tSkillEngine.getInstance().applyEffect(1, null, null);")
		self.assertIn("outside calculateSubEffect", rules.launch.template_error)
		rules = self.rules_with("skillengine/effect/SubEffect.java", "private int chance = 100;", "private int chance;")
		self.assertIn("SubEffect.java has no `private int chance = N;`", rules.launch.subeffect_error)
		# a modelled launcher that gains a second launch is no longer modelled: the closure would follow the first one only
		rules = self.rules_with("skillengine/effect/AuraEffect.java", "SkillEngine.getInstance().applyEffect(skillId, effected, effected);",
		                        "SkillEngine.getInstance().applyEffect(skillId, effected, effected);\n\t\t"
		                        "SkillEngine.getInstance().applyEffect(skillId + 1, effected, effected);")
		self.assertIn("AuraEffect.java launches in more ways than this oracle models", rules.launch.unmodelled["AuraEffect"])
		self.assertEqual(rules.launch.launcher(rules.class_chain("AuraEffect"))[0], None)

	def test_the_penalty_shape_is_read(self):
		# the level of the message arm and the JAXB default of penalty_skill_send_msg come from the Java
		rules = self.rules_with("skillengine/model/Skill.java", "getPenaltySkill(effector, penaltySkill, 1)", "getPenaltySkill(effector, penaltySkill, 2)")
		self.assertEqual((rules.launch.penalty_error, rules.launch.penalty_level), (None, 2))
		rules = self.rules_with("skillengine/model/SkillTemplate.java", "private boolean penaltySkillSendMsg = false;",
		                        "private boolean penaltySkillSendMsg = true;")
		self.assertEqual((rules.launch.penalty_error, rules.launch.penalty_send_msg_default), (None, True))
		# any other shape of the launch, its gate, the PenaltySkill cast or the attribute is not modelled, and says why
		cases = [
			("skillengine/model/Skill.java", "if (!blockedPenaltySkill)\n\t\t\tstartPenaltySkill();", "startPenaltySkill();"),
			("skillengine/model/Skill.java", "if (setCooldowns)\n\t\t\tsetCooldowns();", "if (setCooldowns)\n\t\t\tsetCooldowns();\n\t\tstartPenaltySkill();"),
			("skillengine/model/Skill.java", "blockedPenaltySkill = true;", "blockedChain = true;"),
			("skillengine/model/Skill.java", "SkillEngine.getInstance().applyEffectDirectly(penaltySkill, firstTarget, effector);",
			 "SkillEngine.getInstance().applyEffectDirectly(penaltySkill, effector, firstTarget);"),
			("skillengine/model/Skill.java", "if (penaltySkill == 0)\n\t\t\treturn;",
			 "if (penaltySkill == 0)\n\t\t\treturn;\n\t\tSkillEngine.getInstance().applyEffect(penaltySkill + 1, effector, effector);"),
			("skillengine/model/PenaltySkill.java", "super.useWithoutPropSkill();", "super.useNoAnimationSkill();"),
			("skillengine/model/PenaltySkill.java", "super(skillTemplate, effector, skillLevel, effector, null);",
			 "super(skillTemplate, effector, 1, effector, null);"),
			("skillengine/SkillEngine.java", "return new PenaltySkill(template, effector, skillLevel);", "return new PenaltySkill(template, effector, 1);"),
			("skillengine/model/SkillTemplate.java", '@XmlAttribute(name = "penalty_skill_id")', '@XmlAttribute(name = "penalty_skill")'),
		]
		for relative, old, new in cases:
			with self.subTest(file=relative, new=new):
				self.assertIsNotNone(self.rules_with(relative, old, new).launch.penalty_error)
		# applyEffectDirectly(int, Creature, Creature) at another level unmodels the provoker and the penalty
		rules = self.rules_with("skillengine/SkillEngine.java", "applyEffect(effector, effected, skillTemplate, skillTemplate.getLvl(), null, ForceType.DEFAULT)",
		                        "applyEffect(effector, effected, skillTemplate, 1, null, ForceType.DEFAULT)")
		self.assertEqual(sorted(rules.launch.unmodelled), ["ProvokerEffect"])
		self.assertIn("applyEffectDirectly(int, Creature, Creature)", rules.launch.penalty_error)
		# and a penalty the Java does not have the modelled shape for is refused when the closure reaches it
		with fixture_tree() as tree:
			data = StaticData(tree.root)
			closure = LaunchClosure(data, rules, load_skill_templates(data, {1090, 1001}))
			self.assertEqual(closure.close({(1001, 1)}, {(1001, 1)}), {(1001, 1)}, "a cast without a penalty skill does not need the shape")
			with self.assertRaises(OracleError) as caught:
				closure.close({(1090, 1)}, {(1090, 1)})
			self.assertIn("skill 1090 penalty_skill_id 1091", str(caught.exception))


SKILL_TREE = (
	'<skill skillId="1001" minLevel="1" classId="WARRIOR" autolearn="true"/>'
	'<skill skillId="1002" minLevel="1" autolearn="true"/>'                                   # no classId: every class
	'<skill skillId="1003" minLevel="1" classId="WARRIOR" race="ASMODIANS" autolearn="true"/>'  # the other race
	'<skill skillId="1004" minLevel="1" classId="WARRIOR" race="ELYOS" autolearn="true"/>'
	'<skill skillId="1005" minLevel="1" classId="WARRIOR"/>'                                  # not autolearn
	'<skill skillId="1006" minLevel="2" classId="WARRIOR" autolearn="true"/>'
	'<skill skillId="1007" minLevel="1" classId="MAGE" autolearn="true"/>'
	'<skill skillId="1008" minLevel="1" classId="WARRIOR" autolearn="true"/>'
	'<skill skillId="30001" minLevel="1" autolearn="true"/>'
	'<skill skillId="1013" minLevel="1" race="ELYOS" autolearn="true"/>'                     # class-less and race specific
	'<skill skillId="1014" minLevel="1" race="ASMODIANS" autolearn="true"/>'
	'<skill skillId="1009" minLevel="1" classId="GLADIATOR" autolearn="true"/>'
	'<skill skillId="1034" minLevel="3" classId="WARRIOR" autolearn="true"/>'                  # a launcher on the character's own bar
	'<skill skillId="1092" minLevel="4" classId="WARRIOR" autolearn="true"/>'                  # a penalty skill with the message
	'<skill skillId="1096" minLevel="4" classId="WARRIOR" autolearn="true"/>'                  # a passive with a penalty skill
)

SKILLS = """
<skill_template skill_id="1001" name="fixture bolt" stack="A" lvl="1" skilltype="MAGICAL" skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE"
	duration="2000" cooldown="50" cooldown_delta_lv="2" cooldownId="11" apply_casting_time_bonus="true">
	<properties first_target="TARGET" first_target_range="25"/>
	<startconditions><dp value="7"/><chain category="F_CHAIN"/></startconditions>
	<endconditions><mp value="10" delta="3"/><hp value="5"/></endconditions>
	<effects><spellatkinstant value="100" e="1" element="FIRE"/></effects>
</skill_template>
<skill_template skill_id="1002" name="fixture everyone" stack="B" lvl="1" skillsubtype="NONE" activation="ACTIVE" duration="1000">
	<effects><return e="1" noresist="true"/></effects>
</skill_template>
<skill_template skill_id="1003" name="fixture asmodian" stack="C" skillsubtype="NONE" activation="ACTIVE" duration="0"/>
<skill_template skill_id="1004" name="fixture debuff" stack="D" lvl="1" skillsubtype="DEBUFF" tslot="DEBUFF" dispel_category="ALL"
	req_dispel_level="1" activation="ACTIVE" duration="0" cooldown="600">
	<effects>
		<statdown duration2="50000" e="2"><change stat="MAXHP" func="PERCENT" value="-30"/></statdown>
		<root duration2="20000" duration1="1000" e="1" randomtime="300" preeffect="2 3"/>
	</effects>
</skill_template>
<skill_template skill_id="1005" name="fixture learnt by hand" stack="E" skillsubtype="NONE" activation="ACTIVE" duration="0"/>
<skill_template skill_id="1006" name="fixture level two" stack="F" lvl="2" skillsubtype="NONE" activation="ACTIVE" duration="0"/>
<skill_template skill_id="1007" name="fixture mage" stack="G" skillsubtype="NONE" activation="ACTIVE" duration="0"/>
<skill_template skill_id="1008" name="fixture passive" stack="H" lvl="1" skillsubtype="NONE" tslot="NOSHOW" activation="PASSIVE" duration="0">
	<effects><statboost e="1"><change stat="PHYSICAL_ATTACK" func="ADD" value="7"/></statboost></effects>
</skill_template>
<skill_template skill_id="1009" name="fixture gladiator" stack="I" skillsubtype="NONE" activation="ACTIVE" duration="0"/>
<skill_template skill_id="1010" name="fixture charge" stack="J" skillsubtype="ATTACK" activation="CHARGE" duration="3000"
	apply_casting_time_bonus="true"/>
<skill_template skill_id="1011" name="fixture unknown tag" stack="K" lvl="4" skillsubtype="BUFF" tslot="BUFF" activation="ACTIVE" duration="0">
	<effects><nosucheffect e="1"/><alwaysdodge duration2="5000" e="2"/></effects>
</skill_template>
<skill_template skill_id="1012" name="fixture ratio" stack="L" skillsubtype="NONE" activation="ACTIVE" duration="0">
	<startconditions><mp value="4"/></startconditions>
	<endconditions><mp value="10" ratio="true"/></endconditions>
</skill_template>
<skill_template skill_id="1015" name="fixture bleed first" stack="R" lvl="1" skillsubtype="DEBUFF" tslot="DEBUFF" activation="ACTIVE" duration="0">
	<effects><bleed checktime="3000" value="5" e="1"/><root duration2="20000" e="2"/></effects>
</skill_template>
<skill_template skill_id="1017" name="fixture int overflow" stack="U" lvl="1" skillsubtype="DEBUFF" tslot="DEBUFF" activation="ACTIVE" duration="0">
	<effects><bleed checktime="3000" value="5" duration2="2147483000" e="1"/><root duration2="5000" e="2"/></effects>
</skill_template>
<skill_template skill_id="1016" name="fixture heal over time" stack="T" lvl="1" skillsubtype="HEAL" tslot="BUFF" activation="ACTIVE" duration="0">
	<effects><heal checktime="1000" value="10" duration2="2000" duration1="100" e="1"/></effects>
</skill_template>
<skill_template skill_id="1018" name="fixture event boost" stack="V" lvl="1" skillsubtype="BUFF" tslot="BOOST" activation="ACTIVE" duration="0">
	<effects><xpboost duration2="1184000000" duration1="2000000000" e="1" basiclvl="2" noresist="true"/><root duration2="5000" e="2"/></effects>
</skill_template>
<skill_template skill_id="1013" name="fixture elyos everyone" stack="P" lvl="1" skillsubtype="NONE" activation="ACTIVE" duration="0"/>
<skill_template skill_id="1014" name="fixture asmodian everyone" stack="Q" lvl="1" skillsubtype="NONE" activation="ACTIVE" duration="0"/>
<skill_template skill_id="1020" name="fixture npc skill" stack="M" lvl="1" skillsubtype="ATTACK" activation="ACTIVE" duration="3000">
	<effects><skillatk value="27" e="1"/></effects>
</skill_template>
<skill_template skill_id="8291" name="fixture soul sickness" stack="N" lvl="1" skillsubtype="NONE" tslot="SPEC2" activation="PROVOKED" duration="0">
	<effects><statdown duration2="40000" duration1="20000" e="1"/><statdown duration2="40000" duration1="20000" e="2" preeffect="1"/></effects>
</skill_template>
<skill_template skill_id="30001" name="fixture collection" stack="O" lvl="1" skilltype="NONE" skillsubtype="NONE" tslot="NONE" activation="NONE"
	duration="0"/>
"""

# The launched-skill closure (m5b2/skills.py LaunchClosure): one fixture per launch kind, a two-step chain, cycles, and the shapes it refuses.
# The attributes are those of skill_templates.xml's own launchers (e.g. :117912's provoker, :17522's subeffect).
LAUNCH_SKILLS = """
<skill_template skill_id="1030" name="fixture subeffect" stack="LA" lvl="2" skillsubtype="ATTACK" activation="ACTIVE" duration="0">
	<effects>
		<skillatk value="10" e="1"><subeffect skill_id="1051" chance="30"/></skillatk>
		<nosucheffect e="3"><subeffect skill_id="424242"/></nosucheffect>
	</effects>
</skill_template>
<skill_template skill_id="1031" name="fixture gated subeffect" stack="LV" lvl="1" skillsubtype="ATTACK" activation="ACTIVE" duration="0">
	<effects>
		<skillatk value="1" e="1"/>
		<spellatkinstant value="5" e="2"><subconditions/><subeffect skill_id="1052"/></spellatkinstant>
	</effects>
</skill_template>
<skill_template skill_id="1033" name="fixture subeffect that never lands" stack="LW" lvl="1" skillsubtype="ATTACK" activation="ACTIVE" duration="0">
	<effects><skillatk value="1" e="4"><subeffect skill_id="1053" chance="0"/></skillatk></effects>
</skill_template>
<skill_template skill_id="1035" name="fixture two subeffect effects" lvl="1" activation="ACTIVE">
	<effects>
		<skillatk value="10" e="1"><subeffect skill_id="1051" chance="30"/></skillatk>
		<spellatkinstant value="5" e="2"><subeffect skill_id="1052"/></spellatkinstant>
	</effects>
</skill_template>
<skill_template skill_id="1051" name="fixture stumble" stack="LB" lvl="4" skillsubtype="NONE" tslot="DEBUFF" activation="ACTIVE" duration="3000">
	<effects><stumble duration1="2000" e="1"/></effects>
</skill_template>
<skill_template skill_id="1052" name="fixture stagger" stack="LC" lvl="1" skillsubtype="NONE" tslot="DEBUFF" activation="ACTIVE" duration="3000">
	<effects><stagger duration1="2000" e="1"/></effects>
</skill_template>
<skill_template skill_id="1053" name="fixture never" stack="LD" lvl="1" skillsubtype="NONE" activation="ACTIVE" duration="0">
	<effects><spin e="1"/></effects>
</skill_template>
<skill_template skill_id="1032" name="fixture provokers" stack="LE" lvl="1" skillsubtype="BUFF" tslot="BUFF" activation="ACTIVE" duration="0">
	<effects>
		<provoker skill_id="1054" provoke_target="OPPONENT" hittype="NMLATK" hittypeprob2="20" radius="25" duration2="5000" e="1"/>
		<provoker skill_id="1054" provoke_target="ME" duration2="5000" e="2"/>
	</effects>
</skill_template>
<skill_template skill_id="1054" name="fixture blessing" stack="LF" lvl="3" skillsubtype="BUFF" tslot="BUFF" activation="PROVOKED" duration="1000">
	<effects><statup duration2="5000" duration1="100" e="1"><change stat="MAXHP" func="ADD" value="1"/></statup></effects>
</skill_template>
<skill_template skill_id="1034" name="fixture delayed" stack="LG" lvl="1" skillsubtype="DEBUFF" tslot="DEBUFF" activation="ACTIVE" duration="0">
	<effects><delayedskill skill_id="1055" duration2="3000" e="1"/></effects>
</skill_template>
<skill_template skill_id="1055" name="fixture delayed blow" stack="LH" lvl="2" skillsubtype="ATTACK" activation="PROVOKED" duration="0">
	<effects><skillatk value="20" e="1"><subeffect skill_id="1052"/></skillatk></effects>
</skill_template>
<skill_template skill_id="1036" name="fixture carve" stack="LI" lvl="1" skillsubtype="ATTACK" activation="ACTIVE" duration="0">
	<effects><carvesignet signet="SIGNETX" signet_id="1040" signet_cap="3" prob="70" value="10" e="1"/></effects>
</skill_template>
<skill_template skill_id="1037" name="fixture deep carve" stack="LJ" lvl="1" skillsubtype="ATTACK" activation="ACTIVE" duration="0">
	<effects><carvesignet signet="SIGNETX" signet_id="1040" signet_cap="5" signet_increment="2" value="10" e="1"/></effects>
</skill_template>
<skill_template skill_id="1038" name="fixture carve that never lands" stack="LK" lvl="1" skillsubtype="ATTACK" activation="ACTIVE" duration="0">
	<effects><carvesignet signet="SIGNETX" signet_id="1040" signet_cap="7" prob="0" value="10" e="1"/></effects>
</skill_template>
""" + "".join(
	f'<skill_template skill_id="{1039 + level}" name="fixture signet {level}" stack="SIGNETX" lvl="{level}" skillsubtype="DEBUFF" tslot="DEBUFF" '
	f'activation="PROVOKED" duration="0"><effects><signet duration2="10000" e="1"/></effects></skill_template>\n' for level in range(1, 8)) + """
<skill_template skill_id="1060" name="fixture launcher" stack="LL" lvl="1" skillsubtype="DEBUFF" activation="ACTIVE" duration="0">
	<effects><skilllauncher skill_id="1061" e="1"/></effects>
</skill_template>
<skill_template skill_id="1061" name="fixture launched root" stack="LM" lvl="2" skillsubtype="DEBUFF" tslot="DEBUFF" activation="PROVOKED"
	duration="0">
	<effects><root duration2="1000" e="1"/></effects>
</skill_template>
<skill_template skill_id="1062" name="fixture last stand" stack="LN" lvl="1" skillsubtype="BUFF" tslot="NOSHOW" activation="PASSIVE" duration="0">
	<effects><condskilllauncher skill_id="1063" value="30" e="1"/></effects>
</skill_template>
<skill_template skill_id="1063" name="fixture dodge" stack="LO" lvl="1" skillsubtype="BUFF" tslot="BUFF" activation="PROVOKED" duration="0">
	<effects><alwaysdodge duration2="5000" e="1"/></effects>
</skill_template>
<skill_template skill_id="1064" name="fixture aura" stack="LP" lvl="1" skillsubtype="BUFF" tslot="BUFF" activation="TOGGLE" duration="0">
	<effects><aura skill_id="1065" distance="20" e="1"/></effects>
</skill_template>
<skill_template skill_id="1065" name="fixture aura pulse" stack="LQ" lvl="1" skillsubtype="BUFF" tslot="BUFF" activation="PROVOKED" duration="0">
	<effects><statup duration2="6500" e="1"><change stat="MAXMP" func="ADD" value="1"/></statup></effects>
</skill_template>
<skill_template skill_id="1070" name="fixture ping" stack="LR" lvl="5" skillsubtype="ATTACK" activation="ACTIVE" duration="0">
	<effects><skillatk value="1" e="1"><subeffect skill_id="1071"/></skillatk></effects>
</skill_template>
<skill_template skill_id="1071" name="fixture pong" stack="LS" lvl="1" skillsubtype="ATTACK" activation="ACTIVE" duration="0">
	<effects><skillatk value="1" e="1"><subeffect skill_id="1070"/></skillatk></effects>
</skill_template>
<skill_template skill_id="1072" name="fixture echo" stack="LT" lvl="1" skillsubtype="ATTACK" activation="ACTIVE" duration="0">
	<effects><skillatk value="1" e="1"><subeffect skill_id="1072" chance="50"/></skillatk></effects>
</skill_template>
<skill_template skill_id="1073" name="fixture burst" stack="LU" lvl="1" skillsubtype="ATTACK" activation="ACTIVE" duration="0">
	<effects><signetburst signet="SIGNETX" signetlvl="5" value="10" e="1"><subeffect skill_id="1052"/></signetburst></effects>
</skill_template>
<skill_template skill_id="1080" name="fixture resurrect" lvl="1" activation="ACTIVE">
	<effects><resurrect skill_id="1061" e="1"/></effects>
</skill_template>
<skill_template skill_id="1081" name="fixture servant" lvl="1" activation="ACTIVE">
	<effects><summonservant npc_id="900001" e="1"/></effects>
</skill_template>
<skill_template skill_id="1082" name="fixture burst add effect" lvl="1" activation="ACTIVE">
	<effects><signetburst signet="SIGNETX" signetlvl="5" value="10" e="1"><subeffect skill_id="1052" addeffect="true"/></signetburst></effects>
</skill_template>
<skill_template skill_id="1083" name="fixture two subeffects" lvl="1" activation="ACTIVE">
	<effects><skillatk value="1" e="1"><subeffect skill_id="1051"/><subeffect skill_id="1052"/></skillatk></effects>
</skill_template>
<skill_template skill_id="1084" name="fixture provoker without skill" lvl="1" activation="ACTIVE">
	<effects><provoker provoke_target="ME" e="1"/></effects>
</skill_template>
<skill_template skill_id="1085" name="fixture missing launch" lvl="1" activation="ACTIVE">
	<effects><skillatk value="1" e="1"><subeffect skill_id="424243"/></skillatk></effects>
</skill_template>
<skill_template skill_id="1086" name="fixture provoker without target" lvl="1" activation="ACTIVE">
	<effects><provoker skill_id="1054" e="1"/></effects>
</skill_template>
<skill_template skill_id="1087" name="fixture pet order" lvl="1" activation="ACTIVE">
	<effects><petorderuseultraskill e="1"/></effects>
</skill_template>
<skill_template skill_id="1088" name="fixture launches a resurrect" lvl="1" activation="ACTIVE">
	<effects><skilllauncher skill_id="1080" e="1"/></effects>
</skill_template>
<skill_template skill_id="1090" name="fixture penalty" stack="PA" lvl="1" skillsubtype="ATTACK" activation="ACTIVE" duration="0"
	penalty_skill_id="1091">
	<effects><skillatk value="1" e="1"/></effects>
</skill_template>
<skill_template skill_id="1091" name="fixture guardian" stack="PB" lvl="3" skillsubtype="BUFF" tslot="BUFF" activation="ACTIVE" duration="0">
	<effects><provoker skill_id="1054" provoke_target="ME" hittype="EVERYHIT" duration2="5000" e="1"/></effects>
</skill_template>
<skill_template skill_id="1092" name="fixture penalty with message" stack="PC" lvl="1" skillsubtype="ATTACK" activation="ACTIVE" duration="0"
	penalty_skill_id="1093" penalty_skill_send_msg="true">
	<effects><skillatk value="1" e="1"/></effects>
</skill_template>
<skill_template skill_id="1093" name="fixture mp return" stack="PD" lvl="4" skillsubtype="HEAL" activation="ACTIVE" duration="0"
	penalty_skill_id="1094">
	<effects><mphealinstant value="10" e="1"/></effects>
</skill_template>
<skill_template skill_id="1094" name="fixture second penalty" stack="PE" lvl="2" skillsubtype="BUFF" tslot="BUFF" activation="ACTIVE" duration="0">
	<effects><alwaysblock duration2="3000" e="1"/></effects>
</skill_template>
<skill_template skill_id="1095" name="fixture launches a penalty skill" stack="PF" lvl="1" skillsubtype="DEBUFF" activation="ACTIVE" duration="0">
	<effects><skilllauncher skill_id="1090" e="1"/></effects>
</skill_template>
<skill_template skill_id="1096" name="fixture passive penalty" stack="PG" lvl="1" skillsubtype="NONE" tslot="NOSHOW" activation="PASSIVE"
	duration="0" penalty_skill_id="1091">
	<effects><statup e="1"><change stat="MAXHP" func="ADD" value="1"/></statup></effects>
</skill_template>
<skill_template skill_id="1097" name="fixture missing penalty" lvl="1" activation="ACTIVE" penalty_skill_id="424244"/>
<skill_template skill_id="1098" name="fixture penalty onto a penalty" stack="PH" lvl="1" skillsubtype="ATTACK" activation="ACTIVE" duration="0"
	penalty_skill_id="1090"/>
"""

NPC_SKILLS = (
	'<npc_skills npc_ids="900001 900002"><npc_skill id="1020" lv="3" prob="25"/></npc_skills>'
	'<npc_skills npc_ids="900001"><npc_skill id="1002" lv="9" prob="100" is_post_spawn="true"/></npc_skills>'  # the second list of 900001
	'<npc_skills npc_ids="900004"><npc_skill id="1032" lv="1" prob="50"/><npc_skill id="1030" lv="2" prob="50"/>'
	'<npc_skill id="1031" lv="2" prob="50"/></npc_skills>'
	'<npc_skills npc_ids="900005"><npc_skill id="1090" lv="1" prob="100"/></npc_skills>'
	'<npc_skills npc_ids="900006"><npc_skill id="1096" lv="1" prob="100"/></npc_skills>'
)

NPCS = ('<npc_template npc_id="900001" level="3" name="fixture caster" cast_speed="1100"><stats maxHp="10"/></npc_template>'
        '<npc_template npc_id="900002" level="4" name="fixture default"><stats maxHp="10"/></npc_template>'
        '<npc_template npc_id="900003" level="5" name="fixture silent"><stats maxHp="10"/></npc_template>'
        '<npc_template npc_id="900004" level="6" name="fixture launcher"><stats maxHp="10"/></npc_template>'
        '<npc_template npc_id="900005" level="7" name="fixture penalty caster"><stats maxHp="10"/></npc_template>'
        '<npc_template npc_id="900006" level="7" name="fixture passive caster"><stats maxHp="10"/></npc_template>')


def fixture_tree(item='<item_template id="500" name="fixture sword" item_group="SWORD"><weapon_stats attack_range="1500"/></item_template>',
                 item_sets='<itemset id="1"><itempart itemid="1"/></itemset>', passive_change="PHYSICAL_ATTACK", passive_tag="statboost",
                 sickness_attributes="", sickness_activation="PROVOKED"):
	tree = Tree()
	skills = SKILLS.replace('<change stat="PHYSICAL_ATTACK"', f'<change stat="{passive_change}"').replace("<statboost e=", f"<{passive_tag} e=") \
		.replace("</statboost>", f"</{passive_tag}>").replace('<skill_template skill_id="8291" ', f'<skill_template skill_id="8291" {sickness_attributes} ') \
		.replace('tslot="SPEC2" activation="PROVOKED"', f'tslot="SPEC2" activation="{sickness_activation}"') + LAUNCH_SKILLS
	tree.minimal({
		"skill_tree": SKILL_TREE,
		"skill_data": skills,
		"npc_skill_templates": NPC_SKILLS,
		"npc_templates": NPCS,
		"item_templates": item,
		"item_sets": item_sets,
		"player_initial_data": '<elyos_spawn_location map_id="1" x="1" y="2" z="3" heading="0"/>'
		                       '<asmodian_spawn_location map_id="2" x="1" y="2" z="3" heading="0"/>'
		                       '<player_data class="WARRIOR"><items><item id="500" count="1"/></items></player_data>',
	})
	return tree


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5b2FixtureReportTest(unittest.TestCase):
	"""The whole report on a small static_data tree (the Java tables still come from the Java sources)."""

	def setUp(self):
		self.tree = fixture_tree()
		self.data = StaticData(self.tree.root)

	def tearDown(self):
		self.tree.close()

	def report(self, data=None, **kwargs):
		return skills_report(data or self.data, JAVA_SRC, "ELYOS", "WARRIOR", **kwargs)

	@staticmethod
	def skill(report, skill_id, level=None):
		found = [s for s in report["skills"] if s["skillId"] == skill_id and (level is None or s["level"] == level)]
		if len(found) != 1:
			raise AssertionError(f"{len(found)} entries for skill {skill_id} level {level}")
		return found[0]

	def test_the_autolearn_set_follows_learn_new_skills(self):
		report = self.report()
		self.assertEqual(report["format"], "aion-m5b2-skills")
		self.assertEqual(report["character"]["skills"],
		                 [{"skillId": s, "level": 1} for s in (1001, 1002, 1004, 1008, 1013, 30001)],
		                 "the class row, the class-less rows (race PC_ALL and ELYOS), the Elyos row, the passive and 30001 - not the Asmodian rows, "
		                 "the manual, the level 2 or the Mage row")
		self.assertEqual(report["character"]["passives"], [1008])
		level2 = self.report(level=2)
		self.assertIn({"skillId": 1006, "level": 2}, level2["character"]["skills"], "level 2 adds the minLevel 2 row at its template lvl")
		with self.assertRaises(OracleError):
			self.report(level=10)  # PlayerCommonData caps a non-daeva at 9
		with self.assertRaises(OracleError):
			self.report(level=0)
		with self.assertRaises(OracleError):
			skills_report(self.data, JAVA_SRC, "ELYOS", "GLADIATOR")  # CM_CREATE_CHARACTER refuses a class that is not a starting class

	def test_a_class_that_is_not_a_starting_class_learns_its_starting_class_rows_too(self):
		# SkillLearnService.learnNewSkills: below level 10 the starting class arm runs first, and it is the starting class it passes to
		# autoLearnSkills, so its 30001 row is taught although the GLADIATOR arm skips it (SkillLearnService.java:63-66, :88)
		skills = learn_new_skills(self.data, JavaEnums(JAVA_SRC), "ELYOS", "GLADIATOR", 1, 1)
		self.assertEqual(sorted(skills), [1001, 1002, 1004, 1008, 1009, 1013, 30001])

	def test_a_cast_skill_with_costs_cooldown_and_chain(self):
		bolt = self.skill(self.report(), 1001)
		self.assertEqual((bolt["castDuration"], bolt["baseCastDuration"], bolt["castSpeed"], bolt["allowAnimationBoost"]), (2000, 2000, 1.0, True))
		self.assertEqual((bolt["cooldown"], bolt["cooldownDeltaLv"], bolt["cooldownMillis"], bolt["cooldownId"]), (52, 2, 5000, 11),
		                 "Skill.getCooldown adds cooldown_delta_lv * level; SM_SKILL_COOLDOWN's duration is the template value * 100")
		self.assertEqual(bolt["mpCost"], 13, "<mp value=10 delta=3> at level 1")
		self.assertEqual(bolt["costs"], {"start": {"dp": {"value": 7}}, "use": {},
		                                 "end": {"mp": {"value": 10, "delta": 3, "ratio": False, "cost": 13},
		                                         "hp": {"value": 5, "delta": 0, "ratio": False, "cost": 5}}})
		self.assertEqual(bolt["chainCategory"], "F_CHAIN")
		self.assertEqual((bolt["skillType"], bolt["subType"], bolt["category"], bolt["activation"], bolt["method"]),
		                 ("MAGICAL", "ATTACK", "NONE", "ACTIVE", "CAST"))
		self.assertEqual((bolt["chainSkillProb"], bolt["dispelCategory"], bolt["reqDispelLevel"]), (100, "NONE", 0), "the JAXB defaults")
		self.assertEqual(bolt["targetSlot"], {"name": "NONE", "ordinal": 7, "id": 128})
		self.assertEqual(bolt["properties"], {"first_target": "TARGET", "first_target_range": "25"})
		[spell] = bolt["effects"]
		self.assertEqual((spell["tag"], spell["class"], spell["classChain"], spell["position"], spell["value"], spell["skillElement"]),
		                 ("spellatkinstant", "SpellAttackInstantEffect", ["SpellAttackInstantEffect", "DamageEffect", "EffectTemplate"], 1, 100, "FIRE"))
		self.assertEqual((bolt["effectDuration"], bolt["notModelled"]), (0, []))
		everyone = self.skill(self.report(), 1002)
		self.assertEqual((everyone["castDuration"], everyone["skillType"], everyone["category"], everyone["targetSlot"]), (1000, "NONE", "NONE", None),
		                 "no apply_casting_time_bonus: the base; no tslot attribute: null")

	def test_a_debuff_with_positions_out_of_document_order(self):
		debuff = self.skill(self.report(), 1004)
		self.assertEqual(debuff["targetSlot"], {"name": "DEBUFF", "ordinal": 1, "id": 2})
		self.assertEqual((debuff["dispelCategory"], debuff["reqDispelLevel"], debuff["cooldown"], debuff["cooldownMillis"]), ("ALL", 1, 600, 60000))
		self.assertEqual([(e["class"], e["position"], e["duration1"], e["duration2"]) for e in debuff["effects"]],
		                 [("StatdownEffect", 2, 0, 50000), ("RootEffect", 1, 1000, 20000)], "document order")
		self.assertEqual((debuff["effectDuration"], debuff["effectDurationRandomTime"]), (21000, 300),
		                 "position 1 decides: 20000 + 1000 * level 1, and its randomtime")
		self.assertEqual(debuff["effects"][1]["preEffects"], [2, 3])
		self.assertEqual(debuff["effects"][0]["changes"], [{"stat": "MAXHP", "func": "PERCENT", "value": "-30"}])

	def test_extra_skills_soul_sickness_and_what_is_not_modelled(self):
		report = self.report(extra_skills=["1010", "1011", "1012", "1004:5", "1001:3"], death_count=3)
		bolt3 = self.skill(report, 1001, 3)
		self.assertEqual((bolt3["cooldown"], bolt3["cooldownMillis"], bolt3["mpCost"]), (56, 5000, 19),
		                 "at level 3: 50 + 2 * 3 and 10 + 3 * 3; the SM_SKILL_COOLDOWN duration ignores the level")
		charge = self.skill(report, 1010)
		self.assertIsNone(charge["castDuration"])
		self.assertIsNone(charge["castSpeed"])
		self.assertTrue(any("CHARGE" in reason for reason in charge["notModelled"]))
		unknown = self.skill(report, 1011)
		self.assertEqual(unknown["level"], 4, "an extra skill without a level takes its template lvl")
		self.assertEqual(unknown["ignoredEffectElements"], ["nosucheffect"], "JAXB drops a tag @XmlElements does not bind")
		self.assertEqual([e["class"] for e in unknown["effects"]], ["AlwaysDodgeEffect"])
		self.assertEqual(unknown["effectDuration"], 5000)
		ratio = self.skill(report, 1012)
		self.assertIsNone(ratio["mpCost"])
		self.assertEqual(ratio["costs"]["start"], {"mp": {"value": 4, "delta": 0, "ratio": False, "cost": 4}}, "a start section mp is reported")
		self.assertTrue(any("ratio" in reason for reason in ratio["notModelled"]))
		debuff5 = self.skill(report, 1004, 5)
		self.assertEqual((debuff5["effectDuration"], debuff5["sources"]), (25000, ["extra"]), "20000 + 1000 * 5 at the requested level")
		self.assertEqual(self.skill(report, 1004, 1)["sources"], ["autolearn"])
		sickness = self.skill(report, 8291)
		self.assertEqual((sickness["level"], sickness["effectDuration"], sickness["method"], sickness["sources"]), (3, 100000, "PROVOKED",
		                                                                                                          ["soulSickness"]),
		                 "updateSoulSickness casts at skill level deathCount: 40000 + 20000 * 3")
		self.assertEqual(report["soulSickness"], {"skillId": 8291, "deathCount": 3, "maxDeathCount": 10})
		self.assertEqual(sickness["targetSlot"], {"name": "SPEC2", "ordinal": 4, "id": 16})
		with self.assertRaises(OracleError):
			self.report(death_count=11)
		with self.assertRaises(OracleError):
			self.report(death_count=0)
		with self.assertRaises(OracleError):
			self.report(extra_skills=["424242"])  # no template

	def test_over_time_templates_last_a_second_longer(self):
		# AbstractOverTimeEffect.getDuration2 (AbstractOverTimeEffect.java:64-67) is `duration2 + 1000`, and calculateTemplateDuration calls it
		report = self.report(extra_skills=["1015", "1016:3"])
		bleed_first = self.skill(report, 1015)
		self.assertEqual([(e["class"], e["duration2"], e["effectiveDuration2"]) for e in bleed_first["effects"]],
		                 [("BleedEffect", 0, 1000), ("RootEffect", 20000, 20000)], "the attribute as written, and what getDuration2() answers")
		self.assertEqual(bleed_first["effectDuration"], 1000,
		                 "position 1 is a bleed without duration2: getDuration2() is 1000, which is positive, so the root's 20000 never counts")
		heal = self.skill(report, 1016, 3)
		self.assertEqual([(e["class"], e["classChain"][1:3], e["effectiveDuration2"]) for e in heal["effects"]],
		                 [("HealEffect", ["HealOverTimeEffect", "AbstractOverTimeEffect"], 3000)])
		self.assertEqual(heal["effectDuration"], 3300, "2000 + 1000 + 100 * level 3: the override two classes up the chain still applies")
		overflow = self.skill(self.report(extra_skills=["1017"]), 1017)
		self.assertEqual(overflow["effects"][0]["effectiveDuration2"], 2147484000 - 2**32,
		                 "`duration2 + 1000` is int arithmetic in Java: 2147483000 + 1000 wraps to a negative getDuration2()")
		self.assertEqual(overflow["effectDuration"], 5000, "... which is not positive, so the next position decides")

	def test_an_event_boost_lasts_integer_max_value(self):
		# the shape of skill_templates.xml:109087/:109104, the 60-day Experience Boost: getDuration2() 1184000000 + duration1 2000000000 * level
		# is a long in Effect.calculateTemplateDuration (Effect.java:902), and Effect.calculateEffectsDuration clamps it (:896)
		report = self.report(extra_skills=["1018:1", "1018:2"])
		for level, unclamped in ((1, 3184000000), (2, 5184000000)):
			with self.subTest(level=level):
				boost = self.skill(report, 1018, level)
				self.assertEqual([e["class"] for e in boost["effects"]], ["XPBoostEffect", "RootEffect"])
				self.assertEqual((boost["effectDuration"], boost["effectDurationRandomTime"], boost["notModelled"]), (2**31 - 1, 0, []),
				                 f"(int) Math.min(Integer.MAX_VALUE, {unclamped}) - not the int arithmetic (the root's 5000 at level 1, 889032704 "
				                 "at level 2) and not the unclamped long")

	def test_npc_skill_lists_first_list_wins_and_npc_cast_speed(self):
		report = self.report(npc_ids=[900001, 900002, 900003])
		first, default, silent = report["npcs"]
		self.assertEqual((first["npcId"], first["castSpeed"], first["level"]), (900001, 1100, 3))
		self.assertEqual([(s["skillId"], s["level"], s["prob"], s["isPostSpawn"]) for s in first["skills"]], [(1020, 3, 25, False)],
		                 "NpcSkillData.afterUnmarshal: putIfAbsent, so the second list naming 900001 is ignored")
		self.assertEqual(first["skills"][0]["castDuration"], 3300, "Math.round(3000 * 1.1f)")
		self.assertEqual((first["skills"][0]["target"], first["skills"][0]["maxHp"], first["skills"][0]["nextSkillTime"]), ("MOST_HATED", 100, -1))
		self.assertEqual((default["castSpeed"], default["skills"][0]["castDuration"]), (1000, 3000), "cast_speed defaults to 1000")
		self.assertEqual(silent["skills"], [], "an npc without a list")
		self.assertEqual(self.skill(report, 1020, 3)["sources"], ["npc:900001", "npc:900002"])

	def test_casting_time_sources_make_the_cast_duration_unknown(self):
		clean = self.report()
		self.assertEqual((clean["character"]["castingTimeSources"], clean["character"]["skillCostSources"]), ([], []))
		variants = {
			"item": fixture_tree(item='<item_template id="500" name="fixture sword" item_group="SWORD"><weapon_stats attack_range="1500"/>'
			                          '<modifiers><add name="BOOST_CASTING_TIME" value="100"/></modifiers></item_template>'),
			"set": fixture_tree(item_sets='<itemset id="7"><itempart itemid="500"/></itemset>'),
			"change": fixture_tree(passive_change="BOOST_CASTING_TIME_ATTACK"),
			"effect": fixture_tree(passive_tag="boostskillcastingtime"),
		}
		for name, tree in variants.items():
			with self.subTest(source=name), tree:
				report = self.report(StaticData(tree.root))
				self.assertEqual(len(report["character"]["castingTimeSources"]), 1, report["character"]["castingTimeSources"])
				bolt = self.skill(report, 1001)
				self.assertIsNone(bolt["castDuration"])
				self.assertIsNone(bolt["castSpeed"])
				self.assertEqual(self.skill(report, 1002)["castDuration"], 1000, "a skill without apply_casting_time_bonus keeps its base")
		with fixture_tree(passive_tag="boostskillcost") as tree:
			report = self.report(StaticData(tree.root))
			self.assertEqual(report["character"]["skillCostSources"], ["passive skill 1008 <boostskillcost>"])
			self.assertIsNone(self.skill(report, 1001)["mpCost"])
			self.assertEqual(self.skill(report, 1001)["castDuration"], 2000)


def edge_view(edges):
	"""(kind, effect position, launched skill, level, chance, followed) of each launch edge, in report order; a penalty has no effect (None)."""
	return [(e["kind"], e["effect"]["position"] if e["effect"] else None, e["skillId"], e["level"], e["chance"], e["followed"]) for e in edges]


def launcher_view(links):
	"""(launching skill, its level, kind, effect position, chance) of each launchedBy link; a penalty has no effect (None)."""
	return [(link["skillId"], link["level"], link["kind"], link["effect"]["position"] if link["effect"] else None, link["chance"]) for link in links]


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5b2LaunchClosureTest(unittest.TestCase):
	"""The skills an effect launches, followed to a fixpoint (LaunchClosure), on the LAUNCH_SKILLS fixture: one case per launch kind, a chain,
	cycles, the per-npc and per-character closures, and every launch shape the oracle refuses."""

	@classmethod
	def setUpClass(cls):
		cls.tree = fixture_tree()
		cls.data = StaticData(cls.tree.root)

	@classmethod
	def tearDownClass(cls):
		cls.tree.close()

	def report(self, **kwargs):
		return skills_report(self.data, JAVA_SRC, "ELYOS", "WARRIOR", **kwargs)

	skill = staticmethod(M5b2FixtureReportTest.skill)

	def test_subeffects_run_at_level_1_with_their_chance(self):
		# EffectTemplate.calculateSubEffect: `int level = 1` (EffectTemplate.java:419) whatever the template's lvl, Rnd.chance() >= chance fails
		# (:415), the chance defaulting to 100 (SubEffect.java:18); a tag Effects.java does not bind is dropped with its <subeffect>
		report = self.report(extra_skills=["1030", "1031", "1033"])
		parent = self.skill(report, 1030, 2)
		self.assertEqual(edge_view(parent["launches"]), [("subeffect", 1, 1051, 1, 30, True)],
		                 "the <nosucheffect> of position 3 is dropped by JAXB, so its skill 424242 (no template) is never launched, and it is no "
		                 "second <subeffect> of the Effect either")
		self.assertEqual([(e["effect"]["tag"], e["effect"]["class"], e["effect"]["index"], e["gates"]) for e in parent["launches"]],
		                 [("skillatk", "SkillAttackInstantEffect", 0, [])])
		gated = self.skill(report, 1031)["launches"]
		self.assertEqual(edge_view(gated), [("subeffect", 2, 1052, 1, 100, True)])
		self.assertEqual([(e["effect"]["tag"], e["effect"]["index"], e["gates"]) for e in gated], [("spellatkinstant", 1, ["subconditions"])])
		self.assertEqual(edge_view(self.skill(report, 1033)["launches"]), [("subeffect", 4, 1053, 1, 0, False)])
		stumble = self.skill(report, 1051)
		self.assertEqual((stumble["level"], stumble["lvl"], stumble["sources"], stumble["effectDuration"]), (1, 4, ["launch"], 2000),
		                 "launched at level 1, not at its lvl 4: duration1 2000 * 1")
		self.assertEqual(launcher_view(stumble["launchedBy"]), [(1030, 2, "subeffect", 1, 30)])
		self.assertEqual(launcher_view(self.skill(report, 1052)["launchedBy"]), [(1031, 1, "subeffect", 2, 100)])
		self.assertEqual([s["skillId"] for s in report["skills"] if s["skillId"] == 1053], [],
		                 "a chance of 0 never launches (Rnd.chance() is never below 0): reported as an edge, not followed")
		self.assertIn("StumbleEffect", report["effectClasses"]["addedByLaunches"]["leaves"])
		self.assertNotIn("SpinEffect", report["effectClasses"]["withBases"])

	def test_provokers_launch_at_the_template_level_per_qualifying_hit(self):
		# ProvokerEffect.java:43 (ATTACK for NMLATK/BACKATK, else ATTACKED), :69-73 (OPPONENT, radius, hittypeprob2), :62 applyEffectDirectly
		# at skillTemplate.getLvl() (SkillEngine.java:121-124); hittype EVERYHIT and hittypeprob2 100 by default (EffectTemplate.java:52-54)
		report = self.report(extra_skills=["1032"])
		parent = self.skill(report, 1032)
		self.assertEqual(edge_view(parent["launches"]), [("provoker", 1, 1054, 3, 20, True), ("provoker", 2, 1054, 3, 100, True)])
		self.assertEqual([(e["observer"], e["hitType"], e["provokeTarget"], e["radius"]) for e in parent["launches"]],
		                 [("ATTACK", "NMLATK", "OPPONENT", 25), ("ATTACKED", "EVERYHIT", "ME", 0)])
		blessing = self.skill(report, 1054)
		self.assertEqual((blessing["level"], blessing["effectDuration"], blessing["sources"]), (3, 5300, ["launch"]),
		                 "at its lvl 3: 5000 + 100 * 3")
		self.assertEqual(launcher_view(blessing["launchedBy"]), [(1032, 1, "provoker", 1, 20), (1032, 1, "provoker", 2, 100)])

	def test_delayed_skills_and_the_fixpoint(self):
		# DelayedSkillEffect.java:24-25 when the effect ends by time, at the lvl of 1055 (2); 1055 then launches 1052 by a subeffect: two steps
		report = self.report(extra_skills=["1034"])
		self.assertEqual(edge_view(self.skill(report, 1034)["launches"]), [("delayedskill", 1, 1055, 2, 100, True)])
		self.assertEqual(self.skill(report, 1034)["launches"][0]["trigger"], "endedByTime")
		blow = self.skill(report, 1055)
		self.assertEqual((blow["level"], blow["sources"], launcher_view(blow["launchedBy"])), (2, ["launch"], [(1034, 1, "delayedskill", 1, 100)]))
		self.assertEqual(launcher_view(self.skill(report, 1052)["launchedBy"]), [(1055, 2, "subeffect", 1, 100)], "the second step")
		self.assertEqual(report["effectClasses"]["addedByLaunches"], {"leaves": ["SkillAttackInstantEffect", "StaggerEffect"],
		                                                              "withBases": ["SkillAttackInstantEffect", "StaggerEffect"]},
		                 "DamageEffect is not added: it is a base of the autolearnt 1001's SpellAttackInstantEffect")

	def test_carved_signets_follow_every_carver_of_the_stack(self):
		# CarveSignetEffect.java:34-44: 1036 (cap 3, prob 70) carves 1, 2, 3 itself, and keeps a SIGNETX carved to 4 or 5 by 1037 (cap 5,
		# increment 2) at that level: min(4 + 1, max(3, 4)) = 4, min(5 + 1, max(3, 5)) = 5. 1038 (cap 7) has prob 0 and never carves.
		report = self.report(extra_skills=["1036", "1037"])
		carve = self.skill(report, 1036)
		self.assertEqual(edge_view(carve["launches"]), [("carvesignet", 1, 1039 + level, level, 70, True) for level in range(1, 6)])
		self.assertEqual([(e["carvedLevel"], e["viaOtherCarvers"]) for e in carve["launches"]],
		                 [(1, False), (2, False), (3, False), (4, True), (5, True)])
		self.assertEqual({(e["signet"], e["signetCap"], e["signetIncrement"]) for e in carve["launches"]}, {("SIGNETX", 3, 1)})
		deep = self.skill(report, 1037)
		self.assertEqual([(e["carvedLevel"], e["skillId"], e["chance"], e["viaOtherCarvers"]) for e in deep["launches"]],
		                 [(2, 1041, 100, False), (3, 1042, 100, True), (4, 1043, 100, False), (5, 1044, 100, False)],
		                 "increment 2 from nothing is 2, then 4, then 5; 3 only on a signet 1036 carved to 1; never 1")
		self.assertEqual(sorted(s["skillId"] for s in report["skills"] if s["skillId"] in range(1040, 1047)), [1040, 1041, 1042, 1043, 1044],
		                 "no 1045 or 1046: the prob 0 carver cannot carve 6 or 7")
		self.assertEqual(launcher_view(self.skill(report, 1044)["launchedBy"]), [(1036, 1, "carvesignet", 1, 70), (1037, 1, "carvesignet", 1, 100)])
		self.assertEqual([link["carvedLevel"] for link in self.skill(report, 1044)["launchedBy"]], [5, 5])

	def test_skill_launchers_conditional_launchers_and_auras(self):
		# SkillLauncherEffect.java:23, CondSkillLauncherEffect.java:46 (HP at or below value %; a PASSIVE launcher's effect is permanent),
		# AuraEffect.java:71 - each at the launched template's lvl
		report = self.report(extra_skills=["1060", "1062", "1064"])
		self.assertEqual(edge_view(self.skill(report, 1060)["launches"]), [("skilllauncher", 1, 1061, 2, 100, True)])
		conditional = self.skill(report, 1062)["launches"]
		self.assertEqual(edge_view(conditional), [("condskilllauncher", 1, 1063, 1, 100, True)])
		self.assertEqual((conditional[0]["hpPercent"], conditional[0]["permanent"], conditional[0]["trigger"]), (30, True, "hpAtOrBelow"))
		self.assertEqual(edge_view(self.skill(report, 1064)["launches"]), [("aura", 1, 1065, 1, 100, True)])
		self.assertEqual([self.skill(report, s)["sources"] for s in (1061, 1063, 1065)], [["launch"]] * 3)
		self.assertEqual(self.skill(report, 1061)["effectDuration"], 1000)

	def test_cycles_end(self):
		# 1070 (lvl 5) and 1071 launch each other through subeffects, at level 1; 1072 launches itself. The fixpoint is over (skill, level).
		report = self.report(extra_skills=["1070", "1072"])
		self.assertEqual(sorted((s["skillId"], s["level"], tuple(s["sources"])) for s in report["skills"] if s["skillId"] in (1070, 1071, 1072)),
		                 [(1070, 1, ("launch",)), (1070, 5, ("extra",)), (1071, 1, ("launch",)), (1072, 1, ("extra", "launch"))])
		self.assertEqual(launcher_view(self.skill(report, 1071)["launchedBy"]), [(1070, 1, "subeffect", 1, 100), (1070, 5, "subeffect", 1, 100)])
		self.assertEqual(launcher_view(self.skill(report, 1070, 1)["launchedBy"]), [(1071, 1, "subeffect", 1, 100)])
		self.assertEqual(self.skill(report, 1070, 5)["launchedBy"], [], "nothing launches 1070 at level 5")
		self.assertEqual(launcher_view(self.skill(report, 1072)["launchedBy"]), [(1072, 1, "subeffect", 1, 50)])

	def test_a_signet_burst_rolls_its_own_launch(self):
		# SignetBurstEffect.java:44 sets Effect.launchSubEffect from the signet data, and calculateSubEffect only runs while it is set
		report = self.report(extra_skills=["1073"])
		[edge] = self.skill(report, 1073)["launches"]
		self.assertEqual((edge["skillId"], edge["level"], edge["gates"]), (1052, 1, ["SignetBurstEffect.launchSubEffect"]))

	def test_the_npc_and_character_closures(self):
		report = self.report(level=3, npc_ids=[900004, 900001])
		launcher, caster = report["npcs"]
		self.assertEqual(launcher["launchedSkills"], [{"skillId": 1051, "level": 1}, {"skillId": 1052, "level": 1}, {"skillId": 1054, "level": 3}])
		self.assertEqual(launcher["effectClasses"]["addedByLaunches"], {"leaves": ["StaggerEffect", "StatupEffect", "StumbleEffect"],
		                                                                "withBases": ["BufEffect", "StaggerEffect", "StatupEffect", "StumbleEffect"]})
		self.assertEqual(launcher["effectClasses"]["leaves"], ["ProvokerEffect", "SkillAttackInstantEffect", "SpellAttackInstantEffect",
		                                                       "StaggerEffect", "StatupEffect", "StumbleEffect"])
		self.assertEqual((caster["launchedSkills"], caster["effectClasses"]["addedByLaunches"]), ([], {"leaves": [], "withBases": []}))
		# the level 3 Warrior autolearns 1034 (the fixture's skill_tree row): its closure is the character's
		character = report["character"]
		self.assertIn({"skillId": 1034, "level": 1}, character["skills"])
		self.assertEqual(character["launchedSkills"], [{"skillId": 1052, "level": 1}, {"skillId": 1055, "level": 2}])
		self.assertEqual(character["effectClasses"]["addedByLaunches"], {"leaves": ["SkillAttackInstantEffect", "StaggerEffect"],
		                                                                 "withBases": ["SkillAttackInstantEffect", "StaggerEffect"]},
		                 "DamageEffect is already a base of the autolearnt 1001's SpellAttackInstantEffect")
		self.assertEqual(self.skill(report, 1052)["sources"], ["launch"])
		self.assertEqual(report["effectClasses"]["addedByLaunches"], {"leaves": ["StaggerEffect", "StatupEffect", "StumbleEffect"],
		                                                              "withBases": ["StaggerEffect", "StatupEffect", "StumbleEffect"]},
		                 "1055's SkillAttackInstantEffect is new for the character, not for the report (the npcs' 1030 and 1020 have it), and "
		                 "BufEffect is a base of the direct StatdownEffect")
		self.assertEqual(report["version"], 2)

	def test_penalty_skills_run_when_the_cast_ends(self):
		# Skill.endCast (Skill.java:646-648) -> startPenaltySkill (:478-489). Without penalty_skill_send_msg the penalty skill is applied from
		# the first target on the caster at its lvl (applyEffectDirectly, :488; SkillEngine.java:121-124). With it, PenaltySkill.useSkill casts
		# it at level 1 (getPenaltySkill(effector, penaltySkill, 1), :483), and a skill so used runs endCast itself (PenaltySkill.java:12-14
		# -> useWithoutPropSkill), so its own penalty skill follows. No roll: chance 100, unless blockedPenaltySkill (:603-606)
		report = self.report(extra_skills=["1090", "1092"])
		[penalty] = self.skill(report, 1090)["launches"]
		self.assertEqual(edge_view([penalty]), [("penalty", None, 1091, 3, 100, True)])
		self.assertEqual({key: penalty[key] for key in ("trigger", "sendMsg", "effector", "runsEndCast", "gates")},
		                 {"trigger": "endCast", "sendMsg": False, "effector": "firstTarget", "runsEndCast": False, "gates": ["Skill.blockedPenaltySkill"]})
		guardian = self.skill(report, 1091)
		self.assertEqual((guardian["level"], guardian["sources"], launcher_view(guardian["launchedBy"])), (3, ["launch"], [(1090, 1, "penalty", None, 100)]))
		self.assertEqual(launcher_view(self.skill(report, 1054)["launchedBy"]), [(1091, 3, "provoker", 1, 100)], "the penalty skill's provoker")
		[message] = self.skill(report, 1092)["launches"]
		self.assertEqual(edge_view([message]), [("penalty", None, 1093, 1, 100, True)])
		self.assertEqual((message["sendMsg"], message["effector"], message["runsEndCast"]), (True, "caster", True))
		mp_return = self.skill(report, 1093)
		self.assertEqual((mp_return["level"], mp_return["lvl"], mp_return["sources"]), (1, 4, ["launch"]), "a PenaltySkill at level 1, not its lvl 4")
		self.assertEqual(edge_view(mp_return["launches"]), [("penalty", None, 1094, 2, 100, True)])
		second = self.skill(report, 1094)
		self.assertEqual((second["level"], launcher_view(second["launchedBy"])), (2, [(1093, 1, "penalty", None, 100)]),
		                 "1093 runs endCast, so its own penalty skill 1094 follows, at its lvl 2")
		self.assertEqual(report["effectClasses"]["addedByLaunches"]["leaves"], ["AlwaysBlockEffect", "MPHealInstantEffect", "ProvokerEffect",
		                                                                         "StatupEffect"])

	def test_only_a_skill_that_runs_end_cast_launches_its_penalty(self):
		# a skill an effect launches (Effect.applyEffect), a penalty skill applied without the message (applyEffectDirectly) and a passive
		# (applied directly, SkillLearnService.java:35-36; CM_CASTSPELL.java:92 refuses to cast one) never run Skill.endCast
		report = self.report(extra_skills=["1095", "1096", "1098"])
		cleave = self.skill(report, 1090)
		self.assertEqual(launcher_view(cleave["launchedBy"]), [(1095, 1, "skilllauncher", 1, 100), (1098, 1, "penalty", None, 100)])
		self.assertEqual(edge_view(cleave["launches"]), [("penalty", None, 1091, 3, 100, True)], "the edge is listed ...")
		self.assertEqual([s["skillId"] for s in report["skills"] if s["skillId"] in (1091, 1054)], [], "... but nothing reached 1090 or 1096 by a cast")
		self.assertEqual(self.skill(report, 1096)["launches"][0]["skillId"], 1091)
		# the same skill cast as well: one link per launching (skill, level), not one per way it was reached
		cast = self.report(extra_skills=["1095", "1090"])
		self.assertEqual(self.skill(cast, 1090)["sources"], ["extra", "launch"])
		self.assertEqual(launcher_view(self.skill(cast, 1091)["launchedBy"]), [(1090, 1, "penalty", None, 100)])
		# an npc casts every npc_skills row through Skill.useSkill (CreatureController.java:455-460); the level 4 Warrior casts 1092 and its
		# PenaltySkill 1093 runs endCast, but not the passive 1096
		report = self.report(level=4, npc_ids=[900005])
		[npc] = report["npcs"]
		self.assertEqual(npc["launchedSkills"], [{"skillId": 1054, "level": 3}, {"skillId": 1091, "level": 3}])
		character = report["character"]
		self.assertEqual([s["skillId"] for s in character["skills"] if 1090 < s["skillId"] < 2000], [1092, 1096])
		self.assertEqual(character["launchedSkills"], [{"skillId": 1052, "level": 1}, {"skillId": 1055, "level": 2}, {"skillId": 1093, "level": 1},
		                                               {"skillId": 1094, "level": 2}])
		self.assertEqual(launcher_view(self.skill(report, 1091)["launchedBy"]), [(1090, 1, "penalty", None, 100)], "the npc's 1090, not the passive")
		# CreatureController.useSkill does not ask whether the template is passive: an npc row runs endCast whatever its activation
		[passive_caster] = self.report(npc_ids=[900006])["npcs"]
		self.assertEqual(passive_caster["launchedSkills"], [{"skillId": 1054, "level": 3}, {"skillId": 1091, "level": 3}])
		# updateSoulSickness casts its skill through Skill.useSkill (PlayerController.java:732) whatever the activation, so it runs endCast -
		# even as a PASSIVE, which the character itself could not cast
		with fixture_tree(sickness_attributes='penalty_skill_id="1091"', sickness_activation="PASSIVE") as tree:
			sick = skills_report(StaticData(tree.root), JAVA_SRC, "ELYOS", "WARRIOR")
			self.assertEqual(launcher_view(self.skill(sick, 1091)["launchedBy"]), [(8291, 1, "penalty", None, 100)])

	def test_launch_shapes_the_oracle_does_not_model_are_refused(self):
		cases = {
			"1080": "ResurrectEffect launches a skill in a shape this oracle does not model",
			"1081": "SummonEffect binds the attribute npc_id",
			"1082": "addeffect",
			"1083": "2 <subeffect>s",
			"1084": "has no skill_id",
			"1085": "skill 1085 <skillatk> (subeffect) launches skill 424243, which has no skill_template",
			"1086": "provoke_target None",
			"1087": "PET_SKILL_DATA",
			"1088": "skill 1080 <resurrect>: ResurrectEffect",
			# an Effect keeps one sub effect and one <subconditions> abort flag for all its templates (Effect.java:479-481, :1001-1003)
			"1035": "skill 1035 has 2 effects with a <subeffect>",
			"1097": "skill 1097 penalty_skill_id (penalty) launches skill 424244, which has no skill_template",
		}
		for skill, message in cases.items():
			with self.subTest(skill=skill), self.assertRaises(OracleError) as caught:
				self.report(extra_skills=[skill])
			self.assertIn(message, str(caught.exception))

	def test_the_command_line_refuses_with_exit_code_2(self):
		arguments = ["m5b2-skills", "--static-data", str(self.tree.root), "--java-src", str(JAVA_SRC), "--race", "ELYOS", "--class", "WARRIOR"]
		err = io.StringIO()
		with contextlib.redirect_stderr(err), contextlib.redirect_stdout(io.StringIO()):
			self.assertEqual(oracle.main(arguments + ["--skill", "1088"]), 2)
		self.assertIn("does not model", err.getvalue())
		out = io.StringIO()
		with contextlib.redirect_stdout(out):
			self.assertEqual(oracle.main(arguments + ["--skill", "1034"]), 0)
		[blow] = [s for s in json.loads(out.getvalue())["skills"] if s["skillId"] == 1055]
		self.assertEqual(launcher_view(blow["launchedBy"]), [(1034, 1, "delayedskill", 1, 100)])


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5b2RealDataTest(unittest.TestCase):
	"""The gate's own characters (m5b2-plan.md D4): a level 1 Elyos Warrior and a level 1 Elyos Mage, the revive debuff (D5) and npc 210133."""

	@classmethod
	def setUpClass(cls):
		data = StaticData(runner.DEFAULT_STATIC_DATA)
		cls.data = data
		cls.warrior = skills_report(data, JAVA_SRC, "ELYOS", "WARRIOR")
		cls.mage = skills_report(data, JAVA_SRC, "ELYOS", "MAGE", extra_skills=["3195", "1838"], npc_ids=[210133])

	@staticmethod
	def skill(report, skill_id):
		[entry] = [s for s in report["skills"] if s["skillId"] == skill_id]
		return entry

	def test_the_autolearn_sets_include_the_class_less_rows_and_match_m5a_creation(self):
		for report, player_class in ((self.warrior, "WARRIOR"), (self.mage, "MAGE")):
			with self.subTest(player_class=player_class):
				ids = [s["skillId"] for s in report["character"]["skills"]]
				for class_less in (243, 245, 302):
					self.assertIn(class_less, ids, "skill_tree.xml rows without a classId belong to every class (m5b2-plan.md §12)")
				self.assertEqual(report["character"]["skills"], creation_report(self.data, JAVA_SRC, "ELYOS", player_class)["skills"])
				self.assertEqual((report["character"]["castingTimeSources"], report["character"]["skillCostSources"]), ([], []))
		self.assertEqual([s["skillId"] for s in self.warrior["character"]["skills"]], [37, 39, 40, 41, 42, 43, 103, 140, 243, 245, 302, 2864, 30001])
		self.assertEqual([s["skillId"] for s in self.mage["character"]["skills"]], [40, 100, 103, 243, 245, 302, 1282, 1328, 30001])
		self.assertEqual(self.warrior["character"]["passives"], [37, 39, 40, 41, 42, 43, 103, 140], "the eight passives of D2")
		self.assertEqual([self.skill(self.mage, s)["effects"][0]["class"] for s in (243, 245, 302)], ["ReturnEffect", "HealInstantEffect",
		                                                                                              "EscapeEffect"])

	def test_2864_ferocious_strike(self):
		strike = self.skill(self.warrior, 2864)
		self.assertEqual((strike["castDuration"], strike["mpCost"], strike["cooldown"], strike["cooldownMillis"]), (0, 0, 100, 10000),
		                 "instant, no <mp>, a 10 s cooldown")
		self.assertEqual((strike["chainCategory"], strike["chainSkillProb"], strike["category"]), ("W_CHAINA_1TH_1", 100, "CHAIN_SKILL"))
		self.assertEqual(strike["targetSlot"], {"name": "NONE", "ordinal": 7, "id": 128})
		self.assertEqual([(e["class"], e["duration2"], e["value"]) for e in strike["effects"]], [("SkillAttackInstantEffect", 0, 27)])
		self.assertEqual((strike["effectDuration"], strike["level"], strike["lvl"]), (0, 1, 1))

	def test_1282_flame_bolt(self):
		bolt = self.skill(self.mage, 1282)
		self.assertEqual((bolt["castDuration"], bolt["castSpeed"], bolt["allowAnimationBoost"]), (2000, 1.0, True), "the 2,000 ms cast bar of X4")
		self.assertEqual((bolt["mpCost"], bolt["cooldown"], bolt["cooldownMillis"]), (19, 0, 0))
		self.assertEqual([(e["class"], e["skillElement"], e["value"]) for e in bolt["effects"]], [("SpellAttackInstantEffect", "FIRE", 141)])
		self.assertEqual((bolt["chainCategory"], bolt["notModelled"]), ("M_CHAINA_1TH_1", []))

	def test_1328_root(self):
		root = self.skill(self.mage, 1328)
		self.assertEqual((root["castDuration"], root["mpCost"], root["cooldown"]), (0, 38, 600))
		self.assertEqual(root["targetSlot"], {"name": "DEBUFF", "ordinal": 1, "id": 2}, "the DEBUFF slot of X6")
		self.assertEqual([(e["class"], e["duration1"], e["duration2"]) for e in root["effects"]], [("RootEffect", 0, 20000)])
		self.assertEqual((root["effectDuration"], root["effectDurationRandomTime"]), (20000, 0), "the 20-second debuff of X6")
		self.assertEqual(root["effects"][0]["attributes"]["resistchance"], "10", "the 10 % that C8's silence is about")

	def test_8291_soul_sickness(self):
		sickness = self.skill(self.mage, 8291)
		self.assertEqual((sickness["activation"], sickness["method"], sickness["level"]), ("PROVOKED", "PROVOKED", 1))
		self.assertEqual(sickness["targetSlot"], {"name": "SPEC2", "ordinal": 4, "id": 16})
		self.assertEqual([(e["class"], e["duration1"], e["duration2"]) for e in sickness["effects"]], [("StatdownEffect", 20000, 40000)] * 3)
		self.assertEqual([[c["stat"] for c in e["changes"]] for e in sickness["effects"]], [["MAXHP"], ["MAXMP"], ["SPEED", "FLY_SPEED"]])
		self.assertEqual(sickness["effectDuration"], 60000, "40000 + 20000 * deathCount 1")
		self.assertEqual((sickness["castDuration"], sickness["mpCost"], sickness["cooldown"]), (0, 0, 0))

	def test_the_other_gate_skills_and_npc_210133(self):
		evasion = self.skill(self.mage, 3195)
		self.assertEqual([(e["class"], e["duration2"], e["position"]) for e in evasion["effects"]],
		                 [("AlwaysDodgeEffect", 5000, 1), ("AlwaysResistEffect", 5000, 2)])
		self.assertEqual((evasion["targetSlot"]["name"], evasion["effectDuration"], evasion["sources"]), ("BUFF", 5000, ["extra"]))
		heal = self.skill(self.mage, 1838)
		self.assertEqual((heal["castDuration"], heal["mpCost"], [e["class"] for e in heal["effects"]]), (2000, 13, ["HealInstantEffect"]))
		[npc] = self.mage["npcs"]
		self.assertEqual((npc["npcId"], npc["level"], npc["castSpeed"]), (210133, 1, 1000))
		self.assertEqual([(s["skillId"], s["level"], s["prob"], s["castDuration"]) for s in npc["skills"]], [(16419, 1, 25, 2500)])
		self.assertEqual(self.skill(self.mage, 16419)["effects"][0]["class"], "SkillAttackInstantEffect")

	def test_damage_over_time_on_the_real_data(self):
		# The two over time skills a level 1..9 starting character meets on the start maps (m5b2-plan.md §2.4): the Mage's 1447 Erosion,
		# autolearnt at minLevel 5 (skill_tree.xml:1584), and 17018 Bite of the level 8 scar 210306 (§2.4(b), its BleedEffect)
		report = skills_report(self.data, JAVA_SRC, "ELYOS", "MAGE", level=9, npc_ids=[210306])
		erosion = self.skill(report, 1447)
		self.assertEqual(erosion["sources"], ["autolearn"])
		self.assertEqual([(e["class"], e["duration2"], e["effectiveDuration2"]) for e in erosion["effects"]],
		                 [("SpellAttackInstantEffect", 0, 0), ("SpellAttackEffect", 15000, 16000)])
		self.assertEqual(erosion["effectDuration"], 16000,
		                 "skill_templates.xml: <spellatk checktime=3000 duration2=15000 e=2>, plus AbstractOverTimeEffect.getDuration2's second")
		bite = self.skill(report, 17018)
		self.assertEqual(bite["sources"], ["npc:210306"])
		self.assertEqual([(e["class"], e["position"], e["duration1"], e["duration2"]) for e in bite["effects"]],
		                 [("SkillAttackInstantEffect", 1, 0, 0), ("BleedEffect", 2, 300, 12100)])
		self.assertEqual(bite["effectDuration"], 13400, "<bleed duration2=12100 duration1=300> at npc skill level 1: 12100 + 1000 + 300 * 1")
		root = self.skill(report, 1328)
		self.assertEqual(root["effectDuration"], 20000, "a RootEffect is no over time effect: X6's 20 seconds stay exact")

	def test_the_gate_skills_launch_nothing(self):
		# npc 210133's only skill, 16419 Brandish, has no subeffect and no launcher; nor has anything a level 1 Mage or Warrior casts
		[npc] = self.mage["npcs"]
		self.assertEqual((npc["launchedSkills"], npc["effectClasses"]["addedByLaunches"]), ([], {"leaves": [], "withBases": []}))
		for report in (self.warrior, self.mage):
			self.assertEqual(report["effectClasses"]["addedByLaunches"], {"leaves": [], "withBases": []})
			self.assertEqual([s["skillId"] for s in report["skills"] if s["launches"] or s["launchedBy"]], [])

	def test_the_start_map_npcs_launch_stagger_stumble_and_blessing_of_rock(self):
		# m5b2-plan.md §10 "what remains": the closure adds 8217, 8218 and 16393 on the start maps. From skill_templates.xml:
		# 16742 Wide Thrust (:123086, <subeffect skill_id="8217"/> :123096), 16785 Knockback (:123777, :123787), 17720 Press Strike (:137554,
		# :137564) -> 8217 "Stunned", a <stagger duration1="2000"> (:79903-79912); 16422 Thrust (:118299, :118309) and 16460 Strike Chain Skill
		# (:118854, :118864) -> 8218 Stumble, a <stumble duration1="2000"> (:79916-79925); 16394 (:117903, <provoker skill_id="16393"
		# provoke_target="ME" hittype="EVERYHIT">, :117912) and 16856 (:124842, :124851) -> 16393 Blessing of Rock, lvl 1, a <statup
		# duration2="5000" duration1="100"> (:117888-117897). The npcs are npc_skills.xml:2881 (210162), :3259 (210407), :5637 (211284),
		# :3888 (210637).
		report = skills_report(self.data, JAVA_SRC, "ELYOS", "WARRIOR", npc_ids=[210162, 210407, 211284, 210637])
		launched = {npc["npcId"]: [(s["skillId"], s["level"]) for s in npc["launchedSkills"]] for npc in report["npcs"]}
		self.assertEqual(launched, {210162: [(8217, 1), (16393, 1)], 210407: [(8217, 1), (16393, 1)], 211284: [(8218, 1)], 210637: [(8217, 1)]})
		added = {npc["npcId"]: npc["effectClasses"]["addedByLaunches"]["leaves"] for npc in report["npcs"]}
		self.assertEqual(added, {210162: ["StaggerEffect", "StatupEffect"], 210407: ["StaggerEffect", "StatupEffect"], 211284: ["StumbleEffect"],
		                         210637: ["StaggerEffect"]})
		self.assertEqual(report["npcs"][0]["effectClasses"]["addedByLaunches"]["withBases"], ["BufEffect", "StaggerEffect", "StatupEffect"])
		stagger, stumble, blessing = self.skill(report, 8217), self.skill(report, 8218), self.skill(report, 16393)
		self.assertEqual(launcher_view(stagger["launchedBy"]), [(16742, 1, "subeffect", 1, 100), (16785, 1, "subeffect", 1, 100),
		                                                         (17720, 1, "subeffect", 1, 100)])
		self.assertEqual(launcher_view(stumble["launchedBy"]), [(16422, 1, "subeffect", 1, 100), (16460, 1, "subeffect", 1, 100)])
		self.assertEqual(launcher_view(blessing["launchedBy"]), [(16394, 1, "provoker", 1, 100), (16856, 1, "provoker", 1, 100)])
		self.assertEqual([(s["sources"], s["effects"][0]["class"], s["effectDuration"]) for s in (stagger, stumble, blessing)],
		                 [(["launch"], "StaggerEffect", 2000), (["launch"], "StumbleEffect", 2000), (["launch"], "StatupEffect", 5100)])
		[provoke] = self.skill(report, 16394)["launches"]
		self.assertEqual((provoke["observer"], provoke["provokeTarget"], provoke["radius"]), ("ATTACKED", "ME", 0),
		                 "EVERYHIT is no NMLATK or BACKATK: the observer is ATTACKED, so every hit on the npc may cast 16393 on the npc")

	def test_the_start_maps_closure_adds_exactly_three_skills(self):
		# every npc spawned on Poeta and Ishalgen (the spawn files' npc ids), their npc_skills lists, and the closure over all of them
		npc_ids = set()
		for spawns in ("210010000_Poeta.xml", "220010000_Ishalgen.xml"):
			npc_ids |= {int(spawn.get("npc_id")) for spawn in ET.parse(runner.DEFAULT_STATIC_DATA / "spawns" / "Npcs" / spawns).getroot().iter("spawn")}
		lists = npc_skill_lists(self.data, npc_ids)
		self.assertEqual(len(lists), 68, "m5b2-plan.md §2.4(b): 32 of Poeta's npc ids and 36 of Ishalgen's own skills")
		roots = {(row["skillId"], row["level"]) for rows in lists.values() for row in rows}
		self.assertEqual(len({skill for skill, _ in roots}), 29, "§2.4(b): the 29 distinct npc skill ids of the two maps")
		closure = LaunchClosure(self.data, JavaSkillRules.read(JAVA_SRC), load_skill_templates(self.data, {skill for skill, _ in roots}))
		self.assertEqual(sorted(closure.close(roots, roots) - roots), [(8217, 1), (8218, 1), (16393, 1)],
		                 "an npc casts its rows (Skill.useSkill), so their penalty skills count; none of the 29 has one")
		npc_closure = lambda rows: closure.close({(r["skillId"], r["level"]) for r in rows}, {(r["skillId"], r["level"]) for r in rows})  # noqa: E731
		self.assertEqual(sorted(npc for npc, rows in lists.items() if npc_closure(rows) - roots), [210162, 210407, 210409, 210637, 211284])
		self.assertEqual([edge for skill, _ in sorted(closure.close(roots, roots)) for edge in closure.edges[skill] if edge["kind"] == "penalty"], [])

	def test_the_mage_reaches_stagger_through_frozen_shock_at_level_7(self):
		# the player path: 1226 Frozen Shock is autolearnt at minLevel 7 (skill_tree.xml:1369) and its <spellatkinstant> carries
		# <subeffect skill_id="8217"/> (skill_templates.xml:17512, :17522)
		level6 = skills_report(self.data, JAVA_SRC, "ELYOS", "MAGE", level=6)
		self.assertEqual(level6["character"]["launchedSkills"], [])
		level7 = skills_report(self.data, JAVA_SRC, "ELYOS", "MAGE", level=7)
		self.assertEqual(level7["character"]["launchedSkills"], [{"skillId": 8217, "level": 1}])
		self.assertEqual(level7["character"]["effectClasses"]["addedByLaunches"], {"leaves": ["StaggerEffect"], "withBases": ["StaggerEffect"]})
		self.assertEqual(launcher_view(self.skill(level7, 8217)["launchedBy"]), [(1226, 1, "subeffect", 1, 100)])
		self.assertEqual(self.skill(level7, 1226)["launches"][0]["effect"]["class"], "SpellAttackInstantEffect")

	def test_penalty_skills_on_the_real_data(self):
		# Skill.startPenaltySkill (Skill.java:478-489). skill_templates.xml: 17551 Holy Wave (:135257) and 17846 Dark Force Cleave (:139275)
		# carry penalty_skill_id="16394", Blessing of Rock (:117903), whose provoker launches 16393 (:117912); npc 212346 casts 17846 at lv 34
		# (npc_skills.xml:9286-9291). 3019 Empyrean Chastisement (:50824) -> 9042 Razorlight Veil (:90377) without the message; 2171
		# Crosstrigger (:34262) -> 8938 (:89178) with penalty_skill_send_msg="true", so at level 1 through PenaltySkill. Before the penalty
		# kind, this report left 16394, 9042 and 8938 out without a word.
		report = skills_report(self.data, JAVA_SRC, "ELYOS", "WARRIOR", extra_skills=["17551", "3019", "2171"], npc_ids=[212346])
		[npc] = report["npcs"]
		self.assertEqual(npc["launchedSkills"], [{"skillId": 8218, "level": 1}, {"skillId": 16393, "level": 1}, {"skillId": 16394, "level": 1}])
		self.assertEqual(launcher_view(self.skill(report, 16394)["launchedBy"]), [(17551, 1, "penalty", None, 100), (17846, 34, "penalty", None, 100)])
		self.assertIn((16394, 1, "provoker", 1, 100), launcher_view(self.skill(report, 16393)["launchedBy"]))
		penalties = {s: [(e["skillId"], e["level"], e["sendMsg"], e["effector"]) for e in self.skill(report, s)["launches"] if e["kind"] == "penalty"]
		             for s in (17551, 3019, 2171)}
		self.assertEqual(penalties, {17551: [(16394, 1, False, "firstTarget")], 3019: [(9042, 1, False, "firstTarget")],
		                             2171: [(8938, 1, True, "caster")]})
		self.assertEqual([(self.skill(report, s)["sources"], self.skill(report, s)["effects"][0]["class"]) for s in (9042, 8938)],
		                 [(["launch"], "ShieldEffect"), (["launch"], "MPHealInstantEffect")])

	def test_the_command_line(self):
		out = io.StringIO()
		with contextlib.redirect_stdout(out):
			code = oracle.main(["m5b2-skills", "--race", "ELYOS", "--class", "MAGE", "--skill", "8291:2", "--death-count", "2"])
		self.assertEqual(code, 0)
		answer = json.loads(out.getvalue())
		self.assertEqual((answer["race"], answer["playerClass"], answer["level"]), ("ELYOS", "MAGE", 1))
		[sickness] = [s for s in answer["skills"] if s["skillId"] == 8291]
		self.assertEqual((sickness["level"], sickness["sources"], sickness["effectDuration"]), (2, ["extra", "soulSickness"], 80000))
		err = io.StringIO()
		with contextlib.redirect_stderr(err), contextlib.redirect_stdout(io.StringIO()):
			self.assertEqual(oracle.main(["m5b2-skills", "--race", "ELYOS", "--class", "SORCERER"]), 2, "OracleError: not a starting class")
		self.assertIn("starting class", err.getvalue())


if __name__ == "__main__":
	unittest.main()
