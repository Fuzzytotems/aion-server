"""M5b-2 skills oracle (m5b2/skills.py, m5b2-plan.md G-01): the formulas alone, the Java tables as they stand today, the whole report on a small
static_data tree, and the four skill ids the M5b-2 gate asserts on the real data (2864 Ferocious Strike, 1282 Flame Bolt, 1328 Root and the
revive debuff 8291), plus the class-less autolearn rows (243 Return, 245 Bandage Heal, 302 Escape) that the plan's first inventory missed.

Expected values are derived by hand from the Java sources named in m5b2/skills.py and repeated per case. Everything that reads the Java source
tree is skipped without it, like M5bFixtureReportTest.
"""

import contextlib
import io
import json
import unittest

from m5a.creation import JavaEnums, creation_report, learn_new_skills
from m5a.data import StaticData
from m5a.javafloat import f32
from m5b2.skills import (JavaSkillRules, _condition_cost, effect_duration, magical_cast_duration_without_functions, npc_cast_duration,
                         skills_report)
from staticdata_oracle import OracleError
from staticdata_oracle import run as runner

import oracle

from .support import Tree

JAVA_SRC = runner.TOOL_DIR.parents[2] / "game-server" / "src"
HAVE_JAVA_TREE = (JAVA_SRC / "com" / "aionemu" / "gameserver").is_dir() and runner.DEFAULT_STATIC_DATA.is_dir()


def effect(position, duration1=0, duration2=0, random_time=0):
	return {"position": position, "duration1": duration1, "duration2": duration2, "randomTime": random_time}


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

	def test_condition_costs(self):
		# MpCondition.getCost / HpCondition.getCost: value + delta * skillLevel; a ratio is a percentage of the max, which is not static data
		self.assertEqual(_condition_cost("mp", {"value": "19", "delta": "0"}, 1, 1282), {"value": 19, "delta": 0, "ratio": False, "cost": 19})
		self.assertEqual(_condition_cost("mp", {"value": "10", "delta": "3"}, 4, 1), {"value": 10, "delta": 3, "ratio": False, "cost": 22})
		self.assertEqual(_condition_cost("hp", {"value": "5"}, 2, 1), {"value": 5, "delta": 0, "ratio": False, "cost": 5})
		self.assertEqual(_condition_cost("mp", {"value": "10", "ratio": "true"}, 1, 1)["cost"], None)
		self.assertEqual(_condition_cost("dp", {"value": "7"}, 1, 1), {"value": 7}, "DpCondition only compares")
		with self.assertRaises(OracleError):
			_condition_cost("mp", {}, 1, 1)  # value is required


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

	def test_target_slots_soul_sickness_and_the_cast_cap(self):
		# SkillTargetSlot(id) in declaration order: the ordinal is what SM_ABNORMAL_STATE writes, the id is the slot mask
		self.assertEqual(self.rules.target_slots, {"BUFF": (0, 1), "DEBUFF": (1, 2), "CHANT": (2, 4), "SPEC": (3, 8), "SPEC2": (4, 16),
		                                           "BOOST": (5, 32), "NOSHOW": (6, 64), "NONE": (7, 128)})
		self.assertEqual((self.rules.soul_sickness_skill, self.rules.soul_sickness_max_death_count), (8291, 10))
		self.assertEqual(self.rules.cast_duration_cap, f32(0.25))


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

NPC_SKILLS = (
	'<npc_skills npc_ids="900001 900002"><npc_skill id="1020" lv="3" prob="25"/></npc_skills>'
	'<npc_skills npc_ids="900001"><npc_skill id="1002" lv="9" prob="100" is_post_spawn="true"/></npc_skills>'  # the second list of 900001
)

NPCS = ('<npc_template npc_id="900001" level="3" name="fixture caster" cast_speed="1100"><stats maxHp="10"/></npc_template>'
        '<npc_template npc_id="900002" level="4" name="fixture default"><stats maxHp="10"/></npc_template>'
        '<npc_template npc_id="900003" level="5" name="fixture silent"><stats maxHp="10"/></npc_template>')


def fixture_tree(item='<item_template id="500" name="fixture sword" item_group="SWORD"><weapon_stats attack_range="1500"/></item_template>',
                 item_sets='<itemset id="1"><itempart itemid="1"/></itemset>', passive_change="PHYSICAL_ATTACK", passive_tag="statboost"):
	tree = Tree()
	skills = SKILLS.replace('<change stat="PHYSICAL_ATTACK"', f'<change stat="{passive_change}"').replace("<statboost e=", f"<{passive_tag} e=") \
		.replace("</statboost>", f"</{passive_tag}>")
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
