"""M5a oracles (m5a/): spawn rules on small static_data trees, the game clock and temporary spawn times, PositionUtil range semantics, the
base stat formulas, and the real data for the two scenario characters. Expected values are derived by hand from the Java sources named in
m5a/spawns.py and m5a/creation.py. The real data cases are skipped without the Java tree.
"""

import unittest
import xml.etree.ElementTree as ET

from m5a.creation import (ItemInfo, JavaEnums, PassiveRules, base_stat_dependent_additional_value, creation_report, max_hp, max_mp,
                          passive_stat_functions, stats_info_base_max_hp, stats_info_base_max_mp, stats_info_current_max,
                          stats_info_main_hand_p_attack)
from m5a.data import StaticData
from m5a.javafloat import f32, in_range
from m5a.spawns import GameClock, TemporarySpawn, border_target, evaluate, load_groups, load_npc_templates, spots_report
from staticdata_oracle import OracleError
from staticdata_oracle import run as runner

from .support import Tree, xml

JAVA_SRC = runner.TOOL_DIR.parents[2] / "game-server" / "src"
HAVE_JAVA_TREE = (JAVA_SRC / "com" / "aionemu" / "gameserver").is_dir() and runner.DEFAULT_STATIC_DATA.is_dir()

NPCS = (
	'<npc_template npc_id="200001" level="3" type="NONE"/>'
	'<npc_template npc_id="200002" level="5"/>'
	'<npc_template npc_id="200003" level="7" type="FLAG"/>'
	'<npc_template npc_id="200004" level="9"/>'
	'<npc_template npc_id="200005" level="1"/>'
)

SPAWNS_A = """
<spawn_map map_id="1">
	<spawn npc_id="200001"><spot x="10" y="10" z="0" h="5"/><spot x="20" y="10" z="0" h="6"/></spawn>
	<spawn npc_id="200002" pool="1"><spot x="30" y="10" z="0" h="0"/><spot x="40" y="10" z="0" h="0"/></spawn>
	<spawn npc_id="200004" pool="2"><spot x="50" y="10" z="0"/><spot x="60" y="10" z="0"/></spawn>
	<spawn npc_id="200005" handler="STATIC"><spot x="70" y="10" z="0"/></spawn>
	<spawn npc_id="200003"><spot x="900" y="900" z="0"/></spawn>
	<spawn npc_id="299999"><spot x="11" y="11" z="0"/></spawn>
	<spawn npc_id="400100"><spot x="12" y="12" z="0"/></spawn>
	<spawn npc_id="200001" difficult_id="1"><spot x="13" y="13" z="0"/></spawn>
</spawn_map>
<spawn_map map_id="2"><spawn npc_id="200001"><spot x="1" y="1" z="1"/></spawn></spawn_map>
"""

SPAWNS_B = """
<spawn_map map_id="1">
	<spawn npc_id="200002" custom="true"><spot x="35" y="15" z="0"/></spawn>
	<spawn npc_id="200002"><spot x="36" y="16" z="0"/></spawn>
	<spawn npc_id="200004"><temporary_spawn spawn_time="21.*.*" despawn_time="9.*.*"/><spot x="80" y="10" z="0" walker_id="W1"/></spawn>
	<spawn npc_id="200001"><spot x="90" y="10" z="0"><temporary_spawn spawn_time="*.*.2" despawn_time="*.*.3"/></spot></spawn>
</spawn_map>
"""


class M5aSpawnRulesTest(unittest.TestCase):
	def setUp(self):
		self.tree = Tree()
		self.tree.write("world_maps.xml", xml("world_maps", '<map id="1" world_size="1024"/>'))
		self.tree.write("npcs/npcs.xml", xml("npc_templates", NPCS))
		self.tree.write("spawns/A.xml", xml("spawns", SPAWNS_A))
		self.tree.write("spawns/B.xml", xml("spawns", SPAWNS_B))
		self.tree.static_data('file="world_maps.xml"', 'file="npcs" singleRootTag="true"', 'file="spawns" singleRootTag="true"')
		self.data = StaticData(self.tree.root)

	def tearDown(self):
		self.tree.close()

	def rows(self, clock=GameClock()):
		return evaluate(load_groups(self.data, 1), load_npc_templates(self.data), clock)

	def by_position(self, rows):
		return {(r["npcId"], r["x"], r["y"]): r for r in rows}

	def test_groups_of_the_map_with_custom_replacement(self):
		rows = self.by_position(self.rows(GameClock(hour=22, day=5, month=4)))
		self.assertNotIn((200002, 30.0, 10.0), rows, "the custom spawn of B.xml removed the groups of A.xml")
		self.assertNotIn((200002, 36.0, 16.0), rows, "a later spawn of the custom npc id in the same spawn_map is skipped")
		self.assertTrue(rows[(200002, 35.0, 15.0)]["deterministic"])
		self.assertNotIn((200001, 1.0, 1.0), rows, "another map")

	def test_spawned_and_deterministic_flags(self):
		rows = self.by_position(self.rows(GameClock(hour=22, day=2, month=4)))
		first = rows[(200001, 10.0, 10.0)]
		self.assertEqual((first["spawned"], first["deterministic"], first["level"], first["h"]), (True, True, 3, 5))
		pool = rows[(200004, 50.0, 10.0)]
		self.assertEqual((pool["spawned"], pool["flags"]["pool"]), (True, False), "a pool not smaller than the spots spawns every spot")
		self.assertEqual(rows[(200005, 70.0, 10.0)]["spawned"], False, "handler groups spawn no Npc")
		self.assertEqual(rows[(200005, 70.0, 10.0)]["flags"]["handler"], "STATIC")
		self.assertEqual(rows[(299999, 11.0, 11.0)]["spawned"], False, "no npc template")
		self.assertTrue(rows[(400100, 12.0, 12.0)]["flags"]["gatherable"])
		self.assertEqual(rows[(400100, 12.0, 12.0)]["spawned"], True, "gatherables need no npc template")
		self.assertEqual(rows[(200001, 13.0, 13.0)]["spawned"], False, "difficulty 1 is not spawned in the open world")
		flag = rows[(200003, 900.0, 900.0)]
		self.assertTrue(flag["flags"]["flag"])
		walker = rows[(200004, 80.0, 10.0)]
		self.assertEqual((walker["spawned"], walker["deterministic"], walker["flags"]["walker"], walker["flags"]["temporary"]), (True, False, True, True))
		self.assertEqual(rows[(200001, 90.0, 10.0)]["spawned"], False, "spot temporary spawn *.*.2 to *.*.3: month 4 is outside")

	def test_temporary_spawns_follow_the_clock(self):
		rows = self.by_position(self.rows(GameClock(hour=10, day=1, month=2)))
		self.assertEqual(rows[(200004, 80.0, 10.0)]["spawned"], False, "21.*.* to 9.*.*: hour 10 is outside")
		self.assertEqual(rows[(200001, 90.0, 10.0)]["spawned"], True, "month 2 is inside 2..3")
		unknown = self.by_position(self.rows(GameClock()))
		self.assertIsNone(unknown[(200004, 80.0, 10.0)]["spawned"], "the hour is not known")
		self.assertIsNone(unknown[(200001, 90.0, 10.0)]["spawned"], "the month is not known")

	def test_pool_smaller_than_its_spots_is_nondeterministic(self):
		self.tree.write("spawns/C.xml", xml("spawns", '<spawn_map map_id="1"><spawn npc_id="200005" pool="1"><spot x="1" y="2" z="3"/>'
		                                              '<spot x="4" y="5" z="6"/></spawn></spawn_map>'))
		data = StaticData(self.tree.root)
		rows = evaluate(load_groups(data, 1), load_npc_templates(data), GameClock(hour=1))
		pooled = [r for r in rows if r["npcId"] == 200005 and r["flags"]["pool"]]
		self.assertEqual(len(pooled), 2)
		self.assertTrue(all(r["spawned"] is None and not r["deterministic"] for r in pooled))

	def test_report_uses_strict_float_range(self):
		report = spots_report(self.data, 1, (10.0, 0.0, 0.0), 10.0, GameClock(hour=22, day=2, month=4))
		positions = {(s["npcId"], s["x"], s["y"]) for s in report["spots"]}
		self.assertNotIn((200001, 10.0, 10.0), positions, "distance exactly 10 is not < 10")
		report = spots_report(self.data, 1, (10.0, 0.5, 0.0), 10.0, GameClock(hour=22, day=2, month=4))
		self.assertIn((200001, 10.0, 10.0), {(s["npcId"], s["x"], s["y"]) for s in report["spots"]})
		self.assertEqual([f["npcId"] for f in report["flagNpcs"]], [200003], "flag npcs of the whole map")

	def test_border_target_finds_appearing_and_disappearing_npcs(self):
		self.tree.write("spawns/D.xml", xml("spawns", '<spawn_map map_id="1"><spawn npc_id="200005"><spot x="10" y="300" z="0"/></spawn></spawn_map>'))
		data = StaticData(self.tree.root)
		result = border_target(data, 1, (10.0, 100.0, 0.0), GameClock(hour=22, day=2, month=4))
		self.assertEqual((result["distance"], result["direction"]), (150, 90))
		self.assertEqual([a["npcId"] for a in result["appear"]], [200005])
		# 200002 at (35, 15) is 88.6 m from the start; 200001 at (10, 10) is exactly 90 m away, which isInRange excludes
		self.assertEqual([d["npcId"] for d in result["disappear"]], [200002])
		with self.assertRaises(OracleError):
			border_target(data, 1, (1000.0, 1000.0, 0.0), GameClock(hour=22))


class M5aClockTest(unittest.TestCase):
	def test_game_time_parts(self):
		self.assertEqual(GameClock.from_minutes(0), GameClock(0, 1, 1))
		self.assertEqual(GameClock.from_minutes(31 * 1440), GameClock(0, 1, 2), "the first minute of February")
		self.assertEqual(GameClock.from_minutes(31 * 1440 + 2 * 1440 + 5 * 60 + 59), GameClock(5, 3, 2))
		self.assertEqual(GameClock.from_minutes(372 * 1440 + 90), GameClock(1, 1, 1), "a year has 12 * 31 days")
		with self.assertRaises(OracleError):
			GameClock.from_minutes(-1)

	def parse(self, attributes):
		return TemporarySpawn.parse(ET.fromstring(f"<temporary_spawn {attributes}/>"))

	def test_hour_ranges_and_expressions(self):
		night = self.parse('spawn_time="21.*.*" despawn_time="9.*.*"')
		self.assertEqual([night.is_in_spawn_time(GameClock(hour=h)) for h in (20, 21, 0, 8, 9)], [False, True, True, True, False])
		day = self.parse('spawn_time="9.*.*" despawn_time="21.*.*"')
		self.assertEqual([day.is_in_spawn_time(GameClock(hour=h)) for h in (8, 9, 20, 21)], [False, True, True, False])
		self.assertTrue(self.parse('spawn_time="5.*.*" despawn_time="5.*.*"').is_in_spawn_time(GameClock(hour=0)), "equal hours: always")
		self.assertTrue(self.parse('spawn_time="7.*.*"').is_in_spawn_time(GameClock(hour=7)), "no despawn: from the hour on")
		self.assertFalse(self.parse('spawn_time="7.*.*"').is_in_spawn_time(GameClock(hour=6)))
		every_second = self.parse('spawn_time="/2.*.*" despawn_time="/2.*.*"')
		self.assertTrue(every_second.is_in_spawn_time(GameClock(hour=3)), "checkWithDespawnExpression: 3 >= 2 and 2 == 2")
		self.assertFalse(self.parse('spawn_time="/2.*.*" despawn_time="/3.*.*"').is_in_spawn_time(GameClock(hour=5)))
		self.assertTrue(self.parse("").is_in_spawn_time(GameClock()), "<temporary_spawn/> without times")
		with self.assertRaises(OracleError):
			self.parse('spawn_time="7"')

	def test_weekdays_need_the_weekday(self):
		weekend = self.parse('weekdays="SATURDAY SUNDAY"')
		self.assertIsNone(weekend.is_in_spawn_time(GameClock(hour=1)))
		self.assertTrue(weekend.is_in_spawn_time(GameClock(hour=1, weekday="SUNDAY")))
		self.assertFalse(weekend.is_in_spawn_time(GameClock(hour=1, weekday="MONDAY")))


class M5aJavaFloatTest(unittest.TestCase):
	def test_range_is_strict_on_float_squares(self):
		self.assertFalse(in_range(0, 0, 0, 3, 4, 0, 5))
		self.assertTrue(in_range(0, 0, 0, 3, 4, 0, 5.0001))
		self.assertEqual(f32(0.1), 0.10000000149011612)


class M5aStatFormulaTest(unittest.TestCase):
	def test_base_hp_and_mp(self):
		# WARRIOR 400/400: 200 + 43 + 1.15 = 244.15; 140 + 70 + 0.005 = 210.005
		self.assertEqual((max_hp(400, 1), max_mp(400, 1)), (244, 210))
		# MAGE 260/600: 130 + 27.95 + 0.7475 = 158.6975; 210 + 105 + 0.0075 = 315.0075
		self.assertEqual((max_hp(260, 1), max_mp(600, 1)), (158, 315))
		# GLADIATOR 440 at level 10: 220 + 473 + 126.5 = 819.5; will 600 at level 10: 210 + 1050 + 0.75 = 1260.75
		self.assertEqual((max_hp(440, 10), max_mp(600, 10)), (819, 1260))

	def test_stats_info_base_adds_the_health_and_will_dependent_value(self):
		# PlayerStatFunctions.MaxHpFunction / MaxMpFunction add getHealthDependentAdditionalHp / getWillDependentAdditionalMp to the BASE,
		# so SM_STATS_INFO's [base hp] / [base mana] are not the stats template values (PlayerGameStats.java:340-346, :368-370).
		self.assertEqual(base_stat_dependent_additional_value(110, 400), 40, "WARRIOR health 110: (110-100)/100f * 400")
		self.assertEqual(base_stat_dependent_additional_value(90, 400), -40, "WARRIOR will 90: a negative addition")
		self.assertEqual(base_stat_dependent_additional_value(100, 999), 0, "a base stat of 100 adds nothing")
		self.assertEqual(base_stat_dependent_additional_value(90, 260), -26, "MAGE health 90")
		self.assertEqual(base_stat_dependent_additional_value(115, 600), 90, "MAGE will 115")
		# WARRIOR(power 110, health 110, ..., will 90, healthMultiplier 400, willMultiplier 400)
		self.assertEqual((stats_info_base_max_hp(110, 400, 1), stats_info_base_max_mp(90, 400, 1)), (284, 170))
		# MAGE(power 90, health 90, ..., will 115, healthMultiplier 260, willMultiplier 600)
		self.assertEqual((stats_info_base_max_hp(90, 260, 1), stats_info_base_max_mp(115, 600, 1)), (132, 405))


PASSIVE_SKILLS = (
	# 2001: a statboost with the three BufEffect functions, the ADD one with a per-level delta
	'<skill_template skill_id="2001" activation="PASSIVE" lvl="1"><effects><statboost e="1">'
	'<change stat="PHYSICAL_ATTACK" func="ADD" value="7" delta="2"/><change stat="PHYSICAL_DEFENSE" func="PERCENT" value="10"/>'
	'<change stat="PARRY" func="REPLACE" value="300"/><change func="ADD" value="1"/></statboost><notanelement e="2"/></effects></skill_template>'
	# 2002: an ACTIVE skill with the same effect registers nothing at enter world
	'<skill_template skill_id="2002" activation="ACTIVE" lvl="1"><effects><statboost e="1">'
	'<change stat="PHYSICAL_ATTACK" func="ADD" value="7"/></statboost></effects></skill_template>'
	# 2003: a ONE_HAND mastery: the attack stats become MAIN_HAND_POWER and OFF_HAND_POWER, anything else is dropped
	'<skill_template skill_id="2003" activation="PASSIVE" lvl="1"><effects><wpnmastery weapon="SWORD" e="1">'
	'<change stat="PHYSICAL_ATTACK" func="PERCENT" value="16"/><change stat="PHYSICAL_ACCURACY" func="ADD" value="50"/></wpnmastery></effects>'
	'</skill_template>'
	# 2004: a TWO_HAND mastery keeps its stat
	'<skill_template skill_id="2004" activation="PASSIVE" lvl="1"><effects><wpnmastery weapon="GREATSWORD" e="1">'
	'<change stat="PHYSICAL_ATTACK" func="PERCENT" value="20"/></wpnmastery></effects></skill_template>'
	# 2005: a chain armor mastery with a fixed bonus of 3 + 1 * level
	'<skill_template skill_id="2005" activation="PASSIVE" lvl="1"><effects><armormastery armor="CHAIN" value="3" delta="1" e="1">'
	'<change stat="PHYSICAL_DEFENSE" func="PERCENT" value="10"/></armormastery></effects></skill_template>'
	# 2006: a shield mastery
	'<skill_template skill_id="2006" activation="PASSIVE" lvl="1"><effects><shieldmastery e="1">'
	'<change stat="DAMAGE_REDUCE" func="PERCENT" value="4"/></shieldmastery></effects></skill_template>'
	# 2007-2010: what the model refuses - a conditioned change, a heal over time, a REPLACE of MAXHP, a HEALTH bonus
	'<skill_template skill_id="2007" activation="PASSIVE" lvl="1"><effects><statboost e="1">'
	'<change stat="PHYSICAL_ATTACK" func="ADD" value="7"><conditions><weapon weapon="SWORD"/></conditions></change></statboost></effects>'
	'</skill_template>'
	'<skill_template skill_id="2008" activation="PASSIVE" lvl="1"><effects><heal e="1" value="10" checktime="1000"/></effects></skill_template>'
	'<skill_template skill_id="2009" activation="PASSIVE" lvl="1"><effects><statboost e="1">'
	'<change stat="MAXHP" func="REPLACE" value="1000"/></statboost></effects></skill_template>'
	'<skill_template skill_id="2010" activation="PASSIVE" lvl="1"><effects><statboost e="1">'
	'<change stat="HEALTH" func="ADD" value="5"/></statboost></effects></skill_template>'
	# 2011: a MAXHP bonus and a MAXMP rate leave the base alone
	'<skill_template skill_id="2011" activation="PASSIVE" lvl="1"><effects><statboost e="1">'
	'<change stat="MAXHP" func="ADD" value="100"/><change stat="MAXMP" func="PERCENT" value="10"/></statboost></effects></skill_template>'
)


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5aPassiveModelTest(unittest.TestCase):
	"""passive_stat_functions on a small skill_data (the effect classes and the armor factors come from the Java sources); the expected values
	follow BufEffect.getModifiers and the startEffect of WeaponMasteryEffect, ArmorMasteryEffect and ShieldMasteryEffect."""

	@classmethod
	def setUpClass(cls):
		cls.tree = Tree()
		cls.tree.minimal({"skill_data": PASSIVE_SKILLS})
		cls.data = StaticData(cls.tree.root)
		cls.enums = JavaEnums(JAVA_SRC)
		cls.rules = PassiveRules(JAVA_SRC)

	@classmethod
	def tearDownClass(cls):
		cls.tree.close()

	def functions(self, skills, equipped):
		return [(f["skillId"], f["function"], f["stat"], f["value"], f["bonus"], f["applies"])
		        for f in passive_stat_functions(self.data, self.enums, self.rules, skills, equipped)]

	def test_a_statboost_registers_the_three_buf_functions_with_the_level_delta(self):
		self.assertEqual(self.functions({2001: 3, 2002: 1}, {}), [
			(2001, "StatAddFunction", "PHYSICAL_ATTACK", 13, True, True),  # 7 + 2 * 3
			(2001, "StatRateFunction", "PHYSICAL_DEFENSE", 10, True, True),
			(2001, "StatSetFunction", "PARRY", 300, False, True),
		], "the stat-less change and the unbound <notanelement> are dropped, the ACTIVE 2002 registers nothing")

	def test_a_one_hand_mastery_splits_the_attack_between_the_hands_that_hold_its_weapon(self):
		self.assertEqual(self.functions({2003: 1}, {"MAIN_HAND": "SWORD"}), [
			(2003, "StatWeaponMasteryFunction", "MAIN_HAND_POWER", 16, True, True),
			(2003, "StatWeaponMasteryFunction", "OFF_HAND_POWER", 16, True, False),
		], "PHYSICAL_ACCURACY is dropped; the off hand holds nothing")
		self.assertEqual([f[5] for f in self.functions({2003: 1}, {"MAIN_HAND": "MACE", "SUB_HAND": "SWORD"})], [False, True],
		                 "a sword in the off hand only")

	def test_a_two_hand_mastery_keeps_its_stat_and_needs_the_weapon_in_the_main_hand(self):
		self.assertEqual(self.functions({2004: 1}, {"MAIN_HAND": "GREATSWORD"}),
		                 [(2004, "StatWeaponMasteryFunction", "PHYSICAL_ATTACK", 20, True, True)])
		self.assertEqual(self.functions({2004: 1}, {"MAIN_HAND": "SWORD"}),
		                 [(2004, "StatWeaponMasteryFunction", "PHYSICAL_ATTACK", 20, True, False)])

	def test_an_armor_mastery_scales_with_the_slots_of_its_armor_type(self):
		chain = {"MAIN_HAND": "SWORD", "TORSO": "CH_TORSO", "PANTS": "CH_PANTS", "SHOULDER": "LT_SHOULDER"}
		[function] = passive_stat_functions(self.data, self.enums, self.rules, {2005: 2}, chain)
		# torso 30 + pants 25 of CHAIN (the leather shoulder does not count): 10 * 55 / 100 = 5 in int arithmetic; (3 + 1 * 2) * 55 / 100f
		self.assertEqual((function["value"], function["equipmentFactor"], function["fixedBonus"], function["applies"]), (5, 55, 5, True))
		self.assertEqual(function["bonusAdded"], f32(2.75))
		chain["SHOULDER"] = "CH_SHOULDER"
		[function] = passive_stat_functions(self.data, self.enums, self.rules, {2005: 2}, chain)
		self.assertEqual((function["value"], function["equipmentFactor"]), (7, 70), "a chain shoulder adds 15")
		[function] = passive_stat_functions(self.data, self.enums, self.rules, {2005: 2}, {"TORSO": "LT_TORSO"})
		self.assertEqual((function["value"], function["bonusAdded"], function["applies"]), (0, 0.0, False), "no chain armor")

	def test_a_shield_mastery_needs_a_shield(self):
		self.assertEqual(self.functions({2006: 1}, {"SUB_HAND": "SHIELD"}), [(2006, "StatShieldMasteryFunction", "DAMAGE_REDUCE", 4, True, True)])
		self.assertEqual(self.functions({2006: 1}, {"SUB_HAND": "SWORD"})[0][5], False)

	def test_what_the_model_does_not_know_is_refused(self):
		for skill_id, reason in ((2007, "<conditions>"), (2008, "does not model"), (2009, "base max HP/MP"), (2010, "base max HP/MP")):
			with self.subTest(skill=skill_id):
				with self.assertRaises(OracleError) as raised:
					passive_stat_functions(self.data, self.enums, self.rules, {skill_id: 1}, {})
				self.assertIn(reason, str(raised.exception))
		self.assertEqual([f[2] for f in self.functions({2011: 1}, {})], ["MAXHP", "MAXMP"], "bonus functions leave the base alone")


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5aMainHandAttackModelTest(unittest.TestCase):
	"""stats_info_main_hand_p_attack (m5b2-plan.md X1) on hand-built weapons and passives. Every expected value is derived by hand from
	PlayerGameStats.getMainHandPAttack(DISPLAY), PhysicalAttackFunction, StatAddFunction, StatWeaponMasteryFunction and Stat2 in float."""

	SWORD = ItemInfo("SWORD", 1, attack_type="PHYSICAL", min_damage=16, max_damage=20, has_weapon_stats=True)
	STATBOOST = {"skillId": 140, "function": "StatAddFunction", "stat": "PHYSICAL_ATTACK", "value": 7, "bonus": True, "applies": True}
	SWORD_MASTERY = {"skillId": 37, "function": "StatWeaponMasteryFunction", "stat": "MAIN_HAND_POWER", "value": 16, "bonus": True,
	                 "applies": True}
	MACE_MASTERY = {"skillId": 39, "function": "StatWeaponMasteryFunction", "stat": "MAIN_HAND_POWER", "value": 20, "bonus": True,
	                "applies": False}

	@classmethod
	def setUpClass(cls):
		cls.enums = JavaEnums(JAVA_SRC)

	def attack(self, weapon, passives, equipped=None, power=110):
		return stats_info_main_hand_p_attack(self.enums, power, weapon, equipped if equipped is not None else [weapon], passives)

	def test_the_training_sword_with_the_warrior_passives(self):
		# mean (16 + 20) / 2f = 18.0; base rate 110 * 0.01f = 1.1f; base 18 * 1.1f = 19.8 -> (int) 19; current 19.8 + 7 * 1 + 18 * 0.16f = 29.68
		# -> (int) 29
		self.assertEqual(self.attack(self.SWORD, [self.STATBOOST, self.SWORD_MASTERY, self.MACE_MASTERY]), {"base": 19, "current": 29})

	def test_each_passive_moves_the_current_value_and_never_the_base(self):
		self.assertEqual(self.attack(self.SWORD, []), {"base": 19, "current": 19}, "no passive applied: 19.8 -> 19 twice")
		self.assertEqual(self.attack(self.SWORD, [self.STATBOOST]), {"base": 19, "current": 26}, "19.8 + 7 = 26.8")
		self.assertEqual(self.attack(self.SWORD, [self.SWORD_MASTERY]), {"base": 19, "current": 22}, "19.8 + 2.88 = 22.68")
		self.assertEqual(self.attack(self.SWORD, [self.STATBOOST, self.STATBOOST, self.SWORD_MASTERY]), {"base": 19, "current": 36},
		                 "a statboost counted twice: 19.8 + 14 + 2.88 = 36.68")
		wrong_group = dict(self.MACE_MASTERY, applies=True)
		self.assertEqual(self.attack(self.SWORD, [self.STATBOOST, wrong_group]), {"base": 19, "current": 30},
		                 "the mace mastery applied to a sword: 19.8 + 7 + 3.6 = 30.4")

	def test_a_magical_main_hand_answers_the_empty_addition_stat(self):
		book = ItemInfo("SPELLBOOK", 1, attack_type="MAGICAL_FIRE", min_damage=10, max_damage=12, has_weapon_stats=True)
		self.assertEqual(self.attack(book, [self.STATBOOST]), {"base": 0, "current": 0})

	def test_the_current_maxima_add_the_bonus_modifiers_and_refuse_the_rest(self):
		tunic = ItemInfo("RB_TORSO", 1, modifiers=(("add", "EVASION", 34, False), ("add", "MAXMP", 26, True)))
		leggings = ItemInfo("RB_PANTS", 1, modifiers=(("add", "MAXMP", 21, True),))
		self.assertEqual(stats_info_current_max("MAXMP", 405, [tunic, leggings], []), {"base": 405, "bonus": 47.0, "current": 452})
		self.assertEqual(stats_info_current_max("MAXHP", 132, [tunic, leggings], []), {"base": 132, "bonus": 0.0, "current": 132})
		boost = {"skillId": 1, "function": "StatAddFunction", "stat": "MAXHP", "value": 50, "bonus": True, "applies": True}
		self.assertEqual(stats_info_current_max("MAXHP", 132, [], [boost])["current"], 182)
		for reason, items in (("<rate", [ItemInfo("X", 1, modifiers=(("rate", "MAXMP", 10, True),))]),
		                      ("bonus=False", [ItemInfo("X", 1, modifiers=(("add", "MAXMP", 10, False),))])):
			with self.subTest(reason=reason):
				with self.assertRaises(OracleError) as raised:
					stats_info_current_max("MAXMP", 405, items, [])
				self.assertIn(reason, str(raised.exception))

	def test_what_the_model_does_not_know_is_refused(self):
		armor = ItemInfo("CH_TORSO", 1, modifier_stats=frozenset({"PHYSICAL_ATTACK"}))
		power = {"skillId": 1, "function": "StatAddFunction", "stat": "POWER", "value": 5, "bonus": True, "applies": True}
		rate = dict(self.STATBOOST, function="StatRateFunction")
		for reason, weapon, passives, equipped in (
				("no main hand weapon", None, [], []),
				("<modifiers>", self.SWORD, [], [self.SWORD, armor]),
				("POWER", self.SWORD, [power], None),
				("StatRateFunction", self.SWORD, [rate], None),
				("more than one weapon mastery", self.SWORD, [self.SWORD_MASTERY, dict(self.MACE_MASTERY, applies=True)], None),
				("attack_type", ItemInfo("SWORD", 1, min_damage=16, max_damage=20, has_weapon_stats=True), [], None)):
			with self.subTest(reason=reason):
				with self.assertRaises(OracleError) as raised:
					self.attack(weapon, passives, equipped)
				self.assertIn(reason, str(raised.exception))


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5aRealDataTest(unittest.TestCase):
	@classmethod
	def setUpClass(cls):
		cls.data = StaticData(runner.DEFAULT_STATIC_DATA)

	def test_java_enum_data(self):
		enums = JavaEnums(JAVA_SRC)
		self.assertEqual(enums.classes["WARRIOR"], (True, 110, 90, 400, 400))
		self.assertEqual((enums.powers["WARRIOR"], enums.powers["MAGE"]), (110, 90), "the fourth PlayerClass constructor argument")
		self.assertEqual(enums.attack_type_magical, {"PHYSICAL": False, "MAGICAL_EARTH": True, "MAGICAL_WATER": True, "MAGICAL_WIND": True,
		                                             "MAGICAL_FIRE": True})
		self.assertEqual(enums.classes["GLADIATOR"], (False, 115, 90, 440, 400))
		self.assertEqual(enums.classes["BARD"], (False, 100, 110, 320, 520))
		self.assertEqual(enums.item_groups["SWORD"], (3, "WEAPON"))
		self.assertEqual(enums.item_groups["EARRING"], (192, "ARMOR"))
		self.assertEqual(enums.item_groups["NONE"], (0, "NONE"))
		self.assertEqual(enums.item_groups["STIGMA"][1], "STIGMA")
		self.assertEqual(enums.slot_for(3), 1, "MAIN_OR_SUB: MAIN_HAND first")
		self.assertEqual(enums.slot_for(192), 64, "EARRINGS_LEFT")
		self.assertEqual(enums.slot_for(1 << 12), 4096, "PANTS")

	def test_elyos_warrior(self):
		report = creation_report(self.data, JAVA_SRC, "ELYOS", "WARRIOR")
		self.assertEqual(report["spawn"], {"mapId": 210010000, "x": f32(1212.9423), "y": f32(1044.8516), "z": f32(140.75568), "heading": 32})
		self.assertEqual(report["baseStats"], {"maxHp": 284, "maxMp": 170}, "SM_STATS_INFO base: 244 + 40 health bonus, 210 - 40 will malus")
		self.assertEqual(report["statsTemplate"], {"maxHp": 244, "maxMp": 210}, "PlayerClass.createStatsTemplate alone")
		items = {i["itemId"]: i for i in report["items"]}
		self.assertEqual(items[182400001], {"itemId": 182400001, "count": 1000, "kinah": True, "equipped": False, "slot": 0})
		self.assertTrue(items[100000094]["equipped"])
		self.assertEqual(items[100000094]["slot"], 1, "the starting sword in the main hand")
		self.assertFalse(items[160000001]["equipped"])
		skills = {s["skillId"]: s["level"] for s in report["skills"]}
		self.assertEqual(skills.get(30001), 1, "human gathering for a starting class")
		self.assertEqual(skills.get(37), 1, "skill_tree.xml: 37 minLevel 1 autolearn WARRIOR")
		# m5b2-plan.md D2: of the eight passives, 37 Basic Sword Training raises the main hand of the Training Sword by 16 %, 42 Basic Chain
		# Armor Proficiency the physical defence of the chain torso and pants (30 + 25: 10 % * 55 / 100 = 5 %, plus 1 * 55 / 100f), 140 adds 7
		# physical attack; 39 (MACE), 40/41/103 (no such armor) and 43 (no shield) change nothing - and the base HP/MP above do not move
		applied = [(f["skillId"], f["function"], f["stat"], f["value"]) for f in report["passiveStatFunctions"] if f["applies"]]
		self.assertEqual(applied, [(37, "StatWeaponMasteryFunction", "MAIN_HAND_POWER", 16), (42, "StatArmorMasteryFunction", "PHYSICAL_DEFENSE", 5),
		                           (140, "StatAddFunction", "PHYSICAL_ATTACK", 7)])
		self.assertEqual(sorted({f["skillId"] for f in report["passiveStatFunctions"]}), [37, 39, 40, 41, 42, 43, 103, 140])
		# m5b2-plan.md X1: the Training Sword's 16-20 at power 110, raised by 37 and 140 - the 19/29 the M5b-2 part 3 regate measured on the wire
		self.assertEqual(report["statsInfo"], {"mainHandPAttack": {"base": 19, "current": 29}, "maxHp": {"base": 284, "bonus": 0.0, "current": 284},
		                                       "maxMp": {"base": 170, "bonus": 0.0, "current": 170}, "notModelled": []},
		                 "the Warrior's chain armor carries no MAXHP or MAXMP modifier")

	def test_asmodian_mage(self):
		report = creation_report(self.data, JAVA_SRC, "ASMODIANS", "MAGE")
		self.assertEqual(report["statsInfo"]["mainHandPAttack"], {"base": 0, "current": 0}, "a spellbook is a magical main hand")
		# the Training Tunic's <add name="MAXMP" value="26" bonus="true"/> and the Training Leggings' 21 (item_templates.xml): 315 + 90 + 47
		self.assertEqual(report["statsInfo"]["maxMp"], {"base": 405, "bonus": 47.0, "current": 452})
		self.assertEqual(report["statsInfo"]["maxHp"], {"base": 132, "bonus": 0.0, "current": 132})
		self.assertEqual(report["spawn"]["mapId"], 220010000)
		self.assertEqual(report["baseStats"], {"maxHp": 132, "maxMp": 405}, "SM_STATS_INFO base: 158 - 26, 315 + 90")
		self.assertEqual(report["statsTemplate"], {"maxHp": 158, "maxMp": 315})
		self.assertTrue(report["skills"])

	def test_poeta_start_spots(self):
		report = spots_report(self.data, 210010000, (1212.9423, 1044.8516, 140.75568), 100.0, GameClock(hour=10))
		self.assertTrue(report["spots"])
		self.assertTrue(all(s["distance"] < 100.001 for s in report["spots"]))
		self.assertTrue(any(s["deterministic"] for s in report["spots"]))
		target = border_target(self.data, 210010000, (1212.9423, 1044.8516, 140.75568), GameClock(hour=10))
		self.assertTrue(target["appear"] and target["disappear"])


if __name__ == "__main__":
	unittest.main()
