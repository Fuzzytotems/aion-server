"""M5b monster oracle (m5b/monster.py, m5b-plan.md G-01): the Java float formulas of the experience reward and of the two attack ranges, the
spot rules the gate picks its monster with, and the whole report on a small static_data tree and on the real data.

Expected values are derived by hand from the Java sources named in m5b/monster.py and repeated per case. Everything that reads the Java source
tree (the enum tables and the formula literals, and therefore the report itself) is skipped without it, like M5aRealDataTest.
"""

import unittest

from m5a.creation import JavaEnums
from m5a.data import StaticData
from m5a.javafloat import f32, round_to_int, to_long
from m5a.spawns import GameClock, evaluate, load_groups, load_npc_templates
from m5b.monster import (JavaCombatRules, attack_range, base_exp, exp_need, experience_reward, max_covered_distance, monster_report,
                         npc_template, player_weapon_stats, xp_hunting)
from staticdata_oracle import OracleError
from staticdata_oracle import run as runner

from .support import Tree, xml

JAVA_SRC = runner.TOOL_DIR.parents[2] / "game-server" / "src"
JAVA_HANDLERS = runner.TOOL_DIR.parents[2] / "game-server" / "data" / "handlers"
HAVE_JAVA_TREE = (JAVA_SRC / "com" / "aionemu" / "gameserver").is_dir() and runner.DEFAULT_STATIC_DATA.is_dir()

# One monster with every attribute the gate reads, and one with none of the optional ones (the JAXB defaults of NpcTemplate.java:54-110)
NPCS = (
	'<npc_template npc_id="900001" level="3" name="fixture beast" rank="SEASONED" rating="ELITE" race="BEAST" tribe="MONSTER" ai="aggressive"'
	' srange="7" sangle="240" arange="2" attack_speed="2500">'
	'<stats maxHp="100"/><bound_radius front="0.2" side="0.4" upper="1"/></npc_template>'
	'<npc_template npc_id="900002" name="fixture bare"><stats maxHp="50"/></npc_template>'
	'<npc_template npc_id="900003" name="fixture no stats"/>'
)

# distances from the Elyos spawn point (100, 100, 10): 10 (a static spot), 20 (a walker), 30 (the nearest plain spot), 50
SPAWNS = """
<spawn_map map_id="1">
	<spawn npc_id="900001" respawn_time="30">
		<spot x="110" y="100" z="10" h="1" static_id="5"/>
		<spot x="100" y="120" z="10" h="2" walker_id="W1"/>
		<spot x="100" y="100" z="40" h="3"/>
		<spot x="150" y="100" z="10" h="4" ai="noaction"/>
	</spawn>
</spawn_map>
"""

WEAPON = ('<item_template id="500" name="fixture sword" item_group="SWORD"><weapon_stats attack_range="1200" attack_speed="1600"/></item_template>'
          '<item_template id="501" name="fixture shield" item_group="SHIELD"/>'
          '<item_template id="502" name="fixture dagger" item_group="DAGGER"><weapon_stats attack_range="900" attack_speed="1200"/></item_template>')


def fixture_tree(items='<item id="500" count="1"/>', experience=("0", "1000", "5000", "20000"),
                 world_map='<map id="1" world_type="ELYSEA" world_size="1024"/>'):
	tree = Tree()
	tree.minimal({
		"world_maps": world_map,
		"npc_templates": NPCS,
		"spawns": SPAWNS,
		"item_templates": WEAPON,
		"player_experience_table": "".join(f"<exp>{e}</exp>" for e in experience),
		"player_initial_data": '<elyos_spawn_location map_id="1" x="100" y="100" z="10" heading="0"/>'
		                       f'<player_data class="WARRIOR"><items>{items}</items></player_data>',
	})
	tree.mkdir("handlers")
	return tree


class M5bFormulaTest(unittest.TestCase):
	"""The four formulas of D7 and the two ranges of G-01, in float arithmetic, without any Java source or static data."""

	def test_base_exp_is_max_hp_times_rating_plus_rank(self):
		# npc 210663: NORMAL 2.2f + DISCIPLINED (ordinal 1) * 0.2f = 2.4000001f; 199 * 2.4000001f = 477.60002f
		multiplier = f32(f32(2.2) + f32(1 * f32(0.2)))
		self.assertEqual(base_exp(199, multiplier), 478)
		self.assertEqual(base_exp(0, multiplier), 0, "calculateBaseExp returns 0 for a non-positive maxHp")
		self.assertEqual(base_exp(-5, multiplier), 0)
		self.assertEqual(base_exp(1, f32(2.0)), 2, "JUNK without a rank bonus")

	def test_experience_reward_rounds_the_float_product(self):
		# 478 * 1.25f * (105 / 100f) = 597.5f * 1.05f = 627.375f
		self.assertEqual(experience_reward(478, f32(1.25), 105), 627)
		self.assertEqual(experience_reward(478, f32(1.25), 100), 598, "597.5f rounds half up")
		self.assertEqual(experience_reward(0, f32(1.25), 105), 0)

	def test_xp_hunting_caps_the_reward_at_a_fifth_of_the_exp_need(self):
		# the level 1 pair of the M5b gate: reward 627, expNeed 400, cap 400 * 0.2f = 80f
		self.assertEqual(xp_hunting(627, 1.0, 400), 80)
		self.assertEqual(xp_hunting(627, 2.0, 400), 80, "the rate is applied before the cap and cannot raise it")
		self.assertEqual(xp_hunting(627, 1.0, 100000), 627, "below the cap the reward passes through")
		self.assertEqual(xp_hunting(627, f32(0.5), 100000), 313, "(long) 313.5f truncates")
		self.assertEqual(xp_hunting(627, 1.0, 0), 0, "at the max level expNeed is 0 and nothing is awarded")

	def test_exp_need_is_the_difference_of_two_start_exp_values(self):
		# getStartExpForLevel(level) is experience[level - 1], and experience[0] is the <exp>0</exp> of level 0, so level 1 needs
		# experience[1] - experience[0] = 400 - 0 = 400, NOT 1433 - 400 (which is what level 2 needs)
		table = [0, 400, 1433, 3820]
		self.assertEqual(exp_need(table, 1), 400)
		self.assertEqual(exp_need(table, 2), 1033)
		self.assertEqual(exp_need(table, 0), 0, "getStartExpForLevel(0) is 0 and getStartExpForLevel(1) is experience[0] = 0")
		self.assertEqual(exp_need(table, len(table)), 0, "the max level needs nothing")
		with self.assertRaises(OracleError):
			exp_need(table, len(table) + 1)

	def test_attack_range_adds_both_bound_radii(self):
		# PlayerController.java:402 with the training sword (1500) plus the player's 0.25 and npc 210663's max(0.55, 0.56)
		self.assertEqual(attack_range(1500, f32(0.25), f32(0.56)), f32(3.31))
		self.assertEqual(attack_range(0, 0.0, 0.0), 1.0, "without a weapon range and bound radii the range is the bare 1 m")
		self.assertEqual(attack_range(1500, 0.0, 0.0), 2.5)

	def test_max_covered_distance_is_the_movement_speed_over_a_hundred_milliseconds(self):
		# PositionUtil.java:289-294: 6000 (the player run speed 6f * 1000) * 100 / 1_000_000f
		self.assertEqual(max_covered_distance(6000, 100), f32(0.6))
		self.assertEqual(max_covered_distance(6000, 0), 0.0)
		self.assertEqual(max_covered_distance(6000, -1), 0.0)
		self.assertEqual(round_to_int(f32(f32(6.0) * 1000)), 6000)
		self.assertEqual(to_long(f32(0.9)), 0, "(long) truncates")


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5bJavaRulesTest(unittest.TestCase):
	"""The literals and enum tables the formulas need, as they stand in the Java sources today."""

	@classmethod
	def setUpClass(cls):
		cls.rules = JavaCombatRules.read(JAVA_SRC)

	def test_rating_multipliers_and_rank_ordinals(self):
		self.assertEqual(self.rules.rating_multipliers, {"JUNK": 2.0, "NORMAL": 2.2, "ELITE": 4.0, "HERO": 5.4, "LEGENDARY": 6.4})
		self.assertEqual(self.rules.rank_step, f32(0.2))
		self.assertEqual(self.rules.rank_ordinals["NOVICE"], 0)
		self.assertEqual(self.rules.rank_ordinals["DISCIPLINED"], 1)
		self.assertEqual(self.rules.rank_ordinals["MASTER"], 5)
		self.assertEqual(self.rules.rating_multiplier("NORMAL", "DISCIPLINED"), f32(2.4000001))
		with self.assertRaises(OracleError):
			self.rules.rating_multiplier(None, "NOVICE")
		with self.assertRaises(OracleError):
			self.rules.rating_multiplier("NORMAL", None)

	def test_xp_reward_table_and_its_clamps(self):
		self.assertEqual(self.rules.xp_reward_percent[0], 100)
		self.assertEqual(self.rules.xp_reward_percent[1], 105, "PLUS_1")
		self.assertEqual(self.rules.xp_reward_percent[-3], 90)
		self.assertEqual(self.rules.xp_reward_from(1), 105)
		self.assertEqual(self.rules.xp_reward_from(5), 120, "above PLUS_4 the reward stays at 120 %")
		self.assertEqual(self.rules.xp_reward_from(-12), 0, "below MINUS_11 nothing is awarded")
		self.assertEqual(self.rules.xp_reward_from(-11), 0)

	def test_player_literals(self):
		self.assertEqual((self.rules.player_bound_front, self.rules.player_bound_side), (f32(0.25), f32(0.25)),
		                 "PlayerAccountData: new BoundRadius(0.25f, 0.25f, boundHeight)")
		self.assertEqual(self.rules.player_run_speed, f32(6.0), "PlayerClass.PlayerStatsTemplate.getRunSpeed")
		self.assertEqual((self.rules.base_attack_range, self.rules.base_attack_speed), (1500, 1500))
		self.assertEqual((self.rules.exp_multiplier_open_world, self.rules.exp_multiplier_instance), (f32(1.25), f32(1.5)))


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5bFixtureReportTest(unittest.TestCase):
	"""The whole report on a small static_data tree (the formula literals and enum tables still come from the Java sources)."""

	def setUp(self):
		self.tree = fixture_tree()
		self.data = StaticData(self.tree.root)

	def tearDown(self):
		self.tree.close()

	def report(self, **kwargs):
		return monster_report(self.data, JAVA_SRC, self.tree.root / "handlers", 1, 900001, 1, GameClock(hour=10), **kwargs)

	def test_template_and_derived_experience(self):
		report = self.report()
		self.assertEqual(report["format"], "aion-m5b-monster")
		self.assertEqual(report["worldType"], "ELYSEA")
		self.assertEqual(report["player"]["race"], "ELYOS", "the world type picks the race whose spawn point the distances use")
		template = report["template"]
		self.assertEqual((template["level"], template["maxHp"], template["rating"], template["rank"], template["rankOrdinal"]),
		                 (3, 100, "ELITE", "SEASONED", 2))
		self.assertEqual((template["aggroRange"], template["aggroAngle"], template["attackRange"], template["attackSpeed"]), (7, 240, 2, 2500))
		self.assertEqual((template["race"], template["tribe"], template["ai"]), ("BEAST", "MONSTER", "aggressive"))
		self.assertEqual(template["boundRadius"], {"front": f32(0.2), "side": f32(0.4), "upper": 1.0, "maxOfFrontAndSide": f32(0.4)})
		# ELITE 4f + SEASONED (ordinal 2) * 0.2f = 4.4000001f; round(100 * 4.4000001f) = 440; round(440 * 1.25f * 1.1f) = 605;
		# expNeed(1) = 1000 - 0, so the cap 1000 * 0.2f = 200f is what a level 1 character gets
		self.assertEqual(report["exp"], {"ratingMultiplier": f32(4.4000001), "baseExp": 440, "expMultiplier": f32(1.25), "xpPercentage": 110,
		                                 "experienceReward": 605, "xpSoloRate": 1.0, "expNeed": 1000, "cap": 200.0, "awarded": 200})

	def test_ranges_use_the_main_hand_weapon_and_both_bound_radii(self):
		report = self.report()
		self.assertEqual((report["player"]["attackRangeStat"], report["player"]["attackSpeed"], report["player"]["movementSpeed"]),
		                 (1200, 1600, 6000))
		# 1 + 1200 / 1000f + 0.25 (player) + 0.4 (npc) = 2.8500001f, plus 6000 * 100 / 1_000_000f = 0.6f
		self.assertEqual(report["ranges"], {"attackRange": f32(2.8500001), "maxCoveredDistance": f32(0.6), "toleranceRange": f32(3.4500003)})

	def test_spots_are_sorted_by_distance_and_the_gate_spot_skips_static_ids_and_walkers(self):
		report = self.report()
		self.assertEqual([s["distance"] for s in report["spots"]], [10.0, 20.0, 30.0, 50.0])
		self.assertEqual([s["staticId"] for s in report["spots"]], [5, 0, 0, 0])
		self.assertEqual([s["fixed"] for s in report["spots"]], [True, False, True, True], "the walker spot is not fixed")
		self.assertFalse(report["pinned"], "one walker spot is enough to make an exact position assertion illegal")
		self.assertEqual(report["respawnTime"], 30)
		nearest = report["nearestPlainSpot"]
		self.assertEqual((nearest["x"], nearest["y"], nearest["z"], nearest["h"], nearest["distance"]), (100.0, 100.0, 40.0, 3, 30.0))
		self.assertEqual(nearest["staticId"], 0)
		self.assertEqual([s["ai"] for s in report["spots"]], ["aggressive", "aggressive", "aggressive", "noaction"],
		                 "a spot's ai attribute overrides the template's")

	def test_player_level_moves_the_reward(self):
		# a level 3 monster against a level 3 character is ZERO (100 %): round(440 * 1.25f * 1f) = 550
		report = monster_report(self.data, JAVA_SRC, self.tree.root / "handlers", 1, 900001, 3, GameClock(hour=10))
		self.assertEqual((report["exp"]["xpPercentage"], report["exp"]["experienceReward"]), (100, 550))
		self.assertEqual(report["exp"]["expNeed"], 15000, "level 3 needs 20000 - 5000")
		self.assertEqual(report["exp"]["awarded"], 550, "below the cap of 3000 the whole reward is awarded")
		# the table has four entries, so level 4 is the max level: getExpNeed answers 0 and the cap swallows the whole reward
		top = monster_report(self.data, JAVA_SRC, self.tree.root / "handlers", 1, 900001, 4, GameClock(hour=10))
		self.assertEqual((top["exp"]["xpPercentage"], top["exp"]["expNeed"], top["exp"]["awarded"]), (100, 0, 0))

	def test_template_defaults_and_missing_values(self):
		bare = npc_template(self.data, 900002)
		self.assertEqual((bare.aggro_angle, bare.attack_speed, bare.race, bare.aggro_range), (360, 2000, "NONE", 0))
		self.assertEqual((bare.rating, bare.rank, bare.tribe, bare.ai), (None, None, None, None))
		self.assertEqual((bare.bound_front, bare.bound_side, bare.bound_upper), (0.0, 0.0, 0.0), "BoundRadius.DEFAULT")
		with self.assertRaises(OracleError):
			npc_template(self.data, 900003)  # no <stats maxHp>
		with self.assertRaises(OracleError):
			npc_template(self.data, 900004)  # no template at all
		with self.assertRaises(OracleError):  # no rating: Java throws in the switch of calculateBaseExp
			monster_report(self.data, JAVA_SRC, self.tree.root / "handlers", 1, 900002, 1, GameClock(hour=10))

	def test_off_hand_rules(self):
		enums = JavaEnums(JAVA_SRC)
		rules = JavaCombatRules.read(JAVA_SRC)
		main = {"itemId": 500, "equipped": True, "slot": 1}
		with fixture_tree() as tree:
			data = StaticData(tree.root)
			self.assertEqual(player_weapon_stats(data, enums, rules, [main]), (1200, 1600))
			self.assertEqual(player_weapon_stats(data, enums, rules, []), (1500, 1500), "no weapon: the bases of PlayerGameStats")
			off = {"itemId": 502, "equipped": True, "slot": 2}  # ItemSlot.SUB_HAND
			self.assertEqual(player_weapon_stats(data, enums, rules, [main, off]), (900, 1900),
			                 "the smaller weapon range, and the off hand adds a quarter of its attack speed")
			shield = {"itemId": 501, "equipped": True, "slot": 2}
			with self.assertRaises(OracleError):  # Java: the shield has no <weapon_stats>, so getBaseAttackSpeed throws
				player_weapon_stats(data, enums, rules, [main, shield])

	def test_maps_the_oracle_refuses_to_answer_for(self):
		with fixture_tree(world_map='<map id="1" world_type="NONE" world_size="1024"/>') as tree:
			data = StaticData(tree.root)
			with self.assertRaises(OracleError):  # no race to measure the distances from
				monster_report(data, JAVA_SRC, tree.root / "handlers", 1, 900001, 1, GameClock(hour=10))
			self.assertEqual(monster_report(data, JAVA_SRC, tree.root / "handlers", 1, 900001, 1, GameClock(hour=10), race="ELYOS")["player"]["race"],
			                 "ELYOS")
		with fixture_tree(world_map='<map id="1" world_type="ELYSEA" world_size="1024" instance="true"/>') as tree:
			data = StaticData(tree.root)
			with self.assertRaises(OracleError):  # an instance multiplies by its maxPlayers, which is not static data
				monster_report(data, JAVA_SRC, tree.root / "handlers", 1, 900001, 1, GameClock(hour=10))
		with fixture_tree() as tree:
			data = StaticData(tree.root)
			handlers = tree.mkdir("handlers")
			(handlers / "FixtureInstance.java").write_text("@InstanceID(1)\npublic class FixtureInstance {}\n", encoding="utf-8")
			with self.assertRaises(OracleError):  # the handler may override getExpMultiplier
				monster_report(data, JAVA_SRC, handlers, 1, 900001, 1, GameClock(hour=10))
			with self.assertRaises(OracleError):  # no handler directory at all: the oracle does not guess
				monster_report(data, JAVA_SRC, tree.root / "no-handlers", 1, 900001, 1, GameClock(hour=10))


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5bRealDataTest(unittest.TestCase):
	"""npc 210663 on Poeta against a level 1 Elyos Warrior: the gate's own monster (m5b-plan.md D11, D7, G-01)."""

	@classmethod
	def setUpClass(cls):
		cls.data = StaticData(runner.DEFAULT_STATIC_DATA)
		cls.report = monster_report(cls.data, JAVA_SRC, JAVA_HANDLERS, 210010000, 210663, 1, GameClock(hour=10))

	def test_template(self):
		self.assertEqual(self.report["template"], {
			"level": 2, "maxHp": 199, "rating": "NORMAL", "rank": "DISCIPLINED", "rankOrdinal": 1, "aggroRange": 8, "aggroAngle": 270,
			"attackRange": 2, "attackSpeed": 2142, "race": "BEAST", "tribe": "MONSTER", "ai": "aggressive",
			"boundRadius": {"front": f32(0.55), "side": f32(0.56), "upper": f32(2.82), "maxOfFrontAndSide": f32(0.56)}})

	def test_the_gate_spot_is_the_nearest_one_without_a_static_id(self):
		spots = self.report["spots"]
		self.assertEqual(len(spots), 38)
		self.assertTrue(all(s["fixed"] for s in spots))
		self.assertTrue(self.report["pinned"], "V2 may assert the exact position of this id")
		self.assertEqual(sorted(s["staticId"] for s in spots if s["staticId"]), [3, 4], "two of the 38 carry a static id")
		# the nearest spot of all is the static_id 4 one at 38.673 m, which the gate must not take (D11)
		self.assertEqual((spots[0]["staticId"], spots[0]["distance"]), (4, 38.673))
		nearest = self.report["nearestPlainSpot"]
		self.assertEqual((nearest["x"], nearest["y"], nearest["z"], nearest["h"]),
		                 (f32(1226.22), f32(1096.57), f32(141.93), 2))
		self.assertEqual((nearest["staticId"], nearest["distance"]), (0, 53.408))
		self.assertEqual(self.report["respawnTime"], 20)

	def test_experience_of_the_kill(self):
		# 199 * (2.2f + 0.2f) = 478; 478 * 1.25f * 1.05f = 627; expNeed(1) = 400 - 0, so Rates.XP_HUNTING caps the award at 400 * 0.2f = 80
		self.assertEqual(self.report["exp"], {"ratingMultiplier": f32(2.4000001), "baseExp": 478, "expMultiplier": f32(1.25), "xpPercentage": 105,
		                                      "experienceReward": 627, "xpSoloRate": 1.0, "expNeed": 400, "cap": 80.0, "awarded": 80})

	def test_ranges_of_the_first_hit(self):
		self.assertEqual((self.report["player"]["attackRangeStat"], self.report["player"]["attackSpeed"]), (1500, 1400),
		                 "the training sword's weapon_stats")
		self.assertEqual(self.report["ranges"], {"attackRange": f32(3.31), "maxCoveredDistance": f32(0.6), "toleranceRange": f32(3.9099998)})

	def test_the_spot_fields_reach_the_m5a_spawn_rows(self):
		rows = [r for r in evaluate(load_groups(self.data, 210010000), load_npc_templates(self.data), GameClock(hour=10)) if r["npcId"] == 210663]
		self.assertEqual(len(rows), 38)
		self.assertEqual(sorted({r["respawnTime"] for r in rows}), [20])
		self.assertEqual(sorted(r["staticId"] for r in rows if r["staticId"]), [3, 4])


if __name__ == "__main__":
	unittest.main()
