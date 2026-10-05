"""M5e progression oracle (m5e/, m5e-plan.md G-01): the charge selection and the isNew walk on hand-made data, then the gate's paths on the
real data and the Java sources - the Elyos Warrior from creation to the first Daeva level (m5e-plan.md §2.1, §2.3, X2, X5, X7), the class
change's pages and actions, the class masters, a skill book, the gate's casts and weapons.

Expected values are derived by hand from the Java sources named in m5e/progression.py and repeated per case; the real-data ones are the
numbers m5e-plan.md §2.1 and §2.3 measured with a separate model (m5e_isnew.py). Everything that reads the Java source tree is skipped without
it, like the M5d tests.
"""

import contextlib
import io
import json
import tempfile
import unittest
from pathlib import Path

from m5a.data import StaticData
from m5e.progression import Character, LearnRow, ProgressionData, SkillInfo, charged_skill, progression_report
from m5e.stumble import load_scene, stumble_end
from staticdata_oracle import OracleError
from staticdata_oracle import run as runner

from geo import run as geo_run
from geo.javamath import add, f32, sub

import oracle

JAVA_SRC = runner.TOOL_DIR.parents[2] / "game-server" / "src"
JAVA_QUEST_HANDLERS = runner.TOOL_DIR.parents[2] / "game-server" / "data" / "handlers" / "quest"
HAVE_JAVA_TREE = (JAVA_SRC / "com" / "aionemu" / "gameserver").is_dir() and runner.DEFAULT_STATIC_DATA.is_dir()


class M5eChargeTest(unittest.TestCase):
	"""CreatureController.useChargeSkill's loop (CreatureController.java:466-492) on charge 108's shape"""

	ENTRY = {"minTime": 400, "skills": [{"time": 1100, "skillId": 2606}, {"time": 1700, "skillId": 2607}, {"time": 5000, "skillId": 2608}]}

	def test_thresholds(self):
		self.assertIsNone(charged_skill(self.ENTRY, 399), "below min_time: refused with an audit line")
		self.assertEqual(charged_skill(self.ENTRY, 400), 2606)
		self.assertEqual(charged_skill(self.ENTRY, 1100), 2606, "chargeTimeSum >= chargeTimeMillis breaks at the first skill")
		self.assertEqual(charged_skill(self.ENTRY, 1101), 2607)
		self.assertEqual(charged_skill(self.ENTRY, 2800), 2607)
		self.assertEqual(charged_skill(self.ENTRY, 2801), 2608, "++index == size - 1 stops the loop at the last skill")
		self.assertEqual(charged_skill(self.ENTRY, 60000), 2608)

	def test_cast_speed_factor_scales_every_time(self):
		self.assertIsNone(charged_skill(self.ENTRY, 700, 2.0), "min_time 400 * 2")
		self.assertEqual(charged_skill(self.ENTRY, 2200, 2.0), 2606)
		self.assertEqual(charged_skill(self.ENTRY, 2201, 2.0), 2607)


def _data(rows: list[LearnRow], skills: list[SkillInfo]) -> ProgressionData:
	"""a ProgressionData without static data: the rows in document order and the templates"""
	data = ProgressionData.__new__(ProgressionData)
	data.rows = rows
	data.skills = {s.skill_id: s for s in skills}
	data.by_stack = {}
	for s in skills:
		if s.stack is not None:
			data.by_stack.setdefault(s.stack, []).append(s)
	data.experience = [0, 400, 1433]
	return data


class M5eSkillTreeWalkTest(unittest.TestCase):
	"""SkillTreeData.getSkillsForSkill / getHighestSkill / createSkillTree (SkillTreeData.java:94-162) on hand-made rows"""

	def setUp(self):
		# a stack A1 (lvl 1) <- A2 (lvl 2) <- A3 (lvl 3); A3, the top, exists only for GLADIATOR (the WA_ROBUSTHIT shape of m5e-plan.md §2.1)
		self.rows = [LearnRow("WARRIOR", 1, None, "PC_ALL", 3, True, 0), LearnRow("WARRIOR", 2, 1, "PC_ALL", 6, True, 0),
		             LearnRow("GLADIATOR", 3, 2, "PC_ALL", 10, True, 0), LearnRow("GLADIATOR", 2, 1, "PC_ALL", 6, True, 0),
		             LearnRow("GLADIATOR", 1, None, "PC_ALL", 3, True, 0)]
		self.skills = [SkillInfo(1, "a", 1, "A", "ACTIVE", "NONE"), SkillInfo(2, "a", 2, "A", "ACTIVE", "NONE"),
		               SkillInfo(3, "a", 3, "A", "ACTIVE", "NONE")]
		self.data = _data(self.rows, self.skills)

	def test_the_walk_starts_at_the_top_of_the_stack_for_the_players_class(self):
		self.assertEqual(self.data.highest_skill(1), 3)
		self.assertEqual(self.data.skills_for_skill(2, "WARRIOR", "ELYOS", -1), [], "the top 3 has no WARRIOR row: the tree is empty")
		tree = self.data.skills_for_skill(2, "GLADIATOR", "ELYOS", -1)
		self.assertEqual([t.skill_id for t in tree], [1, 2, 3], "createSkillTree inserts at 0: ascending")
		self.assertEqual([t.skill_id for t in self.data.skills_for_skill(2, "GLADIATOR", "ELYOS", 6)], [1, 2], "minLevel > level removed")

	def test_equal_levels_keep_the_first_of_the_stack(self):
		data = _data(self.rows, [SkillInfo(1, "a", 2, "A", "ACTIVE", "NONE"), SkillInfo(2, "a", 2, "A", "ACTIVE", "NONE")])
		self.assertEqual(data.highest_skill(2), 1, "Stream.max with equal levels keeps the first")
		self.assertEqual(data.highest_skill(99), 99, "no template: the id itself")

	def test_a_class_less_top_finds_only_class_less_pre_skills(self):
		rows = [LearnRow(None, 2, 1, "PC_ALL", 5, True, 0), LearnRow("WARRIOR", 1, None, "PC_ALL", 1, True, 0)]
		data = _data(rows, [SkillInfo(1, "a", 1, "B", "ACTIVE", "NONE"), SkillInfo(2, "a", 2, "B", "ACTIVE", "NONE")])
		self.assertEqual([t.skill_id for t in data.skills_for_skill(2, "WARRIOR", "ELYOS", -1)], [2],
		                 "getTemplatesForSkill(learn, null class, PC_ALL) skips the WARRIOR row")

	def test_a_stigma_top_skips_a_non_stigma_pre_skill(self):
		rows = [LearnRow("WARRIOR", 2, 1, "PC_ALL", 5, False, 1), LearnRow("WARRIOR", 1, None, "PC_ALL", 1, True, 0)]
		data = _data(rows, [SkillInfo(1, "a", 1, "C", "ACTIVE", "NONE"), SkillInfo(2, "a", 2, "C", "ACTIVE", "NONE")])
		self.assertEqual([t.skill_id for t in data.skills_for_skill(2, "WARRIOR", "ELYOS", -1)], [2])


@unittest.skipUnless(HAVE_JAVA_TREE, "needs game-server/src and the static data")
class M5eRealDataTest(unittest.TestCase):

	@classmethod
	def setUpClass(cls):
		cls.data = StaticData(runner.DEFAULT_STATIC_DATA)
		cls.warrior = progression_report(cls.data, JAVA_SRC, JAVA_QUEST_HANDLERS, "ELYOS", "WARRIOR", 1,
		                                 ["create", "enter:1", "level:2", "quit", "enter:8", "level:9", "class:SORCERER", "class:GLADIATOR",
		                                  "class:TEMPLAR", "level:10", "quit", "enter:15"],
		                                 None, False, [769, 758, 519, 2981, 1699, 1809, 2606], [100000094, 100900038, 102100181],
		                                 {"dp": 2000, "robot": False})

	def step(self, name: str) -> dict:
		return next(s for s in self.warrior["steps"] if s["step"] == name)

	@staticmethod
	def adds(step: dict) -> dict[int, int | None]:
		return {e["skillId"]: e["messageId"] for e in step["events"] if e["op"] == "add"}

	def test_experience_and_the_cap(self):
		start = self.warrior["experience"]["startExp"]
		self.assertEqual([start[str(lvl)] for lvl in (1, 2, 9, 10, 15, 20)], [0, 400, 82982, 126069, 649169, 2314771])
		self.assertEqual(self.warrior["experience"]["nonDaevaCap"], {"level": 9, "exp": 126069})

	def test_the_offline_levels_and_level_9(self):
		self.assertEqual(self.step("level:2")["events"], [], "a Warrior learns nothing at 2")
		enter = self.step("enter:8")
		self.assertEqual(set(self.adds(enter)), {2877, 139, 2890, 2865, 2903, 2878}, "m5e-plan.md §2.1: levels 3-8")
		self.assertTrue(all(m is None for m in self.adds(enter).values()), "before the spawn: no SM_SKILL_LIST")
		self.assertEqual(self.adds(self.step("level:9")), {138: 1300050}, "X2: Boost Parry I, new")

	def test_the_class_change(self):
		self.assertEqual(self.warrior["classChange"]["pageId"], 2375)
		self.assertEqual(self.warrior["classChange"]["questId"], 1006)
		wrong = self.step("class:SORCERER")
		self.assertEqual((wrong["dialogActionId"], wrong["accepted"], wrong["message"]), (3058, False, "Invalid class chosen"))
		change = self.step("class:GLADIATOR")
		self.assertEqual((change["dialogActionId"], change["accepted"], change["classId"], change["daeva"]), (2376, True, 1, True))
		self.assertEqual(self.adds(change), {44: 0, 45: 1300050, 46: 0, 48: 0, 49: 0, 50: 0, 51: 1300050, 52: 1300050, 53: 1300050,
		                                     54: 1300050}, "X5, m5e-plan.md §2.3")
		self.assertEqual(change["baseStats"]["maxHp"], 814, "GLADIATOR at 9: the class template's base max HP")
		again = self.step("class:TEMPLAR")
		self.assertEqual((again["dialogActionId"], again["accepted"], again["message"]), (2461, False, "You already switched class"))
		valid = {c["dialogActionId"]: c["validFor"] for c in self.warrior["classChange"]["choices"]}
		self.assertEqual((valid[2376], valid[2461], valid[3058]), ("WARRIOR", "WARRIOR", None))

	def test_the_first_daeva_level(self):
		level = self.step("level:10")
		self.assertEqual(self.adds(level), {30003: 1330004, 40009: 1330061, 169: 0, 246: 1300050, 249: 1300050, 348: 1300050, 519: 1300050,
		                                    758: 1300050, 769: 1300050, 2891: 0, 2981: 1300050, 30002: 1330004}, "X7")
		removed = [e["skillId"] for e in level["events"] if e["op"] == "remove"]
		self.assertEqual(removed, [30001], "the 30001 -> 30002 swap of a Daeva")
		self.assertEqual(level["events"][-1]["op"], "remove", "removeSkill after the 30002 add")

	def test_the_level_15_passive(self):
		self.assertIn(563, self.adds(self.step("enter:15")), "X10: Determination at 15")

	def test_the_class_master(self):
		self.assertEqual({k: self.warrior["trainer"][k] for k in ("questId", "npcId", "page", "questVar", "rewardGroup")},
		                 {"questId": 1205, "npcId": 203087, "page": 1011, "questVar": 1, "rewardGroup": 0})

	def test_the_casts(self):
		casts = {s["skillId"]: s for s in self.warrior["skills"]}
		self.assertFalse(casts[769]["accepted"], "the Training Sword is a SWORD: 769 wants a greatsword or a polearm")
		self.assertEqual(casts[758]["chain"] if "chain" in casts[758] else None, None)
		self.assertTrue(casts[519]["accepted"], "519 accepts a SWORD with 2,000 DP")
		self.assertEqual(casts[519]["costs"]["dpuse"]["value"], 2000)
		self.assertEqual([l["skillId"] for l in casts[519]["launches"]], [8218])
		self.assertEqual([l["skillId"] for l in casts[1699]["launches"]], [8296])
		self.assertEqual([l["skillId"] for l in casts[1809]["launches"]], [8998])
		self.assertEqual(self.warrior["launched"]["8998"]["effects"][0]["duration2"], 6500)
		self.assertEqual(self.warrior["launched"]["8998"]["tslot"], "CHANT")
		self.assertEqual(casts[1809]["tslot"], "NOSHOW")
		self.assertFalse(casts[2606]["accepted"], "no robot: RideRobotCondition refuses")
		self.assertEqual([s["skillId"] for s in casts[2606]["charge"]["skills"]], [2606, 2607, 2608])

	def test_the_weapons(self):
		weapons = {w["itemId"]: w for w in self.warrior["weapons"]}
		self.assertEqual(weapons[100000094]["itemGroup"], "SWORD")
		self.assertEqual(weapons[100900038]["itemGroup"], "GREATSWORD")
		self.assertEqual(weapons[100900038]["requiredSkills"], [51])
		self.assertTrue(weapons[100900038]["equipSkillKnown"], "51 comes with the class change")
		self.assertEqual((weapons[102100181]["itemGroup"], weapons[102100181]["robotId"]), ("KEYBLADE", 2500002))

	def test_the_chain_follower_after_its_opener(self):
		report = progression_report(self.data, JAVA_SRC, JAVA_QUEST_HANDLERS, "ELYOS", "GLADIATOR", 10, [], None, True, [758],
		                            [100900038], {"dp": 0, "robot": False, "chainAfter": "W_CHAINC_1TH_1"})
		cast = report["skills"][0]
		self.assertTrue(cast["accepted"], cast["refusals"])
		self.assertEqual(cast["chain"], {"category": "W_CHAINC_2TH_1", "precategory": "W_CHAINC_1TH_1", "time": 3000})

	def test_a_skill_book(self):
		report = progression_report(self.data, JAVA_SRC, JAVA_QUEST_HANDLERS, "ELYOS", "MAGE", 1,
		                            ["create", "quit", "enter:9", "class:SORCERER", "level:10", "book:169500932", "book:169500932"], None, False,
		                            [], [], {"dp": 0})
		first, second = report["steps"][-2], report["steps"][-1]
		self.assertEqual((first["canAct"], first["book"]["skillId"]), (True, 18))
		self.assertEqual([(e["skillId"], e["messageId"]) for e in first["events"]], [(18, 1300050)], "X16")
		self.assertEqual((second["canAct"], second["refusal"], second["events"]), (False, "known", []))

	def test_the_asmodian_tables(self):
		report = progression_report(self.data, JAVA_SRC, JAVA_QUEST_HANDLERS, "ASMODIANS", "WARRIOR", 1, [], None, False, [], [], {})
		self.assertEqual(report["classChange"]["pageId"], 3057)
		self.assertEqual(report["classChange"]["questId"], 2008)
		valid = {c["dialogActionId"]: c["playerClass"] for c in report["classChange"]["choices"]}
		self.assertEqual((valid[3058], valid[3143]), ("GLADIATOR", "TEMPLAR"), "the Asmodian actions are shifted by one page")
		self.assertEqual((report["trainer"]["questId"], report["trainer"]["npcId"]), (2132, 203527))

	def test_known_skills_seed_the_list(self):
		with tempfile.TemporaryDirectory() as tmp:
			path = Path(tmp) / "known.txt"
			path.write_text("2890:1 140:1", encoding="utf-8")
			report = progression_report(self.data, JAVA_SRC, JAVA_QUEST_HANDLERS, "ELYOS", "GLADIATOR", 9, ["enter:9", "level:10"], path, True,
			                            [], [], {})
		adds = {e["skillId"]: e["messageId"] for e in report["steps"][1]["events"] if e["op"] == "add"}
		self.assertEqual((adds[169], adds[2891]), (0, 0), "the walks reach the known 140 and 2890")

	def test_the_command_line(self):
		out = io.StringIO()
		with contextlib.redirect_stdout(out):
			code = oracle.main(["m5e-progression", "--race", "ELYOS", "--class", "WARRIOR", "--step", "create", "quit", "enter:8"])
		self.assertEqual(code, 0)
		self.assertEqual(json.loads(out.getvalue())["format"], "aion-m5e-progression")
		with contextlib.redirect_stderr(io.StringIO()):
			self.assertEqual(oracle.main(["m5e-progression", "--race", "ELYOS", "--class", "WARRIOR", "--step", "level:2"]), 2,
			                 "an online level change of a character that is not in the world is refused")

	def test_a_seeded_daeva_learns_at_its_enter_world(self):
		report = progression_report(self.data, JAVA_SRC, JAVA_QUEST_HANDLERS, "ELYOS", "PRIEST", 1,
		                            ["create", "enter:1", "quit", "seed:CLERIC", "enter:10"], None, False, [], [], {})
		enter = report["steps"][-1]
		self.assertEqual((enter["playerClass"], enter["daeva"], enter["level"]), ("CLERIC", True, 10))
		self.assertIn(1699, {s["skillId"] for s in enter["skills"]}, "Light of Resurrection, a Cleric's level-10 skill")
		self.assertNotIn(30001, {s["skillId"] for s in enter["skills"]}, "a Daeva's 30001 became 30002")
		self.assertIn(30002, {s["skillId"] for s in enter["skills"]})
		with self.assertRaises(OracleError):
			progression_report(self.data, JAVA_SRC, JAVA_QUEST_HANDLERS, "ELYOS", "PRIEST", 1, ["create", "enter:1", "seed:CLERIC"], None,
			                   False, [], [], {})

	def test_a_stigma_stone(self):
		# Crippling Cut, a level-20 Gladiator stone (RARE): StigmaService.notifyEquipAction's 25,000 and addStigmaSkills' new stigma skill
		report = progression_report(self.data, JAVA_SRC, JAVA_QUEST_HANDLERS, "ELYOS", "GLADIATOR", 20, [], None, True, [], [], {},
		                            [140001109], {"globalPrices": 100, "globalPricesModifier": 100, "taxes": 100})
		stone = report["stigmas"][0]
		self.assertEqual((stone["quality"], stone["basePrice"], stone["price"]), ("RARE", 25000, 25000))
		self.assertEqual(len(stone["events"]), 1)
		self.assertEqual(stone["events"][0]["messageId"], 1300401, "a new stigma skill (SkillLearnService.java:52)")
		self.assertEqual(stone["events"][0]["level"], 1, "the stone's enchant level + 1")
		with self.assertRaises(OracleError):
			progression_report(self.data, JAVA_SRC, JAVA_QUEST_HANDLERS, "ELYOS", "GLADIATOR", 20, [], None, True, [], [], {}, [100900038])

	def test_a_new_characters_old_level_is_0(self):
		# PlayerService.newPlayer writes no old_level, so the column's default 0 (aion_gs.sql:911) is what the first enter world reads
		# (PlayerEnterWorldService.java:204): onLevelChange(0, 1), which learns level 1 again and adds nothing
		create, enter = self.warrior["steps"][0], self.warrior["steps"][1]
		self.assertEqual(enter["step"], "enter:1")
		self.assertEqual(enter["levelChange"], [0, 1])
		self.assertEqual(enter["events"], [], "level 1's skills are known since create")
		self.assertEqual(enter["skills"], create["skills"])

	def test_an_online_level_change_caps_a_non_daeva_at_9(self):
		# PlayerCommonData.setExp online: maxLevel 10 for a non-Daeva, level = min(levelForExp, maxLevel - 1) (PlayerCommonData.java:276, 281)
		report = progression_report(self.data, JAVA_SRC, JAVA_QUEST_HANDLERS, "ELYOS", "WARRIOR", 1, ["create", "enter:8", "level:12"], None,
		                            False, [], [], {})
		capped = report["steps"][-1]
		self.assertEqual((capped["level"], capped["levelChange"], capped["levelCapped"], capped["daeva"]), (9, [8, 9], 12, False))
		self.assertEqual({e["skillId"] for e in capped["events"] if e["op"] == "add"}, {138}, "only level 9's skill: nothing of 10-12")
		stays = progression_report(self.data, JAVA_SRC, JAVA_QUEST_HANDLERS, "ELYOS", "WARRIOR", 1, ["create", "enter:9", "level:10"], None,
		                           False, [], [], {})["steps"][-1]
		self.assertEqual((stays["level"], stays["levelChange"], stays["events"]), (9, [9, 9], []), "the wall: a level-9 Warrior stays 9")
		# a Daeva is not capped: the warrior's level:10 after its class change (test_the_first_daeva_level)
		self.assertEqual(self.step("level:10")["level"], 10)
		self.assertNotIn("levelCapped", self.step("level:10"))

	def test_unknown_step(self):
		with self.assertRaises(OracleError):
			progression_report(self.data, JAVA_SRC, JAVA_QUEST_HANDLERS, "ELYOS", "WARRIOR", 1, ["fly"], None, False, [], [], {})


@unittest.skipUnless(geo_run.DEFAULT_GEO_DIR.is_dir() and geo_run.DEFAULT_WORLD_MAPS.is_file(), "the Java tree's geo data is not present")
class M5eStumbleTest(unittest.TestCase):
	"""m5e-stumble (X9g): GeoMap.getClosestCollision's end on open ground, from the Poeta geo data"""

	@classmethod
	def setUpClass(cls):
		cls.scene = load_scene(geo_run.DEFAULT_GEO_DIR, geo_run.DEFAULT_WORLD_MAPS, 210010000)

	def test_open_ground_gives_getz_at_the_end(self):
		# kunandes's spot (Poeta spawn file), 2 m east: terrain only; the end's z is getZ(x, y, z + 1, z - 2) (GeoMap.java:142-147)
		end = stumble_end(self.scene, 840.494, 1217.09, 119.068, 842.494, 1217.09)
		self.assertTrue(end["open"], end["reason"])
		self.assertEqual(end["groundSource"], "terrain")
		self.assertEqual(end["groundZ"], self.scene.get_z(f32(842.494), f32(1217.09), add(f32(119.068), 1.0), sub(f32(119.068), 2.0))[0])
		self.assertNotEqual(end["groundZ"], f32(119.068), "the terrain is not flat there: a port that ignores geo keeps the start z")

	def test_a_mesh_on_the_segment_is_not_open(self):
		# a building of Akarios village: the ray at z + 1 hits it 1.68 m from its origin
		end = stumble_end(self.scene, 830.0, 1210.0, 119.0, 832.0, 1210.0)
		self.assertFalse(end["open"])
		self.assertIsNone(end["groundZ"])
		self.assertIn("geometry crosses the ray", end["reason"])

	def test_the_command_line(self):
		out = io.StringIO()
		with contextlib.redirect_stdout(out):
			code = oracle.main(["m5e-stumble", "--map", "210010000", "--stumble", "840.494,1217.09,119.068,842.494,1217.09"])
		self.assertEqual(code, 0)
		answer = json.loads(out.getvalue())
		self.assertEqual((answer["format"], len(answer["stumbles"]), answer["stumbles"][0]["open"]), ("aion-m5e-stumble", 1, True))
		with contextlib.redirect_stderr(io.StringIO()):
			self.assertEqual(oracle.main(["m5e-stumble", "--map", "210010000", "--stumble", "1,2,3"]), 2, "not FX,FY,FZ,TX,TY")


if __name__ == "__main__":
	unittest.main()
