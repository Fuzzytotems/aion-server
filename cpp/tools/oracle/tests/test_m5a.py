"""M5a oracles (m5a/): spawn rules on small static_data trees, the game clock and temporary spawn times, PositionUtil range semantics, the
base stat formulas, and the real data for the two scenario characters. Expected values are derived by hand from the Java sources named in
m5a/spawns.py and m5a/creation.py. The real data cases are skipped without the Java tree.
"""

import unittest
import xml.etree.ElementTree as ET

from m5a.creation import JavaEnums, creation_report, max_hp, max_mp
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


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5aRealDataTest(unittest.TestCase):
	@classmethod
	def setUpClass(cls):
		cls.data = StaticData(runner.DEFAULT_STATIC_DATA)

	def test_java_enum_data(self):
		enums = JavaEnums(JAVA_SRC)
		self.assertEqual(enums.classes["WARRIOR"], (True, 400, 400))
		self.assertEqual(enums.classes["GLADIATOR"], (False, 440, 400))
		self.assertEqual(enums.classes["BARD"], (False, 320, 520))
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
		self.assertEqual(report["baseStats"], {"maxHp": 244, "maxMp": 210})
		items = {i["itemId"]: i for i in report["items"]}
		self.assertEqual(items[182400001], {"itemId": 182400001, "count": 1000, "kinah": True, "equipped": False, "slot": 0})
		self.assertTrue(items[100000094]["equipped"])
		self.assertEqual(items[100000094]["slot"], 1, "the starting sword in the main hand")
		self.assertFalse(items[160000001]["equipped"])
		skills = {s["skillId"]: s["level"] for s in report["skills"]}
		self.assertEqual(skills.get(30001), 1, "human gathering for a starting class")
		self.assertEqual(skills.get(37), 1, "skill_tree.xml: 37 minLevel 1 autolearn WARRIOR")

	def test_asmodian_mage(self):
		report = creation_report(self.data, JAVA_SRC, "ASMODIANS", "MAGE")
		self.assertEqual(report["spawn"]["mapId"], 220010000)
		self.assertEqual(report["baseStats"], {"maxHp": 158, "maxMp": 315})
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
