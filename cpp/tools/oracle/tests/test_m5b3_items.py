"""M5b-3 item, cube-budget and material oracles (m5b3/items.py, m5b3/materials.py; m5b3-plan.md G-01): the slot arithmetic alone, the Java
tables as they stand today and on an edited copy, the item report and the budget on a small static_data tree, the material zones of a
synthetic geo scene, and the gate's items, corpses and camp fires on the real data (m5b3-plan.md §2.4-§2.6, §10).

Expected values are derived by hand from the Java sources named in the two modules and repeated per case. Everything that reads the Java source
tree or the geo files is skipped without them.
"""

import contextlib
import io
import json
import shutil
import struct
import tempfile
import unittest
from pathlib import Path

from m5a.data import StaticData
from m5b3.drops import DropData, drops_report
from m5b3.items import JavaItemRules, ItemTemplateInfo, cube_budget, item_report, load_skill_templates_counted, new_slots
from m5b3.materials import material_report, zone_area
from m5b3.survey import droppable_items, item_survey, starter_items
from staticdata_oracle import OracleError
from staticdata_oracle import run as runner

import oracle

from .support import Tree
from .test_geo_oracle import mesh_entry, placement

JAVA_SRC = runner.TOOL_DIR.parents[2] / "game-server" / "src"
JAVA_HANDLERS = runner.TOOL_DIR.parents[2] / "game-server" / "data" / "handlers"
GEO_DIR = runner.TOOL_DIR.parents[2] / "game-server" / "data" / "geo"
HAVE_JAVA_TREE = (JAVA_SRC / "com" / "aionemu" / "gameserver").is_dir() and runner.DEFAULT_STATIC_DATA.is_dir()
HAVE_GEO = (GEO_DIR / "models.mesh").is_file() and (GEO_DIR / "210010000.geo").is_file()
KINAH = 182400001


def template(item_id, stack=1, extra=-1, mask=0):
	return ItemTemplateInfo(item_id, {"max_stack_count": str(stack), "mask": str(mask)}, [], None, None, extra)


class SlotArithmeticTest(unittest.TestCase):
	"""new_slots: ItemService.addItem's three arms for one entry (ItemService.java:70-173)"""

	def test_kinah_and_the_special_cube_take_no_slot(self):
		self.assertEqual(new_slots(template(KINAH), 25, [1000], KINAH), 0, "increaseKinah")
		self.assertEqual(new_slots(template(7, stack=100, extra=1), 5, [], KINAH), 0, "an <inventory id> above 0 is the special cube")

	def test_a_non_stackable_item_takes_one_slot_per_unit(self):
		self.assertEqual(new_slots(template(8), 1, [], KINAH), 1)
		self.assertEqual(new_slots(template(8), 3, [1, 1], KINAH), 3, "addNonStackableItem: one item per unit, whatever the cube holds")

	def test_a_stackable_item_fills_the_room_of_every_stack_first(self):
		shard = template(169000003, stack=10000)
		self.assertEqual(new_slots(shard, 15, [], KINAH), 1, "a new stack of 15")
		self.assertEqual(new_slots(shard, 15, [2], KINAH), 0, "merged into the stack of 2")
		potion = template(162000002, stack=1000)
		self.assertEqual(new_slots(potion, 5, [998], KINAH), 1, "2 fit, 3 need a new stack")
		self.assertEqual(new_slots(potion, 5, [998, 997], KINAH), 0, "both stacks are filled before a new one is made")
		self.assertEqual(new_slots(potion, 2500, [], KINAH), 3, "ItemFactory.calculateCount caps a new stack at max_stack_count")
		self.assertEqual(new_slots(potion, 1, [1000], KINAH), 1, "a full stack has no room")


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class JavaItemRulesTest(unittest.TestCase):
	BASE = ("com", "aionemu", "gameserver")
	FILES = ("services/item/ItemService.java", "services/item/ItemFactory.java", "model/gameobjects/Item.java", "model/templates/item/ItemTemplate.java",
	         "model/items/storage/Storage.java", "services/drop/DropService.java", "model/gameobjects/player/Player.java", "dataholders/SkillData.java",
	         "skillengine/effect/EffectTemplate.java", "model/items/ItemMask.java", "model/templates/item/actions/ItemActions.java",
	         "model/items/storage/StorageType.java", "model/items/ItemId.java")

	def rules_with(self, relative: str, old: str, new: str) -> JavaItemRules:
		with tempfile.TemporaryDirectory() as root:
			source, target = JAVA_SRC.joinpath(*self.BASE), Path(root).joinpath(*self.BASE)
			for file in self.FILES:
				(target / file).parent.mkdir(parents=True, exist_ok=True)
				shutil.copyfile(source / file, target / file)
			changed = target / relative
			text = changed.read_text(encoding="utf-8")
			self.assertIn(old, text, f"{relative} no longer contains the text this case changes")
			changed.write_text(text.replace(old, new, 1), encoding="utf-8")
			return JavaItemRules.read(Path(root))

	def test_the_tables_today(self):
		rules = JavaItemRules.read(JAVA_SRC)
		flags = dict(rules.mask_flags)
		self.assertEqual((flags["LIMIT_ONE"], flags["STORABLE_IN_WH"], flags["BREAKABLE"], flags["CAN_PROC_ENCHANT"]), (1, 8, 64, 1024))
		self.assertEqual(len(rules.action_classes), 32, "the 32 bound action classes of m5b3-plan.md §2.3")
		self.assertEqual(rules.action_classes["skilluse"], "SkillUseAction")
		self.assertEqual(rules.action_classes["enchant"], "EnchantItemAction")
		self.assertEqual((rules.cube_limit, rules.cube_row_length, rules.kinah_item_id), (27, 9, KINAH))
		self.assertEqual(rules.flag_names(12352), ["BREAKABLE", "REMODELABLE", "CAN_SPLIT"], "164002116: not STORABLE_IN_WH")

	def test_literals_are_read(self):
		self.assertEqual(self.rules_with("model/items/storage/StorageType.java", "CUBE(0, 27, 9, 102)", "CUBE(0, 36, 9, 102)").cube_limit, 36)
		self.assertEqual(dict(self.rules_with("model/items/ItemMask.java", "LIMIT_ONE = 1;", "LIMIT_ONE = (1 << 20);").mask_flags)["LIMIT_ONE"],
		                 1 << 20)

	def test_a_changed_statement_is_refused(self):
		cases = [
			("model/templates/item/ItemTemplate.java", "return this.maxStackCount > 1;", "return this.maxStackCount > 0;"),
			("services/item/ItemFactory.java", "if (count > maxStackCount && !itemTemplate.isKinah())", "if (count > maxStackCount)"),
			("services/item/ItemService.java", "inventory.increaseKinah(count);", "inventory.increaseKinah(count * 2);"),
			("dataholders/SkillData.java", "skillTemplateById.put(skillId, skillTemplate);", "skillTemplateById.putIfAbsent(skillId, skillTemplate);"),
			("skillengine/effect/EffectTemplate.java", "return value + delta * effect.getSkillLevel();", "return value;"),
		]
		for relative, old, new in cases:
			with self.subTest(relative=relative, old=old):
				with self.assertRaises(OracleError):
					self.rules_with(relative, old, new)


ITEMS = """
<item_template id="182400001" name="kinah" level="1"/>
<item_template id="500" name="fixture potion" level="1" mask="12414" max_stack_count="1000">
	<actions><skilluse level="2" skillid="9000"/></actions>
	<uselimits usedelay="30000" usedelayid="11"/>
</item_template>
<item_template id="501" name="fixture godstone" level="1" mask="12414" max_stack_count="100">
	<godstone nonbreakcount="0" breakprob="0" probabilityleft="500" probability="1000" skilllvl="1" skillid="9001"/>
</item_template>
<item_template id="502" name="fixture sword" level="1" mask="1024"/>
<item_template id="503" name="fixture manastone" level="1" max_stack_count="100"><actions><enchant count="1"/></actions></item_template>
<item_template id="504" name="fixture lore" level="1" mask="1" max_stack_count="10"/>
<item_template id="505" name="fixture bag item" level="1" max_stack_count="10"><inventory id="1"/></item_template>
<item_template id="506" name="fixture unbound" level="1"><actions><nosuchaction/></actions></item_template>
<item_template id="507" name="fixture shard" level="1" max_stack_count="10000"/>
"""

SKILLS = """
<skill_template skill_id="9000" name="first" lvl="1"><effects><heal value="1"/></effects></skill_template>
<skill_template skill_id="9001" name="proc" lvl="1"><effects><procatk_instant delta="100" element="EARTH"/></effects></skill_template>
<skill_template skill_id="9000" name="second" lvl="1"><effects><prochealinstant value="37" delta="3"/><heal value="37" duration2="20000"/></effects>
</skill_template>
"""


def item_tree() -> Tree:
	tree = Tree()
	tree.minimal({"item_templates": ITEMS, "skill_data": SKILLS})
	return tree


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class ItemReportFixtureTest(unittest.TestCase):
	@classmethod
	def setUpClass(cls):
		cls.tree = item_tree()
		cls.data = StaticData(cls.tree.root)

	@classmethod
	def tearDownClass(cls):
		cls.tree.close()

	def test_the_last_skill_template_of_an_id_wins(self):
		templates, counts = load_skill_templates_counted(self.data, {9000, 9001})
		self.assertEqual((templates[9000].attrs["name"], counts[9000]), ("second", 2), "SkillData.afterUnmarshal: skillTemplateById.put")
		self.assertEqual(counts[9001], 1)

	def test_actions_skill_effects_and_values(self):
		report = item_report(self.data, JAVA_SRC, [500, 501, 502])
		potion, godstone, sword = report["items"]
		self.assertEqual(potion["cooldown"], {"useDelayMillis": 30000, "useDelayId": 11})
		self.assertEqual((potion["maxStackCount"], potion["stackable"]), (1000, True))
		(action,) = potion["actions"]
		self.assertEqual((action["tag"], action["class"]), ("skilluse", "SkillUseAction"))
		skill = action["skill"]
		self.assertEqual((skill["skillId"], skill["level"], skill["templateCount"], skill["name"]), (9000, 2, 2, "second"))
		self.assertEqual([(e["class"], e["valueAtLevel"]) for e in skill["effects"]], [("ProcHealInstantEffect", 43), ("HealEffect", 37)],
		                 "EffectTemplate.calculateBaseValue: value + delta * skill level (37 + 3 * 2)")
		self.assertIn("AbstractHealEffect", skill["effectClassChain"])
		self.assertEqual(godstone["godstone"]["attributes"]["probability"], "1000")
		self.assertEqual(godstone["godstone"]["skill"]["effectClasses"], ["ProcAtkInstantEffect"])
		self.assertEqual(godstone["godstone"]["skill"]["effects"][0]["valueAtLevel"], 100, "value 0 + delta 100 * level 1")
		self.assertEqual((sword["maskFlags"], sword["maxStackCount"], sword["stackable"], sword["cooldown"]), (["CAN_PROC_ENCHANT"], 1, False, None))

	def test_refusals(self):
		with self.assertRaises(OracleError):
			item_report(self.data, JAVA_SRC, [506])  # <nosuchaction> is not bound by ItemActions.java
		with self.assertRaises(OracleError):
			item_report(self.data, JAVA_SRC, [999])  # no template

	def test_the_cube_budget(self):
		rules = JavaItemRules.read(JAVA_SRC)

		def rule(index, name, items, picks=1, certain=True, never=False):
			return {"ruleIndex": index, "ruleName": name, "indexes": [index], "entriesIfFired": picks, "certain": certain, "never": never,
			        "candidates": [{"itemId": i, "countRange": r} for i, r in items]}

		report = {"rules": [rule(1, "Kinah", [(KINAH, [5, 25])]), rule(2, "Shards", [(507, [2, 15])]), rule(3, "Potions", [(500, [1, 1]), (502, [1, 1])]),
		                    rule(4, "Lore", [(504, [1, 1])]), rule(5, "Bag", [(505, [1, 1])]), rule(6, "Two picks", [(502, [1, 1]), (503, [2, 2]), (507, [1, 1])], 2),
		                    rule(7, "Maybe", [(502, [1, 1])], certain=False), rule(8, "Never", [(502, [1, 1])], never=True)]}
		budget = cube_budget(report, self.data, rules, [(KINAH, 1000), (507, 3), (500, 100), (504, 1)])
		self.assertEqual((budget["limit"], budget["slotsUsed"], budget["slotsFree"]), (27, 3, 24), "kinah takes no slot")
		by_name = {e["ruleName"]: e for e in budget["entries"]}
		self.assertEqual(budget["kinahEntries"], 1)
		self.assertEqual([m["ruleName"] for m in budget["deterministicMerges"]], ["Shards", "Lore"],
		                 "the shard always fits the stack of 3; the lore item is refused (LIMIT_ONE already in the cube) and stays in the corpse")
		self.assertEqual(by_name["Lore"]["candidates"][0]["merge"], "refusedLimitOne")
		self.assertEqual((by_name["Potions"]["newSlotsWorst"], by_name["Potions"]["newSlotsBest"]), (1, 0), "the potion merges, the sword does not")
		self.assertEqual(by_name["Bag"]["candidates"][0]["merge"], "specialCube")
		self.assertEqual(budget["specialCubeItems"], [505])
		self.assertEqual(by_name["Two picks"]["newSlotsWorst"], 2, "two distinct candidates: the two largest of 1, 1, 0")
		self.assertEqual((by_name["Maybe"]["newSlotsWorst"], by_name["Maybe"]["newSlotsBest"]), (1, 0), "an uncertain rule may add nothing")
		self.assertNotIn("Never", by_name)
		self.assertEqual(budget["worstCaseNewSlots"], 0 + 0 + 1 + 0 + 0 + 2 + 1)
		self.assertEqual(budget["bestCaseNewSlots"], 0 + 0 + 0 + 0 + 0 + 1)
		with self.assertRaises(OracleError):
			cube_budget(report, self.data, rules, [(507, 10001)])  # more than max_stack_count
		with self.assertRaises(OracleError):
			cube_budget(report, self.data, rules, [(KINAH, 1), (KINAH, 2)])  # one kinah item per storage

	def test_the_cube_budget_of_a_custom_drop_group(self):
		"""a custom drop's count is DropItem.calculateCount = Rnd.get(minAmount, maxAmount) (DropItem.java:38-40), the keys drops_report gives"""
		rules = JavaItemRules.read(JAVA_SRC)

		def drop(item_id, low, high, final=50.0):
			return {"itemId": item_id, "minAmount": low, "maxAmount": high, "finalChance": final}

		def group(number, drops, min_entries, max_entries, applies=True):
			return {"group": number, "name": f"group {number}", "applies": applies, "minEntries": min_entries, "maxEntries": max_entries,
			        "drops": drops}

		report = {"rules": [], "customDrop": [
			# the sword is non-stackable: 3 of it are 3 items (addNonStackableItem); the potion's 1995 overflow the stack of 100: 900 fill it, the
			# other 1095 make two new stacks of at most 1000; the manastone at finalChance 0 is never picked (`chance < finalChance`)
			group(0, [drop(502, 2, 3, final=100.0), drop(500, 5, 1995), drop(503, 250, 250, final=0.0)], 1, 2),
			group(1, [drop(507, 1, 5)], 0, 1),              # merges into the shard stack of 3 at any count
			group(2, [drop(502, 9, 9)], 0, 1, applies=False),  # another race's group adds nothing
			group(3, [drop(KINAH, 10, 90)], 0, 1),
		]}
		budget = cube_budget(report, self.data, rules, [(500, 100), (507, 3)])
		by_group = {e["customGroup"]: e for e in budget["entries"]}
		self.assertEqual(sorted(by_group), [0, 1, 3], "the group that does not apply is not an entry")
		first = by_group[0]
		self.assertEqual([(c["itemId"], c["countRange"], c["newSlotsWorst"], c["newSlotsBest"]) for c in first["candidates"]],
		                 [(502, [2, 3], 3, 2), (500, [5, 1995], 2, 0)], "the manastone at finalChance 0 is no candidate")
		self.assertEqual((first["newSlotsWorst"], first["newSlotsBest"]), (3 + 2, 0), "two distinct drops at most; the one certain pick may be the potion")
		self.assertEqual([m.get("customGroup") for m in budget["deterministicMerges"]], [1], "the shard merges whatever count is picked")
		self.assertEqual((by_group[3]["kinah"], budget["kinahEntries"]), (True, 1))
		self.assertEqual((budget["worstCaseNewSlots"], budget["bestCaseNewSlots"]), (5, 0))
		one_certain = {"rules": [], "customDrop": [group(0, [drop(502, 2, 3, final=100.0)], 1, 1)]}
		self.assertEqual(cube_budget(one_certain, self.data, rules, [])["bestCaseNewSlots"], 2, "the certain sword at its minimum amount of 2")


SURVEY_ITEMS = ITEMS + """
<item_template id="508" name="fixture stone, replaced" level="1"><godstone skillid="9000" skilllvl="1" probability="1" breakprob="0"/></item_template>
<item_template id="509" name="fixture double buff" level="1"><actions><skilluse level="1" skillid="9002"/></actions></item_template>
<item_template id="508" name="fixture stone, later" level="1"/>
"""
SURVEY_SKILLS = SKILLS + """
<skill_template skill_id="9002" name="double" lvl="1"><effects><statup/><statup/></effects></skill_template>
<skill_template skill_id="9003" name="bonfire" lvl="1"><effects><procatk_instant value="5" element="FIRE"/><heal value="1"/></effects>
</skill_template>
"""
SURVEY_INITIAL = ('<player_data class="WARRIOR"><items><item id="182400001" count="1000"/><item id="500" count="100"/></items></player_data>'
                  '<player_data class="MAGE"><items><item id="502" count="1"/></items></player_data><player_data class="PRIEST"/>')
SURVEY_MATERIALS = ('<material id="60"><skill id="9003" level="1" target="PLAYER" frequency="5"/></material>'
                    '<material id="61"><skill id="9003" level="1" target="PLAYER" frequency="5" conditions="NIGHT"/>'
                    '<skill id="9001" level="1" target="PLAYER" frequency="5"/></material><material id="62"/>')


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class ItemSurveyFixtureTest(unittest.TestCase):
	"""m5b3/survey.py on a small tree: the scope, the last skill template, a replaced item template, effects against items"""

	def test_the_three_scopes(self):
		with Tree() as tree:
			tree.minimal({"item_templates": SURVEY_ITEMS, "skill_data": SURVEY_SKILLS, "player_initial_data": SURVEY_INITIAL,
			              "material_templates": SURVEY_MATERIALS})
			data = StaticData(tree.root)
			self.assertEqual(starter_items(data), {KINAH, 500, 502}, "every class's <items>; a <player_data> without items adds none")
			survey = item_survey(data, JAVA_SRC, {1: {501, 509}, 2: {503}})
		skilluse = survey["skilluse"]
		self.assertEqual([r["itemId"] for r in skilluse["itemRows"]], [500, 509], "the starter potion and the droppable buff; 501-503 have none")
		self.assertEqual((skilluse["items"], skilluse["fromStarter"], skilluse["droppableByMap"]), (2, 1, {"1": 1, "2": 0}))
		self.assertEqual(skilluse["leafClasses"], {
			"HealEffect": {"items": 1, "effects": 1, "skills": 1},
			"ProcHealInstantEffect": {"items": 1, "effects": 1, "skills": 1},  # 9000's LAST template ("second"), not the first's lone heal
			"StatupEffect": {"items": 1, "effects": 2, "skills": 1},  # two effects of one item: m5b3-plan.md §2.5 counts effects
		})
		self.assertIn("AbstractHealEffect", skilluse["classChain"], "closed under extends")
		self.assertIn("EffectTemplate", skilluse["classChain"])
		godstones = survey["godstones"]
		self.assertEqual((godstones["items"], godstones["skills"]), (1, 1), "508's later template has no godstone (ItemData: items.put)")
		self.assertEqual(godstones["leafClasses"], {"ProcAtkInstantEffect": {"skills": 1, "items": 1}})
		self.assertEqual(godstones["droppableByMap"]["1"]["itemIds"], [501])
		self.assertEqual(godstones["droppableByMap"]["2"]["items"], 0)
		materials = survey["materials"]
		self.assertEqual((materials["materialsWithSkills"], materials["skills"]), (2, 2), "material 62 has no skill")
		self.assertEqual(materials["leafClasses"], {"HealEffect": {"skills": 1}, "ProcAtkInstantEffect": {"skills": 2}})
		self.assertEqual([(r["skillId"], r["materials"]) for r in materials["skillRows"]], [(9001, [61]), (9003, [60, 61])])

	def test_refusals(self):
		with Tree() as tree:
			tree.minimal({"item_templates": SURVEY_ITEMS, "skill_data": SURVEY_SKILLS, "player_initial_data": SURVEY_INITIAL})
			data = StaticData(tree.root)
			with self.assertRaises(OracleError):
				item_survey(data, JAVA_SRC, {1: {506}})  # <nosuchaction> is not bound by ItemActions.java
			with self.assertRaises(OracleError):
				item_survey(data, JAVA_SRC, {1: {999}})  # no template


class MaterialAreaTest(unittest.TestCase):
	"""MaterialZoneTemplate's area rule (MaterialZoneTemplate.java:13-38)"""

	def test_the_three_shapes(self):
		self.assertEqual(zone_area("PR_L_FIRE_SEMISPHERE_01A_1", (1.0, 2.0, 3.0), (3.0, 4.0, 12.0))["type"], "SEMISPHERE")
		self.assertEqual(zone_area("PR_L_FIRE_SEMISPHERE_01A_1", (1.0, 2.0, 3.0), (3.0, 4.0, 12.0))["r"], 14.0, "sqrt(9 + 16 + 144) + 1")
		sphere = zone_area("PR_L_WEATHERFIRE_BOX_01A_1", (1.0, 2.0, 3.0), (3.0, 4.0, 12.0))
		self.assertEqual((sphere["type"], sphere["r"]), ("SPHERE", 14.0))
		cylinder = zone_area("A_CONE_B_1", (1.0, 2.0, 3.0), (3.0, 4.0, 12.0))
		self.assertEqual((cylinder["type"], cylinder["r"], cylinder["top"], cylinder["bottom"]), ("CYLINDER", 6.0, 16.0, -10.0),
		                 "r = sqrt(9 + 16) + 1, z +- (12 + 1)")
		self.assertEqual(zone_area("X_H_COLUME_1", (0.0, 0.0, 0.0), (1.0, 1.0, 1.0))["type"], "CYLINDER")


IDENTITY = (1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0)
FIRE = ([0.0, 0.0, 0.0, 2.0, 0.0, 0.0, 0.0, 2.0, 2.0], [0, 1, 2], 1, 60, 3)
NIGHT_FIRE = ([0.0, 0.0, 0.0, 2.0, 0.0, 0.0, 0.0, 2.0, 2.0], [0, 1, 2], 1, 61, 3)
UNKNOWN_MATERIAL = ([0.0, 0.0, 0.0, 2.0, 0.0, 0.0, 0.0, 2.0, 2.0], [0, 1, 2], 1, 99, 3)
ROCK = ([0.0, 0.0, 0.0, 2.0, 0.0, 0.0, 0.0, 2.0, 0.0], [0, 1, 2], 1, 60, 1)  # material 60 without the MATERIAL intention


class MaterialFixtureTest(unittest.TestCase):
	"""the zones of a synthetic scene: the material template filter, the duplicate zone name, the area and the distances"""

	def test_zones_of_a_map(self):
		with Tree() as tree:
			tree.minimal({"material_templates": '<material id="60"><skill id="8302" level="1" target="PLAYER" frequency="5"/></material>'
			                                    '<material id="61"><skill id="8302" level="1" target="PLAYER" frequency="5" conditions="SUNNY NIGHT"/>'
			                                    '</material>'})
			geo = tree.mkdir("geo")
			(geo / "models.mesh").write_bytes(mesh_entry("levels/fire_semisphere_01a.cgf", [FIRE]) + mesh_entry("levels/night_fire.cgf", [NIGHT_FIRE])
			                                  + mesh_entry("levels/odd.cgf", [UNKNOWN_MATERIAL]) + mesh_entry("levels/rock.cgf", [ROCK]))
			(geo / "110010000.geo").write_bytes(placement("levels/fire_semisphere_01a.cgf", 100, 0, 0) + placement("levels/night_fire.cgf", 10, 0, 0)
			                                    + placement("levels/fire_semisphere_01a.cgf", 100, 0, 0) + placement("levels/odd.cgf", 0, 0, 0)
			                                    + placement("levels/rock.cgf", 0, 0, 0))
			world_maps = geo / "world_maps.xml"
			world_maps.write_text('<world_maps>\n\t<map id="110010000" world_size="1024"/>\n</world_maps>\n')
			report = material_report(StaticData(tree.root), geo, world_maps, 110010000, (0.0, 0.0, 0.0))
		self.assertEqual((report["meshPlacements"], report["materialGeometries"]), (5, 4), "the rock has no MATERIAL intention")
		self.assertEqual(report["geometriesWithoutMaterialTemplate"], {"99": 1}, "ZoneService: no MaterialTemplate, no zone")
		self.assertEqual(len(report["duplicateZoneNames"]), 1, "the second fire at the same spot has the same name: logged, not created")
		self.assertEqual((report["skillZones"], report["skillZonesByMaterial"], report["skillIds"]), (2, {"60": 1, "61": 1}, [8302]))
		first, second = report["zones"]
		# the triangle's bound center is (1, 1, 1), placed at x 10: (11, 1, 1), sqrt(123) from the origin
		self.assertEqual((first["materialId"], round(first["distance"], 3)), (61, 11.091), "nearest first: the night fire at x 10")
		self.assertEqual(first["center"], [11.0, 1.0, 1.0])
		self.assertEqual(second["area"]["type"], "SEMISPHERE")
		self.assertEqual(second["skills"][0]["conditions"], None)
		self.assertEqual(first["skills"][0]["conditions"], ["SUNNY", "NIGHT"])
		self.assertEqual(report["nearestUnconditional"]["materialId"], 60, "material 61 has conditions")
		self.assertEqual(report["terrain"]["materialsFile"], None)

	def test_the_child_suffix_of_a_zone_name(self):
		"""createZone (GeoWorldLoader.java:240-273): "_CHILD" + (index + 1) only when the node has several children - the name decides which
		geometries ZoneService takes for duplicates, and the zone G-04 names as the fire"""
		with Tree() as tree:
			tree.minimal({"material_templates": '<material id="60"><skill id="8302" level="1" target="PLAYER" frequency="5"/></material>'
			                                    '<material id="61"><skill id="8302" level="1" target="PLAYER" frequency="5" conditions="NIGHT"/>'
			                                    '</material>'})
			geo = tree.mkdir("geo")
			# two children with the same bound at the same spot: only the suffix keeps their zone names apart
			(geo / "models.mesh").write_bytes(mesh_entry("levels/twin_fire.cgf", [FIRE, NIGHT_FIRE]) + mesh_entry("levels/fire_semisphere_01a.cgf", [FIRE]))
			(geo / "110010000.geo").write_bytes(placement("levels/twin_fire.cgf", 10, 0, 0) + placement("levels/fire_semisphere_01a.cgf", 100, 0, 0))
			world_maps = geo / "world_maps.xml"
			world_maps.write_text('<world_maps>\n\t<map id="110010000" world_size="1024"/>\n</world_maps>\n')
			report = material_report(StaticData(tree.root), geo, world_maps, 110010000, (0.0, 0.0, 0.0))
		names = [(z["materialId"], z["zoneName"]) for z in report["zones"]]
		self.assertEqual(report["duplicateZoneNames"], [], f"the two children are two zones: {names}")
		self.assertEqual(len(names), 3, names)
		twin = sorted(n for m, n in names if "TWIN" in n)
		self.assertRegex(twin[0], r"^TWIN_FIRE_CHILD1_-?\d+_110010000$")
		self.assertRegex(twin[1], r"^TWIN_FIRE_CHILD2_-?\d+_110010000$")
		self.assertEqual(twin[0].split("_")[-2], twin[1].split("_")[-2], "the same world bound center, the same vector hash")
		self.assertEqual(dict(names)[61], twin[1], "the second child is the night fire")
		(single,) = [n for m, n in names if "TWIN" not in n]
		self.assertRegex(single, r"^FIRE_SEMISPHERE_01A_-?\d+_110010000$", "a node with one child has no suffix")


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class RealDataTest(unittest.TestCase):
	"""The gate's items and corpses (m5b3-plan.md §2.4-§2.6, §10.1)"""

	STARTER_CUBE = [(KINAH, 1000), (160000001, 12), (169300002, 20), (162000002, 100), (162000007, 100), (164002116, 50), (164002117, 50),
	                (164002118, 50), (169620005, 2), (164002039, 1)]  # player_initial_data.xml:5-21, the Warrior's items but its equipped gear

	@classmethod
	def setUpClass(cls):
		cls.data = StaticData(runner.DEFAULT_STATIC_DATA)
		cls.items = {i["itemId"]: i for i in item_report(cls.data, JAVA_SRC, [162000002, 168000116, 164002116, 164002039, 169620005, 100000094,
		                                                                      162000007])["items"]}

	def test_the_potions(self):
		potion = self.items[162000002]
		self.assertEqual(potion["cooldown"], {"useDelayMillis": 30000, "useDelayId": 11}, "item_templates.xml:830728")
		skill = potion["actions"][0]["skill"]
		self.assertEqual((skill["skillId"], skill["templateCount"]), (9889, 1))
		self.assertEqual([(e["class"], e["valueAtLevel"]) for e in skill["effects"]], [("ProcHealInstantEffect", 37), ("HealEffect", 37)],
		                 "m5b3-plan.md §2.5: 37 HP at once and 37 per tick")
		self.assertEqual(skill["effects"][1]["attributes"]["duration2"], "20000")
		mana = self.items[162000007]["actions"][0]["skill"]
		self.assertEqual((mana["skillId"], mana["effectClasses"][-1]), (9894, "ProcMPHealInstantEffect"))

	def test_the_godstone_and_its_one_template(self):
		godstone = self.items[168000116]["godstone"]
		self.assertEqual((godstone["attributes"]["probability"], godstone["attributes"]["breakprob"], godstone["attributes"]["skillid"]),
		                 ("1000", "0", "8267"))
		skill = godstone["skill"]
		# m5b3-plan.md §2.6 (b) and risk 11 say skill 8267 has two templates (skill_templates.xml:80570, :80575); the data has ONE, at :80570 -
		# the WIND procatk_instant just above it belongs to the preceding template. The oracle still keeps the last of several (the fixture case)
		self.assertEqual(skill["templateCount"], 1)
		self.assertEqual(skill["effectClasses"], ["ProcAtkInstantEffect"])
		self.assertEqual(skill["effects"][0]["attributes"]["element"], "EARTH")
		self.assertEqual(skill["attributes"]["skilltype"], "MAGICAL")
		self.assertIn("CAN_PROC_ENCHANT", self.items[100000094]["maskFlags"], "the Training Sword can take a godstone (ItemMask bit 10)")

	def test_the_starter_items(self):
		self.assertNotIn("STORABLE_IN_WH", self.items[164002116]["maskFlags"], "mask 12352: not storable in a warehouse (L8)")
		boon = self.items[164002039]
		self.assertEqual(boon["expireTimeMinutes"], 4321)
		self.assertEqual(boon["actions"][0]["skill"]["effectClasses"], ["HiPassEffect", "NoDeathPenaltyEffect", "NoResurrectPenaltyEffect"])
		self.assertEqual(self.items[169620005]["actions"][0]["skill"]["effectClasses"], ["XPBoostEffect"])

	def test_the_cube_budget_of_the_gate_corpses(self):
		drops = DropData(self.data, JAVA_SRC, JAVA_HANDLERS)
		rules = JavaItemRules.read(JAVA_SRC)
		first = cube_budget(drops_report(drops, 210663, drop_rate="1000000"), self.data, rules, self.STARTER_CUBE)
		self.assertEqual((first["slotsUsed"], first["worstCaseNewSlots"], first["kinahEntries"], first["deterministicMerges"]), (9, 10, 0, []),
		                 "m5b3-plan.md risk 5: 9 starter stacks + up to 10 new stacks from the first 210663")
		self.assertTrue(first["fitsWorstCase"])
		after = self.STARTER_CUBE + [(169000003, 2), (182004793, 1)]  # the shard and the junk the first corpse certainly gave
		second = cube_budget(drops_report(drops, 210133, drop_rate="1000000"), self.data, rules, after)
		self.assertEqual((second["worstCaseNewSlots"], second["kinahEntries"]), (8, 1), "up to 8 from 210133: kinah takes no slot, the shard merges")
		self.assertEqual([m["ruleName"] for m in second["deterministicMerges"]], ["Power Shards"])
		third = cube_budget(drops_report(drops, 210663, drop_rate="1000000"), self.data, rules, after)
		self.assertEqual(sorted(m["ruleName"] for m in third["deterministicMerges"]), ["JUNK_SPAKY_MATERIAL", "Power Shards"],
		                 "L6c: the second 210663 merges into both")
		self.assertEqual(third["worstCaseNewSlots"], 8)

	def test_the_survey_of_m5b3_plan_2_5_and_2_6(self):
		"""the counts §2.5-§2.6 took from throwaway parses (the review's finding 4), re-derived"""
		drops = DropData(self.data, JAVA_SRC, JAVA_HANDLERS)
		droppable, killers = droppable_items(drops, [210010000, 220010000])
		self.assertEqual((len(droppable[210010000]), len(droppable[220010000])), (999, 1077), "§2.4's distinct droppable items")
		self.assertEqual((killers[210010000]["race"], killers[220010000]["race"]), ("ELYOS", "ASMODIANS"))
		survey = item_survey(self.data, JAVA_SRC, droppable, killers)
		skilluse = survey["skilluse"]
		self.assertEqual((skilluse["items"], len(skilluse["leafClasses"])), (48, 9), "§2.5: 48 items, 9 leaf effect classes")
		self.assertEqual((skilluse["fromStarter"], skilluse["droppableByMap"]), (8, {"210010000": 31, "220010000": 31}),
		                 "§2.5 'SkillUseAction ... 31 of the droppable items' (Poeta), the same count on Ishalgen")
		self.assertEqual({cls: (v["items"], v["effects"]) for cls, v in skilluse["leafClasses"].items()}, {
			"StatupEffect": (12, 20),  # §2.5's table says 20 "items": the juice, the 8 serums/agents with two statups each and 3 event potions
			"HealEffect": (9, 9), "MPHealEffect": (9, 9), "ProcHealInstantEffect": (18, 18), "ProcMPHealInstantEffect": (18, 18),
			"XPBoostEffect": (1, 1), "NoResurrectPenaltyEffect": (1, 1), "NoDeathPenaltyEffect": (1, 1), "HiPassEffect": (1, 1)})
		godstones = survey["godstones"]
		self.assertEqual((godstones["items"], godstones["skills"], len(godstones["leafClasses"])), (268, 110, 10),
		                 "§2.6: 268 items, 110 proc skills, 10 leaf classes")
		self.assertEqual({cls: godstones["leafClasses"][cls]["skills"] for cls in ("ProcAtkInstantEffect", "PoisonEffect", "SilenceEffect",
		                                                                            "BlindEffect", "ParalyzeEffect")},
		                 {"ProcAtkInstantEffect": 70, "PoisonEffect": 6, "SilenceEffect": 5, "BlindEffect": 5, "ParalyzeEffect": 4})
		poeta = godstones["droppableByMap"]["210010000"]
		self.assertEqual(poeta["items"], 34, "§2.6: the 34 illusion godstones Poeta monsters drop")
		self.assertEqual(poeta["leafClasses"], sorted(godstones["leafClasses"]), "... use exactly the same ten")
		materials = survey["materials"]
		self.assertEqual((materials["skills"], len(materials["leafClasses"])), (28, 13), "§2.6: 28 material skills, 13 leaf classes")
		self.assertEqual({cls: materials["leafClasses"][cls]["skills"] for cls in ("ProcAtkInstantEffect", "DispelEffect", "AbsoluteSnareEffect",
		                                                                            "FearEffect", "MpAttackInstantEffect", "ProcHealInstantEffect")},
		                 {"ProcAtkInstantEffect": 13, "DispelEffect": 5, "AbsoluteSnareEffect": 4, "FearEffect": 1, "MpAttackInstantEffect": 1,
		                  "ProcHealInstantEffect": 1})

	def test_cli(self):
		out = io.StringIO()
		with contextlib.redirect_stdout(out):
			self.assertEqual(oracle.main(["m5b3-item", "--item", "162000002"]), 0)
		self.assertEqual(json.loads(out.getvalue())["items"][0]["cooldown"]["useDelayMillis"], 30000)
		out = io.StringIO()
		with contextlib.redirect_stdout(out):
			self.assertEqual(oracle.main(["m5b3-drops", "--npc", "210133", "--drop-rate", "1000000", "--inventory", "169000003:2"]), 0)
		self.assertEqual([m["ruleName"] for m in json.loads(out.getvalue())["cube"]["deterministicMerges"]], ["Power Shards"])
		out = io.StringIO()
		with contextlib.redirect_stdout(out):
			self.assertEqual(oracle.main(["m5b3-item", "--survey", "--map", "210010000"]), 0)
		self.assertEqual(json.loads(out.getvalue())["skilluse"]["droppableByMap"], {"210010000": 31})
		with contextlib.redirect_stderr(io.StringIO()):
			self.assertEqual(oracle.main(["m5b3-item", "--survey"]), 2)  # a survey names its maps
			self.assertEqual(oracle.main(["m5b3-item", "--item", "162000002", "--survey", "--map", "210010000"]), 2)
			self.assertEqual(oracle.main(["m5b3-item", "--item", "162000002", "--map", "210010000"]), 2)
			self.assertEqual(oracle.main(["m5b3-drops", "--npc", "210133", "--inventory", "169000003:x"]), 2)
			self.assertEqual(oracle.main(["m5b3-drops", "--survey", "--map", "210010000", "--inventory", "169000003"]), 2)
			self.assertEqual(oracle.main(["m5b3-material", "--map", "210010000", "--near", "1,2"]), 2)


@unittest.skipUnless(HAVE_JAVA_TREE and HAVE_GEO, "Java tree or geo data not present")
class MaterialRealDataTest(unittest.TestCase):
	"""the camp fires of the start maps (m5b3-plan.md §2.6: 98 / 48, the review's independent count 97 / 44)"""

	def test_poeta_and_ishalgen(self):
		data = StaticData(runner.DEFAULT_STATIC_DATA)
		world_maps = runner.DEFAULT_STATIC_DATA / "world_maps.xml"
		poeta = material_report(data, GEO_DIR, world_maps, 210010000, (1212.9423, 1044.8516, 140.75568), limit=1)
		self.assertEqual((poeta["meshPlacements"], poeta["skillZones"], poeta["skillPlacements"]), (4496, 98, 98))
		self.assertEqual(poeta["skillZonesByMaterial"], {"60": 22, "61": 7, "62": 69})
		self.assertEqual(poeta["skillIds"], [8302], "every one is Flame Strike")
		self.assertEqual(poeta["duplicateZoneNames"], [])
		fire = poeta["nearestUnconditional"]
		self.assertEqual((fire["materialId"], fire["mesh"].rsplit("/", 1)[-1], fire["area"]["type"]), (60, "pr_l_fire_semisphere_01a.cgf", "SEMISPHERE"))
		self.assertEqual(round(fire["distance"]), 407, "m5b3-plan.md §2.6: 407 m from the Elyos spawn")
		self.assertEqual(round(poeta["zones"][0]["distance"]), 108, "the nearest of any is a material-62 fire at 108 m")
		self.assertEqual(poeta["terrain"]["materialsFile"], None, "Poeta has no terrain materials: no terrain material skill (§12 inference)")
		ishalgen = material_report(data, GEO_DIR, world_maps, 220010000, None)
		self.assertEqual((ishalgen["meshPlacements"], ishalgen["skillZones"], ishalgen["skillZonesByMaterial"]),
		                 (5017, 48, {"60": 11, "61": 1, "62": 36}))
		self.assertEqual(ishalgen["terrain"]["materialsFile"], None)


if __name__ == "__main__":
	unittest.main()
