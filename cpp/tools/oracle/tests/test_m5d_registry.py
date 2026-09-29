"""m5d-quests --registration-order and --census (m5d/registry.py, m5d-plan.md G-01 and §18.3): the XML registry's lists and the quest census
of §2.3-§2.5, on small static_data trees (with the fixture Java handler of test_m5d) and on the real data.

Expected values are derived by hand from the Java rules named in m5d/registry.py and repeated per case; the real-data census repeats the
counts m5d-plan.md §2.3-§2.5 measured (rev 2). Everything that reads the Java source tree is skipped without it.
"""

import contextlib
import io
import json
import unittest
from unittest import mock

from m5a.data import StaticData
from m5d.quests import QuestWorld
from m5d.registry import census_report, registration_order_report
from staticdata_oracle import run as runner

import oracle

from .support import Tree
from .test_m5d import HAVE_JAVA_TREE, JAVA_CONFIG, JAVA_HANDLER, JAVA_QUEST_HANDLERS, JAVA_SRC


def world_dirs(quests: str, scripts: str, java: dict[str, str] | None = None, **holders) -> tuple[Tree, object, object]:
	"""A static_data tree with the given holders, test_m5d's Java handler (quest 106, start npc 800003), the `java` files (path under
	data/handlers -> text) and an empty config directory."""
	tree = Tree()
	tree.minimal({"quests": quests, "quest_scripts": scripts, **holders})
	handlers = tree.root.parent / "handlers" / "quest" / "fixture"
	handlers.mkdir(parents=True)
	(handlers / "_106FixtureJava.java").write_text(JAVA_HANDLER, encoding="utf-8")
	for rel, text in (java or {}).items():
		path = tree.root.parent / "handlers" / rel
		path.parent.mkdir(parents=True, exist_ok=True)
		path.write_text(text, encoding="utf-8")
	(tree.root.parent / "config" / "main").mkdir(parents=True)
	return tree, tree.root.parent / "handlers" / "quest", tree.root.parent / "config"


def run_cli(argv: list[str]) -> tuple[int, dict | None, str]:
	out, err = io.StringIO(), io.StringIO()
	with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
		code = oracle.main(argv)
	return code, json.loads(out.getvalue()) if code == 0 else None, err.getvalue()


# Document order 316, 305, 289, 320, 311, 327, 342, 333: eight ids below 65536 in a new HashMap<>() of 16 buckets (12 < 13 entries), so the
# bucket is id & 15: 320 -> 0, 305 and 289 -> 1 (in that insertion order), 342 -> 6, 311 and 327 -> 7, 316 -> 12, 333 -> 13.
REG_ORDER = [320, 305, 289, 342, 311, 327, 316, 333]

REG_QUESTS = """
<quest id="316" name="hunt" minlevel_permitted="1"><rewards exp="5"/><quest_kill var="0" count="1" npc_ids="920001 920002" seq="0"/></quest>
<quest id="305" name="hunt" minlevel_permitted="1"><rewards exp="5"/><quest_kill var="0" count="1" npc_ids="920002 920001" seq="0"/></quest>
<quest id="289" name="invasion hunt" minlevel_permitted="1"><rewards exp="5"/><quest_kill var="0" count="1" npc_ids="920001" seq="0"/></quest>
<quest id="320" name="elyos level" minlevel_permitted="30" race_permitted="ELYOS"><rewards exp="5"/></quest>
<quest id="311" name="report" minlevel_permitted="1"><rewards exp="5"/></quest>
<quest id="327" name="any race level" minlevel_permitted="30"><rewards exp="5"/></quest>
<quest id="342" name="pc_all level" minlevel_permitted="30" race_permitted="PC_ALL"><rewards exp="5"/></quest>
<quest id="333" name="invasion kills" minlevel_permitted="1"><rewards exp="5"/></quest>
"""

REG_SCRIPTS = """
<monster_hunt id="316" start_npc_ids="820001" end_npc_ids="820002"/>
<monster_hunt id="305" start_npc_ids="820001" end_npc_ids="820002 820005"/>
<monster_hunt id="289" start_npc_ids="820001" end_npc_ids="820005" invasion_world="210020000"/>
<report_on_levelup id="320" end_npc_ids="820002"/>
<report_to id="311" start_npc_ids="820003"/>
<report_on_levelup id="327" end_npc_ids="820002"/>
<report_on_levelup id="342" end_npc_ids="820003"/>
<kill_in_world id="333" end_npc_ids="820004" worlds="210020000" amount="1" invasion_world="210020000"/>
"""

NEITHER = {"ascending": False, "documentOrder": False}


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5dRegistrationOrderFixtureTest(unittest.TestCase):
	"""QuestEngine.init's XML registration order and the lists it builds, on eight fixture quests whose HashMap order is neither ascending
	nor the document order."""

	@classmethod
	def setUpClass(cls):
		cls.tree, handlers, config = world_dirs(REG_QUESTS, REG_SCRIPTS)
		cls.world = QuestWorld(StaticData(cls.tree.root), JAVA_SRC, handlers, config)
		cls.report = registration_order_report(cls.world)

	@classmethod
	def tearDownClass(cls):
		cls.tree.close()

	def test_the_registration_order_is_the_hash_map_order(self):
		self.assertEqual(self.report["order"], REG_ORDER)
		self.assertEqual(self.report["orderFlags"], NEITHER)
		self.assertEqual((self.report["registry"], self.report["xmlQuests"]), ("xml", 8))

	def test_the_enter_world_and_level_up_lists(self):
		# registerOnEnterWorld: every report_on_levelup, and monster_hunt/kill_in_world with an invasion_world, in registration order
		self.assertEqual(self.report["questOnEnterWorld"], [320, 289, 342, 327, 333])
		self.assertEqual(self.report["questOnEnterWorldFlags"], NEITHER, "ascending 289, 320, 327, 333, 342; document 289, 320, 327, 342, 333")
		# registerOnLevelChanged: 320 (ELYOS), then 342 (PC_ALL, a list onLevelChanged never reads), then 327 (no race: both races' lists)
		self.assertEqual(self.report["questOnLevelUp"], {"ASMODIANS": [327], "ELYOS": [320, 327], "PC_ALL": [342]})

	def test_every_npcs_lists(self):
		self.assertEqual(self.report["onTalkEvent"], {"820001": [305, 289, 316], "820002": [320, 305, 327, 316], "820003": [342, 311],
		                                              "820004": [333], "820005": [305, 289]})
		# a HashSet<>(0) of 305, 289, 316: 1 bucket, 2 at the first put, 4 at the second (305 and 289 in bucket 1), 316 in bucket 0
		self.assertEqual(self.report["onQuestStart"], {"820001": [316, 305, 289], "820003": [311]})
		# the monster hunts 305 (920002, 920001), 289 (920001) and 316 (920001, 920002), appended in registration order
		self.assertEqual(self.report["onKillEvent"], {"920001": [305, 289, 316], "920002": [305, 316]})
		# 820005's [305, 289] is the document order, not the ascending one
		self.assertEqual(self.report["talkLists"], {"npcs": 5, "withSeveralQuests": 4, "notAscending": 4, "notDocumentOrder": 3,
		                                            "neitherAscendingNorDocumentOrder": [820001, 820002, 820003]})
		# 920001's list is neither ascending (289, 305, 316) nor the document order (316, 305, 289); 920002's is ascending only
		self.assertEqual(self.report["killLists"], {"npcs": 2, "withSeveralQuests": 2, "notAscending": 1, "notDocumentOrder": 2,
		                                            "neitherAscendingNorDocumentOrder": [920001]})
		# 820001's [316, 305, 289] is the document order, not the insertion order 305, 289, 316
		self.assertEqual(self.report["startLists"], {"npcs": 2, "withSeveralQuests": 1, "notAscending": 1, "notDocumentOrder": 0,
		                                             "notInsertionOrder": 1, "neitherAscendingNorDocumentNorInsertionOrder": []})

	def test_one_npc(self):
		npc = registration_order_report(self.world, 820001)["npc"]
		self.assertEqual(npc, {"registered": True, "npcId": 820001,
		                       "onQuestStart": [316, 305, 289],
		                       "onQuestStartFlags": {"ascending": False, "documentOrder": True, "insertionOrder": False},
		                       "onTalkEvent": [305, 289, 316], "onTalkEventFlags": NEITHER,
		                       "onKillEvent": [], "onKillEventFlags": {"ascending": True, "documentOrder": True},
		                       "onQuestStartInsertion": [305, 289, 316], "javaStartQuests": []})
		monster = registration_order_report(self.world, 920001)["npc"]
		self.assertEqual((monster["registered"], monster["onKillEvent"], monster["onKillEventFlags"]), (True, [305, 289, 316], NEITHER))
		talk_only = registration_order_report(self.world, 820002)["npc"]
		self.assertEqual((talk_only["registered"], talk_only["onQuestStart"], talk_only["onTalkEvent"]), (True, [], [320, 305, 327, 316]),
		                 "an npc with talk events only is registered")
		java = registration_order_report(self.world, 800003)["npc"]
		self.assertEqual((java["registered"], java["onTalkEvent"], java["javaStartQuests"]), (False, [], [106]),
		                 "the Java handler's start npc: not in the XML registry (D9)")
		self.assertNotIn("onTalkEvent", registration_order_report(self.world, 820001), "with --npc only that npc's lists")

	def test_the_command_line(self):
		with mock.patch("m5d.quests.QuestWorld", return_value=self.world):
			code, answer, _ = run_cli(["m5d-quests", "--registration-order", "--npc", "820002"])
			self.assertEqual(code, 0)
			self.assertEqual((answer["format"], answer["npc"]["onTalkEvent"]), ("aion-m5d-registration-order", [320, 305, 327, 316]))
			code, answer, _ = run_cli(["m5d-quests", "--registration-order"])
			self.assertEqual((code, answer["order"], answer["onTalkEvent"]["820003"]), (0, REG_ORDER, [342, 311]))
			refused = [(["--npc", "820001", "--census"], "--npc goes with --registration-order")]
			for mode, option in ((["--census"], ["--completed", "305"]), (["--registration-order"], ["--race", "ELYOS"]),
			                     (["--census"], ["--gender", "MALE"]), (["--registration-order"], ["--started", "305"]),
			                     (["--census"], ["--inventory", "990"]), (["--registration-order"], ["--class", "CLERIC"]),
			                     (["--census"], ["--level", "1"]), (["--registration-order"], ["--game-minutes", "0"]),
			                     (["--census"], ["--game-hour", "0"]), (["--registration-order"], ["--game-day", "1"]),
			                     (["--census"], ["--game-month", "1"]), (["--registration-order"], ["--weekday", "MONDAY"])):
				refused.append((mode + option, f"{option[0]} describes a character or the game time"))
			for argv, message in refused:
				with self.subTest(argv=argv):
					code, _, err = run_cli(["m5d-quests"] + argv)
					self.assertEqual(code, 2)
					self.assertIn(f"oracle error: {message}", err)
			for argv in (["--census", "--map", "1"], []):  # argparse: exactly one mode
				with self.subTest(argv=argv), contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
					oracle.main(["m5d-quests"] + argv)
			# the --map path still defaults the character to a level 1 WARRIOR
			with mock.patch("m5d.quests.map_report", return_value={}) as map_report:
				self.assertEqual(run_cli(["m5d-quests", "--map", "1"])[0], 0)
				self.assertEqual(map_report.call_args.args[3:5], ("WARRIOR", 1))
				self.assertEqual(run_cli(["m5d-quests", "--map", "1", "--class", "CLERIC", "--level", "7"])[0], 0)
				self.assertEqual(map_report.call_args.args[3:5], ("CLERIC", 7))


# Document order 300, 263, 512, 274, 401, 268: six ids below 65536 in a new HashMap<>() of 16 buckets, so the bucket is id & 15: 512 -> 0,
# 401 -> 1, 274 -> 2, 263 -> 7, 300 and 268 -> 12 (in that insertion order).
FLAGS_ORDER = [512, 401, 274, 263, 300, 268]

FLAGS_QUESTS = """
<quest id="300" name="report" minlevel_permitted="1"><rewards exp="5"/></quest>
<quest id="263" name="report" minlevel_permitted="1"><rewards exp="5"/></quest>
<quest id="512" name="elyos level" minlevel_permitted="30" race_permitted="ELYOS"><rewards exp="5"/></quest>
<quest id="274" name="report" minlevel_permitted="1"><rewards exp="5"/></quest>
<quest id="401" name="any race level" minlevel_permitted="30"><rewards exp="5"/></quest>
<quest id="268" name="report" minlevel_permitted="1"><rewards exp="5"/></quest>
"""

FLAGS_SCRIPTS = """
<report_to id="300" start_npc_ids="840003"/>
<report_to id="263" start_npc_ids="840001 840002"/>
<report_on_levelup id="512" end_npc_ids="840004"/>
<report_to id="274" start_npc_ids="840001 840002"/>
<report_on_levelup id="401" end_npc_ids="840004"/>
<report_to id="268" start_npc_ids="840001"/>
"""


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5dRegistrationFlagsFixtureTest(unittest.TestCase):
	"""The order flags and the list summaries where they differ: a questOnEnterWorld in document order under an order that is neither, a
	level-up list that is not ascending, and start sets whose HashSet<>(0) order is and is not their insertion order."""

	@classmethod
	def setUpClass(cls):
		cls.tree, handlers, config = world_dirs(FLAGS_QUESTS, FLAGS_SCRIPTS)
		cls.world = QuestWorld(StaticData(cls.tree.root), JAVA_SRC, handlers, config)
		cls.report = registration_order_report(cls.world)

	@classmethod
	def tearDownClass(cls):
		cls.tree.close()

	def test_the_order_and_the_engine_lists(self):
		self.assertEqual((self.report["order"], self.report["orderFlags"]), (FLAGS_ORDER, NEITHER))
		# the two report_on_levelup quests register 512 before 401: the document order, not the ascending one
		self.assertEqual(self.report["questOnEnterWorld"], [512, 401])
		self.assertEqual(self.report["questOnEnterWorldFlags"], {"ascending": False, "documentOrder": True})
		# 512 (ELYOS), then 401 (no race: ASMODIANS' list, then ELYOS')
		self.assertEqual(self.report["questOnLevelUp"], {"ASMODIANS": [401], "ELYOS": [512, 401]})

	def test_the_start_sets(self):
		self.assertEqual(self.report["onTalkEvent"], {"840001": [274, 263, 268], "840002": [274, 263], "840003": [300], "840004": [512, 401]})
		# HashSet<>(0): 4 buckets from the second put on, the bucket is id & 3: 268 -> 0, 274 -> 2, 263 -> 3
		self.assertEqual(self.report["onQuestStart"], {"840001": [268, 274, 263], "840002": [274, 263], "840003": [300]})
		self.assertEqual(self.report["talkLists"], {"npcs": 4, "withSeveralQuests": 3, "notAscending": 3, "notDocumentOrder": 2,
		                                            "neitherAscendingNorDocumentOrder": [840001, 840002]})
		# 840001's [268, 274, 263]: not ascending (263, 268, 274), not the document order (263, 274, 268), not the insertion order
		# (274, 263, 268); 840002's [274, 263] is its insertion order
		self.assertEqual(self.report["startLists"], {"npcs": 3, "withSeveralQuests": 2, "notAscending": 2, "notDocumentOrder": 2,
		                                             "notInsertionOrder": 1, "neitherAscendingNorDocumentNorInsertionOrder": [840001]})
		self.assertEqual(self.report["killLists"], {"npcs": 0, "withSeveralQuests": 0, "notAscending": 0, "notDocumentOrder": 0,
		                                            "neitherAscendingNorDocumentOrder": []})
		for npc, insertion, flags in ((840001, [274, 263, 268], {"ascending": False, "documentOrder": False, "insertionOrder": False}),
		                              (840002, [274, 263], {"ascending": False, "documentOrder": False, "insertionOrder": True})):
			with self.subTest(npc=npc):
				view = registration_order_report(self.world, npc)["npc"]
				self.assertEqual((view["onQuestStartInsertion"], view["onQuestStartFlags"]), (insertion, flags))


# One XML quest per census rule. npcs: 830001 general, 830002 aggressive, 830003 simple_abyssguard, 830004 following, 830005 dummy with a
# spot ai "general", 830006 general with a spot ai "__NO_AI__", 830007 a siege spawn, 830008 an instance map's spawn, 830009 (dummy) a RIFT
# spawn, 830010 difficulty 1 on map 1, 830011 a base spawn and an instance spawn, 830012 no spawn, 830013 a level 2 town spawn (and a timed
# event's), 830014 a level 1 town spawn, 830015 a timed event's spawn, 830016 a spawn without spots, 830017 a spawn without npc template,
# 830018 a STATIC spawn, 830019 a SENTINEL handler spawn, 830020 a spawn on a map world_maps does not have, 400101 a gatherable's spawn; the
# houses: 830021 land 1's manager (address 1 on map 1 has a MANAGER and a SIGN spawn), 830024 its teleport npc (no TELEPORT spawn), 830025
# its sale sign (and a timed event's spawn), 830022 the manager of a studio land (default building 2, PERSONAL_INS), 830023 the manager of
# a land on the instance map 2, 830028 land 4's teleport npc (address 4 has a TELEPORT spawn only; the npc also has a STATIC spawn); 830030
# named by a fixture instance handler, 800003 by the fixture Java quest handler, 830052 by game-server/src (CraftSkillUpdateService).
CENSUS_QUESTS = """
<quest id="106" name="java" quest_zone="Poeta" minlevel_permitted="1"><rewards exp="5"/></quest>
<quest id="401" name="disabled" minlevel_permitted="99"><rewards exp="5"/></quest>
<quest id="402" name="item order" minlevel_permitted="1" category="CHALLENGE_TASK"><rewards exp="5"/></quest>
<quest id="403" name="level up" minlevel_permitted="30" race_permitted="ELYOS"><rewards exp="5"/></quest>
<quest id="404" name="talk" quest_zone="Poeta" minlevel_permitted="1" restricted="true"><rewards ap="10"/></quest>
<quest id="405" name="hunt" minlevel_permitted="1" restricted="true"><rewards exp="5"/><bonus level="20" type="MANASTONE"/>
	<quest_kill var="0" count="1" npc_ids="930001" seq="0"/></quest>
<quest id="406" name="work order" minlevel_permitted="1"><rewards exp="5"/><extended_rewards ap="5"/></quest>
<quest id="407" name="relic" minlevel_permitted="1"><rewards gp="3"/></quest>
<quest id="408" name="after java" minlevel_permitted="1"><rewards exp="5"/><start_conditions><finished quest_id="106"/></start_conditions></quest>
<quest id="409" name="objects" minlevel_permitted="1"><rewards exp="5"/>
	<start_conditions><finished quest_id="106"/></start_conditions><start_conditions><finished quest_id="404"/></start_conditions>
	<collect_items><collect_item item_id="182200201" count="1"/></collect_items><quest_drop npc_id="700001" item_id="182200201"/></quest>
<quest id="410" name="acquired java" minlevel_permitted="1"><rewards exp="5"/><start_conditions><acquired>106</acquired></start_conditions></quest>
<quest id="411" name="loot" minlevel_permitted="1"><rewards exp="5"/><start_conditions><unfinished>106</unfinished></start_conditions>
	<collect_items><collect_item item_id="182200202" count="1"/></collect_items><quest_drop npc_id="930001" item_id="182200202"/></quest>
<quest id="412" name="following" minlevel_permitted="1"><rewards exp="5"/></quest>
<quest id="413" name="elsewhere" minlevel_permitted="1"><rewards exp="5"/><collect_items><collect_item item_id="182200203" count="1"/></collect_items></quest>
<quest id="414" name="no ai" minlevel_permitted="1"><rewards extend_inventory="1"/></quest>
<quest id="415" name="siege" minlevel_permitted="1"><rewards extend_inventory="2"/></quest>
<quest id="416" name="instance after java" minlevel_permitted="1"><rewards exp="5"/><start_conditions><finished quest_id="106"/></start_conditions></quest>
<quest id="417" name="rift" minlevel_permitted="1"><rewards exp="5"/></quest>
<quest id="418" name="difficulty" minlevel_permitted="1"><rewards exp="5"/></quest>
<quest id="419" name="base and instance" minlevel_permitted="1"><rewards exp="5"/></quest>
<quest id="420" name="nowhere" minlevel_permitted="1" category="CHALLENGE_TASK"><rewards exp="5"/></quest>
<quest id="421" name="town level 2" minlevel_permitted="1"><rewards exp="5"/></quest>
<quest id="422" name="town level 1" minlevel_permitted="1"><rewards exp="5"/></quest>
<quest id="423" name="event" minlevel_permitted="1"><rewards exp="5"/></quest>
<quest id="424" name="no spot" minlevel_permitted="1"><rewards exp="5"/></quest>
<quest id="425" name="no template" minlevel_permitted="1"><rewards exp="5"/></quest>
<quest id="426" name="skill" minlevel_permitted="1" category="CHALLENGE_TASK"><rewards exp="5"/></quest>
<quest id="427" name="zone kills" minlevel_permitted="1"><rewards exp="5"/></quest>
<quest id="428" name="kill spawned" minlevel_permitted="1"><rewards exp="5"/></quest>
<quest id="429" name="no handler" quest_zone="Poeta" minlevel_permitted="1"/>
<quest id="430" name="no handler" quest_zone="Ishalgen" minlevel_permitted="1"/>
<quest id="431" name="static" minlevel_permitted="1"><rewards exp="5"/></quest>
<quest id="432" name="other handler" minlevel_permitted="1"><rewards exp="5"/></quest>
<quest id="433" name="no world map" minlevel_permitted="1"><rewards exp="5"/></quest>
<quest id="434" name="gatherable" minlevel_permitted="1"><rewards exp="5"/></quest>
<quest id="435" name="mandatory java group" minlevel_permitted="1"><rewards exp="5"/>
	<start_conditions><acquired>106</acquired></start_conditions><start_conditions><noacquired>404</noacquired></start_conditions></quest>
<quest id="436" name="house manager" minlevel_permitted="1"><rewards exp="5"/></quest>
<quest id="437" name="no teleport spawn" minlevel_permitted="1"><rewards exp="5"/></quest>
<quest id="438" name="house sign" minlevel_permitted="1"><rewards exp="5"/></quest>
<quest id="439" name="studio" minlevel_permitted="1"><rewards exp="5"/></quest>
<quest id="440" name="instance house" minlevel_permitted="1"><rewards exp="5"/></quest>
<quest id="441" name="instance handler" minlevel_permitted="1"><rewards exp="5"/></quest>
<quest id="442" name="quest handler" minlevel_permitted="1"><rewards exp="5"/></quest>
<quest id="443" name="house teleport" minlevel_permitted="1"><rewards exp="5"/></quest>
<quest id="444" name="named in src" minlevel_permitted="1"><rewards exp="5"/></quest>
"""

CENSUS_SCRIPTS = """
<report_to id="401" start_npc_ids="830001"/>
<item_order id="402" talk_npc_id1="830001" end_npc_id="830001"/>
<report_on_levelup id="403" end_npc_ids="830001"/>
<report_to id="404" start_npc_ids="830001"/>
<monster_hunt id="405" start_npc_ids="830002"/>
<work_order id="406" start_npc_ids="830003 830001" recipe_id="1"><give_component item_id="182290000" count="1"/></work_order>
<relic_rewards id="407" start_npc_ids="830003"/>
<report_to id="408" start_npc_ids="830001"/>
<item_collecting id="409" start_npc_ids="830001"/>
<report_to id="410" start_npc_ids="830003"/>
<item_collecting id="411" start_npc_ids="830001"/>
<report_to id="412" start_npc_ids="830004"/>
<item_collecting id="413" start_npc_ids="830005"/>
<report_to id="414" start_npc_ids="830006"/>
<report_to id="415" start_npc_ids="830007"/>
<report_to id="416" start_npc_ids="830008"/>
<report_to id="417" start_npc_ids="830009"/>
<report_to id="418" start_npc_ids="830010"/>
<report_to id="419" start_npc_ids="830011"/>
<report_to id="420" start_npc_ids="830012"/>
<report_to id="421" start_npc_ids="830013"/>
<report_to id="422" start_npc_ids="830014"/>
<report_to id="423" start_npc_ids="830015"/>
<report_to id="424" start_npc_ids="830016"/>
<report_to id="425" start_npc_ids="830017"/>
<skill_use id="426" start_npc_ids="830001"><skill end_var="1" ids="1"/></skill_use>
<kill_in_zone id="427" start_npc_ids="830002" zones="FIXTURE_ZONE" amount="1"/>
<kill_spawned id="428" start_npc_ids="830001"><monster var="0" end_var="1" npc_ids="930002" spawner_object_id="730001"/></kill_spawned>
<report_to id="431" start_npc_ids="830018"/>
<report_to id="432" start_npc_ids="830019"/>
<report_to id="433" start_npc_ids="830020"/>
<report_to id="434" start_npc_ids="400101"/>
<report_to id="435" start_npc_ids="830001"/>
<report_to id="436" start_npc_ids="830021"/>
<report_to id="437" start_npc_ids="830024"/>
<report_to id="438" start_npc_ids="830025"/>
<report_to id="439" start_npc_ids="830022"/>
<report_to id="440" start_npc_ids="830023"/>
<report_to id="441" start_npc_ids="830030"/>
<report_to id="442" start_npc_ids="800003"/>
<report_to id="443" start_npc_ids="830028"/>
<report_to id="444" start_npc_ids="830052"/>
"""

CENSUS_AIS = {830001: "general", 830002: "aggressive", 830003: "simple_abyssguard", 830004: "following", 830005: "dummy", 830006: "general",
              830007: "general", 830008: "general", 830009: "dummy", 830010: "general", 830011: "general", 830012: "general",
              830013: "general", 830014: "general", 830015: "general", 830016: "general", 830018: "general", 830019: "general",
              830020: "general", 830021: "general", 830022: "general", 830023: "general", 830024: "general", 830025: "general",
              830028: "general", 830030: "general", 830052: "general", 400101: "general", 930001: "general", 930002: "general"}

# a fixture instance handler: 830030 is spawned; 830024 appears only inside a floating literal and 830022 only inside a name
CENSUS_JAVA = {"instance/FixtureInstance.java": """package instance;

public class FixtureInstance extends GeneralInstanceHandler {

	@Override
	public void onEnterInstance(Player player) {
		spawn(830030, 1f, 1f, 1f, (byte) 0);
		spawn(1, 830024.5f, 1f, 1f, (byte) 0);
		int x830022 = 0;
	}
}
"""}

SPOT = '<spot x="1" y="1" z="1"/>'

CENSUS_HOLDERS = {
	"world_maps": '<map id="1" world_type="ELYSEA" world_size="1024"/><map id="2" instance="true" world_size="1024"/>',
	"npc_templates": "".join(f'<npc_template npc_id="{npc}" name="fixture {npc}" level="1" ai="{ai}"/>' for npc, ai in CENSUS_AIS.items()),
	"spawns": f"""
<spawn_map map_id="1">
	<spawn npc_id="830001">{SPOT}</spawn>
	<spawn npc_id="830002">{SPOT}</spawn>
	<spawn npc_id="830003">{SPOT}</spawn>
	<spawn npc_id="830004">{SPOT}</spawn>
	<spawn npc_id="830005"><spot x="1" y="1" z="1" ai="general"/></spawn>
	<spawn npc_id="830006"><spot x="1" y="1" z="1" ai="__NO_AI__"/></spawn>
	<spawn npc_id="830009" handler="RIFT">{SPOT}</spawn>
	<spawn npc_id="830010" difficult_id="1">{SPOT}</spawn>
	<spawn npc_id="830016"/>
	<spawn npc_id="830017">{SPOT}</spawn>
	<spawn npc_id="830018" handler="STATIC">{SPOT}</spawn>
	<spawn npc_id="830019" handler="SENTINEL">{SPOT}</spawn>
	<spawn npc_id="830028" handler="STATIC">{SPOT}</spawn>
	<spawn npc_id="400101">{SPOT}</spawn>
	<spawn npc_id="930001">{SPOT}</spawn>
	<siege_spawn siege_id="1"><siege_race race="ELYOS"><siege_mod mod="PEACE"><spawn npc_id="830007">{SPOT}</spawn></siege_mod></siege_race></siege_spawn>
	<base_spawn id="1"><occupier_template occupier="PEACE"><spawn npc_id="830011">{SPOT}</spawn></occupier_template></base_spawn>
</spawn_map>
<spawn_map map_id="2">
	<spawn npc_id="830008">{SPOT}</spawn>
	<spawn npc_id="830011">{SPOT}</spawn>
</spawn_map>
<spawn_map map_id="3">
	<spawn npc_id="830020">{SPOT}</spawn>
</spawn_map>
""",
	"town_spawns_data": f'<spawn_map map_id="1"><town_spawn town_id="1"><town_level level="1"><spawn npc_id="830014">{SPOT}</spawn></town_level>'
	                    f'<town_level level="2"><spawn npc_id="830013">{SPOT}</spawn></town_level></town_spawn></spawn_map>',
	"timed_events": f'<event name="fixture" start="2015-01-01T00:00:00" end="2015-01-02T00:00:00"><spawns><spawn_map map_id="1">'
	                f'<spawn npc_id="830015">{SPOT}</spawn><spawn npc_id="830013">{SPOT}</spawn><spawn npc_id="830025">{SPOT}</spawn>'
	                f'</spawn_map></spawns></event>',
	"buildings": '<building id="1" type="PERSONAL_FIELD"/><building id="2" type="PERSONAL_INS"/>',
	"house_lands": "".join(
		f'<land id="{land}" manager_npc="{manager}" teleport_npc="{teleport}" sign_nosale="{signs[0]}" sign_sale="{signs[1]}" '
		f'sign_waiting="{signs[2]}" sign_home="{signs[3]}"><addresses><address id="{land}" map="{map_id}"/></addresses>'
		f'<buildings>{buildings}</buildings></land>'
		for land, manager, teleport, signs, map_id, buildings in (
			(1, 830021, 830024, (830026, 830025, 830026, 830026), 1, '<building id="1" default="true"/>'),
			(2, 830022, 830022, (830022,) * 4, 1, '<building id="1"/><building id="2" default="true"/>'),
			(3, 830023, 830023, (830023,) * 4, 2, '<building id="1" default="true"/>'),
			(4, 830027, 830028, (830029,) * 4, 1, '<building id="1" default="true"/>'))),
	"house_npcs": '<house address="1"><spawn type="MANAGER"/><spawn type="SIGN"/></house><house address="2"><spawn type="MANAGER"/></house>'
	              '<house address="3"><spawn type="MANAGER"/></house><house address="4"><spawn type="TELEPORT"/></house>',
}


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5dCensusFixtureTest(unittest.TestCase):
	"""The census rules of m5d/registry.py, one fixture quest per rule."""

	@classmethod
	def setUpClass(cls):
		cls.tree, handlers, config = world_dirs(CENSUS_QUESTS, CENSUS_SCRIPTS, CENSUS_JAVA, **CENSUS_HOLDERS)
		cls.world = QuestWorld(StaticData(cls.tree.root), JAVA_SRC, handlers, config)
		cls.census = census_report(cls.world)

	@classmethod
	def tearDownClass(cls):
		cls.tree.close()

	def rows(self) -> dict:
		return {row: entry["quests"] for row, entry in self.census["start"].items()}

	def test_the_handlers(self):
		c = self.census
		self.assertEqual(c["templates"], {"total": 45, "byCategory": {"QUEST": 42, "CHALLENGE_TASK": 3}})
		self.assertEqual(c["neither"], {"total": 2, "byCategory": {"QUEST": 2}}, "429 and 430: no XML template, no Java handler")
		self.assertEqual(c["xmlRestricted"], 2)

	def test_the_start_rows(self):
		self.assertEqual(self.rows(), {
			"disabled": [401],                                             # minlevel 99, although its npc talks
			"noStartNpc": [402, 403],                                      # item_order, report_on_levelup
			"talkingAi": [404, 405, 406, 409, 411, 413, 426, 427, 428],    # 406: general wins over simple_abyssguard; 413: the spot's ai
			"abyssGuardAi": [407],
			"talkingAiBlockedByJava": [408, 435],                          # 408: <finished> 106 (Java) is its only group; 435: two
			                                                               # mandatory groups, <acquired> 106 fails, <noacquired> 404 passes
			"abyssGuardAiBlockedByJava": [410],                            # <acquired> 106
			"otherAi": [412, 414],
			"serviceSpawned": [415, 416, 417, 419],
			"neverSpawned": [418, 420, 421, 422, 423, 424, 425, 431, 432, 433, 434, 436, 437, 438, 439, 440, 441, 442, 443, 444],
		})
		self.assertEqual(self.census["startTotal"], 42, "a partition of the 42 XML quests")
		self.assertEqual(self.census["start"]["otherAi"]["ais"], {"412": ["following"], "414": ["None"]}, "__NO_AI__: no ai at all")

	def test_the_spawn_kinds(self):
		service = self.census["start"]["serviceSpawned"]
		self.assertEqual(service["bySpawn"], {"siege": 1, "instance": 2, "rift": 1}, "419 counts under instance, the first of its two")
		self.assertEqual(service["inSeveralKinds"], [419])
		never = self.census["start"]["neverSpawned"]
		# 421's town npc and 438's house sign are also timed-event spawns: the town and the house come first
		self.assertEqual((never["townSpawns"]["quests"], never["townSpawns"]["atTownLevel1"]), ([421, 422], 1))
		self.assertEqual(never["houseSpawns"], {"count": 3, "quests": [436, 438, 443]}, "land 1's manager and sale sign, land 4's teleport npc "
		                 "(before its STATIC spawn)")
		self.assertEqual(never["eventSpawns"]["quests"], [423])
		self.assertEqual(never["inertSpawns"]["kinds"], {"418": ["difficulty"], "424": ["noSpots"], "425": ["notAnNpc"], "431": ["static"],
		                                                 "432": ["handler"], "433": ["noWorldMap"], "434": ["notAnNpc"]})
		# 437: land 1's teleport npc without a TELEPORT spawn; 439: a studio land's manager; 440: a house on the instance map 2
		self.assertEqual(never["noSpawnData"], {"count": 7, "quests": [420, 437, 439, 440, 441, 442, 444], "namedInJava": {
			"outsideQuestHandlers": {"count": 2, "quests": [441, 444], "where": {"441": ["instance"], "444": ["src"]}},
			"questHandlersOnly": {"count": 1, "quests": [442]},
			"nowhere": {"count": 4, "quests": [420, 437, 439, 440]}}})
		self.assertEqual(self.census["serviceOtherwiseReachable"], {"count": 2, "bySpawn": {"instance": 1, "siege": 1}, "quests": [415, 419]},
		                 "416 has a Java precondition, 417's rift npc a dummy ai")

	def test_what_the_reachable_quests_need(self):
		c = self.census
		self.assertEqual(c["reachable"]["count"], 10)
		self.assertEqual({row: entry["quests"] for row, entry in c["completion"].items()}, {
			"dialogsAndKills": [404, 405], "questLoot": [411], "crafting": [406], "itemsFromElsewhere": [413], "questObjects": [409],
			"turnIn": [407], "skillUse": [426], "pvpKills": [427], "other": [428]})
		self.assertEqual(c["dialogsAndKillsRewards"], {"bonus": 1, "ap": 1, "gp": 0, "cube": 0, "warehouse": 0, "any": 2})
		# 406's AP is in its <extended_rewards>; 414 (cube) and 415 (warehouse) are not reachable
		self.assertEqual(c["rewards"], {"xml": {"bonus": 1, "ap": 2, "gp": 1, "cube": 1, "warehouse": 1, "any": 6},
		                                "reachable": {"bonus": 1, "ap": 2, "gp": 1, "cube": 0, "warehouse": 0, "any": 4}})

	def test_the_challenge_tasks_and_the_start_zones(self):
		self.assertEqual(self.census["challengeTasks"], {"xml": 3, "byRow": {"neverSpawned": 1, "noStartNpc": 1, "talkingAi": 1}, "reachable": 1})
		self.assertEqual(self.census["startZones"], {"Poeta": {"total": 3, "xml": [404], "java": [106], "none": [429]},
		                                             "Ishalgen": {"total": 1, "xml": [], "java": [], "none": [430]}})

	def test_the_command_line(self):
		with mock.patch("m5d.quests.QuestWorld", return_value=self.world):
			code, answer, _ = run_cli(["m5d-quests", "--census"])
		self.assertEqual((code, answer["format"], answer["start"]["abyssGuardAi"]["quests"]), (0, "aion-m5d-census", [407]))


def java_hash_bucket(key: int, table: int) -> int:
	h = key & 0xFFFFFFFF
	return (h ^ (h >> 16)) & (table - 1)


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5dRegistryRealDataTest(unittest.TestCase):
	"""The registry of the 4,184 XML quests (T-04's registration-order case) and the census against m5d-plan.md §2.3-§2.5."""

	@classmethod
	def setUpClass(cls):
		cls.world = QuestWorld(StaticData(runner.DEFAULT_STATIC_DATA), JAVA_SRC, JAVA_QUEST_HANDLERS, JAVA_CONFIG)

	def test_the_whole_order(self):
		# 4,184 entries: the table doubles while the size exceeds 3/4 of it, 16 -> ... -> 8192 (3,072 < 4,184 <= 6,144), and a resize keeps
		# the insertion order inside a bucket: the order is (bucket of 8192, document position)
		report = registration_order_report(self.world, 203057)
		position = {q: i for i, q in enumerate(self.world.xml_insertion)}
		self.assertEqual(report["order"], sorted(self.world.xml_insertion, key=lambda q: (java_hash_bucket(q, 8192), position[q])))
		self.assertEqual(len(report["order"]), 4184)

	def test_the_lists_of_the_t04_case(self):
		report = registration_order_report(self.world, 203057)
		mires = report["npc"]
		self.assertEqual(mires["onTalkEvent"], [1101, 1102, 1103, 1104], "1101 ends at mires; 1102, 1103 and 1104 start there")
		self.assertEqual(mires["onTalkEventFlags"]["ascending"], True, "mires' list does not tell the HashMap order from a sorted one")
		self.assertEqual(mires["onQuestStart"], [1104, 1102, 1103], "a HashSet<>(0) of three: 4 buckets, 1104 in 0, 1102 in 2, 1103 in 3")
		# an insertion-ordered set (the C++ runtime::HashSet) would give 1102, 1103, 1104: mires' start set tells them apart
		self.assertEqual((mires["onQuestStartInsertion"], mires["onQuestStartFlags"]),
		                 ([1102, 1103, 1104], {"ascending": False, "documentOrder": False, "insertionOrder": False}))
		# the ten stigma quests (report_on_levelup, 13830-13834 and 23830-23834) and the 16 invasion quests (39005-39009, 39020-39022,
		# 49005-49009, 49020-49022); ids below 65536, so the bucket is id & 8191: 5638.., 6237.., 6252.., 7446.., 8045.., 8060..
		self.assertEqual(report["questOnEnterWorld"], [13830, 13831, 13832, 13833, 13834, 39005, 39006, 39007, 39008, 39009, 39020, 39021, 39022,
		                                               23830, 23831, 23832, 23833, 23834, 49005, 49006, 49007, 49008, 49009, 49020, 49021, 49022])
		self.assertEqual(report["questOnEnterWorldFlags"], NEITHER)
		self.assertEqual(report["questOnLevelUp"], {"ASMODIANS": [23830, 23831, 23832, 23833, 23834], "ELYOS": [13830, 13831, 13832, 13833, 13834]})

	def test_the_list_summaries(self):
		report = registration_order_report(self.world)
		# 213029 is killed for 45036 (bucket 45036 & 8191 = 4076) before 4534 (bucket 4534): neither ascending nor the document order
		self.assertEqual(report["onKillEvent"]["213029"], [45036, 4534])
		summaries = {}
		for name, neither, npc in (("talkLists", "neitherAscendingNorDocumentOrder", 203057),
		                           ("killLists", "neitherAscendingNorDocumentOrder", 213029),
		                           ("startLists", "neitherAscendingNorDocumentNorInsertionOrder", 203057)):
			s = report[name]
			summaries[name] = (s["npcs"], s["withSeveralQuests"], s["notAscending"], s["notDocumentOrder"], s.get("notInsertionOrder"),
			                   len(s[neither]), npc in s[neither])
		# mires' talk list is ascending, its start set is none of the three orders
		self.assertEqual(summaries, {"talkLists": (1989, 1077, 149, 504, None, 137, False), "killLists": (3062, 1190, 136, 174, None, 51, True),
		                             "startLists": (1441, 777, 474, 512, 447, 386, True)})

	def test_the_census(self):
		c = census_report(self.world)
		# §2.3
		self.assertEqual(c["templates"]["total"], 8043)
		self.assertEqual(c["templates"]["byCategory"], {"QUEST": 4542, "EVENT": 832, "IMPORTANT": 672, "TASK": 574, "FACTION": 364,
		                                                "MISSION": 312, "SIGNIFICANT": 215, "CHALLENGE_TASK": 174, "PUBLIC": 145,
		                                                "SEEN_MARKER": 133, "NON_COUNT": 55, "PRIMARY": 20, "LEGION": 5})
		self.assertEqual(c["neither"], {"total": 2824, "byCategory": {"QUEST": 1855, "EVENT": 376, "MISSION": 199, "FACTION": 115, "IMPORTANT": 99,
		                                                              "PUBLIC": 99, "SIGNIFICANT": 25, "NON_COUNT": 23, "CHALLENGE_TASK": 18,
		                                                              "SEEN_MARKER": 10, "LEGION": 3, "PRIMARY": 2}})
		self.assertEqual(c["xmlRestricted"], 1845)
		# §2.4, the start rows: 2,072 / 439 / 307 (270 + 37) / 322 / 498 / 415 / 126 / 5
		self.assertEqual({row: entry["count"] for row, entry in c["start"].items()}, {
			"disabled": 126, "noStartNpc": 415, "talkingAi": 2072, "abyssGuardAi": 439, "talkingAiBlockedByJava": 270,
			"abyssGuardAiBlockedByJava": 37, "otherAi": 5, "serviceSpawned": 322, "neverSpawned": 498})
		self.assertEqual(c["start"]["serviceSpawned"]["bySpawn"], {"siege": 185, "instance": 91, "base": 41, "vortex": 3, "ahserion": 2})
		self.assertEqual((c["reachable"]["count"], c["serviceOtherwiseReachable"]["count"]), (2511, 310))
		self.assertEqual(c["serviceOtherwiseReachable"]["bySpawn"], {"siege": 182, "instance": 82, "base": 41, "vortex": 3, "ahserion": 2})
		# rev 2 did not split the 498: TownService spawns 54 of their givers at startup, at a town level above 1, and the houses 2 (the
		# butlers of 18829 and 28829); 202 are in no spawn data, 24 of them named by instance or ai handlers (this census's finding)
		never = c["start"]["neverSpawned"]
		self.assertEqual((never["townSpawns"]["count"], never["townSpawns"]["atTownLevel1"], never["houseSpawns"]["quests"],
		                  never["eventSpawns"]["count"], never["inertSpawns"]["count"], never["noSpawnData"]["count"]),
		                 (54, 0, [18829, 28829], 240, 0, 202))
		named = never["noSpawnData"]["namedInJava"]
		self.assertEqual((named["outsideQuestHandlers"]["count"], named["questHandlersOnly"]["count"], named["nowhere"]["count"]), (24, 5, 173))
		self.assertEqual({q: named["outsideQuestHandlers"]["where"][str(q)] for q in (16941, 16991, 18738, 26979, 30225, 30507, 30719)},
		                 {16941: ["instance", "quest"], 16991: ["instance"], 18738: ["instance"], 26979: ["instance"], 30225: ["instance"],
		                  30507: ["instance"], 30719: ["ai", "instance", "quest"]})
		# §2.4, what completing the 2,511 needs
		completion = {row: (entry["count"], entry["byKind"]) for row, entry in c["completion"].items()}
		self.assertEqual(completion["dialogsAndKills"], (901, {"monster_hunt": 609, "report_to": 272, "report_to_many": 19, "xml_quest": 1}))
		self.assertEqual({row: count for row, (count, _) in completion.items()}, {
			"dialogsAndKills": 901, "questLoot": 505, "crafting": 586, "itemsFromElsewhere": 316, "questObjects": 135, "turnIn": 30,
			"skillUse": 24, "pvpKills": 14, "other": 0})
		self.assertEqual(completion["crafting"][1], {"work_order": 574, "crafting_rewards": 12})
		self.assertEqual(completion["turnIn"][1], {"relic_rewards": 26, "fountain_rewards": 4})
		self.assertEqual(completion["pvpKills"][1], {"kill_in_zone": 12, "kill_in_world": 2})
		self.assertEqual(c["dialogsAndKillsRewards"], {"bonus": 34, "ap": 82, "gp": 2, "cube": 1, "warehouse": 0, "any": 106})
		self.assertEqual(c["rewards"], {"xml": {"bonus": 760, "ap": 363, "gp": 57, "cube": 2, "warehouse": 0, "any": 1115},
		                                "reachable": {"bonus": 687, "ap": 169, "gp": 14, "cube": 1, "warehouse": 0, "any": 844}})
		self.assertEqual(c["challengeTasks"], {"xml": 156, "byRow": {"noStartNpc": 106, "neverSpawned": 50}, "reachable": 0})
		# §2.5
		self.assertEqual({zone: (z["total"], len(z["xml"]), len(z["java"]), len(z["none"])) for zone, z in c["startZones"].items()},
		                 {"Poeta": (48, 28, 13, 7), "Ishalgen": (52, 28, 17, 7)})
		self.assertEqual(c["startZones"]["Poeta"]["none"], [1128, 9612, 9613, 9614, 80617, 80621, 80643])
		self.assertEqual(c["startZones"]["Ishalgen"]["none"], [2111, 2130, 2150, 2151, 80619, 80622, 80644])


if __name__ == "__main__":
	unittest.main()
