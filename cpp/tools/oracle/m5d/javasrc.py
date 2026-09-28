"""Readers for the Java sources the m5d oracles model, so that a change in the Java tree is a test failure and not a silently wrong expectation.

- A small JAXB model: the @XmlAttribute/@XmlElement bindings of the quest template classes (QuestTemplate and model/templates/quest/*,
  XMLQuest and the questEngine/handlers/models/*Data classes), with each field's Java type and initializer, read from the class bodies
  (XmlAccessType.FIELD: an instance field without annotation is an element named after the field; static, transient and @XmlTransient fields
  are not bound). unmarshal() turns an ElementTree element into the values JAXB would put into the fields, and refuses what it does not model
  (an @XmlEnumValue, @XmlValue, @XmlJavaTypeAdapter, a float field, an enum literal that is not a constant, a single-valued element given twice).
- XMLQuests.java's @XmlElements (the 16 tags of quest_script_data and their data classes).
- The enum and constant tables: DialogAction (public static final int), DialogPage (ids and getRewardPageByIndex), QuestStatus (value()),
  Race, Gender, PlayerClass, QuestCategory, QuestExtraCategory, QuestMentorType, QuestTarget, QuestRepeatCycle, BonusType, AbyssRankEnum.
- The configuration: @Property(key, defaultValue) of a Config class, the value of the key in config/{administration,main,network} and the
  profile over them (Config.loadProperties, Config.java:81-95, loads those folders as defaults and config/mygs.properties over them), plus
  every timed event that overrides the key through <config_properties> (Config.load puts EventService's active event properties over
  everything, :47-48).
- The Java quest handlers under data/handlers/quest (QuestHandlerLoader): quest id from `super(N)` and the start npcs of every
  `qe.registerQuestNpc(X).addOnQuestStart(...)` - a text scan, resolved only for literals, int fields, int array fields and loops over them.
- Java's HashMap/HashSet iteration order (java.util.HashMap: putVal, resize, treeifyBin), for the registration order of XMLQuests.getAllQuests
  and the iteration order of a QuestNpc's onQuestStart set.
"""

from __future__ import annotations

import re
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path

from staticdata_oracle import OracleError

from m5a.data import java_boolean, java_int

GS = Path("com") / "aionemu" / "gameserver"


def read_source(path: Path) -> str:
	try:
		return Path(path).read_text(encoding="utf-8")
	except OSError as e:
		raise OracleError(f"{path}: {e}") from e


_LEXEMES = re.compile(r"//[^\n]*|/\*.*?\*/|/\*|\"(?:\\.|[^\"\\\n])*\"|'(?:\\.|[^'\\\n])*'", re.DOTALL)


def strip_comments(text: str) -> str:
	"""The source with // and /* */ comments replaced by spaces (newlines kept, so offsets and line numbers stay), string and char literals
	left alone (a "//" inside a string is not a comment)."""

	def blank(match: re.Match) -> str:
		lexeme = match.group(0)
		if lexeme == "/*":
			raise OracleError("unterminated block comment")
		if lexeme[0] in "\"'":
			return lexeme
		return re.sub(r"[^\n]", " ", lexeme)

	return _LEXEMES.sub(blank, text)


def line_of(text: str, offset: int) -> int:
	return text.count("\n", 0, offset) + 1


def _matching(text: str, start: int, open_ch: str, close_ch: str) -> int:
	"""Index of the bracket closing the one at `start` (string literals skipped)."""
	depth = 0
	i = start
	while i < len(text):
		c = text[i]
		if c in "\"'":
			j = i + 1
			while j < len(text) and text[j] != c:
				j += 2 if text[j] == "\\" else 1
			i = j + 1
			continue
		if c == open_ch:
			depth += 1
		elif c == close_ch:
			depth -= 1
			if depth == 0:
				return i
		i += 1
	raise OracleError(f"unbalanced {open_ch}{close_ch}")


@dataclass(frozen=True)
class JavaField:
	name: str
	type: str
	modifiers: frozenset[str]
	annotations: tuple[tuple[str, str | None], ...]  # (name, argument text inside the parentheses or None), in source order
	initializer: str | None
	line: int

	def annotation(self, name: str) -> str | None:
		for ann, args in self.annotations:
			if ann == name:
				return args if args is not None else ""
		return None

	def has(self, name: str) -> bool:
		return any(ann == name for ann, _ in self.annotations)


@dataclass(frozen=True)
class JavaClass:
	name: str
	kind: str  # class | enum
	extends: str | None
	annotations: tuple[str, ...]  # class-level annotation names
	accessor_type: str | None  # the XmlAccessType of @XmlAccessorType
	fields: tuple[JavaField, ...]
	path: Path
	text: str  # comment-stripped source


_ANNOTATION = re.compile(r"@(\w+)\s*")
_MODIFIERS = {"public", "protected", "private", "static", "final", "transient", "volatile"}


def parse_class(path: Path, name: str | None = None) -> JavaClass:
	"""The declaration of class `name` (default: the file's stem) and its fields at the top level of the class body."""
	text = strip_comments(read_source(path))
	name = name or Path(path).stem
	head = re.search(r"((?:@\w+(?:\s*\([^)]*\))?\s*)*)(?:public\s+|protected\s+|private\s+|abstract\s+|final\s+|static\s+)*(class|enum)\s+" + name +
	                 r"\b([^{]*)\{", text)
	if not head:
		raise OracleError(f"{path}: no class or enum {name}")
	annotations = tuple(re.findall(r"@(\w+)", head.group(1)))
	accessor = re.search(r"@XmlAccessorType\s*\(\s*XmlAccessType\.(\w+)\s*\)", head.group(1))
	extends = re.search(r"\bextends\s+([\w.]+)", head.group(3))
	body_start = head.end() - 1
	body_end = _matching(text, body_start, "{", "}")
	fields = _top_level_fields(text, body_start + 1, body_end)
	return JavaClass(name, head.group(2), extends.group(1) if extends else None, annotations, accessor.group(1) if accessor else None,
	                 tuple(fields), Path(path), text)


def _top_level_fields(text: str, start: int, end: int) -> list[JavaField]:
	"""The field declarations directly in a class body: statements ending in ';' at brace depth 0 whose head is not a method."""
	fields = []
	statement_start = start
	i = start
	paren = 0
	while i < end:
		c = text[i]
		if c in "\"'":
			j = i + 1
			while j < end and text[j] != c:
				j += 2 if text[j] == "\\" else 1
			i = j + 1
			continue
		if c == "(":
			paren += 1
		elif c == ")":
			paren -= 1
		elif c == "{" and paren == 0:
			before = text[statement_start:i]
			close = _matching(text, i, "{", "}")
			if re.search(r"=\s*(?:new\s+[\w.<>\[\]]+\s*)?$", before):  # an array initializer: still the same field statement
				i = close + 1
				continue
			i = close + 1  # a method, constructor, initializer or nested type: skipped with its annotations
			statement_start = i
			continue
		elif c == ";" and paren == 0:
			field_ = _parse_field(text, statement_start, i)
			if field_ is not None:
				fields.append(field_)
			statement_start = i + 1
		i += 1
	return fields


def _parse_field(text: str, start: int, end: int) -> JavaField | None:
	statement = text[start:end]
	offset = len(statement) - len(statement.lstrip())
	rest = statement.strip()
	annotations = []
	while rest.startswith("@"):
		match = _ANNOTATION.match(rest)
		name = match.group(1)
		rest = rest[match.end():]
		args = None
		if rest.startswith("("):
			close = _matching(rest, 0, "(", ")")
			args = rest[1:close]
			rest = rest[close + 1:].lstrip()
		annotations.append((name, args))
	head, eq, initializer = rest.partition("=")
	if "(" in head:  # an abstract or interface method declaration, not a field
		return None
	match = re.fullmatch(r"((?:\w+\s+)*?)([\w.]+(?:\s*<.*>)?(?:\s*\[\s*\])*)\s+(\w+)\s*", head, re.DOTALL)
	if not match:
		return None
	modifiers = frozenset(match.group(1).split())
	if not modifiers <= _MODIFIERS:
		return None
	return JavaField(match.group(3), re.sub(r"\s+", "", match.group(2)), modifiers, tuple(annotations), initializer.strip() if eq else None,
	                 line_of(text, start + offset))


def java_enum_constants(path: Path, name: str) -> list[tuple[str, str | None]]:
	"""(name, argument text) of every constant of `enum name`, in declaration order: the constant list ends at the first ';' at depth 0 or,
	for an enum without a body (QuestTarget), at its closing brace. A constant with a class body of its own is refused."""
	text = strip_comments(read_source(path))
	head = re.search(r"\benum\s+" + name + r"\b[^{]*\{", text)
	if not head:
		raise OracleError(f"{path}: enum {name} not found")
	start = head.end()
	end = _matching(text, start - 1, "{", "}")
	constants = []
	current = ""
	depth = 0
	for ch in text[start:end] + ";":
		if ch == "(":
			depth += 1
		elif ch == ")":
			depth -= 1
		elif ch == "{" and depth == 0:
			raise OracleError(f"{path}: enum {name} has a constant with a class body, which this reader does not model")
		if depth == 0 and ch in ",;":
			item = current.strip()
			if item:
				match = re.fullmatch(r"(?:@\w+\s*)*([A-Za-z_]\w*)\s*(?:\((.*)\))?", item, re.DOTALL)
				if not match:
					raise OracleError(f"{path}: cannot parse enum constant {item!r}")
				constants.append((match.group(1), match.group(2)))
			current = ""
			if ch == ";":
				break
		else:
			current += ch
	return constants


def _annotation_arg(args: str | None, key: str) -> str | None:
	if not args:
		return None
	match = re.search(r"\b" + key + r"\s*=\s*(\"[^\"]*\"|[\w.]+)", args)
	if not match:
		return None
	value = match.group(1)
	return value[1:-1] if value.startswith('"') else value


@dataclass(frozen=True)
class Binding:
	node: str  # attribute | element
	xml_name: str
	field: str
	java_type: str
	required: bool
	xml_list: bool
	initializer: str | None
	declared_in: str
	line: int


def jaxb_bindings(cls: JavaClass) -> list[Binding]:
	"""The JAXB properties a class declares itself (not its superclass), refusing the annotations this reader does not model."""
	if cls.accessor_type != "FIELD":
		raise OracleError(f"{cls.path.name}: @XmlAccessorType is {cls.accessor_type}, this oracle models XmlAccessType.FIELD only")
	bindings = []
	for f in cls.fields:
		if "static" in f.modifiers or "transient" in f.modifiers or f.has("XmlTransient"):
			continue
		for unsupported in ("XmlValue", "XmlJavaTypeAdapter", "XmlElementWrapper", "XmlElementRef", "XmlAnyElement", "XmlID", "XmlIDREF",
		                    "XmlElements", "XmlMixed", "XmlAnyAttribute"):
			if f.has(unsupported):
				raise OracleError(f"{cls.path.name}:{f.line}: @{unsupported} on {f.name} is not modelled")
		attribute = f.annotation("XmlAttribute")
		element = f.annotation("XmlElement")
		if attribute is not None and element is not None:
			raise OracleError(f"{cls.path.name}:{f.line}: {f.name} is both an attribute and an element")
		if attribute is not None:
			node, args = "attribute", attribute
		else:
			node, args = "element", element  # an unannotated field is an element named after the field (XmlAccessType.FIELD)
		xml_name = _annotation_arg(args, "name") or f.name
		required = _annotation_arg(args, "required") == "true"
		element_type = _annotation_arg(args, "type")
		java_type = f.type
		if element_type:
			simple = element_type.removesuffix(".class")
			java_type = f"List<{simple}>" if java_type.startswith("List<") else simple
		bindings.append(Binding(node, xml_name, f.name, java_type, required, f.has("XmlList"), f.initializer, cls.name, f.line))
	return bindings


@dataclass
class JObj:
	"""The fields JAXB fills from one element: `values` by Java field name, `present` the fields whose XML node was there, `ignored` the
	attributes and child elements JAXB drops because no field binds them (`@name` / `<tag>`)."""

	cls: str
	values: dict[str, object]
	present: set[str]
	ignored: list[str]
	element: ET.Element

	def __getitem__(self, name: str):
		return self.values[name]

	def get(self, name: str, default=None):
		return self.values.get(name, default)


SCALAR_TYPES = {"int", "Integer", "long", "Long", "short", "Short", "byte", "Byte", "boolean", "Boolean", "String"}
PRIMITIVE_ZERO = {"int": 0, "long": 0, "short": 0, "byte": 0, "boolean": False}
INT_RANGES = {"int": 32, "Integer": 32, "long": 64, "Long": 64, "short": 16, "Short": 16, "byte": 8, "Byte": 8}
RAW_PACKAGE = "xmlQuest"  # questEngine/handlers/models/xmlQuest: the xmlQuest language, kept as raw elements (not modelled)


_PLAIN_DECIMAL = re.compile(r"[ \t\r\n]*[+-]?[0-9]+[ \t\r\n]*")


def java_integer(text: str, bits: int, what: str) -> int:
	"""A plain decimal integer (optional sign, ASCII digits, XML white space around it). Anything else is refused: JAXB's
	DatatypeConverterImpl._parseInt reads other shapes its own way (it skips white space inside, takes a sign anywhere, wraps on overflow),
	and Python's int() accepts forms Java does not ('1_0', non-ASCII digits)."""
	if not _PLAIN_DECIMAL.fullmatch(text):
		raise OracleError(f"{what}={text!r} is not a plain decimal integer (not modelled)")
	number = int(text.strip(), 10)
	low, high = -(1 << (bits - 1)), (1 << (bits - 1)) - 1
	if not low <= number <= high:
		raise OracleError(f"{what}={text!r} is out of the {bits}-bit range")
	return number


class JaxbModel:
	"""The quest template classes of one Java source tree, loaded on demand by simple name."""

	ENUM_SOURCES = {
		"Race": ("model", "Race.java"),
		"Gender": ("model", "Gender.java"),
		"PlayerClass": ("model", "PlayerClass.java"),
		"QuestCategory": ("model", "templates", "quest", "QuestCategory.java"),
		"QuestExtraCategory": ("model", "templates", "quest", "QuestExtraCategory.java"),
		"QuestMentorType": ("model", "templates", "quest", "QuestMentorType.java"),
		"QuestTarget": ("model", "templates", "quest", "QuestTarget.java"),
		"QuestRepeatCycle": ("model", "templates", "quest", "QuestRepeatCycle.java"),
		"BonusType": ("model", "templates", "rewards", "BonusType.java"),
		"QuestStatus": ("questEngine", "model", "QuestStatus.java"),
		"ConditionOperation": ("questEngine", "model", "ConditionOperation.java"),
		"ConditionUnionType": ("questEngine", "model", "ConditionUnionType.java"),
	}

	def __init__(self, java_src: Path):
		self.base = Path(java_src) / GS
		if not self.base.is_dir():
			raise OracleError(f"{self.base} does not exist: pass --java-src (game-server/src)")
		self.search_dirs = [self.base / "model" / "templates", self.base / "model" / "templates" / "quest",
		                    self.base / "questEngine" / "handlers" / "models"]
		self._classes: dict[str, JavaClass] = {}
		self._bindings: dict[str, dict[tuple[str, str], Binding]] = {}
		self._paths: dict[str, Path] = {}
		self._raw: dict[str, bool] = {}
		self.enums: dict[str, list[tuple[str, str | None]]] = {}
		for name, parts in self.ENUM_SOURCES.items():
			path = self.base.joinpath(*parts)
			source = strip_comments(read_source(path))
			if "@XmlEnumValue" in source:
				raise OracleError(f"{path.name}: @XmlEnumValue is not modelled (this oracle maps enum constants by name)")
			self.enums[name] = java_enum_constants(path, name)
		self.enum_names = {name: [c for c, _ in constants] for name, constants in self.enums.items()}
		self.quest_kinds = self._xml_quest_kinds()

	def _xml_quest_kinds(self) -> dict[str, str]:
		"""XMLQuests.java @XmlElements: quest_script_data tag -> data class, in declaration order."""
		path = self.base / "dataholders" / "XMLQuests.java"
		text = strip_comments(read_source(path))
		block = re.search(r"@XmlElements\s*\(\s*\{(.*?)\}\s*\)\s*private\s+List<XMLQuest>\s+data\s*;", text, re.DOTALL)
		if not block:
			raise OracleError(f"{path}: no `@XmlElements({{...}}) private List<XMLQuest> data;`")
		kinds: dict[str, str] = {}
		for tag, cls in re.findall(r'@XmlElement\s*\(\s*name\s*=\s*"([^"]+)"\s*,\s*type\s*=\s*(\w+)\.class\s*\)', block.group(1)):
			if tag in kinds:
				raise OracleError(f"XMLQuests.java binds <{tag}> twice")
			kinds[tag] = cls
		if not re.search(r"data\.forEach\(\s*quest\s*->\s*questsById\.put\(\s*quest\.getId\(\)\s*,\s*quest\s*\)\s*\)", text):
			raise OracleError(f"{path}: afterUnmarshal is not `data.forEach(quest -> questsById.put(quest.getId(), quest))` (later id wins)")
		return kinds

	def find(self, name: str) -> Path:
		if name in self._paths:
			return self._paths[name]
		for directory in self.search_dirs:
			path = directory / f"{name}.java"
			if path.is_file():
				self._paths[name] = path
				return path
		for path in (self.base / "questEngine" / "handlers" / "models" / RAW_PACKAGE).rglob(f"{name}.java"):
			self._paths[name] = path
			return path
		raise OracleError(f"no Java source for class {name} under {self.search_dirs}")

	def java_class(self, name: str) -> JavaClass:
		if name not in self._classes:
			self._classes[name] = parse_class(self.find(name), name)
		return self._classes[name]

	def is_raw(self, name: str) -> bool:
		if name not in self._raw:
			self._raw[name] = RAW_PACKAGE in self.find(name).parts
		return self._raw[name]

	def bindings(self, name: str) -> dict[tuple[str, str], Binding]:
		"""(node, xml name) -> binding of the class and its superclasses (up to the first class outside the quest model packages)."""
		if name in self._bindings:
			return self._bindings[name]
		cls = self.java_class(name)
		result: dict[tuple[str, str], Binding] = {}
		if cls.extends and cls.extends != "Object":
			try:
				self.find(cls.extends)
			except OracleError:
				raise OracleError(f"{cls.path.name}: superclass {cls.extends} is not a quest model class this oracle can read") from None
			result.update(self.bindings(cls.extends))
		for b in jaxb_bindings(cls):
			key = (b.node, b.xml_name)
			if key in result:
				raise OracleError(f"{cls.path.name}:{b.line}: {b.node} {b.xml_name} is bound twice in the class chain")
			result[key] = b
		self._bindings[name] = result
		return result

	def field_names(self, name: str) -> set[str]:
		return {b.field for b in self.bindings(name).values()}

	# --- unmarshalling -----------------------------------------------------------------------------------------------------------------

	def _enum_value(self, enum: str, text: str, what: str) -> str:
		value = text.strip()
		if value not in self.enum_names[enum]:
			raise OracleError(f"{what}={text!r} is not a {enum} constant")
		return value

	def _scalar(self, java_type: str, text: str, what: str):
		if java_type in INT_RANGES:
			return java_integer(text, INT_RANGES[java_type], what)
		if java_type in ("boolean", "Boolean"):
			return java_boolean(text)
		if java_type == "String":
			return text
		if java_type in self.enums:
			return self._enum_value(java_type, text, what)
		raise OracleError(f"{what}: Java type {java_type} is not modelled")

	def _default(self, b: Binding, what: str):
		init = b.initializer
		if init is None:
			return PRIMITIVE_ZERO.get(b.java_type)
		if init == "null":
			return None
		if b.java_type in INT_RANGES:
			return java_integer(init.rstrip("Ll"), INT_RANGES[b.java_type], f"{what} initializer")
		if b.java_type in ("boolean", "Boolean") and init in ("true", "false"):
			return init == "true"
		enum = re.fullmatch(r"(\w+)\.(\w+)", init)
		if enum and enum.group(1) == b.java_type and b.java_type in self.enums:
			return self._enum_value(b.java_type, enum.group(2), f"{what} initializer")
		raise OracleError(f"{b.declared_in}.java:{b.line}: initializer {init!r} of {b.field} is not modelled")

	def unmarshal(self, element: ET.Element, cls_name: str, what: str) -> JObj:
		bindings = self.bindings(cls_name)
		values: dict[str, object] = {}
		present: set[str] = set()
		ignored = [f"@{a}" for a in element.attrib if ("attribute", a) not in bindings]
		by_tag: dict[str, list[ET.Element]] = {}
		for c in element:
			if ("element", c.tag) in bindings:
				by_tag.setdefault(c.tag, []).append(c)
			else:
				ignored.append(f"<{c.tag}>")
		for (node, xml_name), b in bindings.items():
			label = f"{what} {'@' if node == 'attribute' else ''}{xml_name}"
			list_of = re.fullmatch(r"List<(\w+)>", b.java_type)
			if list_of and b.initializer not in (None, "null"):
				raise OracleError(f"{b.declared_in}.java:{b.line}: list field {b.field} with initializer {b.initializer!r} is not modelled")
			if node == "attribute":
				text = element.get(xml_name)
				if text is None:
					if b.required:
						raise OracleError(f"{label}: missing required attribute")
					values[b.field] = None if list_of else self._default(b, label)
					continue
				present.add(b.field)
				values[b.field] = [self._scalar(list_of.group(1), t, label) for t in text.split()] if list_of else self._scalar(b.java_type, text, label)
				continue
			children = by_tag.get(xml_name)
			if not children:
				if b.required:
					raise OracleError(f"{label}: missing required element")
				values[b.field] = None if list_of else self._default(b, label)
				continue
			present.add(b.field)
			item_type = list_of.group(1) if list_of else b.java_type
			if b.xml_list:
				if len(children) > 1 or not list_of:
					raise OracleError(f"{label}: an @XmlList element given {len(children)} times is not modelled")
				values[b.field] = [self._scalar(item_type, t, label) for t in (children[0].text or "").split()]
				continue
			if not list_of and len(children) > 1:
				raise OracleError(f"{label}: a single-valued element given {len(children)} times (JAXB keeps the last) is not modelled")
			converted = [self._element_value(c, item_type, label) for c in children]
			values[b.field] = converted if list_of else converted[0]
		return JObj(cls_name, values, present, ignored, element)

	def _element_value(self, child: ET.Element, java_type: str, what: str):
		if java_type in SCALAR_TYPES or java_type in self.enums:
			return self._scalar(java_type, child.text or "", what)
		if self.is_raw(java_type):
			return raw_element(child)
		return self.unmarshal(child, java_type, what)


def raw_element(element: ET.Element) -> dict:
	"""An element as plain data (tag, attributes, children) - for the xmlQuest language, which this oracle reports but does not model."""
	result: dict = {"tag": element.tag}
	if element.attrib:
		result["attributes"] = dict(element.attrib)
	if len(element):
		result["children"] = [raw_element(c) for c in element]
	return result


# --- constant tables ---------------------------------------------------------------------------------------------------------------------


@dataclass(frozen=True)
class DialogTables:
	actions: dict[str, int]             # DialogAction: name -> id
	pages: dict[str, int]               # DialogPage: name -> id()
	reward_pages: dict[int, str]        # DialogPage.getRewardPageByIndex: index -> constant
	quest_status: dict[str, int]        # QuestStatus: name -> value()

	@staticmethod
	def read(java_src: Path) -> "DialogTables":
		base = Path(java_src) / GS
		text = strip_comments(read_source(base / "model" / "DialogAction.java"))
		actions = {name: int(value) for name, value in re.findall(r"public\s+static\s+final\s+int\s+(\w+)\s*=\s*(-?\d+)\s*;", text)}
		if not actions:
			raise OracleError("DialogAction.java: no `public static final int NAME = N;` constants")
		page_path = base / "model" / "DialogPage.java"
		pages = {}
		for name, args in java_enum_constants(page_path, "DialogPage"):
			last = (args or "").split(",")[-1].strip()
			if not re.fullmatch(r"\d+", last):
				raise OracleError(f"DialogPage.{name}: the id argument {last!r} is not an int literal")
			pages[name] = int(last)
		page_text = strip_comments(read_source(page_path))
		body = re.search(r"getRewardPageByIndex\(Integer rewardIndex\)\s*\{(.*?)return\s+DialogPage\.NULL;\s*\}", page_text, re.DOTALL)
		if not body:
			raise OracleError("DialogPage.getRewardPageByIndex does not have the shape this oracle was written against")
		reward_pages = {int(i): name for i, name in re.findall(r"case\s+(\d+)\s*:\s*return\s+DialogPage\.(\w+)\s*;", body.group(1))}
		status = {}
		for name, args in java_enum_constants(base / "questEngine" / "model" / "QuestStatus.java", "QuestStatus"):
			status[name] = java_int((args or "").strip(), f"QuestStatus.{name}")
		return DialogTables(actions, pages, reward_pages, status)

	def action(self, name: str) -> dict:
		if name not in self.actions:
			raise OracleError(f"DialogAction.{name} does not exist")
		return {"name": name, "id": self.actions[name]}

	def reward_page(self, group: int | None) -> int:
		"""DialogPage.getRewardPageByIndex(group).id(): NULL (0) for a null or unmapped group."""
		if group is None or group not in self.reward_pages:
			return self.pages["NULL"]
		return self.pages[self.reward_pages[group]]


# --- configuration -----------------------------------------------------------------------------------------------------------------------


@dataclass(frozen=True)
class ConfigValue:
	key: str
	field: str
	java_type: str
	class_default: str
	properties_value: str | None
	properties_file: str | None
	profile_value: str | None  # the profile (config/mygs.properties by default), which Config.loadProperties reads over the defaults folders
	profile_file: str | None
	event_overrides: tuple[str, ...]

	@property
	def effective(self) -> str:
		if self.profile_value is not None:
			return self.profile_value
		return self.properties_value if self.properties_value is not None else self.class_default

	def as_json(self) -> dict:
		return {"key": self.key, "field": self.field, "classDefault": self.class_default, "propertiesFile": self.properties_file,
		        "propertiesValue": self.properties_value, "profile": self.profile_file, "profileValue": self.profile_value,
		        "used": self.effective, "eventOverrides": list(self.event_overrides)}


def _properties(path: Path) -> dict[str, tuple[str, bool]]:
	"""java.util.Properties.load for the plain forms of the config files: key -> (value, continued or escaped)."""
	result: dict[str, tuple[str, bool]] = {}
	for raw in read_source(path).splitlines():
		line = raw.strip()
		if not line or line[0] in "#!":
			continue
		match = re.match(r"([^=:\s]+)\s*[=:\s]\s*(.*)$", line)
		if not match:
			continue
		value = match.group(2)
		result[match.group(1)] = (value.rstrip("\\").strip(), "\\" in value)
	return result


def read_config(java_src: Path, config_dir: Path, events_dir: Path | None, class_path: tuple[str, ...], field_name: str,
                profile: Path | None = None) -> ConfigValue:
	"""The value a Config field gets (Config.loadProperties): the key's value in the profile, else in the defaults folders, else the
	@Property defaultValue. `profile` None reads no profile; a profile path that is not a file is refused (the caller decides whether the
	default config/mygs.properties is there)."""
	source_path = Path(java_src) / GS / "configs" / Path(*class_path)
	text = strip_comments(read_source(source_path))
	match = re.search(r"@Property\s*\(\s*key\s*=\s*\"([^\"]+)\"\s*,\s*defaultValue\s*=\s*\"([^\"]*)\"\s*\)\s*public\s+static\s+([\w\[\]]+)\s+" +
	                  field_name + r"\s*;", text)
	if not match:
		raise OracleError(f"{source_path.name}: no `@Property(key, defaultValue) public static T {field_name};`")
	key, default, java_type = match.groups()
	found = []
	for folder in ("administration", "main", "network"):
		directory = Path(config_dir) / folder
		for path in sorted(directory.glob("*.properties")) if directory.is_dir() else []:
			props = _properties(path)
			if key in props:
				found.append((f"{folder}/{path.name}", props[key]))
	if len(found) > 1:
		raise OracleError(f"{key} is set in {[f for f, _ in found]}: which one wins depends on the directory listing order")
	if found and found[0][1][1]:
		raise OracleError(f"{key} in {found[0][0]} continues on the next line or has an escape, which this reader does not model")
	profile_value = profile_file = None
	if profile is not None:
		profile = Path(profile)
		if not profile.is_file():
			raise OracleError(f"profile {profile}: no such file")
		props = _properties(profile)
		if key in props:
			if props[key][1]:
				raise OracleError(f"{key} in {profile.name} continues on the next line or has an escape, which this reader does not model")
			profile_value, profile_file = props[key][0], profile.name
	overrides = []
	if events_dir is not None and Path(events_dir).is_dir():
		for path in sorted(Path(events_dir).glob("*.xml")):
			try:
				root = ET.parse(path).getroot()
			except ET.ParseError as e:
				raise OracleError(f"{path}: {e}") from e
			for event in root.iter("event"):
				for prop in event.iter("property"):
					prop_key = (prop.text or "").split("=", 1)[0].strip()
					if prop_key == key:
						overrides.append(f"{path.name}: event {event.get('name')!r} ({event.get('start')} .. {event.get('end')}): {prop.text.strip()}")
	return ConfigValue(key, field_name, java_type, default, found[0][1][0] if found else None, found[0][0] if found else None, profile_value,
	                   profile_file, tuple(overrides))


def float_array(value: ConfigValue) -> list[str]:
	"""CommaSeparatedValueTransformer.splitAndTrimValues for a float[] without quotes."""
	text = value.effective
	if '"' in text:
		raise OracleError(f"{value.key}: quoted values are not modelled")
	parts = [p.strip() for p in text.split(",")]
	if parts and parts[-1] == "":
		parts.pop()
	if not parts:
		raise OracleError(f"{value.key} is empty (Rates.get would warn 'Missing rates' and use 1)")
	return parts


# --- Java quest handlers -----------------------------------------------------------------------------------------------------------------


@dataclass
class JavaQuestHandler:
	quest_id: int
	class_name: str
	file: str
	start_npcs: list[int] = field(default_factory=list)
	unresolved: list[str] = field(default_factory=list)  # start npc expressions the text scan could not resolve


def _int_fields(text: str) -> tuple[dict[str, int], dict[str, list[int]]]:
	"""The int and int[] fields of a handler (declarations with a modifier, so that method locals are not taken for fields)."""
	modifiers = r"(?:private|protected|public|static|final)\s+(?:(?:private|protected|public|static|final)\s+)*"
	ints = {n: int(v) for n, v in re.findall(modifiers + r"int\s+(\w+)\s*=\s*(-?\d+)\s*;", text)}
	arrays = {}
	for name, body in re.findall(modifiers + r"int\s*\[\s*\]\s*(\w+)\s*=\s*(?:new\s+int\s*\[\s*\]\s*)?\{([^}]*)\}", text):
		arrays[name] = [int(v) for v in re.findall(r"-?\d+", body)]
	return ints, arrays


def scan_java_handlers(handlers_dir: Path) -> tuple[dict[int, JavaQuestHandler], list[str]]:
	"""Every public quest handler class under data/handlers/quest (QuestHandlerLoader.isValidClass: public, not abstract), keyed by the quest
	id its constructor passes to super(); duplicates (QuestEngine.addQuestHandler keeps the first loaded, in an order this oracle cannot
	know) are returned as the second value."""
	handlers: dict[int, JavaQuestHandler] = {}
	duplicates: list[str] = []
	root = Path(handlers_dir)
	if not root.is_dir():
		raise OracleError(f"{root} does not exist: pass --java-handlers (game-server/data/handlers/quest)")
	for path in sorted(root.rglob("*.java")):
		text = strip_comments(read_source(path))
		cls = re.search(r"public\s+class\s+(\w+)\s+extends\s+(\w+)", text)
		if not cls or re.search(r"\babstract\s+class\s+" + cls.group(1) + r"\b", text):
			continue
		ints, arrays = _int_fields(text)
		sup = re.search(r"\bsuper\s*\(\s*(\w+)\s*\)", text)
		if not sup:
			raise OracleError(f"{path}: no super(questId) call")
		arg = sup.group(1)
		quest_id = int(arg) if arg.isdigit() else ints.get(arg)
		if quest_id is None:
			raise OracleError(f"{path}: super({arg}) is not an int literal or an int field of the class")
		handler = JavaQuestHandler(quest_id, cls.group(1), path.relative_to(root).as_posix())
		loops = dict(re.findall(r"for\s*\(\s*(?:final\s+)?int\s+(\w+)\s*:\s*(\w+)\s*\)", text))
		for expr in re.findall(r"registerQuestNpc\(\s*([^()]*?)\s*\)\s*\.\s*addOnQuestStart\s*\(", text):
			element = re.fullmatch(r"(\w+)\s*\[\s*(\d+)\s*\]", expr)
			if re.fullmatch(r"\d+", expr):
				npcs = [int(expr)]
			elif element and element.group(1) in arrays and int(element.group(2)) < len(arrays[element.group(1)]):
				npcs = [arrays[element.group(1)][int(element.group(2))]]
			elif expr in ints:
				npcs = [ints[expr]]
			elif expr in loops and loops[expr] in arrays:
				npcs = arrays[loops[expr]]
			else:
				handler.unresolved.append(expr)
				continue
			for npc in npcs:
				if npc not in handler.start_npcs:
					handler.start_npcs.append(npc)
		if quest_id in handlers:
			duplicates.append(f"quest {quest_id}: {handlers[quest_id].file} and {handler.file}")
			continue
		handlers[quest_id] = handler
	return handlers, duplicates


def check_registration_order(java_src: Path) -> None:
	"""QuestEngine.init loads the Java handlers (scriptManager.load) before it registers the XML quests, and addQuestHandler keeps the first
	handler of a quest id (putIfAbsent): a Java handler wins over an XML template (QuestEngine.java:86-110, 892-898)."""
	path = Path(java_src) / GS / "questEngine" / "QuestEngine.java"
	text = strip_comments(read_source(path))
	load = text.find("scriptManager.load(GSConfig.QUEST_HANDLER_DIRECTORY)")
	xml = re.search(r"for\s*\(\s*XMLQuest\s+xmlQuest\s*:\s*DataManager\.XML_QUESTS\.getAllQuests\(\)\s*\)\s*xmlQuest\.register\(this\)", text)
	put = re.search(r"questHandlers\.putIfAbsent\(\s*questId\s*,\s*questHandler\s*\)\s*!=\s*null", text)
	if load < 0 or not xml or load > xml.start() or not put:
		raise OracleError(f"{path}: init/addQuestHandler do not have the shape this oracle was written against (Java handlers first, "
		                  "then the XML quests, putIfAbsent)")


# --- java.util.HashMap iteration order ---------------------------------------------------------------------------------------------------

TREEIFY_THRESHOLD = 8
MIN_TREEIFY_CAPACITY = 64


def java_int_hash(key: int) -> int:
	"""HashMap.hash(Integer): h ^ (h >>> 16) of Integer.hashCode() (the value), as an unsigned 32-bit pattern."""
	h = key & 0xFFFFFFFF
	return h ^ (h >> 16)


def _table_size_for(capacity: int) -> int:
	if capacity <= 1:
		return 1
	return 1 << (capacity - 1).bit_length()


def hash_iteration_order(keys: list[int], initial_capacity: int | None = None) -> list[int]:
	"""The iteration order of a java.util.HashMap (or HashSet) after `keys` were put in this order (a key put again keeps its place):
	buckets ascending, each bucket in insertion order. new HashMap<>() is initial_capacity None (16 at the first put); new HashSet<>(0) is 0.
	A bucket that would become a tree bin (TREEIFY_THRESHOLD entries in a table of at least MIN_TREEIFY_CAPACITY) iterates in tree-insertion
	order, which this oracle does not model: OracleError."""
	capacity = 0
	threshold = 16 if initial_capacity is None else _table_size_for(initial_capacity)
	default = initial_capacity is None
	table: list[list[int]] = []
	seen: set[int] = set()

	def resize():
		nonlocal capacity, threshold, table, default
		old_cap, old_thr = capacity, threshold
		if old_cap > 0:
			new_cap = old_cap << 1
			new_thr = old_thr << 1 if old_cap >= 16 else 0
		elif not default and old_thr > 0:
			new_cap, new_thr = old_thr, 0
		else:
			new_cap, new_thr = 16, 12
		if new_thr == 0:
			new_thr = int(new_cap * 0.75)
		default = False
		new_table: list[list[int]] = [[] for _ in range(new_cap)]
		for bucket in table:
			for key in bucket:
				new_table[java_int_hash(key) & (new_cap - 1)].append(key)
		capacity, threshold, table = new_cap, new_thr, new_table

	for key in keys:
		if key in seen:
			continue
		if capacity == 0:
			resize()
		bucket = table[java_int_hash(key) & (capacity - 1)]
		bucket.append(key)
		seen.add(key)
		if len(bucket) - 1 >= TREEIFY_THRESHOLD:
			if capacity < MIN_TREEIFY_CAPACITY:
				resize()
			else:
				raise OracleError(f"HashMap bucket of key {key} becomes a tree bin (tree iteration order is not modelled)")
		if len(seen) > threshold:
			resize()
	return [key for bucket in table for key in bucket]


def hash_bucket_groups(keys) -> list[list[int]] | None:
	"""A new HashMap<>() holding `keys`, when their insertion order is unknown: the buckets of the final table in iteration order, each with
	its keys sorted (their order inside a bucket is the insertion order, which the caller does not know). The table is 16 buckets and doubles
	while the size exceeds 3/4 of it (putVal's ++size > threshold). None when a bucket of 16, 32 or 64 could reach TREEIFY_THRESHOLD + 1 keys:
	treeifyBin's resize would then depend on the insertion order."""
	unique = sorted(set(keys))
	capacity, threshold = 16, 12
	while len(unique) > threshold:
		capacity, threshold = capacity << 1, threshold << 1
	size = 16
	while size <= max(capacity, MIN_TREEIFY_CAPACITY):
		counts: dict[int, int] = {}
		for key in unique:
			index = java_int_hash(key) & (size - 1)
			counts[index] = counts.get(index, 0) + 1
		if counts and max(counts.values()) > TREEIFY_THRESHOLD:
			return None
		size <<= 1
	buckets: dict[int, list[int]] = {}
	for key in unique:
		buckets.setdefault(java_int_hash(key) & (capacity - 1), []).append(key)
	return [buckets[index] for index in sorted(buckets)]
