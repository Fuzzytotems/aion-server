"""M5c craft oracle (m5c/craft.py, m5c-plan.md §2.6 and G-01): the Java arithmetic alone, the Java bodies and tables as they stand today and on
an edited copy, the configuration layers, the whole report on a small static_data tree, and what a new character can gather in Poeta and
Ishalgen plus the gate's recipe 155001381 (Roast Inina) on the real data.

Expected values are derived by hand from the Java sources named in m5c/craft.py and repeated per case (Java float arithmetic where the Java is
float). Everything that reads the Java source tree is skipped without it, like M5b2FixtureReportTest. The report tests use a distinct rate array
per key at membership 1 and a level difference of 24 (0 < diff < 41) so that a key read for another, or a formula without its diff term or cap,
cannot pass; the shape tests include every edit of the M5c craft review that the method-by-method check accepted (now refused by the class pins).
"""

import contextlib
import dataclasses
import io
import json
import shutil
import tempfile
import unittest
from fractions import Fraction
from pathlib import Path

from m5a.data import StaticData
from m5a.javafloat import f32
from m5c.craft import (CraftContext, _int, add_skill_xp, chance_below, craft_bonus_modifier, craft_failure_step, craft_report, craft_speed,
                       craft_success_step, craft_xp_reward, fail_reduction, gained_skill_xp, gather_failure_step, gather_success_step,
                       gather_xp_reward, material_alternatives, material_rolls, next_down, player_exp, rates_calc_result_int, required_exp,
                       steps_to_fill)
from m5c.craft_config import load_craft_config, membership_rate
from m5c.craft_java import JavaCraftRules, _compile, normalize
from staticdata_oracle import OracleError
from staticdata_oracle import run as runner

import oracle

from .support import Tree

JAVA_SRC = runner.TOOL_DIR.parents[2] / "game-server" / "src"
CONFIG_DIR = runner.TOOL_DIR.parents[2] / "game-server" / "config"
HAVE_JAVA_TREE = (JAVA_SRC / "com" / "aionemu" / "gameserver").is_dir() and runner.DEFAULT_STATIC_DATA.is_dir()
NO_EVENTS = "gameserver.event.service.disabled_events=*"
GATE_PROFILE = [NO_EVENTS, "gameserver.craft.fail.chance=0", "gameserver.rates.crafting.crit_chances=0, 0"]  # m5c-plan.md D6
TOP = next_down(f32(2.0))  # the largest Rnd.nextFloat(1f, 2f)

# The literals of the modelled Java bodies as they stand today (CraftingTask.analyzeInteraction, GatheringTask.analyzeInteraction, ...), for the
# formula tests that run without the Java tree; M5cJavaRulesTest checks that JavaCraftRules.read finds exactly these.
CRAFT = {"xpFactor": 0.008, "xpOffset": 100, "xpBase": 60, "bonusDivisor": 100, "multiMin": 1.0, "multiMax": 2.0,
         "failReductionStep": f32(0.015), "failReductionMin": 0.25, "bmLegend": f32(0.9), "bmUnique": f32(0.7), "bmEpic": 0.5, "bmMythic": f32(0.3),
         "successMinStep": 70, "lvlBoniFrom": 10, "lvlBoniFactor": 2, "blueBonus": 100, "halfDivisor": 2.0, "stepFactor": 10,
         "failMinStepLimited": 70, "failMinStep": 120, "failDivisor": 1.5, "failFactor": 10, "speedBase": 900, "speedStep": 30, "speedMin": 300,
         "delayBase": 1200, "delayMin": 500, "delayStep": 30}
GATHER = {"rewardXpFactor": 0.0031, "rewardXpOffset": 5.3, "rewardXpOffset2": 1592.8, "rewardXpBase": 60, "successMinStep": 70, "lvlBoniFrom": 10,
          "lvlBoniFactor": 2, "blueBonus": 100, "halfDivisor": 2.0, "stepFactor": 10, "failMinStep": 120, "failDivisor": 2.0, "failFactor": 10,
          "failReductionStep": f32(0.015), "failReductionMin": 0.25}
SKILL = {"maxGap": 40, "humanSkill": 30001, "humanCap": 49, "tapA": 30002, "tapB": 30003, "tapFree": 449, "tapCap": 499,
         "craftCapSkills": [40001, 40002, 40003, 40004, 40007, 40008, 40010], "capLevels": [99, 199, 299, 399, 449, 499, 549], "lvlFactor": 0.23,
         "lvlOffset": 17.2}


class M5cCraftFormulaTest(unittest.TestCase):
	"""The arithmetic of the report, without any Java source or static data."""

	def test_craft_xp_and_the_bonus(self):
		# finishCrafting: (int) (0.008 * (skillLvl + 100) * (skillLvl + 100) + 60) in double, then + xpReward * bonus / 100 in int
		self.assertEqual(craft_xp_reward(CRAFT, 1, 0), (141, 141), "0.008 * 101 * 101 + 60 = 141.608")
		self.assertEqual(craft_xp_reward(CRAFT, 1, 15), (141, 162), "craft type 1: 141 + 2115 / 100 = 141 + 21")
		self.assertEqual(craft_xp_reward(CRAFT, 30, 0)[0], 195, "0.008 * 130 * 130 + 60 = 195.2")
		self.assertEqual(craft_xp_reward(CRAFT, 100, 0)[0], 380, "0.008 * 200 * 200 + 60 = 380, exactly")

	def test_gather_xp(self):
		# rewardPlayer: (int) ((0.0031 * (skillLvl + 5.3) * (skillLvl + 1592.8) + 60)) in double
		self.assertEqual(gather_xp_reward(GATHER, 1), 91, "0.0031 * 6.3 * 1593.8 + 60 = 91.13")
		self.assertEqual(gather_xp_reward(GATHER, 10), 136, "Mela Sapling: 0.0031 * 15.3 * 1602.8 + 60 = 136.02")
		self.assertEqual(gather_xp_reward(GATHER, 15), 161, "Impure Iron Ore: 0.0031 * 20.3 * 1607.8 + 60 = 161.18")

	def test_required_exp(self):
		# addSkillXp: (int) (0.23 * (skillLvl + 17.2) * (skillLvl + 17.2))
		self.assertEqual(required_exp(1, 0.23, 17.2), 76, "0.23 * 18.2^2 = 76.19")
		self.assertEqual(required_exp(2, 0.23, 17.2), 84)
		self.assertEqual(required_exp(30, 0.23, 17.2), 512)
		self.assertEqual(required_exp(48, 0.23, 17.2), 977)

	def test_add_skill_xp_caps_gap_and_level_up(self):
		rules = JavaCraftRules(skill=SKILL)
		up = add_skill_xp(rules, 40001, 1, 0, 141, 1, False)
		self.assertEqual((up["granted"], up["levelAfter"], up["currentXpAfter"], up["requiredExp"]), (True, 2, 0, 76),
		                 "141 >= 76: one craft makes cooking 1 -> 2 and drops the other 65 xp (setCurrentXp(0))")
		below = add_skill_xp(rules, 40001, 1, 10, 60, 1, False)
		self.assertEqual((below["levelAfter"], below["currentXpAfter"]), (1, 70), "10 + 60 < 76: the xp is kept")
		self.assertEqual(add_skill_xp(rules, 40001, 1, 16, 60, 1, False)["levelAfter"], 2, "16 + 60 == 76 levels up (>=)")
		self.assertFalse(add_skill_xp(rules, 40001, 45, 0, 100, 4, False)["granted"], "45 - 4 > 40: no xp")
		self.assertTrue(add_skill_xp(rules, 40001, 44, 0, 100, 4, False)["granted"], "44 - 4 == 40 is still granted")
		for skill, level, granted in ((40001, 99, False), (40001, 100, True), (40010, 549, False), (30001, 49, False), (30001, 99, False),
		                              (30002, 449, True), (30002, 499, False), (30003, 399, False), (40009, 99, True), (40005, 99, True),
		                              (30001, 48, True)):
			with self.subTest(skill=skill, level=level):
				self.assertEqual(add_skill_xp(rules, skill, level, 0, 1, level, False)["granted"], granted,
				                 "the fallthrough switch: 30001 is capped at 49 and then falls through the tapping and crafting arms; tapping skips "
				                 "the crafting caps at 449 only; 40009 (morph) and the unused 40005 are in no arm")
		self.assertTrue(add_skill_xp(rules, 30002, 499, 0, 1, 499, True)["granted"], "gameserver.craft.disable.tapping.cap lifts the 499 cap")
		self.assertTrue(add_skill_xp(rules, 30002, 520, 0, 1, 520, True)["granted"])
		self.assertTrue(add_skill_xp(rules, 40001, 520, 0, 1, 520, True)["granted"], "the tapping flag does not concern crafting skills at 520")

	def test_rates_and_boost(self):
		# Rates.calcResult(int): (long) (value * rate) - a long times a float is float - then Math.toIntExact, which answers the input on overflow
		self.assertEqual(rates_calc_result_int(141, f32(1.5)), 211, "141 * 1.5f = 211.5 -> 211")
		self.assertEqual(rates_calc_result_int(141, f32(0.5)), 70)
		self.assertEqual(rates_calc_result_int(1, f32(0.5)), 0, "GATHERING_COUNT at rate 0.5 adds 0 items")
		self.assertEqual(rates_calc_result_int(2**31 - 1, f32(2.0)), 2**31 - 1, "the overflow answers the value itself")
		self.assertEqual(gained_skill_xp(141, f32(1.0), True, 100, f32(100.0), 1), 141)
		self.assertEqual(gained_skill_xp(1, f32(0.5), True, 100, f32(100.0), 1), 1, "Math.max(1, 0)")
		self.assertEqual(gained_skill_xp(3, f32(0.5), False, 100, f32(100.0), 1), 1, "(long) (3 * 0.5f) = 1, no boost for the morph skill")
		self.assertEqual(player_exp(141, f32(2.0), 100, f32(100.0)), 282, "membership 1 of the default rates '1.0, 2.0'")

	def test_membership_rate(self):
		# Rates.get: rates[min(length - 1, membership)], 1 for an empty array
		self.assertEqual(membership_rate([1.0, 2.0], 0), 1.0)
		self.assertEqual(membership_rate([1.0, 2.0], 1), 2.0)
		self.assertEqual(membership_rate([1.0, 2.0], 9), 2.0)
		self.assertEqual(membership_rate([], 0), 1.0)

	def test_craft_steps_with_float_rounding(self):
		# analyzeInteraction: Math.round(70 + ((int) (base * multi)) * bonusModifier), base = (blue ? 100 : 0) + ((diff + 1) / 2f + lvlBoni) * 10
		self.assertEqual(TOP, 1.9999998807907104)
		self.assertEqual([craft_success_step(CRAFT, 0, False, m, 1.0) for m in (1.0, TOP)], [75, 79], "(int) (5 * 1.9999999f) is 9, not 10")
		self.assertEqual([craft_success_step(CRAFT, 0, True, m, 1.0) for m in (1.0, TOP)], [175, 279])
		self.assertEqual([craft_success_step(CRAFT, 0, False, m, f32(0.7)) for m in (1.0, TOP)], [74, 76],
		                 "UNIQUE: 5 * 0.7f = 3.4999999, but 70 + 3.4999999 is the float 73.5, and Math.round(73.5f) is 74")
		self.assertEqual([craft_success_step(CRAFT, 0, True, m, f32(0.3)) for m in (1.0, TOP)], [102, 133], "MYTHIC: 70 + 105 * 0.3f = 101.5 -> 102")
		self.assertEqual([craft_failure_step(CRAFT, 0, False, m, 1.0) for m in (1.0, TOP)], [126, 133], "120 + (int) (6.666667f * multi)")
		self.assertEqual([craft_failure_step(CRAFT, 0, True, m, f32(0.7)) for m in (1.0, TOP)], [74, 79], "a max_production_count recipe: 70 + ...")
		self.assertEqual([craft_success_step(CRAFT, 20, False, m, 1.0) for m in (1.0, TOP)], [375, 679],
		                 "diff 20: lvlBoni 20, base 305; 305 * 1.9999999f = 609.99994f, (int) -> 609")
		self.assertEqual([gather_success_step(GATHER, 20, False, m) for m in (1.0, TOP)], [375, 680],
		                 "gathering rounds 70 + 609.99994f instead of truncating it first: 680")
		self.assertEqual([gather_success_step(GATHER, 0, False, m) for m in (1.0, TOP)], [75, 80], "Math.round(79.9999995f) = 80")
		self.assertEqual([gather_success_step(GATHER, 0, True, m) for m in (1.0, TOP)], [175, 280])
		self.assertEqual([gather_failure_step(GATHER, 0, m) for m in (1.0, TOP)], [125, 130])

	def test_bonus_modifier_speed_and_bar_delay(self):
		self.assertEqual([craft_bonus_modifier(CRAFT, q) for q in ("JUNK", "COMMON", "RARE", "LEGEND", "UNIQUE", "EPIC", "MYTHIC")],
		                 [1.0, 1.0, 1.0, f32(0.9), f32(0.7), 0.5, f32(0.3)])
		self.assertEqual(craft_speed(CRAFT, 0, 1.0), (900, 1200))
		self.assertEqual(craft_speed(CRAFT, 10, 1.0), (600, 900))
		self.assertEqual(craft_speed(CRAFT, 30, 1.0), (300, 500), "900 - 900 is capped at 300, 1200 - 900 at 500")
		self.assertEqual(craft_speed(CRAFT, 0, f32(0.7)), (1170, 1200), "Math.round(900 * (2 - 0.7f)); the bar delay is 1200 below quality 1")
		self.assertEqual(craft_speed(CRAFT, 40, f32(0.3)), (1530, 1200), "the level difference does not matter below quality 1")

	def test_fail_reduction_and_chances(self):
		self.assertEqual(fail_reduction(CRAFT, 0), 1.0)
		self.assertEqual(fail_reduction(CRAFT, 20), f32(0.7000000476837158), "1 - 20 * 0.015f in float")
		self.assertEqual(fail_reduction(CRAFT, 60), 0.25, "Math.max(..., 0.25f)")
		self.assertEqual(chance_below(33.0), Fraction(33, 100))
		self.assertEqual(chance_below(0.0), 0)
		self.assertEqual(chance_below(-5.0), 0)
		self.assertEqual(chance_below(150.0), 1, "Rnd.chance() is below 100, so a chance of 100 or more always hits")
		self.assertEqual(chance_below(f32(15.333333)), Fraction(f32(15.333333)) / 100, "the exact value of the float threshold")

	def test_material_rolls(self):
		# startGathering: chance = Rnd.nextInt(10000000); the first material whose running sum is >= chance
		self.assertEqual(material_rolls([{"itemId": 1, "rate": 10000000}], 10000000), ([10000000], 0))
		self.assertEqual(material_rolls([{"itemId": 1, "rate": 7000000}, {"itemId": 2, "rate": 3000000}], 10000000), ([7000001, 2999999], 0),
		                 "the first material also wins chance 0, the last one never sees chance 10000000")
		self.assertEqual(material_rolls([{"itemId": 1, "rate": 0}, {"itemId": 2, "rate": 10000000}], 10000000), ([1, 9999999], 0),
		                 "a rate of 0 still wins chance 0 when it comes first")
		self.assertEqual(material_rolls([{"itemId": 1, "rate": 5000000}, {"itemId": 2, "rate": 4000000}], 10000000), ([5000001, 4000000], 999999),
		                 "rates below the bound leave chances that select nothing (a null material in Java)")
		with self.assertRaises(OracleError):
			material_rolls([{"itemId": 1, "rate": -1}], 10000000)
		with self.assertRaises(OracleError):
			material_rolls([{"itemId": 1, "rate": 2**31 - 1}, {"itemId": 2, "rate": 1}], 10000000)

	def test_material_alternatives(self):
		# checkCraft: the first alternative whose FIRST item the client names is taken; each component is compared alone with the whole stack, then
		# each gets its own decreaseByItemId (which takes what is left and whose false return is ignored)
		alternatives = material_alternatives([[(10, 10), (11, 2), (10, 7)], [(10, 1)], [(11, 3)]])
		self.assertEqual([(a["index"], a["firstItemId"], a["selectable"], a["shadowedBy"]) for a in alternatives],
		                 [(0, 10, True, None), (1, 10, False, 0), (2, 11, True, None)], "alternative 1 starts with item 10 like alternative 0")
		self.assertEqual(alternatives[0]["items"], [{"itemId": 10, "required": 10, "consumed": 17, "components": 2},
		                                            {"itemId": 11, "required": 2, "consumed": 2, "components": 1}],
		                 "10 x10 and 10 x7: 10 held pass both checks (10 >= 10, 10 >= 7), and the two decreases take min(held, 17)")
		self.assertEqual(alternatives[1]["components"], [{"itemId": 10, "quantity": 1}])

	def test_strict_int(self):
		# an xs:int attribute: ASCII digits with an optional sign (Python's int() would also take '1_0' and other scripts' digits)
		self.assertEqual((_int("12", "x"), _int(" -3 ", "x"), _int("+7", "x"), _int(None, "x", 5)), (12, -3, 7, 5))
		for text in ("1_0", "١٢", "0x10", "1.0", "", " 12"):
			with self.subTest(text=ascii(text)), self.assertRaises(OracleError):
				_int(text, "x")

	def test_steps_to_fill(self):
		self.assertEqual(steps_to_fill(1000, 75, 279), (4, 14))
		self.assertEqual(steps_to_fill(1000, 125, 130), (8, 8))
		self.assertEqual(steps_to_fill(1000, 1000, 1000), (1, 1))
		with self.assertRaises(OracleError):
			steps_to_fill(1000, 0, 10)

	def test_template_matching(self):
		# the canonical text ignores layout and comments but keeps literals, and a placeholder is typed
		self.assertEqual(normalize("int  a = b /* c */ + 1; // d\n return  x;")[0], "int a=b+1;return x;")
		self.assertEqual(normalize('log("a  //b"  );')[0], 'log("a  //b");')
		regex, _, kinds = _compile("int x = (int) ($d:factor * (y + $offset)) + $f:scale * $offset;")
		match = regex.fullmatch(normalize("int x=(int)(0.008*(y+100))+2f*100;")[0])
		self.assertEqual((match.group("factor"), match.group("offset"), match.group("scale")), ("0.008", "100", "2f"))
		self.assertEqual(kinds, {"factor": "d", "offset": "i", "scale": "f"})
		for changed in ("int x=(int)(0.008f*(y+100))+2f*100;", "int x=(int)(0.008*(y+100))+2*100;", "int x=(int)(0.008*(y+100))+2f*101;",
		                "int x=(int)(0.008*(y+5.5f))+2f*5.5f;"):
			with self.subTest(changed=changed):
				self.assertIsNone(regex.fullmatch(changed), "a literal of another type, or a repeated literal that differs, does not match")


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5cJavaRulesTest(unittest.TestCase):
	"""The literals and tables JavaCraftRules reads from the Java sources, as they stand today."""

	@classmethod
	def setUpClass(cls):
		cls.rules = JavaCraftRules.read(JAVA_SRC)

	def test_the_formula_literals_are_the_ones_the_formula_tests_use(self):
		for key, value in CRAFT.items():
			self.assertEqual(self.rules.craft[key], value, key)
		for key, value in GATHER.items():
			self.assertEqual(self.rules.gather[key], value, key)
		for key, value in SKILL.items():
			self.assertEqual(self.rules.skill[key], value, key)

	def test_task_timing_and_ranges(self):
		c, g = self.rules.craft, self.rules.gather
		self.assertEqual((c["delay"], c["fullBar"], c["intervalBase"], c["intervalStep"], c["capDefault"], c["capUniqueEpic"], c["capMythic"]),
		                 (1000, 1000, 2500, 60, 1200, 1500, 1700))
		self.assertEqual((c["morphSkill"], c["morphInterval"], c["morphUnk"], c["packetMorphDelay"]), (40009, 200, 129, 1000))
		self.assertEqual((c["stationRange"], c["packetRange"], c["bonusCraftType"], c["bonusPercent"], c["houseBonus"]), (5, 10, 1, 15, 5))
		self.assertEqual((c["alwaysSuccessDiff"], c["blueBase"], c["blueDivisor"]), (41, 15, 3.0))
		self.assertEqual((g["startDelayMin"], g["startDelayMax"], g["intervalBase"], g["intervalStep"], g["intervalMin"]), (200, 600, 2500, 60, 1200))
		self.assertEqual((g["instantDiff"], g["fastSpeed"], g["fastDelay"], g["purpleBase"], g["purpleDivisor"], g["blueBase"]),
		                 (41, 300, 500, 1, 10.0, 5))
		self.assertEqual((g["gatherRange"], g["rollBound"], g["gatherCount"], g["checkEquipped"], g["checkInventory"]), (3, 10000000, 1, 1, 2))
		self.assertEqual((self.rules.player_bound, self.rules.object_bound), (0.25, 0.0))
		self.assertEqual(c["progress"], {"NORMAL": 1, "CRIT_BLUE": 2, "CRIT_PURPLE": 3})
		self.assertEqual(c["actions"], {"init": 0, "proc": 3, "start": 1, "success": 5, "failure": 6, "cancel": 4, "abort": 4})
		self.assertEqual(g["actions"], {"init": 0, "start": 1, "success": 6, "failurePre": 1, "failure": 7, "abort": 5, "occupied": 8})

	def test_the_tables(self):
		self.assertEqual(self.rules.bonus_items, {40001: 169401081, 40002: 169401076, 40003: 169401077, 40004: 169401078, 40007: 169401080,
		                                          40008: 169401079, 40010: 169401082})
		self.assertEqual(self.rules.upgrade_costs, {0: 3500, 99: 17000, 199: 115000, 299: 460000})
		self.assertEqual((self.rules.upgrade_cost_crafting_only, self.rules.max_upgradable, self.rules.crafting_range),
		                 ((449, 6004900), (499, 399), (40001, 40010)))
		self.assertEqual(self.rules.professions, {"ESSENCETAPPING": 30002, "AETHERTAPPING": 30003, "COOKING": 40001, "WEAPONSMITHING": 40002,
		                                          "ARMORSMITHING": 40003, "TAILORING": 40004, "ALCHEMY": 40007, "HANDICRAFTING": 40008,
		                                          "CONSTRUCTION": 40010})
		self.assertEqual(len(self.rules.profession_by_npc), 36)
		self.assertEqual(self.rules.profession_by_npc[203784], "COOKING", "Hestia, the Sanctum cooking master of m5c-plan.md §2.10")
		self.assertEqual(self.rules.boost_stats[30001], "BOOST_ESSENCETAPPING_XP_RATE", "human gathering shares the essence tapping boost")
		self.assertNotIn(40009, self.rules.boost_stats, "no boost for morphing")
		self.assertEqual(self.rules.skill["masterMinLevel"], 10)
		self.assertEqual(self.rules.rates, {"boostBase": 100, "boostDivisor": 100.0, "legionBonus": f32(1.1), "noExpWorld": 301200000})

	def test_config_fields_and_the_verified_list(self):
		fields = self.rules.config_fields
		self.assertEqual(fields[("CraftConfig", "MAX_CRAFT_FAILURE_CHANCE")], ("gameserver.craft.fail.chance", "33", "int"))
		self.assertEqual(fields[("CraftConfig", "MAX_GATHER_FAILURE_CHANCE")], ("gameserver.gather.fail.chance", "33", "int"))
		self.assertEqual(fields[("RatesConfig", "CRAFT_CRIT_CHANCES")], ("gameserver.rates.crafting.crit_chances", "15.0, 30.0", "float[]"))
		self.assertEqual(fields[("RatesConfig", "CRAFT_COMBO_CHANCES")], ("gameserver.rates.crafting.combo_crit_chances", "25.0, 50.0", "float[]"))
		self.assertEqual(fields[("EventsConfig", "DISABLED_EVENTS")], ("gameserver.event.service.disabled_events", None, "Set<String>"))
		methods = {v.method for v in self.rules.verified}
		for name in ("CraftService.finishCrafting", "CraftService.checkCraft", "CraftingTask.analyzeInteraction", "CraftingTask.calculateCrit",
		             "GatheringTask.analyzeInteraction", "GatherableController.startGathering", "PlayerSkillList.addSkillXp", "Profession.getUpgradeCost",
		             "StatEnum.getModifier", "Rates.SKILL_XP_CRAFTING", "PlayerCommonData.addExp", "ItemTemplate (no getBoundRadius() override)"):
			self.assertIn(name, methods)
		finish = next(v for v in self.rules.verified if v.method == "CraftService.finishCrafting")
		self.assertEqual(finish.as_json()["fileLine"], "services/craft/CraftService.java:43-95")
		for name in ("CraftingTask (every member and the header pinned)", "GatheringTask (every member and the header pinned)",
		             "AbstractCraftTask (every member and the header pinned)", "AbstractInteractionTask (every member and the header pinned)",
		             "GatherableController (every member and the header pinned)", "RecipeTemplate (every member and the header pinned)",
		             "GatherableTemplate (every member and the header pinned)", "CraftingTask.onInteractionFinish", "AbstractCraftTask.<init>",
		             "AbstractInteractionTask.<init>", "RecipeTemplate.getComponents", "GatherableTemplate.getHarvestCount", "PositionUtil.isInRange",
		             "PacketSendUtility.broadcastPacket", "Storage.decreaseByItemId", "Item.decreaseItemCount", "SkillLearnService.sendPacket"):
			self.assertIn(name, methods)
		ranges = sorted(v.as_json()["fileLine"] for v in self.rules.verified if v.method == "PositionUtil.isInRange")
		self.assertEqual(ranges, ["utils/PositionUtil.java:235-237", "utils/PositionUtil.java:243-251", "utils/PositionUtil.java:257-262"],
		                 "the 3-, 4- and 7-argument isInRange")

	def test_the_skill_learn_literals(self):
		s = self.rules.skill
		self.assertEqual((s["levelUpAnimation"], s["levelUpAnimationCraftingOnly"], s["nearbyQuestLevels"]),
		                 ([1, 100, 200, 300, 400, 450, 500], 1, [399, 499]))
		self.assertEqual(s["skillListMessage"], {"tapping": 1330005, "other": 1330064})


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5cJavaShapeTest(unittest.TestCase):
	"""JavaCraftRules.read on a copy of the Java files it reads, one of them changed: literals are read, every other change is refused."""

	BASE = ("com", "aionemu", "gameserver")

	@classmethod
	def setUpClass(cls):
		# one copy of every file JavaCraftRules reads; each case changes one file and puts it back
		cls.tmp = tempfile.TemporaryDirectory()
		cls.root = Path(cls.tmp.name)
		for rel in sorted({v.file for v in JavaCraftRules.read(JAVA_SRC).verified}):
			target = cls.root.joinpath(*cls.BASE, *rel.split("/"))
			target.parent.mkdir(parents=True, exist_ok=True)
			shutil.copyfile(JAVA_SRC.joinpath(*cls.BASE, *rel.split("/")), target)

	@classmethod
	def tearDownClass(cls):
		cls.tmp.cleanup()

	def rules_with(self, relative: str, old: str, new: str) -> JavaCraftRules:
		changed = self.root.joinpath(*self.BASE, *relative.split("/"))
		original = changed.read_bytes()
		text = original.decode("utf-8").replace("\r\n", "\n")
		self.assertIn(old, text, f"{relative} no longer contains the text this case changes")
		changed.write_bytes(text.replace(old, new, 1).encode("utf-8"))
		try:
			return JavaCraftRules.read(self.root)
		finally:
			changed.write_bytes(original)

	def test_literals_are_read(self):
		rules = self.rules_with("services/craft/CraftService.java", "(int) ((0.008 *", "(int) ((0.009 *")
		self.assertEqual(craft_xp_reward(rules.craft, 1, 0)[0], 151, "0.009 * 101 * 101 + 60 = 151.8")
		rules = self.rules_with("skillengine/task/CraftingTask.java", "float failReduction = Math.max(1 - skillLvlDiff * 0.015f, 0.25f);",
		                        "float failReduction = Math.max(1 - skillLvlDiff * 0.02f, 0.25f);")
		self.assertEqual(rules.craft["failReductionStep"], f32(0.02))
		rules = self.rules_with("controllers/GatherableController.java", "PositionUtil.isInRange(getOwner(), player, 3, false)",
		                        "PositionUtil.isInRange(getOwner(), player, 4, false)")
		self.assertEqual(rules.gather["gatherRange"], 4)
		rules = self.rules_with("model/stats/container/StatEnum.java", "default -> null;", "case 40009 -> BOOST_COOKING_XP_RATE;\n\t\t\tdefault -> null;")
		self.assertEqual(rules.boost_stats[40009], "BOOST_COOKING_XP_RATE", "a table row is read, not assumed")
		rules = self.rules_with("services/craft/CraftSkillUpdateService.java", "professionByNpc.put(203784, Profession.COOKING);",
		                        "professionByNpc.put(203784, Profession.ALCHEMY);")
		self.assertEqual(rules.profession_by_npc[203784], "ALCHEMY")
		rules = self.rules_with("services/SkillLearnService.java", "isNew ? 1330061 : 1330064", "isNew ? 1330061 : 1330065")
		self.assertEqual(rules.skill["skillListMessage"]["other"], 1330065)
		rules = self.rules_with("services/SkillLearnService.java", "case 1, 100, 200, 300, 400, 450, 500 ->", "case 1, 100, 200, 300, 400, 450, 550 ->")
		self.assertEqual(rules.skill["levelUpAnimation"], [1, 100, 200, 300, 400, 450, 550])
		rules = self.rules_with("skillengine/task/AbstractInteractionTask.java", "protected int delay = 1000;", "protected int delay = 1100;")
		self.assertEqual(rules.craft["delay"], 1100, "a snippet-checked field initializer is a literal read, not a pinned digest")

	def test_a_body_the_oracle_does_not_model_is_refused(self):
		cases = [
			# a literal of another type: 0.008 -> 0.008f would make the xp formula float arithmetic
			("services/craft/CraftService.java", "(int) ((0.008 *", "(int) ((0.008f *"),
			# one more statement
			("skillengine/task/CraftingTask.java", "craftType = CraftType.NORMAL;", "craftType = CraftType.NORMAL;\n\t\tskillLvlDiff++;"),
			# another comparison
			("skillengine/task/GatheringTask.java", "if (skillLvlDiff >= 41) {", "if (skillLvlDiff > 41) {"),
			# the interval cap switch changes its arms
			("services/craft/CraftService.java", "case MYTHIC:\n\t\t\t\tintervalCap = 1700;", "case LEGEND:\n\t\t\t\tintervalCap = 1700;"),
			# a table body that is not only rows
			("services/craft/CraftService.java", "\t\treturn 0;\n\t}\n\n}", "\t\treturn -1;\n\t}\n\n}"),
			# the fallthrough switch loses a cap level
			("model/skill/PlayerSkillList.java", "\t\t\t\t\tcase 549:\n", ""),
			# a bound radius override the range model ignores
			("model/templates/item/ItemTemplate.java", "public ItemQuality getItemQuality() {",
			 "public BoundRadius getBoundRadius() { return null; }\n\n\tpublic ItemQuality getItemQuality() {"),
			# the config field changes type
			("configs/main/CraftConfig.java", "public static int MAX_CRAFT_FAILURE_CHANCE;", "public static float MAX_CRAFT_FAILURE_CHANCE;"),
			# the material roll changes its bound expression
			("controllers/GatherableController.java", "Rnd.nextInt(10000000)", "Rnd.nextInt(10000000 + 1)"),
			# the crit chance gets another term
			("skillengine/task/CraftingTask.java", "if (Rnd.chance() >= chance)", "if (Rnd.chance() >= chance + critCount)"),
		]
		for relative, old, new in cases:
			with self.subTest(file=relative, new=new), self.assertRaises(OracleError):
				self.rules_with(relative, old, new)

	def test_the_whole_class_is_pinned(self):
		# every change the M5c craft review made to a copy of the tree, which the method-by-method check accepted, and more of the same kind
		cases = [
			# an extra override in CraftingTask
			("skillengine/task/CraftingTask.java", "\t@Override\n\tprotected void onFailureFinish() {",
			 "\t@Override\n\tprotected boolean onInteraction() {\n\t\treturn super.onInteraction();\n\t}\n\n"
			 "\t@Override\n\tprotected void onFailureFinish() {"),
			# a body in CraftingTask.onInteractionFinish
			("skillengine/task/CraftingTask.java", "protected void onInteractionFinish() {\n\t}",
			 "protected void onInteractionFinish() {\n\t\tcritCount = 0;\n\t}"),
			# GatheringTask overrides start()
			("skillengine/task/GatheringTask.java", "\tpublic int getGathererId() {",
			 "\t@Override\n\tpublic void start() {\n\t\tdelay = 5000;\n\t\tsuper.start();\n\t}\n\n\tpublic int getGathererId() {"),
			# an instance initializer in CraftingTask
			("skillengine/task/CraftingTask.java", "private int executionSpeed;", "private int executionSpeed;\n\t{\n\t\tdelay = 3000;\n\t}"),
			# the AbstractCraftTask constructor
			("skillengine/task/AbstractCraftTask.java", "this.skillLvlDiff = skillLvlDiff;", "this.skillLvlDiff = skillLvlDiff + 1;"),
			# a method the oracle does not model, pinned by its digest
			("controllers/GatherableController.java", "gatheringTask.abort();", "gatheringTask.stop();"),
			# the getters the model reads
			("model/templates/recipe/RecipeTemplate.java", "Collections.<ComponentsData> emptyList() : componentsData;",
			 "Collections.<ComponentsData> emptyList() : componentsData.subList(0, 1);"),
			("model/templates/gather/GatherableTemplate.java", "return harvestCount;", "return harvestCount * 2;"),
			# a field initializer, the class header and one more field
			("skillengine/task/AbstractCraftTask.java", "protected CraftType craftType = CraftType.NORMAL;",
			 "protected CraftType craftType = CraftType.CRIT_BLUE;"),
			("model/templates/gather/GatherableTemplate.java", "@XmlAccessorType(XmlAccessType.FIELD)", "@XmlAccessorType(XmlAccessType.PROPERTY)"),
			("model/templates/gather/GatherableTemplate.java", "protected int eraseValue;",
			 "protected int eraseValue;\n\t@XmlAttribute\n\tprotected int bonusCount;"),
			# the range comparison, the packet audience and the consumption the model relies on
			("utils/PositionUtil.java", "return dx * dx + dy * dy + dz * dz < range * range;", "return dx * dx + dy * dy + dz * dz <= range * range;"),
			("utils/PacketSendUtility.java", "if (toSelf)", "if (!toSelf)"),
			("model/gameobjects/Item.java", "long removeCount = count >= itemCount ? itemCount : count;",
			 "long removeCount = count > itemCount ? itemCount : count;"),
		]
		for relative, old, new in cases:
			with self.subTest(file=relative, new=new), self.assertRaises(OracleError):
				self.rules_with(relative, old, new)


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5cCraftConfigTest(unittest.TestCase):
	"""craft_config.load_craft_config: the @Property defaults, the three default folders, the profile and --set, and the refusals."""

	@classmethod
	def setUpClass(cls):
		cls.fields = JavaCraftRules.read(JAVA_SRC).config_fields

	def config_dir(self, root: Path, main: dict[str, str], other: dict[str, str] | None = None) -> Path:
		for folder in ("administration", "main", "network"):
			(root / "config" / folder).mkdir(parents=True, exist_ok=True)
		for name, text in main.items():
			(root / "config" / "main" / name).write_text(text, encoding="iso-8859-1")
		for name, text in (other or {}).items():
			(root / "config" / name).write_text(text, encoding="iso-8859-1")
		return root / "config"

	@staticmethod
	def values(config) -> dict:
		return {value.key: value.value for value in config.values()}

	def test_the_java_defaults(self):
		values = self.values(load_craft_config(self.fields, None, None, [NO_EVENTS]))
		self.assertEqual((values["gameserver.craft.fail.chance"], values["gameserver.gather.fail.chance"]), (33, 33))
		self.assertEqual(values["gameserver.rates.crafting.crit_chances"], [15.0, 30.0])
		self.assertEqual(values["gameserver.rates.skill_xp.gathering"], [1.0, 2.0])
		self.assertFalse(values["gameserver.security.captcha.enable"])
		with self.assertRaises(OracleError):
			load_craft_config(self.fields, None, None, [])  # disabled_events has no defaultValue and nothing sets it

	def test_the_layers(self):
		with tempfile.TemporaryDirectory() as tmp:
			config = self.config_dir(Path(tmp), {"craft.properties": "# comment\ngameserver.craft.fail.chance = 20\n",
			                                     "events.properties": "gameserver.event.service.disabled_events = *\n"},
			                         {"mygs.properties": "gameserver.gather.fail.chance: 5\ngameserver.craft.fail.chance=21\n"})
			loaded = load_craft_config(self.fields, config, None)
			fail = next(v for v in loaded.values() if v.key == "gameserver.craft.fail.chance")
			self.assertEqual((fail.value, fail.source, fail.raw), (20, "main/craft.properties", "20"))
			values = self.values(load_craft_config(self.fields, config, config / "mygs.properties", ["gameserver.craft.fail.chance=0",
			                                                                                          "gameserver.rates.crafting.crit_chances=0, 0"]))
			self.assertEqual((values["gameserver.craft.fail.chance"], values["gameserver.gather.fail.chance"]), (0, 5),
			                 "--set wins over the profile, the profile over the default folders")
			self.assertEqual(values["gameserver.rates.crafting.crit_chances"], [0.0, 0.0], "the float[] property accepts '0, 0' (m5c-plan.md §8)")
			profile = load_craft_config(self.fields, config, config / "mygs.properties")
			fail = next(v for v in profile.values() if v.key == "gameserver.craft.fail.chance")
			self.assertEqual((fail.value, fail.source), (21, "mygs.properties"), "without --set: the profile's 21 over main/craft.properties' 20")
			self.assertEqual(self.values(load_craft_config(self.fields, config, config / "missing.properties"))["gameserver.craft.fail.chance"], 20,
			                 "a missing profile is no error")

	def test_the_refusals(self):
		cases = [
			({"craft.properties": "gameserver.craft.fail.chance = 20\n", "craft2.properties": "gameserver.craft.fail.chance = 30\n",
			  "events.properties": "gameserver.event.service.disabled_events = *\n"}, [], "two default files disagree"),
			({"events.properties": "gameserver.event.service.disabled_events = \n"}, [], "the shipped empty value: events are active"),
			({"events.properties": "gameserver.event.service.disabled_events = Some Event\n"}, [], "one event disabled, the others active"),
			({"events.properties": "gameserver.event.service.disabled_events = *, x\n"}, [], "isAllEvents wants the set {*}"),
			({"events.properties": "gameserver.event.service.disabled_events = *\ngameserver.craft.fail.chance = 20 \n"}, [],
			 "Integer.decode(\"20 \") throws at startup"),
			({"events.properties": "gameserver.event.service.disabled_events = *\n"}, ["gameserver.craft.fail.chance=0x14"], "hexadecimal"),
			({"events.properties": "gameserver.event.service.disabled_events = *\n"}, ["gameserver.security.captcha.enable=yes"], "not a boolean"),
			({"events.properties": "gameserver.event.service.disabled_events = *\n"}, ["gameserver.rates.crafting.crit_chances=NaN"], "NaN"),
			({"events.properties": "gameserver.event.service.disabled_events = *\n"}, ["no-equals-sign"], "--set without ="),
		]
		for main, overrides, why in cases:
			with self.subTest(why=why), tempfile.TemporaryDirectory() as tmp, self.assertRaises(OracleError):
				load_craft_config(self.fields, self.config_dir(Path(tmp), main), None, overrides)
		with tempfile.TemporaryDirectory() as tmp, self.assertRaises(OracleError):
			load_craft_config(self.fields, Path(tmp) / "nowhere", None, [NO_EVENTS])


RECIPES = """
<recipe_template id="900001" nameid="1" skillid="40001" race="ELYOS" skillpoint="1" autolearn="1" productid="700001" quantity="2">
	<components_data><component quantity="1" itemid="700010"/><component quantity="2" itemid="700011"/></components_data>
	<comboproduct itemid="700002"/><comboproduct itemid="700003"/>
</recipe_template>
<recipe_template id="900002" nameid="2" skillid="40001" race="PC_ALL" skillpoint="5" autolearn="2" productid="700004" quantity="1"/>
<recipe_template id="900003" nameid="3" skillid="40001" race="ASMODIANS" skillpoint="1" autolearn="1" productid="700001" quantity="1">
	<components_data><component quantity="1" itemid="700010"/></components_data>
</recipe_template>
<recipe_template id="900004" nameid="4" skillid="40001" race="ELYOS" skillpoint="1" productid="700005" quantity="1" max_production_count="1"
	craft_delay_id="7" craft_delay_time="60">
	<components_data><component quantity="3" itemid="700010"/></components_data>
	<components_data><component quantity="1" itemid="700011"/></components_data>
</recipe_template>
<recipe_template id="900005" nameid="5" skillid="40009" race="ELYOS" skillpoint="1" dp="200" autolearn="1" productid="700001" quantity="3">
	<components_data><component quantity="1" itemid="700010"/></components_data>
</recipe_template>
<recipe_template id="900006" nameid="6" skillid="40002" race="ELYOS" skillpoint="1" productid="700001" quantity="1" unknown="1"/>
<recipe_template id="900007" nameid="7" skillid="40002" race="ELYOS" skillpoint="1" productid="700001" quantity="1"><components_data/></recipe_template>
<recipe_template id="900008" nameid="8" skillid="40002" race="ELYOS" skillpoint="1" productid="799999" quantity="1"/>
<recipe_template id="900009" nameid="9" skillid="40002" race="ELYOS" skillpoint="1" productid="700006" quantity="1"/>
<recipe_template id="900010" nameid="10" skillid="40002" race="ELYOS" skillpoint="1" productid="700001" quantity="1"><comboproduct/></recipe_template>
<recipe_template id="900011" nameid="11" skillid="40002" race="ELYOS" skillpoint="1" productid="700001" quantity="1">
	<components_data><component quantity="10" itemid="700010"/><component quantity="2" itemid="700011"/><component quantity="7" itemid="700010"/>
	</components_data>
	<components_data><component quantity="1" itemid="700010"/></components_data>
	<components_data><component quantity="3" itemid="700011"/></components_data>
</recipe_template>
<recipe_template id="900012" nameid="12" skillid="40002" race="ELYOS" skillpoint="1" autolearn="1" productid="700001" quantity="1"
	max_production_count="1"><components_data><component quantity="1" itemid="700010"/></components_data></recipe_template>
<recipe_template id="900013" nameid="13" skillid="40002" race="ELYOS" skillpoint="1" productid="700001" quantity="1">
	<components_data><component quantity="0" itemid="700010"/></components_data></recipe_template>
<recipe_template id="900014" nameid="14" skillid="40001" race="ASMODIANS" skillpoint="1" productid="700001" quantity="1">
	<components_data><component quantity="1" itemid="169401081"/></components_data></recipe_template>
<recipe_template id="900015" nameid="15" skillid="40001" race="ELYOS" skillpoint="90" productid="700001" quantity="1"
	craft_delay_id="8" craft_delay_time="3000000"><components_data><component quantity="1" itemid="700010"/></components_data></recipe_template>
<recipe_template id="900016" nameid="16" skillid="40004" race="ELYOS" skillpoint="1" productid="700001" quantity="1">
	<components_data><component quantity="1" itemid="700010"/></components_data></recipe_template>
"""

ITEMS = """
<item_template id="700001" name="fixture meal" quality="COMMON" price="300"/>
<item_template id="700002" name="fixture tasty meal" quality="RARE" price="300"/>
<item_template id="700003" name="fixture feast" quality="MYTHIC" price="300"/>
<item_template id="700004" name="fixture snack" quality="COMMON"/>
<item_template id="700005" name="fixture heroic meal" quality="UNIQUE"/>
<item_template id="700006" name="fixture without quality"/>
<item_template id="700010" name="fixture shell" quality="COMMON"/>
<item_template id="700011" name="fixture salt" quality="COMMON" price="50"/>
<item_template id="169401081" name="fixture cooking stone" quality="COMMON"/>
<item_template id="169401076" name="fixture weaponsmithing stone" quality="COMMON"/>
<item_template id="169401078" name="fixture tailoring stone" quality="COMMON"/>
<item_template id="700020" name="Recipe: fixture meal" quality="COMMON" price="550" race="ELYOS"><actions><craftlearn recipeid="900001"/></actions>
</item_template>
<item_template id="700030" name="fixture low herb" quality="COMMON"/>
<item_template id="700031" name="fixture high herb" quality="COMMON"/>
<item_template id="700032" name="fixture berry" quality="COMMON"/>
<item_template id="700041" name="fixture ore" quality="COMMON"/>
<item_template id="700042" name="fixture rare ore" quality="RARE"/>
"""

SKILLS = """
<skill_template skill_id="30001" name="Collection" activation="NONE"/>
<skill_template skill_id="30002" name="Essencetapping" activation="NONE"/>
<skill_template skill_id="30003" name="Aethertapping" activation="NONE"/>
<skill_template skill_id="40004" name="fixture passive tailoring" activation="PASSIVE"/>
<skill_template skill_id="40001" name="Cooking" activation="NONE"/>
<skill_template skill_id="40002" name="Weaponsmithing" activation="NONE"/>
<skill_template skill_id="40009" name="Morph Substances" activation="NONE"/>
"""

GATHERABLES = """
<gatherable_template id="400601" name="fixture plant" nameId="11" sourceType="PLANT" harvestCount="3" skillLevel="1" harvestSkill="30001">
	<materials><material rate="3000000" nameid="1" itemid="700030" name="low"/><material rate="7000000" nameid="2" itemid="700031" name="high"/></materials>
</gatherable_template>
<gatherable_template id="400602" name="fixture sapling" nameId="12" sourceType="BERRY" harvestCount="0" skillLevel="10" harvestSkill="30001" lvlLimit="5">
	<materials><material rate="10000000" itemid="700032" name="berry"/></materials>
</gatherable_template>
<gatherable_template id="400603" name="fixture rift" nameId="13" sourceType="ORE" harvestCount="2" skillLevel="1" harvestSkill="30003" reqItem="700040"
	reqItemNameId="99" checkType="2" eraseValue="1">
	<materials><material rate="10000000" itemid="700041" name="ore"/></materials>
	<exmaterials><material rate="10000000" itemid="700042" name="rare ore"/></exmaterials>
</gatherable_template>
<gatherable_template id="400604" name="fixture short" nameId="14" harvestCount="1" skillLevel="1" harvestSkill="30003">
	<materials><material rate="9000000" itemid="700041"/></materials>
</gatherable_template>
<gatherable_template id="400605" name="fixture no extra" nameId="15" harvestCount="1" skillLevel="1" harvestSkill="30003" reqItem="1" checkType="1">
	<materials><material rate="10000000" itemid="700041"/></materials>
</gatherable_template>
<gatherable_template id="400606" name="fixture deep vein" nameId="16" harvestCount="1" skillLevel="420" harvestSkill="30003">
	<materials><material rate="10000000" itemid="700041"/></materials>
</gatherable_template>
<gatherable_template id="400607" name="fixture phantom" nameId="17" harvestCount="1" skillLevel="450" harvestSkill="30002">
	<materials><material rate="10000000" itemid="799999"/></materials>
</gatherable_template>
<gatherable_template id="400608" name="fixture tie" nameId="18" harvestCount="1" skillLevel="2" harvestSkill="30001">
	<materials><material rate="5000000" itemid="700031" name="first"/><material rate="5000000" itemid="700030" name="second"/></materials>
</gatherable_template>
"""

SPAWNS = """
<spawn_map map_id="1">
	<spawn npc_id="400601" respawn_time="295"><spot x="1" y="1" z="1"/><spot x="2" y="2" z="2"/></spawn>
	<spawn npc_id="400602" respawn_time="60"><spot x="3" y="3" z="3"/></spawn>
	<spawn npc_id="400603" respawn_time="60"><spot x="4" y="4" z="4"/></spawn>
</spawn_map>
"""


def fixture_tree() -> Tree:
	tree = Tree()
	tree.minimal({"recipe_templates": RECIPES, "item_templates": ITEMS, "skill_data": SKILLS, "gatherable_templates": GATHERABLES,
	              "spawns": SPAWNS, "skill_tree": '<skill skillId="30001" minLevel="1" autolearn="true"/>',
	              "world_maps": '<map id="1" world_type="ELYSEA"/><map id="2" instance="true"/>'})
	return tree


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5cCraftFixtureReportTest(unittest.TestCase):
	"""The whole report on a small static_data tree; the rules come from the Java sources, the config from the @Property defaults."""

	@classmethod
	def setUpClass(cls):
		cls.tree = fixture_tree()
		cls.ctx = CraftContext.create(StaticData(cls.tree.root), JAVA_SRC, None, None, [NO_EVENTS])
		cls.gate = dataclasses.replace(cls.ctx, config=load_craft_config(cls.ctx.rules.config_fields, None, None, GATE_PROFILE))

	@classmethod
	def tearDownClass(cls):
		cls.tree.close()

	def test_a_recipe_with_procs_at_the_default_rates(self):
		report = craft_report(self.ctx, recipe=900001)
		self.assertEqual(report["format"], "aion-m5c-craft")
		recipe = report["recipe"]
		self.assertEqual((recipe["skill"]["name"], recipe["skill"]["kind"], recipe["skill"]["profession"], recipe["skillpoint"], recipe["dp"]),
		                 ("Cooking", "crafting", "COOKING", 1, 0))
		self.assertEqual(recipe["components"], [[{"itemId": 700010, "quantity": 1, "name": "fixture shell"},
		                                         {"itemId": 700011, "quantity": 2, "name": "fixture salt"}]])
		self.assertEqual(recipe["product"], {"itemId": 700001, "quantity": 2, "name": "fixture meal", "quality": "COMMON"})
		self.assertEqual([(c["critCount"], c["itemId"], c["quality"]) for c in recipe["comboProducts"]], [(1, 700002, "RARE"), (2, 700003, "MYTHIC")])
		self.assertEqual([i["itemId"] for i in report["learn"]["recipeItems"]], [700020], "the item whose <craftlearn> names the recipe")
		self.assertEqual((report["learn"]["autolearnRaces"], report["learn"]["autolearnAtSkillLevel"]), (["ELYOS"], 1))
		craft = report["craft"]
		self.assertEqual((craft["skillLvlDiff"], craft["timing"]["interval"], craft["timing"]["firstTickDelay"], craft["refusal"]), (0, 2500, 1000, None))
		self.assertEqual(craft["station"]["effectiveRange"], 5.25, "5 plus the player's 0.25 plus the StaticObject's 0 (centerToCenter false)")
		first, second, third = craft["bars"]
		self.assertEqual((first["failureThreshold"], first["successPerTick"], first["critBlueThreshold"], first["critBluePerSuccess"]),
		                 (33.0, 0.67, 15.0, 0.15), "gameserver.craft.fail.chance 33 at level difference 0")
		self.assertEqual((first["successStep"], first["failureStep"], first["executionSpeed"], first["showBarDelay"]),
		                 ({"normal": [75, 79], "critBlue": [175, 279]}, [126, 133], 900, 1200))
		self.assertEqual(first["steps"], {"fewest": 4, "most": 14, "mostToAnyEnd": 21, "fewestToFailure": 8},
		                 "4 blue steps at best, 14 normal steps at worst; with failures up to 13 + 7 + 1 ticks")
		self.assertEqual((second["bonusModifier"], third["bonusModifier"], third["successStep"]["critBlue"], third["executionSpeed"]),
		                 (1.0, f32(0.3), [102, 133], 1530), "the proc bars take their combo product's quality")
		self.assertTrue(all(o["probability"] is None for o in craft["outcomes"]), "a bar can fail: the outcome distribution is not modelled")
		self.assertEqual(craft["crit"]["critChance"], 15.0)

	def test_the_gate_profile_makes_the_outcome_exact(self):
		craft = craft_report(self.gate, recipe=900001)["craft"]
		self.assertEqual([(o["critCount"], o["itemId"], o["quantity"], o["probability"]) for o in craft["outcomes"]],
		                 [(0, 700001, 2, 1.0), (1, 700002, 2, 0.0), (2, 700003, 2, 0.0)])
		self.assertEqual(craft["outcomes"][0]["finishMillis"], {"fewest": 11000, "most": 36000}, "1000 + 4 * 2500 .. 1000 + 14 * 2500")
		procs = dataclasses.replace(self.ctx, config=load_craft_config(self.ctx.rules.config_fields, None, None,
		                                                               [NO_EVENTS, "gameserver.craft.fail.chance=0"]))
		outcomes = craft_report(procs, recipe=900001)["craft"]["outcomes"]
		self.assertEqual([o["probability"] for o in outcomes], [0.85, 0.1125, 0.0375],
		                 "calculateCrit: 15 % for the first proc, then the combo rate 25 % (0.15 * 0.75, 0.15 * 0.25)")
		self.assertEqual(outcomes[1]["finishMillis"]["fewest"], 1000 + (4 + 4 + 1) * 2500, "a proc restarts the bar one tick after it filled")

	def test_skill_up_and_the_bonus_craft(self):
		report = craft_report(self.gate, recipe=900001)
		up = report["skillUp"]
		self.assertEqual((up["xpReward"], up["gainedSkillXp"], up["requiredExp"], up["levelAfter"], up["currentXpAfter"]), (141, 141, 76, 2, 0))
		self.assertEqual(up["playerExp"]["reward"], 141, "addExp(xpReward, Rates.XP_CRAFTING) at rate 1.0")
		self.assertEqual(up["autolearnedOnLevelUp"], {"ELYOS": [], "ASMODIANS": []}, "900002 needs level 5")
		bonus = craft_report(self.gate, recipe=900001, craft_type=1)
		self.assertEqual((bonus["skillUp"]["xpRewardWithBonus"], bonus["craft"]["bonusItem"]["itemId"], bonus["craft"]["bonusItem"]["consumed"]),
		                 (162, 169401081, True))
		high = craft_report(self.gate, recipe=900001, skill_level=4, current_xp=0)
		self.assertEqual((high["craft"]["skillLvlDiff"], high["craft"]["timing"]["interval"], high["skillUp"]["levelAfter"]), (3, 2320, 5))
		self.assertEqual(high["skillUp"]["autolearnedOnLevelUp"], {"ELYOS": [900002], "ASMODIANS": [900002]},
		                 "reaching 5 teaches the PC_ALL recipe of skillpoint 5 (SkillLearnService.onLearnSkill)")
		far = craft_report(self.gate, recipe=900001, skill_level=42)
		self.assertEqual((far["skillUp"]["granted"], far["skillUp"]["playerExp"]), (False, None), "42 - 1 > 40: no skill xp and no exp")
		self.assertTrue(far["craft"]["bars"][0]["alwaysSuccess"])

	def test_a_limited_unique_recipe_with_two_alternatives_and_a_cooldown(self):
		report = craft_report(self.ctx, recipe=900004, skill_level=30)
		craft = report["craft"]
		self.assertEqual((craft["timing"]["interval"], craft["timing"]["intervalCap"]), (1500, 1500), "UNIQUE caps 2500 - 29 * 60 at 1500")
		bar = craft["bars"][0]
		self.assertEqual((bar["bonusModifier"], bar["executionSpeed"], bar["showBarDelay"]), (f32(0.7), 1170, 1200))
		self.assertEqual([([c["itemId"] for c in alt["components"]], alt["selectable"]) for alt in craft["materials"]["alternatives"]],
		                 [([700010], True), ([700011], True)])
		self.assertEqual(report["afterCraft"], {"productAddedAfterSkillUp": True, "recipeDeleted": True, "cooldownId": 7, "cooldownMillis": 60000,
		                                        "creatorNameSet": "only on a weapon or armor product (ItemUpdatePredicate.changeItem)"})
		self.assertEqual(craft_report(self.ctx, recipe=900004)["craft"]["bars"][0]["failureStep"], [74, 79],
		                 "a max_production_count recipe fails in steps from 70, not 120")

	def test_the_morph_skill(self):
		craft = craft_report(self.gate, recipe=900005)["craft"]
		self.assertEqual((craft["station"], craft["morphPacketUnk"], craft["timing"]["interval"], craft["dp"]["refusedForStartingClass"]),
		                 (None, 129, 200, True))
		self.assertEqual((craft["bars"][0]["instant"], craft["outcomes"][0]["finishMillis"]), (True, {"fewest": 1200, "most": 1200}))
		updates = [p for p in craft["packets"]["start"] if p["packet"] == "SM_CRAFT_UPDATE"]
		self.assertEqual([p["delay"] for p in updates], [1000, 1000], "SM_CRAFT_UPDATE writes delay 1000 for the morph skill")
		with self.assertRaises(OracleError):
			craft_report(self.gate, recipe=900005, craft_type=1)  # getBonusReqItem(40009) is 0

	def test_refusals_of_the_skill_level(self):
		self.assertEqual(craft_report(self.ctx, recipe=900002, skill_level=3)["craft"]["refusal"], "STR_COMBINE_OUT_OF_SKILL_POINT (checkCraft)")
		self.assertIsNone(craft_report(self.ctx, recipe=900002, skill_level=3)["skillUp"])
		self.assertTrue(craft_report(self.ctx, recipe=900002, skill_level=0)["craft"]["refusal"].startswith("STR_COMBINE_CANT_USE"))

	def test_recipes_the_oracle_refuses(self):
		for recipe, why in ((1, "unknown id"), (900006, "an unknown attribute"), (900007, "an empty components_data"),
		                    (900008, "a product without a template"), (900009, "a product without a quality"), (900010, "a combo without itemid")):
			with self.subTest(why=why), self.assertRaises(OracleError):
				craft_report(self.ctx, recipe=recipe)
		with self.assertRaises(OracleError):
			craft_report(self.ctx, recipe=900001, craft_type=2)
		with self.assertRaises(OracleError):
			craft_report(self.ctx, recipe=900001, skill=40001, level=1)  # two modes

	def test_a_gatherable(self):
		report = craft_report(self.ctx, gatherable=400601)
		entry = report["gatherable"]
		self.assertEqual([(m["itemId"], m["rolls"], m["probability"], m["documentPosition"]) for m in entry["materials"]],
		                 [(700031, 7000001, 0.7000001, 1), (700030, 2999999, 0.2999999, 0)],
		                 "GatherableData sorts by descending rate; the first material also wins chance 0")
		self.assertEqual((entry["xpReward"], entry["count"], entry["range"]["effectiveRange"]), (91, 1, 3.25))
		task = entry["task"]
		self.assertEqual((task["timing"], task["failureThreshold"], task["perSuccess"]),
		                 ({"firstTickDelay": [200, 600], "interval": 2500}, 33.0, {"critPurple": 0.01, "critBlue": 0.04, "normal": 0.95}))
		self.assertEqual((task["successStep"]["normal"], task["successStep"]["critBlue"], task["failureStep"]), ([75, 80], [175, 280], [125, 130]))
		self.assertEqual(task["steps"], {"fewest": 1, "most": 14, "mostToAnyEnd": 21, "fewestToFailure": 8}, "a purple crit fills the bar at once")
		self.assertEqual(task["finishMillis"], {"fewest": 2700, "most": 53100})
		self.assertEqual((entry["skillUp"]["levelAfter"], entry["skillUp"]["playerExp"]["reward"]), (2, 91))
		self.assertEqual(entry["node"]["despawnAfterInteractions"], 3)
		self.assertEqual(entry["packets"]["aborted"][0]["to"], "known players only, not the actor",
		                 "onInteractionAbort broadcasts the animation without toSelf")
		instant = craft_report(self.ctx, gatherable=400601, skill_level=42)["gatherable"]["task"]
		self.assertEqual((instant["instant"], instant["executionSpeed"], instant["showBarDelay"], instant["timing"]["interval"]), (True, 300, 500, 1200))
		self.assertFalse(craft_report(self.ctx, gatherable=400601, skill_level=42)["gatherable"]["skillUp"]["granted"], "42 - 1 > 40")

	def test_gatherable_refusals_and_extra_materials(self):
		sapling = craft_report(self.ctx, gatherable=400602, skill_level=10)["gatherable"]
		self.assertEqual((sapling["refusal"], sapling["task"], sapling["node"]["despawnAfterInteractions"]),
		                 ("STR_MSG_CANT_GATHERING_B_LEVEL_CHECK (character level below lvlLimit)", None, None), "lvlLimit 5, harvestCount 0")
		self.assertIsNone(craft_report(self.ctx, gatherable=400602, skill_level=10, character_level=5)["gatherable"]["refusal"])
		self.assertTrue(craft_report(self.ctx, gatherable=400602, skill_level=9, character_level=5)["gatherable"]["refusal"]
		                .startswith("STR_GATHER_OUT_OF_SKILL_POINT"))
		rift = craft_report(self.ctx, gatherable=400603)["gatherable"]
		self.assertEqual((rift["requiredItem"], [m["itemId"] for m in rift["exMaterials"]], rift["erasedOnSuccess"]),
		                 ({"itemId": 700040, "nameId": 99, "checkType": 2, "eraseValue": 1}, [700042], 1))
		for gatherable in (400604, 400605, 1):
			with self.subTest(gatherable=gatherable), self.assertRaises(OracleError):
				craft_report(self.ctx, gatherable=gatherable)
		captcha = dataclasses.replace(self.ctx, config=load_craft_config(self.ctx.rules.config_fields, None, None,
		                                                                 [NO_EVENTS, "gameserver.security.captcha.enable=true"]))
		with self.assertRaises(OracleError):
			craft_report(captcha, gatherable=400601)

	def test_a_crafting_skill(self):
		report = craft_report(self.ctx, skill=40001, level=1)
		self.assertEqual(report["recipes"]["ELYOS"], {"autolearn": [{"id": 900001, "skillpoint": 1, "productId": 700001, "quantity": 2}],
		                                              "newAtLevel": [900001], "craftableCount": 2})
		self.assertEqual([r["id"] for r in report["recipes"]["ASMODIANS"]["autolearn"]], [900003])
		five = craft_report(self.ctx, skill=40001, level=5)
		self.assertEqual([r["id"] for r in five["recipes"]["ELYOS"]["autolearn"]], [900001, 900002], "the PC_ALL recipe for both races")
		self.assertEqual(five["recipes"]["ELYOS"]["newAtLevel"], [900002])
		self.assertEqual((report["skillUp"]["requiredExp"], report["upgrade"]["cost"], report["upgrade"]["refusal"]), (76, None, "STR_MSG_DONT_RANK_UP"))
		self.assertEqual(report["xpByRecipeSkillpoint"], [{"objectLevel": 1, "xpReward": 141, "gainedSkillXp": 141, "granted": True,
		                                                   "levelsUp": True, "playerExp": 141}])
		learn = craft_report(self.ctx, skill=40001, level=0)["upgrade"]
		self.assertEqual((learn["cost"], learn["newLevel"], learn["minCharacterLevel"], learn["masters"]), (3500, 1, 10, [203784, 204100, 830058, 830142]))
		self.assertEqual(craft_report(self.ctx, skill=40001, level=449)["upgrade"]["cost"], 6004900)
		self.assertEqual(craft_report(self.ctx, skill=40001, level=399)["upgrade"]["refusal"], "STR_CRAFT_CANT_EXTEND_MONEY")
		self.assertEqual(craft_report(self.ctx, skill=40001, level=99)["skillUp"]["cap"].split(":")[0], "level 99 is a cap level")

	def test_a_gathering_skill_on_a_map(self):
		report = craft_report(self.ctx, skill=30001, level=1, map_id=1)
		self.assertEqual(report["skill"]["skillTree"], [{"skillId": "30001", "minLevel": "1", "autolearn": "true"}])
		self.assertIsNone(report["upgrade"], "30001 is no Profession: no master raises it")
		entries = report["gatherables"]["entries"]
		self.assertEqual([(e["id"], e["canGather"], e["spots"]["count"], e["spots"]["respawnTimes"]) for e in entries],
		                 [(400601, True, 2, [295]), (400602, False, 1, [60])], "400603 is a 30003 gatherable")
		self.assertEqual([e["id"] for e in craft_report(self.ctx, skill=30001, level=1)["gatherables"]["entries"]], [400601],
		                 "without a map: the templates of the skill up to the level")
		self.assertEqual(craft_report(self.ctx, skill=30001, level=49)["skillUp"]["cap"].split(" (")[0], "human gathering is capped at 49")
		with self.assertRaises(OracleError):
			craft_report(self.ctx, skill=30001, level=1, map_id=2)  # an instance map
		with self.assertRaises(OracleError):
			craft_report(self.ctx, skill=30003, level=1)  # 400604 and 400605 are refused
		with self.assertRaises(OracleError):
			craft_report(self.ctx, skill=12345, level=1)

	def test_materials_named_twice_and_shadowed_alternatives(self):
		craft = craft_report(self.gate, recipe=900011)["craft"]
		alternatives = craft["materials"]["alternatives"]
		self.assertEqual([(a["firstItemId"], a["selectable"], a["shadowedBy"]) for a in alternatives],
		                 [(700010, True, None), (700010, False, 0), (700011, True, None)],
		                 "the second alternative starts with 700010 like the first: checkCraft never gets to it")
		self.assertEqual([(i["itemId"], i["required"], i["consumed"]) for i in alternatives[0]["items"]], [(700010, 10, 17), (700011, 2, 2)],
		                 "700010 x10 and x7: 10 held pass, and the two decreases take min(held, 17)")

	def test_recipe_shapes_the_oracle_refuses(self):
		for recipe, craft_type, reason, why in ((900012, 0, "autolearn with max_production_count", "deleted, then relearnt on the level-up"),
		                                        (900013, 0, "has quantity 0", "a component of quantity 0"),
		                                        (900014, 1, "bonus item 169401081 is also a component", "craft type 1 takes it in the middle"),
		                                        (900016, 0, "activation 'PASSIVE'", "a level-up of a passive skill applies its effects")):
			with self.subTest(why=why), self.assertRaisesRegex(OracleError, reason):
				craft_report(self.gate, recipe=recipe, craft_type=craft_type)
		self.assertIsNotNone(craft_report(self.gate, recipe=900014)["craft"], "craft type 0 does not take the bonus item")
		self.assertFalse(craft_report(self.gate, recipe=900016, skill_level=42)["skillUp"]["granted"], "no level-up, no passive effect")

	def test_checks_and_the_cancel_pair(self):
		craft = craft_report(self.gate, recipe=900001)["craft"]
		self.assertTrue(all(check["then"] == "the cancel pair (packets.refused)" for check in craft["checks"]),
		                "startCrafting sends sendCancelCraft for every false checkCraft")
		self.assertEqual([(c["message"], c["auditLog"]) for c in craft["checks"][:4]],
		                 [(None, False), (None, True), ("STR_COMBINE_TOO_FAR_FROM_TOOL", False), (None, True)],
		                 "in progress, wrong target, out of range, DP: no system message for the DP refusal, an AuditLogger line")
		refused, aborted = craft["packets"]["refused"], craft["packets"]["aborted"]
		self.assertEqual([(p["packet"], p["to"], p["action"], p.get("delay")) for p in refused],
		                 [("SM_CRAFT_UPDATE", "self", 4, 0), ("SM_CRAFT_ANIMATION", "self and known players", 2, None)])
		self.assertEqual([p["packet"] for p in craft["packets"]["start"]], ["SM_DP_INFO", "SM_STATS_INFO", "SM_STATUPDATE_DP", "SM_CRAFT_UPDATE",
		                                                                  "SM_CRAFT_UPDATE", "SM_CRAFT_ANIMATION", "SM_CRAFT_ANIMATION"],
		                 "setDp's packets (not for a starting class) come before the task's")
		morph = craft_report(self.gate, recipe=900005)["craft"]
		self.assertEqual((morph["packets"]["refused"][0]["delay"], morph["packets"]["aborted"][0]["delay"], aborted[0]["delay"]), (1000, 1000, 0),
		                 "SM_CRAFT_UPDATE writes delay 1000 for the morph skill, the cancel and abort updates too")
		self.assertIn("cancel pair", morph["dp"]["note"], "a starting class is refused a dp 200 recipe with packets, not silently")

	def test_skill_up_packets(self):
		def shape(packets):
			return [(p.get("packet"), p.get("message") or p.get("animation"), p.get("level"), p.get("exp")) for p in packets if "packet" in p]

		self.assertEqual(shape(craft_report(self.gate, recipe=900001)["skillUp"]["packets"]),
		                 [("SM_SKILL_LIST", 1330064, 2, None), ("SM_SYSTEM_MESSAGE", "STR_GET_EXP2", None, 141)],
		                 "cooking 1 -> 2: SkillLearnService.sendPacket's crafting message, no CRAFT_LEVEL_UP below 100")
		self.assertEqual(shape(craft_report(self.gate, recipe=900001, skill_level=42)["skillUp"]["packets"]),
		                 [("SM_SYSTEM_MESSAGE", "STR_MSG_DONT_GET_PRODUCTION_EXP", None, None)])
		self.assertEqual(shape(craft_report(self.gate, recipe=900015, skill_level=99)["skillUp"]["packets"]),
		                 [("SM_SYSTEM_MESSAGE", "STR_CRAFT_INFO_MAXPOINT_UP", None, None),
		                  ("SM_SYSTEM_MESSAGE", "STR_MSG_DONT_GET_PRODUCTION_EXP", None, None)],
		                 "99 is a cap level: addSkillXp's own message first")
		self.assertEqual(shape(craft_report(self.ctx, gatherable=400601)["gatherable"]["skillUp"]["packets"]),
		                 [("SM_SKILL_LIST", 1330005, 2, None), ("SM_SYSTEM_MESSAGE", "STR_EXTRACT_GATHERING_SUCCESS_GETEXP", None, None),
		                  ("SM_SYSTEM_MESSAGE", "STR_GET_EXP2", None, 91)], "a tapping skill's message, then rewardPlayer's")
		# 30003 at 449 (free for tapping) on a skillLevel 420 vein: xp (int) (0.0031 * 425.3 * 2012.8 + 60) = 2713, the level needs
		# (int) (0.23 * 466.2^2) = 49988: 47275 + 2713 reaches it exactly, 47274 does not
		up = craft_report(self.ctx, gatherable=400606, skill_level=449, current_xp=47275)["gatherable"]["skillUp"]
		self.assertEqual((up["gainedSkillXp"], up["requiredExp"], up["levelAfter"]), (2713, 49988, 450))
		self.assertEqual(shape(up["packets"]), [("SM_ACTION_ANIMATION", "CRAFT_LEVEL_UP", None, None), ("SM_SKILL_LIST", 1330005, 450, None),
		                                        ("SM_SYSTEM_MESSAGE", "STR_EXTRACT_GATHERING_SUCCESS_GETEXP", None, None),
		                                        ("SM_SYSTEM_MESSAGE", "STR_GET_EXP2", None, 2713)], "450 is a CRAFT_LEVEL_UP level")
		below = craft_report(self.ctx, gatherable=400606, skill_level=449, current_xp=47274)["gatherable"]["skillUp"]
		self.assertEqual((below["levelAfter"], below["currentXpAfter"], [p.get("packet") for p in below["packets"]]),
		                 (449, 49987, ["SM_SYSTEM_MESSAGE", "SM_SYSTEM_MESSAGE"]))

	def test_a_level_difference_of_24(self):
		# craft 900001 (COMMON, skillpoint 1) at 25: lvlBoni (24 - 10) * 2 = 28, base (25 / 2f + 28) * 10 = 405; (int) (405 * 1.9999999f) = 809;
		# failure (int) (25 / 1.5f * 10 * multi): 166 .. 333; crit blue 15 + 24 / 3f = 23; interval 2500 - 1440 capped at 1200; speed 900 - 720
		# floored at 300, delay 1200 - 720 floored at 500; the fail threshold 33 * (1 - 24 * 0.015f) in float
		threshold = f32(33.0 * f32(1.0 - f32(24.0 * f32(0.015))))
		self.assertAlmostEqual(threshold, 21.12, places=5)
		craft = craft_report(self.ctx, recipe=900001, skill_level=25)["craft"]
		bar = craft["bars"][0]
		self.assertEqual((bar["successStep"], bar["failureStep"], bar["critBlueThreshold"], bar["failureThreshold"]),
		                 ({"normal": [475, 879], "critBlue": [575, 1079]}, [286, 453], 23.0, threshold))
		self.assertEqual((craft["timing"]["interval"], bar["executionSpeed"], bar["showBarDelay"], bar["critBluePerSuccess"]), (1200, 300, 500, 0.23))
		# gather 400601 (skillLevel 1) at 25: Math.round(70 + 405 * multi) without the (int): 475 .. 880 (blue 575 .. 1080); failure
		# Math.round(120 + 25 / 2f * 10 * multi) = 245 .. 370; purple 1 + 24 / 10f = 3.4, blue 5 + 24 / 3f = 13 minus the purple
		task = craft_report(self.ctx, gatherable=400601, skill_level=25)["gatherable"]["task"]
		self.assertEqual((task["successStep"]["normal"], task["successStep"]["critBlue"], task["failureStep"], task["failureThreshold"]),
		                 ([475, 880], [575, 1080], [245, 370], threshold))
		self.assertEqual((task["critPurpleThreshold"], task["critBlueThreshold"], task["timing"]["interval"], task["executionSpeed"], task["showBarDelay"]),
		                 (f32(3.4), 13.0, 1200, 300, 500))
		self.assertEqual(task["perSuccess"], {"critPurple": float(Fraction(f32(3.4)) / 100), "critBlue": float((13 - Fraction(f32(3.4))) / 100),
		                                      "normal": float(1 - Fraction(13, 100))})

	def test_every_rate_key_at_membership_1(self):
		# a distinct array per key, and membership 1 takes element 1 (Rates.get): a key read for another cannot pass
		overrides = [NO_EVENTS, "gameserver.craft.fail.chance=0", "gameserver.rates.crafting.crit_chances=0, 0",
		             "gameserver.rates.skill_xp.crafting=1.0, 1.5", "gameserver.rates.skill_xp.gathering=1.0, 2.5",
		             "gameserver.rates.xp.crafting=1.0, 3.0", "gameserver.rates.xp.gathering=1.0, 0.5", "gameserver.rates.gathering.count=1.0, 4.0"]
		ctx = CraftContext.create(StaticData(self.tree.root), JAVA_SRC, None, None, overrides, membership=1)
		craft = craft_report(ctx, recipe=900001)["skillUp"]
		self.assertEqual((craft["skillXpRate"], craft["gainedSkillXp"], craft["playerExp"]["rate"], craft["playerExp"]["reward"]), (1.5, 211, 3.0, 423),
		                 "(long) (141 * 1.5f) = 211 skill xp; the exp is (long) (141 * 3.0f) of the UNRATED xp reward")
		gather = craft_report(ctx, gatherable=400601)["gatherable"]
		self.assertEqual((gather["count"], gather["skillUp"]["gainedSkillXp"], gather["skillUp"]["playerExp"]["reward"]), (4, 227, 45),
		                 "(long) (1 * 4.0f) items, (long) (91 * 2.5f) = 227 skill xp, (long) (91 * 0.5f) = 45 exp")

	def test_a_material_tie_keeps_the_document_order(self):
		# List.sort is stable: equal rates keep their document order, and the first material also wins chance 0
		entry = craft_report(self.ctx, gatherable=400608)["gatherable"]
		self.assertEqual([(m["itemId"], m["name"], m["rolls"]) for m in entry["materials"]], [(700031, "first", 5000001), (700030, "second", 4999999)])

	def test_a_cooldown_that_overflows(self):
		# finishCrafting: getCraftDelayTime() * 1000 is int arithmetic before the long addition: 3000000 * 1000 wraps
		self.assertEqual(craft_report(self.gate, recipe=900015)["afterCraft"]["cooldownMillis"], 3000000 * 1000 - 2**32)

	def test_master_upgrades_of_a_gathering_skill(self):
		upgrade = craft_report(self.ctx, skill=30002, level=449)["upgrade"]
		self.assertEqual((upgrade["cost"], upgrade["refusal"], upgrade["maxUpgradableLevel"]), (None, "STR_MSG_DONT_RANK_UP_GATHERING", 399),
		                 "getUpgradeCost(449) is isCrafting() ? 6004900 : null, and 449 > getMaxUpgradableLevel() 399")
		self.assertEqual(craft_report(self.ctx, skill=30002, level=299)["upgrade"]["cost"], 460000)

	def test_a_material_without_an_item_template(self):
		with self.assertRaisesRegex(OracleError, "item 799999 has no item_template.*ItemService.addItem"):
			craft_report(self.ctx, gatherable=400607)  # ItemService.addItem would throw on success

	def test_the_command_line(self):
		with tempfile.TemporaryDirectory() as tmp:
			config = Path(tmp)
			for folder in ("administration", "main", "network"):
				(config / folder).mkdir()
			(config / "main" / "events.properties").write_text("gameserver.event.service.disabled_events = *\n", encoding="iso-8859-1")
			base = ["m5c-craft", "--static-data", str(self.tree.root), "--java-src", str(JAVA_SRC), "--config", str(config)]
			out = io.StringIO()
			with contextlib.redirect_stdout(out):
				code = oracle.main(base + ["--recipe", "900001", "--set", "gameserver.craft.fail.chance=0", "--set", "gameserver.rates.crafting.crit_chances=0"])
			self.assertEqual(code, 0)
			answer = json.loads(out.getvalue())
			self.assertEqual((answer["mode"], answer["craft"]["outcomes"][0]["probability"], answer["config"]["gameserver.craft.fail.chance"]["source"]),
			                 ("recipe", 1.0, "--set"))
			err = io.StringIO()
			with contextlib.redirect_stderr(err), contextlib.redirect_stdout(io.StringIO()):
				self.assertEqual(oracle.main(base + ["--recipe", "900008"]), 2, "OracleError: the product has no template")
			self.assertIn("799999", err.getvalue())
			with contextlib.redirect_stderr(io.StringIO()), contextlib.redirect_stdout(io.StringIO()):
				self.assertEqual(oracle.main(base + ["--skill", "40001"]), 2, "--skill without --level")


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5cCraftRealDataTest(unittest.TestCase):
	"""A new character in Poeta and Ishalgen, and the gate's crafting case (m5c-plan.md §2.10, C15, X19) under the gate profile (D6)."""

	@classmethod
	def setUpClass(cls):
		cls.ctx = CraftContext.create(StaticData(runner.DEFAULT_STATIC_DATA), JAVA_SRC, CONFIG_DIR, None, GATE_PROFILE)

	def test_a_new_character_gathers_only_the_level_1_plant(self):
		# 30001 Collection is the only profession skill below character level 10 (craft_skill_tree.xml; 30003 and 40009 come at 10), at skill
		# level 1; crafting needs a master (character level 10, CraftSkillUpdateService.learnSkill), and no master spawns in Poeta or Ishalgen
		# (they are in the capitals and in Oriel and Pernon)
		for map_id, expected in ((210010000, [(400201, "Impure Iron Ore", 15, False, 20), (400601, "Young Aria", 1, True, 71),
		                                      (400701, "Mela Sapling", 10, False, 20)]),
		                         (220010000, [(400251, "Impure Iron Ore", 15, False), (400651, "Young Azpha", 1, True),
		                                      (400751, "Raydam Sapling", 10, False)])):
			with self.subTest(map=map_id):
				entries = craft_report(self.ctx, skill=30001, level=1, map_id=map_id)["gatherables"]["entries"]
				got = [(e["id"], e["name"], e["skillLevel"], e["canGather"]) + ((e["spots"]["count"],) if map_id == 210010000 else ())
				       for e in entries]
				self.assertEqual(got, expected)
				self.assertTrue(all(e["spots"]["respawnTimes"] == [295] and e["spots"]["spawned"] == [True] for e in entries))
		skill = craft_report(self.ctx, skill=30001, level=1)["skill"]
		self.assertEqual((skill["name"], skill["kind"], skill["skillTree"]), ("Collection", "gathering",
		                                                                      [{"skillId": "30001", "minLevel": "1", "autolearn": "true"}]))

	def test_young_aria_at_skill_level_1(self):
		entry = craft_report(self.ctx, gatherable=400601)["gatherable"]
		self.assertEqual([(m["itemId"], m["name"], m["probability"]) for m in entry["materials"]], [(152000401, "Aria", 1.0)])
		self.assertEqual((entry["xpReward"], entry["count"], entry["harvestCount"], entry["lvlLimit"]), (91, 1, 3, 0))
		up = entry["skillUp"]
		self.assertEqual((up["boostStat"], up["gainedSkillXp"], up["requiredExp"], up["levelAfter"], up["playerExp"]["reward"]),
		                 ("BOOST_ESSENCETAPPING_XP_RATE", 91, 76, 2, 91), "one gather makes Collection 1 -> 2 and gives 91 exp")
		task = entry["task"]
		self.assertEqual((task["timing"]["interval"], task["failureThreshold"], task["executionSpeed"], task["showBarDelay"]), (2500, 33.0, 900, 1200),
		                 "the gate profile sets only the CRAFT fail chance: gathering keeps gameserver.gather.fail.chance 33")
		mela = craft_report(self.ctx, gatherable=400701, skill_level=10)["gatherable"]
		self.assertEqual((mela["xpReward"], mela["skillUp"]["requiredExp"], mela["skillUp"]["levelAfter"], mela["skillUp"]["currentXpAfter"]),
		                 (136, 170, 10, 136), "Mela Sapling at Collection 10: (int) (0.23 * 27.2^2) = 170 > 136, the xp is kept")

	def test_roast_inina_under_the_gate_profile(self):
		report = craft_report(self.ctx, recipe=155001381)
		recipe = report["recipe"]
		self.assertEqual((recipe["skill"]["skillId"], recipe["skill"]["name"], recipe["race"], recipe["skillpoint"], recipe["autolearn"]),
		                 (40001, "Cooking", "ELYOS", 1, 1))
		self.assertEqual(recipe["components"], [[{"itemId": 152001001, "quantity": 1, "name": "Inina"},
		                                         {"itemId": 169400096, "quantity": 2, "name": "Salt"}]])
		self.assertEqual(recipe["product"], {"itemId": 160001001, "quantity": 2, "name": "Roast Inina", "quality": "COMMON"})
		self.assertEqual([(c["itemId"], c["name"]) for c in recipe["comboProducts"]], [(160001051, "Tasty Roast Inina")])
		self.assertEqual([(i["itemId"], i["templatePrice"]) for i in report["learn"]["recipeItems"]], [(152201381, 550)])
		craft = report["craft"]
		self.assertEqual((craft["timing"]["interval"], craft["station"]["effectiveRange"], craft["station"]["packetRange"]), (2500, 5.25, 10))
		self.assertEqual([(o["itemId"], o["quantity"], o["probability"]) for o in craft["outcomes"]], [(160001001, 2, 1.0), (160001051, 2, 0.0)],
		                 "crit chances 0: always the base product (X19)")
		self.assertEqual(craft["outcomes"][0]["finishMillis"], {"fewest": 11000, "most": 36000},
		                 "the plan's 10-36 s is 11-36 s: the first analyze tick is 1000 ms after the start, the last tick one interval after the bar "
		                 "filled")
		bar = craft["bars"][0]
		self.assertEqual((bar["successPerTick"], bar["executionSpeed"], bar["showBarDelay"], bar["steps"]["fewest"], bar["steps"]["most"]),
		                 (1.0, 900, 1200, 4, 14))
		up = report["skillUp"]
		self.assertEqual((up["xpReward"], up["gainedSkillXp"], up["requiredExp"], up["levelAfter"], up["playerExp"]["reward"]), (141, 141, 76, 2, 141),
		                 "m5c-plan.md §2.10: 141 >= 76, cooking 1 -> 2, and 141 exp at the crafting rate 1.0")
		skill = craft_report(self.ctx, skill=40001, level=1)
		self.assertEqual([r["id"] for r in skill["recipes"]["ELYOS"]["autolearn"]], [155001381], "the only Elyos cooking autolearn recipe at 1")
		self.assertEqual(craft_report(self.ctx, skill=40001, level=0)["upgrade"]["cost"], 3500)

	def test_materials_named_twice_and_shadowed_on_the_real_data(self):
		# recipe_templates.xml:87752: 162000012 x10 and x7 in one alternative (a morph recipe); 155101544 (:86966): two alternatives on 169405265
		twice = craft_report(self.ctx, recipe=155101624)["craft"]["materials"]["alternatives"]
		self.assertEqual([(i["itemId"], i["required"], i["consumed"]) for i in twice[0]["items"]], [(162000012, 10, 17)],
		                 "10 held are enough to start, and the craft then takes min(held, 17)")
		shadowed = craft_report(self.ctx, recipe=155101544)["craft"]["materials"]["alternatives"]
		self.assertEqual([(a["firstItemId"], a["selectable"], a["shadowedBy"]) for a in shadowed], [(169405265, True, None), (169405265, False, 0)])


if __name__ == "__main__":
	unittest.main()
