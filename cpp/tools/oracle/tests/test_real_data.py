"""Runs V2/V3/V4 over the real game-server/data/static_data and checks the committed documents in expected/.

Regenerate after an intentional data or rule change with `python oracle.py generate` (or AION_ORACLE_UPDATE=1 when running the tests).
Skipped when the Java tree is not present.
"""

import os
import re
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

from staticdata_oracle import enums
from staticdata_oracle import run as runner
from staticdata_oracle.counts import HOLDERS, LINES
from staticdata_oracle.imports import list_xml_files, resolve_imports
from staticdata_oracle.xsdcheck import check, load_schemas

DATA = runner.DEFAULT_STATIC_DATA
_SCHEMAS = []


def xsdcheck_run(ir):
	if not _SCHEMAS:
		_SCHEMAS.append(load_schemas(DATA))
	return check(ir, _SCHEMAS[0])
JAVA = runner.TOOL_DIR.parents[2] / "game-server" / "src" / "com" / "aionemu" / "gameserver"


def java_enum_constants(path: Path, name: str) -> list[str]:
	text = path.read_text(encoding="utf-8")
	text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
	text = re.sub(r"//[^\n]*", "", text)
	m = re.search(r"\benum\s+" + name + r"\b[^{]*\{", text)
	if not m:
		raise AssertionError(f"enum {name} not found in {path}")
	body, depth, constants, current = text[m.end():], 0, [], ""
	for ch in body:
		if depth == 0 and ch in ";}":
			break
		if ch in "({":
			depth += 1
		elif ch in ")}":
			depth -= 1
		if depth == 0 and ch == ",":
			constants.append(current)
			current = ""
		else:
			current += ch
	constants.append(current)
	names = []
	for c in constants:
		c = re.sub(r"@\w+(\([^)]*\))?", "", c).strip()
		if c:
			names.append(re.match(r"[A-Za-z_][A-Za-z0-9_]*", c).group(0))
	return names


@unittest.skipUnless((DATA / "static_data.xml").is_file(), "game-server/data/static_data not present")
class RealStaticDataTest(unittest.TestCase):

	@classmethod
	def setUpClass(cls):
		cls.result = runner.run(DATA)
		cls.files = runner.output_files(cls.result)

	def test_expected_documents_are_current(self):
		if os.environ.get("AION_ORACLE_UPDATE") == "1":
			runner.write_outputs(self.result, runner.EXPECTED_DIR)
		stale = []
		for name, content in self.files.items():
			path = runner.EXPECTED_DIR / name
			if not path.is_file() or path.read_text(encoding="utf-8") != content:
				stale.append(name)
		self.assertEqual(stale, [], "expected/ is out of date: run `python oracle.py generate` and review the diff")

	def test_anchor_counts(self):
		values = {line["javaLine"]: line["values"] for line in self.result["counts"]["lines"]}
		anchors = {315: [161], 321: [102009], 324: [63287], 335: [13570], 342: [8043], 345: [6449], 347: [3978], 350: [12494], 391: [2450]}
		for java_line, expected in anchors.items():
			self.assertEqual(values[java_line], expected, java_line)
		self.assertEqual(self.result["counts"]["extras"]["xml_quests"]["value"], 4184)
		self.assertEqual(sum(len(v) for v in values.values()), 92)
		by_tag = self.result["totals"]["byTag"]
		self.assertEqual(by_tag["item_template"]["count"], 102009)
		self.assertEqual(by_tag["npc_template"]["count"], 63287)
		self.assertEqual(by_tag["routestep"]["count"], 112765)
		imports = {h["import"]: h for h in self.result["totals"]["byImport"]}
		self.assertEqual((imports["spawns"]["files"], imports["zones"]["files"], imports["npc_walker"]["files"]), (211, 149, 13))
		self.assertEqual(imports["npc_walker"]["elements"], 1 + 6449 + 112765)

	def test_census_finds_the_known_present_empty_values(self):
		paths = self.result["census"]["paths"]
		self.assertEqual(paths["mails/mail/template@name"]["flags"]["EMPTY"], 17)
		self.assertEqual(paths["world_maps/map@flags"]["flags"]["EMPTY"], 1)
		self.assertEqual(paths["npc_shouts/shout_group@client_ai"]["flags"]["EMPTY"], 1)

	@unittest.skipUnless(os.name == "nt", "NTFS enumeration order can only be observed on Windows")
	def test_directory_order_matches_file_system_enumeration(self):
		def scandir_order(directory, out):
			with os.scandir(directory) as it:
				for entry in it:
					if entry.is_dir(follow_symlinks=False):
						scandir_order(entry.path, out)
					elif entry.name.lower().endswith(".xml"):
						out.append(Path(entry.path))
			return out

		checked = 0
		for imp in resolve_imports(DATA / "static_data.xml"):
			if imp.is_directory:
				directory = DATA / imp.rel
				self.assertEqual(list_xml_files(directory, True), scandir_order(directory, []), imp.rel)
				checked += len(imp.files)
		self.assertGreater(checked, 500)

	@unittest.skipUnless(JAVA.is_dir(), "Java sources not present")
	def test_enum_tables_match_java(self):
		for name, rel in enums.ENUM_SOURCES.items():
			with self.subTest(enum=name):
				self.assertEqual(tuple(java_enum_constants(JAVA / rel, name)), enums.BY_NAME[name])

	@unittest.skipUnless(JAVA.is_dir(), "Java sources not present")
	def test_tribe_literals_are_java_constants(self):
		tribes = set(java_enum_constants(JAVA / "model" / "TribeClass.java", "TribeClass"))
		names = {t.get("name") for t in ET.parse(DATA / "tribe" / "tribe_relations.xml").getroot().iter("tribe")}
		self.assertEqual(names - tribes, set())
		signets = set(java_enum_constants(JAVA / "skillengine" / "model" / "SignetEnum.java", "SignetEnum"))
		self.assertEqual(signets, set(enums.SIGNET_ENUM))

	@unittest.skipUnless(JAVA.is_dir(), "Java sources not present")
	def test_holder_and_line_tables_match_static_data_java(self):
		lines = (JAVA / "dataholders" / "StaticData.java").read_text(encoding="utf-8").splitlines()
		elements = re.findall(r'@XmlElement\(name = "([a-z0-9_]+)"\)', "\n".join(lines))
		self.assertEqual(sorted(elements), sorted(HOLDERS))
		for java_line, template, _ in LINES:
			source = lines[java_line - 1]
			m = re.match(r'\s*log\.info\((.*)\);\s*$', source)
			self.assertIsNotNone(m, f"StaticData.java:{java_line} is not a log.info statement")
			expr = re.sub(r"\s*\+\s*\(.*\)$", ' + X + ""', m.group(1))
			java_template = re.sub(r'"\s*\+\s*[^"]+?\s*\+\s*"', "{}", expr)
			self.assertEqual(java_template, f'"{template}"', f"StaticData.java:{java_line}")

	def test_xsd_check_on_real_schema(self):
		p = "com.aionemu.gameserver."
		attributes = {"id": True, "cName": True, "name": False, "name_id": False, "twin_count": False, "beginner_twin_count": False,
		              "max_user": False, "prison": False, "instance": False, "death_level": True, "water_level": True, "world_type": False,
		              "world_size": False, "drop_type": False, "except_buff": False, "flags": False, "pve_attack_ratio": False,
		              "pve_defend_ratio": False}  # WorldMapTemplate.java
		props = [{"javaName": n, "node": "attribute", "xmlName": n, "required": r} for n, r in attributes.items()]
		props.append({"javaName": "aiInfo", "node": "element", "xmlName": "ai_info", "required": False})
		ir = {"format": "aion-xmlmodel", "version": 1, "classes": [
			{"fqn": p + "dataholders.WorldMapsData", "superclass": None, "xmlTypeName": None, "xmlRootElement": "world_maps", "properties": [
				{"javaName": "worldMaps", "node": "element", "xmlName": "map", "required": False,
				 "typeFqn": p + "model.templates.world.WorldMapTemplate"}]},
			{"fqn": p + "model.templates.world.WorldMapTemplate", "superclass": None, "xmlTypeName": None, "xmlRootElement": "map",
			 "properties": props},
		]}
		report = xsdcheck_run(ir)
		self.assertEqual(report["matches"][1]["xsdType"], "world_maps.xsd#element(world_maps)/element(map)")
		self.assertEqual([(d["kind"], d["name"]) for d in report["differences"]],
		                 [("attributeRequiredMismatch", "flags"), ("attributeRequiredMismatch", "world_size")])

	def test_xsd_inventory(self):
		inventory = load_schemas(DATA).inventory()
		self.assertEqual(inventory["files"], 96)
		self.assertEqual(inventory["counts"]["xs:complexType"], 744)
		self.assertEqual(inventory["counts"]["xs:attribute"], 1429)
		self.assertEqual(inventory["namesDefinedDifferently"], [])


if __name__ == "__main__":
	unittest.main()
