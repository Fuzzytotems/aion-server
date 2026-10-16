"""M5c economy oracle, stage 1 (harness-b): the C19 blocks of m5c/sanctum.py (the Daeva seed and its enter-world learn list, the capital craft's
master, vendors, tool spots and exact kinah) and economy.py's manastone `socketing` (m5c-plan.md G-01's rest, §10.2 C16 and C19, §10.3 X17-X21a,
X24): the arithmetic alone, the Java literals as they stand today and on an edited copy, the whole blocks on a small static_data tree, and the
gate's seed, Sanctum and armour on the real data.

Expected values are derived by hand from the Java sources named in m5c/sanctum.py and m5c/economy.py and repeated per case. Everything that reads
the Java source tree is skipped without it, like the other M5c oracle tests.
"""

import contextlib
import io
import json
import shutil
import tempfile
import unittest
from pathlib import Path

from m5a.data import StaticData
from m5a.javafloat import f32
from m5c.craft import CraftContext
from m5c.economy import ECONOMY_KEYS, JavaEconomyRules, economy_report
from m5c.sanctum import C19_MEMBERS, C19_SOURCES, JavaC19Rules, _autolearn, level_at_load
from m5c.trade_config import load_config
from staticdata_oracle import OracleError
from staticdata_oracle import run as runner

import oracle

from .support import Tree
from .test_m5c_economy import COMMONS, CONFIG_DIR, EXPERIENCE, GATE_PROFILE, HANDLERS, HAVE_JAVA_TREE, JAVA_SRC, SQL

BASE = ("com", "aionemu", "gameserver")
NO_EVENTS = "gameserver.event.service.disabled_events=*"
# the M5c gate's keys (m5c-plan.md D6, game-server/config/m5c.properties.example)
C19_PROFILE = GATE_PROFILE + [NO_EVENTS, "gameserver.craft.fail.chance=0", "gameserver.rates.crafting.crit_chances=0, 0"]
TABLE = [0, 400, 1433, 3820, 9054, 17655, 30978, 52010, 82982, 126069, 2162140395]  # EXPERIENCE's values


class M5cSanctumFormulaTest(unittest.TestCase):
	"""PlayerCommonData.setExp's level at load and autoLearnSkills' row filter, without any Java source."""

	def rules(self, **changes):
		values = {"ascension_quests": {"ELYOS": 1006, "ASMODIANS": 2008}, "daeva_level": 10, "non_daeva_max_level": 10,
		          "daeva_swap": (10, 30001, 30002), "combine_action": 46, "question_id": 900852, "learn_min_level": 10, "craft_level_up_animation": 4}
		values.update(changes)
		return JavaC19Rules(**values)

	def test_the_level_at_load(self):
		rules = self.rules()
		# a Daeva's cap is the table's length (11 levels here): exp 126,069 is level 10, the table's last level is 11 - 1 = 10 at most
		self.assertEqual(level_at_load(TABLE, 126069, True, rules), 10)
		# without the quest the cap is 10: min(getLevelForExp(126069) = 10, 10 - 1) = 9 with a full bar (m5c0-client-session.md F-1)
		self.assertEqual(level_at_load(TABLE, 126069, False, rules), 9)
		self.assertEqual(level_at_load(TABLE, 126070, False, rules), 10, "exp ABOVE getStartExpForLevel(10) lifts the cap without the quest")
		self.assertEqual(level_at_load(TABLE, 999999999999, False, rules), 10, "far above: the cap goes, the table's end answers getMaxLevel() - 1")
		self.assertEqual(level_at_load(TABLE, 126068, False, rules), 9)
		self.assertEqual(level_at_load(TABLE, 3820, False, rules), 4)
		self.assertEqual(level_at_load(TABLE, 3819, False, rules), 3, "getLevelForExp: the highest level whose start exp the exp reaches")
		self.assertEqual(level_at_load(TABLE, 2162140395, True, rules), 10, "getLevelForExp answers getMaxLevel() - 1 at the table's end")

	def test_the_row_filter(self):
		starting = {"WARRIOR": "WARRIOR", "GLADIATOR": "WARRIOR", "MAGE": "MAGE"}
		rows = [("WARRIOR", "PC_ALL", 10, True, 1), (None, "PC_ALL", 10, True, 30001), (None, "PC_ALL", 10, True, 30003),
		        ("GLADIATOR", "ASMODIANS", 10, True, 2), ("GLADIATOR", "PC_ALL", 10, False, 3), ("GLADIATOR", "PC_ALL", 9, True, 4),
		        ("MAGE", "PC_ALL", 10, True, 5)]
		self.assertEqual(_autolearn(rows, "WARRIOR", 10, "ELYOS", starting), [1, 30001, 30003])
		self.assertEqual(_autolearn(rows, "GLADIATOR", 10, "ELYOS", starting), [30003], "30001 is skipped for a class that is no starting class")
		self.assertEqual(_autolearn(rows, "GLADIATOR", 10, "ASMODIANS", starting), [2, 30003],
		                 "the race filter; getTemplatesFor lists the race's rows before the PC_ALL ones, whatever the file order")
		self.assertEqual(_autolearn(rows, "GLADIATOR", 9, "ELYOS", starting), [4], "minLevel is the level itself")


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5cSanctumJavaRulesTest(unittest.TestCase):
	"""The literals the C19 blocks read, as they stand today, and the refusal of an edited copy."""

	@classmethod
	def setUpClass(cls):
		cls.root = tempfile.TemporaryDirectory()
		cls.java = Path(cls.root.name) / "src"
		for relative in C19_SOURCES:
			target = cls.java.joinpath(*BASE) / relative
			target.parent.mkdir(parents=True, exist_ok=True)
			shutil.copyfile(JAVA_SRC.joinpath(*BASE) / relative, target)

	@classmethod
	def tearDownClass(cls):
		cls.root.cleanup()

	def changed(self, relative: str, old: str, new: str) -> None:
		path = self.java.joinpath(*BASE) / relative
		self.doCleanups()
		original = path.read_bytes()
		text = original.decode("utf-8").replace("\r\n", "\n")
		self.assertIn(old, text, f"{path} no longer contains the text this case changes")
		path.write_bytes(text.replace(old, new, 1).encode("utf-8"))
		self.addCleanup(path.write_bytes, original)

	def test_today(self):
		rules = JavaC19Rules.read(JAVA_SRC)
		self.assertEqual(rules.ascension_quests, {"ELYOS": 1006, "ASMODIANS": 2008})
		self.assertEqual((rules.daeva_level, rules.non_daeva_max_level, rules.daeva_swap), (10, 10, (10, 30001, 30002)))
		self.assertEqual((rules.combine_action, rules.question_id, rules.learn_min_level), (46, 900852, 10))
		self.assertEqual(rules.craft_level_up_animation, 4, "ActionAnimation.CRAFT_LEVEL_UP(4)")
		self.assertEqual(JavaC19Rules.read(self.java), rules, "the copy reads the same")

	def test_a_changed_rule_is_refused(self):
		cases = [
			("model/gameobjects/player/PlayerCommonData.java", "qsl.getQuestState(1006) != null", "qsl.getQuestState(1007) != null"),
			("model/gameobjects/player/PlayerCommonData.java", "exp > pxt.getStartExpForLevel(10)", "exp >= pxt.getStartExpForLevel(10)"),
			("model/gameobjects/player/PlayerCommonData.java", "if (playerClass.isStartingClass())\n\t\t\treturn false;", "if (false)\n\t\t\treturn false;"),
			("dataholders/PlayerExperienceTable.java", "if (expValue >= experience[(i - 1)]) {", "if (expValue > experience[(i - 1)]) {"),
			("model/skill/PlayerSkillList.java", "if (skillLevel <= existingSkill.getSkillLevel())", "if (skillLevel < existingSkill.getSkillLevel())"),
			("model/gameobjects/player/RecipeList.java", "PacketSendUtility.sendPacket(player, new SM_LEARN_RECIPE(recipeId));", ""),
			("spawnengine/StaticObjectSpawnManager.java", "VisibleObjectTemplate objectTemplate = DataManager.ITEM_DATA.getItemTemplate(spawn.getNpcId());",
			 "VisibleObjectTemplate objectTemplate = DataManager.NPC_DATA.getNpcTemplate(spawn.getNpcId());"),
			("spawnengine/SpawnEngine.java", "case STATIC -> StaticObjectSpawnManager.spawnTemplate(spawn, instance.getInstanceId());",
			 "case STATIC -> {}"),
			("network/aion/serverpackets/SM_GATHERABLE_INFO.java", "writeD(visibleObject.getSpawn().getStaticId());", "writeD(0);"),
			("services/DialogService.java", "CraftSkillUpdateService.getInstance().learnSkill(player, npc);", "break;"),
			("model/craft/Profession.java", "return DataManager.SKILL_DATA.getSkillTemplate(skillId).getL10n();", "return null;"),
		]
		for relative, old, new in cases:
			with self.subTest(file=relative, old=old):
				self.changed(relative, old, new)
				with self.assertRaises(OracleError):
					JavaC19Rules.read(self.java)
		self.doCleanups()
		self.changed("services/player/PlayerLeaveWorldService.java", "PlayerDAO.storeOldCharacterLevel(player.getObjectId(), player.getLevel());",
		             "PlayerDAO.storeOldCharacterLevel(player.getObjectId(), 1);")
		with self.assertRaises(OracleError):
			JavaC19Rules.read(self.java)

	def test_a_comment_is_no_change(self):
		self.changed("model/gameobjects/player/PlayerCommonData.java", "QuestStateList qsl;", "QuestStateList qsl; // the quests")
		self.assertEqual(JavaC19Rules.read(self.java).ascension_quests["ELYOS"], 1006)

	def test_literals_are_read(self):
		# the literals not pinned by a member fingerprint (DialogAction, SM_QUESTION_WINDOW, the learn level in CraftSkillUpdateService) are read
		self.changed("network/aion/serverpackets/SM_QUESTION_WINDOW.java", "STR_CRAFT_ADDSKILL_CONFIRM = 900852;", "STR_CRAFT_ADDSKILL_CONFIRM = 900853;")
		self.assertEqual(JavaC19Rules.read(self.java).question_id, 900853)
		self.changed("model/DialogAction.java", "COMBINE_SKILL_LEVELUP = 46;", "COMBINE_SKILL_LEVELUP = 47;")
		self.assertEqual(JavaC19Rules.read(self.java).combine_action, 47)
		self.changed("services/craft/CraftSkillUpdateService.java", "if (player.getLevel() < 10)\n\t\t\treturn;", "if (player.getLevel() < 11)\n\t\t\treturn;")
		self.assertEqual(JavaC19Rules.read(self.java).learn_min_level, 11)
		self.changed("model/animations/ActionAnimation.java", "CRAFT_LEVEL_UP(4)", "CRAFT_LEVEL_UP(5)")
		self.assertEqual(JavaC19Rules.read(self.java).craft_level_up_animation, 5)
		self.changed("services/SkillLearnService.java", "if (!player.getSkillList().isSkillPresent(30002))\n\t\t\t\tplayer.getSkillList().addSkill(player, 30002,",
		             "if (!player.getSkillList().isSkillPresent(30003))\n\t\t\t\tplayer.getSkillList().addSkill(player, 30002,")
		with self.assertRaises(OracleError):  # the swap's shape: the skill checked must be the one added
			JavaC19Rules.read(self.java)

	def test_every_member_is_listed_once(self):
		self.assertEqual(len({(r, m) for r, m, _ in C19_MEMBERS}), len(C19_MEMBERS))


# ---- the fixture ------------------------------------------------------------------------------------------------------------------------

SKILL_TREE = (
	'<skill skillId="1001" minLevel="1" autolearn="true" classId="WARRIOR"/>'
	'<skill skillId="1002" minLevel="2" autolearn="true" classId="WARRIOR"/>'
	'<skill skillId="1005" minLevel="5" autolearn="true" classId="WARRIOR"/>'
	'<skill skillId="1009" minLevel="9" autolearn="true" classId="WARRIOR"/>'
	# a starting class's row at level 10: the Gladiator does not learn it (learnNewSkills takes the starting class below 10 only)
	'<skill skillId="1010" minLevel="10" autolearn="true" classId="WARRIOR"/>'
	# a Gladiator row at level 1: never learned, learnNewSkills starts at old_level + 1
	'<skill skillId="2001" minLevel="1" autolearn="true" classId="GLADIATOR"/>'
	'<skill skillId="2009" minLevel="9" autolearn="true" classId="GLADIATOR"/>'
	'<skill skillId="2010" minLevel="10" autolearn="true" classId="GLADIATOR"/>'
	'<skill skillId="2011" minLevel="10" autolearn="true" classId="GLADIATOR" race="ASMODIANS"/>'
	'<skill skillId="2012" minLevel="10" classId="GLADIATOR"/>'
	# the profession rows of craft_skill_tree.xml: class-less
	'<skill skillId="30001" minLevel="1" autolearn="true"/>'
	'<skill skillId="30003" minLevel="10" autolearn="true"/>'
	'<skill skillId="40009" minLevel="10" autolearn="true"/>'
)

SKILLS = "".join(f'<skill_template skill_id="{s}" name="fixture {s}" nameId="{n}" lvl="{lvl}" activation="NONE"/>' for s, n, lvl in (
	(1001, 11, 1), (1002, 12, 1), (1005, 15, 1), (1009, 19, 1), (1010, 20, 1), (2001, 21, 1), (2009, 29, 1), (2010, 30, 2), (2011, 31, 1),
	(2012, 32, 1), (30001, 280101, 1), (30002, 280102, 1), (30003, 280103, 1), (40009, 280109, 1), (40001, 280383, 1)))

RECIPES = (
	# the morph recipes: one per race, a PC_ALL one, one above the morph skill's level 1, one not autolearn and one without a race
	'<recipe_template id="800001" nameid="1" skillid="40009" race="ELYOS" skillpoint="1" dp="200" autolearn="1" productid="700010" quantity="3">'
	'<components_data><component quantity="1" itemid="700012"/></components_data></recipe_template>'
	'<recipe_template id="800002" nameid="2" skillid="40009" race="PC_ALL" skillpoint="1" autolearn="1" productid="700010" quantity="1">'
	'<components_data><component quantity="1" itemid="700012"/></components_data></recipe_template>'
	'<recipe_template id="800003" nameid="3" skillid="40009" race="ASMODIANS" skillpoint="1" autolearn="1" productid="700010" quantity="1">'
	'<components_data><component quantity="1" itemid="700012"/></components_data></recipe_template>'
	'<recipe_template id="800004" nameid="4" skillid="40009" race="ELYOS" skillpoint="2" autolearn="1" productid="700010" quantity="1">'
	'<components_data><component quantity="1" itemid="700012"/></components_data></recipe_template>'
	'<recipe_template id="800005" nameid="5" skillid="40009" race="ELYOS" skillpoint="1" productid="700010" quantity="1">'
	'<components_data><component quantity="1" itemid="700012"/></components_data></recipe_template>'
	'<recipe_template id="800006" nameid="6" skillid="40009" skillpoint="1" autolearn="1" productid="700010" quantity="1">'
	'<components_data><component quantity="1" itemid="700012"/></components_data></recipe_template>'
	# the capital craft: cooking at skill 1, one shell and two salt make two meals; an Asmodian twin
	'<recipe_template id="900001" nameid="7" skillid="40001" race="ELYOS" skillpoint="1" autolearn="1" productid="700001" quantity="2">'
	'<components_data><component quantity="1" itemid="700010"/><component quantity="2" itemid="700011"/></components_data></recipe_template>'
	'<recipe_template id="900003" nameid="8" skillid="40001" race="ASMODIANS" skillpoint="1" autolearn="1" productid="700001" quantity="2">'
	'<components_data><component quantity="1" itemid="700010"/><component quantity="2" itemid="700011"/></components_data></recipe_template>'
	# a morph recipe for --craft-recipe (refused: the morph skill needs no master)
	'<recipe_template id="900004" nameid="9" skillid="40009" race="ELYOS" skillpoint="1" productid="700001" quantity="1">'
	'<components_data><component quantity="1" itemid="700010"/></components_data></recipe_template>'
)

ITEMS = (
	'<item_template id="700001" name="fixture meal" quality="COMMON" price="300"/>'
	'<item_template id="700010" name="fixture shell" quality="COMMON" price="5"/>'
	'<item_template id="700011" name="fixture salt" quality="COMMON" price="50"/>'
	'<item_template id="700012" name="fixture morph input" quality="COMMON" price="1"/>'
	'<item_template id="169401081" name="fixture cooking stone" quality="COMMON"/>'
	'<item_template id="150000009" name="fixture oven" level="1" quality="COMMON" price="100"/>'
	# socketing: armour of level 4 (one slot, one optional), one without any socket, one with optional sockets only, level 10 and 11 pieces,
	# a piece whose sockets pass MAX_BASIC_STONES, an accessory-range id; the stones
	'<item_template id="110000001" name="fixture tunic" level="4" item_group="RB_TORSO" quality="COMMON" price="500" option_slot_bonus="1" m_slots="1"/>'
	'<item_template id="110000002" name="fixture bare" level="4" item_group="RB_TORSO" quality="COMMON" price="500"/>'
	'<item_template id="110000003" name="fixture optional" level="4" item_group="RB_TORSO" quality="COMMON" price="500" option_slot_bonus="2"/>'
	'<item_template id="110000010" name="fixture ten" level="10" item_group="RB_TORSO" quality="COMMON" price="500" m_slots="1"/>'
	'<item_template id="110000011" name="fixture eleven" level="11" item_group="RB_TORSO" quality="COMMON" price="500" m_slots="1"/>'
	'<item_template id="110000012" name="fixture many" level="11" item_group="RB_TORSO" quality="COMMON" price="500" m_slots="5" option_slot_bonus="3"/>'
	'<item_template id="120000001" name="fixture earring" level="4" item_group="EARRING" quality="COMMON" price="500" m_slots="1"/>'
	# an item below 120000000 whose group is neither a weapon nor an armour (EXTRACT_SWORD's EquipType is NONE), with a socket in its template;
	# a stigma armour and a stigma manastone (isStigma: a <stigma> child)
	'<item_template id="100000901" name="fixture extraction sword" level="4" item_group="EXTRACT_SWORD" quality="COMMON" price="500" m_slots="1"/>'
	'<item_template id="110000006" name="fixture stigma armour" level="4" item_group="RB_TORSO" quality="COMMON" price="500" m_slots="1"><stigma/>'
	'</item_template>'
	'<item_template id="167000041" name="fixture stigma stone" level="10" item_group="MANASTONE" quality="COMMON" price="10"><stigma/></item_template>'
	'<item_template id="167000010" name="fixture stone 10" level="10" item_group="MANASTONE" quality="COMMON" price="10"/>'
	'<item_template id="167000011" name="fixture rare stone 10" level="10" item_group="MANASTONE" quality="RARE" price="10"/>'
	'<item_template id="167000020" name="fixture stone 20" level="20" item_group="MANASTONE" quality="COMMON" price="10"/>'
	'<item_template id="167000021" name="fixture stone 21" level="21" item_group="MANASTONE" quality="COMMON" price="10"/>'
	'<item_template id="167000030" name="fixture stone 30" level="30" item_group="MANASTONE" quality="COMMON" price="10"/>'
	'<item_template id="166000001" name="fixture enchantment stone" level="10" item_group="ENCHANTMENT" quality="RARE" price="10"/>'
	'<item_template id="168000001" name="fixture odd stone" level="10" item_group="MANASTONE" quality="COMMON" price="10"/>'
)

NPCS = (
	# the cooking master (a real master id: CraftSkillUpdateService's professionByNpc table is Java's), two salt vendors, a stranger
	'<npc_template npc_id="203784" level="40" name="fixture master" ai="general"><stats maxHp="10"/><bound_radius front="0.25" side="0.35" upper="2"/>'
	'<talk_info distance="5" is_dialog="true" func_dialogs="46 79"/></npc_template>'
	'<npc_template npc_id="900020" level="10" name="fixture vendor" ai="general"><stats maxHp="10"/><bound_radius front="0.25" side="0.35" upper="2"/>'
	'<talk_info distance="5" is_dialog="true" func_dialogs="2 3"/></npc_template>'
	'<npc_template npc_id="900021" level="10" name="fixture far vendor" ai="general"><stats maxHp="10"/><bound_radius front="0.25" side="0.35" upper="2"/>'
	'<talk_info distance="5" is_dialog="true" func_dialogs="2 3"/></npc_template>'
	# map 5: a cooking master of CraftSkillUpdateService's table whose template lacks COMBINE_SKILL_LEVELUP (46), a vendor with the salt's
	# goods list but no BUY function (canSell false: not buyable) and a vendor that only walks (no fixed spot)
	'<npc_template npc_id="830058" level="40" name="fixture master without 46" ai="general"><stats maxHp="10"/>'
	'<bound_radius front="0.25" side="0.35" upper="2"/><talk_info distance="5" is_dialog="true" func_dialogs="79"/></npc_template>'
	'<npc_template npc_id="900022" level="10" name="fixture buyer only" ai="general"><stats maxHp="10"/><bound_radius front="0.25" side="0.35" upper="2"/>'
	'<talk_info distance="5" is_dialog="true" func_dialogs="3"/></npc_template>'
	'<npc_template npc_id="900023" level="10" name="fixture walking vendor" ai="general"><stats maxHp="10"/>'
	'<bound_radius front="0.25" side="0.35" upper="2"/><talk_info distance="5" is_dialog="true" func_dialogs="2 3"/></npc_template>'
)

SPAWNS = """
<spawn_map map_id="2">
	<spawn npc_id="203784"><spot x="100" y="100" z="10"/></spawn>
	<spawn npc_id="900020"><spot x="100" y="108" z="10"/></spawn>
	<spawn npc_id="900021"><spot x="100" y="130" z="10"/></spawn>
	<spawn npc_id="150000009" handler="STATIC"><spot x="104" y="100" z="10" static_id="7"/><spot x="90" y="100" z="10" static_id="8"/></spawn>
</spawn_map>
<spawn_map map_id="3">
	<spawn npc_id="900020"><spot x="100" y="108" z="10"/></spawn>
	<spawn npc_id="150000009" handler="STATIC"><spot x="104" y="100" z="10" static_id="7"/></spawn>
</spawn_map>
<spawn_map map_id="4">
	<spawn npc_id="203784"><spot x="100" y="100" z="10"/></spawn>
	<spawn npc_id="150000009" handler="STATIC" pool="1"><spot x="104" y="100" z="10" static_id="7"/><spot x="90" y="100" z="10" static_id="8"/></spawn>
</spawn_map>
<spawn_map map_id="5">
	<spawn npc_id="830058"><spot x="100" y="100" z="10"/></spawn>
	<spawn npc_id="900020"><spot x="100" y="108" z="10"/></spawn>
	<spawn npc_id="900022"><spot x="100" y="104" z="10"/></spawn>
	<spawn npc_id="900023"><spot x="100" y="103" z="10" walker_id="FIXTURE_WALKER"/></spawn>
	<spawn npc_id="150000009"><spot x="101" y="100" z="10"/></spawn>
	<spawn npc_id="150000009" handler="STATIC"><spot x="90" y="100" z="10" static_id="8"/><spot x="104" y="100" z="10" static_id="7"/></spawn>
</spawn_map>
"""

# the near vendor pays full price, the far one half: the report takes the nearest to the master, not the cheapest
TRADE = ('<tradelist_template npc_id="900020"><tradelist id="50"/></tradelist_template>'
         '<tradelist_template npc_id="900021" sell_price_rate="50"><tradelist id="50"/></tradelist_template>'
         '<tradelist_template npc_id="900022"><tradelist id="50"/></tradelist_template>'
         '<tradelist_template npc_id="900023"><tradelist id="50"/></tradelist_template>'
         '<trade_in_list_template npc_id="1"/><purchase_template npc_id="1"/>')
GOODS = '<list id="50"><item id="700011"/></list><in_list id="1"/><purchase_list id="1"/>'


def fixture_tree(skill_tree: str = SKILL_TREE, skills: str = SKILLS) -> Tree:
	tree = Tree()
	tree.minimal({"skill_tree": skill_tree, "skill_data": skills, "recipe_templates": RECIPES, "item_templates": ITEMS, "npc_templates": NPCS,
	              "spawns": SPAWNS, "npc_trade_list": TRADE, "goodslists": GOODS, "player_experience_table": EXPERIENCE})
	return tree


# the Daeva swap's other arms and the stored-crafting refusal, on a skill tree of their own
SWAP_SKILL_TREE = (
	'<skill skillId="1001" minLevel="1" autolearn="true" classId="WARRIOR"/>'
	# human gathering for the Elyos only: an Asmodian Warrior never holds 30001, so learnNewSkills has nothing to swap
	'<skill skillId="30001" minLevel="1" autolearn="true" race="ELYOS"/>'
	# an Elyos Warrior learns 30002 itself at level 5: the swap then only removes 30001
	'<skill skillId="30002" minLevel="5" autolearn="true" classId="WARRIOR" race="ELYOS"/>'
	# an Asmodian Warrior learns a crafting skill at level 3: a stored skill from --daeva-old-level 3 on
	'<skill skillId="40002" minLevel="3" autolearn="true" classId="WARRIOR" race="ASMODIANS"/>'
	'<skill skillId="2010" minLevel="10" autolearn="true" classId="GLADIATOR"/>'
	'<skill skillId="40009" minLevel="10" autolearn="true"/>'
)

# 30002's lvl (3) differs from 30001's (1): the swap adds 30002 at 30001's level (learnNewSkills: getSkillLevel(30001))
SWAP_SKILLS = "".join(f'<skill_template skill_id="{s}" name="fixture {s}" nameId="{n}" lvl="{lvl}" activation="NONE"/>' for s, n, lvl in (
	(1001, 11, 1), (2010, 30, 1), (30001, 280101, 1), (30002, 280102, 3), (40002, 280384, 1), (40009, 280109, 1), (40001, 280383, 1)))


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5cSanctumFixtureReportTest(unittest.TestCase):
	"""The C19 blocks and the socketing on a small static_data tree (the Java tables come from the Java sources)."""

	@classmethod
	def setUpClass(cls):
		cls.tree = fixture_tree()
		cls.data = StaticData(cls.tree.root)
		cls.rules = JavaEconomyRules.read(JAVA_SRC, HANDLERS, COMMONS, SQL)
		cls.config = load_config(JAVA_SRC, None, None, GATE_PROFILE, cls.data, keys=ECONOMY_KEYS)
		cls.craft_ctx = CraftContext.create(cls.data, JAVA_SRC, None, None, [NO_EVENTS])

	@classmethod
	def tearDownClass(cls):
		cls.tree.close()

	def report(self, config=None, **kwargs):
		return economy_report(self.data, JAVA_SRC, config or self.config, map_id=2, rules=self.rules, craft_ctx=self.craft_ctx, **kwargs)

	def daeva(self, **kwargs):
		return self.report(daeva_class="GLADIATOR", **kwargs)["daeva"]

	def test_the_daeva_seed(self):
		daeva = self.daeva()
		self.assertEqual((daeva["class"], daeva["startingClass"], daeva["race"]), ("GLADIATOR", "WARRIOR", "ELYOS"))
		self.assertEqual(daeva["seed"], {"playerClass": "GLADIATOR", "exp": 126069, "quest": {"id": 1006, "status": "COMPLETE"}, "oldLevel": 1},
		                 "getStartExpForLevel(10) = experience[9]; updateDaeva's Elyos quest")
		self.assertEqual((daeva["level"], daeva["levelWithoutQuest"]), (10, 9), "F-1: without the quest row a full bar at level 9")
		self.assertEqual(daeva["levelAboveThresholdWithoutQuest"], {"exp": 126070, "level": 10, "isDaeva": False})
		self.assertEqual(self.daeva(player_race="ASMODIANS")["seed"]["quest"]["id"], 2008)

	def test_the_enter_world_from_level_1(self):
		world = self.daeva()["enterWorld"]
		self.assertEqual((world["onLevelChange"], world["learnNewSkills"]), ([1, 10], [2, 10]))
		self.assertEqual(world["storedSkills"], [1001, 30001], "the Warrior's level-1 autolearn skills")
		# level 10: the Gladiator's rows (2010, then the class-less 30003 and 40009); 9: the Warrior's 1009, then the Gladiator's 2009; 5; 2
		self.assertEqual([(e["skillId"], e["level"], e["class"]) for e in world["learnedSkills"]],
		                 [(2010, 10, "GLADIATOR"), (30003, 10, "GLADIATOR"), (40009, 10, "GLADIATOR"), (1009, 9, "WARRIOR"), (2009, 9, "GLADIATOR"),
		                  (1005, 5, "WARRIOR"), (1002, 2, "WARRIOR")])
		self.assertEqual(world["skills"], [1001, 1002, 1005, 1009, 2009, 2010, 30002, 30003, 40009],
		                 "not 1010 (the Warrior's level-10 row), 2001 (below old_level + 1), 2011 (Asmodian), 2012 (not autolearn), 30001 (swapped)")
		self.assertEqual(world["daevaSwap"], {"removed": 30001, "added": 30002, "level": 1, "smSkillRemove": 30001})
		self.assertEqual((world["skillLevels"]["30002"], world["skillLevels"]["2010"]), (1, 2), "the swap keeps 30001's level; the template's lvl")
		self.assertEqual(world["learnedRecipes"], [800001, 800002], "40009's level-1 autolearn recipes of the Elyos and of PC_ALL")
		self.assertEqual(world["recipesBySkill"], {"40009": [800001, 800002]})

	def test_the_enter_world_from_other_levels(self):
		asmodian = self.daeva(player_race="ASMODIANS")["enterWorld"]
		self.assertIn(2011, asmodian["skills"])
		self.assertEqual([(e["skillId"], e["level"]) for e in asmodian["learnedSkills"][:4]], [(2011, 10), (2010, 10), (30003, 10), (40009, 10)],
		                 "getTemplatesFor: the Asmodian row 2011 before the PC_ALL rows, although skill_tree lists 2010 first")
		self.assertEqual(asmodian["learnedRecipes"], [800002, 800003])
		nine = self.daeva(daeva_old_level=9)["enterWorld"]
		self.assertEqual(nine["learnNewSkills"], [10, 10])
		self.assertEqual(nine["storedSkills"], [1001, 1002, 1005, 1009, 30001])
		self.assertEqual([e["skillId"] for e in nine["learnedSkills"]], [2010, 30003, 40009],
		                 "the Gladiator's level-9 row 2009 is NOT learned: learnNewSkills(10, 10) starts at old_level + 1")
		for old in (0, 10, 11):  # the stored skills are a starting class's: levels 1-9
			with self.subTest(old=old), self.assertRaises(OracleError):
				self.daeva(daeva_old_level=old)
		with self.assertRaises(OracleError):
			self.report(daeva_class="WARRIOR")  # a starting class is never a Daeva
		with self.assertRaises(OracleError):
			economy_report(self.data, JAVA_SRC, self.config, map_id=2, rules=self.rules, daeva_class="GLADIATOR")  # no CraftContext

	def craft(self, **kwargs):
		return self.report(craft_recipe=900001, craft_map=2, craft_tool=150000009, **kwargs)["craft"]

	def test_the_master_and_the_question(self):
		craft = self.craft()
		self.assertEqual((craft["master"]["npcId"], craft["learn"]["dialogAction"], craft["learn"]["supported"], craft["learn"]["minCharacterLevel"]),
		                 (203784, 46, True, 10))
		self.assertEqual(craft["learn"]["cost"], 3500, "Profession.getUpgradeCost(0), no price factor")
		code = 280383 << 1 | 1
		self.assertEqual(craft["learn"]["question"], {"id": 900852, "params": [{"l10nId": 280383, "utf16": [36, code & 0xFFFF, code >> 16]}, "3500", ""],
		                                              "senderId": 0, "range": 0},
		                 "SM_QUESTION_WINDOW(STR_CRAFT_ADDSKILL_CONFIRM, 0, 0, ChatUtil.l10n(nameId), \"3500\")")
		animation = {"packet": "SM_ACTION_ANIMATION", "to": "everyone, the player too", "animation": "CRAFT_LEVEL_UP", "id": 4, "levelOrObjectId": 0}
		self.assertEqual(craft["learn"]["yes"], {"kinahDelta": -3500, "skill": {"skillId": 40001, "level": 1}, "learnedRecipes": [900001],
		                                         "animations": [animation]},
		                 "the Asmodian twin 900003 is not learned; onLearnSkill's level 1 of a crafting skill (not of a tapping one) animates")
		arm = next(a for a in craft["master"]["talk"]["functions"] if a["action"] == 46)
		self.assertEqual((arm["name"], arm["question"]), ("COMBINE_SKILL_LEVELUP", 900852))
		self.assertEqual(craft["seedSpot"], {"worldId": 2, "x": 102.0, "y": 100.0, "z": 10.0}, "the master's near spot, 2 m along +x")

	def test_the_vendors_and_the_exact_kinah(self):
		craft = self.craft()
		shell, salt = craft["components"]
		self.assertEqual((shell["itemId"], shell["vendors"], shell["vendor"]), (700010, [], None))
		self.assertEqual(craft["seedItems"], [{"itemId": 700010, "count": 1}], "no vendor sells the shell: the gate seeds it")
		self.assertEqual([(v["npcId"], v["kinah"], v["distanceFromMaster"]) for v in salt["vendors"]], [(900020, 140, 8.0), (900021, 70, 30.0)],
		                 "getBuyPrice(50) = 70 through 100/125/100/113 %; times 2, times the sell_price_rate 100 or 50")
		self.assertEqual(salt["vendor"], 900020, "the vendor nearest the master, not the cheapest")
		self.assertEqual((craft["exactKinah"], craft["kinahParts"]), (3640, {"learn": 3500, "components": 140}))
		self.assertEqual(craft["recipe"]["cmCraftMaterials"], {"700010": 1, "700011": 2})
		self.assertEqual((craft["recipe"]["product"]["itemId"], craft["recipe"]["xpReward"], craft["recipe"]["skillLevelAfter"]), (700001, 141, 2))

	def test_the_craft_updates(self):
		recipe = self.craft()["recipe"]
		# analyzeInteraction at the level difference 0 and a COMMON product (bonusModifier 1): 900 - 0 * 30 and max(500, 1200 - 0 * 30)
		self.assertEqual((recipe["executionSpeed"], recipe["showBarDelay"]), (900, 1200))
		self.assertEqual(recipe["updates"], {
			"init": {"action": 0, "success": 1000, "failure": 1000, "executionSpeed": 0, "delay": 0},
			"start": {"action": 1, "success": 0, "failure": 0, "executionSpeed": 0, "delay": 0},
			"success": {"action": 5, "success": 1000, "executionSpeed": 0, "delay": 0},
			"cancel": {"action": 4, "success": 0, "failure": 0, "executionSpeed": 0, "delay": 0}},
		                 "onInteractionStart's pair, onSuccessFinish's (its failure bar is the running value), sendCancelCraft's: all speed 0, delay 0")
		self.assertEqual(recipe["skillUpAnimations"], [], "the craft's level-up is to 2, no animation level (1, 100, 200, 300, 400, 450, 500)")

	def test_the_tool_spots(self):
		craft = self.craft(craft_distances=[3, 5.2, 5.3, 7, 10, 12])
		tool = craft["tool"]
		self.assertEqual([(t["staticId"], t["distanceFromMaster"]) for t in tool["tools"]], [(7, 4.0), (8, 10.0)], "every spot of the STATIC group")
		self.assertEqual((tool["chosen"]["staticId"], tool["checkCraftRange"], tool["packetRange"]), (7, 5.25, 10.0))
		self.assertEqual([(s["distance"], s["x"], s["inPacketRange"], s["inCheckCraftRange"]) for s in tool["spots"]],
		                 [(3, 107.0, True, True), (5.2, f32(109.2), True, True), (5.3, f32(109.3), True, False), (7, 111.0, True, False),
		                  (10, 114.0, False, False), (12, 116.0, False, False)],
		                 "checkCraft: 5 + the player's 0.25 (5.2 in, 5.3 out); CM_CRAFT: centre to centre, strictly below 10")
		self.assertEqual([s["outcome"] for s in tool["spots"][2:5]],
		                 ["STR_COMBINE_TOO_FAR_FROM_TOOL and the cancel pair", "STR_COMBINE_TOO_FAR_FROM_TOOL and the cancel pair", "nothing (CM_CRAFT returns)"])
		turned = self.craft(direction=180.0)["tool"]["spots"][0]
		self.assertEqual((turned["x"], turned["otherToolsInCheckCraftRange"]), (101.0, []))
		# along +y the master's band spot (the middle of [6, 6.6) = 106.3) lies 1.7 m and its near spot (102) 6 m from the vendor at y 108, both
		# inside the vendor's 5 + 1 + 0.35 + 0.25 = 6.6
		north = self.craft(direction=90.0)
		self.assertEqual((north["master"]["talk"]["bandSpot"]["otherNpcsInTalkRange"], north["master"]["talk"]["nearSpot"]["otherNpcsInTalkRange"]),
		                 ([900020], [900020]))
		self.assertEqual(craft["master"]["talk"]["bandSpot"]["otherNpcsInTalkRange"], [], "along +x the vendor is 8 m away")

	def test_the_vendor_and_tool_choices(self):
		# map 5: the master 830058 lacks COMBINE_SKILL_LEVELUP in its func_dialogs; 900022 (4 m) has the salt's goods list but no BUY function,
		# 900023 (3 m) only walks, 900020 stands 8 m away; a regular spawn of the oven's template stands 1 m from the master, and the STATIC
		# group lists its far spot (8, 10 m) before its near one (7, 4 m)
		craft = self.report(craft_recipe=900001, craft_map=5, craft_tool=150000009)["craft"]
		self.assertEqual((craft["master"]["npcId"], craft["learn"]["supported"]), (830058, False),
		                 "the question needs 46 in the master's func_dialogs (CM_DIALOG_SELECT's npc gate audits it otherwise)")
		self.assertEqual(craft["learn"]["cost"], 3500, "the price is the profession's, whoever the master")
		salt = craft["components"][1]
		self.assertEqual([(v["npcId"], v["kinah"], v["distanceFromMaster"]) for v in salt["vendors"]], [(900020, 140, 8.0)],
		                 "not 900022 (canSell false: CM_BUY_ITEM ignores the buy), not 900023 (no fixed spot), not 900021 (not on map 5)")
		self.assertEqual((salt["vendor"], craft["exactKinah"]), (900020, 3640))
		tool = craft["tool"]
		self.assertEqual([(t["staticId"], t["distanceFromMaster"]) for t in tool["tools"]], [(7, 4.0), (8, 10.0)],
		                 "only the handler=\"STATIC\" group's spots (the regular spawn is no StaticObject), nearest the master first")
		self.assertEqual(tool["chosen"]["staticId"], 7)

	def test_refusals(self):
		with self.assertRaises(OracleError):
			self.report(craft_recipe=900001, craft_map=3, craft_tool=150000009)  # no master on map 3
		with self.assertRaises(OracleError):
			self.report(craft_recipe=900001, craft_map=4, craft_tool=150000009)  # a tool group with a pool
		with self.assertRaises(OracleError):
			self.report(craft_recipe=900001, craft_map=2)  # no --craft-tool
		with self.assertRaises(OracleError):
			self.report(craft_recipe=900001, craft_map=2, craft_tool=700001)  # no STATIC group of the template
		with self.assertRaises(OracleError):
			self.report(craft_recipe=900003, craft_map=2, craft_tool=150000009)  # an Asmodian recipe for the Elyos
		with self.assertRaises(OracleError):
			self.report(craft_recipe=900004, craft_map=2, craft_tool=150000009)  # the morph skill
		with self.assertRaises(OracleError):
			self.report(craft_recipe=999999, craft_map=2, craft_tool=150000009)

	def socket(self, item: int, stone: int, config=None, membership: int = 0) -> dict:
		return self.report(config, item_ids=[item], manastones=[stone], membership=membership)["items"][0]["socketing"][0]

	def test_socketing_a_manastone(self):
		tunic = self.socket(110000001, 167000010)
		self.assertEqual((tunic["canAct"], tunic["fits"], tunic["slotLevel"], tunic["socketsRange"]), (True, True, 20, [1, 2]))
		# the default rate 75: 75 + (20 - 10) / 1.75f in float arithmetic
		self.assertEqual((tunic["rate"], tunic["successChance"], tunic["certain"]), (75.0, f32(75.0 + f32(10 / f32(1.75))), False))
		gate = load_config(JAVA_SRC, None, None, GATE_PROFILE + ["gameserver.rates.manastone_chances=200, 50"], self.data, keys=ECONOMY_KEYS)
		self.assertEqual((self.socket(110000001, 167000010, gate)["certain"], self.socket(110000001, 167000010, gate)["successChance"]),
		                 (True, f32(200.0 + f32(10 / f32(1.75)))), "D6's 200: uncapped, so no randomness")
		self.assertEqual(self.socket(110000001, 167000010, gate, membership=1)["rate"], 50.0, "Rates.get: the membership's rate")
		self.assertEqual(self.socket(110000001, 167000010, gate, membership=5)["rate"], 50.0, "min(length - 1, membership)")
		# Rnd.chance() is below 100 always: a chance of 100.71 (95 + 5.71) is certain, one of 99.71 (94 + 5.71) is not
		for rate, certain in ((95, True), (94, False)):
			config = load_config(JAVA_SRC, None, None, GATE_PROFILE + [f"gameserver.rates.manastone_chances={rate}, {rate}"], self.data, keys=ECONOMY_KEYS)
			with self.subTest(rate=rate):
				self.assertEqual(self.socket(110000001, 167000010, config)["certain"], certain)
		rare = self.socket(110000001, 167000011)
		self.assertEqual(rare["successChance"], f32(f32(75.0 * f32(0.8)) + f32(10 / f32(1.75))), "a RARE stone: * 0.8f first")
		# a rate of 0 and a stone of the slot level: 0 + 0 / 1.75f = 0, and Rnd.chance() < 0 never holds
		zero = load_config(JAVA_SRC, None, None, GATE_PROFILE + ["gameserver.rates.manastone_chances=0, 0"], self.data, keys=ECONOMY_KEYS)
		never = self.socket(110000001, 167000020, zero)
		self.assertEqual((never["fits"], never["successChance"], never["impossible"], never["certain"]), (True, 0.0, True, False))
		self.assertEqual((tunic["impossible"], self.socket(110000001, 167000010, zero)["impossible"]), (False, False),
		                 "a chance above 0 (5.71 at the rate 0) is possible")

	def test_socketing_needs_a_weapon_or_an_armour(self):
		# Item.getSockets answers 0 for an item that is neither isWeapon() nor isArmor(), whatever its m_slots (Item.java:611-624): canAct
		# passes (100000901 / 1000000 = 100), the level check passes, then "Manastone socket overload" and the stone is lost
		sword = self.socket(100000901, 167000010)
		self.assertEqual((sword["canAct"], sword["equipType"], sword["fits"], sword["refusedBy"], sword["socketsRange"], sword["auditLog"]),
		                 (True, "NONE", False, "noSocket", [0, 0], "Manastone socket overload"))
		self.assertEqual((sword["message"], sword["stoneConsumed"]), ("STR_GIVE_ITEM_OPTION_FAILED", True))
		self.assertEqual(self.socket(110000001, 167000010)["equipType"], "ARMOR")
		with self.assertRaises(OracleError):  # a stigma on a stigma is StigmaService.chargeStigma (CM_MANASTONE), not modelled
			self.socket(110000006, 167000041)
		self.assertEqual((self.socket(110000001, 167000041)["fits"], self.socket(110000006, 167000010)["fits"]), (True, True),
		                 "a stigma stone on a plain armour, a plain stone on a stigma armour: socketManastone")
		empty = load_config(JAVA_SRC, None, None, GATE_PROFILE + ["gameserver.rates.manastone_chances="], self.data, keys=ECONOMY_KEYS)
		with self.assertRaises(OracleError):  # Rates.get with no rate: 'Missing rates' and 1 (not modelled)
			self.socket(110000001, 167000010, empty)

	def test_socketing_refusals(self):
		self.assertEqual((self.socket(110000001, 167000020)["fits"], self.socket(110000001, 167000021)["refusedBy"]), (True, "stoneLevel"),
		                 "item level 4: slot level 10 * ceil(14 / 10) = 20")
		self.assertEqual((self.socket(110000010, 167000020)["slotLevel"], self.socket(110000010, 167000030)["refusedBy"]), (20, "stoneLevel"),
		                 "item level 10: ceil(20 / 10) = 2, slot level 20")
		self.assertEqual((self.socket(110000011, 167000030)["slotLevel"], self.socket(110000011, 167000030)["fits"]), (30, True),
		                 "item level 11: ceil(2.1) = 3, slot level 30")
		refused = self.socket(110000001, 167000030)
		self.assertEqual((refused["message"], refused["stoneConsumed"]), ("STR_GIVE_ITEM_OPTION_FAILED", True), "the stone is lost after the 2 s")
		bare = self.socket(110000002, 167000010)
		self.assertEqual((bare["fits"], bare["refusedBy"], bare["auditLog"], bare["socketsRange"]), (False, "noSocket", "Manastone socket overload", [0, 0]))
		optional = self.socket(110000003, 167000010)
		self.assertEqual((optional["fits"], optional["needsOptionalSocket"], optional["socketsRange"]), (True, True, [0, 2]),
		                 "a socket only after identification rolled one")
		self.assertEqual(self.socket(110000012, 167000030)["socketsRange"], [5, 6], "capped at MAX_BASIC_STONES")
		earring = self.socket(120000001, 167000010)
		self.assertEqual((earring["canAct"], earring["refusedBy"], earring["stoneConsumed"]), (False, "canAct", False), "an item id / 1000000 of 120")
		self.assertEqual(self.socket(110000001, 168000001)["refusedBy"], "canAct", "a stone id / 1000000 of neither 166 nor 167")
		with self.assertRaises(OracleError):
			self.socket(110000001, 166000001)  # an enchantment stone: arm 1
		with self.assertRaises(OracleError):
			self.report(item_ids=[110000001], manastones=[167000010], membership=-1)

	def test_the_command_line(self):
		base = ["m5c-economy", "--static-data", str(self.tree.root), "--java-src", str(JAVA_SRC), "--java-handlers", str(HANDLERS),
		        "--commons-src", str(COMMONS), "--config", str(CONFIG_DIR), "--map", "2", "--no-profile", "--set", "gameserver.siege.enable=false",
		        "--set", NO_EVENTS]
		out = io.StringIO()
		with contextlib.redirect_stdout(out):
			code = oracle.main(base + ["--item", "110000001", "--manastone", "167000010", "--membership", "1", "--daeva", "GLADIATOR",
			                           "--daeva-old-level", "9", "--craft-recipe", "900001", "--craft-tool", "150000009", "--craft-map", "2",
			                           "--craft-distance", "7", "--craft-distance", "12"])
		self.assertEqual(code, 0)
		answer = json.loads(out.getvalue())
		self.assertEqual(answer["items"][0]["socketing"][0]["fits"], True)
		self.assertEqual(answer["daeva"]["enterWorld"]["learnNewSkills"], [10, 10])
		self.assertEqual((answer["craft"]["exactKinah"], [s["distance"] for s in answer["craft"]["tool"]["spots"]]), (3640, [7.0, 12.0]))
		out = io.StringIO()
		with contextlib.redirect_stdout(out):
			self.assertEqual(oracle.main(base + ["--npc", "203784"]), 0)
		answer = json.loads(out.getvalue())
		self.assertEqual((answer["daeva"], answer["craft"]), (None, None), "no C19 block unless asked")


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5cSanctumSwapTest(unittest.TestCase):
	"""learnNewSkills' Daeva swap when 30001 is missing or 30002 is known already, and a crafting skill among the stored ones (SWAP_SKILL_TREE)."""

	@classmethod
	def setUpClass(cls):
		cls.tree = fixture_tree(SWAP_SKILL_TREE, SWAP_SKILLS)
		cls.data = StaticData(cls.tree.root)
		cls.rules = JavaEconomyRules.read(JAVA_SRC, HANDLERS, COMMONS, SQL)
		cls.config = load_config(JAVA_SRC, None, None, GATE_PROFILE, cls.data, keys=ECONOMY_KEYS)
		cls.craft_ctx = CraftContext.create(cls.data, JAVA_SRC, None, None, [NO_EVENTS])

	@classmethod
	def tearDownClass(cls):
		cls.tree.close()

	def daeva(self, race: str, old_level: int = 1) -> dict:
		return economy_report(self.data, JAVA_SRC, self.config, map_id=2, rules=self.rules, craft_ctx=self.craft_ctx, daeva_class="GLADIATOR",
		                      daeva_old_level=old_level, player_race=race)["daeva"]["enterWorld"]

	def test_30002_known_already(self):
		# the Elyos Warrior learned 30002 at level 5 (learnNewSkills(2, 10) as the starting class): the swap adds nothing and removes 30001
		world = self.daeva("ELYOS")
		self.assertEqual(world["storedSkills"], [1001, 30001])
		self.assertEqual([(e["skillId"], e["level"], e["class"]) for e in world["learnedSkills"]],
		                 [(2010, 10, "GLADIATOR"), (40009, 10, "GLADIATOR"), (30002, 5, "WARRIOR")])
		self.assertEqual(world["daevaSwap"], {"removed": 30001, "added": None, "level": 1, "smSkillRemove": 30001},
		                 "the level is 30001's (getSkillLevel(30001) = 1), not 30002's template lvl 3")
		self.assertEqual(world["skills"], [1001, 2010, 30002, 40009])
		self.assertEqual(world["skillLevels"]["30002"], 3, "30002 keeps the level it was learned at: addSkill is not called again")

	def test_no_30001_no_swap(self):
		# the Asmodian Warrior never held 30001: `isSkillPresent(30001)` is false, so nothing is added or removed
		world = self.daeva("ASMODIANS")
		self.assertEqual(world["storedSkills"], [1001])
		self.assertIsNone(world["daevaSwap"])
		self.assertEqual(world["skills"], [1001, 2010, 40002, 40009], "no 30002")
		self.assertEqual(world["recipesBySkill"], {"40009": [800002, 800003], "40002": []}, "40002 is learned at level 3, after old_level 1")

	def test_a_stored_crafting_skill_is_refused(self):
		with self.assertRaises(OracleError):  # 40002 was learned at level 3 before the seed: its recipes would be known already (not modelled)
			self.daeva("ASMODIANS", old_level=3)
		self.assertIsNone(self.daeva("ASMODIANS", old_level=2)["daevaSwap"], "old level 2: 40002 is learned, not stored")


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5cSanctumRealDataTest(unittest.TestCase):
	"""The gate's Daeva, Sanctum craft and armour on the real data under the gate profile (m5c-plan.md §10.1-§10.3)."""

	@classmethod
	def setUpClass(cls):
		cls.data = StaticData(runner.DEFAULT_STATIC_DATA)
		cls.config = load_config(JAVA_SRC, CONFIG_DIR, None, C19_PROFILE + ["gameserver.rates.manastone_chances=200, 200"], cls.data, keys=ECONOMY_KEYS)
		cls.craft_ctx = CraftContext.create(cls.data, JAVA_SRC, CONFIG_DIR, None, C19_PROFILE)
		cls.report = economy_report(cls.data, JAVA_SRC, cls.config, item_ids=[110100355, 113100293, 114100311, 100901051], manastones=[167000226],
		                            player_class="MAGE", level=4, craft_ctx=cls.craft_ctx, daeva_class="GLADIATOR", daeva_old_level=2,
		                            craft_recipe=155001381, craft_tool=150000009)

	def test_the_daeva(self):
		daeva = self.report["daeva"]
		self.assertEqual((daeva["seed"]["exp"], daeva["seed"]["quest"]["id"], daeva["level"], daeva["levelWithoutQuest"]), (126069, 1006, 10, 9))
		world = daeva["enterWorld"]
		self.assertTrue({30002, 30003, 40009} <= set(world["skills"]), "X21a")
		self.assertNotIn(30001, world["skills"])
		self.assertEqual(world["learnedRecipes"], [155000001, 155000002, 155000005], "X21a: the Elyos morph recipes (recipe_templates.xml:3-27)")
		self.assertEqual(world["daevaSwap"]["smSkillRemove"], 30001)
		self.assertEqual((world["skillLevels"]["169"], world["skillLevels"]["30002"]), (2, 1),
		                 "X21a: Boost Physical Attack II at its template's lvl 2 (skill_templates.xml:2193); 30002 at 30001's level 1")

	def test_the_sanctum_craft(self):
		craft = self.report["craft"]
		self.assertEqual((craft["master"]["npcId"], craft["learn"]["cost"], craft["learn"]["question"]["id"], craft["learn"]["yes"]["learnedRecipes"]),
		                 (203784, 3500, 900852, [155001381]), "X17: Hestia, 3,500, exactly one SM_LEARN_RECIPE")
		salt = craft["components"][1]
		self.assertEqual((salt["itemId"], salt["vendor"], salt["vendors"][0]["kinah"], salt["vendors"][0]["distanceFromMaster"]), (169400096, 203785, 140, 7.688),
		                 "Luelas, 7.7 m from Hestia (m5c-plan.md §2.10)")
		self.assertEqual((craft["exactKinah"], craft["seedItems"]), (3640, [{"itemId": 152001001, "count": 1}]), "C19's exact kinah and the Inina seed")
		tool = craft["tool"]
		self.assertEqual(sorted(t["staticId"] for t in tool["tools"]), [38, 103, 104, 118], "spawns/Statics/110010000_Sanctum.xml:48-53")
		# the file lists 104, 38, 103, 118: the nearest to Hestia is chosen, not the first
		self.assertEqual([(t["staticId"], t["distanceFromMaster"]) for t in tool["tools"]], [(103, 5.447), (118, 5.561), (38, 5.777), (104, 6.572)])
		self.assertEqual(tool["chosen"]["staticId"], 103)
		self.assertEqual([s["outcome"] for s in tool["spots"]],
		                 ["CraftingTask", "STR_COMBINE_TOO_FAR_FROM_TOOL and the cancel pair", "nothing (CM_CRAFT returns)"], "X18, X19")
		self.assertEqual((craft["recipe"]["steps"]["fewest"], craft["recipe"]["steps"]["most"], craft["recipe"]["xpReward"]), (4, 14, 141), "X19, X20")
		self.assertEqual((craft["recipe"]["executionSpeed"], craft["recipe"]["showBarDelay"], craft["recipe"]["skillUpAnimations"]), (900, 1200, []),
		                 "X20: Roast Inina is COMMON, Cooking 1 against the skillpoint 1; X19: the level-up to 2 has no animation")
		self.assertEqual([a["id"] for a in craft["learn"]["yes"]["animations"]], [4], "X17: CRAFT_LEVEL_UP at Cooking 1")

	def test_the_manastone_fits_the_robe_pieces(self):
		for item in self.report["items"][:3]:
			with self.subTest(item=item["itemId"]):
				socket = item["socketing"][0]
				self.assertEqual((socket["fits"], socket["certain"], socket["needsOptionalSocket"], socket["slotLevel"]), (True, True, False, 20),
				                 "C16 / X24: one m_slot, a level-10 stone on a level-4 piece, D6's chance 200")

	def test_an_extraction_sword_has_no_socket(self):
		# [Event] Extraction Greatsword (item_templates.xml:87392): EXTRACT_SWORD's EquipType is NONE, so Item.getSockets answers 0
		sword = self.report["items"][3]
		self.assertEqual((sword["itemId"], sword["equipType"]), (100901051, "NONE"))
		socket = sword["socketing"][0]
		self.assertEqual((socket["canAct"], socket["fits"], socket["refusedBy"], socket["socketsRange"]), (True, False, "noSocket", [0, 0]))


if __name__ == "__main__":
	unittest.main()
