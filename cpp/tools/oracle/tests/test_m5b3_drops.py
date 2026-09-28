"""M5b-3 drop oracle (m5b3/drops.py, m5b3-plan.md G-01): the float and Rnd arithmetic alone, the AI ask() parser, the Java tables as they stand
today and on an edited copy, the whole report on a small static_data tree with fixture AI and quest handler classes, and the M5b gate's monster
210663, the kinah monster 210133 and the Poeta survey on the real data (m5b3-plan.md §2.4).

Expected values are derived by hand from the Java sources named in m5b3/drops.py and repeated per case. Everything that reads the Java source
tree is skipped without it, like M5bFixtureReportTest.
"""

import contextlib
import io
import json
import shutil
import tempfile
import unittest
from fractions import Fraction
from functools import lru_cache
from pathlib import Path

from m5a.data import StaticData
from m5a.javafloat import f32
from m5b3.drops import (AiCatalog, DropData, JavaDropRules, _ask_answers, _custom_group_avoidance, _custom_group_distribution, chance_probability,
                        drops_report, enum_names, java_float, map_survey, select_element_probabilities, selection_avoidance, selection_inclusion)
from staticdata_oracle import OracleError
from staticdata_oracle import run as runner

import oracle

from .support import Tree

JAVA_SRC = runner.TOOL_DIR.parents[2] / "game-server" / "src"
JAVA_HANDLERS = runner.TOOL_DIR.parents[2] / "game-server" / "data" / "handlers"
HAVE_JAVA_TREE = (JAVA_SRC / "com" / "aionemu" / "gameserver").is_dir() and JAVA_HANDLERS.is_dir() and runner.DEFAULT_STATIC_DATA.is_dir()
LATTICE = 2 ** 24


class M5b3ArithmeticTest(unittest.TestCase):
	"""Float parsing, the Rnd lattice, Chance.selectElement, the custom group draw and the ask() parser, without any Java source or data."""

	def test_java_float_rounds_the_decimal_once(self):
		# Float.parseFloat rounds the decimal to the nearest float; through a double first, 1 + 2^-24 + 2.5e-17 lands exactly on the midpoint
		# 1 + 2^-24 and ties to even (1.0f) - the wrong answer
		self.assertEqual(java_float("1.0000000596046448", "x"), 1.0000001192092896)
		self.assertEqual(f32(float("1.0000000596046448")), 1.0, "the double rounding java_float avoids")
		self.assertEqual(java_float("3.5", "x"), 3.5)
		self.assertEqual(java_float("0.01", "x"), f32(0.01))
		self.assertEqual(java_float(None, "x", 100.0), 100.0, "the JAXB field default")
		for bad in ("1,5", "INF", "", "0x10"):
			with self.subTest(bad=bad), self.assertRaises(OracleError):
				java_float(bad, "x")

	def test_rnd_chance_lattice(self):
		# Rnd.chance() = f32((k * 2^-24) * 100), k in [0, 2^24); P(chance() < t) is the smallest k whose value reaches t, over 2^24
		self.assertEqual(chance_probability(50.0), Fraction(1, 2), "k * 100 / 2^24 < 50 exactly for k < 2^23")
		# ceil(3.5 * 2^24 / 100) = 587203; k = 587202 gives 3.4999967f, below 3.5
		self.assertEqual(chance_probability(3.5), Fraction(587203, LATTICE))
		# 0.01f = 0.0099999998f: ceil(0.0099999998 * 2^24 / 100) = 1678
		self.assertEqual(chance_probability(f32(0.01)), Fraction(1678, LATTICE))
		self.assertEqual(chance_probability(100.0), 1, "nextFloat(100f) never returns 100: its largest value is nextDown(100f)")
		self.assertEqual(chance_probability(100000.0), 1)
		self.assertEqual(chance_probability(0.0), 0, "`Rnd.chance() >= 0` always skips")
		self.assertEqual(chance_probability(-5.0), 0)
		self.assertEqual(chance_probability(99.99999237060547), Fraction(LATTICE - 1, LATTICE), "only the largest value, nextDown(100f), reaches it")

	def test_select_element(self):
		# Chance.selectElement: randomChance = nextFloat(sum) and the first running sum >= it; two equal weights are not exactly 1/2 each
		# because the value at k = 2^23, 0.5f * 200f = 100f, is the first element's running sum (<=)
		self.assertEqual(select_element_probabilities([100.0, 100.0]), [Fraction(LATTICE // 2 + 1, LATTICE), Fraction(LATTICE // 2 - 1, LATTICE)])
		self.assertEqual(select_element_probabilities([100.0]), [1])
		self.assertEqual(sum(select_element_probabilities([100.0, 100.0, 200.0])), 1)
		self.assertIsNone(select_element_probabilities([0.0, 0.0]), "a sum of 0: selectElement returns null")

	def test_selection_inclusion(self):
		# collectDrops with max_drop_rule 2 over weights 100, 100, 200: C is first with 1/2, else second with 2/3 -> 1/2 + 2 * 1/4 * 2/3 = 5/6;
		# A is first with 1/4, second after B with 1/4 * 1/3 and after C with 1/2 * 1/2 -> 7/12 (up to the lattice)
		inclusion = selection_inclusion([100.0, 100.0, 200.0], 2)
		self.assertAlmostEqual(float(inclusion[0]), 7 / 12, places=6)
		self.assertAlmostEqual(float(inclusion[1]), 7 / 12, places=6)
		self.assertAlmostEqual(float(inclusion[2]), 5 / 6, places=6)
		self.assertEqual(sum(inclusion), 2, "exactly two picks")
		self.assertEqual(selection_inclusion([100.0, 100.0], 5), [1, 1], "more picks than candidates take them all")

	def test_custom_group_draw(self):
		# DropGroup.tryAddDropItems, max_items 2, final chances 100 and 50: roll 1 below 50 takes the 50 drop (the nearest above the roll), above
		# 50 the 100 drop; roll 2 then takes the other one for sure (100) or with 1/2 (50)
		distribution, inclusion = _custom_group_distribution([100.0, 50.0], 2)
		self.assertEqual(distribution, {1: Fraction(1, 4), 2: Fraction(3, 4)})
		self.assertEqual(inclusion, [1, Fraction(3, 4)])
		distribution, inclusion = _custom_group_distribution([40.0, 40.0, 40.0], 1)
		self.assertEqual(distribution[1], chance_probability(40.0), "one roll below 40 takes one of the three tied drops")
		self.assertEqual(inclusion[0], chance_probability(40.0) / 3)
		self.assertEqual(_custom_group_distribution([0.0], 3)[0], {0: 1}, "a final chance of 0 is never above a roll")
		self.assertEqual(_custom_group_distribution([100.0], -1), ({0: 1}, [0]), "max_items -1: tryAddDropItems' loop does not run")

	def test_loot_effect_avoidance(self):
		# collectDrops' draw: the probability that one marked candidate is NOT drawn is exactly 1 - its inclusion probability
		weights = [100.0, 100.0, 200.0]
		inclusion = selection_inclusion(weights, 2)
		for marked in range(3):
			self.assertEqual(selection_avoidance(weights, 2, {marked}), 1 - inclusion[marked])
		self.assertEqual(selection_avoidance(weights, 2, {0, 1}), 0, "two picks of three always draw 0 or 1")
		self.assertEqual(selection_avoidance(weights, 1, {0, 1}), select_element_probabilities(weights)[2])
		self.assertEqual(selection_avoidance(weights, 0, {0}), 1, "max_drop_rule 0 draws nothing")
		# the custom group of test_custom_group_draw: the 50 drop is taken with 3/4, the 100 drop always
		self.assertEqual(_custom_group_avoidance([100.0, 50.0], [False, True], 2), Fraction(1, 4))
		self.assertEqual(_custom_group_avoidance([100.0, 50.0], [True, False], 2), 0)
		self.assertEqual(_custom_group_avoidance([40.0, 40.0, 40.0], [True, False, False], 1), 1 - chance_probability(40.0) / 3,
		                 "a roll below 40 takes one of the three tied drops uniformly")
		self.assertEqual(_custom_group_avoidance([100.0], [True], -1), 1, "no roll")

	def test_ask_parser(self):
		self.assertIsNone(_ask_answers("class A extends NpcAI { void x() {} }"), "no ask(): the parent decides")
		self.assertEqual(_ask_answers("public boolean ask(AIQuestion question) { return false; }"),
		                 {"REWARD_AP_XP_DP_LOOT": "false", "REWARD_LOOT": "false", "ALLOW_DECAY": "false"})
		self.assertEqual(_ask_answers("boolean ask(AIQuestion q) { return switch (q) { case REWARD_LOOT, ALLOW_DECAY -> false; "
		                              "default -> super.ask(q); }; }"),
		                 {"REWARD_AP_XP_DP_LOOT": "super", "REWARD_LOOT": "false", "ALLOW_DECAY": "false"})
		# a nested switch in an arm is that arm's expression: not modelled for its labels, and its `default` is not the outer default
		self.assertEqual(_ask_answers("boolean ask(AIQuestion q) { return switch (q) { case REWARD_LOOT -> switch (getNpcId()) { case 1 -> false; "
		                              "default -> true; }; default -> super.ask(q); }; }"),
		                 {"REWARD_AP_XP_DP_LOOT": "super", "REWARD_LOOT": None, "ALLOW_DECAY": "super"})
		self.assertEqual(_ask_answers("boolean ask(AIQuestion q) { return switch (q) { case REWARD_AP -> { yield x > 1; } case CAN_SHOUT -> true; }; }"),
		                 {"REWARD_AP_XP_DP_LOOT": None, "REWARD_LOOT": None, "ALLOW_DECAY": None}, "no default arm: unmodelled")
		self.assertEqual(_ask_answers("boolean ask(AIQuestion q) { if (q == AIQuestion.REWARD_LOOT) return false; return super.ask(q); }"), "unparsed")
		self.assertEqual(_ask_answers("boolean ask(AIQuestion q) { switch (q) { case REWARD_LOOT: return false; } return super.ask(q); }"), "unparsed")

	def test_enum_names(self):
		with tempfile.TemporaryDirectory() as root:
			path = Path(root) / "E.java"
			path.write_text("public enum E {\n\tA,\n\t// B,\n\tC(1, \"x,y\"),\n\t@Deprecated D\n}\n", encoding="utf-8")  # no `;`, like GroupDropType
			self.assertEqual(enum_names(path, "E"), ("A", "C", "D"))
			path.write_text("enum E { A(1), B(2); private int x; }", encoding="utf-8")
			self.assertEqual(enum_names(path, "E"), ("A", "B"))
			with self.assertRaises(OracleError):
				enum_names(path, "F")


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5b3JavaRulesTest(unittest.TestCase):
	"""The tables the evaluator reads from the Java sources, as they stand today."""

	@classmethod
	def setUpClass(cls):
		cls.rules = JavaDropRules.read(JAVA_SRC)

	def test_modifiers_and_literals(self):
		r = self.rules
		self.assertEqual(r.rank_modifiers, {"NOVICE": f32(0.9), "DISCIPLINED": 1.0, "SEASONED": f32(1.05), "EXPERT": f32(1.1), "VETERAN": f32(1.15),
		                                    "MASTER": f32(1.2)}, "DropRegistrationService.getRankModifier")
		self.assertEqual(r.rating_modifiers, {"JUNK": 0.5, "NORMAL": 1.0, "ELITE": f32(1.3), "HERO": f32(1.8), "LEGENDARY": 2.0})
		self.assertEqual((r.kinah_exponent, r.kinah_item), (6, 182400001), "Math.pow(rank * rating, 6), ItemId.KINAH")
		self.assertEqual((r.chest_ai, r.chest_prefix, r.chest_suffix, r.quest_ai), ("chest", "treasure", "box", "quest_use_item"))
		self.assertEqual((r.min_default_level, r.default_level_maps), (2, (210010000, 220010000)), "level < 2 outside POETA and ISHALGEN")
		self.assertEqual(r.base_boost, 100)
		self.assertEqual(r.drop_reward, {-10: 0, -9: 40, -8: 60, -7: 70, -6: 80, -5: 100})
		self.assertEqual([r.drop_reward_from(d) for d in (-30, -10, -7, -5, 0, 12)], [0, 0, 70, 100, 100, 100])
		self.assertEqual((r.loot_effects[168000213], r.loot_effects[188053547], r.loot_effects.get(168000212, 0)), (1003, 1002, 0))
		self.assertEqual(r.no_ai, "__NO_AI__")
		self.assertEqual(r.drop_rates_default, "1.0, 2.0", "RatesConfig.DROP_RATES")
		self.assertEqual(r.rule_defaults, {"minDiff": -99, "maxDiff": 99, "memberLimit": 1, "maxDropRule": 1})
		self.assertEqual(r.enums["RestrictionRace"], ("ASMODIANS", "ELYOS"))
		self.assertIn("SPAKY", r.enums["GroupDropType"], "GroupDropType ends without `;`")
		self.assertEqual(len(r.enums["GroupDropType"]), len(set(r.enums["GroupDropType"])))
		self.assertEqual(r.teleporter_rewrite, (1, "noaction", "TELEPORTER", "siege_teleporter"), "NpcTemplate.afterUnmarshal")
		self.assertEqual(r.repose_level, 10, "PlayerCommonData.isReadyForReposeEnergy")

	def test_ai_chains(self):
		catalog = AiCatalog(JAVA_SRC, JAVA_HANDLERS)
		aggressive = catalog.drop_behaviour("aggressive")
		self.assertEqual(aggressive["classChain"], ["AggressiveNpcAI", "GeneralNpcAI", "NpcAI", "AITemplate"])
		self.assertEqual((aggressive["REWARD_AP_XP_DP_LOOT"], aggressive["REWARD_LOOT"], aggressive["ALLOW_DECAY"]), (True, True, True),
		                 "NpcAI.ask: case ALLOW_DECAY, REWARD_AP_XP_DP_LOOT, REWARD_LOOT -> true")
		self.assertEqual(catalog.drop_behaviour(None)["REWARD_LOOT"], False, "DummyAI: AITemplate.ask answers false")
		no_loot = catalog.drop_behaviour("aggressive_no_loot")  # AggressiveNoLootNpcAI: case REWARD_LOOT, ALLOW_DECAY -> false
		self.assertEqual((no_loot["REWARD_AP_XP_DP_LOOT"], no_loot["REWARD_LOOT"], no_loot["ALLOW_DECAY"]), (True, False, False))
		self.assertEqual(no_loot["classChain"][:2], ["AggressiveNoLootNpcAI", "AggressiveNpcAI"])
		self.assertEqual(aggressive["registersDropItself"], [])
		# the AIs that call registerDrop themselves, on use (ChestAI.java:68, QuestItemNpcAI.java:68)
		self.assertEqual(catalog.drop_behaviour("chest")["registersDropItself"], ["ChestAI"])
		self.assertEqual(catalog.drop_behaviour("quest_use_item")["registersDropItself"], ["QuestItemNpcAI"])
		with self.assertRaises(OracleError):
			catalog.drop_behaviour("halloween_pumpkin")  # overrides handleDropRegistered
		with self.assertRaises(OracleError):
			catalog.drop_behaviour("no such ai")  # AIEngine.newAI throws


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5b3JavaRulesShapeTest(unittest.TestCase):
	"""JavaDropRules.read on a copy of the Java files it reads, one of them changed: the literals are read, the modelled statements required."""

	BASE = ("com", "aionemu", "gameserver")
	FILES = ("services/drop/DropRegistrationService.java", "model/drop/DropModifiers.java", "model/drop/NpcDrop.java", "model/drop/DropGroup.java",
	         "model/drop/DropItem.java", "model/Chance.java", "controllers/NpcController.java", "dataholders/GlobalDropData.java",
	         "services/QuestService.java", "world/WorldMapType.java", "model/items/ItemId.java", "utils/stats/DropRewardEnum.java",
	         "model/templates/spawns/SpawnTemplate.java", "configs/main/RatesConfig.java", "model/templates/globaldrops/GlobalRule.java",
	         "model/templates/globaldrops/StringFunction.java", "model/Race.java", "model/TribeClass.java", "model/templates/npc/NpcRating.java",
	         "model/templates/npc/NpcRank.java", "model/templates/npc/GroupDropType.java", "model/templates/npc/NpcTemplateType.java",
	         "model/templates/npc/AbyssNpcType.java", "world/WorldDropType.java", "model/templates/npc/NpcTemplate.java",
	         "model/gameobjects/player/PlayerCommonData.java")

	def rules_with(self, relative: str, old: str, new: str) -> JavaDropRules:
		with tempfile.TemporaryDirectory() as root:
			source, target = JAVA_SRC.joinpath(*self.BASE), Path(root).joinpath(*self.BASE)
			for file in self.FILES:
				(target / file).parent.mkdir(parents=True, exist_ok=True)
				shutil.copyfile(source / file, target / file)
			changed = target / relative
			text = changed.read_text(encoding="utf-8")
			self.assertIn(old, text, f"{relative} no longer contains the text this case changes")
			changed.write_text(text.replace(old, new, 1), encoding="utf-8")
			return JavaDropRules.read(Path(root))

	def test_literals_are_read(self):
		rules = self.rules_with("services/drop/DropRegistrationService.java", "case NOVICE -> 0.9f;", "case NOVICE -> 0.95f;")
		self.assertEqual(rules.rank_modifiers["NOVICE"], f32(0.95))
		rules = self.rules_with("utils/stats/DropRewardEnum.java", "MINUS_6(-6, 80)", "MINUS_6(-6, 85)")
		self.assertEqual(rules.drop_reward_from(-6), 85)
		rules = self.rules_with("configs/main/RatesConfig.java", 'key = "gameserver.rates.drop", defaultValue = "1.0, 2.0"',
		                        'key = "gameserver.rates.drop", defaultValue = "3.0"')
		self.assertEqual(rules.drop_rates_default, "3.0")
		rules = self.rules_with("model/templates/npc/NpcTemplate.java", "if (level > 1 &&", "if (level > 5 &&")
		self.assertEqual(rules.teleporter_rewrite[0], 5)
		rules = self.rules_with("model/gameobjects/player/PlayerCommonData.java", "return getLevel() >= 10;", "return getLevel() >= 12;")
		self.assertEqual(rules.repose_level, 12)

	def test_a_changed_statement_is_refused(self):
		cases = [
			("services/drop/DropRegistrationService.java", "if (Rnd.chance() >= chance)", "if (Rnd.chance() > chance)"),
			("services/drop/DropRegistrationService.java", "if (diff >= rule.getMinDiff() && diff <= rule.getMaxDiff())",
			 "if (diff > rule.getMinDiff() && diff <= rule.getMaxDiff())"),
			("model/drop/DropModifiers.java", "return chance * boostDropRate;", "return chance * boostDropRate * 2;"),
			("model/Chance.java", "if (randomChance <= luck)", "if (randomChance < luck)"),
			("controllers/NpcController.java", "getOwner().getAi().ask(AIQuestion.REWARD_LOOT)", "getOwner().getAi().ask(AIQuestion.REWARD_AP)"),
			("services/drop/DropRegistrationService.java", "Math.pow(getRankModifier(npc) * getRatingModifier(npc), 6)",
			 "Math.pow(getRankModifier(npc) + getRatingModifier(npc), 6)"),
			("model/drop/DropItem.java", "case 188053083 -> 1003;", "case 188053083 -> lootEffect();"),
			("model/templates/npc/NpcTemplate.java", 'ai = "siege_teleporter";', "ai = siegeAi();"),
			("model/templates/npc/NpcTemplate.java", "return abyssNpcType != null ? abyssNpcType : AbyssNpcType.NONE;", "return abyssNpcType;"),
			("model/gameobjects/player/PlayerCommonData.java", "if (!isReadyForReposeEnergy()) {", "if (isReadyForReposeEnergy()) {"),
		]
		for relative, old, new in cases:
			with self.subTest(file=relative, new=new), self.assertRaises(OracleError):
				self.rules_with(relative, old, new)


# ---------------------------------------------------------------------------------------------------------------------------------------------
# The fixture: map 1 is an ELYSEA drop map, map 2 has no drop_type (NONE), map 3 has an instance handler, 210010000 has Poeta's id

NPCS = """
<npc_template npc_id="900001" name="fixture beast" level="3" rank="SEASONED" rating="ELITE" race="LYCAN" tribe="MONSTER" group_drop="SPAKY" ai="aggressive"/>
<npc_template npc_id="900002" name="fixture citizen" level="5" rank="NOVICE" rating="NORMAL" race="ELYOS" tribe="GENERAL" group_drop="NONE" ai="aggressive" type="GENERAL"/>
<npc_template npc_id="900003" name="fixture hatchling" level="1" rank="NOVICE" rating="NORMAL" race="LYCAN" tribe="MONSTER" group_drop="SPAKY" ai="aggressive"/>
<npc_template npc_id="900004" name="fixture quest object" level="3" rank="NOVICE" rating="NORMAL" group_drop="NONE" ai="quest_use_item"/>
<npc_template npc_id="900005" name="zoned beast" level="3" rank="SEASONED" rating="ELITE" race="LYCAN" tribe="MONSTER" group_drop="SPAKY" ai="aggressive"/>
<npc_template npc_id="900006" name="fixture keeper" level="3" rank="NOVICE" rating="NORMAL" group_drop="SPAKY" ai="noloot"/>
<npc_template npc_id="900007" name="fixture chest" level="3" rank="NOVICE" rating="NORMAL" group_drop="TREASUREBOX" ai="aggressive"/>
<npc_template npc_id="900008" name="fixture unranked" level="3" rating="NORMAL" race="LYCAN" group_drop="SPAKY" ai="aggressive"/>
<npc_template npc_id="900009" name="fixture iffy" level="3" rank="NOVICE" rating="NORMAL" group_drop="SPAKY" ai="iffy"/>
<npc_template npc_id="900010" name="fixture hooked" level="3" rank="NOVICE" rating="NORMAL" group_drop="SPAKY" ai="hooked"/>
<npc_template npc_id="900011" name="fixture groupless" level="3" rank="NOVICE" rating="NORMAL" ai="aggressive"/>
<npc_template npc_id="900012" name="fixture mixed" level="3" rank="NOVICE" rating="NORMAL" group_drop="SPAKY" ai="aggressive"/>
<npc_template npc_id="900013" name="fixture mute" level="3" rank="NOVICE" rating="NORMAL" group_drop="SPAKY" ai="aggressive"/>
<npc_template npc_id="900014" name="fixture yearling" level="2" rank="NOVICE" rating="NORMAL" race="LYCAN" tribe="MONSTER" group_drop="SPAKY" ai="aggressive"/>
<npc_template npc_id="900015" name="fixture defender" level="3" rank="NOVICE" rating="NORMAL" race="LYCAN" group_drop="SPAKY" ai="aggressive" abyss_type="DEFENDER"/>
<npc_template npc_id="900016" name="fixture guard" level="3" rank="NOVICE" rating="NORMAL" race="LYCAN" group_drop="SPAKY" ai="aggressive" abyss_type="GUARD"/>
<npc_template npc_id="900017" name="fixture teleporter" level="3" rank="NOVICE" rating="NORMAL" group_drop="NONE" ai="aggressive" abyss_type="TELEPORTER"/>
<npc_template npc_id="900018" name="fixture tamed" level="3" rank="NOVICE" rating="NORMAL" group_drop="SPAKY" ai="noreward"/>
<npc_template npc_id="900019" name="fixture husk" level="3" rank="NOVICE" rating="NORMAL" group_drop="SPAKY" ai="nolootdecay"/>
<npc_template npc_id="900020" name="fixture coffer" level="3" rank="NOVICE" rating="NORMAL" group_drop="SPAKY" ai="coffer"/>
<npc_template npc_id="900021" name="fixture sentry" level="3" rank="NOVICE" rating="NORMAL" race="LYCAN" group_drop="SPAKY" ai="aggressive"/>
<npc_template npc_id="900022" name="fixture ferry" level="1" group_drop="NONE" ai="aggressive" abyss_type="TELEPORTER"/>
<npc_template npc_id="900023" name="fixture gate" level="3" group_drop="NONE" ai="noaction" abyss_type="TELEPORTER"/>
"""

SPAWNS = """
<spawn_map map_id="1">
	<spawn npc_id="900001" respawn_time="30"><spot x="1" y="1" z="1" h="0"/><spot x="2" y="1" z="1" h="0" ai="aggressive"/></spawn>
	<spawn npc_id="900002"><spot x="3" y="1" z="1"/></spawn>
	<spawn npc_id="900003"><spot x="4" y="1" z="1"/></spawn>
	<spawn npc_id="900004"><spot x="5" y="1" z="1"/></spawn>
	<spawn npc_id="900005"><spot x="6" y="1" z="1"/></spawn>
	<spawn npc_id="900006"><spot x="7" y="1" z="1"/></spawn>
	<spawn npc_id="900007"><spot x="8" y="1" z="1"/></spawn>
	<spawn npc_id="900008"><spot x="9" y="1" z="1"/></spawn>
	<spawn npc_id="900009"><spot x="10" y="1" z="1"/></spawn>
	<spawn npc_id="900010"><spot x="11" y="1" z="1"/></spawn>
	<spawn npc_id="900011"><spot x="12" y="1" z="1"/></spawn>
	<spawn npc_id="900012"><spot x="13" y="1" z="1"/><spot x="14" y="1" z="1" ai="noloot"/></spawn>
	<spawn npc_id="900013"><spot x="15" y="1" z="1" ai="__NO_AI__"/></spawn>
	<spawn npc_id="900014"><spot x="16" y="1" z="1"/></spawn>
	<spawn npc_id="900015"><spot x="17" y="1" z="1"/></spawn>
	<spawn npc_id="900016"><spot x="18" y="1" z="1"/></spawn>
	<spawn npc_id="900017"><spot x="19" y="1" z="1"/></spawn>
	<spawn npc_id="900018"><spot x="20" y="1" z="1"/></spawn>
	<spawn npc_id="900019"><spot x="21" y="1" z="1"/></spawn>
	<spawn npc_id="900020"><spot x="22" y="1" z="1"/></spawn>
	<spawn npc_id="900021"><spot x="23" y="1" z="1"/></spawn>
</spawn_map>
<spawn_map map_id="2"><spawn npc_id="900001"><spot x="1" y="1" z="1"/></spawn></spawn_map>
<spawn_map map_id="3"><spawn npc_id="900001"><spot x="1" y="1" z="1"/></spawn></spawn_map>
<spawn_map map_id="210010000"><spawn npc_id="900003"><spot x="1" y="1" z="1"/></spawn></spawn_map>
"""

ITEMS = """
<item_template id="182400001" name="kinah" level="1"/>
<item_template id="500" name="fixture junk" level="1" max_stack_count="100"/>
<item_template id="501" name="fixture sword" level="3" option_slot_bonus="1"/>
<item_template id="502" name="fixture asmodian ring" level="3" race="ASMODIANS"/>
<item_template id="503" name="fixture high helm" level="9"/>
<item_template id="504" name="fixture low boots" level="1"/>
<item_template id="168000213" name="fixture godstone" level="1" quality="LEGEND"/>
<item_template id="600" name="fixture handler item"/>
<item_template id="700" name="fixture quest item"/>
<item_template id="800" name="fixture custom a"/>
<item_template id="801" name="fixture custom b"/>
<item_template id="900" name="fixture named sword" level="3"/>
<item_template id="901" name="fixture festival gift"/>
"""

# rule indexes 0..18 in this order
RULES = """
<gd_rule rule_name="Kinah" chance="50" dynamic_chance="true"><gd_races><gd_race race="LYCAN"/></gd_races>
	<gd_items><gd_item id="182400001" min_count="5" max_count="7"/></gd_items></gd_rule>
<gd_rule rule_name="JUNK_SPAKY" chance="40"><gd_npc_groups><gd_npc_group group="SPAKY"/></gd_npc_groups>
	<gd_items><gd_item id="500" min_count="2" max_count="4"/></gd_items></gd_rule>
<gd_rule rule_name="Picks" chance="10" max_drop_rule="2"><gd_items><gd_item id="501"/><gd_item id="504"/><gd_item id="168000213" chance="200"/></gd_items></gd_rule>
<gd_rule rule_name="Window" chance="100" min_diff="-5" max_diff="2"><gd_items><gd_item id="503"/><gd_item id="504"/><gd_item id="500"/></gd_items></gd_rule>
<gd_rule rule_name="Race filter" chance="5"><gd_items><gd_item id="502"/></gd_items></gd_rule>
<gd_rule rule_name="Asmodian only" chance="5" restriction_race="ASMODIANS"><gd_items><gd_item id="500"/></gd_items></gd_rule>
<gd_rule rule_name="Other map" chance="5"><gd_maps><gd_map map_id="999"/></gd_maps><gd_items><gd_item id="500"/></gd_items></gd_rule>
<gd_rule rule_name="Named" chance="90"><gd_npcs><gd_npc npc_id="900001"/><gd_npc npc_id="900003"/></gd_npcs><gd_items><gd_item id="900"/></gd_items></gd_rule>
<gd_rule rule_name="By name" chance="20"><gd_npc_names><gd_npc_name function="START_WITH" value="Fixture B"/></gd_npc_names>
	<gd_items><gd_item id="504"/></gd_items></gd_rule>
<gd_rule rule_name="No name match" chance="1"><gd_npc_names><gd_npc_name function="CONTAINS" value="nobody"/></gd_npc_names>
	<gd_items><gd_item id="500"/></gd_items></gd_rule>
<gd_rule rule_name="Zoned" chance="5"><gd_zones><gd_zone zone="FIXTURE_ZONE"/></gd_zones><gd_npcs><gd_npc npc_id="900005"/></gd_npcs>
	<gd_items><gd_item id="500"/></gd_items></gd_rule>
<gd_rule rule_name="Reduced" chance="50" level_based_chance_reduction="true"><gd_items><gd_item id="504"/></gd_items></gd_rule>
<gd_rule rule_name="Asmodae world" chance="5"><gd_worlds><gd_world wd_type="ASMODAE"/></gd_worlds><gd_items><gd_item id="500"/></gd_items></gd_rule>
<gd_rule rule_name="Normal only" chance="5"><gd_ratings><gd_rating rating="NORMAL"/></gd_ratings><gd_items><gd_item id="500"/></gd_items></gd_rule>
<gd_rule rule_name="Excluded" chance="5"><gd_excluded_npcs npc_ids="900001 900002"/><gd_items><gd_item id="500"/></gd_items></gd_rule>
<gd_rule rule_name="Tribe" chance="5"><gd_tribes><gd_tribe tribe="AGGRESSIVEMONSTER"/></gd_tribes><gd_items><gd_item id="500"/></gd_items></gd_rule>
<gd_rule rule_name="Equals name" chance="30"><gd_npc_names><gd_npc_name function="EQUALS" value="FIXTURE Beast"/></gd_npc_names>
	<gd_items><gd_item id="504"/></gd_items></gd_rule>
<gd_rule rule_name="Zoned ring" chance="5"><gd_zones><gd_zone zone="FIXTURE_ZONE"/></gd_zones><gd_npcs><gd_npc npc_id="900021"/></gd_npcs>
	<gd_items><gd_item id="502"/></gd_items></gd_rule>
<gd_rule rule_name="Twins" chance="5" max_drop_rule="2"><gd_npcs><gd_npc npc_id="900021"/></gd_npcs>
	<gd_items><gd_item id="500"/><gd_item id="500"/><gd_item id="504"/></gd_items></gd_rule>
"""

CUSTOM = """
<npc_drop npc_id="900001">
	<drop_group name="fixture group" max_items="2"><drop item_id="800" chance="100"/><drop item_id="801" chance="50" min_amount="2" max_amount="3"/></drop_group>
	<drop_group race="ASMODIANS"><drop item_id="801"/></drop_group>
</npc_drop>
<npc_drop npc_id="900001"><drop_group><drop item_id="999999"/></drop_group></npc_drop>
<npc_drop npc_id="900014"><drop_group max_items="-1"><drop item_id="800"/></drop_group>
	<drop_group><drop item_id="168000213" chance="50"/></drop_group></npc_drop>
"""

QUESTS = """
<quest id="9000" name="fixture collect"><collect_items><collect_item item_id="700" count="3"/></collect_items>
	<quest_drop npc_id="900001" item_id="700" chance="30"/></quest>
<quest id="9001" name="fixture alliance" target="ALLIANCE"/>
<quest id="9002" name="fixture mentee" mentor_type="MENTE"><quest_drop npc_id="900001" item_id="700" collecting_step="2" drop_each_member="1"/></quest>
"""

AI_CLASSES = {
	"FixtureAggressiveAI": '@AIName("aggressive")\npublic class FixtureAggressiveAI extends NpcAI {\n}\n',
	"FixtureNoLootAI": '@AIName("noloot")\npublic class FixtureNoLootAI extends FixtureAggressiveAI {\n\t@Override\n\tpublic boolean ask(AIQuestion question) '
	                   '{\n\t\treturn switch (question) {\n\t\t\tcase REWARD_LOOT, ALLOW_DECAY -> false;\n\t\t\tdefault -> super.ask(question);\n\t\t};\n\t}\n}\n',
	"FixtureQuestItemAI": '@AIName("quest_use_item")\npublic class FixtureQuestItemAI extends NpcAI {\n}\n',
	"FixtureIffyAI": '@AIName("iffy")\npublic class FixtureIffyAI extends NpcAI {\n\tpublic boolean ask(AIQuestion question) {\n\t\tif (question == '
	                 'AIQuestion.REWARD_LOOT)\n\t\t\treturn false;\n\t\treturn super.ask(question);\n\t}\n}\n',
	"FixtureHookedAI": '@AIName("hooked")\npublic class FixtureHookedAI extends NpcAI {\n\t@Override\n\tprotected void handleDropRegistered() {\n\t}\n}\n',
	"FixtureNoRewardAI": '@AIName("noreward")\npublic class FixtureNoRewardAI extends NpcAI {\n\t@Override\n\tpublic boolean ask(AIQuestion question) {\n'
	                     '\t\treturn switch (question) {\n\t\t\tcase REWARD_AP_XP_DP_LOOT -> false;\n\t\t\tdefault -> super.ask(question);\n\t\t};\n\t}\n}\n',
	"FixtureHuskAI": '@AIName("nolootdecay")\npublic class FixtureHuskAI extends NpcAI {\n\t@Override\n\tpublic boolean ask(AIQuestion question) {\n'
	                 '\t\treturn switch (question) {\n\t\t\tcase REWARD_LOOT -> false;\n\t\t\tdefault -> super.ask(question);\n\t\t};\n\t}\n}\n',
	"FixtureSiegeTeleporterAI": '@AIName("siege_teleporter")\npublic class FixtureSiegeTeleporterAI extends NpcAI {\n\t@Override\n'
	                            '\tpublic boolean ask(AIQuestion question) {\n\t\treturn false;\n\t}\n}\n',
	# like ChestAI: registers the drop itself on use, and answers REWARD_LOOT false so that the kill path registers nothing
	"FixtureCofferAI": '@AIName("coffer")\npublic class FixtureCofferAI extends NpcAI {\n\t@Override\n\tpublic boolean ask(AIQuestion question) {\n'
	                   '\t\treturn switch (question) {\n\t\t\tcase REWARD_LOOT -> false;\n\t\t\tdefault -> super.ask(question);\n\t\t};\n\t}\n\n'
	                   '\t@Override\n\tprotected void handleUseItemFinish(Player player) {\n\t\tDropRegistrationService.getInstance().registerDrop(getOwner(), '
	                   'player, player.getLevel(), List.of(player));\n\t\tAIActions.die(this, player);\n\t}\n}\n',
}

QUEST_HANDLER = ("package quest;\n\npublic class _9001Fixture extends AbstractQuestHandler {\n\n\tpublic _9001Fixture() {\n\t\tsuper(9001);\n\t}\n\n"
                 "\t@Override\n\tpublic void register() {\n\t\tqe.addHandlerSideQuestDrop(questId, 900001, 600, 3, 50);\n\t}\n}\n")


def fixture_tree(quest_handler: str = QUEST_HANDLER) -> Tree:
	tree = Tree()
	tree.minimal({
		"world_maps": '<map id="1" name="fixture" world_type="ELYSEA" drop_type="ELYSEA" world_size="1024"/>'
		              '<map id="2" world_type="ELYSEA" world_size="1024"/>'
		              '<map id="3" world_type="ASMODAE" drop_type="ASMODAE" world_size="1024"/>'
		              '<map id="210010000" world_type="ELYSEA" drop_type="ELYSEA" world_size="1024"/>',
		"npc_templates": NPCS,
		"spawns": SPAWNS,
		"item_templates": ITEMS,
		"global_rules": RULES,
		"custom_drop": CUSTOM,
		"quests": QUESTS,
		"global_npc_exclusions": "<npc_ids>900099</npc_ids><npc_types>GENERAL</npc_types><npc_tribes>DUMMY</npc_tribes>",
		"timed_events": '<event name="Fixture Festival"><event_drops><gd_rule rule_name="Festival" chance="100"><gd_items><gd_item id="901"/>'
		                '</gd_items></gd_rule></event_drops></event>',
		"player_experience_table": "".join(f"<exp>{e}</exp>" for e in range(0, 10000, 1000)),
	})
	for name, text in AI_CLASSES.items():
		tree.write(f"handlers/ai/{name}.java", "package ai;\n\n" + text)
	tree.write("handlers/quest/_9001Fixture.java", quest_handler)
	tree.write("handlers/instance/FixtureInstance.java", "@InstanceID(3)\npublic class FixtureInstance extends GeneralInstanceHandler {}\n")
	return tree


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5b3FixtureReportTest(unittest.TestCase):
	"""The whole report on a small static_data tree (the Java tables and NpcAI/AITemplate still come from the Java sources)."""

	@classmethod
	def setUpClass(cls):
		cls.tree = fixture_tree()
		cls.drops = DropData(StaticData(cls.tree.root), JAVA_SRC, cls.tree.root / "handlers")

	@classmethod
	def tearDownClass(cls):
		cls.tree.close()

	def report(self, npc_id=900001, map_id=1, **kwargs):
		return drops_report(self.drops, npc_id, map_id, **kwargs)

	@staticmethod
	def rule(report, name):
		found = [r for r in report["rules"] if r["ruleName"] == name]
		if len(found) != 1:
			raise AssertionError(f"{len(found)} applicable rules named {name}")
		return found[0]

	def test_the_applicable_rules_and_why_the_others_are_not(self):
		report = self.report()
		self.assertEqual(report["format"], "aion-m5b3-drops")
		self.assertTrue(report["registerDrop"])
		self.assertEqual(report["ai"]["behaviours"][0]["classChain"], ["FixtureAggressiveAI", "NpcAI", "AITemplate"])
		self.assertEqual([r["ruleIndex"] for r in report["rules"]], [0, 1, 2, 3, 7, 8, 9, 11, 16])
		self.assertEqual([r["ruleIndex"] for r in report["rulesWithoutCandidates"]], [4], "the only candidate of 'Race filter' is Asmodian")
		blocked = {k: v for k, v in report["ruleStatistics"]["blockedBy"].items() if v}
		self.assertEqual(blocked, {"restrictionRace": 1, "maps": 1, "worlds": 1, "ratings": 1, "tribes": 1, "npcs": 3, "excludedNpcs": 1},
		                 "'Zoned' and 'Zoned ring' fail on their gd_npcs, which decide without the zone; 'Twins' names another npc")
		by_name = self.rule(report, "By name")
		self.assertTrue(by_name["npcSpecific"] and by_name["npcsFromNames"], "processRules turned the matched name into gd_npcs")
		no_match = self.rule(report, "No name match")
		self.assertFalse(no_match["npcSpecific"], "no template matched: the rule keeps gd_npcs null and applies to every npc (processRules)")
		equals = self.rule(report, "Equals name")  # EQUALS is equalsIgnoreCase: "FIXTURE Beast" names "fixture beast"
		self.assertTrue(equals["npcSpecific"] and equals["npcsFromNames"])
		self.assertEqual(self.drops.global_rules[16].lists["gd_npcs"], [900001])
		self.assertEqual((report["rulesZoneUndecided"], report["ruleStatistics"]["zoneUndecided"]), ([], 0))

	def test_chances_candidates_and_counts(self):
		report = self.report()
		kinah = self.rule(report, "Kinah")
		# dynamic: 50 * (SEASONED 1.05f * ELITE 1.3f = 1.3649999f) = 68.249992f
		factor = f32(f32(1.05) * f32(1.3))
		self.assertEqual((kinah["rankModifier"], kinah["ratingModifier"], kinah["effectiveChance"]), (f32(1.05), f32(1.3), f32(50 * factor)))
		self.assertEqual(kinah["fireProbability"], float(chance_probability(f32(50 * factor))))
		# count *= level 3 * Math.pow(1.3649999f, 6) = 3 * 6.4683826: 5 -> 97.03, 6 -> 116.43, 7 -> 135.84, truncated
		self.assertEqual([(v["count"], v["probability"]) for v in kinah["candidates"][0]["countValues"]], [(97, 1 / 3), (116, 1 / 3), (135, 1 / 3)])
		self.assertEqual(report["kinah"][0]["countRange"], [97, 135])
		window = self.rule(report, "Window")
		self.assertEqual([c["itemId"] for c in window["candidates"]], [504, 500], "503 is level 9: 3 - 9 = -6 < min_diff -5")
		self.assertTrue(window["certain"], "chance 100 * boost 1.0 = 100f")
		self.assertEqual((window["entriesIfFired"], window["allCandidatesDrop"]), (1, False))
		picks = self.rule(report, "Picks")
		self.assertEqual((picks["entriesIfFired"], picks["allCandidatesDrop"]), (2, False))
		self.assertEqual([round(c["pickProbability"], 6) for c in picks["candidates"]], [round(7 / 12, 6), round(7 / 12, 6), round(5 / 6, 6)])
		self.assertEqual([(c["optionalSocket"], c["lootEffectId"]) for c in picks["candidates"]], [(-1, 0), (0, 0), (0, 1003)],
		                 "option_slot_bonus makes the socket -1; 168000213 is a godstone of DropItem.getLootEffectId")
		junk = self.rule(report, "JUNK_SPAKY")
		self.assertEqual((junk["candidates"][0]["countRange"], junk["candidates"][0]["countValues"]), ([2, 4], None), "uniform, not kinah")
		self.assertEqual(report["lootEnable"]["lootEffectIds"], [0, 1003])

	def test_custom_and_quest_drops(self):
		report = self.report()
		groups = report["customDrop"]
		self.assertEqual([g["applies"] for g in groups], [True, False], "the first <npc_drop> only; the ASMODIANS group is not the killer's race")
		first = groups[0]
		self.assertEqual((first["minEntries"], first["maxEntries"]), (1, 2))
		self.assertEqual(first["entryDistribution"], [{"count": 1, "probability": 0.25}, {"count": 2, "probability": 0.75}])
		self.assertEqual([(d["itemId"], d["finalChance"], d["pickProbability"], d["minAmount"], d["maxAmount"]) for d in first["drops"]],
		                 [(800, 100.0, 1.0, 1, 1), (801, 50.0, 0.75, 2, 3)])
		quests = {(q["questId"], q["handlerSide"]): q for q in report["questDrops"]}
		self.assertEqual(set(quests), {(9000, False), (9002, False), (9001, True)})
		self.assertEqual(quests[(9000, False)]["conditions"], ["quest 9000 is in state START", "the inventory holds fewer than 3 of item 700"])
		# P(Rnd.chance() < 30): ceil(30 * 2^24 / 100) = 5033165; k = 5033164 gives 29.999995f (a tie, rounded to even), below 30
		self.assertEqual(quests[(9000, False)]["chanceProbability"], 5033165 / LATTICE)
		self.assertIn("MENTE", quests[(9002, False)]["soloNever"])
		handler = quests[(9001, True)]
		self.assertEqual((handler["itemId"], handler["neededAmount"], handler["chance"]), (600, 3, 50))
		self.assertIn("ALLIANCE", handler["soloNever"])
		entries = report["entries"]
		# the custom group (1..2) plus the certain 'Window' (1) at least; everything: 2 + 1 + 1 + 2 + 1 + 1 + 1 + 1 + 1 + 1
		self.assertEqual((entries["min"], entries["max"], entries["deterministic"]), (2, 12, False))
		self.assertIsNone(self.rule(report, "Kinah")["indexes"], "the custom group's entry count is random, so no index is exact")
		self.assertTrue(any("quest drops" in reason for reason in report["notModelled"]))

	def test_level_reduction_and_race(self):
		# npc level 3 - player level 9 = -6: DropRewardEnum MINUS_6, 80 %, only for level_based_chance_reduction rules
		report = self.report(player_level=9)
		self.assertEqual((report["modifiers"]["dropRewardPercent"], report["modifiers"]["reductionDropRate"]), (80, f32(0.8)))
		self.assertEqual(self.rule(report, "Reduced")["effectiveChance"], f32(50 * f32(0.8)))
		self.assertEqual(self.rule(report, "Window")["effectiveChance"], 100.0, "not a level based rule")
		asmodian = self.report(race="ASMODIANS")
		self.assertIn(4, [r["ruleIndex"] for r in asmodian["rules"]], "the Asmodian ring is a candidate for an Asmodian killer")
		self.assertIn(5, [r["ruleIndex"] for r in asmodian["rules"]], "restriction_race ASMODIANS")
		self.assertEqual([g["applies"] for g in asmodian["customDrop"]], [True, True])

	def test_the_default_exclusion_and_poeta(self):
		# level 1 outside Poeta and Ishalgen: only rules with gd_npcs are looked at (Named, By name, Zoned); By name and Zoned do not name it
		outside = self.report(900003, 1)
		self.assertEqual(outside["globalDrops"]["defaultExclusion"], "level 1 < 2 outside Poeta and Ishalgen")
		self.assertEqual([r["ruleName"] for r in outside["rules"]], ["Named"])
		self.assertEqual(outside["ruleStatistics"]["skippedNotAllowedDefault"], 13)
		# on the map with Poeta's id the default rules apply; at rate 10000 every one is certain, so the entry count and indexes are exact
		poeta = self.report(900003, 210010000, drop_rate="10000")
		self.assertEqual([r["ruleName"] for r in poeta["rules"]],
		                 ["Kinah", "JUNK_SPAKY", "Picks", "Window", "Named", "No name match", "Reduced", "Normal only", "Excluded"])
		self.assertTrue(all(r["certain"] for r in poeta["rules"]))
		self.assertEqual((poeta["entries"]["min"], poeta["entries"]["max"], poeta["entries"]["pNoDrop"]), (10, 10, 0.0))
		self.assertEqual([r["indexes"] for r in poeta["rules"]], [[1], [2], [3, 4], [5], [6], [7], [8], [9], [10]])
		self.assertIn("SM_LOOT_ITEMLIST lists exactly 10 entries with the indexes 1..10 (in any order: HashSet, plan D10)", poeta["gateAssertions"])
		self.assertEqual(poeta["lootEnable"]["pLootEffect"], float(selection_inclusion([100.0, 100.0, 200.0], 2)[2]), "'Picks' is certain")
		self.assertFalse(any(line.startswith("which rule each index") for line in poeta["gateAssertions"]),
		                 "the rules come from one file: the rule of each index does not depend on the order Java lists the files")
		# NOVICE 0.9f * NORMAL 1f, ^6 = 0.531441 (not a double: both neighbours give the same counts): 5 -> 2.66, 6 -> 3.19, 7 -> 3.72
		self.assertEqual([(v["count"], round(v["probability"], 6)) for v in poeta["kinah"][0]["countValues"]], [(2, round(1 / 3, 6)),
		                                                                                                          (3, round(2 / 3, 6))])
		off = self.report(900003, 210010000, drop_rate="0")
		self.assertEqual((off["entries"]["max"], off["lootEnable"]["lootEffectIds"]), (0, [0]), "rate 0: nothing can drop")
		self.assertIn("the drop set is empty: no entry (RespawnService.scheduleDecayTask keeps IMMEDIATE_DECAY)", off["gateAssertions"])

	def test_npcs_without_global_drops(self):
		citizen = self.report(900002)
		self.assertEqual((citizen["globalDrops"]["globalNpcExclusion"], citizen["globalDrops"]["evaluated"]), ("npc_types", False))
		self.assertEqual(citizen["entries"]["max"], 0)
		quest_object = self.report(900004)
		self.assertTrue(quest_object["globalDrops"]["questUseItemAi"])
		self.assertFalse(quest_object["globalDrops"]["evaluated"])
		chest = self.report(900007)
		self.assertTrue(chest["modifiers"]["isChest"], "group_drop TREASUREBOX starts with treasure")
		self.assertEqual((chest["globalDrops"]["defaultExclusion"], chest["rules"]), ("a chest", []))
		no_drop_map = self.report(900001, 2)
		self.assertTrue(no_drop_map["globalDrops"]["worldDropTypeNone"], "map 2 has no drop_type: WorldDropType.NONE")
		self.assertEqual((no_drop_map["rules"], no_drop_map["entries"]["min"], no_drop_map["entries"]["max"]), ([], 1, 2), "the custom drop remains")

	def test_ais_that_register_nothing(self):
		keeper = self.report(900006)
		self.assertFalse(keeper["registerDrop"], "noloot: case REWARD_LOOT -> false")
		self.assertEqual(keeper["ai"]["behaviours"][0]["classChain"], ["FixtureNoLootAI", "FixtureAggressiveAI", "NpcAI", "AITemplate"])
		self.assertFalse(keeper["allowDecay"])
		mute = self.report(900013)
		self.assertEqual((mute["ai"]["aiNames"], mute["registerDrop"]), ([None], False), "a spot ai of SpawnTemplate.NO_AI is DummyAI")

	def test_each_ai_question_decides_its_own_step(self):
		# NpcController.onDie: the corpse stays on ALLOW_DECAY (NpcController.java:148, 160), doReward runs on REWARD_AP_XP_DP_LOOT (:150),
		# and doReward registers the drop on REWARD_LOOT (:244): a drop needs both, the corpse neither
		tamed = self.report(900018)  # noreward: case REWARD_AP_XP_DP_LOOT -> false, else NpcAI's true
		self.assertEqual((tamed["registerDrop"], tamed["allowDecay"]), (False, True))
		husk = self.report(900019)  # nolootdecay: case REWARD_LOOT -> false, else NpcAI's true
		self.assertEqual((husk["registerDrop"], husk["allowDecay"]), (False, True))
		self.assertEqual((husk["entries"]["max"], husk["lootEnable"]), (0, None))

	def test_an_ai_that_registers_the_drop_itself(self):
		coffer = self.report(900020)  # like ChestAI: registerDrop from handleUseItemFinish; REWARD_LOOT false, so a kill registers nothing
		self.assertEqual((coffer["registerDrop"], coffer["registerDropByAi"], coffer["dropTrigger"]),
		                 (False, ["FixtureCofferAI"], "the AI's own registerDrop call"))
		self.assertEqual([r["ruleName"] for r in coffer["rules"]][:3], ["JUNK_SPAKY", "Picks", "Window"], "the drop it registers is evaluated")
		self.assertEqual(coffer["lootEnable"]["to"], "the user")
		self.assertTrue(coffer["gateAssertions"][0].startswith("registerDrop runs when the AI calls it (FixtureCofferAI; "), coffer["gateAssertions"][0])
		self.assertTrue(any(line.startswith("FixtureCofferAI call(s) registerDrop itself") for line in coffer["notModelled"]))
		self.assertEqual(self.report()["registerDropByAi"], [], "aggressive: the kill registers the drop")

	def test_the_siege_teleporter_rewrite(self):
		# NpcTemplate.afterUnmarshal: level > 1, ai not noaction, abyss_type TELEPORTER -> ai siege_teleporter (whose fixture class answers false)
		teleporter = self.report(900017)
		self.assertEqual((teleporter["ai"]["aiNames"], teleporter["npc"]["templateAi"], teleporter["npc"]["templateAiInXml"]),
		                 (["siege_teleporter"], "siege_teleporter", "aggressive"))
		self.assertEqual((teleporter["registerDrop"], teleporter["allowDecay"]), (False, False))
		self.assertEqual((self.drops.npcs[900022].ai, self.drops.npcs[900023].ai), ("aggressive", "noaction"), "level 1; noaction")

	def test_default_exclusion_boundaries(self):
		# isAllowedDefaultGlobalDropNpc: `npc.getLevel() < 2` outside Poeta and Ishalgen, and abyss types other than NONE and DEFENDER
		yearling = self.report(900014)
		self.assertIsNone(yearling["globalDrops"]["defaultExclusion"], "level 2 on a map that is not Poeta")
		self.assertIn("Window", [r["ruleName"] for r in yearling["rules"]])
		self.assertIsNone(self.report(900015)["globalDrops"]["defaultExclusion"], "AbyssNpcType DEFENDER keeps the default rules")
		guard = self.report(900016)
		self.assertEqual((guard["globalDrops"]["defaultExclusion"], guard["rules"]), ("abyss_type GUARD", []))

	def test_a_custom_group_without_rolls(self):
		# DropGroup.tryAddDropItems: `for (int i = 0; i < maxItems && ...)` does not run for max_items -1
		report = self.report(900014)
		group = report["customDrop"][0]
		self.assertEqual((group["maxItems"], group["minEntries"], group["maxEntries"], group["entryDistribution"]),
		                 (-1, 0, 0, [{"count": 0, "probability": 1.0}]))
		self.assertEqual(group["drops"][0]["pickProbability"], 0.0)
		# its second group takes the godstone 168000213 (loot effect 1003) with P(roll < 50) = 1/2, independently of 'Picks' (the same item,
		# weight 200, in a draw of two of three, if the rule fires at 10 %)
		picks = chance_probability(10.0) * selection_inclusion([100.0, 100.0, 200.0], 2)[2]
		self.assertEqual(report["lootEnable"]["pLootEffect"], float(1 - (1 - picks) * Fraction(1, 2)))

	def test_zone_rules_that_add_no_entry(self):
		# addGlobalDrops rolls before collectAllowedDrops checks the restrictions: a zone rule that never fires, or has no candidate, adds no entry
		# whatever isInsideZone answers
		off = self.report(900005, drop_rate="0")
		self.assertEqual([(r["ruleName"], r["addsNoEntry"]) for r in off["rulesZoneUndecided"]], [("Zoned", "it never fires")])
		self.assertEqual((off["ruleStatistics"]["zoneUndecided"], off["entries"]["max"]), (1, 0))
		sentry = self.report(900021)  # 'Zoned ring' has only the Asmodian ring
		self.assertEqual([(r["ruleName"], r["addsNoEntry"]) for r in sentry["rulesZoneUndecided"]], [("Zoned ring", "no candidate")])
		with self.assertRaises(OracleError) as caught:
			self.report(900021, race="ASMODIANS")
		self.assertIn("zones", str(caught.exception))

	def test_a_drop_rate_beyond_the_float_range(self):
		for rate in ("1e38", "3.4e38"):  # rate * 100 is Infinity in float
			with self.subTest(rate=rate), self.assertRaises(OracleError) as caught:
				self.report(drop_rate=rate)
			self.assertIn("Infinity", str(caught.exception))

	def test_repose_energy_assumption(self):
		# PlayerCommonData.updateMaxRepose clears the repose energy below level 10; from level 10 on it grows offline, so 0 is an assumption
		young = self.report(player_level=9)
		self.assertFalse(young["killer"]["reposeEnergyAssumed"])
		self.assertTrue(any(a.startswith("Energy of Repose 0 at level 9 (below 10 ") for a in young["config"]["assumptions"]))
		ten = self.report(player_level=10)
		self.assertTrue(ten["killer"]["reposeEnergyAssumed"])
		self.assertTrue(any(a.startswith("Energy of Repose 0 ASSUMED at level 10 ") for a in ten["config"]["assumptions"]))

	def test_loot_effect_probability_and_wording(self):
		report = self.report()
		# only 'Picks' has an item with a loot effect: 168000213 (weight 200) is among its two picks with inclusion[2], if the rule fires (10 %)
		expected = float(chance_probability(10.0) * selection_inclusion([100.0, 100.0, 200.0], 2)[2])
		self.assertEqual(report["lootEnable"]["pLootEffect"], expected)
		lines = report["gateAssertions"]
		self.assertTrue(lines[0].startswith("SM_LOOT_STATUS(corpse, LOOT_ENABLE) to the killer exactly once within 240 s of the kill"))
		self.assertIn(f"P(not 0) = {expected} without a quest drop", [line for line in lines if line.startswith("its lootEffectId")][0])
		self.assertIn(": its entries hold 2 entries drawn without replacement from the 3 candidate positions [501, 504, 168000213]; count 1",
		              [line for line in lines if "'Picks'" in line][0])
		twins = [line for line in self.report(900021)["gateAssertions"] if "'Twins'" in line][0]
		self.assertIn("2 entries drawn without replacement from the 3 candidate positions [500, 500, 504] (an id listed twice can drop twice)", twins)

	def test_refusals(self):
		cases = {
			900005: "zones",                  # 'Zoned' passes every predicate but the zone
			900008: "no rank",                # the dynamic Kinah rule needs getRankModifier
			900009: "ask(AIQuestion)",        # an if statement
			900010: "handleDropRegistered",
			900011: "group_drop",
			900012: "answer the loot questions differently",
		}
		for npc_id, phrase in cases.items():
			with self.subTest(npc=npc_id):
				with self.assertRaises(OracleError) as caught:
					self.report(npc_id)
				self.assertIn(phrase, str(caught.exception))
		with self.assertRaises(OracleError):
			self.report(900001, 3)  # @InstanceID(3)
		with self.assertRaises(OracleError):
			self.report(900001, None)  # three maps spawn it
		with self.assertRaises(OracleError):
			self.report(900001, 1, player_level=11)  # the experience table has 10 levels
		with self.assertRaises(OracleError):
			self.report(900001, 1, drop_rate="1.0, 2.0")  # one value, the membership's
		with self.assertRaises(OracleError):
			self.report(900002, 210010000)  # no spot there

	def test_data_java_refuses_at_startup(self):
		broken = [
			("global_rules", RULES, '<gd_item id="502"/>', '<gd_item id="424242"/>'),  # GlobalDropItem.afterUnmarshal: no item template
			("global_rules", RULES, 'min_count="2" max_count="4"', 'min_count="4" max_count="2"'),
			("global_rules", RULES, 'rule_name="Tribe"', 'rule_name="Tribe" chance_typo="1"'),
			("global_rules", RULES, '<gd_tribe tribe="AGGRESSIVEMONSTER"/>', '<gd_tribe tribe="NO_SUCH_TRIBE"/>'),  # JAXB: null, then an NPE
			("npc_templates", NPCS, ' name="fixture mute"', ""),  # GlobalDropData.processRules: a gd_npc_names rule needs every template's name
			# Drop.afterUnmarshal runs on every <npc_drop> before CustomDrop.afterUnmarshal drops the repeated one
			("custom_drop", CUSTOM, '<drop item_id="999999"/>', '<drop item_id="999999" chance="0"/>'),
		]
		for holder, original, old, new in broken:
			self.assertIn(old, original)
			text = original.replace(old, new, 1)
			with self.subTest(holder=holder, new=new), fixture_tree() as tree:
				tree.write(f"{holder}.xml", f'<?xml version="1.0" encoding="UTF-8"?>\n<{holder}>{text}</{holder}>\n')
				with self.assertRaises(OracleError):
					DropData(StaticData(tree.root), JAVA_SRC, tree.root / "handlers")
		with fixture_tree(QUEST_HANDLER.replace("900001, 600", "NPC_ID, 600")) as tree:
			with self.assertRaises(OracleError):  # a handler side drop whose npc the oracle cannot read
				DropData(StaticData(tree.root), JAVA_SRC, tree.root / "handlers")

	def test_survey_and_cli(self):
		survey = map_survey(self.drops, 1)
		self.assertEqual(survey["npcIds"], 21)
		self.assertEqual(sorted(r["npcId"] for r in survey["refused"]), [900005, 900008, 900009, 900010, 900011, 900012])
		self.assertEqual(survey["withKinah"], 4, "the LYCAN npcs allowed default drops: 900001, 900014, 900015 and 900021; 900003 is level 1 "
		                                         "outside Poeta, 900016 a GUARD")
		self.assertEqual(survey["questDropItems"], 2)
		out = io.StringIO()
		with contextlib.redirect_stdout(out):
			code = oracle.main(["m5b3-drops", "--static-data", str(self.tree.root), "--java-src", str(JAVA_SRC), "--java-handlers",
			                    str(self.tree.root / "handlers"), "--npc", "900003", "--map", "210010000", "--drop-rate", "10000"])
		self.assertEqual(code, 0)
		self.assertEqual(json.loads(out.getvalue())["entries"]["min"], 10)
		with contextlib.redirect_stderr(io.StringIO()), contextlib.redirect_stdout(io.StringIO()):
			self.assertEqual(oracle.main(["m5b3-drops", "--static-data", str(self.tree.root), "--java-src", str(JAVA_SRC), "--java-handlers",
			                              str(self.tree.root / "handlers"), "--npc", "900005", "--map", "1"]), 2, "OracleError: the zone decides")
			self.assertEqual(oracle.main(["m5b3-drops", "--survey"]), 2, "--survey needs --map")


@lru_cache(maxsize=1)
def real_drop_data() -> DropData:
	return DropData(StaticData(runner.DEFAULT_STATIC_DATA), JAVA_SRC, JAVA_HANDLERS)


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5b3RealDataTest(unittest.TestCase):
	"""The M5b gate's monster 210663, the kinah monster 210133 and the Poeta survey (m5b3-plan.md §2.4, D3, Y2, Y4)."""

	@classmethod
	def setUpClass(cls):
		cls.drops = real_drop_data()
		cls.sparkie = drops_report(cls.drops, 210663)
		cls.kerub = drops_report(cls.drops, 210133)

	def test_210663_at_the_default_rate(self):
		report = self.sparkie
		self.assertEqual(report["map"]["id"], 210010000, "the one map with a regular spawn of it")
		self.assertEqual((report["npc"]["race"], report["npc"]["groupDrop"], report["npc"]["spots"]), ("BEAST", "SPAKY", 38))
		self.assertTrue(report["registerDrop"])
		self.assertEqual([r["ruleName"] for r in report["rules"]],
		                 ["Buff Food", "Potions (Common)", "Potions (Rare)", "Power Shards", "Illusion Godstones (Legend)", "Illusion Godstones (Unique)",
		                  "Manastones (Common)", "Weapons (Common)", "Armor (Common)", "JUNK_SPAKY_MATERIAL"])
		self.assertEqual(report["kinah"], [], "BEAST is not in the Kinah rule's gd_races")
		self.assertEqual([r["effectiveChance"] for r in report["rules"]], [3.5, 3.5, 2.5, 6.0, f32(0.02), f32(0.01), 15.0, f32(1.3), 2.75, 40.0],
		                 "DISCIPLINED 1f * NORMAL 1f, rate 1.0")
		self.assertEqual(round(1 - report["entries"]["pNoDrop"], 3), 0.582, "m5b3-plan.md §2.4: some drop on 58.2 % of kills")
		shards = [r for r in report["rules"] if r["ruleName"] == "Power Shards"][0]
		self.assertEqual([(c["itemId"], c["countRange"]) for c in shards["candidates"]], [(169000003, [2, 15])])
		junk = report["rules"][-1]
		self.assertEqual([c["itemId"] for c in junk["candidates"]], [182004793])
		self.assertEqual(report["questDrops"], [])
		self.assertEqual(report["customDrop"], [])

	def test_210133_and_its_kinah(self):
		report = self.kerub
		self.assertEqual([r["ruleName"] for r in report["rules"]][:2], ["Kinah", "Buff Food"])
		self.assertEqual(len(report["rules"]), 10)
		self.assertIn("Armor (Common)", [r["ruleName"] for r in report["rulesWithoutCandidates"]], "the lightest armour is level 4: diff -3")
		self.assertEqual(report["globalDrops"]["defaultExclusion"], None, "level 1, but on Poeta")
		kinah = report["kinah"][0]
		self.assertEqual((kinah["fireProbability"], kinah["countRange"]), (0.5, [5, 25]), "5..25 * level 1 * 1f^6")
		self.assertEqual(len(kinah["countValues"]), 21)
		self.assertEqual(round(1 - report["entries"]["pNoDrop"], 3), 0.785, "m5b3-plan.md §2.4: 78.5 %")

	def test_the_gate_rates(self):
		forced = drops_report(self.drops, 210663, drop_rate="10000")
		self.assertEqual((forced["entries"]["min"], forced["entries"]["max"]), (10, 10))
		self.assertEqual([r["indexes"] for r in forced["rules"]], [[i] for i in range(1, 11)])
		self.assertEqual(forced["entries"]["minCertainEffectiveChance"], 100.0, "0.01f * 10000 is exactly 100f: certain, with no margin")
		self.assertTrue(any(line.startswith("caution") for line in forced["gateAssertions"]))
		wide = drops_report(self.drops, 210663, drop_rate="1000000")
		self.assertEqual(wide["entries"]["minCertainEffectiveChance"], 10000.0)
		self.assertFalse(any(line.startswith("caution") for line in wide["gateAssertions"]))
		# both Illusion Godstone rules are certain and draw one of 17, four of which carry loot effect 1003: the sum of their pick
		# probabilities is P(effect) per rule (one pick), independently of the walk selection_avoidance takes
		godstones = [r for r in wide["rules"] if r["ruleName"].startswith("Illusion Godstones")]
		self.assertEqual([len([c for c in r["candidates"] if c["lootEffectId"] == 1003]) for r in godstones], [4, 4])
		none = 1.0
		for rule in godstones:
			none *= 1 - sum(c["pickProbability"] for c in rule["candidates"] if c["lootEffectId"])
		self.assertAlmostEqual(wide["lootEnable"]["pLootEffect"], 1 - none, places=12)
		self.assertTrue(0.3 < wide["lootEnable"]["pLootEffect"] < 0.5, "about 1 - (13/17)^2 with equal weights")
		# the 10 rules come from three files, so which rule an index belongs to depends on the order Java lists them
		self.assertEqual(len({r["file"] for r in wide["rules"]}), 3)
		self.assertTrue(any(line.startswith("which rule each index belongs to assumes Java lists the global_rules files") for line in wide["gateAssertions"]))
		off = drops_report(self.drops, 210133, drop_rate="0")
		self.assertEqual((off["entries"]["max"], off["registerDrop"]), (0, True), "m5b3-plan.md D4: registerDrop still runs, the set is empty")

	def test_poeta_survey(self):
		survey = map_survey(self.drops, 210010000)
		self.assertEqual((survey["npcIds"], survey["withApplicableRule"], survey["withKinah"], survey["withCustomDrop"], survey["questDropItems"],
		                  survey["distinctDroppableItems"]), (147, 77, 42, 0, 20, 999), "m5b3-plan.md §2.4, the Poeta column")
		self.assertEqual(survey["refused"], [])


if __name__ == "__main__":
	unittest.main()
