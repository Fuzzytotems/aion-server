import unittest

from staticdata_oracle import OracleError
from staticdata_oracle import run as runner
from staticdata_oracle.census import CensusVisitor, classify, markdown_report
from staticdata_oracle.merged import stream_static_data
from staticdata_oracle.totals import TotalsVisitor, compare
from tests.support import HEADER, Tree, xml


class MergedStreamTest(unittest.TestCase):

	def setUp(self):
		self.tree = Tree()
		self.addCleanup(self.tree.close)

	def test_events(self):
		self.tree.write("d/a.xml", xml("npc_walker", '<walker_template route_id="1"><routestep step="1"/></walker_template>', 'version="1"'))
		self.tree.write("d/b.xml", xml("npc_walker", '<!-- c --><walker_template route_id="2"/>', 'version="2" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:x="y"'))
		self.tree.static_data('file="d" singleRootTag="true"')

		class Recorder:
			def __init__(self):
				self.events = []

			def begin_holder(self, imp, file, tag, attrib):
				self.events.append(("holder", file.rel, tag, attrib))

			def skipped_root(self, imp, file, tag, attrib):
				self.events.append(("skipped", file.rel, tag, sorted(attrib)))

			def start(self, path, tag, attrib):
				self.events.append(("start", path, dict(attrib)))

			def holder_child(self, element):
				self.events.append(("child", element.tag, len(element)))

			def end_holder(self, imp):
				self.events.append(("end",))

		r = Recorder()
		stream_static_data(self.tree.root, [r])
		self.assertEqual(r.events, [
			("holder", "d/a.xml", "npc_walker", {"version": "1"}),
			("start", ("walker_template",), {"route_id": "1"}),
			("start", ("walker_template", "routestep"), {"step": "1"}),
			("child", "walker_template", 1),
			("skipped", "d/b.xml", "npc_walker", ["version", "{http://www.w3.org/2001/XMLSchema-instance}x"]),
			("start", ("walker_template",), {"route_id": "2"}),
			("child", "walker_template", 0),
			("end",),
		])

	def test_nested_root_name_in_single_root_directory(self):
		self.tree.write("d/a.xml", xml("spawns", "<x><spawns/></x>"))
		self.tree.static_data('file="d" singleRootTag="true"')
		with self.assertRaisesRegex(OracleError, "root tag name"):
			stream_static_data(self.tree.root, [])

	def test_byte_order_mark_is_rejected(self):
		self.tree.write("w.xml", "", raw=b"\xef\xbb\xbf" + xml("world_maps").encode())
		self.tree.static_data('file="w.xml"')
		with self.assertRaisesRegex(OracleError, "byte order mark"):
			stream_static_data(self.tree.root, [])

	def test_malformed_xml(self):
		self.tree.write("w.xml", HEADER + "<world_maps><map></world_maps>")
		self.tree.static_data('file="w.xml"')
		with self.assertRaisesRegex(OracleError, "w.xml"):
			stream_static_data(self.tree.root, [])


class TotalsTest(unittest.TestCase):

	def setUp(self):
		self.tree = Tree()
		self.addCleanup(self.tree.close)

	def totals(self):
		v = TotalsVisitor()
		stream_static_data(self.tree.root, [v])
		return v.result()

	def test_totals(self):
		self.tree.write("w.xml", xml("world_maps", '<map id="1" name="a"/><map id="2"><ai_info/></map>',
		                             'xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="w.xsd"'))
		self.tree.write("s/1.xml", xml("spawns", '<spawn_map map_id="1"/>', 'a="1"'))
		self.tree.write("s/2.xml", xml("spawns", '<spawn_map map_id="2"/>', 'b="2"'))
		self.tree.static_data('file="w.xml"', 'file="s" singleRootTag="true"')
		t = self.totals()
		self.assertEqual(t["elements"], 1 + 2 + 1 + 1 + 2)
		self.assertEqual(t["attributes"], 3 + 1 + 2)
		self.assertEqual(t["byTag"]["map"], {"count": 2, "attributes": {"id": 2, "name": 1}})
		self.assertEqual(t["byTag"]["spawns"], {"count": 1, "attributes": {"a": 1}})
		self.assertEqual(t["namespaceAttributes"], {"xsi:noNamespaceSchemaLocation": 1})
		self.assertEqual(t["skippedRoots"], [{"file": "s/2.xml", "tag": "spawns", "attributes": ["b"]}])
		self.assertEqual([(h["import"], h["files"], h["elements"], h["attributes"]) for h in t["byImport"]],
		                 [("w.xml", 1, 4, 3), ("s", 2, 3, 3)])
		self.assertEqual(compare(t, t), [])
		other = dict(t, byTag=dict(t["byTag"], map={"count": 3, "attributes": {"id": 2}}))
		self.assertEqual(compare(t, other), ["<map>: 3 elements, expected 2", "<map> @name: 0, expected 1"])
		with self.assertRaises(OracleError):
			compare(t, {"format": "x"})


class CensusTest(unittest.TestCase):

	def test_classify(self):
		cases = {
			"": ("empty", ("EMPTY",)),
			"  ": ("blank", ("BLANK",)),
			"12": ("int", ()),
			"-0": ("int-noncanonical", ("NEGATIVE_ZERO",)),
			"007": ("int-noncanonical", ("LEADING_ZERO",)),
			"+5": ("int-noncanonical", ("PLUS_SIGN",)),
			" 5": ("int", ("WHITESPACE_EDGE",)),
			"3000000000": ("long", ("INT_OVERFLOW",)),
			"99999999999999999999": ("bigint", ("INT_OVERFLOW", "LONG_OVERFLOW")),
			"1.5": ("decimal", ()),
			"5.": ("decimal-edge", ("DECIMAL_EDGE",)),
			".5": ("decimal-edge", ("DECIMAL_EDGE",)),
			"1e5": ("exponent", ("EXPONENT",)),
			"-1.4E-5": ("exponent", ("EXPONENT",)),
			"1.5f": ("float-suffix", ("FLOAT_SUFFIX",)),
			"NaN": ("float-special", ("FLOAT_SPECIAL",)),
			"0x1F": ("hex", ("HEX",)),
			"true": ("bool", ()),
			"True": ("bool-noncanonical", ("BOOL_NONCANONICAL",)),
			"PC_ALL": ("enum", ()),
			"aggressive": ("identifier", ()),
			"1 2 3": ("int-list", ()),
			"1  2": ("int-list", ("MULTI_SPACE",)),
			"1.5 2": ("decimal-list", ()),
			"ELYOS ASMODIANS": ("enum-list", ()),
			"Mon,Tue": ("comma-list", ()),
			"2020-01-01T10:00": ("datetime", ()),
			"2020-01-01": ("date", ()),
			"10:30": ("time", ()),
			"Hello world!": ("text", ()),
			"Hello  world!": ("text", ("MULTI_SPACE",)),
			"café": ("text", ("NON_ASCII",)),
			"a\tb.": ("text", ("CONTROL_CHAR",)),
			"A\tB": ("enum-list", ("CONTROL_CHAR",)),
		}
		for value, (shape, flags) in cases.items():
			with self.subTest(value=value):
				s, f, _ = classify(value)
				self.assertEqual((s, f), (shape, flags))
		self.assertEqual(classify("-12")[2], -12)

	def test_census_document(self):
		tree = Tree()
		self.addCleanup(tree.close)
		maps = "".join(f'<map id="{i}" flag="{f}" name="n{i}"/>' for i, f in enumerate(["true", "false", "1", "", "TRUE", "007"]))
		maps += '<map id="100"><desc>  text  </desc><desc>x<b/>tail</desc></map>'
		tree.write("w.xml", xml("world_maps", maps))
		tree.static_data('file="w.xml"')
		v = CensusVisitor()
		stream_static_data(tree.root, [v])
		c = v.result()
		flag = c["paths"]["world_maps/map@flag"]
		self.assertEqual(flag["count"], 6)
		self.assertEqual(flag["elementCount"], 7)
		self.assertEqual(flag["flags"], {"BOOL_MIXED_NUMERIC": 1, "BOOL_NONCANONICAL": 1, "EMPTY": 1, "ENUM_CASE_VARIANTS": 1,
		                                 "LEADING_ZERO": 1})
		self.assertEqual(flag["values"], {"": 1, "007": 1, "1": 1, "TRUE": 1, "false": 1, "true": 1})
		self.assertEqual(flag["intRange"], [1, 7])
		self.assertEqual(c["paths"]["world_maps/map@id"]["intRange"], [0, 100])
		self.assertNotIn("flags", c["paths"]["world_maps/map@id"])
		text = c["paths"]["world_maps/map/desc#text"]
		self.assertEqual(text["count"], 2)
		self.assertEqual(text["flags"], {"MIXED_CONTENT": 1, "WHITESPACE_EDGE": 1})
		self.assertEqual(c["summary"]["attributeValues"], 6 * 3 + 1)
		report = markdown_report(c)
		self.assertIn("### `world_maps/map@flag`", report)
		self.assertNotIn("world_maps/map@id`", report)

	def test_value_limit(self):
		tree = Tree()
		self.addCleanup(tree.close)
		tree.write("w.xml", xml("world_maps", "".join(f'<map id="{i}"/>' for i in range(30)) + f'<map name="{"x" * 100}"/>'))
		tree.static_data('file="w.xml"')
		v = CensusVisitor()
		stream_static_data(tree.root, [v])
		c = v.result()
		self.assertEqual(c["paths"]["world_maps/map@id"]["distinct"], ">24")
		self.assertNotIn("values", c["paths"]["world_maps/map@id"])
		self.assertTrue(c["paths"]["world_maps/map@name"]["distinct"].startswith("not recorded"))


class RunOutputsTest(unittest.TestCase):

	def test_outputs_are_deterministic(self):
		tree = Tree()
		self.addCleanup(tree.close)
		tree.minimal()
		first = runner.output_files(runner.run(tree.root))
		second = runner.output_files(runner.run(tree.root))
		self.assertEqual(first, second)
		self.assertEqual(sorted(first), ["census.json", "census_report.md", "static_data_counts.json", "static_data_counts.txt", "totals.json"])
		self.assertEqual(first["static_data_counts.txt"].count("\n"), 90)


if __name__ == "__main__":
	unittest.main()
