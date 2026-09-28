import unittest

from staticdata_oracle import OracleError
from staticdata_oracle.imports import java_parse_boolean, list_xml_files, ntfs_sort_key, resolve_imports
from tests.support import Tree, xml


class ImportResolutionTest(unittest.TestCase):

	def setUp(self):
		self.tree = Tree()
		self.addCleanup(self.tree.close)

	def resolve(self, country_code=99):
		return resolve_imports(self.tree.root / "static_data.xml", country_code)

	def test_file_import_keeps_root(self):
		self.tree.write("a/world_maps.xml", xml("world_maps"))
		self.tree.static_data('file="a/world_maps.xml"')
		[imp] = self.resolve()
		self.assertFalse(imp.is_directory)
		self.assertEqual(imp.rel, "a/world_maps.xml")
		self.assertEqual([(f.rel, f.skip_root_start, f.skip_root_end) for f in imp.files], [("a/world_maps.xml", False, False)])

	def test_region_override_applies_to_regular_files_only(self):
		self.tree.write("goods/goodslists.xml", xml("goodslists"))
		self.tree.write("goods/goodslists_europe.xml", xml("goodslists"))
		self.tree.write("npcs/a.xml", xml("npc_templates"))
		self.tree.mkdir("npcs_europe")  # a directory never overrides
		self.tree.write("npcs_europe/b.xml", xml("npc_templates"))
		self.tree.static_data('file="goods/goodslists.xml"', 'file="npcs" singleRootTag="true"')
		goods, npcs = self.resolve(2)
		self.assertEqual(goods.rel, "goods/goodslists_europe.xml")
		self.assertTrue(goods.region_override)
		self.assertEqual(npcs.rel, "npcs")
		self.assertFalse(npcs.region_override)
		goods, _ = self.resolve(99)
		self.assertEqual(goods.rel, "goods/goodslists.xml")
		goods, _ = self.resolve(1)  # no _usa variant
		self.assertEqual(goods.rel, "goods/goodslists.xml")
		goods, _ = self.resolve(3)  # code without region
		self.assertEqual(goods.rel, "goods/goodslists.xml")

	def test_region_override_of_name_without_extension(self):
		self.tree.write("data", "<x/>")
		self.tree.write("data_russia", xml("world_maps"))
		self.tree.static_data('file="data"')
		[imp] = self.resolve(7)
		self.assertEqual(imp.rel, "data_russia")

	def test_directory_order_is_depth_first_uppercase_ordinal(self):
		for rel in ["d/b.xml", "d/A.xml", "d/_x.xml", "d/a_dir/z.xml", "d/a_dir/Y.XML", "d/B_dir/c.xml", "d/notes.txt", "d/b.xsd", "d/[1].xml"]:
			self.tree.write(rel, xml("spawns"))
		self.tree.static_data('file="d" singleRootTag="true"')
		[imp] = self.resolve()
		# uppercase ordinal: "A.XML" < "A_DIR" < "B.XML" < "B_DIR" < "[1].XML" < "_X.XML" ('.'=0x2E < '_'=0x5F, '['=0x5B < '_')
		self.assertEqual([f.rel for f in imp.files], ["d/A.xml", "d/a_dir/Y.XML", "d/a_dir/z.xml", "d/b.xml", "d/B_dir/c.xml", "d/[1].xml",
		                                             "d/_x.xml"])
		self.assertEqual([f.skip_root_start for f in imp.files], [False] + [True] * 6)
		self.assertTrue(all(f.skip_root_end for f in imp.files))

	def test_non_recursive_directory(self):
		self.tree.write("d/a.xml", xml("spawns"))
		self.tree.write("d/sub/b.xml", xml("spawns"))
		self.tree.static_data('file="d" singleRootTag="true" recursiveImport="FALSE"')
		[imp] = self.resolve()
		self.assertEqual([f.rel for f in imp.files], ["d/a.xml"])

	def test_ntfs_sort_key(self):
		self.assertLess(ntfs_sort_key("Zeta.xml"), ntfs_sort_key("_a.xml"))
		self.assertLess(ntfs_sort_key("a.xml"), ntfs_sort_key("a_b.xml"))
		with self.assertRaises(OracleError):
			ntfs_sort_key("café.xml")

	def test_case_collision_is_rejected(self):
		self.tree.write("d/a.xml", xml("spawns"))
		try:
			self.tree.write("d/A.xml", xml("spawns"))
		except OSError:
			self.skipTest("file system cannot store names differing only in case")
		if len(list((self.tree.root / "d").iterdir())) < 2:
			self.skipTest("case-insensitive file system")
		with self.assertRaises(OracleError):
			list_xml_files(self.tree.root / "d", True)

	def test_java_parse_boolean(self):
		self.assertTrue(java_parse_boolean("TRUE"))
		self.assertTrue(java_parse_boolean("true"))
		self.assertFalse(java_parse_boolean("1"))
		self.assertFalse(java_parse_boolean("yes"))

	def test_errors(self):
		cases = {
			"missing file": ('file="nope.xml"',),
			"missing file attribute": ('singleRootTag="true"',),
			"unknown attribute": ('file="w.xml" skipRoot="true"',),
			"directory without singleRootTag": ('file="d"',),
			"empty directory": ('file="e" singleRootTag="true"',),
		}
		self.tree.write("w.xml", xml("world_maps"))
		self.tree.write("d/a.xml", xml("spawns"))
		self.tree.mkdir("e")
		for name, imports in cases.items():
			with self.subTest(name):
				self.tree.static_data(*imports)
				with self.assertRaises(OracleError):
					self.resolve()

	def test_other_elements_in_static_data_are_rejected(self):
		self.tree.write("static_data.xml", xml("static_data", '<import file="w.xml"/><comment_like/>'))
		self.tree.write("w.xml", xml("world_maps"))
		with self.assertRaises(OracleError):
			self.resolve()


if __name__ == "__main__":
	unittest.main()
