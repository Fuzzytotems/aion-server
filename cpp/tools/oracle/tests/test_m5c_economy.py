"""M5c economy oracle (m5c/economy.py, m5c-plan.md G-01): the arithmetic alone (the service price, soul healing, the talk range with both bound
radii and the float band only its "+ 1" admits), the Java tables and members as they stand today and on an edited copy, the whole report on a
small static_data tree (every start window, the out-of-range message, the cube arms, a letter, extraction, identification and equipping with
every check equipItem makes that the oracle models - class, level, maximum level, race, the equip skills a class learns and the slot), and the
gate's npcs and items on the real data (m5c-plan.md §10.3 X2, X3, X13, X15, X23, X25-X27, C15).

Expected values are derived by hand from the Java sources named in m5c/economy.py and repeated per case. Everything that reads the Java source
tree is skipped without it, like the M5c trade tests.
"""

import contextlib
import dataclasses
import io
import json
import shutil
import tempfile
import unittest
from pathlib import Path

from m5a.data import StaticData
from m5a.javafloat import f32, in_range
from m5c.economy import (COMMONS_MEMBERS, ECONOMY_KEYS, HANDLER_MEMBERS, JAVA_SOURCES, JavaEconomyRules, _spot_at, band_spot, economy_report,
                         learned_skills, recovery_price, service_price, talk_limits)
from m5c.trade_config import load_config
from staticdata_oracle import OracleError
from staticdata_oracle import run as runner

import oracle

from .support import Tree

JAVA_SRC = runner.TOOL_DIR.parents[2] / "game-server" / "src"
CONFIG_DIR = runner.TOOL_DIR.parents[2] / "game-server" / "config"
HANDLERS = JAVA_SRC.parent / "data" / "handlers"
COMMONS = runner.TOOL_DIR.parents[2] / "commons" / "src"
SQL = JAVA_SRC.parent / "sql" / "aion_gs.sql"
HAVE_JAVA_TREE = ((JAVA_SRC / "com" / "aionemu" / "gameserver").is_dir() and (HANDLERS / "ai").is_dir() and (COMMONS / "com").is_dir()
                  and SQL.is_file() and runner.DEFAULT_STATIC_DATA.is_dir())
BASE = ("com", "aionemu", "gameserver")
GATE_PROFILE = ["gameserver.siege.enable=false", "gameserver.limits.enable=false"]
PRICES = {"globalPrices": 125, "globalPricesModifier": 100, "taxes": 113}


class M5cEconomyFormulaTest(unittest.TestCase):
	"""The arithmetic of PricesService.getPriceForService, DialogService's RECOVERY arm and PositionUtil.isInTalkRange, without any Java source."""

	def test_the_service_price_truncates_three_times(self):
		# Seril's removal (X25): 650 * 1.25 = 812.5 -> 812, * 1.00, * 1.13 = 917.56 -> 917
		self.assertEqual(service_price(650, PRICES), 917)
		self.assertEqual(service_price(650, {**PRICES, "taxes": 112}), 909, "taxes rounded down (the §10.4 mutation) move it to 909")
		# X13's letter: 10 + 2 + 25 = 37 -> 46.25 = 46 -> 51.98 = 51; the 10-kinah letter: 10 -> 12.5 = 12 -> 13.56 = 13
		self.assertEqual((service_price(37, PRICES), service_price(10, PRICES)), (51, 13))
		self.assertEqual(service_price(37, {**PRICES, "taxes": 112}), 51, "46 * 1.12 = 51.52: X13 cannot catch the rounding (§10.4)")
		self.assertEqual(service_price(3, PRICES), 3, "3.75 -> 3 before the taxes: 3.39 -> 3")

	def test_the_recovery_price(self):
		# X15: 0.25 - 0.00000015 * 1000 = 0.24985 (double), 1000 * 0.24985 = 249.85 -> 249
		self.assertEqual(recovery_price(1000), (0.25 - 0.00000015 * 1000, 249))
		self.assertEqual(recovery_price(1), (0.25 - 0.00000015, 0), "0.2499998 truncates to 0")
		self.assertEqual(recovery_price(999999)[1], 100000, "the last value of the sliding factor: 999999 * 0.10000015 = 100000.0499...")
		self.assertEqual(recovery_price(500000)[1], 87500, "500000 * 0.175 = 87500")
		self.assertEqual(recovery_price(1000000), (0.1, 100000), "from 1,000,000 on the factor is 0.1")
		self.assertEqual(recovery_price(20000000)[1], 2000000)

	def test_the_talk_range_adds_both_bound_radii_in_float(self):
		# minalinerk: talk 5, bound max(0.595, 0.3774) = 0.595; the player's max(0.25, 0.25): (5 + 1) + 0.595f + 0.25f
		limits = talk_limits(5, f32(0.595), f32(0.25))
		self.assertEqual(limits["talkRange"], 6.0)
		self.assertEqual(limits["limit"], f32(f32(6 + f32(0.595)) + f32(0.25)))
		self.assertEqual(limits["limitWithoutPlusOne"], f32(f32(5 + f32(0.595)) + f32(0.25)))
		self.assertEqual(limits["limitCenterToCenter"], 6.0)
		npc = (f32(851.671), f32(1252.67), f32(118.833))
		band = band_spot(npc, limits)
		self.assertTrue(in_range(*npc, *band, limits["limit"]), "isInTalkRange admits the band spot")
		self.assertFalse(in_range(*npc, *band, limits["limitWithoutPlusOne"]), "without the + 1 it is refused")
		self.assertFalse(in_range(*npc, *band, limits["limitCenterToCenter"]), "centre to centre (no bound radii) it is refused too")
		self.assertEqual(band[1:], npc[1:], "along +x at the npc's y and z")

	def test_the_band_lies_past_both_other_ranges(self):
		# with bound radii of 0.25 the "+ 1" band [2.25, 3.25) begins inside the centre-to-centre range 3: the spot lies in [3, 3.25)
		band = band_spot((0.0, 0.0, 0.0), talk_limits(2, 0.0, f32(0.25)))
		self.assertEqual(band, (3.125, 0.0, 0.0))
		self.assertEqual(band_spot((0.0, 0.0, 0.0), talk_limits(2, f32(0.35), f32(0.25))), (f32(3.3), 0.0, 0.0), "0.6: the band [3, 3.6) past 3")
		with self.assertRaises(OracleError):  # without any bound radius the band is [3, 3): nothing only the "+ 1" admits
			band_spot((0.0, 0.0, 0.0), talk_limits(2, 0.0, 0.0))

	def test_the_direction_turns_the_spot(self):
		self.assertEqual(_spot_at((10.0, 20.0, 30.0), 2.0), (12.0, 20.0, 30.0))
		x, y, z = _spot_at((10.0, 20.0, 30.0), 2.0, 90.0)
		self.assertAlmostEqual(x, 10.0, places=5)
		self.assertEqual((y, z), (22.0, 30.0))


def copy_java_tree(root: Path) -> tuple[Path, Path, Path, Path]:
	"""The Java files, AI classes, Rnd.java and aion_gs.sql the economy rules read, copied below root."""
	root = Path(root)
	java, handlers, commons, sql = root / "src", root / "handlers", root / "commons", root / "sql" / "aion_gs.sql"
	for source, target, relatives in ((JAVA_SRC.joinpath(*BASE), java.joinpath(*BASE), JAVA_SOURCES),
	                                  (HANDLERS, handlers, tuple(r for r, _, _ in HANDLER_MEMBERS)),
	                                  (COMMONS / "com" / "aionemu" / "commons", commons / "com" / "aionemu" / "commons", tuple(r for r, _, _ in COMMONS_MEMBERS))):
		for relative in relatives:
			(target / relative).parent.mkdir(parents=True, exist_ok=True)
			shutil.copyfile(source / relative, target / relative)
	sql.parent.mkdir(parents=True, exist_ok=True)
	shutil.copyfile(SQL, sql)
	return java, handlers, commons, sql


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5cEconomyJavaRulesTest(unittest.TestCase):
	"""The tables the report reads from the Java sources, as they stand today."""

	def test_tables(self):
		rules = JavaEconomyRules.read(JAVA_SRC, HANDLERS, COMMONS, SQL)
		self.assertEqual(rules.dialog_actions, {"BUY": 2, "SELL": 3, "RECOVERY": 35, "REMOVE_ITEM_OPTION": 42, "EXTEND_INVENTORY": 47, "OPEN_POSTBOX": 38})
		self.assertEqual((rules.pages["MAIL"], rules.pages["REMOVE_MANASTONE"], rules.page_by_action[42], rules.page_by_action[38]), (18, 20, 20, 18))
		self.assertEqual(rules.mailbox_states, {"CLOSED": 0, "REGULAR": 1, "EXPRESS": 2})
		self.assertEqual(rules.questions, {"STR_ASK_RECOVER_EXPERIENCE": 160011, "STR_WAREHOUSE_EXPAND_WARNING": 900686})
		self.assertEqual((rules.messages["STR_DIALOG_TOO_FAR_TO_TALK"], rules.messages["STR_WAREHOUSE_TOO_FAR_FROM_NPC"],
		                  rules.messages["STR_REMOVE_ITEM_OPTION_SUCCEED"], rules.messages["STR_MSG_ITEM_IDENTIFY_SUCCEED"]),
		                 (1300346, 1300419, 1300473, 1401626))
		self.assertEqual(rules.player_bound, 0.25)
		self.assertEqual(rules.stones, {"ALPHA": (20, "RARE"), "BETA": (40, "LEGEND"), "GAMMA": (55, "UNIQUE"), "DELTA": (60, "EPIC"),
		                                "EPSILON": (65, "MYTHIC"), "OMEGA": (65, "MYTHIC")})
		self.assertEqual(rules.effective_level_bonus, {"COMMON": 5, "RARE": 5, "LEGEND": 10, "UNIQUE": 15, "EPIC": 20, "MYTHIC": 25})
		self.assertEqual(rules.removal_base, 650)
		self.assertEqual({k: v for k, v in rules.mail.items() if k != "qualityRates"},
		                 {"expressBaseCost": 500, "normalBaseCost": 10, "expressCostFactor": 5, "normalCostFactor": 1, "kinahRate": f32(0.01)})
		self.assertEqual(rules.mail["qualityRates"], {"MYTHIC": f32(0.05), "EPIC": f32(0.05), "UNIQUE": f32(0.04), "LEGEND": f32(0.04),
		                                              "RARE": f32(0.03), "default": f32(0.02)})
		self.assertEqual(rules.tune_count_sql_default, 0, "sql/aion_gs.sql: `tune_count` smallint NOT NULL DEFAULT '0'")
		# ItemGroup.getRequiredSkills (ItemGroup.java:16, 30, 32, 37, 42, 47, 52, 57): a Mage learns only 40, 100 and 103 (skill_tree.xml:45, 96, 99)
		self.assertEqual({group: rules.required_skills[group] for group in ("SWORD", "SHIELD", "TORSO", "RB_TORSO", "CL_TORSO", "LT_TORSO", "CH_TORSO",
		                                                                     "PL_TORSO", "NOWEAPON", "EARRING", "NONE")},
		                 {"SWORD": (37, 44), "SHIELD": (43, 50), "TORSO": (103, 106), "RB_TORSO": (103, 106), "CL_TORSO": (40,), "LT_TORSO": (41, 48),
		                  "CH_TORSO": (42, 49), "PL_TORSO": (54,), "NOWEAPON": (), "EARRING": (), "NONE": ()})
		self.assertEqual(len(rules.required_skills), len(rules.enums.item_groups), "every ItemGroup constant")


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5cEconomyJavaShapeTest(unittest.TestCase):
	"""JavaEconomyRules.read on a copy of the files, one of them changed: every modelled member is fingerprinted, the literals are read."""

	@classmethod
	def setUpClass(cls):
		cls.root = tempfile.TemporaryDirectory()
		cls.java, cls.handlers, cls.commons, cls.sql = copy_java_tree(Path(cls.root.name))

	@classmethod
	def tearDownClass(cls):
		cls.root.cleanup()

	def read(self):
		return JavaEconomyRules.read(self.java, self.handlers, self.commons, self.sql)

	def changed(self, path: Path, old: str, new: str) -> None:
		"""One edit in the copied tree, which the next call (or the end of the test) takes back."""
		self.doCleanups()
		original = path.read_bytes()
		text = original.decode("utf-8").replace("\r\n", "\n")
		self.assertIn(old, text, f"{path} no longer contains the text this case changes")
		path.write_bytes(text.replace(old, new, 1).encode("utf-8"))
		self.addCleanup(path.write_bytes, original)

	def game(self, relative: str) -> Path:
		return self.java.joinpath(*BASE) / relative

	def test_the_copy_reads(self):
		self.assertEqual(self.read().removal_base, 650)

	def test_a_changed_rule_is_refused(self):
		cases = [
			(self.game("utils/PositionUtil.java"), "float range = npc.getObjectTemplate().getTalkDistance() + 1;",
			 "float range = npc.getObjectTemplate().getTalkDistance() + 2;"),
			(self.game("utils/PositionUtil.java"), "return dx * dx + dy * dy + dz * dz < range * range;", "return dx * dx + dy * dy + dz * dz <= range * range;"),
			(self.game("services/DialogService.java"), "final double factor = (expLost < 1000000 ? 0.25 - (0.00000015 * expLost) : 0.1);",
			 "final double factor = (expLost < 1000000 ? 0.3 - (0.00000015 * expLost) : 0.1);"),
			(self.game("services/DialogService.java"), "PacketSendUtility.sendPacket(player, new SM_DIALOG_WINDOW(npc.getObjectId(), DialogPage.getByActionId(dialogActionId).id()));",
			 "PacketSendUtility.sendPacket(player, new SM_DIALOG_WINDOW(npc.getObjectId(), dialogActionId));"),
			(self.game("services/CubeExpandService.java"), "int newNpcExpansions = player.getNpcExpands() + 1;", "int newNpcExpansions = player.getNpcExpands() + 2;"),
			(self.game("services/trade/PricesService.java"), "* getTaxes(playerRace) / 100D);\n\t}\n\n\t/**\n\t * @return The calculated price after taxes",
			 "* getTaxes(playerRace) / 99D);\n\t}\n\n\t/**\n\t * @return The calculated price after taxes"),
			(self.game("services/EnchantService.java"), "ItemService.addItem(player, stoneId, itemTemplate.isWeapon() ? Rnd.get(2, 5) : Rnd.get(1, 3));",
			 "ItemService.addItem(player, stoneId, itemTemplate.isWeapon() ? Rnd.get(2, 6) : Rnd.get(1, 3));"),
			(self.game("services/EnchantService.java"), "if (itemTemplate.isWeapon())\n\t\t\trndEffectiveLevel += 5;", "if (itemTemplate.isArmor())\n\t\t\trndEffectiveLevel += 5;"),
			(self.game("services/mail/MailService.java"), "kinahMailCommission = (long) (attachedKinah * 0.01f * costFactor);",
			 "kinahMailCommission = (long) (attachedKinah * 0.01f * costFactor) + 1;"),
			(self.game("services/item/ItemActionService.java"), "item.setOptionalSockets(Rnd.get(0, item.getItemTemplate().getOptionSlotBonus()));",
			 "item.setOptionalSockets(Rnd.get(1, item.getItemTemplate().getOptionSlotBonus()));"),
			(self.game("model/templates/item/ItemTemplate.java"), "if (maxEnchantBonus == 0 && optionSlotBonus == 0 && rndBonusId == 0)",
			 "if (maxEnchantBonus == 0 && rndBonusId == 0)"),
			(self.game("model/gameobjects/player/Equipment.java"), "if (requiredLevel == -1 || requiredLevel > owner.getLevel()) {",
			 "if (requiredLevel == -1 || requiredLevel > owner.getLevel() + 1) {"),
			(self.handlers / "ai" / "PostboxAI.java", "player.getMailbox().mailBoxState = PlayerMailboxState.REGULAR;",
			 "player.getMailbox().mailBoxState = PlayerMailboxState.EXPRESS;"),
			(self.handlers / "ai" / "GeneralNpcAI.java", "TalkEventHandler.onTalk(this, player);", "TalkEventHandler.onSimpleTalk(this, player);"),
			(self.commons / "com" / "aionemu" / "commons" / "utils" / "Rnd.java", "(int) nextLong(minInclusive, maxInclusive + 1L);",
			 "(int) nextLong(minInclusive, maxInclusive);"),
			# equipping: the maximum level, the equip skills, how they are learned, and the statements the learn path stands on
			(self.game("model/templates/item/ItemTemplate.java"), "return maxLevelRestrictions[playerClass.ordinal()];",
			 "return maxLevelRestrictions[playerClass.getStartingClass().ordinal()];"),
			(self.game("model/templates/item/ItemTemplate.java"), '@XmlAttribute(name = "restrict_max")', '@XmlAttribute(name = "restrict_maximum")'),
			(self.game("model/gameobjects/player/Equipment.java"), "if (owner.getSkillList().isSkillPresent(skill))",
			 "if (owner.getSkillList().isSkillPresent(skill + 1))"),
			(self.game("model/templates/item/enums/ItemGroup.java"), "this(0, ItemSubType.NONE, new int[] {});",
			 "this(0, ItemSubType.NONE, new int[] { 40 });"),
			(self.game("model/skill/PlayerSkillList.java"), "return skills.containsKey(skillId);", "return skills.containsKey(skillId) || skillId == 40;"),
			(self.game("services/SkillLearnService.java"), "if (level < 10 && playerStartClass != null)", "if (level <= 10 && playerStartClass != null)"),
			(self.game("services/SkillLearnService.java"), "if (!template.isAutolearn())", "if (template.getMinLevel() > 99)"),
			(self.game("dataholders/SkillTreeData.java"), "templates.get(makeHash(playerClass.ordinal(), Race.PC_ALL.ordinal(), level));",
			 "templates.get(makeHash(playerClass.ordinal(), Race.ELYOS.ordinal(), level));"),
			(self.game("skillengine/model/SkillLearnTemplate.java"), "private Race race = Race.PC_ALL;", "private Race race = Race.ELYOS;"),
			(self.game("services/player/PlayerService.java"), "SkillLearnService.learnNewSkills(newPlayer, 1, newPlayer.getLevel());",
			 "SkillLearnService.learnNewSkills(newPlayer, 2, newPlayer.getLevel());"),
			(self.game("controllers/PlayerController.java"), "int minNewLevel = oldLevel < newLevel ? oldLevel + 1 : oldLevel - 1;",
			 "int minNewLevel = oldLevel < newLevel ? oldLevel + 2 : oldLevel - 1;"),
			(self.game("model/PlayerClass.java"), "return startingClass == this;", "return startingClass != null;"),
			# the members the other arms lean on
			(self.game("network/aion/serverpackets/SM_CUBE_UPDATE.java"), "itemsCount = player.getInventory().size();",
			 "itemsCount = player.getInventory().size() + 1;"),
			(self.game("model/gameobjects/Creature.java"), "aiName = SpawnTemplate.NO_AI.equals(spawnTemplate.getAiName()) ? null : spawnTemplate.getAiName();",
			 "aiName = spawnTemplate.getAiName();"),
			(self.game("dataholders/ItemRandomBonusData.java"), "return bonusData.get(statBonusType).get(statBonusSetId);",
			 "return bonusData.get(statBonusType).get(statBonusSetId + 1);"),
			(self.game("model/templates/item/ItemTemplate.java"), "return itemGroup.getEquipType();", "return EquipType.WEAPON;"),
			(self.game("services/CubeExpandService.java"), "expand(player, 1);", "expand(player, 2);"),
			(self.game("services/player/PlayerEnterWorldService.java"),
			 "player.getController().onLevelChange(PlayerDAO.getOldCharacterLevel(player.getObjectId()), player.getLevel());",
			 "player.getController().onLevelChange(player.getLevel(), player.getLevel());"),
		]
		for path, old, new in cases:
			with self.subTest(file=path.name, new=new), self.assertRaises(OracleError):
				self.changed(path, old, new)
				self.read()

	def test_a_comment_is_no_change(self):
		self.changed(self.game("services/EnchantService.java"), "int rndEffectiveLevel = effectiveLevel + Rnd.get(0, 10);",
		             "int rndEffectiveLevel = effectiveLevel /* the roll */ + Rnd.get(0, 10); // inclusive")
		self.assertEqual(self.read().removal_base, 650)

	def test_literals_are_read(self):
		self.changed(self.sql, "`tune_count` smallint NOT NULL DEFAULT '0',", "`tune_count` smallint NOT NULL DEFAULT '-1',")
		self.assertEqual(self.read().tune_count_sql_default, -1)
		self.changed(self.game("network/aion/serverpackets/SM_QUESTION_WINDOW.java"), "STR_ASK_RECOVER_EXPERIENCE = 160011;",
		             "STR_ASK_RECOVER_EXPERIENCE = 160012;")
		self.assertEqual(self.read().questions["STR_ASK_RECOVER_EXPERIENCE"], 160012)
		self.changed(self.game("model/DialogPage.java"), "REMOVE_MANASTONE(DialogAction.REMOVE_ITEM_OPTION, 20),",
		             "REMOVE_MANASTONE(DialogAction.REMOVE_ITEM_OPTION, 21),")
		self.assertEqual(self.read().page_by_action[42], 21)


NPCS = (
	# a function npc like minalinerk (talk 5, bound 0.595) and one with a spot ai override
	'<npc_template npc_id="900001" level="9" name="fixture grocer" ai="general"><stats maxHp="10"/>'
	'<bound_radius front="0.595" side="0.3774" upper="1.16875"/><talk_info distance="5" is_dialog="true" func_dialogs="2 3"/></npc_template>'
	# the postbox: no is_dialog, no func_dialogs, the postbox AI
	'<npc_template npc_id="900002" level="1" name="fixture mailbox" ai="postbox"><stats maxHp="10"/><bound_radius front="0.25" side="0.35" upper="2"/>'
	'<talk_info distance="5"/></npc_template>'
	# Seril's removal, the soul healer and the cube expander
	'<npc_template npc_id="900003" level="20" name="fixture seril" ai="general"><stats maxHp="10"/><bound_radius front="0.25" side="0.35" upper="2"/>'
	'<talk_info distance="5" is_dialog="true" func_dialogs="42"/></npc_template>'
	'<npc_template npc_id="900004" level="45" name="fixture healer" ai="general"><stats maxHp="10"/><bound_radius front="0.25" side="0.35" upper="2"/>'
	'<talk_info distance="3" is_dialog="true" func_dialogs="35"/></npc_template>'
	'<npc_template npc_id="900005" level="9" name="fixture expander" ai="general"><stats maxHp="10"/><bound_radius front="0.595" side="0.3774" upper="1"/>'
	'<talk_info distance="5" is_dialog="true" func_dialogs="47 99"/></npc_template>'
	# a talker without functions, a mute npc, a subdialog npc, a walker, a butler and a second expander with two levels
	'<npc_template npc_id="900006" level="9" name="fixture talker" ai="general"><stats maxHp="10"/><bound_radius front="0.5" side="0.5"/>'
	'<talk_info is_dialog="true"/></npc_template>'
	'<npc_template npc_id="900007" level="9" name="fixture mute" ai="general"><stats maxHp="10"/><bound_radius front="0.5" side="0.5"/></npc_template>'
	'<npc_template npc_id="900008" level="9" name="fixture sub" ai="general"><stats maxHp="10"/><bound_radius front="0.5" side="0.5"/>'
	'<talk_info func_dialogs="2" subdialog_type="LEVEL" subdialog_value="5"/></npc_template>'
	'<npc_template npc_id="900009" level="9" name="fixture walker" ai="general"><stats maxHp="10"/><talk_info func_dialogs="2"/></npc_template>'
	'<npc_template npc_id="900010" level="9" name="fixture butler" ai="butler"><stats maxHp="10"/><bound_radius front="0.5" side="0.5"/>'
	'<talk_info func_dialogs="2"/></npc_template>'
	'<npc_template npc_id="900011" level="9" name="fixture capital expander" ai="general"><stats maxHp="10"/><bound_radius front="0.5" side="0.5"/>'
	'<talk_info func_dialogs="47"/></npc_template>'
	'<npc_template npc_id="900012" level="9" name="fixture silent" ai="general"><stats maxHp="10"/><bound_radius front="0.5" side="0.5"/>'
	'<talk_info func_dialogs="2"/></npc_template>'
	# a town npc (TalkEventHandler.java:29-38 answers by the player's town residence)
	'<npc_template npc_id="900013" level="9" name="fixture villager" ai="general" title_id="462877"><stats maxHp="10"/>'
	'<bound_radius front="0.5" side="0.5"/><talk_info is_dialog="true" func_dialogs="2"/></npc_template>'
)

SPAWNS = """
<spawn_map map_id="1">
	<spawn npc_id="900001"><spot x="100" y="200" z="30" h="1"/></spawn>
	<spawn npc_id="900002"><spot x="400" y="200" z="30" h="1"/><spot x="120" y="200" z="30" h="1"/></spawn>
	<spawn npc_id="900003"><spot x="130" y="200" z="30" h="1"/></spawn>
	<spawn npc_id="900004"><spot x="140" y="200" z="30" h="1"/></spawn>
	<spawn npc_id="900005"><spot x="150" y="200" z="30" h="1"/></spawn>
	<spawn npc_id="900006"><spot x="160" y="200" z="30" h="1"/></spawn>
	<spawn npc_id="900007"><spot x="170" y="200" z="30" h="1"/></spawn>
	<spawn npc_id="900008"><spot x="180" y="200" z="30" h="1"/></spawn>
	<spawn npc_id="900009"><spot x="190" y="200" z="30" h="1" walker_id="w1"/></spawn>
	<spawn npc_id="900010"><spot x="200" y="200" z="30" h="1"/></spawn>
	<spawn npc_id="900011"><spot x="210" y="200" z="30" h="1"/></spawn>
	<spawn npc_id="900012"><spot x="220" y="200" z="30" h="1" ai="postbox"/></spawn>
	<spawn npc_id="900013"><spot x="230" y="200" z="30" h="1"/></spawn>
</spawn_map>
"""

CUBE = ('<expansion_npc ids="900005"><expand level="1" price="1000"/></expansion_npc>'
        '<expansion_npc ids="900011"><expand level="2" price="12000"/><expand level="3" price="80000"/></expansion_npc>')

# a sword (weapon, level 2, option slot 1), a tunic (armour, level 4, restrict 4 for every class), a stone (no slots), a LEGEND blade with an
# enchant bonus and a rnd_bonus, a mage-only robe, a potion and a template whose quality calculateEffectiveLevel does not know
ITEMS = (
	'<item_template id="100" name="fixture sword" level="2" item_group="SWORD" quality="COMMON" price="200" option_slot_bonus="1" '
	'restrict="2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2" max_enchant="10" m_slots="1"/>'
	'<item_template id="101" name="fixture tunic" level="4" item_group="RB_TORSO" quality="COMMON" price="500" option_slot_bonus="1" '
	'restrict="4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4" max_enchant="10" m_slots="1"/>'
	'<item_template id="102" name="fixture stone" level="10" item_group="MANASTONE" quality="COMMON" price="100"/>'
	'<item_template id="103" name="fixture blade" level="48" item_group="SWORD" quality="LEGEND" price="9000" max_enchant_bonus="2" rnd_bonus="7" '
	'option_slot_bonus="2"/>'
	'<item_template id="104" name="fixture robe" level="1" item_group="RB_TORSO" quality="RARE" price="40" restrict="0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 0 0" '
	'rnd_count="0" option_slot_bonus="1"/>'
	'<item_template id="105" name="fixture potion" level="1" quality="COMMON" price="250"/>'
	'<item_template id="106" name="fixture odd" level="1" item_group="SWORD" quality="JUNK" price="1"/>'
	'<item_template id="107" name="fixture epic" level="10" item_group="SWORD" quality="EPIC" price="1" option_slot_bonus="1"/>'
	# equipping: a chain, a leather and a plate torso, a shirt without restrict, a sword for levels 1-3, an Asmodian sword and a warrior-only
	# blade (restrict 1 at WARRIOR, 0 at every other ordinal, GLADIATOR's included)
	'<item_template id="108" name="fixture hauberk" level="1" item_group="CH_TORSO" quality="COMMON" price="1" restrict="1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1"/>'
	'<item_template id="109" name="fixture jerkin" level="1" item_group="LT_TORSO" quality="COMMON" price="1" restrict="1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1"/>'
	'<item_template id="110" name="fixture plate" level="1" item_group="PL_TORSO" quality="COMMON" price="1" restrict="1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1"/>'
	'<item_template id="111" name="fixture shirt" level="1" item_group="CL_TORSO" quality="COMMON" price="1"/>'
	'<item_template id="112" name="fixture capped sword" level="1" item_group="SWORD" quality="COMMON" price="1" '
	'restrict="1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1" restrict_max="3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3"/>'
	'<item_template id="113" name="fixture asmodian sword" level="1" item_group="SWORD" quality="COMMON" price="1" race="ASMODIANS" '
	'restrict="1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1"/>'
	'<item_template id="114" name="fixture warrior blade" level="1" item_group="SWORD" quality="COMMON" price="1" '
	'restrict="1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0"/>'
)

# the equip skills the fixture classes learn (ItemGroup: SWORD 37/44, RB_TORSO 103/106, CL_TORSO 40, LT_TORSO 41/48, CH_TORSO 42/49, PL_TORSO 54)
SKILL_TREE = (
	'<skill skillId="37" minLevel="1" autolearn="true" classId="WARRIOR"/>'
	'<skill skillId="103" minLevel="1" autolearn="true" classId="WARRIOR"/>'
	'<skill skillId="103" minLevel="1" autolearn="true" classId="MAGE"/>'
	# a starting class's row at level 10: its advanced classes do not learn it (learnNewSkills takes the starting class below 10 only)
	'<skill skillId="54" minLevel="10" autolearn="true" classId="WARRIOR"/>'
	# leather for an Asmodian mage from level 3: the race and the level filter
	'<skill skillId="41" minLevel="3" autolearn="true" race="ASMODIANS" classId="MAGE"/>'
	# chain for a mage, but not autolearn
	'<skill skillId="42" minLevel="1" classId="MAGE"/>'
	# cloth for every class (a class-less row), and human gathering at 10, which an advanced class's own arm skips
	'<skill skillId="40" minLevel="1" autolearn="true"/>'
	'<skill skillId="30001" minLevel="10" autolearn="true"/>'
)

EXPERIENCE = "".join(f"<exp>{e}</exp>" for e in (0, 400, 1433, 3820, 9054, 17655, 30978, 52010, 82982, 126069, 2162140395))


def fixture_tree() -> Tree:
	tree = Tree()
	tree.minimal({"npc_templates": NPCS, "spawns": SPAWNS, "cube_expander": CUBE, "item_templates": ITEMS, "player_experience_table": EXPERIENCE,
	              "skill_tree": SKILL_TREE})
	return tree


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5cEconomyFixtureReportTest(unittest.TestCase):
	"""The whole report on a small static_data tree (the Java tables come from the Java sources)."""

	@classmethod
	def setUpClass(cls):
		cls.tree = fixture_tree()
		cls.data = StaticData(cls.tree.root)
		cls.rules = JavaEconomyRules.read(JAVA_SRC, HANDLERS, COMMONS, SQL)
		cls.config = load_config(JAVA_SRC, None, None, GATE_PROFILE, cls.data, keys=ECONOMY_KEYS)

	@classmethod
	def tearDownClass(cls):
		cls.tree.close()

	def report(self, config=None, **kwargs):
		return economy_report(self.data, JAVA_SRC, config or self.config, map_id=1, rules=self.rules, **kwargs)

	def config_with(self, *keys):
		return load_config(JAVA_SRC, None, None, GATE_PROFILE + list(keys), self.data, keys=ECONOMY_KEYS)

	def talk(self, *npc_ids, **kwargs):
		return {t["npcId"]: t for t in self.report(npc_ids=list(npc_ids), **kwargs)["talk"]}

	def test_a_function_npc(self):
		grocer = self.talk(900001)[900001]
		self.assertEqual((grocer["talkDistance"], grocer["boundRadius"]["maxOfFrontAndSide"], grocer["playerBoundRadius"]), (5, f32(0.595), 0.25))
		self.assertEqual(grocer["limit"], f32(f32(6.0 + f32(0.595)) + 0.25))
		band = grocer["bandSpot"]
		self.assertEqual((band["inTalkRange"], band["inRangeWithoutPlusOne"], band["inRangeCenterToCenter"]), (True, False, False), "X2's band")
		self.assertEqual((band["y"], band["z"], band["distance"]), (200.0, 30.0, 6.4225), "the middle of [6, 6.845): past talk + 1 and inside the radii")
		self.assertTrue(grocer["nearSpot"]["inTalkRange"])
		self.assertEqual((grocer["farSpot"]["distance"], grocer["farSpot"]["inTalkRange"]), (10.0, False))
		self.assertEqual(grocer["outOfRange"], {"message": "STR_DIALOG_TOO_FAR_TO_TALK", "messageId": 1300346, "window": None}, "an is_dialog npc")
		self.assertEqual((grocer["startWindow"]["ai"], grocer["startWindow"]["page"], grocer["startWindow"]["questId"]), ("GeneralNpcAI", 10, 0))
		self.assertEqual([(f["action"], f["name"]) for f in grocer["functions"]], [(2, "BUY"), (3, "SELL")])

	def test_the_postbox(self):
		talk = self.talk(900001, 900002)
		postbox = talk[900002]
		self.assertEqual((postbox["chosenSpot"]["x"], postbox["chosenSpot"]["distanceFromReference"]), (120.0, 20.0),
		                 "of two spots the one nearest the first npc's")
		self.assertEqual(postbox["startWindow"], {"ai": "PostboxAI", "page": 18, "questId": 0, "pageValue": 1,
		                                          "via": "PostboxAI.handleDialogStart: the mailbox state REGULAR, DialogPage.MAIL"}, "X3")
		self.assertEqual(postbox["outOfRange"]["message"], "STR_WAREHOUSE_TOO_FAR_FROM_NPC", "not an is_dialog npc")
		self.assertEqual(postbox["functions"], [])
		self.assertEqual(postbox["limit"], f32(f32(6.0 + f32(0.35)) + 0.25))
		self.assertEqual(self.talk(900002, near=(399.0, 200.0, 30.0))[900002]["chosenSpot"]["x"], 400.0, "--near picks the other spot")

	def test_the_function_arms(self):
		talk = self.talk(900003, 900004, 900005)
		self.assertEqual(talk[900003]["functions"], [{"action": 42, "name": "REMOVE_ITEM_OPTION", "answer": "SM_DIALOG_WINDOW", "page": 20,
		                                              "via": "DialogService.sendDialogWindow: DialogPage.getByActionId"}], "X25: page 20, not 42")
		self.assertEqual(talk[900004]["functions"][0]["question"], 160011)
		self.assertEqual(talk[900004]["limit"], f32(f32(4.0 + f32(0.35)) + 0.25), "talk distance 3")
		self.assertEqual([f["name"] for f in talk[900005]["functions"]], ["EXTEND_INVENTORY", None], "an arm the oracle does not model is named so")
		self.assertEqual((talk[900003]["farSpot"]["otherNpcsInTalkRange"], talk[900004]["farSpot"]["otherNpcsInTalkRange"]), ([900004], [900005]),
		                 "10 m along +x from each lands on the next npc: the report names the other npcs a spot is in talk range of")
		self.assertEqual(talk[900005]["farSpot"]["otherNpcsInTalkRange"], [])

	def test_other_start_windows(self):
		talk = self.talk(900006, 900007)
		self.assertEqual(talk[900006]["startWindow"]["page"], None, "a talker without functions depends on quests and the daeva state")
		self.assertEqual((talk[900007]["canInteract"], talk[900007]["startWindow"]), (False, None), "no talk_info: no dialog at all")
		self.assertIsNone(talk[900007]["outOfRange"], "onDialogRequest returns before the range check (NpcController.java:252-253): no message")
		self.assertEqual(talk[900006]["outOfRange"]["message"], "STR_DIALOG_TOO_FAR_TO_TALK")

	def test_refused_npcs(self):
		for npc_id, why in ((900008, "subdialog_type"), (900009, "walker"), (900010, "butler"), (900012, "postbox spot ai on a function npc: fine"),
		                    (900013, "a town npc"), (999999, "no template")):
			with self.subTest(npc=npc_id, why=why):
				if npc_id == 900012:
					self.assertEqual(self.talk(npc_id)[npc_id]["startWindow"]["ai"], "PostboxAI", "the spot's ai replaces the template's")
					continue
				with self.assertRaises(OracleError):
					self.talk(npc_id)
		with self.assertRaises(OracleError):
			self.talk(900001, far=1.0)  # a far spot inside the range

	def test_recovery(self):
		recovery = self.report(recover_exp=1000)["recovery"]
		self.assertEqual((recovery["price"], recovery["question"]["id"], recovery["question"]["params"]), (249, 160011, ["249", "", ""]), "X15")
		self.assertEqual(recovery["yes"]["kinahDelta"], -249)
		self.assertEqual(recovery["yes"]["expDelta"], 1000)
		self.assertEqual([m["id"] for m in recovery["yes"]["messages"]], [1370002, 1300674])
		self.assertEqual(self.report(recover_exp=0)["recovery"]["question"], None, "STR_DONOT_HAVE_RECOVER_EXPERIENCE, no question")
		with self.assertRaises(OracleError):
			self.report(recover_exp=-1)

	def test_the_cube(self):
		cube = self.report(npc_ids=[900005])["cube"][0]
		self.assertEqual((cube["price"], cube["question"]["params"][0], cube["yes"]["npcExpandsAfter"]), (1000, "1000", 1), "X26: the raw price")
		self.assertEqual(cube["yes"]["smCubeUpdate"], {"action": 0, "storage": 0, "npcExpands": 1, "questExpands": 0, "itemExpands": 0})
		second = self.report(npc_ids=[900005], npc_expands=1)["cube"][0]
		self.assertEqual(second["answer"], "STR_EXTEND_INVENTORY_CANT_EXTEND_MORE_DUE_TO_MAXIMUM_EXTEND_LEVEL_BY_THIS_NPC", "the Poeta expander has one level")
		capital = self.report(npc_ids=[900011])["cube"][0]
		self.assertEqual(capital["answer"], "STR_EXTEND_INVENTORY_CANT_EXTEND_DUE_TO_MINIMUM_EXTEND_LEVEL_BY_THIS_NPC", "its first level is 2")
		self.assertEqual(self.report(npc_ids=[900011], npc_expands=1)["cube"][0]["price"], 12000)
		limited = self.report(config=self.config_with("gameserver.npcexpands.limit=2"), npc_ids=[900011], npc_expands=2)["cube"][0]
		self.assertEqual(limited["answer"], "STR_EXTEND_INVENTORY_CANT_EXTEND_MORE_DUE_TO_MAXIMUM_EXTEND_LEVEL_BY_THIS_NPC", "NPC_CUBE_EXPANDS_SIZE_LIMIT")
		full = self.report(npc_ids=[900005], quest_expands=6, item_expands=5)["cube"][0]
		self.assertEqual(full["answer"], "STR_EXTEND_INVENTORY_CANT_EXTEND_MORE", "0 + 6 + 5 + 1 = 12 is above the limit 11")
		self.assertEqual(self.report(npc_ids=[900005], quest_expands=5, item_expands=5)["cube"][0]["price"], 1000, "11 is not above it")

	def test_the_removal_and_the_mail(self):
		report = self.report(npc_ids=[900003], mails=["105:5:200", "0:0:10", "100:1:0:express", "0:0:1000"])
		self.assertEqual(report["manastoneRemoval"]["byRace"], {"ELYOS": 917, "ASMODIANS": 917}, "X25")
		potion, kinah, express, thousand = report["mail"]
		self.assertEqual((potion["itemCommission"], potion["kinahCommission"], potion["serviceBase"], potion["byRace"]["ELYOS"]["total"]),
		                 (25, 2, 37, 251), "X13: 250 * 0.02f * 5 = 25, 200 * 0.01f = 2, 10 + 27 -> 51, + 200")
		self.assertEqual((kinah["kinahCommission"], kinah["byRace"]["ELYOS"]["total"]), (0, 23), "10 * 0.01f is just below 0.1: 0")
		self.assertEqual((express["baseCost"], express["costFactor"], express["itemCommission"], express["byRace"]["ELYOS"]["total"]),
		                 (500, 5, 20, service_price(520, PRICES)), "200 * 0.02f * 1 * 5 = 20")
		self.assertEqual(thousand["kinahCommission"], 10, "1000 * 0.01f = 9.99999978 rounds to the float 10.0")
		self.assertEqual(self.report(mails=["0:0:-1"])["mail"][0]["answer"], "nothing (sendMail: an audit for negative kinah)")
		for bad in ("105:5", "105:5:1:fast", "x:1:1"):
			with self.subTest(mail=bad), self.assertRaises(OracleError):
				self.report(mails=[bad])

	def test_extraction(self):
		items = {i["itemId"]: i for i in self.report(item_ids=[100, 101, 102, 103, 107])["items"]}
		sword = items[100]["breakItem"]
		self.assertEqual((sword["effectiveLevel"], sword["rollRange"], sword["countRange"]), (7, [12, 22], [2, 5]), "COMMON + 5, the roll + 5 for a weapon")
		self.assertEqual(sword["stones"], [{"itemId": 166000191, "probability": 1.0}], "X27: Alpha only")
		tunic = items[101]["breakItem"]
		self.assertEqual((tunic["rollRange"], tunic["countRange"]), ([9, 19], [1, 3]), "armour: no + 5, 1..3 stones")
		self.assertFalse(items[102]["breakItem"]["breakable"], "a manastone is neither armour nor a weapon")
		blade = items[103]["breakItem"]  # LEGEND 48: 58 + 0..10 + 5 = 63..73, BETA from 50, GAMMA from 70
		self.assertEqual(blade["stones"], [{"itemId": 166000192, "probability": 7 / 11}, {"itemId": 166000193, "probability": 4 / 11}])
		epic = items[107]["breakItem"]  # EPIC 10: 30..40, all Alpha
		self.assertEqual(epic["rollRange"], [35, 45])
		with self.assertRaises(OracleError):  # calculateEffectiveLevel answers 0: breakItem throws
			self.report(item_ids=[106])

	def test_identification(self):
		items = {i["itemId"]: i["identification"] for i in self.report(item_ids=[100, 102, 103, 104])["items"]}
		sword = items[100]
		self.assertEqual((sword["canTune"], sword["newItemTuneCount"], sword["newItemIdentified"]), (True, -1, False), "option_slot_bonus keeps -1")
		self.assertEqual((sword["sqlDefaultTuneCount"], sword["sqlDefaultLoadsIdentified"], sword["seedTuneCountForUnidentified"]), (0, True, -1), "D5")
		self.assertEqual((sword["optionalSocketsRange"], sword["enchantBonusRange"], sword["statBonusId"]), ([0, 1], [0, 0], 0), "X23")
		self.assertEqual((sword["tuneCountAfter"], sword["animation"]), (0, {"time": 5000, "start": 9, "end": 10, "abort": 11}))
		self.assertEqual((items[102]["canTune"], items[102]["maxTuneCount"]), (False, 0), "no equipment slots: never tunable")
		self.assertEqual((items[103]["enchantBonusRange"], items[103]["statBonusId"]), ([0, 2], None), "a rnd_bonus set is not modelled")
		self.assertEqual((items[104]["canTune"], items[104]["newItemIdentified"]), (False, True), "rnd_count 0 wins over option_slot_bonus")
		# were the SQL default -1, a row written without tune_count would load unidentified - but only for a template that can tune
		# (Item.java:128-130 turns -1 into 0 for the others)
		minus_one = dataclasses.replace(self.rules, tune_count_sql_default=-1)
		loaded = {i["itemId"]: i["identification"] for i in economy_report(self.data, JAVA_SRC, self.config, map_id=1, rules=minus_one,
		                                                                   item_ids=[100, 102])["items"]}
		self.assertEqual((loaded[100]["sqlDefaultTuneCount"], loaded[100]["sqlDefaultLoadsIdentified"]), (-1, False))
		self.assertEqual((loaded[102]["canTune"], loaded[102]["sqlDefaultLoadsIdentified"]), (False, True), "a stone cannot tune: loads identified")

	def equip(self, item_ids, **kwargs):
		return {i["itemId"]: i["equip"] for i in self.report(item_ids=item_ids, **kwargs)["items"]}

	def refused(self, equip):
		return equip["passes"], equip["refusedBy"], equip["message"], equip["messageId"]

	def test_equipping(self):
		items = self.equip([100, 101, 104])
		self.assertEqual((items[101]["passes"], items[101]["requiredLevel"], items[101]["startExpOfRequiredLevel"], items[101]["message"]),
		                 (False, 4, 3820, "STR_CANNOT_USE_ITEM_TOO_LOW_LEVEL_MUST_BE_THIS_LEVEL"), "a level-1 mage cannot wear a level-4 tunic")
		self.assertEqual((items[101]["refusedBy"], items[101]["messageId"]), ("requiredLevel", 1300372))
		self.assertTrue(items[104]["passes"], "restrict 1 at the MAGE ordinal (6), and a mage knows 103")
		self.assertEqual((items[104]["requiredSkills"], items[104]["knownRequiredSkills"], items[104]["race"]), ([103, 106], [103], "ELYOS"))
		at_four = self.equip([101, 104], player_class="WARRIOR", level=4)
		self.assertTrue(at_four[101]["passes"], "a warrior learns 103 at level 1 as well")
		self.assertEqual(self.refused(at_four[104]), (False, "class", "STR_CANNOT_USE_ITEM_INVALID_CLASS", 1300371), "restrict 0 at WARRIOR")
		gladiator = self.equip([104], player_class="GLADIATOR", level=10)[104]
		self.assertEqual((gladiator["classSpecific"], gladiator["passes"]), (False, False), "the starting class WARRIOR's restrict is 0 too")
		templar = self.equip([101], player_class="TEMPLAR", level=10)[101]
		self.assertEqual((templar["classSpecific"], templar["requiredLevel"], templar["passes"], templar["knownRequiredSkills"]), (True, 4, True, [103]),
		                 "a templar knows 103 from its starting class WARRIOR's level 1")
		for kwargs in ({"player_class": "BARD2"}, {"level": 0}, {"player_race": "PC_ALL"}):
			with self.subTest(**kwargs), self.assertRaises(OracleError):
				self.report(item_ids=[101], **kwargs)

	def test_the_class_and_level_arms(self):
		# the warrior blade: restrict 1 at WARRIOR only. A gladiator is class specific through its starting class, but its own restrict 0 is
		# getRequiredLevel -1, which equipItem refuses as too low a level (Equipment.java:75-79)
		blade = self.equip([114], player_class="GLADIATOR", level=10)[114]
		self.assertEqual((blade["classSpecific"], blade["requiredLevel"], blade["startExpOfRequiredLevel"]), (True, -1, None))
		self.assertEqual(self.refused(blade), (False, "requiredLevel", "STR_CANNOT_USE_ITEM_TOO_LOW_LEVEL_MUST_BE_THIS_LEVEL", 1300372))
		self.assertTrue(self.equip([114], player_class="WARRIOR", level=1)[114]["passes"])
		# restrict_max 3: level 3 may wear it, level 4 is refused before the skills are asked (a mage does not know 37 either)
		self.assertEqual(self.equip([112], player_class="WARRIOR", level=3)[112]["maxLevelRestrict"], 3)
		self.assertTrue(self.equip([112], player_class="WARRIOR", level=3)[112]["passes"])
		self.assertEqual(self.refused(self.equip([112], player_class="WARRIOR", level=4)[112]),
		                 (False, "maxLevel", "STR_CANNOT_USE_ITEM_TOO_HIGH_LEVEL", 1400267))
		self.assertEqual(self.equip([112], player_class="MAGE", level=4)[112]["refusedBy"], "maxLevel")
		self.assertEqual(self.equip([100])[100]["maxLevelRestrict"], 0, "no restrict_max: getMaxLevelRestrict answers 0")
		# the shirt has no restrict at all: DEFAULT_LEVEL_RESTRICTION, 1 for every class
		shirt = self.equip([111], player_class="GLADIATOR", level=10)[111]
		self.assertEqual((shirt["classSpecific"], shirt["requiredLevel"], shirt["passes"]), (True, 1, True))

	def test_the_race_and_the_slot(self):
		elyos = self.equip([113], player_class="WARRIOR")[113]
		self.assertEqual(self.refused(elyos), (False, "race", "STR_CANNOT_USE_ITEM_INVALID_RACE", 1300373))
		self.assertEqual(elyos["itemRace"], "ASMODIANS")
		self.assertTrue(self.equip([113], player_class="WARRIOR", player_race="ASMODIANS")[113]["passes"])
		self.assertEqual(self.equip([113], player_class="MAGE")[113]["refusedBy"], "race", "the race is checked before the skills")
		# a potion passes every check before the slot: its group has no equipment slot, and equipItem returns null without a packet
		self.assertEqual(self.refused(self.equip([105])[105]), (False, "itemSlot", None, None))

	def test_the_equip_skills(self):
		# checkAvailableEquipSkills: refused WITHOUT a packet unless the character knows one of the group's skills
		mage = self.equip([100, 108, 109, 110, 111])
		self.assertEqual(self.refused(mage[100]), (False, "requiredLevel", "STR_CANNOT_USE_ITEM_TOO_LOW_LEVEL_MUST_BE_THIS_LEVEL", 1300372))
		self.assertEqual(self.refused(self.equip([100], level=2)[100]), (False, "equipSkill", None, None), "a mage does not know 37 or 44")
		self.assertEqual(self.refused(mage[108]), (False, "equipSkill", None, None), "42 is in the mage's tree, but not autolearn")
		self.assertEqual((mage[109]["refusedBy"], mage[109]["knownRequiredSkills"]), ("equipSkill", []), "41 is the Asmodian mage's")
		self.assertEqual(mage[110]["refusedBy"], "equipSkill")
		self.assertEqual((mage[111]["passes"], mage[111]["knownRequiredSkills"]), (True, [40]), "40 is a class-less row: every class learns it")
		asmodian = {level: self.equip([109], player_race="ASMODIANS", level=level)[109] for level in (2, 3)}
		self.assertEqual((asmodian[2]["refusedBy"], asmodian[3]["passes"], asmodian[3]["knownRequiredSkills"]), ("equipSkill", True, [41]),
		                 "an Asmodian mage learns 41 at level 3")
		# a starting class's row at level 10 is learned by the class itself, not by its advanced classes (learnNewSkills: below 10 only)
		self.assertTrue(self.equip([110], player_class="WARRIOR", level=10)[110]["passes"])
		self.assertEqual(self.equip([110], player_class="TEMPLAR", level=10)[110]["refusedBy"], "equipSkill")
		self.assertTrue(self.equip([100], player_class="GLADIATOR", level=10, player_race="ASMODIANS")[100]["passes"],
		                "37 from the starting class WARRIOR's level 1")

	def test_the_learned_skills(self):
		enums = self.rules.enums
		self.assertEqual(learned_skills(self.data, enums, "ELYOS", "MAGE", 1), {40, 103})
		self.assertEqual(learned_skills(self.data, enums, "ASMODIANS", "MAGE", 3), {40, 41, 103})
		self.assertEqual(learned_skills(self.data, enums, "ELYOS", "WARRIOR", 10), {37, 40, 54, 103, 30001})
		self.assertEqual(learned_skills(self.data, enums, "ELYOS", "TEMPLAR", 10), {37, 40, 103},
		                 "neither the starting class's level-10 rows nor 30001 through the templar's own arm (SkillLearnService.java:88)")
		self.assertEqual(self.report(item_ids=[104], level=3)["character"],
		                 {"class": "MAGE", "race": "ELYOS", "level": 3, "learnedSkills": [40, 103]})
		self.assertIsNone(self.report()["character"]["learnedSkills"], "no --item, no skill tree walk")

	def test_the_command_line(self):
		base = ["m5c-economy", "--static-data", str(self.tree.root), "--java-src", str(JAVA_SRC), "--java-handlers", str(HANDLERS),
		        "--commons-src", str(COMMONS), "--config", str(CONFIG_DIR), "--map", "1"]
		out = io.StringIO()
		with contextlib.redirect_stdout(out):
			code = oracle.main(base + ["--no-profile", "--set", "gameserver.siege.enable=false", "--npc", "900001", "--npc", "900002",
			                           "--recover-exp", "1000", "--mail", "105:5:200", "--item", "104", "--item", "113", "--class", "MAGE",
			                           "--level", "2", "--race", "ASMODIANS", "--direction", "90"])
		self.assertEqual(code, 0)
		answer = json.loads(out.getvalue())
		self.assertEqual((answer["format"], answer["recovery"]["price"], answer["mail"][0]["byRace"]["ELYOS"]["total"]), ("aion-m5c-economy", 249, 251))
		self.assertEqual((answer["talk"][1]["startWindow"]["page"], answer["items"][0]["equip"]["passes"]), (18, True))
		self.assertEqual((answer["character"]["race"], answer["items"][1]["equip"]["refusedBy"]), ("ASMODIANS", "equipSkill"),
		                 "--race passes the race check of the Asmodian sword; the mage still lacks 37 and 44")
		self.assertAlmostEqual(answer["talk"][0]["bandSpot"]["y"], 206.4225, places=3, msg="--direction 90 turns the spots to +y")
		self.assertTrue(answer["assumptions"][1].startswith("every spot lies 90 degrees counter-clockwise from +x (--direction)"), answer["assumptions"][1])
		err = io.StringIO()
		with contextlib.redirect_stderr(err), contextlib.redirect_stdout(io.StringIO()):
			code = oracle.main(base + ["--no-profile", "--npc", "900001"])
		self.assertEqual(code, 2, "sieges on without --influence")
		self.assertIn("gameserver.siege.enable is true", err.getvalue())


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5cEconomyRealDataTest(unittest.TestCase):
	"""The gate's npcs and items in Poeta under the gate profile (m5c-plan.md §10.2-§10.3)."""

	@classmethod
	def setUpClass(cls):
		cls.data = StaticData(runner.DEFAULT_STATIC_DATA)
		cls.config = load_config(JAVA_SRC, CONFIG_DIR, None, GATE_PROFILE, cls.data, keys=ECONOMY_KEYS)
		cls.report = economy_report(cls.data, JAVA_SRC, cls.config, npc_ids=[798007, 700000, 203336, 203064, 798008], recover_exp=1000,
		                            mails=["162000002:5:200", "0:0:10"], item_ids=[100000133, 110100355])
		# C15's Mage B at level 4 (exp 3,820): the Plainsman's Sword, Hauberk and Jerkin, then the robe pieces Tunic, Leggings and Shoes
		cls.mage4 = economy_report(cls.data, JAVA_SRC, cls.config, item_ids=[100000133, 110500345, 110300316, 110100355, 113100293, 114100311],
		                           level=4)

	def test_the_config(self):
		self.assertEqual((self.config["gameserver.cube.expansion_limit"].value, self.config["gameserver.npcexpands.limit"].value), (11, 5))
		self.assertEqual(self.report["prices"]["ELYOS"]["smPrices"], [125, 100, 113])

	def test_the_talk_spots(self):
		talk = {t["npcId"]: t for t in self.report["talk"]}
		minalinerk = talk[798007]
		self.assertEqual((minalinerk["chosenSpot"]["x"], minalinerk["chosenSpot"]["y"], minalinerk["chosenSpot"]["z"]),
		                 (f32(851.671), f32(1252.67), f32(118.833)), "spawns/Npcs/210010000_Poeta.xml")
		self.assertEqual((minalinerk["talkDistance"], minalinerk["boundRadius"]["maxOfFrontAndSide"]), (5, f32(0.595)))
		self.assertEqual((minalinerk["bandSpot"]["distance"], minalinerk["bandSpot"]["inTalkRange"], minalinerk["bandSpot"]["inRangeWithoutPlusOne"],
		                  minalinerk["bandSpot"]["inRangeCenterToCenter"]), (6.4225, True, False, False), "X2's band spot, the middle of [6, 6.845)")
		self.assertEqual(minalinerk["startWindow"]["page"], 10, "X2: page 10")
		self.assertEqual((talk[700000]["startWindow"]["page"], talk[700000]["startWindow"]["pageValue"]), (18, 1), "X3: MAIL with REGULAR")
		self.assertEqual(talk[203336]["chosenSpot"]["distanceFromReference"], 11.1, "Seril stands 11.1 m from minalinerk (C16)")
		self.assertEqual(talk[203336]["functions"][0]["page"], 20, "X25")
		self.assertEqual([f["name"] for f in talk[203064]["functions"]], ["RECOVERY"])
		self.assertEqual([f["name"] for f in talk[798008]["functions"]], ["EXTEND_INVENTORY"])

	def test_the_prices(self):
		self.assertEqual(self.report["recovery"]["price"], 249, "X15")
		self.assertEqual(self.report["cube"][0]["price"], 1000, "X26")
		self.assertEqual(self.report["manastoneRemoval"]["byRace"]["ELYOS"], 917, "X25")
		self.assertEqual([m["byRace"]["ELYOS"]["total"] for m in self.report["mail"]], [251, 23], "X13 and C11's second letter")

	def test_the_plainsman_items(self):
		sword, tunic = self.report["items"]
		self.assertEqual((sword["breakItem"]["stones"], sword["breakItem"]["countRange"]), ([{"itemId": 166000191, "probability": 1.0}], [2, 5]), "X27")
		self.assertEqual((tunic["identification"]["canTune"], tunic["identification"]["sqlDefaultLoadsIdentified"],
		                  tunic["identification"]["optionalSocketsRange"], tunic["identification"]["enchantBonusRange"]), (True, True, [0, 1], [0, 0]),
		                 "D5: the tunic must be seeded with tune_count -1; X23's rolls")
		self.assertEqual((tunic["equip"]["passes"], tunic["equip"]["requiredLevel"], tunic["equip"]["startExpOfRequiredLevel"]), (False, 4, 3820),
		                 "C15: a level-1 mage cannot equip a Plainsman's armour piece (restrict 4): the gate must seed B's exp to 3,820")

	def test_c15_needs_a_robe_piece(self):
		# at level 4 a Mage knows 40, 100 and 103 of the equip skills (skill_tree.xml:45, 96, 99): the Sword (37/44), the Hauberk (42/49) and
		# the Jerkin (41/48) are refused without a packet; the robe pieces (103/106) pass
		equip = {i["itemId"]: i["equip"] for i in self.mage4["items"]}
		for item_id in (100000133, 110500345, 110300316):
			with self.subTest(item=item_id):
				self.assertEqual((equip[item_id]["passes"], equip[item_id]["refusedBy"], equip[item_id]["message"], equip[item_id]["knownRequiredSkills"]),
				                 (False, "equipSkill", None, []))
		for item_id in (110100355, 113100293, 114100311):
			with self.subTest(item=item_id):
				self.assertEqual((equip[item_id]["passes"], equip[item_id]["requiredSkills"], equip[item_id]["knownRequiredSkills"]),
				                 (True, [103, 106], [103]))
		self.assertEqual([s for s in self.mage4["character"]["learnedSkills"] if s in (37, 40, 41, 42, 44, 48, 49, 100, 103, 106)], [40, 100, 103])


if __name__ == "__main__":
	unittest.main()
