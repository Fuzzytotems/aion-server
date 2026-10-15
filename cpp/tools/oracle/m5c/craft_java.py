"""The Java rules of crafting and gathering, read from the Java sources for the m5c-craft oracle (m5c/craft.py).

Every method the oracle models is compared WHOLE with the body this module was written against: comments and layout are ignored, every other
token must be the same, and a `$name` in a template is a numeric literal that is read from the source instead of assumed. A body that differs
in anything else - a new branch, a changed condition, one more statement - raises OracleError with the file, the line and the first difference,
so a change in the Java tree is a refusal and never a silently wrong expectation (the lesson of the M5b-2 review: a guard that skipped the
bodies it could not parse). Tables (CraftService.getBonusReqItem, Profession.getUpgradeCost, the professionByNpc map, StatEnum.getModifier) are
parsed and then rebuilt: the body must be exactly the rebuilt text. Nothing here reads the C++ tree.

The classes the tasks are made of are pinned WHOLE as well (CLASS_PINS: CraftService, CM_CRAFT, CraftingTask, GatheringTask, AbstractCraftTask,
AbstractInteractionTask, GatherableController and the recipe and gatherable templates): the file's text before the class body (package, imports,
annotations, declaration) and the ordered list of its members - fields, constructors, methods, nested types, initializer blocks. Each member is
either checked by a template (MODELLED) or pinned by the SHA-256 of its canonical text, so an added override, an initializer block, a changed
field initializer or a changed method the oracle does not model is a refusal too (the M5b-2/M5c review: a method-by-method check let a new
`start()` override or a changed `cancelGathering` through). JavaFile.describe_pins prints a class's current pins for a maintainer who has
re-read a changed class.
"""

from __future__ import annotations

import functools
import hashlib
import re
from dataclasses import dataclass, field
from pathlib import Path

from staticdata_oracle import OracleError

from m5a.creation import enum_constants
from m5a.javafloat import f32

BASE = ("com", "aionemu", "gameserver")
# A placeholder is typed: `$name` (or `$i:name`) is an int literal, `$f:name` a float literal (f suffix), `$d:name` a plain double literal. The
# type is part of the template, so a Java literal that changes type (0.008 -> 0.008f, 5 -> 5.5f) is a refusal, not a silently different
# arithmetic.
LITERALS = {"i": r"[0-9]+", "f": r"(?:[0-9]+(?:\.[0-9]+)?|\.[0-9]+)[fF]", "d": r"[0-9]+\.[0-9]+"}
_PLACEHOLDER = re.compile(r"\$(?:([idf]):)?([A-Za-z][A-Za-z0-9]*)")
_TOKEN = re.compile(r"ZQ([idf])([A-Za-z][A-Za-z0-9]*)QZ")


def _is_word(ch: str) -> bool:
	return ch.isalnum() or ch in "_$"


@functools.lru_cache(maxsize=None)
def normalize(text: str) -> tuple[str, list[int]]:
	"""
	Java source -> (canonical text, the raw index of every canonical char): comments dropped, string and char literals kept verbatim, and a run of
	white space (or a comment) kept as one space only where it separates two word characters. Two sources with the same tokens have the same
	canonical text. Cached by the source text (a reread of an unchanged file costs nothing); callers must not change the returned list.
	"""
	out: list[str] = []
	index: list[int] = []
	i, n = 0, len(text)
	gap: int | None = None
	while i < n:
		c = text[i]
		if c.isspace():
			gap = i if gap is None else gap
			i += 1
			continue
		if text.startswith("//", i):
			gap = i if gap is None else gap
			end = text.find("\n", i)
			i = n if end < 0 else end
			continue
		if text.startswith("/*", i):
			gap = i if gap is None else gap
			end = text.find("*/", i + 2)
			if end < 0:
				raise OracleError("unterminated /* comment in a Java source")
			i = end + 2
			continue
		if gap is not None:
			if out and _is_word(out[-1]) and _is_word(c):
				out.append(" ")
				index.append(gap)
			gap = None
		if c in "\"'":
			j = i + 1
			while j < n and text[j] != c:
				if text[j] == "\n":
					raise OracleError("unterminated Java literal")
				j += 2 if text[j] == "\\" else 1
			j += 1
			out.extend(text[i:j])
			index.extend(range(i, j))
			i = j
			continue
		out.append(c)
		index.append(i)
		i += 1
	return "".join(out), index


def java_number(literal: str, kind: str):
	"""A Java literal matched by LITERALS[kind]: an int, a float (rounded to binary32) or a double."""
	if kind == "i":
		return int(literal)
	if kind == "f":
		return f32(float(literal[:-1]))
	return float(literal)


@functools.lru_cache(maxsize=None)
def _compile(template: str) -> tuple[re.Pattern, tuple[tuple[str, str, str], ...], dict[str, str]]:
	"""A template -> (regex over the canonical text, the pieces (kind, text, regex) for the difference report, placeholder -> literal type)."""
	norm, _ = normalize(_PLACEHOLDER.sub(lambda m: f"ZQ{m.group(1) or 'i'}{m.group(2)}QZ", template))
	parts = _TOKEN.split(norm)
	pieces: list[tuple[str, str, str]] = []
	kinds: dict[str, str] = {}
	for k in range(0, len(parts), 3):
		if parts[k]:
			pieces.append(("literal", parts[k], re.escape(parts[k])))
		if k + 2 >= len(parts):
			break
		kind, name = parts[k + 1], parts[k + 2]
		if name in kinds:
			if kinds[name] != kind:
				raise OracleError(f"template placeholder ${name} is used with two literal types")
			pieces.append(("same", name, f"(?P={name})"))
		else:
			kinds[name] = kind
			pieces.append(("number", name, f"(?P<{name}>{LITERALS[kind]})"))
	return re.compile("".join(p[2] for p in pieces), re.DOTALL), tuple(pieces), kinds


def _first_difference(pieces, body: str) -> tuple[int, str]:
	"""(offset in the body, description) of the first place the body leaves the template."""
	pattern, pos = "", 0
	for kind, text, rx in pieces:
		match = re.compile(pattern + rx, re.DOTALL).match(body)
		if match is None:
			if kind == "literal":
				k = 0
				while k < len(text) and pos + k < len(body) and body[pos + k] == text[k]:
					k += 1
				return pos + k, f"the source has {body[pos + k:pos + k + 60]!r} where the oracle expects {text[k:k + 60]!r}"
			if kind == "same":
				return pos, f"the source has {body[pos:pos + 30]!r} where the oracle expects the same literal as ${text}"
			return pos, f"the source has {body[pos:pos + 30]!r} where the oracle expects a number (${text})"
		pattern += rx
		pos = match.end()
	return pos, f"the source continues with {body[pos:pos + 60]!r} after the modelled body ends"


def _skip_literal(text: str, i: int) -> int:
	"""The index after the string or char literal that starts at text[i]."""
	quote, j = text[i], i + 1
	while j < len(text) and text[j] != quote:
		j += 2 if text[j] == "\\" else 1
	return j + 1


def digest(text: str) -> str:
	"""The pin of a class member or header: the first 16 hex digits of the SHA-256 of its canonical text."""
	return hashlib.sha256(text.encode("utf-8")).hexdigest()[:16]


MODELLED = "modelled"  # a CLASS_PINS member checked by a template (method, table or snippet) instead of a digest


@dataclass(frozen=True)
class Member:
	"""One member of a class body: `key` is its head (a block member) or its declaration up to the initializer (a field)."""

	key: str
	text: str
	start: int
	end: int


def _matching_brace(text: str, open_pos: int) -> int:
	depth, i = 0, open_pos
	while i < len(text):
		c = text[i]
		if c in "\"'":
			i = _skip_literal(text, i)
			continue
		if c == "{":
			depth += 1
		elif c == "}":
			depth -= 1
			if depth == 0:
				return i
		i += 1
	raise OracleError("unbalanced braces in a Java source")


def _word_bounded(text: str, regex: re.Pattern) -> list[re.Match]:
	"""The matches of `regex` in canonical `text` that neither start nor end inside a word."""
	found = [m for m in regex.finditer(text) if m.start() == 0 or not _is_word(text[m.start() - 1])]
	return [m for m in found if m.end() == len(text) or not (_is_word(text[m.end() - 1]) and _is_word(text[m.end()]))]


@functools.lru_cache(maxsize=None)
def _find_block(text: str, norm_head: str) -> tuple[int, int, int] | int:
	"""(start of the head, its `{`, the matching `}`) of the one block `norm_head{` in canonical `text`, or how many there are when not one.
	Cached by text: a reread of an unchanged file does not search it again."""
	found = _word_bounded(text, re.compile(re.escape(norm_head + "{")))
	if len(found) != 1:
		return len(found)
	open_pos = found[0].end() - 1
	return found[0].start(), open_pos, _matching_brace(text, open_pos)


class _ShapeError(Exception):
	def __init__(self, what: str, pos: int):
		super().__init__(what)
		self.what, self.pos = what, pos


@functools.lru_cache(maxsize=None)
def _parse_members(t: str) -> tuple[str, tuple[Member, ...]]:
	"""
	(the canonical text before the class body, the members of the body in order) of the one top-level type in canonical text `t`. A member ends
	at a `;` outside parentheses, or at the `}` of a block that is not a field initializer (a `{` after a top-level `=` is an array initializer
	or an anonymous class, and the member goes on to its `;`). A second top-level type is refused. Cached by text.
	"""
	i, depth, open_pos = 0, 0, None
	while i < len(t):
		c = t[i]
		if c in "\"'":
			i = _skip_literal(t, i)
			continue
		if c == "(":
			depth += 1
		elif c == ")":
			depth -= 1
		elif c == "{" and depth == 0:
			open_pos = i
			break
		i += 1
	if open_pos is None:
		raise _ShapeError("no type body", 0)
	close = _matching_brace(t, open_pos)
	if t[close + 1:]:
		raise _ShapeError("more than one top-level type", close + 1)
	members: list[Member] = []
	i = open_pos + 1
	while i < close:
		start, depth, equals = i, 0, None
		while True:
			if i >= close:
				raise _ShapeError("a member without its `;` or body", start)
			c = t[i]
			if c in "\"'":
				i = _skip_literal(t, i)
				continue
			if c == "(":
				depth += 1
			elif c == ")":
				depth -= 1
			elif depth == 0 and c == "=" and equals is None:
				equals = i
			elif depth == 0 and c == ";":
				end, key = i + 1, t[start:i if equals is None else equals]
				break
			elif depth == 0 and c == "{":
				block_end = _matching_brace(t, i)
				if equals is not None:
					i = block_end + 1
					continue
				end, key = block_end + 1, t[start:i]
				break
			i += 1
		members.append(Member(key, t[start:end], start, end))
		i = end
	return t[:open_pos], tuple(members)


@dataclass(frozen=True)
class Verified:
	"""One Java method (or declaration) the oracle checked against its template."""

	method: str
	file: str
	first_line: int
	last_line: int

	def as_json(self) -> dict:
		return {"method": self.method, "fileLine": f"{self.file}:{self.first_line}-{self.last_line}"}


class JavaFile:
	def __init__(self, java_src: Path, relative: str):
		self.relative = relative
		self.path = Path(java_src).joinpath(*BASE, *relative.split("/"))
		try:
			self.raw = self.path.read_text(encoding="utf-8")
		except OSError as e:
			raise OracleError(f"{self.path}: {e}") from e
		self.text, self.index = normalize(self.raw)
		self.cls = relative.rsplit("/", 1)[-1].removesuffix(".java")
		self.spans: list[tuple[int, int]] = []  # (start, end) of every method, table and snippet a template checked, for pin_class

	def line(self, pos: int) -> int:
		return self.raw.count("\n", 0, self.index[min(pos, len(self.index) - 1)]) + 1

	def _unique(self, regex: re.Pattern, what: str) -> re.Match:
		found = _word_bounded(self.text, regex)
		if len(found) != 1:
			raise OracleError(f"{self.relative}: expected exactly one {what}, found {len(found)} (the Java source does not have the shape this oracle "
			                  "was written against)")
		return found[0]

	def _locate(self, head: str) -> tuple[int, int, int]:
		"""(start of the head, its opening brace, the matching closing brace) of the one method, constructor or type with this head."""
		found = _find_block(self.text, normalize(head)[0])
		if isinstance(found, int):
			raise OracleError(f"{self.relative}: expected exactly one `{head}`, found {found} (the Java source does not have the shape this oracle "
			                  "was written against)")
		return found

	def body(self, head: str) -> tuple[str, int, int, int]:
		"""(canonical body between the braces, offset of the body, first line, last line) of the method or constructor with this head."""
		start, open_pos, close = self._locate(head)
		return self.text[open_pos + 1:close], open_pos + 1, self.line(start), self.line(close)

	def method(self, name: str, head: str, template: str, verified: list[Verified]) -> dict:
		"""Checks the whole body of `head` against `template`; returns the `$name` literals, parsed."""
		start, open_pos, close = self._locate(head)
		body, offset = self.text[open_pos + 1:close], open_pos + 1
		regex, pieces, kinds = _compile(template)
		match = regex.fullmatch(body)
		if match is None:
			pos, what = _first_difference(pieces, body)
			raise OracleError(f"{self.relative}:{self.line(offset + pos)} {self.cls}.{name}: {what} - the Java method is not the one this oracle "
			                  "models")
		verified.append(Verified(f"{self.cls}.{name}", self.relative, self.line(start), self.line(close)))
		self.spans.append((start, close + 1))
		return {key: java_number(value, kinds[key]) for key, value in match.groupdict().items()}

	def getter(self, head: str, field_expr: str, verified: list[Verified]) -> None:
		"""A getter whose whole body is `return <field_expr>;`."""
		self.method(re.search(r"(\w+)\(", head).group(1), head, f"return {field_expr};", verified)

	def table(self, name: str, head: str, item: str, render, prefix: str, suffix: str, verified: list[Verified]) -> list[tuple[str, ...]]:
		"""A body that is a table: the `item` regex rows between `prefix` and `suffix`, rebuilt with `render` and compared with the body."""
		start, open_pos, close = self._locate(head)
		body, offset, first, last = self.text[open_pos + 1:close], open_pos + 1, self.line(start), self.line(close)
		self.spans.append((start, close + 1))
		norm_prefix, norm_suffix = normalize(prefix)[0], normalize(suffix)[0]
		rows = re.findall(item, body)
		rebuilt = norm_prefix + "".join(normalize(render(*row))[0] for row in rows) + norm_suffix
		if rebuilt != body:
			k = 0
			while k < min(len(rebuilt), len(body)) and rebuilt[k] == body[k]:
				k += 1
			raise OracleError(f"{self.relative}:{self.line(offset + k)} {self.cls}.{name}: the source has {body[k:k + 60]!r} where the rebuilt table "
			                  f"has {rebuilt[k:k + 60]!r} - the Java method is not the table this oracle models")
		verified.append(Verified(f"{self.cls}.{name}", self.relative, first, last))
		return rows

	def snippet(self, name: str, template: str, verified: list[Verified]) -> dict:
		"""A declaration (a field, an enum constant body) that must occur exactly once; returns its `$name` literals."""
		regex, _, kinds = _compile(template)
		match = self._unique(regex, f"`{template.strip()}`")
		verified.append(Verified(f"{self.cls}.{name}", self.relative, self.line(match.start()), self.line(match.end() - 1)))
		self.spans.append((match.start(), match.end()))
		return {key: java_number(value, kinds[key]) for key, value in match.groupdict().items()}

	def members(self) -> tuple[str, tuple[Member, ...]]:
		"""(the canonical text before the class body, the members of the body in order) of the file's one top-level type (_parse_members)."""
		try:
			return _parse_members(self.text)
		except _ShapeError as e:
			raise OracleError(f"{self.relative}:{self.line(e.pos)} {self.cls}: {e.what}, which the class pin does not model") from None

	def _modelled(self, member: Member) -> bool:
		return any(member.start <= s < member.end and e == member.end for s, e in self.spans)

	def describe_pins(self) -> tuple[str, tuple[tuple[str, str], ...]]:
		"""(header digest, ((member key, MODELLED or digest), ...)) as the class stands: the CLASS_PINS entry of a re-read class."""
		header, members = self.members()
		return digest(header), tuple((m.key, MODELLED if self._modelled(m) else digest(m.text)) for m in members)

	def pin_class(self, pins: tuple[str, tuple[tuple[str, str], ...]], verified: list[Verified]) -> None:
		"""Checks the whole class against its CLASS_PINS entry, after every template of the file has run (see the module docstring)."""
		header_pin, member_pins = pins
		header, members = self.members()
		if digest(header) != header_pin:
			raise OracleError(f"{self.relative}: the package, imports, annotations or declaration of {self.cls} changed (digest {digest(header)}, "
			                  f"pinned {header_pin}) - the Java class is not the one this oracle models")
		keys, expected = [m.key for m in members], [key for key, _ in member_pins]
		if keys != expected:
			k = next(k for k in range(max(len(keys), len(expected))) if k >= len(keys) or k >= len(expected) or keys[k] != expected[k])
			if k >= len(keys):
				what = f"the member `{expected[k]}` is missing"
			elif k >= len(expected):
				what = f"the source has one more member `{keys[k]}` (line {self.line(members[k].start)})"
			else:
				what = f"the source has the member `{keys[k]}` (line {self.line(members[k].start)}) where the oracle expects `{expected[k]}`"
			raise OracleError(f"{self.relative} {self.cls}: {what} - the Java class is not the one this oracle models")
		for member, (key, pin) in zip(members, member_pins):
			if pin == MODELLED:
				if not self._modelled(member):
					raise OracleError(f"{self.relative}:{self.line(member.start)} {self.cls}: `{key}` is pinned as modelled, but no template checked it")
			elif digest(member.text) != pin:
				raise OracleError(f"{self.relative}:{self.line(member.start)} {self.cls}: `{key}` changed (digest {digest(member.text)}, pinned {pin}); "
				                  "the oracle does not model this member and pinned it as it was read - the Java class is not the one this oracle models")
		verified.append(Verified(f"{self.cls} (every member and the header pinned)", self.relative, 1, self.raw.count("\n") + 1))

	def absent(self, what: str, regex: str, verified: list[Verified]) -> None:
		if re.search(regex, self.text):
			raise OracleError(f"{self.relative}: {what}, which this oracle does not model")
		verified.append(Verified(f"{self.cls} (no {what})", self.relative, 1, self.raw.count("\n") + 1))

	def config_property(self, field_name: str, verified: list[Verified]) -> tuple[str, str | None, str]:
		"""(key, defaultValue or None, Java type) of `@Property(key = ..., defaultValue = ...) public static TYPE FIELD;`."""
		regex = re.compile(r'@Property\(key="([^"]+)"(?:,defaultValue="((?:[^"\\]|\\.)*)")?\)public static ([\w.]+(?:<[\w.,<>]*>)?(?:\[\])*) ?'
		                   + re.escape(field_name) + ";")
		match = self._unique(regex, f"@Property field {field_name}")
		verified.append(Verified(f"{self.cls}.{field_name}", self.relative, self.line(match.start()), self.line(match.end() - 1)))
		return match.group(1), match.group(2), match.group(3)


# ------------------------------------------------------------------------------------------------------------------------------------------
# The templates: the Java bodies as they stand, with `$name` for the literals the oracle reads.

FINISH_CRAFTING = """
if (recipetemplate.getMaxProductionCount() != null) {
	player.getRecipeList().deleteRecipe(player, recipetemplate.getId());
	if (critCount == 0) {
		QuestEngine.getInstance().onFailCraft(new QuestEnv(null, player, 0),
			recipetemplate.getComboProduct(1) == null ? 0 : recipetemplate.getComboProduct(1));
	}
}
int skillId = recipetemplate.getSkillId();
int skillLvl = recipetemplate.getSkillpoint();
int xpReward = (int) (($d:xpFactor * (skillLvl + $xpOffset) * (skillLvl + $xpOffset) + $xpBase));
xpReward = xpReward + (xpReward * bonus / $bonusDivisor);
int gainedCraftXp = Rates.SKILL_XP_CRAFTING.calcResult(player, xpReward);
StatEnum boostStat = StatEnum.getModifier(skillId);
if (boostStat != null)
	gainedCraftXp *= player.getGameStats().getStat(boostStat, $boostBase).getCurrent() / $f:boostDivisor;
gainedCraftXp = Math.max($minXp, gainedCraftXp);
if (player.getSkillList().addSkillXp(player, skillId, gainedCraftXp, skillLvl)) {
	player.getCommonData().addExp(xpReward, Rates.XP_CRAFTING);
} else {
	PacketSendUtility.sendPacket(player,
		SM_SYSTEM_MESSAGE.STR_MSG_DONT_GET_PRODUCTION_EXP(DataManager.SKILL_DATA.getSkillTemplate(skillId).getL10n()));
}
int productItemId = critCount > 0 ? recipetemplate.getComboProduct(critCount) : recipetemplate.getProductId();
ItemService.addItem(player, productItemId, recipetemplate.getQuantity(), true,
	new ItemUpdatePredicate(ItemAddType.CRAFTED_ITEM, ItemUpdateType.INC_ITEM_COLLECT) {
		@Override
		public boolean changeItem(Item item) {
			if (item.getItemTemplate().isWeapon() || item.getItemTemplate().isArmor()) {
				item.setItemCreator(player.getName());
				return true;
			}
			return false;
		}
	});
if (LoggingConfig.LOG_CRAFT) {
	ItemTemplate itemTemplate = DataManager.ITEM_DATA.getItemTemplate(productItemId);
	log.info("Player " + player.getName() + " crafted item " + productItemId + " [" + itemTemplate.getName() + "] (count: "
		+ recipetemplate.getQuantity() + ")" + (critCount > 0 ? " - critical" : ""));
}
if (recipetemplate.getCraftDelayId() != null) {
	long reuseTimeMillis = System.currentTimeMillis() + recipetemplate.getCraftDelayTime() * $delayMillis;
	player.getCraftCooldowns().put(recipetemplate.getCraftDelayId(), reuseTimeMillis);
}
"""

START_CRAFTING = """
RecipeTemplate recipeTemplate = DataManager.RECIPE_DATA.getRecipeTemplateById(recipeId);
int skillId = recipeTemplate.getSkillId();
VisibleObject target = player.getKnownList().getObject(targetObjId);
ItemTemplate itemTemplate = DataManager.ITEM_DATA.getItemTemplate(recipeTemplate.getProductId());
if (!checkCraft(player, recipeTemplate, skillId, target, itemTemplate, craftType, sendMaterialsData)) {
	sendCancelCraft(player, skillId, targetObjId, itemTemplate);
	return;
}
if (recipeTemplate.getDp() != null)
	player.getCommonData().addDp(-recipeTemplate.getDp());
int intervalCap = $capDefault;
switch (itemTemplate.getItemQuality()) {
	case UNIQUE:
	case EPIC:
		intervalCap = $capUniqueEpic;
		break;
	case MYTHIC:
		intervalCap = $capMythic;
		break;
}
int skillLvlDiff = player.getSkillList().getSkillLevel(skillId) - recipeTemplate.getSkillpoint();
CraftingTask craftingTask = new CraftingTask(player, (StaticObject) target, recipeTemplate, skillLvlDiff, craftType == $bonusCraftType ? $bonusPercent : 0);
if (skillId == $morphSkill) {
	craftingTask.setInterval($morphInterval);
} else {
	int interval = $intervalBase - (skillLvlDiff * $intervalStep);
	craftingTask.setInterval(interval < intervalCap ? intervalCap : interval);
}
craftingTask.start();
"""

CHECK_CRAFT = """
if (recipeTemplate == null) {
	return false;
}
if (itemTemplate == null) {
	return false;
}
if (player.getInteractionTask() instanceof CraftingTask craftingTask && craftingTask.isInProgress()) {
	return false;
}
if ((skillId != $morphSkill)) {
	if (target == null || !(target instanceof StaticObject)) {
		AuditLogger.log(player, "tried to craft with incorrect target");
		return false;
	} else if (!PositionUtil.isInRange(player, target, $stationRange, false)) {
		PacketSendUtility.sendPacket(player, SM_SYSTEM_MESSAGE.STR_COMBINE_TOO_FAR_FROM_TOOL(target.getObjectTemplate().getL10n()));
		return false;
	}
}
if (recipeTemplate.getDp() != null && (player.getCommonData().getDp() < recipeTemplate.getDp())) {
	AuditLogger.log(player, "tried to craft without required DP count");
	return false;
}
if (player.isInPlayerMode(PlayerMode.RIDE) || player.isInAnyHide()) {
	PacketSendUtility.sendPacket(player, SM_SYSTEM_MESSAGE.STR_SKILL_CAN_NOT_COMBINE_WHILE_IN_CURRENT_STANCE());
	return false;
}
if (player.getInventory().isFull()) {
	PacketSendUtility.sendPacket(player, SM_SYSTEM_MESSAGE.STR_COMBINE_INVENTORY_IS_FULL());
	return false;
}
if (!player.getRecipeList().isRecipePresent(recipeTemplate.getId())) {
	PacketSendUtility.sendPacket(player, SM_SYSTEM_MESSAGE.STR_COMBINE_CAN_NOT_FIND_RECIPE());
	return false;
}
if (recipeTemplate.getCraftDelayId() != null && player.getCraftCooldowns().hasCooldown(recipeTemplate.getCraftDelayId())) {
	PacketSendUtility.sendPacket(player, SM_SYSTEM_MESSAGE.STR_ITEM_CANT_USE_UNTIL_DELAY_TIME());
	return false;
}
if (!player.getSkillList().isSkillPresent(skillId)) {
	PacketSendUtility.sendPacket(player, SM_SYSTEM_MESSAGE.STR_COMBINE_CANT_USE(DataManager.SKILL_DATA.getSkillTemplate(skillId).getL10n()));
	return false;
}
if (player.getSkillList().getSkillLevel(skillId) < recipeTemplate.getSkillpoint()) {
	PacketSendUtility.sendPacket(player,
		SM_SYSTEM_MESSAGE.STR_COMBINE_OUT_OF_SKILL_POINT(DataManager.SKILL_DATA.getSkillTemplate(skillId).getL10n()));
	return false;
}
for (ComponentsData componentsData : recipeTemplate.getComponents()) {
	Component firstComponent = componentsData.getComponent().get(0);
	if (!sendMaterialsData.containsKey(firstComponent.getItemId()))
		continue;
	for (Component component : componentsData.getComponent()) {
		long availableComponentCount = player.getInventory().getItemCountByItemId(component.getItemId());
		if (availableComponentCount < component.getQuantity()) {
			String itemL10n = DataManager.ITEM_DATA.getItemTemplate(component.getItemId()).getL10n();
			if (component.getQuantity() == 1)
				PacketSendUtility.sendPacket(player, SM_SYSTEM_MESSAGE.STR_COMBINE_NO_COMPONENT_ITEM_SINGLE(itemL10n));
			else
				PacketSendUtility.sendPacket(player, SM_SYSTEM_MESSAGE.STR_COMBINE_NO_COMPONENT_ITEM_MULTIPLE(component.getQuantity(), itemL10n));
			return false;
		}
	}
	break;
}
if (craftType == $bonusCraftType && !player.getInventory().decreaseByItemId(getBonusReqItem(skillId), 1)) {
	PacketSendUtility.sendPacket(player,
		SM_SYSTEM_MESSAGE.STR_COMBINE_NO_COMPONENT_ITEM_SINGLE(DataManager.ITEM_DATA.getItemTemplate(getBonusReqItem(skillId)).getL10n()));
	return false;
}
for (ComponentsData componentsData : recipeTemplate.getComponents()) {
	Component firstComponent = componentsData.getComponent().get(0);
	if (!sendMaterialsData.containsKey(firstComponent.getItemId()))
		continue;
	for (Component component : componentsData.getComponent())
		player.getInventory().decreaseByItemId(component.getItemId(), component.getQuantity());
	break;
}
return true;
"""

SEND_CANCEL_CRAFT = """
PacketSendUtility.sendPacket(player, new SM_CRAFT_UPDATE(skillId, itemTemplate, 0, 0, $actionCancel, 0, 0));
PacketSendUtility.broadcastPacket(player, new SM_CRAFT_ANIMATION(player.getObjectId(), targetObjId, 0, $animCancel), true);
"""

CRAFTING_ON_ABORT = """
PacketSendUtility.sendPacket(requester, new SM_CRAFT_UPDATE(recipeTemplate.getSkillId(), itemTemplate, 0, 0, $actionAbort, 0, 0));
PacketSendUtility.broadcastPacket(requester, new SM_CRAFT_ANIMATION(requester.getObjectId(), responder.getObjectId(), 0, $animAbort), true);
"""

CM_CRAFT_RUN = """
Player player = getConnection().getActivePlayer();
if (player == null || !player.isSpawned())
	return;
if (GameServer.isShuttingDownSoon())
	return;
if (unk != $morphUnk) {
	VisibleObject staticObject = player.getKnownList().getObject(targetObjId);
	if (staticObject == null || !PositionUtil.isInRange(player, staticObject, $packetRange)
		|| staticObject.getObjectTemplate().getTemplateId() != targetTemplateId)
		return;
}
CraftService.startCrafting(player, recipeId, targetObjId, craftType, materialsData);
"""

CRAFTING_TASK_CONSTRUCTOR = """
super(requester, responder, skillLvlDiff);
this.recipeTemplate = recipeTemplate;
this.maxCritCount = recipeTemplate.getComboProductSize();
this.bonus = bonus;
this.itemTemplate = DataManager.ITEM_DATA.getItemTemplate(recipeTemplate.getProductId());
"""

CRAFTING_ON_FAILURE = """
PacketSendUtility.sendPacket(requester,
	new SM_CRAFT_UPDATE(recipeTemplate.getSkillId(), itemTemplate, currentSuccessValue, currentFailureValue, $actionFailure, 0, 0));
PacketSendUtility.broadcastPacket(requester, new SM_CRAFT_ANIMATION(requester.getObjectId(), responder.getObjectId(), 0, $animFailure), true);
"""

CRAFTING_ON_SUCCESS = """
if (calculateCrit()) {
	onInteractionStart();
	return false;
} else {
	PacketSendUtility.sendPacket(requester,
		new SM_CRAFT_UPDATE(recipeTemplate.getSkillId(), itemTemplate, currentSuccessValue, currentFailureValue, $actionSuccess, 0, 0));
	PacketSendUtility.broadcastPacket(requester, new SM_CRAFT_ANIMATION(requester.getObjectId(), responder.getObjectId(), 0, $animSuccess), true);
	CraftService.finishCrafting(requester, recipeTemplate, critCount, bonus);
	return true;
}
"""

CALCULATE_CRIT = """
if (critCount >= maxCritCount)
	return false;
if (recipeTemplate.getComboProduct(critCount + 1) == null)
	return false;
float chance;
if (critCount == 0)
	chance = Rates.get(requester, RatesConfig.CRAFT_CRIT_CHANCES);
else
	chance = Rates.get(requester, RatesConfig.CRAFT_COMBO_CHANCES);
House house = requester.getActiveHouse();
if (house != null)
	switch (house.getHouseType()) {
		case ESTATE:
		case PALACE:
			chance += $houseBonus;
			break;
	}
if (Rnd.chance() >= chance)
	return false;
critCount++;
itemTemplate = DataManager.ITEM_DATA.getItemTemplate(recipeTemplate.getComboProduct(critCount));
return true;
"""

CRAFTING_SEND_UPDATE = """
PacketSendUtility.sendPacket(requester, new SM_CRAFT_UPDATE(recipeTemplate.getSkillId(), itemTemplate, currentSuccessValue, currentFailureValue,
	craftType.getProgressId(), executionSpeed, showBarDelay));
"""

CRAFTING_ON_START = """
currentSuccessValue = 0;
currentFailureValue = 0;
PacketSendUtility.sendPacket(requester,
	new SM_CRAFT_UPDATE(recipeTemplate.getSkillId(), itemTemplate, fullBarValue, fullBarValue, critCount == 0 ? $actionInit : $actionProc, 0, 0));
PacketSendUtility.sendPacket(requester, new SM_CRAFT_UPDATE(recipeTemplate.getSkillId(), itemTemplate, 0, 0, $actionStart, 0, 0));
PacketSendUtility.broadcastPacket(requester,
	new SM_CRAFT_ANIMATION(requester.getObjectId(), responder.getObjectId(), recipeTemplate.getSkillId(), $animInit), true);
PacketSendUtility.broadcastPacket(requester,
	new SM_CRAFT_ANIMATION(requester.getObjectId(), responder.getObjectId(), recipeTemplate.getSkillId(), $animStart), true);
"""

CRAFTING_ANALYZE = """
if (recipeTemplate.getSkillId() == $morphSkill) {
	currentSuccessValue = fullBarValue;
	return;
} else if (skillLvlDiff < 0) {
	currentFailureValue = fullBarValue;
	return;
}
craftType = CraftType.NORMAL;
float multi = Rnd.nextFloat($f:multiMin, $f:multiMax);
float failReduction = Math.max(1 - skillLvlDiff * $f:failReductionStep, $f:failReductionMin);
boolean success = skillLvlDiff >= $alwaysSuccessDiff || Rnd.chance() >= CraftConfig.MAX_CRAFT_FAILURE_CHANCE * failReduction;
float bonusModifier = 1;
switch (itemTemplate.getItemQuality()) {
	case LEGEND:
		bonusModifier = $f:bmLegend;
		break;
	case UNIQUE:
		bonusModifier = $f:bmUnique;
		break;
	case EPIC:
		bonusModifier = $f:bmEpic;
		break;
	case MYTHIC:
		bonusModifier = $f:bmMythic;
		break;
}
if (success) {
	if (Rnd.chance() < ($blueBase + skillLvlDiff / $f:blueDivisor))
		craftType = CraftType.CRIT_BLUE;
	int minStep = $successMinStep;
	int lvlBoni = skillLvlDiff > $lvlBoniFrom ? ((skillLvlDiff - $lvlBoniFrom) * $lvlBoniFactor) : 0;
	int bonus = (int) (((craftType == CraftType.CRIT_BLUE ? $blueBonus : 0) + (((skillLvlDiff + 1) / $f:halfDivisor) + lvlBoni) * $stepFactor) * multi);
	currentSuccessValue += Math.round(minStep + (bonus * bonusModifier));
} else {
	int minStep = recipeTemplate.getMaxProductionCount() != null ? $failMinStepLimited : $failMinStep;
	int bonus = (int) (((skillLvlDiff + 1) / $f:failDivisor * $failFactor) * multi);
	currentFailureValue += Math.round(minStep + (bonus * bonusModifier));
}
if (currentSuccessValue > fullBarValue)
	currentSuccessValue = fullBarValue;
else if (currentFailureValue > fullBarValue)
	currentFailureValue = fullBarValue;
int speed = bonusModifier < 1 ? Math.round($speedBase * (2 - bonusModifier)) : ($speedBase - (skillLvlDiff * $speedStep));
executionSpeed = Math.max(speed, $speedMin);
showBarDelay = bonusModifier < 1 ? $delayBase : Math.max($delayMin, $delayBase - (skillLvlDiff * $delayStep));
"""

CRAFT_TASK_ON_INTERACTION = """
if (currentSuccessValue == fullBarValue) {
	return onSuccessFinish();
}
if (currentFailureValue == fullBarValue) {
	onFailureFinish();
	return true;
}
analyzeInteraction();
sendInteractionUpdate();
return false;
"""

INTERACTION_START = """
AbstractInteractionTask oldTask = requester.getInteractionTask();
if (oldTask != null)
	oldTask.abort();
requester.setInteractionTask(this);
RecallService.getInstance().cancel(requester, CancelReason.CANCELLED);
onInteractionStart();
task = ThreadPoolManager.getInstance().scheduleAtFixedRate(new Runnable() {
	@Override
	public void run() {
		boolean stopTask = !requester.isOnline() || onInteraction();
		if (stopTask)
			stop();
	}
}, delay, interval);
"""

INTERACTION_STOP = """
if (requester.getInteractionTask() == this)
	requester.setInteractionTask(null);
onInteractionFinish();
if (task != null && !task.isCancelled()) {
	task.cancel(false);
	task = null;
}
"""

INTERACTION_ABORT = """
onInteractionAbort();
stop();
"""

GATHERING_TASK_CONSTRUCTOR = """
super(requester, gatherable, skillLvlDiff);
this.template = gatherable.getObjectTemplate();
this.gathererObserver = createGathererObserver();
this.material = material;
this.delay = Rnd.get($startDelayMin, $startDelayMax);
int gatherInterval = $intervalBase - (skillLvlDiff * $intervalStep);
this.interval = gatherInterval < $intervalMin ? $intervalMin : gatherInterval;
"""

GATHERING_ON_ABORT = """
PacketSendUtility.broadcastPacket(requester, new SM_GATHER_ANIMATION(requester.getObjectId(), responder.getObjectId(), template.getHarvestSkill(), $animAbort));
PacketSendUtility.sendPacket(requester, new SM_GATHER_UPDATE(template, material, 0, 0, $actionAbort, 0, 0));
"""

GATHERING_ON_FINISH = """
requester.getObserveController().removeObserver(gathererObserver);
((Gatherable) responder).getController().completeInteraction();
"""

GATHERING_ON_START = """
requester.getObserveController().attach(gathererObserver);
PacketSendUtility.sendPacket(requester, new SM_GATHER_UPDATE(template, material, fullBarValue, fullBarValue, $actionInit, 0, 0));
PacketSendUtility.sendPacket(requester, new SM_GATHER_UPDATE(template, material, 0, 0, $actionStart, 0, 0));
PacketSendUtility.broadcastPacket(requester, new SM_GATHER_ANIMATION(requester.getObjectId(), responder.getObjectId(), template.getHarvestSkill(), $animInit), true);
PacketSendUtility.broadcastPacket(requester, new SM_GATHER_ANIMATION(requester.getObjectId(), responder.getObjectId(), template.getHarvestSkill(), $animStart), true);
"""

GATHERING_SEND_UPDATE = """
PacketSendUtility.sendPacket(requester, new SM_GATHER_UPDATE(template, material, currentSuccessValue, currentFailureValue, craftType.getProgressId(), executionSpeed, showBarDelay));
"""

GATHERING_ON_FAILURE = """
PacketSendUtility.sendPacket(requester, new SM_GATHER_UPDATE(template, material, currentSuccessValue, currentFailureValue, $actionFailPre, 0, 0));
PacketSendUtility.sendPacket(requester, new SM_GATHER_UPDATE(template, material, currentSuccessValue, currentFailureValue, $actionFailure, 0, 0));
PacketSendUtility.broadcastPacket(requester, new SM_GATHER_ANIMATION(requester.getObjectId(), responder.getObjectId(), template.getHarvestSkill(), $animFailure), true);
"""

GATHERING_ON_SUCCESS = """
PacketSendUtility.broadcastPacket(requester, new SM_GATHER_ANIMATION(requester.getObjectId(), responder.getObjectId(), template.getHarvestSkill(), $animSuccess), true);
PacketSendUtility.sendPacket(requester, new SM_GATHER_UPDATE(template, material, currentSuccessValue, currentFailureValue, $actionSuccess, 0, 0));
if (template.getEraseValue() > 0)
	requester.getInventory().decreaseByItemId(template.getRequiredItemId(), template.getEraseValue());
ItemService.addItem(requester, material.getItemId(), Rates.GATHERING_COUNT.calcResult(requester, $gatherCount));
requester.getPosition().getWorldMapInstance().getInstanceHandler().onGather(requester, (Gatherable) responder);
((Gatherable) responder).getController().rewardPlayer(requester);
return true;
"""

GATHERING_ANALYZE = """
if (skillLvlDiff >= $instantDiff) {
	currentSuccessValue = fullBarValue;
	executionSpeed = $fastSpeed;
	showBarDelay = $fastDelay;
	return;
} else if (skillLvlDiff < 0) {
	currentFailureValue = fullBarValue;
	return;
}
craftType = CraftType.NORMAL;
float multi = Rnd.nextFloat($f:multiMin, $f:multiMax);
float failReduction = Math.max(1 - skillLvlDiff * $f:failReductionStep, $f:failReductionMin);
boolean success = Rnd.chance() >= CraftConfig.MAX_GATHER_FAILURE_CHANCE * failReduction;
if (success) {
	float critChance = Rnd.chance();
	if (critChance < ($purpleBase + skillLvlDiff / $f:purpleDivisor)) {
		craftType = CraftType.CRIT_PURPLE;
		currentSuccessValue = fullBarValue;
		executionSpeed = $fastSpeed;
		showBarDelay = $fastDelay;
		return;
	} else if (critChance < ($blueBase + skillLvlDiff / $f:blueDivisor)) {
		craftType = CraftType.CRIT_BLUE;
	}
	int lvlBoni = skillLvlDiff > $lvlBoniFrom ? ((skillLvlDiff - $lvlBoniFrom) * $lvlBoniFactor) : 0;
	currentSuccessValue += Math.round($successMinStep + ((craftType == CraftType.CRIT_BLUE ? $blueBonus : 0) + (((skillLvlDiff + 1) / $f:halfDivisor) + lvlBoni) * $stepFactor) * multi);
} else {
	currentFailureValue += Math.round($failMinStep + (((skillLvlDiff + 1) / $f:failDivisor * $failFactor) * multi));
}
if (currentSuccessValue > fullBarValue) {
	currentSuccessValue = fullBarValue;
} else if (currentFailureValue > fullBarValue) {
	currentFailureValue = fullBarValue;
}
int speed = $speedBase - (skillLvlDiff * $speedStep);
executionSpeed = speed < $speedMin ? $speedMin : speed;
showBarDelay = Math.max($delayMin, $delayBase - (skillLvlDiff * $delayStep));
"""

GATHERER_OBSERVER = """
return new ActionObserver(ObserverType.ALL) {
	@Override
	public void startSkillCast(Skill skill) {
		abort();
	}
	@Override
	public void attack(Creature creature, int skillId) {
		abort();
	}
	@Override
	public void attacked(Creature creature, int skillId) {
		abort();
	}
	@Override
	public void moved() {
		abort();
	}
	@Override
	public void dotattacked(Creature creature, Effect dotEffect) {
		abort();
	}
};
"""

START_GATHERING = """
GatherableTemplate template = getOwner().getObjectTemplate();
if (player.getLevel() < template.getLevelLimit()) {
	PacketSendUtility.sendPacket(player, SM_SYSTEM_MESSAGE.STR_MSG_CANT_GATHERING_B_LEVEL_CHECK(template.getLevelLimit()));
	return;
}
if (player.isInPlayerMode(PlayerMode.RIDE) && !player.hasPermission(MembershipConfig.GATHERING_ALLOW_ON_MOUNT)) {
	PacketSendUtility.sendPacket(player, SM_SYSTEM_MESSAGE.STR_MSG_GATHER_RESTRICTION_RIDE());
	return;
}
if (player.getInventory().isFull()) {
	PacketSendUtility.sendPacket(player, SM_SYSTEM_MESSAGE.STR_GATHER_INVENTORY_IS_FULL());
	return;
}
if (player.getController().isUnderStance()) {
	PacketSendUtility.sendPacket(player, SM_SYSTEM_MESSAGE.STR_SKILL_CAN_NOT_GATHER_WHILE_IN_CURRENT_STANCE());
	return;
}
if (!PositionUtil.isInRange(getOwner(), player, $gatherRange, false)) {
	PacketSendUtility.sendPacket(player, SM_SYSTEM_MESSAGE.STR_GATHER_TOO_FAR_FROM_GATHER_SOURCE());
	return;
}
if (!GeoService.getInstance().canSee(player, getOwner())) {
	PacketSendUtility.sendPacket(player, SM_SYSTEM_MESSAGE.STR_GATHER_OBSTACLE_EXIST());
	return;
}
if (player.isGatherRestricted()) {
	PacketSendUtility.sendPacket(player, SM_SYSTEM_MESSAGE.STR_MSG_CAPTCHA_REMAIN_RESTRICT_TIME(player.getGatherRestrictionDurationSeconds()));
	return;
}
if (!checkPlayerSkill(player, template))
	return;
List<Material> materials = getMaterials(player, template);
if (materials == null)
	return;
if (SecurityConfig.CAPTCHA_ENABLE) {
	if (SecurityConfig.CAPTCHA_APPEAR.equals(template.getSourceType()) || SecurityConfig.CAPTCHA_APPEAR.equals("ALL")) {
		int rate = SecurityConfig.CAPTCHA_APPEAR_RATE;
		if (template.getCaptchaRate() > 0)
			rate = (int) (template.getCaptchaRate() * 0.1f);
		if (Rnd.chance() < rate) {
			player.setCaptchaWord(CAPTCHAUtil.getRandomWord());
			player.setCaptchaImage(CAPTCHAUtil.createCAPTCHA(player.getCaptchaWord()).array());
			PunishmentService.setIsNotGatherable(player, 0, true, SecurityConfig.CAPTCHA_EXTRACTION_BAN_TIME * 1000L);
		}
	}
}
int chance = Rnd.nextInt($rollBound);
int current = 0;
Material curMaterial = null;
for (Material mat : materials) {
	current += mat.getRate();
	if (current >= chance) {
		curMaterial = mat;
		break;
	}
}
synchronized (this) {
	if (gatheringTask != null) {
		PacketSendUtility.sendPacket(player, new SM_GATHER_UPDATE(template, curMaterial, 0, 0, $actionOccupied, 0, 0));
		return;
	}
	int skillLvlDiff = player.getSkillList().getSkillLevel(template.getHarvestSkill()) - template.getSkillLevel();
	gatheringTask = new GatheringTask(player, getOwner(), curMaterial, skillLvlDiff);
	gatheringTask.start();
}
"""

CHECK_PLAYER_SKILL = """
int harvestSkillId = template.getHarvestSkill();
if (!player.getSkillList().isSkillPresent(harvestSkillId)) {
	if (harvestSkillId == $humanSkill) {
		PacketSendUtility.sendPacket(player, SM_SYSTEM_MESSAGE.STR_GATHER_INCORRECT_SKILL());
	} else {
		PacketSendUtility.sendPacket(player,
			SM_SYSTEM_MESSAGE.STR_GATHER_LEARN_SKILL(DataManager.SKILL_DATA.getSkillTemplate(harvestSkillId).getL10n()));
	}
	return false;
}
if (player.getSkillList().getSkillLevel(harvestSkillId) < template.getSkillLevel()) {
	PacketSendUtility.sendPacket(player,
		SM_SYSTEM_MESSAGE.STR_GATHER_OUT_OF_SKILL_POINT(DataManager.SKILL_DATA.getSkillTemplate(harvestSkillId).getL10n()));
	return false;
}
return true;
"""

GET_MATERIALS = """
if (template.getRequiredItemId() > 0) {
	if (template.getCheckType() == $checkEquipped) {
		boolean hasRequiredItemEquipped = !player.getEquipment().getEquippedItemsByItemId(template.getRequiredItemId()).isEmpty();
		if (hasRequiredItemEquipped)
			return template.getExtraMaterials().getMaterial();
	} else if (template.getCheckType() == $checkInventory) {
		if (player.getInventory().getItemCountByItemId(template.getRequiredItemId()) < template.getEraseValue()) {
			String requiredItemL10n = ChatUtil.l10n(template.getRequiredItemNameId());
			PacketSendUtility.sendPacket(player, SM_SYSTEM_MESSAGE.STR_MSG_CANT_GATHERING_B_ITEM_CHECK(requiredItemL10n));
			return null;
		}
		return template.getExtraMaterials().getMaterial();
	}
}
return template.getMaterials().getMaterial();
"""

COMPLETE_INTERACTION = """
synchronized (this) {
	gatheringTask = null;
	if (++gatherCount == getOwner().getObjectTemplate().getHarvestCount()) {
		if (getOwner().isInInstance())
			getOwner().getController().delete();
		else
			getOwner().getController().deleteAndScheduleRespawn();
	}
}
"""

REWARD_PLAYER = """
if (player != null) {
	int skillLvl = getOwner().getObjectTemplate().getSkillLevel();
	int xpReward = (int) (($d:xpFactor * (skillLvl + $d:xpOffset) * (skillLvl + $d:xpOffset2) + $xpBase));
	int skillId = getOwner().getObjectTemplate().getHarvestSkill();
	int gainedGatherXp = Rates.SKILL_XP_GATHERING.calcResult(player, xpReward);
	StatEnum boostStat = StatEnum.getModifier(skillId);
	if (boostStat != null)
		gainedGatherXp *= player.getGameStats().getStat(boostStat, $boostBase).getCurrent() / $f:boostDivisor;
	gainedGatherXp = Math.max($minXp, gainedGatherXp);
	if (player.getSkillList().addSkillXp(player, skillId, gainedGatherXp, skillLvl)) {
		PacketSendUtility.sendPacket(player, SM_SYSTEM_MESSAGE.STR_EXTRACT_GATHERING_SUCCESS_GETEXP());
		player.getCommonData().addExp(xpReward, Rates.XP_GATHERING);
	} else
		PacketSendUtility.sendPacket(player, SM_SYSTEM_MESSAGE
			.STR_MSG_DONT_GET_PRODUCTION_EXP(DataManager.SKILL_DATA.getSkillTemplate(skillId).getL10n()));
}
"""

ADD_SKILL_XP = """
PlayerSkillEntry skill = getSkillEntry(skillId);
int skillLvl = skill.getSkillLevel();
if (skillLvl - objSkillLvl > $maxGap)
	return false;
switch (skillId) {
	case $humanSkill:
		if (skillLvl == $humanCap)
			return false;
	case $tapA:
	case $tapB:
		if (skillLvl == $tapFree || skillLvl >= $tapCap && CraftConfig.DISABLE_AETHER_AND_ESSENCE_TAPPING_CAP)
			break;
	case $c1:
	case $c2:
	case $c3:
	case $c4:
	case $c5:
	case $c6:
	case $c7:
		switch (skillLvl) {
			case $l1:
			case $l2:
			case $l3:
			case $l4:
			case $l5:
			case $l6:
			case $l7:
				PacketSendUtility.sendPacket(player, SM_SYSTEM_MESSAGE.STR_CRAFT_INFO_MAXPOINT_UP());
				return false;
		}
}
int requiredExp = (int) ($d:lvlFactor * (skillLvl + $d:lvlOffset) * (skillLvl + $d:lvlOffset));
if (skill.getCurrentXp() + xpReward >= requiredExp) {
	skillLvl++;
	skill.setCurrentXp(0);
	skill.setSkillLvl(skillLvl);
	SkillLearnService.onLearnSkill(player, skillId, skillLvl, false);
} else
	skill.setCurrentXp(skill.getCurrentXp() + xpReward);
return true;
"""

ON_LEARN_SKILL = """
PlayerSkillEntry skill = player.getSkillList().getSkillEntry(skillId);
if (skill.isProfessionSkill())
	switch (skillLevel) {
		case $anim1, $anim2, $anim3, $anim4, $anim5, $anim6, $anim7 -> {
			if (skillLevel != $anim1 || skill.isCraftingSkill())
				PacketSendUtility.broadcastPacket(player, new SM_ACTION_ANIMATION(player.getObjectId(), ActionAnimation.CRAFT_LEVEL_UP), true);
		}
	}
if (player.getEffectController() != null) {
	if (player.isSpawned())
		sendPacket(player, skill, isNew);
	SkillTemplate skillTemplate = DataManager.SKILL_DATA.getSkillTemplate(skillId);
	if (skillTemplate.isPassive())
		SkillEngine.getInstance().applyEffectDirectly(skillTemplate, skillLevel, player, player);
	if (skill.isProfessionSkill() && (skill.getSkillLevel() == $quest1 || skill.getSkillLevel() == $quest2))
		player.getController().updateNearbyQuests();
}
if (skill.isCraftingSkill() || skill.isMorphSkill())
	RecipeService.autoLearnRecipes(player, skillId, skillLevel);
"""

AUTO_LEARN_RECIPES = """
for (RecipeTemplate recipe : DataManager.RECIPE_DATA.getAutolearnRecipes(player.getRace(), skillId, skillLvl))
	player.getRecipeList().addRecipe(player, recipe.getId());
"""

RECIPE_DATA_AFTER_UNMARSHAL = """
for (RecipeTemplate it : list) {
	recipeData.put(it.getId(), it);
	if (it.getAutoLearn() != 0)
		autoLearnRecipes.add(it);
}
list = null;
"""

GET_AUTOLEARN_RECIPES = """
List<RecipeTemplate> list = new ArrayList<>();
for (RecipeTemplate recipe : autoLearnRecipes) {
	if (recipe.getSkillId() != skillId || recipe.getSkillpoint() > maxLevel)
		continue;
	if (recipe.getRace() != Race.PC_ALL && recipe.getRace() != race)
		continue;
	list.add(recipe);
}
return list;
"""

GET_COMBO_PRODUCT = """
if (comboproduct == null || comboproduct.get(num - 1) == null) {
	return null;
}
return comboproduct.get(num - 1).getItemId();
"""

GET_COMBO_PRODUCT_SIZE = """
if (comboproduct == null) {
	return 0;
}
return comboproduct.size();
"""

GATHERABLE_DATA_AFTER_UNMARSHAL = """
for (GatherableTemplate gatherable : gatherables) {
	if (gatherable.getMaterials() != null)
		gatherable.getMaterials().getMaterial().sort(null);
	if (gatherable.getExtraMaterials() != null)
		gatherable.getExtraMaterials().getMaterial().sort(null);
	gatherableData.put(gatherable.getTemplateId(), gatherable);
}
gatherables = null;
"""

LEARN_SKILL = """
if (player.getLevel() < $minLevel)
	return;
Profession profession = professionByNpc.get(npc.getNpcId());
if (profession == null)
	return;
int skillId = profession.getSkillId();
if (skillId == 0)
	return;
PlayerSkillList skillList = player.getSkillList();
int skillLevel = skillList.isSkillPresent(skillId) ? skillList.getSkillLevel(skillId) : 0;
Integer price = profession.getUpgradeCost(skillLevel);
if (price == null) {
	if (skillLevel > profession.getMaxUpgradableLevel())
		PacketSendUtility.sendPacket(player, SM_SYSTEM_MESSAGE.STR_MSG_DONT_RANK_UP_GATHERING());
	else if (skillLevel == 399)
		PacketSendUtility.sendPacket(player, SM_SYSTEM_MESSAGE.STR_CRAFT_CANT_EXTEND_MONEY());
	else if (skillLevel == 499)
		PacketSendUtility.sendPacket(player, SM_SYSTEM_MESSAGE.STR_CRAFT_CANT_EXTEND_GRAND_MASTER());
	else
		PacketSendUtility.sendPacket(player, SM_SYSTEM_MESSAGE.STR_MSG_DONT_RANK_UP());
	return;
}
RequestResponseHandler<Npc> responseHandler = new RequestResponseHandler<Npc>(npc) {
	@Override
	public void acceptRequest(Npc requester, Player responder) {
		if (responder.getInventory().tryDecreaseKinah(price, ItemUpdateType.DEC_KINAH_LEARN)) {
			PlayerSkillList skillList = responder.getSkillList();
			skillList.addSkill(responder, skillId, skillLevel + 1);
		} else {
			PacketSendUtility.sendPacket(responder, SM_SYSTEM_MESSAGE.STR_NOT_ENOUGH_MONEY());
		}
	}
};
if (player.getResponseRequester().putRequest(SM_QUESTION_WINDOW.STR_CRAFT_ADDSKILL_CONFIRM, responseHandler)) {
	String professionName = skillLevel == 0 ? profession.getClientName() : profession.getClientName(skillLevel + 1);
	PacketSendUtility.sendPacket(player,
		new SM_QUESTION_WINDOW(SM_QUESTION_WINDOW.STR_CRAFT_ADDSKILL_CONFIRM, 0, 0, professionName, String.valueOf(price)));
}
"""

RATES_GET = """
if (membershipRates.length == 0) {
	LoggerFactory.getLogger(Rates.class).warn("Missing rates", new IllegalStateException());
	return 1;
}
int membershipLevel = player.getAccount().getMembership();
return membershipRates[Math.min(membershipRates.length - 1, membershipLevel)];
"""

RATES_CALC_XP_RATE = """
float endRate = get(player, membershipRates);
endRate *= player.getGameStats().getStat(boostRate, $boostBase).getCurrent() / $f:boostDivisor;
if (player.isLegionMember() && player.getLegion().hasBonus())
	endRate *= $f:legionBonus;
return endRate;
"""

RATES_CALC_RESULT_INT = """
long result = calcResult(player, (long) value);
try {
	return Math.toIntExact(result);
} catch (ArithmeticException e) {
	LoggerFactory.getLogger(getClass()).error(name() + " result is too large for " + player + ": " + result, e);
	return value;
}
"""

RATES_CONSTANTS = {
	"XP_GATHERING": "XP_GATHERING { @Override public long calcResult(Player player, long xp) { "
	                "return (long) (xp * calcXpRate(player, RatesConfig.XP_GATHERING_RATES, StatEnum.BOOST_GATHERING_XP_RATE)); } }",
	"XP_CRAFTING": "XP_CRAFTING { @Override public long calcResult(Player player, long xp) { "
	               "return (long) (xp * calcXpRate(player, RatesConfig.XP_CRAFTING_RATES, StatEnum.BOOST_CRAFTING_XP_RATE)); } }",
	"SKILL_XP_GATHERING": "SKILL_XP_GATHERING { @Override public long calcResult(Player player, long skillXp) { "
	                      "return (long) (skillXp * get(player, RatesConfig.SKILL_XP_GATHERING_RATES)); } }",
	"SKILL_XP_CRAFTING": "SKILL_XP_CRAFTING { @Override public long calcResult(Player player, long skillXp) { "
	                     "return (long) (skillXp * get(player, RatesConfig.SKILL_XP_CRAFTING_RATES)); } }",
	"GATHERING_COUNT": "GATHERING_COUNT { @Override public long calcResult(Player player, long gatherCount) { "
	                   "return (long) (gatherCount * get(player, RatesConfig.GATHERING_COUNT_RATES)); } }",
}

ADD_EXP = """
if (noExp)
	return;
long reward = value;
long repose = 0;
long salvation = 0;
Player player = getPlayer();
if (player != null && player.getWorldId() == $noExpWorld)
	return;
if (player != null)
	reward = rates.calcResult(player, value);
if (reward > 0) {
	if (getCurrentReposeEnergy() > 0) {
		long allowedExp = Math.min(getCurrentReposeEnergy(), reward);
		addReposeEnergy(-allowedExp);
		repose = (long) ((allowedExp / 100f) * 40);
	}
	if (isReadyForSalvationPoints() && getCurrentSalvationPercent() > 0) {
		salvation = (long) ((reward / 100f) * getCurrentSalvationPercent());
	}
	reward += repose + salvation;
}
setExp(exp + reward);
if (player != null) {
	if (repose > 0 && salvation > 0) {
		if (name != null)
			PacketSendUtility.sendPacket(player, SM_SYSTEM_MESSAGE.STR_GET_EXP_VITAL_MAKEUP_BONUS(name, reward, repose, salvation));
		else
			PacketSendUtility.sendPacket(player, SM_SYSTEM_MESSAGE.STR_GET_EXP2_VITAL_MAKEUP_BONUS(reward, repose, salvation));
	} else if (repose > 0 && salvation == 0) {
		if (name != null)
			PacketSendUtility.sendPacket(player, SM_SYSTEM_MESSAGE.STR_GET_EXP_VITAL_BONUS(name, reward, repose));
		else
			PacketSendUtility.sendPacket(player, SM_SYSTEM_MESSAGE.STR_GET_EXP2_VITAL_BONUS(reward, repose));
	} else if (repose == 0 && salvation > 0) {
		if (name != null)
			PacketSendUtility.sendPacket(player, SM_SYSTEM_MESSAGE.STR_GET_EXP_MAKEUP_BONUS(name, reward, salvation));
		else
			PacketSendUtility.sendPacket(player, SM_SYSTEM_MESSAGE.STR_GET_EXP2_MAKEUP_BONUS(reward, salvation));
	} else {
		if (name != null)
			PacketSendUtility.sendPacket(player, SM_SYSTEM_MESSAGE.STR_GET_EXP(name, reward));
		else
			PacketSendUtility.sendPacket(player, SM_SYSTEM_MESSAGE.STR_GET_EXP2(reward));
	}
	if (getLevel() == 9 && exp >= DataManager.PLAYER_EXPERIENCE_TABLE.getStartExpForLevel(10))
		PacketSendUtility.sendPacket(player, SM_SYSTEM_MESSAGE.STR_LEVEL_LIMIT_QUEST_NOT_FINISHED1());
}
"""

SET_DP = """
if (playerClass.isStartingClass())
	return;
int maxDp = (getPlayer() == null) ? -1 : getPlayer().getGameStats().getMaxDp().getCurrent();
this.dp = (maxDp >= 0 && dp > maxDp) ? maxDp : dp;
if (getPlayer() != null) {
	PacketSendUtility.broadcastPacket(getPlayer(), new SM_DP_INFO(playerObjId, this.dp), true);
	getPlayer().getGameStats().updateStatsAndSpeedVisually();
	PacketSendUtility.sendPacket(getPlayer(), new SM_STATUPDATE_DP(this.dp));
}
"""

SM_CRAFT_UPDATE_CONSTRUCTOR = """
this.action = action;
this.skillId = skillId;
this.itemId = item.getTemplateId();
this.success = success;
this.failure = failure;
this.itemNameL10n = item.getL10n();
this.executionSpeed = executionSpeed;
if (skillId == $morphSkill) {
	this.delay = $morphDelay;
} else {
	this.delay = delay;
}
"""

SM_GATHER_UPDATE_CONSTRUCTOR = """
this.skillId = template.getHarvestSkill();
this.action = action;
this.itemId = material.getItemId();
this.success = success;
this.failure = failure;
this.executionSpeed = executionSpeed;
this.delay = delay;
this.l10n = material.getL10n();
"""

INTERACTION_TASK_CONSTRUCTOR = """
this.requester = requester;
if (responder == null)
	this.responder = requester;
else
	this.responder = responder;
"""

CRAFT_TASK_CONSTRUCTOR = """
super(requester, responder);
this.skillLvlDiff = skillLvlDiff;
"""

CRAFT_TYPE = """
NORMAL($normal),
CRIT_BLUE($blue),
CRIT_PURPLE($purple);
private int progressId;
private CraftType(int progressId) {
	this.progressId = progressId;
}
public int getProgressId() {
	return progressId;
}
"""

GET_COMPONENTS = "return componentsData == null ? Collections.<ComponentsData> emptyList() : componentsData;"

GET_MATERIAL = """
if (material == null) {
	material = new ArrayList<>();
}
return this.material;
"""

IS_IN_RANGE_4 = """
if (object.getWorldId() != object2.getWorldId() || object.getInstanceId() != object2.getInstanceId())
	return false;
if (!centerToCenter) {
	range += object.getObjectTemplate().getBoundRadius().getMaxOfFrontAndSide();
	range += object2.getObjectTemplate().getBoundRadius().getMaxOfFrontAndSide();
}
return isInRange(object.getX(), object.getY(), object.getZ(), object2.getX(), object2.getY(), object2.getZ(), range);
"""

IS_IN_RANGE_7 = """
float dx = x1 - x2;
float dy = y1 - y2;
float dz = z1 - z2;
return dx * dx + dy * dy + dz * dz < range * range;
"""

BROADCAST_TO_SELF = """
if (toSelf)
	sendPacket(player, packet);
broadcastPacket(player, packet);
"""

STORAGE_DECREASE_BY_ITEM_ID = """
List<Item> items = itemStorage.getItemsById(itemId);
if (items.size() == 0)
	return false;
for (Item item : items) {
	if (count == 0) {
		break;
	}
	count = decreaseItemCount(item, count, ItemUpdateType.DEC_ITEM_USE, questStatus, actor);
}
return count == 0;
"""

STORAGE_DECREASE_ITEM_COUNT = """
if (item == null)
	return 0;
ItemDeleteType deleteType = questStatus != null ? ItemDeleteType.fromQuestStatus(questStatus) : ItemDeleteType.fromUpdateType(updateType);
long leftCount = item.decreaseItemCount(count);
boolean isKinah = item.getItemTemplate().isKinah();
if (item.getItemCount() <= 0 && !isKinah)
	delete(item, deleteType, actor);
else
	ItemPacketService.sendItemPacket(actor, storageType, item, updateType);
setPersistentState(PersistentState.UPDATE_REQUIRED);
return leftCount;
"""

ITEM_DECREASE_ITEM_COUNT = """
if (count <= 0) {
	return 0;
}
long removeCount = count >= itemCount ? itemCount : count;
this.itemCount -= removeCount;
if (itemCount == 0 && !this.itemTemplate.isKinah()) {
	setPersistentState(PersistentState.DELETED);
} else {
	setPersistentState(PersistentState.UPDATE_REQUIRED);
}
return count - removeCount;
"""

STORAGE_ITEM_COUNT = """
List<Item> temp = this.itemStorage.getItemsById(itemId);
if (temp.size() == 0)
	return 0;
long cnt = 0;
for (Item item : temp)
	cnt += item.getItemCount();
return cnt;
"""

ITEM_STORAGE_ITEMS_BY_ID = """
List<Item> temp = new ArrayList<>();
for (Item item : items.values()) {
	if (item.getItemTemplate().getTemplateId() == itemId) {
		temp.add(item);
	}
}
return temp;
"""

SKILL_LEARN_SEND_PACKET = """
if (skill.isProfessionSkill()) {
	if (skill.isTappingSkill())
		PacketSendUtility.sendPacket(player, new SM_SKILL_LIST(skill, isNew ? $tapNew : $tapUp));
	else
		PacketSendUtility.sendPacket(player, new SM_SKILL_LIST(skill, isNew ? $craftNew : $craftUp));
} else if (isNew)
	PacketSendUtility.sendPacket(player,
		new SM_SKILL_LIST(skill, skill.isStigmaSkill() ? skill.isLinkedStigmaSkill() ? 1402891 : 1300401 : 1300050));
else
	PacketSendUtility.sendPacket(player, new SM_SKILL_LIST(skill, 0));
"""

# The members of the classes pinned whole (the module docstring): (header digest, ((member key, MODELLED or the digest of its text), ...)),
# written from JavaFile.describe_pins after reading each class. A member marked MODELLED is checked by a template in JavaCraftRules.read.
CLASS_PINS: dict[str, tuple[str, tuple[tuple[str, str], ...]]] = {
	'services/craft/CraftService.java': ('d3da03e0720325e2', (
		('private static final Logger log', 'eee79a87bac65202'),
		('@SuppressWarnings("lossy-conversions")public static void finishCrafting(Player player,RecipeTemplate recipetemplate,int critCount,'
		 'int bonus)', MODELLED),
		('public static void startCrafting(Player player,int recipeId,int targetObjId,int craftType,Map<Integer,Long>sendMaterialsData)', MODELLED),
		('private static boolean checkCraft(Player player,RecipeTemplate recipeTemplate,int skillId,VisibleObject target,ItemTemplate itemTemplate,'
		 'int craftType,Map<Integer,Long>sendMaterialsData)', MODELLED),
		('private static void sendCancelCraft(Player player,int skillId,int targetObjId,ItemTemplate itemTemplate)', MODELLED),
		('private static int getBonusReqItem(int skillId)', MODELLED),
	)),
	'network/aion/clientpackets/CM_CRAFT.java': ('7854ec61b64ec3b0', (
		('private int unk', '67d6b6835cc62c25'),
		('private int targetTemplateId', '0deab82fc58dc000'),
		('private int recipeId', '01e7b3cbefe87541'),
		('private int targetObjId', 'b1d85cc1bbaa1a13'),
		('private int craftType', 'df0214939025b7b5'),
		('private Map<Integer,Long>materialsData', 'e97f6952acc1408e'),
		('public CM_CRAFT(int opcode,Set<State>validStates)', '47b8e2a4735b19b2'),
		('@Override protected void readImpl()', '5e81bad7d52454bf'),
		('@Override protected void runImpl()', MODELLED),
	)),
	'skillengine/task/CraftingTask.java': ('061a64e7a6f63f6b', (
		('private final RecipeTemplate recipeTemplate', 'd5f59331208d5d87'),
		('private final int maxCritCount', '70be43312b4eadbb'),
		('private final int bonus', '6e463ed76ad6f834'),
		('private ItemTemplate itemTemplate', 'a22364cf235d22dc'),
		('private int critCount', '18191463d0377080'),
		('private int showBarDelay', 'bbf5cd64bae48f95'),
		('private int executionSpeed', 'fc6f17e4a4591c4b'),
		('public CraftingTask(Player requester,StaticObject responder,RecipeTemplate recipeTemplate,int skillLvlDiff,int bonus)', MODELLED),
		('@Override protected void onFailureFinish()', MODELLED),
		('@Override protected boolean onSuccessFinish()', MODELLED),
		('private boolean calculateCrit()', MODELLED),
		('@Override protected void sendInteractionUpdate()', MODELLED),
		('@Override protected void onInteractionAbort()', MODELLED),
		('@Override protected void onInteractionFinish()', MODELLED),
		('@Override protected void onInteractionStart()', MODELLED),
		('@Override protected final void analyzeInteraction()', MODELLED),
	)),
	'skillengine/task/AbstractCraftTask.java': ('aa8d7620a567205b', (
		('protected static final int fullBarValue', MODELLED),
		('protected int currentSuccessValue', '3bdfcec95395a18a'),
		('protected int currentFailureValue', 'ba30757f4587139a'),
		('protected int skillLvlDiff', '946ab12d4f643b31'),
		('protected CraftType craftType', 'be818be1c98c8bac'),
		('protected enum CraftType', MODELLED),
		('public AbstractCraftTask(Player requester,VisibleObject responder,int skillLvlDiff)', MODELLED),
		('@Override protected boolean onInteraction()', MODELLED),
		('protected abstract void analyzeInteraction()', 'c76903699943611b'),
		('protected abstract void sendInteractionUpdate()', '9e24301883421f63'),
		('protected abstract boolean onSuccessFinish()', '6612e23976a78272'),
		('protected abstract void onFailureFinish()', '7ff4a9c8a88b7e78'),
	)),
	'skillengine/task/AbstractInteractionTask.java': ('f0ea013aab4b275b', (
		('private Future<?>task', '5cc04c9bc2ecd18a'),
		('protected int interval', MODELLED),
		('protected int delay', MODELLED),
		('protected final Player requester', '366e1e487038b4f0'),
		('protected final VisibleObject responder', '2cd3a20c48145f19'),
		('public AbstractInteractionTask(Player requester,VisibleObject responder)', MODELLED),
		('protected abstract boolean onInteraction()', '5af9edd84c847aa5'),
		('protected abstract void onInteractionFinish()', '51cabef1455f29b6'),
		('protected abstract void onInteractionStart()', '722177ff11fa0279'),
		('protected abstract void onInteractionAbort()', '5219b9c69582b222'),
		('public void start()', MODELLED),
		('public void stop()', MODELLED),
		('public void abort()', MODELLED),
		('public boolean isInProgress()', MODELLED),
		('public void setInterval(int interval)', MODELLED),
	)),
	'skillengine/task/GatheringTask.java': ('38fff37da1a9d6bd', (
		('private final GatherableTemplate template', 'f7c900d827322b82'),
		('private final ActionObserver gathererObserver', '71277259ca991f86'),
		('private final Material material', 'efdf6d68a20a14a6'),
		('private int showBarDelay', 'bbf5cd64bae48f95'),
		('private int executionSpeed', 'fc6f17e4a4591c4b'),
		('public GatheringTask(Player requester,Gatherable gatherable,Material material,int skillLvlDiff)', MODELLED),
		('@Override protected void onInteractionAbort()', MODELLED),
		('@Override protected void onInteractionFinish()', MODELLED),
		('@Override protected void onInteractionStart()', MODELLED),
		('@Override protected void sendInteractionUpdate()', MODELLED),
		('@Override protected void onFailureFinish()', MODELLED),
		('@Override protected boolean onSuccessFinish()', MODELLED),
		('@Override protected final void analyzeInteraction()', MODELLED),
		('public int getGathererId()', '23c2b3bfef200063'),
		('private ActionObserver createGathererObserver()', MODELLED),
	)),
	'controllers/GatherableController.java': ('b6d3373a5a72ed18', (
		('private int gatherCount', '55c9ea6a4e1fab5d'),
		('private GatheringTask gatheringTask', '4ae44ebcabd27b30'),
		('public void startGathering(Player player)', MODELLED),
		('private boolean checkPlayerSkill(final Player player,final GatherableTemplate template)', MODELLED),
		('private List<Material>getMaterials(Player player,GatherableTemplate template)', MODELLED),
		('public void completeInteraction()', MODELLED),
		('@SuppressWarnings("lossy-conversions")public void rewardPlayer(Player player)', MODELLED),
		('@Override public void onDespawn()', '65e8f6c0c468ad50'),
		('public void cancelGathering()', 'fa8c05fb93adda50'),
		('public int getGatheringPlayerId()', '05f17d8b721d729a'),
	)),
	'model/templates/recipe/RecipeTemplate.java': ('66e0156078e77fd1', (
		('@XmlElement(name="components_data")protected List<ComponentsData>componentsData', 'b8cd2cffbe9c954c'),
		('@XmlElement(name="comboproduct")protected List<ComboProduct>comboproduct', '9c83c3a2a7dd943a'),
		('@XmlAttribute(name="max_production_count")protected Integer maxProductionCount', '1f1a4b81396d9e37'),
		('@XmlAttribute(name="craft_delay_time")protected Integer craftDelayTime', '6771f5bf52c340b0'),
		('@XmlAttribute(name="craft_delay_id")protected Integer craftDelayId', 'acc3b9457ae7d405'),
		('@XmlAttribute protected int quantity', 'd9f112f11393b60c'),
		('@XmlAttribute protected int productid', 'fa2d7efd8fdd5e5f'),
		('@XmlAttribute protected int autolearn', '861f91170c1f20fc'),
		('@XmlAttribute protected int dp', 'cace331cfa4268e9'),
		('@XmlAttribute protected int skillpoint', 'e3a947c1fda3dc1f'),
		('@XmlAttribute protected Race race', 'eb39ab54480b11ce'),
		('@XmlAttribute protected int skillid', '29ecdb2dfe051b7d'),
		('@XmlAttribute protected int itemid', '8b7c0e7e25716cac'),
		('@XmlAttribute protected int nameid', 'fb85d213bb6949cd'),
		('@XmlAttribute protected int id', '7d1c754f47b30751'),
		('public List<ComponentsData>getComponents()', MODELLED),
		('public Integer getComboProduct(int num)', MODELLED),
		('public Integer getComboProductSize()', MODELLED),
		('public Integer getQuantity()', MODELLED),
		('public Integer getProductId()', MODELLED),
		('public int getAutoLearn()', MODELLED),
		('public Integer getDp()', MODELLED),
		('public Integer getSkillpoint()', MODELLED),
		('public Race getRace()', MODELLED),
		('public Integer getSkillId()', MODELLED),
		('public Integer getItemId()', 'ec882009470eb03f'),
		('@Override public int getL10nId()', '8c478ce715ab727f'),
		('public Integer getId()', MODELLED),
		('public Integer getMaxProductionCount()', MODELLED),
		('public Integer getCraftDelayTime()', MODELLED),
		('public Integer getCraftDelayId()', MODELLED),
	)),
	'model/templates/recipe/ComponentsData.java': ('94fbf2d6980a4043', (
		('@XmlElement(name="component")protected List<Component>component', 'c1309b610f3cf68e'),
		('public List<Component>getComponent()', MODELLED),
	)),
	'model/templates/recipe/Component.java': ('2451a34d8e1c6fa2', (
		('@XmlAttribute protected int itemid', '8b7c0e7e25716cac'),
		('@XmlAttribute protected int quantity', 'd9f112f11393b60c'),
		('public int getItemId()', MODELLED),
		('public int getQuantity()', MODELLED),
	)),
	'model/templates/recipe/ComboProduct.java': ('fdb60b0f2af3b1b1', (
		('@XmlAttribute protected int itemid', '8b7c0e7e25716cac'),
		('public int getItemId()', MODELLED),
	)),
	'model/templates/gather/Material.java': ('ff567d45670fb813', (
		('@XmlAttribute protected String name', 'd538e09e15ceb93c'),
		('@XmlAttribute protected int itemid', '8b7c0e7e25716cac'),
		('@XmlAttribute protected int nameid', 'fb85d213bb6949cd'),
		('@XmlAttribute protected int rate', '39cd8668d675e6b2'),
		('public String getName()', '66e63685efbcb5a4'),
		('public int getItemId()', MODELLED),
		('@Override public int getL10nId()', '8c478ce715ab727f'),
		('public int getRate()', MODELLED),
		('@Override public int compareTo(Material o)', MODELLED),
	)),
	'model/templates/gather/Materials.java': ('3cd920e583b84bcd', (
		('protected List<Material>material', 'ec8ea9311dd14e56'),
		('public List<Material>getMaterial()', MODELLED),
	)),
	'model/templates/gather/ExMaterials.java': ('785a64faa8de26f6', (
		('protected List<Material>material', 'ec8ea9311dd14e56'),
		('public List<Material>getMaterial()', MODELLED),
	)),
	'model/templates/gather/GatherableTemplate.java': ('1326cd5ecce6a4d3', (
		('@XmlElement(required=true)protected Materials materials', 'fd6929e89c877917'),
		('@XmlElement(required=true)protected ExMaterials exmaterials', '32d56b36c720607a'),
		('@XmlAttribute protected int id', '7d1c754f47b30751'),
		('@XmlAttribute protected String name', 'd538e09e15ceb93c'),
		('@XmlAttribute protected int nameId', '877e18195ef4d831'),
		('@XmlAttribute protected String sourceType', 'cfa2d0bdaffb7795'),
		('@XmlAttribute protected int harvestCount', '60d0b17c1d9e1716'),
		('@XmlAttribute protected int skillLevel', '42fe2ddc69b7496a'),
		('@XmlAttribute protected int harvestSkill', '067ce2430b0a574a'),
		('@XmlAttribute protected int successAdj', 'ddcc5d42854a22a5'),
		('@XmlAttribute protected int failureAdj', '8e593251a80c2a47'),
		('@XmlAttribute protected int aerialAdj', '513e8d09ff46d4dd'),
		('@XmlAttribute protected int captcha', '7cf45495e18ac6ac'),
		('@XmlAttribute protected int lvlLimit', 'b598262c1381423f'),
		('@XmlAttribute protected int reqItem', '28452b2c2ce92a54'),
		('@XmlAttribute protected int reqItemNameId', '4c1cf6b70ff0b5b9'),
		('@XmlAttribute protected int checkType', 'c954996847010c4b'),
		('@XmlAttribute protected int eraseValue', '8260bbc9a4e52178'),
		('public Materials getMaterials()', MODELLED),
		('public ExMaterials getExtraMaterials()', MODELLED),
		('@Override public int getTemplateId()', MODELLED),
		('public int getAerialAdj()', 'aac38eb0db07693e'),
		('public int getFailureAdj()', '05da285389574471'),
		('public int getSuccessAdj()', 'e31458860b65655c'),
		('public int getHarvestSkill()', MODELLED),
		('public int getSkillLevel()', MODELLED),
		('public int getHarvestCount()', MODELLED),
		('public String getSourceType()', MODELLED),
		('@Override public String getName()', '79aa7f8e5c82dee4'),
		('@Override public int getL10nId()', 'ea891f7b282e5466'),
		('public int getCaptchaRate()', MODELLED),
		('public int getLevelLimit()', MODELLED),
		('public int getRequiredItemId()', MODELLED),
		('public int getRequiredItemNameId()', MODELLED),
		('public int getCheckType()', MODELLED),
		('public int getEraseValue()', MODELLED),
	)),
}

# The config fields the modelled bodies read: (Config class, field) -> the Java type the transformer must produce
CONFIG_FIELDS = {
	("CraftConfig", "MAX_CRAFT_FAILURE_CHANCE"): "int",
	("CraftConfig", "MAX_GATHER_FAILURE_CHANCE"): "int",
	("CraftConfig", "DISABLE_AETHER_AND_ESSENCE_TAPPING_CAP"): "boolean",
	("RatesConfig", "CRAFT_CRIT_CHANCES"): "float[]",
	("RatesConfig", "CRAFT_COMBO_CHANCES"): "float[]",
	("RatesConfig", "SKILL_XP_CRAFTING_RATES"): "float[]",
	("RatesConfig", "SKILL_XP_GATHERING_RATES"): "float[]",
	("RatesConfig", "XP_CRAFTING_RATES"): "float[]",
	("RatesConfig", "XP_GATHERING_RATES"): "float[]",
	("RatesConfig", "GATHERING_COUNT_RATES"): "float[]",
	("SecurityConfig", "CAPTCHA_ENABLE"): "boolean",
	("EventsConfig", "DISABLED_EVENTS"): "Set<String>",
}


def _profession_cost_row(level: str, cost: str) -> str:
	return f"case {level}: return {cost};"


@dataclass
class JavaCraftRules:
	"""The constants and tables of the modelled Java methods; `verified` lists every method and declaration checked against its template."""

	craft: dict = field(default_factory=dict)        # CraftService, CM_CRAFT, CraftingTask, AbstractCraftTask, AbstractInteractionTask literals
	gather: dict = field(default_factory=dict)       # GatheringTask, GatherableController literals
	skill: dict = field(default_factory=dict)        # PlayerSkillList.addSkillXp, PlayerSkillEntry, SkillLearnService literals
	bonus_items: dict[int, int] = field(default_factory=dict)       # CraftService.getBonusReqItem
	professions: dict[str, int] = field(default_factory=dict)       # Profession constant -> skill id
	upgrade_costs: dict[int, int] = field(default_factory=dict)     # Profession.getUpgradeCost, every profession
	upgrade_cost_crafting_only: tuple[int, int] = (0, 0)            # ... the `isCrafting() ? N : null` arm (level, cost)
	max_upgradable: tuple[int, int] = (0, 0)                        # Profession.getMaxUpgradableLevel (crafting, other)
	crafting_range: tuple[int, int] = (0, 0)                        # Profession.isCrafting
	profession_by_npc: dict[int, str] = field(default_factory=dict)  # CraftSkillUpdateService constructor
	boost_stats: dict[int, str] = field(default_factory=dict)       # StatEnum.getModifier
	rates: dict = field(default_factory=dict)                       # Rates literals
	config_fields: dict = field(default_factory=dict)               # (class, field) -> (key, defaultValue, type)
	player_bound: float = 0.0                                       # PlayerAccountData: max(front, side) of the player's BoundRadius
	object_bound: float = 0.0                                       # BoundRadius.DEFAULT: StaticObject (an ItemTemplate) and Gatherable
	verified: list[Verified] = field(default_factory=list)

	@staticmethod
	def read(java_src: Path) -> "JavaCraftRules":
		r = JavaCraftRules()
		v = r.verified
		src = Path(java_src)
		files: dict[str, JavaFile] = {}

		def load(relative: str) -> JavaFile:
			"""One JavaFile per source file: every template of a file records its span on the same object, which pin_class needs."""
			if relative not in files:
				files[relative] = JavaFile(src, relative)
			return files[relative]

		craft = load("services/craft/CraftService.java")
		finish = craft.method("finishCrafting", "public static void finishCrafting(Player player, RecipeTemplate recipetemplate, int critCount, int bonus)",
		                      FINISH_CRAFTING, v)
		start = craft.method("startCrafting", "public static void startCrafting(Player player, int recipeId, int targetObjId, int craftType, "
		                                      "Map<Integer, Long> sendMaterialsData)", START_CRAFTING, v)
		check = craft.method("checkCraft", "private static boolean checkCraft(Player player, RecipeTemplate recipeTemplate, int skillId, "
		                                   "VisibleObject target, ItemTemplate itemTemplate, int craftType, Map<Integer, Long> sendMaterialsData)",
		                     CHECK_CRAFT, v)
		rows = craft.table("getBonusReqItem", "private static int getBonusReqItem(int skillId)", r"case (\d+):return (\d+);",
		                   lambda s, i: f"case {s}: return {i};", "switch (skillId) {", "} return 0;", v)
		r.bonus_items = {int(s): int(i) for s, i in rows}
		cancel = craft.method("sendCancelCraft", "private static void sendCancelCraft(Player player, int skillId, int targetObjId, "
		                                         "ItemTemplate itemTemplate)", SEND_CANCEL_CRAFT, v)
		packet_file = load("network/aion/clientpackets/CM_CRAFT.java")
		packet = packet_file.method("runImpl", "protected void runImpl()", CM_CRAFT_RUN, v)
		task = load("skillengine/task/CraftingTask.java")
		task.method("<init>", "public CraftingTask(Player requester, StaticObject responder, RecipeTemplate recipeTemplate, int skillLvlDiff, int bonus)",
		            CRAFTING_TASK_CONSTRUCTOR, v)
		failure = task.method("onFailureFinish", "protected void onFailureFinish()", CRAFTING_ON_FAILURE, v)
		success = task.method("onSuccessFinish", "protected boolean onSuccessFinish()", CRAFTING_ON_SUCCESS, v)
		crit = task.method("calculateCrit", "private boolean calculateCrit()", CALCULATE_CRIT, v)
		task.method("sendInteractionUpdate", "protected void sendInteractionUpdate()", CRAFTING_SEND_UPDATE, v)
		abort = task.method("onInteractionAbort", "protected void onInteractionAbort()", CRAFTING_ON_ABORT, v)
		on_start = task.method("onInteractionStart", "protected void onInteractionStart()", CRAFTING_ON_START, v)
		analyze = task.method("analyzeInteraction", "protected final void analyzeInteraction()", CRAFTING_ANALYZE, v)
		task.method("onInteractionFinish", "protected void onInteractionFinish()", "", v)
		base_task = load("skillengine/task/AbstractCraftTask.java")
		full_bar = base_task.snippet("fullBarValue", "protected static final int fullBarValue = $fullBar;", v)
		progress = base_task.method("CraftType", "protected enum CraftType", CRAFT_TYPE, v)
		base_task.method("<init>", "public AbstractCraftTask(Player requester, VisibleObject responder, int skillLvlDiff)", CRAFT_TASK_CONSTRUCTOR, v)
		base_task.method("onInteraction", "protected boolean onInteraction()", CRAFT_TASK_ON_INTERACTION, v)
		interaction = load("skillengine/task/AbstractInteractionTask.java")
		default_interval = interaction.snippet("interval", "protected int interval = $interval;", v)
		default_delay = interaction.snippet("delay", "protected int delay = $delay;", v)
		interaction.method("<init>", "public AbstractInteractionTask(Player requester, VisibleObject responder)", INTERACTION_TASK_CONSTRUCTOR, v)
		interaction.method("start", "public void start()", INTERACTION_START, v)
		interaction.method("stop", "public void stop()", INTERACTION_STOP, v)
		interaction.method("abort", "public void abort()", INTERACTION_ABORT, v)
		interaction.method("isInProgress", "public boolean isInProgress()", "return task != null && !task.isCancelled();", v)
		interaction.method("setInterval", "public void setInterval(int interval)", "this.interval = interval;", v)
		for java_file in (craft, packet_file, task, base_task, interaction):
			java_file.pin_class(CLASS_PINS[java_file.relative], v)
		morph = {start["morphSkill"], check["morphSkill"], analyze["morphSkill"]}
		if len(morph) != 1 or start["bonusCraftType"] != check["bonusCraftType"]:
			raise OracleError("CraftService/CraftingTask disagree on the morph skill id or the bonus craft type")
		r.craft = {**finish, **start, **check, **analyze, **packet, **crit, **full_bar, **default_interval, **default_delay,
		           "progress": {"NORMAL": progress["normal"], "CRIT_BLUE": progress["blue"], "CRIT_PURPLE": progress["purple"]},
		           "actions": {"init": on_start["actionInit"], "proc": on_start["actionProc"], "start": on_start["actionStart"],
		                       "success": success["actionSuccess"], "failure": failure["actionFailure"], "cancel": cancel["actionCancel"],
		                       "abort": abort["actionAbort"]},
		           "animations": {"init": on_start["animInit"], "start": on_start["animStart"], "success": success["animSuccess"],
		                          "failure": failure["animFailure"], "cancel": cancel["animCancel"], "abort": abort["animAbort"]}}
		for name, value in (("stationRange", check["stationRange"]), ("packetRange", packet["packetRange"])):
			if not isinstance(value, int):
				raise OracleError(f"CraftService/CM_CRAFT: {name} {value} is not an int literal, which the range model assumes")

		gtask = load("skillengine/task/GatheringTask.java")
		g_init = gtask.method("<init>", "public GatheringTask(Player requester, Gatherable gatherable, Material material, int skillLvlDiff)",
		                      GATHERING_TASK_CONSTRUCTOR, v)
		g_abort = gtask.method("onInteractionAbort", "protected void onInteractionAbort()", GATHERING_ON_ABORT, v)
		gtask.method("onInteractionFinish", "protected void onInteractionFinish()", GATHERING_ON_FINISH, v)
		g_start = gtask.method("onInteractionStart", "protected void onInteractionStart()", GATHERING_ON_START, v)
		gtask.method("sendInteractionUpdate", "protected void sendInteractionUpdate()", GATHERING_SEND_UPDATE, v)
		g_failure = gtask.method("onFailureFinish", "protected void onFailureFinish()", GATHERING_ON_FAILURE, v)
		g_success = gtask.method("onSuccessFinish", "protected boolean onSuccessFinish()", GATHERING_ON_SUCCESS, v)
		g_analyze = gtask.method("analyzeInteraction", "protected final void analyzeInteraction()", GATHERING_ANALYZE, v)
		gtask.method("createGathererObserver", "private ActionObserver createGathererObserver()", GATHERER_OBSERVER, v)
		controller = load("controllers/GatherableController.java")
		g_controller = controller.method("startGathering", "public void startGathering(Player player)", START_GATHERING, v)
		g_skill = controller.method("checkPlayerSkill", "private boolean checkPlayerSkill(final Player player, final GatherableTemplate template)",
		                            CHECK_PLAYER_SKILL, v)
		g_materials = controller.method("getMaterials", "private List<Material> getMaterials(Player player, GatherableTemplate template)", GET_MATERIALS,
		                                v)
		controller.method("completeInteraction", "public void completeInteraction()", COMPLETE_INTERACTION, v)
		g_reward = controller.method("rewardPlayer", "public void rewardPlayer(Player player)", REWARD_PLAYER, v)
		for java_file in (gtask, controller):
			java_file.pin_class(CLASS_PINS[java_file.relative], v)
		r.gather = {**g_init, **g_analyze, **g_controller, **g_materials, **{f"reward{k[0].upper()}{k[1:]}": x for k, x in g_reward.items()},
		            "gatherCount": g_success["gatherCount"], "humanSkill": g_skill["humanSkill"],
		            "actions": {"init": g_start["actionInit"], "start": g_start["actionStart"], "success": g_success["actionSuccess"],
		                        "failurePre": g_failure["actionFailPre"], "failure": g_failure["actionFailure"], "abort": g_abort["actionAbort"],
		                        "occupied": g_controller["actionOccupied"]},
		            "animations": {"init": g_start["animInit"], "start": g_start["animStart"], "success": g_success["animSuccess"],
		                           "failure": g_failure["animFailure"], "abort": g_abort["animAbort"]}}
		if not isinstance(r.gather["gatherRange"], int):
			raise OracleError("GatherableController.startGathering: the gather range is not an int literal, which the range model assumes")

		skill_list = load("model/skill/PlayerSkillList.java")
		xp = skill_list.method("addSkillXp", "public synchronized boolean addSkillXp(Player player, int skillId, int xpReward, int objSkillLvl)",
		                       ADD_SKILL_XP, v)
		entry = load("model/skill/PlayerSkillEntry.java")
		tapping = entry.method("isTappingSkill", "public boolean isTappingSkill()", "return skillId >= $tapMin && skillId <= $tapMax;", v)
		crafting = entry.method("isCraftingSkill", "public boolean isCraftingSkill()", "return skillId >= $craftMin && skillId <= $craftMax && "
		                                                                               "!isMorphSkill();", v)
		morph_entry = entry.method("isMorphSkill", "public boolean isMorphSkill()", "return skillId == $morphSkill;", v)
		profession_entry = entry.method("isProfessionSkill", "public boolean isProfessionSkill()", "return skillId >= $profMin && skillId < $profMax;",
		                                v)
		if morph_entry["morphSkill"] not in morph:
			raise OracleError("PlayerSkillEntry.isMorphSkill names another skill than CraftService")
		learn_service = load("services/SkillLearnService.java")
		learned = learn_service.method("onLearnSkill", "public static void onLearnSkill(Player player, int skillId, int skillLevel, boolean isNew)",
		                               ON_LEARN_SKILL, v)
		learn_packet = learn_service.method("sendPacket", "private static void sendPacket(Player player, PlayerSkillEntry skill, boolean isNew)",
		                                    SKILL_LEARN_SEND_PACKET, v)
		load("services/RecipeService.java").method("autoLearnRecipes", "public static void autoLearnRecipes(Player player, int skillId, "
		                                                                         "int skillLvl)", AUTO_LEARN_RECIPES, v)
		skill_template = load("skillengine/model/SkillTemplate.java")
		skill_template.snippet("activationAttribute", '@XmlAttribute(name = "activation", required = true) private ActivationAttribute '
		                                              "activationAttribute;", v)
		skill_template.method("isPassive", "public boolean isPassive()", "return activationAttribute == ActivationAttribute.PASSIVE;", v)
		r.skill = {**xp, **tapping, **crafting, **morph_entry, **profession_entry,
		           "craftCapSkills": sorted(xp[f"c{i}"] for i in range(1, 8)), "capLevels": sorted(xp[f"l{i}"] for i in range(1, 8)),
		           "levelUpAnimation": sorted(learned[f"anim{i}"] for i in range(1, 8)), "levelUpAnimationCraftingOnly": learned["anim1"],
		           "nearbyQuestLevels": sorted((learned["quest1"], learned["quest2"])),
		           "skillListMessage": {"tapping": learn_packet["tapUp"], "other": learn_packet["craftUp"]}}
		if r.skill["humanSkill"] != r.gather["humanSkill"]:
			raise OracleError("PlayerSkillList.addSkillXp and GatherableController.checkPlayerSkill name different human gathering skills")

		recipe_data = load("dataholders/RecipeData.java")
		recipe_data.method("afterUnmarshal", "void afterUnmarshal(Unmarshaller u, Object parent)", RECIPE_DATA_AFTER_UNMARSHAL, v)
		recipe_data.method("getAutolearnRecipes", "public List<RecipeTemplate> getAutolearnRecipes(Race race, int skillId, int maxLevel)",
		                   GET_AUTOLEARN_RECIPES, v)
		recipe_template = load("model/templates/recipe/RecipeTemplate.java")
		recipe_template.method("getComboProduct", "public Integer getComboProduct(int num)", GET_COMBO_PRODUCT, v)
		recipe_template.method("getComboProductSize", "public Integer getComboProductSize()", GET_COMBO_PRODUCT_SIZE, v)
		recipe_template.method("getComponents", "public List<ComponentsData> getComponents()", GET_COMPONENTS, v)
		for head, field_expr in (("public Integer getQuantity()", "quantity"), ("public Integer getProductId()", "productid"),
		                         ("public int getAutoLearn()", "autolearn"), ("public Integer getDp()", "dp"),
		                         ("public Integer getSkillpoint()", "skillpoint"), ("public Race getRace()", "race"),
		                         ("public Integer getSkillId()", "skillid"), ("public Integer getId()", "id"),
		                         ("public Integer getMaxProductionCount()", "maxProductionCount"), ("public Integer getCraftDelayTime()", "craftDelayTime"),
		                         ("public Integer getCraftDelayId()", "craftDelayId")):
			recipe_template.getter(head, field_expr, v)
		components_data = load("model/templates/recipe/ComponentsData.java")
		components_data.getter("public List<Component> getComponent()", "component", v)
		component = load("model/templates/recipe/Component.java")
		component.getter("public int getItemId()", "itemid", v)
		component.getter("public int getQuantity()", "quantity", v)
		combo_product = load("model/templates/recipe/ComboProduct.java")
		combo_product.getter("public int getItemId()", "itemid", v)
		load("dataholders/GatherableData.java").method("afterUnmarshal", "void afterUnmarshal(Unmarshaller u, Object parent)",
		                                                        GATHERABLE_DATA_AFTER_UNMARSHAL, v)
		material = load("model/templates/gather/Material.java")
		material.method("compareTo", "public int compareTo(Material o)", "return o.rate - rate;", v)
		material.getter("public int getItemId()", "itemid", v)
		material.getter("public int getRate()", "rate", v)
		materials = load("model/templates/gather/Materials.java")
		materials.method("getMaterial", "public List<Material> getMaterial()", GET_MATERIAL, v)
		ex_materials = load("model/templates/gather/ExMaterials.java")
		ex_materials.method("getMaterial", "public List<Material> getMaterial()", GET_MATERIAL, v)
		gatherable_template = load("model/templates/gather/GatherableTemplate.java")
		for head, field_expr in (("public Materials getMaterials()", "materials"), ("public ExMaterials getExtraMaterials()", "exmaterials"),
		                         ("public int getTemplateId()", "id"), ("public int getHarvestSkill()", "harvestSkill"),
		                         ("public int getSkillLevel()", "skillLevel"), ("public int getHarvestCount()", "harvestCount"),
		                         ("public String getSourceType()", "sourceType"), ("public int getCaptchaRate()", "captcha"),
		                         ("public int getLevelLimit()", "lvlLimit"), ("public int getRequiredItemId()", "reqItem"),
		                         ("public int getRequiredItemNameId()", "reqItemNameId"), ("public int getCheckType()", "checkType"),
		                         ("public int getEraseValue()", "eraseValue")):
			gatherable_template.getter(head, field_expr, v)
		for java_file in (recipe_template, components_data, component, combo_product, material, materials, ex_materials, gatherable_template):
			java_file.pin_class(CLASS_PINS[java_file.relative], v)

		profession = load("model/craft/Profession.java")
		for name, args in enum_constants(profession.path, "Profession"):
			if args is None or not re.fullmatch(r"\s*\d+\s*", args):
				raise OracleError(f"Profession.{name}: expected one int constructor argument")
			r.professions[name] = int(args)
		body, _, _, _ = profession.body("public Integer getUpgradeCost(int skillLevel)")
		special = re.search(r"case (\d+):return isCrafting\(\)\?(\d+):null;", body)
		if special is None:
			raise OracleError("Profession.getUpgradeCost: no `case N: return isCrafting() ? COST : null;` arm")
		rows = profession.table("getUpgradeCost", "public Integer getUpgradeCost(int skillLevel)", r"case (\d+):return (\d+);", _profession_cost_row,
		                        "switch (skillLevel) {", f"case {special.group(1)}: return isCrafting() ? {special.group(2)} : null; }} return null;", v)
		r.upgrade_costs = {int(level): int(cost) for level, cost in rows}
		r.upgrade_cost_crafting_only = (int(special.group(1)), int(special.group(2)))
		upgradable = profession.method("getMaxUpgradableLevel", "public int getMaxUpgradableLevel()", "return isCrafting() ? $crafting : $other;", v)
		r.max_upgradable = (upgradable["crafting"], upgradable["other"])
		is_crafting = profession.method("isCrafting", "public boolean isCrafting()", "return skillId >= $min && skillId <= $max;", v)
		r.crafting_range = (is_crafting["min"], is_crafting["max"])
		update = load("services/craft/CraftSkillUpdateService.java")
		npc_rows = update.table("<init>", "private CraftSkillUpdateService()", r"professionByNpc\.put\((\d+),Profession\.([A-Z_]+)\);",
		                        lambda npc, prof: f"professionByNpc.put({npc}, Profession.{prof});", "",
		                        'log.info("CraftSkillUpdateService: Initialized.");', v)
		r.profession_by_npc = {}
		for npc, prof in npc_rows:
			if prof not in r.professions:
				raise OracleError(f"CraftSkillUpdateService: npc {npc} teaches Profession.{prof}, which is not a Profession constant")
			r.profession_by_npc[int(npc)] = prof  # HashMap.put: a later put of the same npc replaces the earlier one
		learn = update.method("learnSkill", "public void learnSkill(Player player, Npc npc)", LEARN_SKILL, v)
		r.skill["masterMinLevel"] = learn["minLevel"]

		stat_enum = load("model/stats/container/StatEnum.java")
		stat_rows = stat_enum.table("getModifier", "public static StatEnum getModifier(int skillId)", r"case ([\d,]+)->([A-Z_]+);",
		                            lambda ids, stat: f"case {', '.join(ids.split(','))} -> {stat};", "return switch (skillId) {",
		                            "default -> null; };", v)
		for ids, stat in stat_rows:
			for skill_id in ids.split(","):
				r.boost_stats[int(skill_id)] = stat

		rates = load("model/gameobjects/player/Rates.java")
		for name, template in RATES_CONSTANTS.items():
			rates.snippet(name, template, v)
		rates.method("get", "public static float get(Player player, float[] membershipRates)", RATES_GET, v)
		r.rates = rates.method("calcXpRate", "private static float calcXpRate(Player player, float[] membershipRates, StatEnum boostRate)",
		                       RATES_CALC_XP_RATE, v)
		rates.method("calcResult", "public int calcResult(Player player, int value)", RATES_CALC_RESULT_INT, v)
		common = load("model/gameobjects/player/PlayerCommonData.java")
		r.rates.update(common.method("addExp", "public void addExp(long value, Rates rates, String name)", ADD_EXP, v))
		common.method("addExp", "public void addExp(long value, Rates rates)", "addExp(value, rates, null);", v)
		common.method("setDp", "public void setDp(int dp)", SET_DP, v)
		common.method("addDp", "public void addDp(int dp)", "setDp(this.dp + dp);", v)
		common.method("getBoundRadius", "public BoundRadius getBoundRadius()", "return boundRadius;", v)

		bound = load("model/account/PlayerAccountData.java").snippet(
			"<init>", "playerCommonData.setBoundingRadius(new BoundRadius($f:front, $f:side, appearance.getBoundHeight()));", v)
		r.player_bound = max(bound["front"], bound["side"])  # BoundRadius.getMaxOfFrontAndSide
		load("model/templates/VisibleObjectTemplate.java").method("getBoundRadius", "public BoundRadius getBoundRadius()",
		                                                                  "return BoundRadius.DEFAULT;", v)
		radius = load("model/templates/BoundRadius.java")
		default = radius.snippet("DEFAULT", "public static final BoundRadius DEFAULT = new BoundRadius($f:front, $f:side, $f:upper);", v)
		radius.method("getMaxOfFrontAndSide", "public float getMaxOfFrontAndSide()", "return Math.max(front, side);", v)
		r.object_bound = max(default["front"], default["side"])
		for relative in ("model/templates/item/ItemTemplate.java", "model/templates/gather/GatherableTemplate.java"):
			load(relative).absent("getBoundRadius() override", r"\bBoundRadius getBoundRadius\(\)", v)
		craft_update = load("network/aion/serverpackets/SM_CRAFT_UPDATE.java").method(
			"<init>", "public SM_CRAFT_UPDATE(int skillId, ItemTemplate item, int success, int failure, int action, int executionSpeed, int delay)",
			SM_CRAFT_UPDATE_CONSTRUCTOR, v)
		if craft_update["morphSkill"] not in morph:
			raise OracleError("SM_CRAFT_UPDATE names another morph skill than CraftService")
		r.craft["packetMorphDelay"] = craft_update["morphDelay"]
		load("network/aion/serverpackets/SM_GATHER_UPDATE.java").method(
			"<init>", "public SM_GATHER_UPDATE(GatherableTemplate template, Material material, int success, int failure, int action, "
			          "int executionSpeed, int delay)", SM_GATHER_UPDATE_CONSTRUCTOR, v)

		# the range checks (checkCraft, CM_CRAFT, startGathering) and the packet audiences
		position = load("utils/PositionUtil.java")
		position.method("isInRange", "public static boolean isInRange(VisibleObject object, VisibleObject object2, float range)",
		                "return isInRange(object, object2, range, true);", v)
		position.method("isInRange", "public static boolean isInRange(VisibleObject object, VisibleObject object2, float range, boolean centerToCenter)",
		                IS_IN_RANGE_4, v)
		position.method("isInRange", "public static boolean isInRange(float x1, float y1, float z1, float x2, float y2, float z2, float range)",
		                IS_IN_RANGE_7, v)
		send = load("utils/PacketSendUtility.java")
		send.method("sendPacket", "public static void sendPacket(Player player, AionServerPacket packet)",
		            "if (player.isOnline()) player.getClientConnection().sendPacket(packet);", v)
		send.method("broadcastPacket", "public static void broadcastPacket(Player player, AionServerPacket packet, boolean toSelf)", BROADCAST_TO_SELF, v)
		send.method("broadcastPacket", "public static void broadcastPacket(VisibleObject object, AionServerPacket packet)",
		            "object.getKnownList().forEachPlayer(player -> sendPacket(player, packet));", v)
		# checkCraft's material check and consumption: Player.getInventory is the cube PlayerStorage, decreaseByItemId takes what is there
		player = load("model/gameobjects/player/Player.java")
		player.snippet("inventory", "private final Storage inventory;", v)
		player.snippet("<init> inventory", "this.inventory = new PlayerStorage(this, StorageType.CUBE);", v)
		player.getter("public Storage getInventory()", "inventory", v)
		load("model/items/storage/PlayerStorage.java").method("decreaseByItemId", "public boolean decreaseByItemId(int itemId, long count)",
		                                                               "return decreaseByItemId(itemId, count, actor);", v)
		storage = load("model/items/storage/Storage.java")
		storage.method("decreaseByItemId", "boolean decreaseByItemId(int itemId, long count, Player actor)",
		               "return decreaseByItemId(itemId, count, null, actor);", v)
		storage.method("decreaseByItemId", "boolean decreaseByItemId(int itemId, long count, QuestStatus questStatus, Player actor)",
		               STORAGE_DECREASE_BY_ITEM_ID, v)
		storage.method("decreaseItemCount", "long decreaseItemCount(Item item, long count, ItemUpdateType updateType, QuestStatus questStatus, "
		                                    "Player actor)", STORAGE_DECREASE_ITEM_COUNT, v)
		storage.method("getItemCountByItemId", "public long getItemCountByItemId(int itemId)", STORAGE_ITEM_COUNT, v)
		load("model/items/storage/ItemStorage.java").method("getItemsById", "public List<Item> getItemsById(int itemId)",
		                                                             ITEM_STORAGE_ITEMS_BY_ID, v)
		load("model/gameobjects/Item.java").method("decreaseItemCount", "public long decreaseItemCount(long count)", ITEM_DECREASE_ITEM_COUNT,
		                                                    v)
		# setDp's updateStatsAndSpeedVisually: SM_STATS_INFO to self (the HP/MP and speed packets of onStatsChange only when those changed)
		stats = load("model/stats/container/PlayerGameStats.java")
		stats.method("updateStatsAndSpeedVisually", "public void updateStatsAndSpeedVisually()", "onStatsChange(null);", v)
		stats.method("onStatsChange", "protected void onStatsChange(Effect effect)",
		             "super.onStatsChange(effect); updateStatsVisually(); checkSpeedStats();", v)
		stats.method("updateStatsVisually", "public void updateStatsVisually()", "updateStatInfo();", v)
		stats.method("updateStatInfo", "public void updateStatInfo()", "PacketSendUtility.sendPacket(owner, new SM_STATS_INFO(owner));", v)

		for (cls, field_name), java_type in CONFIG_FIELDS.items():
			key, default_value, declared = load(f"configs/main/{cls}.java").config_property(field_name, v)
			if declared != java_type:
				raise OracleError(f"{cls}.{field_name} is declared {declared}, the oracle models {java_type}")
			r.config_fields[(cls, field_name)] = (key, default_value, java_type)
		return r
