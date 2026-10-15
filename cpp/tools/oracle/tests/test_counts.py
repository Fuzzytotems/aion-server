import unittest

from staticdata_oracle import OracleError
from staticdata_oracle import run as runner
from staticdata_oracle.counts import HOLDERS, LINES, evaluate
from tests.support import MINIMAL_HOLDERS, Tree, xml


class CountOracleTest(unittest.TestCase):

	def setUp(self):
		self.tree = Tree()
		self.addCleanup(self.tree.close)

	def counts(self, overrides=None, skip=()):
		self.tree.minimal(overrides, skip)
		return runner.run(self.tree.root, totals=False, census=False, region_variants=False)["counts"]

	def line(self, counts, java_line):
		return next(line for line in counts["lines"] if line["javaLine"] == java_line)

	def test_tables_are_consistent(self):
		self.assertEqual(len(HOLDERS), 92)
		self.assertEqual(len(LINES), 90)
		self.assertEqual(sum(len(calls) for _, _, calls in LINES), 93)  # 92 numbers + the exclusion suffix
		self.assertEqual([n for n, _, _ in LINES], list(range(315, 405)))
		self.assertEqual(set(MINIMAL_HOLDERS), set(HOLDERS))

	def test_minimal_data_gives_all_lines(self):
		counts = self.counts()
		self.assertEqual(len(counts["lines"]), 90)
		self.assertEqual(counts["lines"][0], {"javaLine": 315, "line": "Loaded 1 maps", "values": [1], "holders": ["world_maps"]})
		self.assertEqual(self.line(counts, 394)["line"], "Loaded 1 global drop rules")
		self.assertEqual(self.line(counts, 323)["line"], "Loaded 0 bonus item group templates and 0 pet food items")
		self.assertEqual(self.line(counts, 338)["values"], [1])
		self.assertEqual(counts["extras"]["xml_quests"]["value"], 2)

	def test_missing_holder_is_a_java_crash(self):
		with self.assertRaisesRegex(OracleError, "pets"):
			self.counts(skip=("pets",))

	def test_distinct_keys_default_and_last_wins(self):
		counts = self.counts({
			"item_templates": '<item_template id="5"/><item_template id="5"/><item_template/><item_template id="0"/><item_template id="6"/>',
			"npc_walker": '<walker_template route_id="a"/><walker_template route_id="A"/><walker_template route_id="a"/><walker_template/>',
		})
		self.assertEqual(self.line(counts, 321)["values"], [3])  # 5, 0 (absent == 0), 6
		self.assertEqual(self.line(counts, 345)["values"], [3])  # "a", "A", null

	def test_required_list_absent_is_a_java_crash(self):
		with self.assertRaisesRegex(OracleError, "NullPointerException"):
			self.counts({"world_maps": ""})
		counts = self.counts({"fly_rings": "", "material_templates": "", "timed_events": "", "house_parts": ""})
		self.assertEqual(self.line(counts, 361)["values"], [0])

	def test_non_canonical_key_is_rejected(self):
		for value in (" 1", "+1", "1.0", "", "2147483648"):
			with self.subTest(value=value):
				with self.assertRaises(OracleError):
					self.counts({"world_maps": f'<map id="{value}"/>'})

	def test_skill_tree_without_class_counts_every_player_class(self):
		counts = self.counts({"skill_tree": '<skill classId="WARRIOR"/><skill/><skill race="ELYOS"/>'})
		self.assertEqual(self.line(counts, 338)["values"], [1 + 17 + 17])
		with self.assertRaises(OracleError):
			self.counts({"skill_tree": '<skill classId="WIZARD"/>'})

	def test_guide_hash_keys(self):
		counts = self.counts({"guides": '<guide level="10"/><guide level="10" race="PC_ALL"/><guide level="10" race="ELYOS"/>'
		                                 '<guide level="10" classType="WARRIOR"/><guide level="11"/>'})
		self.assertEqual(self.line(counts, 364)["values"], [4])

	def test_siege_types(self):
		counts = self.counts({"siege_locations": '<siege_location id="1" type="FORTRESS"/><siege_location id="2" type="INDUN"/>'
		                                          '<siege_location id="3" type="AGENT_FIGHT"/><siege_location id="1" type="ARTIFACT"/>'})
		self.assertEqual(self.line(counts, 357)["values"], [2])
		with self.assertRaises(OracleError):
			self.counts({"siege_locations": '<siege_location id="1"/>'})

	def test_zones_skip_empty_spheres(self):
		counts = self.counts({"zones": '<zone><points/></zone><zone area_type="SPHERE"><sphere r="0"/></zone>'
		                               '<zone area_type="SPHERE"><sphere r="-1.5"/></zone><zone area_type="SPHERE"><sphere r="2"/></zone>'
		                               '<zone area_type="CYLINDER"><cylinder/></zone>'})
		self.assertEqual(self.line(counts, 347)["values"], [3])
		with self.assertRaises(OracleError):
			self.counts({"zones": '<zone area_type="CYLINDER"><points/></zone>'})

	def test_item_groups(self):
		body = MINIMAL_HOLDERS["item_groups"]
		body = body.replace("<craft_materials/>", '<craft_materials><item id="1"/><item id="1"/></craft_materials>')
		body = body.replace("<medals/>", '<medals><item id="3"/></medals>')  # not part of bonusSize
		body = body.replace("<feed_armor/>", '<feed_armor><item id="1"/><item id="1"/><item id="2"/></feed_armor>')
		body = body.replace("<feed_bone/>", '<feed_bone><item id="1"/></feed_bone>')
		body = body.replace("<feed_exclude/>", '<feed_exclude><item id="9"/></feed_exclude>')  # excluded from petFoodSize
		counts = self.counts({"item_groups": body + '<food><item id="4"/></food>'})  # a repeated single element: last wins
		self.assertEqual(self.line(counts, 323)["line"], "Loaded 3 bonus item group templates and 3 pet food items")
		with self.assertRaisesRegex(OracleError, "feed_soul"):
			self.counts({"item_groups": body.replace("<feed_soul/>", "")})

	def test_npc_shouts_and_npcs(self):
		counts = self.counts({"npc_shouts": '<shout_group><shout_npcs><shout/><shout/></shout_npcs><shout_npcs/></shout_group>'
		                                    '<shout_group><shout_npcs><shout/></shout_npcs></shout_group>'})
		self.assertEqual(self.line(counts, 327)["values"], [3])
		counts = self.counts({"npc_templates": '<npc_template npc_id="1" tribe="PET"/><npc_template npc_id="2"><stats/></npc_template>'})
		self.assertEqual(self.line(counts, 324)["values"], [2])
		with self.assertRaisesRegex(OracleError, "stats"):
			self.counts({"npc_templates": '<npc_template npc_id="1" tribe="GENERAL"/>'})

	def test_town_spawns_nested_last_wins(self):
		body = ('<spawn_map map_id="1"><town_spawn town_id="1"><town_level level="1"><spawn/><spawn/></town_level>'
		        '<town_level level="1"><spawn/></town_level><town_level level="2"><spawn/></town_level></town_spawn>'
		        '<town_spawn town_id="2"><town_level level="1"><spawn/></town_level></town_spawn></spawn_map>'
		        '<spawn_map map_id="2"><town_spawn town_id="3"><town_level level="1"><spawn/><spawn/><spawn/></town_level></town_spawn></spawn_map>'
		        '<spawn_map map_id="2"><town_spawn town_id="3"><town_level level="1"><spawn/></town_level></town_spawn></spawn_map>')
		counts = self.counts({"town_spawns_data": body})
		self.assertEqual(self.line(counts, 391)["values"], [(1 + 1) + 1 + 1])
		with self.assertRaises(OracleError):
			self.counts({"town_spawns_data": '<spawn_map map_id="1"><town_spawn town_id="1"><town_level level="1"/></town_spawn></spawn_map>'})

	def test_house_npcs_and_lands(self):
		counts = self.counts({"house_npcs": '<house address="1"><spawn type="A"/><spawn type="B"/></house><house address="2"/>'
		                                    '<house address="1"><spawn type="A"/></house>'})
		self.assertEqual(self.line(counts, 386)["values"], [1])
		with self.assertRaisesRegex(OracleError, "duplicate"):
			self.counts({"house_npcs": '<house address="1"><spawn type="A"/><spawn type="A"/></house>'})
		with self.assertRaisesRegex(OracleError, "duplicate"):
			self.counts({"house_lands": '<land><addresses><address id="1"/></addresses></land><land><addresses><address id="1"/></addresses></land>'})
		with self.assertRaisesRegex(OracleError, "duplicate"):
			self.counts({"staticdoor_templates": '<world world="1"/><world world="1"/>'})

	def test_decomposables(self):
		counts = self.counts({"decomposable_items": '<decomposable item_id="1"><items/></decomposable><decomposable item_id="2"/>'
		                                            '<decomposable item_id="3" selectable="true"><items/></decomposable>'
		                                            '<decomposable item_id="4" selectable="false"><items/></decomposable>'})
		self.assertEqual(self.line(counts, 367)["values"], [2])

	def test_lists_goods_portals_mails(self):
		counts = self.counts({
			"goodslists": '<list id="1"/><list id="1"/><in_list id="1"/><purchase_list id="2"/><purchase_list id="3"/>',
			"portal_templates2": '<portal_scroll name="a"/><portal_dialog npc_id="1"/><portal_use npc_id="1"/><portal_use npc_id="2"/>',
			"mails": '<mail name="Abc"/><mail name="aBC"/><mail name="x"/>',
			"cube_expander": '<expansion_npc ids="1 2"/><expansion_npc ids="2  3"/>',
			"npc_skill_templates": '<npc_skills npc_ids="5 6"/><npc_skills npc_ids="6"/>',
			"walker_versions": '<walk_parent><version id="a"/><version id="b"/></walk_parent><walk_parent><version id="a"/></walk_parent>',
		})
		self.assertEqual(self.line(counts, 348)["values"], [4])
		self.assertEqual(self.line(counts, 382)["values"], [4])
		self.assertEqual(self.line(counts, 326)["values"], [2])
		self.assertEqual(self.line(counts, 339)["values"], [3])
		self.assertEqual(self.line(counts, 355)["values"], [2])
		self.assertEqual(self.line(counts, 346)["values"], [2])
		with self.assertRaises(OracleError):
			self.counts({"goodslists": '<list id="1"/><in_list id="1"/>'})

	def test_global_drop_exclusion_suffix(self):
		counts = self.counts({"global_npc_exclusions": "<npc_ids></npc_ids>"})
		self.assertEqual(self.line(counts, 394)["line"], "Loaded 1 global drop rules with global drop npc exclusions")

	def test_same_holder_imported_twice_last_wins(self):
		self.tree.minimal()
		self.tree.write("more_maps.xml", xml("world_maps", '<map id="7"/><map id="8"/>'))
		text = (self.tree.root / "static_data.xml").read_text(encoding="utf-8")
		self.tree.write("static_data.xml", text.replace("</static_data>", '<import file="more_maps.xml"/></static_data>'))
		counts = runner.run(self.tree.root, totals=False, census=False, region_variants=False)["counts"]
		self.assertEqual(self.line(counts, 315)["values"], [2])
		self.assertEqual(counts["replacedHolders"], [{"tag": "world_maps", "byImport": "more_maps.xml"}])

	def test_directory_holder_collects_all_files(self):
		self.tree.minimal(skip=("spawns",))
		self.tree.write("spawns/b.xml", xml("spawns", '<spawn_map map_id="1"/><spawn_map map_id="2"/>', 'ignored="1"'))
		self.tree.write("spawns/A/a.xml", xml("spawns", '<spawn_map map_id="2"/><spawn_map map_id="3"/>'))
		text = (self.tree.root / "static_data.xml").read_text(encoding="utf-8")
		self.tree.write("static_data.xml", text.replace("</static_data>", '<import file="spawns" singleRootTag="true"/></static_data>'))
		counts = runner.run(self.tree.root, totals=False, census=False, region_variants=False)["counts"]
		self.assertEqual(self.line(counts, 374)["values"], [3])

	def test_unknown_root_is_rejected(self):
		self.tree.minimal()
		self.tree.write("x.xml", xml("not_a_holder"))
		text = (self.tree.root / "static_data.xml").read_text(encoding="utf-8")
		self.tree.write("static_data.xml", text.replace("</static_data>", '<import file="x.xml"/></static_data>'))
		with self.assertRaisesRegex(OracleError, "not a StaticData element"):
			runner.run(self.tree.root, totals=False, census=False, region_variants=False)

	def test_region_variants_report_changed_lines(self):
		self.tree.minimal()
		self.tree.write("goodslists_japan.xml", xml("goodslists", '<list id="1"/><list id="2"/><in_list id="1"/><purchase_list id="1"/>'))
		self.tree.write("goodslists_usa.xml", xml("goodslists", MINIMAL_HOLDERS["goodslists"]))
		counts = runner.run(self.tree.root, totals=False, census=False)["counts"]
		variants = {v["region"]: v for v in counts["regionVariants"]}
		self.assertEqual(sorted(variants), ["japan", "usa"])
		self.assertEqual(variants["usa"]["changedLines"], [])
		self.assertEqual(variants["japan"]["changedLines"], [{"javaLine": 348, "line": "Loaded 4 goodslist entries", "values": [4]}])
		japan = runner.run(self.tree.root, country_code=4, totals=False, census=False, region_variants=False)["counts"]
		self.assertEqual(self.line(japan, 348)["values"], [4])

	def test_evaluate_requires_all_holders(self):
		with self.assertRaises(OracleError):
			evaluate({})


class CompareCountsLogTest(unittest.TestCase):

	EXPECTED = {"lines": [{"line": "Loaded 161 maps"}, {"line": "Loaded 2 global drop rules with global drop npc exclusions"}]}

	def test_equal(self):
		log = "12:00:01 INFO [main] Loaded 161 maps\nLoaded 12 AI handlers\n12:00:02 INFO Loaded 2 global drop rules with global drop npc exclusions\n"
		self.assertEqual(runner.compare_counts_log(self.EXPECTED, log), [])

	def test_differences(self):
		log = "Loaded 160 maps\nLoaded 2 global drop rules with global drop npc exclusions\n"
		self.assertEqual(runner.compare_counts_log(self.EXPECTED, log), ["line 1: 'Loaded 160 maps', expected 'Loaded 161 maps'"])
		self.assertTrue(runner.compare_counts_log(self.EXPECTED, "Loaded 161 maps\nLoaded 2 global drop rules\n"))


if __name__ == "__main__":
	unittest.main()
