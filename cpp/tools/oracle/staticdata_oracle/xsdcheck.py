"""V1: cross-check of the generator's IR (xmlmodel.json) against the XSDs in data/static_data.

The XSD reader is independent of the generator. It supports exactly the constructs the 96 XSDs use (schema, include, element with
name/ref/type/inline types, complexType, complexContent/extension, sequence/choice/all, attribute, simpleType, unique/key/selector/field,
annotation) and raises OracleError on anything else.

Matching an IR class to an XSD complex type, in this order:
1. xmlTypeName (non-empty) equals the name of a named complexType;
2. xmlRootElement equals the name of a global element (its named or anonymous type);
3. propagation: for a matched pair (class, type), each element property's typeFqn is matched to the type of the XSD element with the
   property's XML name (wrappers: the wrapper element's type, then the inner element; choices: each choice element).
When a name has several differing definitions (the same complexType name in several files), the one with the fewest differences wins
and the pair is listed under "ambiguous".

Compared per matched pair, on both sides flattened over the inheritance chain (IR superclass, XSD extension base):
attribute names, attribute required flags, element names, element required flags (IR required vs XSD minOccurs >= 1 outside a choice).
Element references to the global <import> element (XmlMerger's directive, allowed in every holder XSD) are ignored.
"""

from __future__ import annotations

import fnmatch
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path

from . import OracleError

XS = "{http://www.w3.org/2001/XMLSchema}"
IR_FORMAT = "aion-xmlmodel"
IR_VERSION = 1
REPORT_FORMAT = "aion-xsd-check"

DIFFERENCE_KINDS = ("attributeMissingInXsd", "attributeMissingInIr", "attributeRequiredMismatch", "elementMissingInXsd",
                    "elementMissingInIr", "elementRequiredMismatch", "unmatchedClass")


@dataclass
class ElementDecl:
	name: str
	file: str
	type_name: str | None = None
	anon: "ComplexType | None" = None
	ref: bool = False
	simple: bool = False  # inline simpleType or no type at all
	min_occurs: int = 1
	in_choice: bool = False


@dataclass
class ComplexType:
	ident: str
	file: str
	name: str | None = None
	abstract: bool = False
	base: str | None = None
	attributes: dict = field(default_factory=dict)  # name -> required
	elements: dict = field(default_factory=dict)  # name -> ElementDecl


class Schemas:

	def __init__(self):
		self.complex_types: dict[str, list[ComplexType]] = {}
		self.elements: dict[str, list[ElementDecl]] = {}
		self.simple_types: set[str] = set()
		self.anonymous: list[ComplexType] = []
		self.files: list[str] = []
		self.counts = {"xs:attribute": 0, "xs:complexType": 0, "xs:element": 0, "xs:simpleType": 0, "xs:extension": 0}

	# ---- reading

	def read(self, path: Path, rel: str) -> None:
		try:
			root = ET.parse(path).getroot()
		except ET.ParseError as e:
			raise OracleError(f"{rel}: {e}") from e
		if root.tag != XS + "schema":
			raise OracleError(f"{rel}: root is not xs:schema")
		self.files.append(rel)
		for child in root:
			tag = _local(child, rel)
			if tag in ("include", "annotation"):
				continue
			if tag == "element":
				decl = self._element(child, rel, f"element({child.get('name')})", False)
				if decl.ref:
					raise OracleError(f"{rel}: global element with ref")
				self.elements.setdefault(decl.name, []).append(decl)
			elif tag == "complexType":
				name = child.get("name")
				if not name:
					raise OracleError(f"{rel}: global complexType without name")
				ct = self._complex_type(child, rel, name, name)
				self.complex_types.setdefault(name, []).append(ct)
			elif tag == "simpleType":
				self.counts["xs:simpleType"] += 1
				self.simple_types.add(child.get("name"))
			else:
				raise OracleError(f"{rel}: unsupported top-level xs:{tag}")

	def _complex_type(self, node, rel, name, owner) -> ComplexType:
		self.counts["xs:complexType"] += 1
		if node.get("mixed", "false") != "false":
			raise OracleError(f"{rel}: mixed complexType {owner}")
		ct = ComplexType(f"{rel}#{owner}", rel, name, node.get("abstract", "false") == "true")
		if name is None:
			self.anonymous.append(ct)
		self._content(node, ct, rel, owner)
		return ct

	def _content(self, node, ct, rel, owner):
		for child in node:
			tag = _local(child, rel)
			if tag == "annotation":
				continue
			if tag in ("sequence", "choice", "all"):
				self._particle(child, ct, rel, owner, tag == "choice")
			elif tag == "attribute":
				self._attribute(child, ct, rel, owner)
			elif tag == "complexContent":
				if child.get("mixed", "false") != "false":
					raise OracleError(f"{rel}: mixed complexContent in {owner}")
				ext = [c for c in child if _local(c, rel) != "annotation"]
				if len(ext) != 1 or _local(ext[0], rel) != "extension":
					raise OracleError(f"{rel}: complexContent of {owner} must contain exactly one xs:extension")
				self.counts["xs:extension"] += 1
				ct.base = ext[0].get("base")
				if not ct.base or ":" in ct.base:
					raise OracleError(f"{rel}: extension base {ct.base!r} in {owner} is not a complex type name")
				self._content(ext[0], ct, rel, owner)
			else:
				raise OracleError(f"{rel}: unsupported xs:{tag} in complexType {owner}")

	def _particle(self, node, ct, rel, owner, in_choice):
		for child in node:
			tag = _local(child, rel)
			if tag == "annotation":
				continue
			if tag == "element":
				decl = self._element(child, rel, f"{owner}/element({child.get('name') or child.get('ref')})", in_choice)
				if decl.name in ct.elements:
					previous = ct.elements[decl.name]
					if (previous.type_name, previous.ref) != (decl.type_name, decl.ref):
						raise OracleError(f"{rel}: element {decl.name!r} declared twice with different types in {owner}")
				ct.elements[decl.name] = decl
			elif tag in ("sequence", "choice", "all"):
				self._particle(child, ct, rel, owner, in_choice or tag == "choice")
			else:
				raise OracleError(f"{rel}: unsupported xs:{tag} in content model of {owner}")

	def _element(self, node, rel, owner, in_choice) -> ElementDecl:
		self.counts["xs:element"] += 1
		name, ref = node.get("name"), node.get("ref")
		if (name is None) == (ref is None):
			raise OracleError(f"{rel}: element needs exactly one of name/ref ({owner})")
		min_occurs = int(node.get("minOccurs", "1"))
		decl = ElementDecl(name or ref, rel, node.get("type"), None, ref is not None, False, min_occurs, in_choice)
		for child in node:
			tag = _local(child, rel)
			if tag in ("annotation", "unique", "key", "keyref"):
				continue
			if tag == "complexType":
				if decl.type_name or decl.ref:
					raise OracleError(f"{rel}: element {owner} has both a type and an inline complexType")
				decl.anon = self._complex_type(child, rel, None, owner)
			elif tag == "simpleType":
				self.counts["xs:simpleType"] += 1
				decl.simple = True
			else:
				raise OracleError(f"{rel}: unsupported xs:{tag} in element {owner}")
		if not decl.ref and decl.type_name is None and decl.anon is None:
			decl.simple = True
		return decl

	def _attribute(self, node, ct, rel, owner):
		self.counts["xs:attribute"] += 1
		name = node.get("name")
		if name is None:
			raise OracleError(f"{rel}: attribute without name (ref) in {owner}")
		use = node.get("use", "optional")
		if use not in ("optional", "required", "prohibited"):
			raise OracleError(f"{rel}: attribute {name} has use={use!r}")
		for child in node:
			if _local(child, rel) not in ("annotation", "simpleType"):
				raise OracleError(f"{rel}: unsupported content in attribute {name} of {owner}")
			if _local(child, rel) == "simpleType":
				self.counts["xs:simpleType"] += 1
		if name in ct.attributes:
			raise OracleError(f"{rel}: attribute {name} declared twice in {owner}")
		ct.attributes[name] = use == "required"

	# ---- resolution

	def complex_type_of(self, decl: ElementDecl, context_file: str) -> list[ComplexType]:
		"""Candidate complex types of an element declaration ([] for simple content)."""
		if decl.ref:
			result = []
			for g in self.elements.get(decl.name, []):
				result += self.complex_type_of(g, context_file)
			if decl.name not in self.elements:
				raise OracleError(f"{decl.file}: unresolved element ref {decl.name!r}")
			return _dedupe(result)
		if decl.anon is not None:
			return [decl.anon]
		if decl.simple or decl.type_name is None or ":" in decl.type_name:
			return []
		if decl.type_name in self.complex_types:
			return self.complex_types[decl.type_name]
		if decl.type_name in self.simple_types:
			return []
		raise OracleError(f"{decl.file}: unresolved type {decl.type_name!r} of element {decl.name!r}")

	def base_of(self, ct: ComplexType) -> ComplexType | None:
		if ct.base is None:
			return None
		candidates = self.complex_types.get(ct.base)
		if not candidates:
			raise OracleError(f"{ct.ident}: unresolved extension base {ct.base!r}")
		same_file = [c for c in candidates if c.file == ct.file]
		if len(same_file) == 1:
			return same_file[0]
		distinct = _dedupe(candidates)
		if len(distinct) == 1:
			return distinct[0]
		raise OracleError(f"{ct.ident}: extension base {ct.base!r} is defined differently in {[c.file for c in distinct]}")

	def flattened(self, ct: ComplexType):
		attributes, elements, seen = {}, {}, set()
		chain = []
		t = ct
		while t is not None:
			if id(t) in seen:
				raise OracleError(f"{ct.ident}: cyclic extension")
			seen.add(id(t))
			chain.append(t)
			t = self.base_of(t)
		for t in reversed(chain):
			attributes.update(t.attributes)
			elements.update(t.elements)
		return attributes, elements

	def inventory(self) -> dict:
		return {
			"files": len(self.files),
			"counts": dict(self.counts),
			"namedComplexTypes": sum(len(v) for v in self.complex_types.values()),
			"distinctComplexTypeNames": len(self.complex_types),
			"anonymousComplexTypes": len(self.anonymous),
			"globalElements": sum(len(v) for v in self.elements.values()),
			"namesDefinedDifferently": sorted(n for n, v in self.complex_types.items() if len(_dedupe(v)) > 1),
		}


def _signature(ct: ComplexType):
	return (ct.name, ct.base, tuple(sorted(ct.attributes.items())),
	        tuple(sorted((n, d.type_name, d.ref, d.min_occurs, d.in_choice, d.anon is not None) for n, d in ct.elements.items())))


def _dedupe(types):
	result, seen = [], set()
	for t in types:
		sig = _signature(t) if t.name is not None else id(t)
		if sig not in seen:
			seen.add(sig)
			result.append(t)
	return result


def _local(node, rel):
	tag = node.tag
	if not isinstance(tag, str):
		raise OracleError(f"{rel}: unexpected node {tag!r}")
	if not tag.startswith(XS):
		raise OracleError(f"{rel}: non-XSD element {tag}")
	return tag[len(XS):]


def load_schemas(root: Path) -> Schemas:
	root = Path(root)
	schemas = Schemas()
	paths = sorted((p for p in root.rglob("*") if p.is_file() and p.suffix.lower() == ".xsd"), key=lambda p: p.relative_to(root).as_posix())
	if not paths:
		raise OracleError(f"{root}: no .xsd files")
	for p in paths:
		schemas.read(p, p.relative_to(root).as_posix())
	return schemas


# ---- IR side

@dataclass
class IrProperty:
	java_name: str
	node: str
	xml_name: str | None
	required: bool
	type_fqn: str | None
	wrapper_name: str | None
	choices: list  # [(xmlName, typeFqn)]


@dataclass
class IrClass:
	fqn: str
	superclass: str | None
	xml_type_name: str | None
	xml_root_element: str | None
	xml_transient: bool
	properties: list


def _req(obj, key, types, where, nullable=False):
	if key not in obj:
		raise OracleError(f"IR {where}: missing field {key!r}")
	v = obj[key]
	if v is None and nullable:
		return None
	if not isinstance(v, types):
		raise OracleError(f"IR {where}: field {key!r} has type {type(v).__name__}")
	return v


def parse_ir(doc: dict) -> dict[str, IrClass]:
	if doc.get("format") != IR_FORMAT or doc.get("version") != IR_VERSION:
		raise OracleError(f"IR: expected format {IR_FORMAT!r} version {IR_VERSION}")
	classes = {}
	for i, c in enumerate(_req(doc, "classes", list, "root")):
		where = f"classes[{i}]"
		fqn = _req(c, "fqn", str, where)
		props = []
		for j, p in enumerate(_req(c, "properties", list, fqn)):
			pw = f"{fqn}.properties[{j}]"
			node = _req(p, "node", str, pw)
			if node not in ("attribute", "element"):
				raise OracleError(f"IR {pw}: node must be 'attribute' or 'element'")
			choices = p.get("choices")
			if choices is not None:
				if node != "element" or not isinstance(choices, list) or not choices:
					raise OracleError(f"IR {pw}: choices must be a non-empty list on an element property")
				choices = [(_req(ch, "xmlName", str, pw), _req(ch, "typeFqn", str, pw)) for ch in choices]
			xml_name = _req(p, "xmlName", str, pw, nullable=choices is not None)
			wrapper = p.get("wrapperName")
			if wrapper is not None and (node != "element" or not isinstance(wrapper, str)):
				raise OracleError(f"IR {pw}: wrapperName must be a string on an element property")
			type_fqn = p.get("typeFqn")
			if type_fqn is not None and not isinstance(type_fqn, str):
				raise OracleError(f"IR {pw}: typeFqn must be a string")
			props.append(IrProperty(_req(p, "javaName", str, pw), node, xml_name, bool(_req(p, "required", bool, pw)), type_fqn, wrapper,
			                        choices or []))
		if fqn in classes:
			raise OracleError(f"IR: class {fqn} listed twice")
		classes[fqn] = IrClass(fqn, _req(c, "superclass", str, fqn, nullable=True), _req(c, "xmlTypeName", str, fqn, nullable=True),
		                       _req(c, "xmlRootElement", str, fqn, nullable=True), bool(c.get("xmlTransient", False)), props)
	for c in classes.values():
		seen = set()
		s = c.superclass
		while s is not None and s in classes:
			if s in seen:
				raise OracleError(f"IR: cyclic superclass chain at {c.fqn}")
			seen.add(s)
			s = classes[s].superclass
	return classes


def _ir_flattened(classes, cls):
	chain = []
	c = cls
	while c is not None:
		chain.append(c)
		c = classes.get(c.superclass) if c.superclass else None
	attributes, elements = {}, {}
	for c in reversed(chain):
		for p in c.properties:
			if p.node == "attribute":
				attributes[p.xml_name] = p.required
			elif p.choices:
				for name, _ in p.choices:
					elements[name] = False
			elif p.wrapper_name is not None:
				elements[p.wrapper_name] = p.required
			else:
				elements[p.xml_name] = p.required
	return attributes, elements, [p for c in reversed(chain) for p in c.properties]


def _differences(schemas, classes, cls, ct):
	ir_attrs, ir_elems, _ = _ir_flattened(classes, cls)
	x_attrs, x_decls = schemas.flattened(ct)
	x_elems = {n: (d.min_occurs >= 1 and not d.in_choice) for n, d in x_decls.items() if not (d.ref and n == "import")}
	diffs = []
	for name in sorted(set(ir_attrs) | set(x_attrs)):
		if name not in x_attrs:
			diffs.append(("attributeMissingInXsd", name))
		elif name not in ir_attrs:
			diffs.append(("attributeMissingInIr", name))
		elif ir_attrs[name] != x_attrs[name]:
			diffs.append(("attributeRequiredMismatch", name))
	for name in sorted(set(ir_elems) | set(x_elems)):
		if name not in x_elems:
			diffs.append(("elementMissingInXsd", name))
		elif name not in ir_elems:
			diffs.append(("elementMissingInIr", name))
		elif ir_elems[name] != x_elems[name]:
			diffs.append(("elementRequiredMismatch", name))
	return diffs


def check(ir_doc: dict, schemas: Schemas, allowlist=None) -> dict:
	classes = parse_ir(ir_doc)
	candidates: dict[str, tuple[str, list[ComplexType]]] = {}

	for cls in classes.values():
		if cls.xml_transient:
			continue
		if cls.xml_type_name and cls.xml_type_name in schemas.complex_types:
			candidates[cls.fqn] = ("xmlTypeName", schemas.complex_types[cls.xml_type_name])
		elif cls.xml_root_element and cls.xml_root_element in schemas.elements:
			types = []
			for decl in schemas.elements[cls.xml_root_element]:
				types += schemas.complex_type_of(decl, decl.file)
			if types:
				candidates[cls.fqn] = ("xmlRootElement", _dedupe(types))

	matches: dict[str, dict] = {}
	ambiguous = []

	def choose(fqn):
		route, types = candidates[fqn]
		scored = sorted(((len(_differences(schemas, classes, classes[fqn], t)), t.ident, t) for t in types), key=lambda x: (x[0], x[1]))
		best = scored[0][2]
		if len(types) > 1:
			ambiguous.append({"class": fqn, "chosen": best.ident, "candidates": sorted(t.ident for t in types)})
		matches[fqn] = {"route": route, "type": best}

	changed = True
	while changed:
		changed = False
		for fqn in sorted(candidates):
			if fqn not in matches:
				choose(fqn)
				changed = True
		for fqn in sorted(matches):
			cls, ct = classes[fqn], matches[fqn]["type"]
			_, x_decls = schemas.flattened(ct)
			_, _, props = _ir_flattened(classes, cls)
			for p in props:
				if p.node != "element":
					continue
				targets = p.choices if p.choices else [(p.xml_name, p.type_fqn)]
				for xml_name, type_fqn in targets:
					if type_fqn is None or type_fqn not in classes or type_fqn in candidates or classes[type_fqn].xml_transient:
						continue
					decls = x_decls
					if p.wrapper_name is not None:
						wrapper = x_decls.get(p.wrapper_name)
						inner = schemas.complex_type_of(wrapper, ct.file) if wrapper is not None else []
						if len(inner) != 1:
							continue
						_, decls = schemas.flattened(inner[0])
					decl = decls.get(xml_name)
					if decl is None:
						continue
					types = schemas.complex_type_of(decl, ct.file)
					if types:
						candidates[type_fqn] = (f"property {fqn}.{p.java_name}", _dedupe(types))
						changed = True

	allow = [_allow_entry(e, i) for i, e in enumerate(allowlist or [])]
	used = set()
	differences, allowlisted = [], 0

	def record(fqn, kind, name, xsd):
		nonlocal allowlisted
		for i, (c, k, n) in enumerate(allow):
			if fnmatch.fnmatchcase(fqn, c) and (k == "*" or k == kind) and fnmatch.fnmatchcase(name, n):
				used.add(i)
				allowlisted += 1
				return
		differences.append({"class": fqn, "kind": kind, "name": name, "xsdType": xsd})

	unmatched = []
	for fqn in sorted(classes):
		cls = classes[fqn]
		if cls.xml_transient:
			continue
		if fqn not in matches:
			unmatched.append(fqn)
			record(fqn, "unmatchedClass", "", None)
			continue
		ct = matches[fqn]["type"]
		for kind, name in _differences(schemas, classes, cls, ct):
			record(fqn, kind, name, ct.ident)

	matched_types = {id(m["type"]) for m in matches.values()}
	unmatched_types = sorted(t.ident for ts in schemas.complex_types.values() for t in ts if id(t) not in matched_types)
	stale = [allowlist[i] for i in range(len(allow)) if i not in used]
	return {
		"format": REPORT_FORMAT,
		"version": 1,
		"ok": not differences,
		"summary": {"classes": len(classes), "matched": len(matches), "unmatchedClasses": len(unmatched), "differences": len(differences),
		            "allowlisted": allowlisted, "staleAllowlistEntries": len(stale), "ambiguous": len(ambiguous)},
		"matches": [{"class": fqn, "xsdType": matches[fqn]["type"].ident, "route": matches[fqn]["route"]} for fqn in sorted(matches)],
		"ambiguous": sorted(ambiguous, key=lambda a: a["class"]),
		"differences": differences,
		"unmatchedClasses": unmatched,
		"unmatchedNamedXsdTypes": unmatched_types,
		"staleAllowlist": stale,
	}


def _allow_entry(entry, i):
	if not isinstance(entry, dict) or set(entry) - {"class", "kind", "name", "reason"}:
		raise OracleError(f"allowlist[{i}]: expected an object with class, kind, name, reason")
	kind = entry.get("kind", "*")
	if kind != "*" and kind not in DIFFERENCE_KINDS:
		raise OracleError(f"allowlist[{i}]: unknown kind {kind!r}")
	if not entry.get("reason"):
		raise OracleError(f"allowlist[{i}]: every entry needs a reason")
	return entry.get("class", "*"), kind, entry.get("name", "*")
