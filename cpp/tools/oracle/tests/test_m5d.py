"""M5d quest oracle (m5d/, m5d-plan.md G-01): the Java readers and orders alone, the template simulations alone, the whole quest and map
reports on a small static_data tree (with a fixture Java handler and a fixture config), and the gate's quests 1101, 1102, 1103, 2101, 2102
with the Poeta and Ishalgen marker sets on the real data.

Expected values are derived by hand from the Java sources named in m5d/*.py and repeated per case. Everything that reads the Java source tree is
skipped without it, like M5b2FixtureReportTest.
"""

import contextlib
import io
import json
import shutil
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from m5a.data import StaticData
from m5a.spawns import GameClock
from m5d.javasrc import (DialogTables, JaxbModel, check_registration_order, float_array, hash_bucket_groups, hash_iteration_order,
                         java_enum_constants, java_integer, parse_class, read_config, scan_java_handlers, strip_comments)
from m5d.quests import (QuestRules, QuestWorld, apply_quest_list, check_start_conditions, is_acceptable_quest, make_character, map_report,
                        quest_report, rewards_report)
from m5d.templates import (Flow, Monster, SimState, kill_spawned_kill, monster_hunt_kill, ranked_kill, simulate_monster_hunt, skill_use_event)
from staticdata_oracle import OracleError
from staticdata_oracle import run as runner

import oracle

from .support import Tree

JAVA_SRC = runner.TOOL_DIR.parents[2] / "game-server" / "src"
JAVA_QUEST_HANDLERS = runner.TOOL_DIR.parents[2] / "game-server" / "data" / "handlers" / "quest"
JAVA_CONFIG = runner.TOOL_DIR.parents[2] / "game-server" / "config"
HAVE_JAVA_TREE = (JAVA_SRC / "com" / "aionemu" / "gameserver").is_dir() and runner.DEFAULT_STATIC_DATA.is_dir()


class _Skill(dict):
	"""A QuestSkillData as SkillUse reads it (JObj-like item access)."""


def skill(ids, end_var, var_num=0):
	return _Skill(skillIds=ids, endVar=end_var, varNum=var_num)


class M5dSimulationTest(unittest.TestCase):
	"""The template handlers' var arithmetic on a simulated QuestState, without any Java source or static data."""

	def test_monster_hunt_counts_in_six_bit_vars(self):
		# MonsterHunt.onKillEvent (MonsterHunt.java:174-239), old style: the kill total of var 1 spills into var 2 above 63
		monsters = [Monster([1, 2], 0, 3), Monster([3], 1, 70)]
		state = SimState()
		for _ in range(3):
			self.assertTrue(monster_hunt_kill(state, monsters, 2, False, False, False, False))
		self.assertEqual((state.vars[0], state.updates), (3, 3))
		self.assertFalse(monster_hunt_kill(state, monsters, 1, False, False, False, False), "total 4 > 3: no update")
		for _ in range(64):
			monster_hunt_kill(state, monsters, 3, False, False, False, False)
		self.assertEqual(state.vars[:3], [3, 0, 1], "64 kills: var1 = 64 & 0x3F, var2 = 64 >> 6")
		for _ in range(6):
			monster_hunt_kill(state, monsters, 3, False, False, False, False)
		self.assertEqual(state.vars[:3], [3, 6, 1])
		self.assertFalse(monster_hunt_kill(state, monsters, 3, False, False, False, False), "71 > 70")
		self.assertEqual(state.status, "START", "old style without end_reward: only the end npc sets REWARD")

	def test_monster_hunt_end_reward_and_next_step(self):
		for reward, next_step, var0 in ((True, False, 2), (False, True, 3)):
			state = SimState()
			monster_hunt_kill(state, [Monster([1], 0, 2)], 1, False, False, reward, next_step)
			self.assertEqual(state.status, "START")
			monster_hunt_kill(state, [Monster([1], 0, 2)], 1, False, False, reward, next_step)
			self.assertEqual((state.status, state.vars[0], state.updates), ("REWARD", var0, 3),
			                 "the last kill updates the var, then REWARD (end_reward_next_step also adds 1 to var0)")

	def test_data_driven_monster_hunt_resets_the_vars(self):
		# the totals of the current step reach the end: setQuestVar(curStep + 1) clears the kill vars, REWARD at the last step
		monsters = [Monster([4], 1, 2), Monster([5], 2, 1)]
		state = SimState()
		self.assertFalse(monster_hunt_kill(state, monsters, 4, True, False, False, False), "Java answers false although var1 changed")
		self.assertEqual((state.vars[1], state.updates), (1, 1))
		monster_hunt_kill(state, monsters, 4, True, False, False, False)
		self.assertTrue(monster_hunt_kill(state, monsters, 5, True, False, False, False))
		self.assertEqual((state.status, state.vars, state.updates, state.nearby), ("REWARD", [1, 0, 0, 0, 0, 0], 4, 1))
		rows = simulate_monster_hunt(monsters, True, False, False, False)
		self.assertEqual([r["handled"] for r in rows], [False, False, False, True], "two kills, one extra, the completing kill")

	def test_aggro_monster_hunt_rewards_at_the_first_kill(self):
		state = SimState()
		monster_hunt_kill(state, [Monster([1], 0, 5)], 1, False, True, False, False)
		self.assertEqual((state.status, state.vars[0]), ("REWARD", 0))

	def test_a_var_beyond_the_sixth_is_refused(self):
		with self.assertRaises(OracleError):  # Java: ArrayIndexOutOfBoundsException in QuestVars
			monster_hunt_kill(SimState(), [Monster([1], 5, 100)], 1, False, False, False, False)

	def test_kill_spawned_skill_use_and_ranked_kills(self):
		state = SimState()
		monsters = [Monster([7], 0, 1), Monster([8], 1, 2)]
		kill_spawned_kill(state, monsters, 7)
		kill_spawned_kill(state, monsters, 8)
		self.assertEqual(state.status, "START")
		kill_spawned_kill(state, monsters, 8)
		self.assertEqual((state.status, state.vars[:2]), ("REWARD", [1, 2]))
		state = SimState()
		for _ in range(3):
			skill_use_event(state, [skill([10], 3)], 10)
		self.assertEqual((state.status, state.vars[0]), ("REWARD", 3))
		state = SimState()
		skill_use_event(state, [skill([10], 1), skill([11], 1, 1)], 10)
		skill_use_event(state, [skill([10], 1), skill([11], 1, 1)], 11)
		self.assertEqual(state.status, "START", "SkillUse counts the finished skill lists of ONE event: two lists never reach REWARD")
		state = SimState()
		self.assertTrue(ranked_kill(state, 0, 2, True, False))
		self.assertEqual((state.status, state.vars[0]), ("START", 1))
		ranked_kill(state, 0, 2, True, False)
		self.assertEqual((state.status, state.vars[0]), ("REWARD", 1), "defaultOnKillRankedEvent: endVar - 1 is the last var")
		state = SimState()
		ranked_kill(state, 0, 2, True, True)
		ranked_kill(state, 0, 2, True, True)
		self.assertEqual((state.status, state.vars[:2]), ("REWARD", [1, 0]), "data driven: var1 counts, then setQuestVar(var0 + 1)")


class M5dJavaOrderTest(unittest.TestCase):
	"""java.util.HashMap iteration order and the Java text readers, without the Java tree."""

	def test_hash_iteration_order(self):
		self.assertEqual(hash_iteration_order([1104, 1102, 1103], initial_capacity=0), [1104, 1102, 1103],
		                 "a HashSet<>(0) of three: capacity 4, buckets 0, 2, 3")
		self.assertEqual(hash_iteration_order([17, 1, 33]), [17, 1, 33], "one bucket of a 16 table keeps the insertion order")
		self.assertEqual(hash_iteration_order([16, 0] + list(range(1, 12))), list(range(12)) + [16], "the 13th key doubles the table")
		self.assertEqual(hash_iteration_order([5, 65541]), [65541, 5], "h ^ (h >>> 16): 65541 lands in bucket 4")
		self.assertEqual(hash_iteration_order([3, 3, 1]), [1, 3], "a key put twice keeps its place")
		with self.assertRaises(OracleError):  # the 11th key of one bucket in a 64 table would make a tree bin
			hash_iteration_order([k * 1024 for k in range(20)])
		# putVal: the 9th key of a bucket calls treeifyBin, which resizes a table below 64 (16 -> 32, then 64 at the 10th); the 11th treeifies
		self.assertEqual(hash_iteration_order([k * 64 for k in range(10)]), [k * 64 for k in range(10)])
		with self.assertRaises(OracleError):
			hash_iteration_order([k * 64 for k in range(11)])

	def test_hash_bucket_groups(self):
		# a new HashMap<>() of 6 ids stays at 16 buckets: 1105 -> 1, 1108 -> 4, 1109 -> 5, 1127 -> 7, 1112 -> 8, 1101 -> 13
		self.assertEqual(hash_bucket_groups([1101, 1105, 1108, 1109, 1112, 1127]), [[1105], [1108], [1109], [1127], [1112], [1101]])
		self.assertEqual(hash_bucket_groups([2133, 2101, 2112]), [[2112], [2101, 2133]], "2101 and 2133 share bucket 5")
		self.assertEqual(hash_bucket_groups([16, 0] + list(range(1, 12))), [[k] for k in range(12)] + [[16]],
		                 "13 keys: 32 buckets, where 0 and 16 part")
		self.assertEqual(hash_bucket_groups([16, 0] + list(range(1, 11))), [[0, 16]] + [[k] for k in range(1, 11)], "12 keys: 16 buckets")
		self.assertEqual(hash_bucket_groups([k * 64 for k in range(8)]), [[k * 64 for k in range(8)]])
		self.assertIsNone(hash_bucket_groups([k * 64 for k in range(9)]), "a ninth key in one bucket: treeifyBin's resize")

	def test_java_text(self):
		text = 'int a = 1; // x = 2\nString s = "// not a comment"; /* int b = 3;\n */ char c = \'"\';'
		stripped = strip_comments(text)
		self.assertEqual(stripped.count("\n"), 2)
		self.assertIn('"// not a comment"', stripped)
		self.assertNotIn("x = 2", stripped)
		self.assertNotIn("int b", stripped)
		with tempfile.TemporaryDirectory() as root:
			path = Path(root) / "Sample.java"
			path.write_text("@XmlAccessorType(XmlAccessType.FIELD)\npublic class Sample extends Base {\n\t@XmlAttribute(name = \"a_b\", "
			                "required = true)\n\tprivate int ab = 3;\n\tprivate static final int C = 4;\n\tprotected List<Integer> list;\n\t"
			                "public void m() { int local = 5; }\n\tpublic abstract int n();\n}\nenum Target {\n\tONE,\n\tTWO\n}\n",
			                encoding="utf-8")
			cls = parse_class(path)
			self.assertEqual((cls.extends, cls.accessor_type), ("Base", "FIELD"))
			self.assertEqual([(f.name, f.type, f.initializer) for f in cls.fields], [("ab", "int", "3"), ("C", "int", "4"), ("list", "List<Integer>", None)])
			self.assertEqual(java_enum_constants(path, "Target"), [("ONE", None), ("TWO", None)], "an enum without ';' ends at its brace")

	def test_config_values(self):
		with tempfile.TemporaryDirectory() as root:
			java = Path(root) / "src"
			config = java / "com" / "aionemu" / "gameserver" / "configs" / "main"
			config.mkdir(parents=True)
			(config / "RatesConfig.java").write_text('public class RatesConfig {\n\t@Property(key = "gameserver.rates.kinah.quest", defaultValue = '
			                                          '"1.0, 2.0")\n\tpublic static float[] QUEST_KINAH_RATES;\n}\n', encoding="utf-8")
			props = Path(root) / "config"
			(props / "main").mkdir(parents=True)
			value = read_config(java, props, None, ("main", "RatesConfig.java"), "QUEST_KINAH_RATES")
			self.assertEqual((value.effective, value.properties_file, float_array(value)), ("1.0, 2.0", None, ["1.0", "2.0"]))
			(props / "main" / "rates.properties").write_text("# comment\ngameserver.rates.kinah.quest = 3.5, 4\n", encoding="utf-8")
			value = read_config(java, props, None, ("main", "RatesConfig.java"), "QUEST_KINAH_RATES")
			self.assertEqual((value.effective, value.properties_file, value.profile_file), ("3.5, 4", "main/rates.properties", None),
			                 "the shipped properties file wins over the class default")
			profile = props / "mygs.properties"
			profile.write_text("gameserver.rates.kinah.quest = 9\n", encoding="utf-8")
			value = read_config(java, props, None, ("main", "RatesConfig.java"), "QUEST_KINAH_RATES", profile)
			self.assertEqual((value.effective, value.properties_value, value.profile_file), ("9", "3.5, 4", "mygs.properties"),
			                 "Config.loadProperties reads the profile over the defaults folders")
			self.assertEqual(read_config(java, props, None, ("main", "RatesConfig.java"), "QUEST_KINAH_RATES").effective, "3.5, 4", "no profile")
			with self.assertRaises(OracleError):  # a profile named explicitly must exist
				read_config(java, props, None, ("main", "RatesConfig.java"), "QUEST_KINAH_RATES", props / "missing.properties")
			profile.write_text("gameserver.rates.kinah.quest = 9\\\n  , 10\n", encoding="utf-8")
			with self.assertRaises(OracleError):  # a continued line is not modelled
				read_config(java, props, None, ("main", "RatesConfig.java"), "QUEST_KINAH_RATES", profile)

	def test_java_integer_takes_plain_decimals_only(self):
		self.assertEqual([java_integer(t, 32, "x") for t in ("12", " +12\n", "-7", "0")], [12, 12, -7, 0])
		for text in ("1_0", "١", "1 0", "0x10", "", "12L", "2147483648"):
			with self.subTest(text=ascii(text)), self.assertRaises(OracleError):  # Python's int() reads the first two, JAXB does not
				java_integer(text, 32, "x")


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5dJavaRulesTest(unittest.TestCase):
	"""The tables the oracle reads from the Java sources, as they stand today."""

	@classmethod
	def setUpClass(cls):
		cls.jaxb = JaxbModel(JAVA_SRC)
		cls.dialogs = DialogTables.read(JAVA_SRC)
		cls.rules = QuestRules.read(JAVA_SRC, JAVA_CONFIG, runner.DEFAULT_STATIC_DATA / "events" / "timed_events")

	def test_xml_quests_binds_16_tags(self):
		self.assertEqual(len(self.jaxb.quest_kinds), 16)
		self.assertEqual((self.jaxb.quest_kinds["report_to"], self.jaxb.quest_kinds["monster_hunt"]), ("ReportToData", "MonsterHuntData"))
		bindings = self.jaxb.bindings("KillSpawnedData")
		self.assertIn(("element", "monster"), bindings)
		self.assertIn(("attribute", "start_dialog_id"), bindings, "inherited from MonsterHuntData")
		template = self.jaxb.bindings("QuestTemplate")
		self.assertEqual(template[("attribute", "max_repeat_count")].initializer, "1")
		self.assertNotIn(("attribute", "quest_zone"), template, "QuestTemplate does not bind quest_zone")

	def test_dialog_tables(self):
		actions = self.dialogs.actions
		self.assertEqual((actions["QUEST_SELECT"], actions["QUEST_ACCEPT_1"], actions["SELECT_QUEST_REWARD"], actions["SELECTED_QUEST_NOREWARD"],
		                  actions["SETPRO1"], actions["SET_SUCCEED"], actions["USE_OBJECT"]), (31, 1002, 1009, 23, 10000, 10255, -1))
		self.assertEqual([self.dialogs.reward_page(i) for i in (None, 0, 1, 3, 4, 9, 10)], [0, 5, 6, 8, 45, 50, 0])
		self.assertEqual(self.dialogs.quest_status, {"START": 3, "REWARD": 4, "COMPLETE": 5, "LOCKED": 6})

	def test_rates_and_config_defaults(self):
		self.assertEqual((self.rules.xp_quest_rate, self.rules.kinah_rate, self.rules.ap_rate, self.rules.gp_rate), (1.0, 1.0, 1.0, 1.0),
		                 "RatesConfig defaults '1.0, 2.0', membership 0")
		self.assertEqual(self.rules.config["XP_QUEST_RATES"].class_default, "1.0, 2.0")
		self.assertEqual(self.rules.config["XP_QUEST_RATES"].properties_file, "main/rates.properties")
		self.assertEqual((self.rules.quest_size_limit, self.rules.max_master_crafting), (40, 1))
		self.assertEqual((self.rules.no_exp_world, self.rules.repose_level, self.rules.salvation_level, self.rules.first_rank), (301200000, 10, 15, 1))
		self.assertEqual(self.rules.combine_any, [30002, 30003, 40001, 40002, 40003, 40004, 40007, 40008, 40010])
		self.assertEqual(self.rules.combine_any_excluded_factions, [12, 13])
		self.assertEqual(self.rules.class_selectable["GLADIATOR"], "fighterSelectableReward")
		self.assertEqual((self.rules.quest_exp(130), self.rules.quest_kinah(120)), (130, 120))
		check_registration_order(JAVA_SRC)

	def test_a_changed_rate_shape_is_refused(self):
		with tempfile.TemporaryDirectory() as root:
			base = ("com", "aionemu", "gameserver")
			source, target = JAVA_SRC.joinpath(*base), Path(root).joinpath(*base)
			shutil.copytree(source / "model" / "gameobjects" / "player", target / "model" / "gameobjects" / "player",
			                ignore=lambda d, names: [n for n in names if n not in ("Rates.java", "PlayerCommonData.java")])
			for part in ("services/QuestService.java", "controllers/PlayerController.java", "utils/stats/AbyssRankEnum.java",
			             "model/templates/QuestTemplate.java", "configs/main/RatesConfig.java", "configs/main/CustomConfig.java",
			             "configs/main/CraftConfig.java"):
				(target / part).parent.mkdir(parents=True, exist_ok=True)
				shutil.copyfile(source / part, target / part)
			QuestRules.read(Path(root), JAVA_CONFIG, None)
			rates = target / "model" / "gameobjects" / "player" / "Rates.java"
			text = rates.read_text(encoding="utf-8")
			rates.write_text(text.replace("return (long) (xp * calcXpRate(player, RatesConfig.XP_QUEST_RATES",
			                              "return (long) Math.min(xp * calcXpRate(player, RatesConfig.XP_QUEST_RATES", 1), encoding="utf-8")
			with self.assertRaises(OracleError):
				QuestRules.read(Path(root), JAVA_CONFIG, None)


QUESTS = """
<quest id="100" name="fixture report" quest_zone="Fixture" minlevel_permitted="1" race_permitted="ELYOS"><rewards gold="120" exp="130"/></quest>
<quest id="101" name="fixture hunt" minlevel_permitted="1" race_permitted="ELYOS">
	<rewards gold="7" exp="11"><selectable_reward_item item_id="501"/><selectable_reward_item item_id="502" count="2"/>
		<reward_item item_id="500" count="10"/></rewards>
	<quest_kill step="0" var="0" count="3" npc_ids="900001 900002" seq="0"/>
	<quest_kill var="1" count="70" npc_ids="900003" seq="1"/>
	<start_conditions><finished quest_id="100"/></start_conditions>
</quest>
<quest id="102" name="fixture grey" minlevel_permitted="3" race_permitted="PC_ALL"><rewards exp="5"/></quest>
<quest id="103" name="fixture too high" minlevel_permitted="4"><rewards exp="5"/></quest>
<quest id="104" name="fixture asmodian" minlevel_permitted="1" race_permitted="ASMODIANS"><rewards exp="5"/></quest>
<quest id="105" name="fixture male" minlevel_permitted="1"><rewards exp="5"/><gender_permitted>MALE</gender_permitted></quest>
<quest id="106" name="fixture java" minlevel_permitted="1"><rewards exp="5"/></quest>
<quest id="107" name="fixture item" minlevel_permitted="1"><rewards exp="5"/><inventory_items><inventory_item item_id="160000001" count="1"/></inventory_items></quest>
<quest id="108" name="fixture no item" minlevel_permitted="1"><rewards exp="5"/><inventory_items><inventory_item item_id="999" count="1"/></inventory_items></quest>
<quest id="109" name="fixture faction" minlevel_permitted="1" npcfaction_id="1"><rewards exp="5"/></quest>
<quest id="110" name="fixture collect" minlevel_permitted="1">
	<collect_items><collect_item item_id="182200201" count="3"/></collect_items>
	<rewards gold="290" exp="590"/>
	<quest_drop npc_id="700001" item_id="182200201"/>
	<quest_work_items><quest_work_item item_id="182200999" count="2"/></quest_work_items>
</quest>
<quest id="111" name="fixture data driven" minlevel_permitted="1" data_driven="true"><rewards exp="5"/>
	<quest_kill var="1" count="2" npc_ids="900004" seq="1" step="0"/><quest_kill var="2" count="1" npc_ids="900005" seq="2" step="0"/></quest>
<quest id="113" name="fixture temporary" minlevel_permitted="1"><rewards exp="5"/></quest>
<quest id="114" name="fixture pool" minlevel_permitted="1"><rewards exp="5"/></quest>
"""

SCRIPTS = """
<report_to id="100" start_npc_ids="800001" end_npc_ids="800002"/>
<monster_hunt id="101" start_npc_ids="800002"/>
<report_to id="102" start_npc_ids="800001"/>
<report_to id="103" start_npc_ids="800001"/>
<report_to id="104" start_npc_ids="800001"/>
<report_to id="105" start_npc_ids="800001"/>
<report_to id="107" start_npc_ids="800003"/>
<report_to id="108" start_npc_ids="800003"/>
<report_to id="109" start_npc_ids="800003"/>
<item_collecting id="110" start_npc_ids="800002"/>
<monster_hunt id="111" start_npc_ids="800004"/>
<report_to id="113" start_npc_ids="800005"/>
<report_to id="114" start_npc_ids="800006"/>
<unknown_quest_kind id="999"/>
"""

NPCS = "".join(f'<npc_template npc_id="{i}" name="fixture {i}" level="1" ai="general"/>' for i in
               (800001, 800002, 800003, 800004, 800005, 800006, 900001, 900002, 900003, 900004, 900005, 700001))

SPAWNS = """
<spawn_map map_id="1">
	<spawn npc_id="800001"><spot x="1" y="1" z="1"/></spawn>
	<spawn npc_id="800002"><spot x="2" y="2" z="1"/></spawn>
	<spawn npc_id="800003"><spot x="3" y="3" z="1"/></spawn>
	<spawn npc_id="800004" difficult_id="1"><spot x="4" y="4" z="1"/></spawn>
	<spawn npc_id="800005"><temporary_spawn spawn_time="10.*.*" despawn_time="12.*.*"/><spot x="5" y="5" z="1"/></spawn>
	<spawn npc_id="800006" pool="1"><spot x="6" y="6" z="1"/><spot x="7" y="7" z="1"/></spawn>
</spawn_map>
"""

JAVA_HANDLER = """package quest.fixture;

public class _106FixtureJava extends AbstractQuestHandler {

	private static final int START_NPC_ID = 800003; // the start npc

	public _106FixtureJava() {
		super(106);
	}

	@Override
	public void register() {
		qe.registerQuestNpc(START_NPC_ID).addOnQuestStart(questId);
	}
}
"""


def fixture_world_dirs(quests=QUESTS, scripts=SCRIPTS, kinah_rate="2.0, 3.0", event=None, npcs=NPCS, spawns=SPAWNS, rates=None, profile=None):
	"""A static_data tree, a data/handlers/quest directory with one Java handler and a config directory beside it (main/rates.properties
	sets the kinah rate, or holds `rates`; `profile` is written as config/mygs.properties)."""
	tree = Tree()
	tree.minimal({
		"world_maps": '<map id="1" world_type="ELYSEA" world_size="1024"/>',
		"quests": quests,
		"quest_scripts": scripts,
		"npc_templates": npcs,
		"spawns": spawns,
		"town_spawns_data": '<spawn_map map_id="2"><town_spawn town_id="1"><town_level level="1"><spawn npc_id="800001"/></town_level></town_spawn>'
		                    '</spawn_map>',
		"item_templates": '<item_template id="160000001" name="fixture potion" max_stack_count="100"/>',
		"player_experience_table": "".join(f"<exp>{e}</exp>" for e in (0, 400, 1433, 3820, 7000, 11000, 16000, 22000, 30000, 40000, 50000)),
		"player_initial_data": '<elyos_spawn_location map_id="1" x="1" y="1" z="1" heading="0"/>'
		                       '<player_data class="WARRIOR"><items><item id="160000001" count="12"/></items></player_data>',
	})
	if event:
		tree.write("events/timed_events/custom_events.xml", f'<?xml version="1.0" encoding="UTF-8"?>\n<timed_events>{event}</timed_events>\n')
	handlers = tree.root.parent / "handlers" / "quest" / "fixture"
	handlers.mkdir(parents=True)
	(handlers / "_106FixtureJava.java").write_text(JAVA_HANDLER, encoding="utf-8")
	config = tree.root.parent / "config" / "main"
	config.mkdir(parents=True)
	(config / "rates.properties").write_text(rates or f"gameserver.rates.kinah.quest = {kinah_rate}\n", encoding="utf-8")
	if profile is not None:
		(config.parent / "mygs.properties").write_text(profile, encoding="utf-8")
	return tree, tree.root.parent / "handlers" / "quest", tree.root.parent / "config"


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5dFixtureReportTest(unittest.TestCase):
	"""The whole report on a small static_data tree (the tables still come from the Java sources)."""

	@classmethod
	def setUpClass(cls):
		cls.tree, handlers, config = fixture_world_dirs()
		cls.world = QuestWorld(StaticData(cls.tree.root), JAVA_SRC, handlers, config)

	@classmethod
	def tearDownClass(cls):
		cls.tree.close()

	def test_census_and_registry(self):
		census = self.world.census()
		self.assertEqual((census["questTemplates"], census["xmlQuests"], census["javaHandlers"], census["both"]), (14, 13, 1, []))
		self.assertEqual(census["xmlIgnoredElements"], ["quest_scripts.xml: <unknown_quest_kind>"], "JAXB drops a tag XMLQuests does not bind")
		self.assertEqual(census["xmlByKind"], {"report_to": 10, "monster_hunt": 2, "item_collecting": 1})
		self.assertEqual(self.world.java[106].start_npcs, [800003], "the int field START_NPC_ID")
		self.assertEqual(self.world.registry_of(106), "java")
		self.assertEqual(self.world.xml_start_npcs()[800002], [101, 110], "registration (HashMap) order: 101 in bucket 5, 110 in bucket 14")

	def test_the_report_to_quest(self):
		report = quest_report(self.world, 100)
		self.assertEqual((report["questZone"], report["ignoredByJaxb"]), ("Fixture", ["@quest_zone"]))
		handler = report["handler"]
		self.assertEqual((handler["registry"], handler["xml"]["kind"], handler["xml"]["templateClass"]), ("xml", "report_to", "ReportTo"))
		steps = [(s["status"], s["at"].get("npcs"), s["action"]["id"] if isinstance(s["action"], dict) else s["action"][0]["id"],
		          s["window"]) for s in report["steps"]]
		self.assertEqual(steps, [
			("startable", [800001], 31, {"page": 1011, "questId": 100}),
			("startable", [800001], 1002, {"page": 1003, "questId": 100}),
			("START", [800002], 31, {"page": 2375, "questId": 100}),
			("START", [800002], 1009, {"page": 5, "questId": 100}),
			("REWARD", [800002], -1, {"page": 5, "questId": 100}),
			("REWARD", [800002], 23, None)])
		self.assertEqual(report["steps"][3]["effect"]["vars"], [1, 0, 0, 0, 0, 0], "ReportTo: setQuestVar(1) and REWARD")
		self.assertEqual(report["steps"][5]["effect"]["statusValue"], 5)
		rewards = report["rewards"]
		self.assertEqual(rewards["groups"][0]["kinah"], {"template": 120, "paid": 240}, "the fixture config's kinah rate 2.0")
		self.assertEqual(rewards["firstCompletion"]["payments"], [{"kinah": 240}, {"exp": 130}])
		[at_end] = report["followUp"]["atEndNpcs"]
		self.assertEqual((at_end["npc"], at_end["onQuestStartOrder"], at_end["followUp"], at_end["window"]),
		                 (800002, [101, 110], 101, {"page": 1011, "questId": 101}),
		                 "101 names 100 in <finished>; 110 would only make it page 10")

	def test_the_monster_hunt_quest(self):
		report = quest_report(self.world, 101, completed=["100"])
		kills = report["targets"]["killRun"]
		self.assertEqual(len(kills), 4 + 71, "three kills and one too many for each monster, 70 + 1 for the second")
		self.assertEqual(kills[3]["handled"], False)
		self.assertEqual(kills[-1]["vars"], [3, 6, 1, 0, 0, 0], "70 kills in var1 (low six bits) and var2")
		self.assertEqual([s["window"] for s in report["steps"] if s["status"] == "START"],
		                 [{"page": 1352, "questId": 101}, {"page": 5, "questId": 101}])
		self.assertEqual(report["rewards"]["firstCompletion"]["payments"], [{"item": 500, "count": 10}, {"kinah": 14}, {"exp": 11}])
		self.assertEqual([i["actionId"] for i in report["rewards"]["groups"][0]["selectableItems"]], [8, 9])
		self.assertEqual(report["followUp"]["atEndNpcs"][0]["window"], {"page": 10, "questId": 0}, "110 is startable, it names no quest")

	def test_the_item_collecting_and_data_driven_quests(self):
		report = quest_report(self.world, 110, race="ELYOS")
		accept = report["steps"][1]
		self.assertEqual(accept["effect"]["giveItems"], [{"itemId": 182200999, "count": 2}], "the work item on accept")
		check = report["steps"][3]
		self.assertEqual((check["action"]["id"], check["window"], check["effect"]["removeItems"]),
		                 (39, {"page": 5, "questId": 110}, [{"itemId": 182200201, "count": 3}]))
		self.assertEqual(report["registration"]["onTalkEvent"], [800002, 700001], "the 7xxxxx quest_drop npc is an action item")
		self.assertEqual(report["collect"]["drops"][0]["chance"], 100)
		report = quest_report(self.world, 111, race="ELYOS")
		self.assertEqual(report["targets"]["killRun"][-1]["vars"], [1, 0, 0, 0, 0, 0])
		self.assertEqual(report["steps"][2]["window"], {"page": 10002, "questId": 111}, "data driven: page 10002 in REWARD")

	def test_the_map_markers(self):
		report = map_report(self.world, 1, gender="MALE")
		self.assertEqual(report["nearby"]["xmlOnly"], [100, 102, 105, 107, 110, 114])
		self.assertEqual(report["nearby"]["xmlOnlyGrey"], [102])
		self.assertEqual(report["nearby"]["xmlOnlyWire"], [100, 105, 107, 110, 114, 102 | 1 << 17])
		self.assertEqual(report["nearby"]["withJava"], [100, 102, 105, 106, 107, 110, 114])
		failed = {r["questId"]: r["failed"] for r in report["quests"]}
		self.assertEqual((failed[101], failed[103], failed[104], failed[108], failed[109]),
		                 ("startConditions", "minLevel", "race", "inventoryItems", "npcFaction (a character without an active npc faction)"))
		self.assertNotIn(111, failed, "difficult_id 1: not spawned in the open world")
		self.assertEqual((report["uncertainSpawns"], report["exact"]), ([113], False), "a temporary spawn without a game time")
		timed = map_report(self.world, 1, gender="MALE", clock=GameClock(hour=10))
		self.assertEqual((113 in timed["nearby"]["xmlOnly"], timed["exact"]), (True, True))
		later = map_report(self.world, 1, gender="MALE", completed=["100"], started=[110])
		self.assertEqual(later["nearby"]["xmlOnly"], [101, 102, 105, 107, 114])

	def test_what_the_oracle_refuses(self):
		with self.assertRaises(OracleError):  # quest 105 needs the gender
			map_report(self.world, 1)
		with self.assertRaises(OracleError):
			quest_report(self.world, 12345)
		with self.assertRaises(OracleError):
			make_character(self.world, "ELYOS", "WARRIOR", 10, None)  # not a daeva: capped at 9
		char = make_character(self.world, "ELYOS", "WARRIOR", 1, None)
		self.assertEqual(check_start_conditions(self.world, char, 105)[0], None)
		self.assertEqual(check_start_conditions(self.world, char, 104), (False, "race"), "a definite failure wins over the unknown gender")
		cases = [
			(QUESTS, SCRIPTS + '<report_to id="777" start_npc_ids="1"/>', None),  # an XML quest without template: Java throws at startup
			(QUESTS.replace('<rewards gold="120" exp="130"/>', '<rewards gold="120" exp="130"/><quest_work_items/>'), SCRIPTS, None),
			(QUESTS, SCRIPTS, '<event name="x"><config_properties><property>gameserver.rates.xp.quest = 3</property></config_properties></event>'),
			(QUESTS.replace('race_permitted="ASMODIANS"', 'race_permitted="MARTIANS"'), SCRIPTS, None),
		]
		for quests, scripts, event in cases:
			with self.subTest(event=event):
				tree, handlers, config = fixture_world_dirs(quests, scripts, event=event)
				try:
					with self.assertRaises(OracleError):
						world = QuestWorld(StaticData(tree.root), JAVA_SRC, handlers, config)
						world.registrations()
						quest_report(world, 104)
				finally:
					tree.close()


RULE_QUESTS = """
<quest id="200" name="rates" minlevel_permitted="1" race_permitted="ELYOS"><rewards exp="16777217" ap="100" dp="7" gp="10"/></quest>
<quest id="201" name="relic ap" minlevel_permitted="1" race_permitted="ELYOS" category="NON_COUNT"><rewards ap="100"/></quest>
<quest id="202" name="extended at once" minlevel_permitted="1" reward_repeat_count="1"><rewards gold="10"/>
	<extended_rewards gold="5"><reward_item item_id="600" count="1"/></extended_rewards></quest>
<quest id="203" name="extended later" minlevel_permitted="1"><rewards gold="10"/><extended_rewards gold="5"><reward_item item_id="600"/></extended_rewards></quest>
<quest id="204" name="optional groups" minlevel_permitted="1"><rewards exp="5"/>
	<start_conditions><finished quest_id="200"/></start_conditions><start_conditions><finished quest_id="201"/></start_conditions>
	<start_conditions><noacquired>205</noacquired></start_conditions></quest>
<quest id="205" name="max level" minlevel_permitted="1" maxlevel_permitted="1"><rewards exp="5"/></quest>
<quest id="206" name="reward group" minlevel_permitted="1"><rewards exp="5"/><start_conditions><finished quest_id="200" reward="0"/></start_conditions></quest>
<quest id="207" name="daily" minlevel_permitted="1" max_repeat_count="255" repeat_cycle="ALL"><rewards exp="5"/></quest>
<quest id="208" name="unlimited" minlevel_permitted="1" max_repeat_count="255"><rewards exp="5"/></quest>
<quest id="209" name="rank" minlevel_permitted="1" rank="2"><rewards exp="5"/></quest>
<quest id="210" name="finished" minlevel_permitted="1" race_permitted="ELYOS"><rewards exp="5"/></quest>
<quest id="211" name="acceptable" minlevel_permitted="1"><rewards exp="5"/><start_conditions><finished quest_id="210"/></start_conditions></quest>
<quest id="212" name="not acceptable" minlevel_permitted="1"><start_conditions><finished quest_id="210"/></start_conditions></quest>
<quest id="213" name="never" minlevel_permitted="99"><rewards exp="5"/><start_conditions><finished quest_id="215"/></start_conditions></quest>
<quest id="214" name="later" minlevel_permitted="98"><rewards exp="5"/><start_conditions><finished quest_id="215"/></start_conditions></quest>
<quest id="215" name="finished too" minlevel_permitted="1" race_permitted="ELYOS"><rewards exp="5"/></quest>
<quest id="216" name="male only, names nothing" minlevel_permitted="1"><rewards exp="5"/><gender_permitted>MALE</gender_permitted></quest>
<quest id="219" name="male only at 215's npc" minlevel_permitted="1"><rewards exp="5"/><gender_permitted>MALE</gender_permitted></quest>
<quest id="220" name="gives 990" minlevel_permitted="1" race_permitted="ELYOS"><rewards><reward_item item_id="990" count="1"/></rewards></quest>
<quest id="221" name="needs 990" minlevel_permitted="1"><rewards exp="5"/><inventory_items><inventory_item item_id="990" count="1"/></inventory_items></quest>
<quest id="222" name="takes the potion" minlevel_permitted="1" race_permitted="ELYOS"><rewards exp="5"/>
	<collect_items><collect_item item_id="160000001" count="1"/></collect_items></quest>
<quest id="223" name="needs the potion" minlevel_permitted="1"><rewards exp="5"/><inventory_items><inventory_item item_id="160000001"/></inventory_items></quest>
<quest id="230" name="levels up" minlevel_permitted="1" race_permitted="ELYOS"><rewards exp="500"/></quest>
<quest id="231" name="level 2 follow-up" minlevel_permitted="2"><rewards exp="5"/><start_conditions><finished quest_id="230"/></start_conditions></quest>
<quest id="232" name="report on level 2" minlevel_permitted="2" race_permitted="ELYOS"><rewards exp="5"/></quest>
<quest id="233" name="levels up at 232's npc" minlevel_permitted="1" race_permitted="ELYOS"><rewards exp="500"/></quest>
<quest id="240" name="finish" minlevel_permitted="1" race_permitted="ELYOS"><rewards exp="5"/></quest>
<quest id="241" name="fountain follow-up" minlevel_permitted="1"><rewards exp="5"/><start_conditions><finished quest_id="240"/></start_conditions></quest>
<quest id="242" name="relic" minlevel_permitted="1" race_permitted="ELYOS" category="NON_COUNT">
	<collect_items start_check="true"><collect_item item_id="186000001" count="1"/></collect_items><rewards ap="100"/></quest>
<quest id="243" name="fountain after the relic" minlevel_permitted="1"><rewards exp="5"/><start_conditions><finished quest_id="242"/></start_conditions></quest>
<quest id="250" name="no work item" minlevel_permitted="1" race_permitted="ELYOS"><rewards exp="5"/>
	<quest_work_items><quest_work_item item_id="182200998" count="0"/></quest_work_items></quest>
<quest id="251" name="zone kills" minlevel_permitted="1" race_permitted="ELYOS"><rewards exp="5"/></quest>
<quest id="260" name="steps listed backwards" minlevel_permitted="1" race_permitted="ELYOS" data_driven="true"><rewards exp="5"/>
	<quest_kill var="1" count="1" npc_ids="900014" step="1"/><quest_kill var="1" count="2" npc_ids="900015" step="0"/></quest>
<quest id="270" name="spawns nothing" minlevel_permitted="1" race_permitted="ELYOS"><rewards exp="5"/></quest>
<quest id="271" name="crafts at 300" minlevel_permitted="1" race_permitted="ELYOS"><rewards exp="5"/></quest>
<quest id="272" name="xml kills" minlevel_permitted="1" race_permitted="ELYOS"><rewards exp="5"/></quest>
"""

RULE_SCRIPTS = """
<report_to id="200" start_npc_ids="810000"/>
<report_to id="210" start_npc_ids="810010" end_npc_ids="810011"/>
<report_to id="211" start_npc_ids="810011"/>
<report_to id="212" start_npc_ids="810011"/>
<report_to id="215" start_npc_ids="810012" end_npc_ids="810013"/>
<report_to id="213" start_npc_ids="810013"/>
<report_to id="214" start_npc_ids="810013"/>
<report_to id="216" start_npc_ids="810011"/>
<report_to id="219" start_npc_ids="810013"/>
<report_to id="220" start_npc_ids="810020"/>
<report_to id="221" start_npc_ids="810022"/>
<item_collecting id="222" start_npc_ids="810020"/>
<report_to id="223" start_npc_ids="810022"/>
<report_to id="230" start_npc_ids="810030" end_npc_ids="810031"/>
<report_to id="231" start_npc_ids="810031"/>
<report_on_levelup id="232" end_npc_ids="810033"/>
<report_to id="233" start_npc_ids="810032" end_npc_ids="810033"/>
<report_to id="240" start_npc_ids="810040" end_npc_ids="810041"/>
<fountain_rewards id="241" start_npc_ids="810041"/>
<relic_rewards id="242" start_npc_ids="810042"/>
<fountain_rewards id="243" start_npc_ids="810042"/>
<report_to id="250" start_npc_ids="810050"/>
<kill_in_zone id="251" start_npc_ids="810051" amount="1"/>
<monster_hunt id="260" start_npc_ids="810060"/>
<kill_spawned id="270" start_npc_ids="810070"><monster var="0" end_var="1" npc_ids="" spawner_object_id="810071"/></kill_spawned>
<crafting_rewards id="271" start_npc_id="810072" skill_id="40001" level_reward="300"/>
"""

RULE_SPAWNS = """
<spawn_map map_id="1">
	<spawn npc_id="810020"><spot x="1" y="1" z="1"/></spawn>
	<spawn npc_id="810022"><spot x="2" y="2" z="1"/></spawn>
</spawn_map>
"""

RULE_NPCS = "".join(f'<npc_template npc_id="{i}" name="fixture {i}" level="1" ai="general"/>' for i in (810020, 810022))

RULE_RATES = ("gameserver.rates.kinah.quest = 1.0, 2.0\ngameserver.rates.xp.quest = 1.5, 2.0\ngameserver.rates.ap.quest = 3.0, 4.0\n"
              "gameserver.rates.gp.gain = 2.0, 3.0\n")
RULE_PROFILE = "gameserver.rates.gp.gain = 2.5\n"
NEXT_PAGE_23 = {"page": 23, "questId": 240, "nextPage": True}
GREY = 1 << 17


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5dRulesFixtureTest(unittest.TestCase):
	"""QuestService's rate arithmetic and start checks, the follow-up rules and the refusals, one fixture quest per rule (every rate other
	than 1.0 and apart from the others, and a profile over the defaults folders). Expected values from the Java methods named in m5d/."""

	@classmethod
	def setUpClass(cls):
		cls.tree, handlers, config = fixture_world_dirs(RULE_QUESTS, RULE_SCRIPTS, npcs=RULE_NPCS, spawns=RULE_SPAWNS, rates=RULE_RATES,
		                                                profile=RULE_PROFILE)
		cls.world = QuestWorld(StaticData(cls.tree.root), JAVA_SRC, handlers, config)

	@classmethod
	def tearDownClass(cls):
		cls.tree.close()

	def payments(self, quest_id):
		return quest_report(self.world, quest_id, race="ELYOS")["rewards"]["firstCompletion"]["payments"]

	def char(self, level=1, completed=()):
		char = make_character(self.world, "ELYOS", "WARRIOR", level, None)
		apply_quest_list(self.world, char, completed)
		return char

	def test_the_rates_in_float_arithmetic(self):
		# Rates.XP_QUEST: (long) (16777217 * 1.5f) with the long made a float (16777216.0f); AP_QUEST 3.0 (not NON_COUNT), GP 2.5 from the
		# profile over the defaults folder's 2.0, DP unrated (giveReward order: exp, ap, dp, gp)
		self.assertEqual(self.payments(200), [{"exp": 25165824}, {"ap": 300}, {"dp": 7}, {"gp": 25}])
		self.assertEqual(self.world.rules.config["GP_RATES"].profile_file, "mygs.properties")
		self.assertEqual(self.payments(201), [{"ap": 100}], "NON_COUNT: giveReward does not rate the AP of a relic exchange")

	def test_the_extended_rewards(self):
		# finishQuest: extended rewards when completeCount (0) == reward_repeat_count - 1; their items first, their kinah after the group's
		self.assertEqual(self.payments(202), [{"item": 600, "count": 1}, {"kinah": 10}, {"kinah": 5}])
		self.assertEqual(self.payments(203), [{"kinah": 10}], "reward_repeat_count 0: 0 != -1")

	def test_the_start_checks(self):
		self.assertEqual(quest_report(self.world, 204)["prerequisites"]["requiredConditionCount"], 2, "min(1, 2 optional) + 1 mandatory")
		self.assertEqual(check_start_conditions(self.world, self.char(completed=["200"]), 204), (True, None))
		self.assertEqual(check_start_conditions(self.world, self.char(), 205), (True, None), "level 1 is not above maxlevel 1")
		self.assertEqual(check_start_conditions(self.world, self.char(2), 205), (False, "maxLevel"))
		self.assertEqual(check_start_conditions(self.world, self.char(completed=["200:1"]), 206), (False, "startConditions"),
		                 "<finished reward=0>: the group finishQuest used must be 0")
		self.assertEqual(check_start_conditions(self.world, self.char(completed=["200:0"]), 206), (True, None))
		self.assertEqual(check_start_conditions(self.world, self.char(completed=["207"]), 207), (False, "not repeatable"),
		                 "a time based quest waits for its next repeat time")
		self.assertEqual(check_start_conditions(self.world, self.char(completed=["208"]), 208), (True, None))
		self.assertEqual(check_start_conditions(self.world, self.char(), 209), (False, "abyssRank"), "rank 1 < 2")

	def test_the_follow_up_rules(self):
		[at] = quest_report(self.world, 210)["followUp"]["atEndNpcs"]
		self.assertEqual(at["onQuestStartOrder"], [212, 216, 211], "HashSet<>(0) filled 211, 212, 216: 4 buckets, 212 and 216 in 0, 211 in 3")
		self.assertEqual((at["followUp"], at["window"]), (211, {"page": 1011, "questId": 211}),
		                 "212 has no reward: not acceptable; 216 (gender unknown) names no quest, so it cannot be the follow-up")
		[at] = quest_report(self.world, 215)["followUp"]["atEndNpcs"]
		self.assertEqual([(c["questId"], c["acceptable"]) for c in at["candidates"]], [(213, False), (214, True), (219, True)],
		                 "minlevel 99 is never offered")
		self.assertIsNone(at["window"], "219 decides between page 10 and 0, and the gender is not given")
		self.assertEqual([quest_report(self.world, 215, gender=g)["followUp"]["atEndNpcs"][0]["window"] for g in ("FEMALE", "MALE")],
		                 [{"page": 0, "questId": 0}, {"page": 10, "questId": 0}])
		self.assertEqual((is_acceptable_quest(self.world, self.world.template(213)), is_acceptable_quest(self.world, self.world.template(214))),
		                 (False, True))

	def test_the_follow_up_after_the_level_change(self):
		report = quest_report(self.world, 230)
		# 500 * 1.5 = 750 exp at level 1 (start exp 0): level 2 (400), where 231 (minlevel 2) passes
		self.assertEqual((report["followUp"]["character"]["levelsSinceEnterWorld"], report["followUp"]["character"]["expAfterReward"]), ([1, 2], 750))
		self.assertEqual(report["followUp"]["atEndNpcs"][0]["window"], {"page": 1011, "questId": 231})
		report = quest_report(self.world, 230, exp=700)
		self.assertEqual(report["followUp"]["character"]["levelsSinceEnterWorld"], [2, 3], "700 + 750 = 1450 >= 1433")
		with self.assertRaises(OracleError):
			quest_report(self.world, 230, level=2, exp=100)  # 100 exp is level 1
		[at] = quest_report(self.world, 233)["followUp"]["atEndNpcs"]
		self.assertEqual((at["autoStartedInReward"], at["window"]), ([232], None),
		                 "at level 2 ReportOnLevelUp starts 232 in REWARD, and 232 ends at this npc: its reward dialog comes first")

	def test_the_next_page_when_the_follow_up_does_not_answer(self):
		[at] = quest_report(self.world, 240)["followUp"]["atEndNpcs"]
		self.assertEqual((at["followUp"], at["followUpAnswer"]["handled"], at["window"]), (241, False, NEXT_PAGE_23),
		                 "FountainRewards leaves QUEST_SELECT unhandled: DialogService sends page 23 for the finished quest")
		[at] = quest_report(self.world, 242)["followUp"]["atEndNpcs"]
		self.assertEqual((at["followUp"], at["window"]), (243, {"page": None, "questId": None, "sent": False}),
		                 "RelicRewards answers true whatever sendQuestEndDialog answered: no window")

	def test_the_inventory_of_a_character_with_quests(self):
		base = map_report(self.world, 1)
		self.assertEqual(base["nearby"]["xmlOnly"], [220, 222, 223])
		self.assertEqual({r["questId"]: r["failed"] for r in base["quests"]}[221], "inventoryItems")
		with self.assertRaises(OracleError):  # 220 pays item 990 as a reward: the cube may hold it
			map_report(self.world, 1, completed=["220"])
		self.assertEqual(map_report(self.world, 1, completed=["220"], inventory=["990"])["nearby"]["xmlOnly"], [221, 222, 223])
		self.assertEqual(map_report(self.world, 1, completed=["220"], inventory=["990:0"])["nearby"]["xmlOnly"], [222, 223])
		with self.assertRaises(OracleError):  # 222 took the starting potion as a collect item
			map_report(self.world, 1, completed=["222"])
		self.assertEqual(map_report(self.world, 1, completed=["222"], inventory=["160000001:11"])["nearby"]["xmlOnly"], [220, 223])

	def test_the_steps_the_review_found(self):
		accept = quest_report(self.world, 250)["steps"][1]
		self.assertEqual((accept["action"][0]["id"], accept["effect"]["giveItems"]), (1002, []), "giveQuestItem gives nothing for count 0")
		reward = [s for s in quest_report(self.world, 251)["steps"] if s["status"] == "REWARD" and s["window"] == {"page": 5, "questId": 251}]
		self.assertEqual([a["id"] for a in reward[0]["action"]], [-1, 31, 1009], "KillInZone: USE_OBJECT goes to sendQuestEndDialog too")
		kills = quest_report(self.world, 260)["targets"]["killRun"]
		self.assertEqual([(k["target"], k["handled"]) for k in kills], [(900015, False), (900015, True), (900015, False), (900014, True)],
		                 "step 0's monster first although listed second; then step 1's")
		self.assertEqual((kills[-1]["status"], kills[-1]["vars"]), ("REWARD", [2, 0, 0, 0, 0, 0]))
		flow = Flow(self.world, 210, self.world.template(210))
		flow.state = SimState("START")
		self.assertEqual((flow.quest_dialog(5), flow.quest_dialog(1011)), (None, {"page": 1011, "questId": 210}),
		                 "sendQuestDialog refuses a reward window outside REWARD")
		flow.state.status = "REWARD"
		self.assertEqual(flow.quest_dialog(5), {"page": 5, "questId": 210})

	def test_what_the_rules_fixture_refuses(self):
		for quest_id in (270, 271):  # KillSpawned never reaches REWARD; CraftingRewards.canLearn throws for level_reward 300
			with self.subTest(quest=quest_id), self.assertRaises(OracleError):
				quest_report(self.world, quest_id)
		self.assertEqual(make_character(self.world, "ELYOS", "GLADIATOR", 10, None).level, 10)
		with self.assertRaises(OracleError):  # 11 <exp>: getMaxLevel() 11, the level at most 10
			make_character(self.world, "ELYOS", "GLADIATOR", 11, None)
		for scripts in ('<kill_spawned id="272" start_npc_ids="810073"><monster var="0" end_var="1"/></kill_spawned>',
		                '<xml_quest id="272" start_npc_ids="810073"><on_kill_event><monster var="0" end_var="1"/></on_kill_event></xml_quest>'):
			with self.subTest(scripts=scripts):  # register() dereferences the missing npc_ids at startup
				tree, handlers, config = fixture_world_dirs(RULE_QUESTS, scripts, npcs=RULE_NPCS, spawns=RULE_SPAWNS)
				try:
					world = QuestWorld(StaticData(tree.root), JAVA_SRC, handlers, config)
					with self.assertRaises(OracleError):
						world.registrations()
				finally:
					tree.close()

	def test_the_command_line_lists(self):
		with mock.patch("m5d.quests.QuestWorld", return_value=self.world):
			out = io.StringIO()
			with contextlib.redirect_stdout(out):
				code = oracle.main(["m5d-quests", "--map", "1", "--completed", "220", "208", "--inventory", "990", "--started", "210", "215"])
			self.assertEqual(code, 0)
			answer = json.loads(out.getvalue())
			self.assertEqual(sorted(answer["character"]["quests"]), ["208", "210", "215", "220"])
			self.assertEqual(answer["character"]["inventory"]["stated"], {"990": 1})
			self.assertEqual(answer["nearby"]["xmlOnly"], [221, 222, 223])


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5dRealDataTest(unittest.TestCase):
	"""The M5d gate's quests (m5d-plan.md §10): 1101 -> 1102 -> 1103 in Poeta, 2101 and 2102 in Ishalgen, and the enter-world markers."""

	@classmethod
	def setUpClass(cls):
		cls.world = QuestWorld(StaticData(runner.DEFAULT_STATIC_DATA), JAVA_SRC, JAVA_QUEST_HANDLERS, JAVA_CONFIG)

	def test_the_census(self):
		census = self.world.census()
		self.assertEqual((census["questTemplates"], census["xmlQuests"], census["xmlFiles"], census["javaHandlers"]), (8043, 4184, 89, 1035))
		self.assertEqual((census["both"], census["neither"], census["xmlDuplicateIds"], census["xmlWithoutTemplate"]), ([], 2824, [], []))
		self.assertEqual(census["xmlByKind"]["item_collecting"], 1681)
		self.assertEqual(census["xmlByKind"]["monster_hunt"], 1163)
		self.assertEqual(census["xmlByKind"]["xml_quest"], 1)

	def test_1101_report_to(self):
		report = quest_report(self.world, 1101)
		self.assertEqual(report["registration"]["onQuestStart"], [203049])
		windows = [(s["action"]["id"] if isinstance(s["action"], dict) else s["action"][0]["id"], s["window"]) for s in report["steps"]]
		self.assertEqual(windows, [(31, {"page": 1011, "questId": 1101}), (1002, {"page": 1003, "questId": 1101}),
		                           (31, {"page": 2375, "questId": 1101}), (1009, {"page": 5, "questId": 1101}),
		                           (-1, {"page": 5, "questId": 1101}), (23, None)])
		self.assertEqual(report["rewards"]["firstCompletion"]["payments"], [{"kinah": 120}, {"exp": 130}])
		[mires] = report["followUp"]["atEndNpcs"]
		self.assertEqual((mires["npc"], mires["followUp"], mires["window"]), (203057, 1102, {"page": 1011, "questId": 1102}))

	def test_1102_and_1103(self):
		report = quest_report(self.world, 1102, completed=["1101"])
		self.assertEqual(report["targets"]["kill"], [{"npcIds": [210133, 210134], "var": 0, "count": 3, "step": 0}])
		self.assertEqual([(k["vars"][0], k["questActionUpdates"]) for k in report["targets"]["killRun"]], [(1, 1), (2, 1), (3, 1), (3, 0)])
		self.assertEqual([s["window"]["page"] for s in report["steps"] if s["status"] == "START"], [1352, 5])
		self.assertEqual(report["rewards"]["firstCompletion"]["payments"], [{"kinah": 400}, {"exp": 180}])
		self.assertEqual(report["followUp"]["atEndNpcs"][0]["window"], {"page": 1011, "questId": 1103})
		report = quest_report(self.world, 1103, completed=["1101", "1102"])
		self.assertEqual(report["collect"]["items"], [{"itemId": 182200201, "count": 3}])
		self.assertEqual(report["collect"]["drops"][0]["npcId"], 700105)
		self.assertEqual(report["npcs"]["700105"]["ai"], "quest_use_item")

	def test_2101_and_2102(self):
		report = quest_report(self.world, 2101)
		self.assertEqual(report["rewards"]["firstCompletion"]["payments"], [{"kinah": 80}, {"exp": 130}])
		self.assertEqual(report["followUp"]["atEndNpcs"][0]["window"], {"page": 10, "questId": 0}, "no quest at vandar names 2101")
		report = quest_report(self.world, 2102)
		self.assertEqual([k["handled"] for k in report["targets"]["killRun"]], [True, True, True, True, False])
		self.assertEqual(report["rewards"]["firstCompletion"]["payments"], [{"item": 169300002, "count": 10}, {"kinah": 120}, {"exp": 180}])
		self.assertEqual(report["followUp"]["atEndNpcs"][0]["window"], {"page": 1011, "questId": 2103})

	def test_the_start_map_markers(self):
		poeta = map_report(self.world, 210010000)
		self.assertTrue(poeta["exact"])
		self.assertEqual(poeta["nearby"]["xmlOnly"], [1101, 1105, 1108, 1109, 1112, 1127])
		self.assertEqual(poeta["nearby"]["xmlOnlyGrey"], [1108, 1109, 1112, 1127], "minlevel 2 or 3 at level 1")
		self.assertEqual(poeta["nearby"]["withJava"], [1101, 1105, 1108, 1109, 1111, 1112, 1127], "Java adds 1111 (phase 6)")
		after = map_report(self.world, 210010000, completed=["1101"])
		self.assertEqual(after["nearby"]["xmlOnly"], [1102, 1105, 1108, 1109, 1112, 1127])
		ishalgen = map_report(self.world, 220010000)
		self.assertEqual(ishalgen["nearby"]["xmlOnly"], [2101, 2102, 2104, 2105, 2108, 2109, 2110, 2112, 2133])
		self.assertEqual(ishalgen["nearby"]["xmlOnlyGrey"], [2108, 2109, 2110, 2112, 2133])
		self.assertEqual([q["startsInRewardState"] for q in poeta["reportOnLevelUp"]], [False] * 10, "the stigma quests start at 30+")
		# SM_NEARBY_QUESTS: a HashMap of 6 or 9 ids has 16 buckets (id & 15 below 65536); only 2101 and 2133 share one (bucket 5)
		self.assertEqual(poeta["nearby"]["xmlOnlyWireOrder"], {"buckets": [[1105], [1108 | GREY], [1109 | GREY], [1127 | GREY], [1112 | GREY], [1101]],
		                                                       "exact": True})
		self.assertEqual(ishalgen["nearby"]["xmlOnlyWireOrder"], {
			"buckets": [[2112 | GREY], [2101, 2133 | GREY], [2102], [2104], [2105], [2108 | GREY], [2109 | GREY], [2110 | GREY]], "exact": False})

	def test_the_exp_of_a_big_reward_is_rounded_to_a_float(self):
		# Rates.XP_QUEST: (long) (105189500L * 1.0f), the long made a float first: 105189504
		t = self.world.template(21040)
		self.assertEqual(rewards_report(self.world, 21040, t, "monster_hunt", 0)["groups"][0]["exp"], {"template": 105189500, "paid": 105189504})

	def test_the_command_line(self):
		# the subparsers and their arguments, on this class's world instead of a second one built from the same directories
		with mock.patch("m5d.quests.QuestWorld", return_value=self.world) as built:
			out = io.StringIO()
			with contextlib.redirect_stdout(out):
				code = oracle.main(["m5d-quests", "--map", "220010000", "--level", "1", "--completed", "2101"])
			self.assertEqual(code, 0)
			answer = json.loads(out.getvalue())
			self.assertEqual((answer["character"]["race"], answer["character"]["quests"]["2101"]["status"]), ("ASMODIANS", "COMPLETE"))
			self.assertNotIn(2101, answer["nearby"]["xmlOnly"])
			err = io.StringIO()
			with contextlib.redirect_stderr(err), contextlib.redirect_stdout(io.StringIO()):
				self.assertEqual(oracle.main(["m5d-quests", "--map", "300610000"]), 2, "an instance map is refused")
			self.assertIn("instance", err.getvalue())
			self.assertEqual(built.call_count, 2, "one world per command")
			self.assertEqual(built.call_args.args[1].name, "src", "--java-src defaults to game-server/src beside data/static_data")


if __name__ == "__main__":
	unittest.main()
