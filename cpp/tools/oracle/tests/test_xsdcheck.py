import unittest

from staticdata_oracle import OracleError
from staticdata_oracle.xsdcheck import check, load_schemas, parse_ir
from tests.support import Tree

SCHEMA = '<?xml version="1.0" encoding="UTF-8"?>\n<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">{}</xs:schema>\n'

ITEMS_XSD = SCHEMA.format("""
	<xs:include schemaLocation="../import.xsd"/>
	<xs:element name="item_templates">
		<xs:complexType>
			<xs:sequence>
				<xs:element ref="import" minOccurs="0" maxOccurs="unbounded"/>
				<xs:element name="item_template" type="ItemTemplate" minOccurs="0" maxOccurs="unbounded"/>
			</xs:sequence>
		</xs:complexType>
	</xs:element>
	<xs:complexType name="AbstractItem" abstract="true">
		<xs:attribute name="id" type="xs:int" use="required"/>
	</xs:complexType>
	<xs:complexType name="ItemTemplate">
		<xs:complexContent mixed="false">
			<xs:extension base="AbstractItem">
				<xs:sequence>
					<xs:element name="actions" minOccurs="0">
						<xs:complexType>
							<xs:choice minOccurs="0" maxOccurs="unbounded">
								<xs:element name="skilllearn" type="SkillLearnAction"/>
								<xs:element name="dye" type="DyeAction"/>
							</xs:choice>
						</xs:complexType>
					</xs:element>
					<xs:element name="stats" type="Stats" minOccurs="1"/>
					<xs:element name="addresses">
						<xs:complexType>
							<xs:sequence><xs:element name="address" maxOccurs="unbounded"><xs:complexType>
								<xs:attribute name="x" type="xs:float"/>
							</xs:complexType></xs:element></xs:sequence>
						</xs:complexType>
					</xs:element>
				</xs:sequence>
				<xs:attribute name="level" type="xs:int"/>
				<xs:attribute name="name">
					<xs:simpleType><xs:restriction base="xs:string"/></xs:simpleType>
				</xs:attribute>
			</xs:extension>
		</xs:complexContent>
	</xs:complexType>
	<xs:complexType name="SkillLearnAction"><xs:attribute name="skillid" type="xs:int" use="required"/></xs:complexType>
	<xs:complexType name="DyeAction"><xs:attribute name="color" type="xs:string"/></xs:complexType>
	<xs:complexType name="Stats"><xs:attribute name="hp" type="xs:int"/></xs:complexType>
""")

IMPORT_XSD = SCHEMA.format("""
	<xs:element name="import"><xs:complexType><xs:attribute name="file" type="xs:string" use="required"/></xs:complexType></xs:element>
	<!-- <xs:element name="commented"/> -->
""")

P = "com.aionemu.gameserver."
BASE_ALLOW = [{"class": P + "AbstractItem", "kind": "unmatchedClass", "reason": "only used as a base type"}]


def prop(java, node, name, required=False, type_fqn=None, wrapper=None, choices=None):
	p = {"javaName": java, "node": node, "xmlName": name, "required": required}
	if type_fqn:
		p["typeFqn"] = type_fqn
	if wrapper:
		p["wrapperName"] = wrapper
	if choices:
		p["choices"] = [{"xmlName": n, "typeFqn": t} for n, t in choices]
		p["xmlName"] = None
	return p


def ir(*classes):
	return {"format": "aion-xmlmodel", "version": 1, "classes": list(classes)}


def cls(fqn, props, superclass=None, type_name=None, root=None, transient=False):
	c = {"fqn": P + fqn, "superclass": P + superclass if superclass else None, "xmlTypeName": type_name, "xmlRootElement": root,
	     "properties": props}
	if transient:
		c["xmlTransient"] = True
	return c


def good_ir():
	return ir(
		cls("ItemData", [prop("its", "element", "item_template", type_fqn=P + "ItemTemplate")], root="item_templates", type_name=""),
		cls("VisibleObjectTemplate", [], transient=True),
		cls("AbstractItem", [prop("id", "attribute", "id", True)], superclass="VisibleObjectTemplate"),
		cls("ItemTemplate", [
			prop("level", "attribute", "level"),
			prop("name", "attribute", "name"),
			prop("actions", "element", "actions", type_fqn=P + "ItemActions"),
			prop("stats", "element", "stats", True, P + "Stats"),
			prop("addresses", "element", "address", True, P + "Address", wrapper="addresses"),
		], superclass="AbstractItem", type_name="ItemTemplate"),
		cls("ItemActions", [prop("actions", "element", None, choices=[("skilllearn", P + "SkillLearn"), ("dye", P + "Dye")])], type_name=""),
		cls("SkillLearn", [prop("skillid", "attribute", "skillid", True)], type_name="SkillLearnAction"),
		cls("Dye", [prop("color", "attribute", "color")], type_name="dye"),  # default JAXB name does not exist: matched via the choice
		cls("Stats", [prop("hp", "attribute", "hp")]),  # no type name, no root: matched via ItemTemplate.stats
		cls("Address", [prop("x", "attribute", "x")], type_name=""),
	)


class XsdReaderTest(unittest.TestCase):

	def setUp(self):
		self.tree = Tree()
		self.addCleanup(self.tree.close)
		self.tree.write("items/item_templates.xsd", ITEMS_XSD)
		self.tree.write("import.xsd", IMPORT_XSD)

	def test_inventory(self):
		inv = load_schemas(self.tree.root).inventory()
		self.assertEqual(inv["files"], 2)
		self.assertEqual(inv["counts"]["xs:attribute"], 8)
		self.assertEqual(inv["counts"]["xs:extension"], 1)
		self.assertEqual(inv["namedComplexTypes"], 5)
		self.assertEqual(inv["anonymousComplexTypes"], 5)
		self.assertEqual(inv["globalElements"], 2)

	def test_unsupported_construct(self):
		self.tree.write("bad.xsd", SCHEMA.format('<xs:complexType name="X"><xs:attributeGroup ref="g"/></xs:complexType>'))
		with self.assertRaisesRegex(OracleError, "attributeGroup"):
			load_schemas(self.tree.root)

	def test_matching_and_clean_report(self):
		report = check(good_ir(), load_schemas(self.tree.root), BASE_ALLOW)
		self.assertEqual(report["differences"], [])
		self.assertEqual(report["summary"]["allowlisted"], 1)
		self.assertTrue(report["ok"])
		routes = {m["class"][len(P):]: (m["xsdType"], m["route"]) for m in report["matches"]}
		self.assertEqual(routes["ItemData"], ("items/item_templates.xsd#element(item_templates)", "xmlRootElement"))
		self.assertEqual(routes["ItemTemplate"], ("items/item_templates.xsd#ItemTemplate", "xmlTypeName"))
		self.assertEqual(routes["Stats"], ("items/item_templates.xsd#Stats", f"property {P}ItemTemplate.stats"))
		self.assertEqual(routes["Dye"][0], "items/item_templates.xsd#DyeAction")
		self.assertEqual(routes["Address"][0], "items/item_templates.xsd#ItemTemplate/element(addresses)/element(address)")
		self.assertEqual(routes["ItemActions"][0], "items/item_templates.xsd#ItemTemplate/element(actions)")
		self.assertNotIn("VisibleObjectTemplate", routes)
		self.assertEqual(report["unmatchedClasses"], [P + "AbstractItem"])  # only reachable as a base
		self.assertFalse(check(good_ir(), load_schemas(self.tree.root))["ok"])  # unmatched classes fail unless allowlisted

	def test_differences_and_allowlist(self):
		doc = good_ir()
		item = doc["classes"][3]
		item["properties"][0]["xmlName"] = "lvl"  # misnamed attribute
		item["properties"][3]["required"] = False  # element required mismatch
		doc["classes"][2]["properties"][0]["required"] = False  # inherited attribute required mismatch
		doc["classes"].append(cls("Orphan", [], type_name="Nope"))
		report = check(doc, load_schemas(self.tree.root), [{"class": P + "AbstractItem", "kind": "unmatchedClass", "reason": "base"}])
		got = sorted((d["class"][len(P):], d["kind"], d["name"]) for d in report["differences"])
		self.assertEqual(got, [
			("ItemTemplate", "attributeMissingInIr", "level"),
			("ItemTemplate", "attributeMissingInXsd", "lvl"),
			("ItemTemplate", "attributeRequiredMismatch", "id"),
			("ItemTemplate", "elementRequiredMismatch", "stats"),
			("Orphan", "unmatchedClass", ""),
		])
		self.assertFalse(report["ok"])
		allow = [
			{"class": P + "ItemTemplate", "kind": "attribute*", "name": "*", "reason": "x"},
		]
		with self.assertRaises(OracleError):
			check(doc, load_schemas(self.tree.root), allow)
		allow = [
			{"class": "*.ItemTemplate", "name": "l*", "reason": "renamed"},
			{"class": P + "ItemTemplate", "kind": "attributeRequiredMismatch", "name": "id", "reason": "schema"},
			{"class": P + "ItemTemplate", "kind": "elementRequiredMismatch", "name": "stats", "reason": "schema"},
			{"class": "*", "kind": "unmatchedClass", "reason": "no type"},
			{"class": P + "Gone", "reason": "stale"},
		]
		report = check(doc, load_schemas(self.tree.root), allow)
		self.assertTrue(report["ok"])
		self.assertEqual(report["summary"]["allowlisted"], 6)
		self.assertEqual(report["staleAllowlist"], [{"class": P + "Gone", "reason": "stale"}])
		with self.assertRaisesRegex(OracleError, "reason"):
			check(doc, load_schemas(self.tree.root), [{"class": "*"}])

	def test_ambiguous_type_names(self):
		self.tree.write("other/stats.xsd", SCHEMA.format('<xs:complexType name="Stats"><xs:attribute name="mp" type="xs:int"/></xs:complexType>'))
		schemas = load_schemas(self.tree.root)
		self.assertEqual(schemas.inventory()["namesDefinedDifferently"], ["Stats"])
		doc = good_ir()
		report = check(doc, schemas, BASE_ALLOW)
		self.assertTrue(report["ok"])
		[amb] = report["ambiguous"]
		self.assertEqual((amb["class"], amb["chosen"]), (P + "Stats", "items/item_templates.xsd#Stats"))

	def test_ir_validation(self):
		with self.assertRaises(OracleError):
			parse_ir({"format": "aion-xmlmodel", "version": 2, "classes": []})
		bad = good_ir()
		bad["classes"][0]["properties"][0]["node"] = "text"
		with self.assertRaises(OracleError):
			parse_ir(bad)
		bad = good_ir()
		del bad["classes"][1]["superclass"]
		with self.assertRaises(OracleError):
			parse_ir(bad)


if __name__ == "__main__":
	unittest.main()
