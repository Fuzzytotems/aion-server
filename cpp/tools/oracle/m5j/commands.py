"""m5j-commands: the chat commands as the Java server builds and answers them (m5j-plan.md H-01, §10.2).

Java rules, each with the method the value comes from:
- the access levels: config/administration/commands.properties (CommandsConfig.ACCESS_LEVELS), one `alias = level` per line; an alias the
  file does not name has no level entry (ChatCommand.getLevel then falls back to CommandsConfig's default, which this oracle does not model);
- a command, keyed by its alias with prefix as ChatProcessor.registerCommand keys it (//addskill and the console's addskill are two): the
  `super(...)` of its class's own constructor (data/handlers/{admin,player,console}commands/X.java): alias, description and
  syntax info, each a string literal, a text block (JLS 3.10.6) or a `+` concatenation of them. A part that is not a literal (a constant, a
  ChatUtil call, a method) makes the command `unresolved`, with the expression named; the prefixes are AdminCommand.PREFIX "//",
  PlayerCommand.PREFIX "." and ConsoleCommand.PREFIX "" (AdminCommand.java:16, PlayerCommand.java:11, ConsoleCommand.java:15);
- the syntax info: ChatCommand.parseSyntaxInfo (ChatCommand.java), with ChatUtil.color(text, Color.WHITE) =
  "[color:<text>;1 1 1]" (ChatUtil.java:33-34 with DecimalFormat(".##") of 255 / 255f);
- `help`: ChatCommand.run's first arm (ChatCommand.java:61-64), "Command: " + color(alias with prefix) + "\\n\\t" + (description or
  "No description available.") + "\\n" + syntax info, sent by sendInfo as the parts of ChatUtil.split (ChatUtil.java:346-400, the
  SM_MESSAGE.MESSAGE_SIZE_LIMIT of 1022);
- the access message: AdminCommand.validateAccess (AdminCommand.java:40-41), "<You need access level N or higher to use //alias>";
- ChatType ids (model/ChatType.java), ChatUtil.l10n(id) as UTF-16 code units ("$", id << 1 | 1 low and high, ChatUtil.java:96-102), the
  whisper level (custom.properties' gameserver.chat.whisper.level) and the non-Daeva level cap (PlayerCommonData: maxLevel 10 unless Daeva,
  the level shown maxLevel - 1).
Texts are Java strings: lengths and split indices count UTF-16 code units.
"""

from __future__ import annotations

import re
from pathlib import Path

from staticdata_oracle import OracleError

PREFIXES = {"admincommands": "//", "playercommands": ".", "consolecommands": ""}
BASES = {"admincommands": "AdminCommand", "playercommands": "PlayerCommand", "consolecommands": "ConsoleCommand"}
MESSAGE_SIZE_LIMIT = 1022  # SM_MESSAGE.java:33


def _read(path: Path) -> str:
	try:
		return path.read_text(encoding="utf-8")
	except OSError as e:
		raise OracleError(f"{path}: {e}") from e


# ---- Java literals ----------------------------------------------------------------------------------------------------------------------

_ESCAPES = {"n": "\n", "t": "\t", "r": "\r", "b": "\b", "f": "\f", "s": " ", "0": "\0", "\"": "\"", "'": "'", "\\": "\\"}


def unescape(body: str) -> str:
	"""the value of a Java string literal's body (the escapes of JLS 3.10.7 and \\uXXXX)"""
	out = []
	i = 0
	while i < len(body):
		c = body[i]
		if c != "\\":
			out.append(c)
			i += 1
			continue
		nxt = body[i + 1]
		if nxt == "u":
			j = i + 1
			while body[j] == "u":
				j += 1
			out.append(chr(int(body[j:j + 4], 16)))
			i = j + 4
		elif nxt in _ESCAPES:
			out.append(_ESCAPES[nxt])
			i += 2
		elif nxt == "\n":  # a text block's line continuation
			i += 2
		else:
			raise OracleError(f"unknown escape \\{nxt}")
	return "".join(out)


def text_block(raw: str) -> str:
	"""the value of a Java text block (JLS 3.10.6): content after the opening line, incidental indentation stripped (the closing delimiter's
	line counts), trailing spaces stripped, line terminators \\n, then the escapes"""
	body = raw[3:-3]
	body = body[body.index("\n") + 1:].replace("\r\n", "\n")
	lines = body.split("\n")
	significant = [ln for ln in lines[:-1] if ln.strip()] + [lines[-1]]
	indent = min((len(ln) - len(ln.lstrip(" \t")) for ln in significant), default=0)
	stripped = [ln[indent:].rstrip(" \t") for ln in lines]
	if lines[-1].strip() == "":
		value = "".join(ln + "\n" for ln in stripped[:-1])
	else:
		value = "\n".join(stripped)
	return unescape(value)


_TOKEN = re.compile(r'"""[ \t\f]*\r?\n[\s\S]*?(?<!\\)"""|"(?:\\.|[^"\\\n])*"|//[^\n]*|/\*[\s\S]*?\*/|\s+|[(),+]|[^\s(),+"]+')


def _arguments(source: str, start: int) -> tuple[list[list[str]], int]:
	"""the arguments of the call whose '(' is at `start`: per argument its top-level tokens (comments and spaces dropped)"""
	args: list[list[str]] = [[]]
	depth = 0
	pos = start
	while pos < len(source):
		m = _TOKEN.match(source, pos)
		if m is None:
			raise OracleError(f"cannot read the arguments at {source[pos:pos + 30]!r}")
		tok = m.group(0)
		pos = m.end()
		if tok.isspace() or tok.startswith("//") or tok.startswith("/*"):
			continue
		if tok == "(":
			depth += 1
			if depth == 1:
				continue
		elif tok == ")":
			depth -= 1
			if depth == 0:
				return args, pos
		elif tok == "," and depth == 1:
			args.append([])
			continue
		args[-1].append(tok)
	raise OracleError("unterminated argument list")


def _value(tokens: list[str]) -> tuple[str | None, str | None]:
	"""(value, None) of a literal or a `+` concatenation of literals, else (None, the expression)"""
	parts = []
	expect_operand = True
	for tok in tokens:
		if expect_operand:
			if tok.startswith('"""'):
				parts.append(text_block(tok))
			elif tok.startswith('"'):
				parts.append(unescape(tok[1:-1]))
			else:
				return None, " ".join(tokens)
			expect_operand = False
		else:
			if tok != "+":
				return None, " ".join(tokens)
			expect_operand = True
	if expect_operand:
		return None, " ".join(tokens)
	return "".join(parts), None


def read_command(path: Path, package: str) -> dict:
	"""a command class: alias, description and raw syntax info from its own constructor's super(...), or what could not be read"""
	source = _read(path)
	name = path.stem
	ctor = re.search(r"\b(?:public|protected|private)?\s*" + re.escape(name) + r"\s*\(\s*\)\s*\{", source)
	if ctor is None:
		return {"class": name, "unresolved": "no no-argument constructor"}
	call = re.compile(r"\b(super|this)\s*\(").search(source, ctor.end())
	if call is None:
		return {"class": name, "unresolved": "no super(...) in the constructor"}
	args, _ = _arguments(source, call.end() - 1)
	values = []
	for arg in args:
		value, expression = _value(arg)
		if value is None:
			return {"class": name, "unresolved": f"argument {len(values) + 1} is {expression}"}
		values.append(value)
	if call.group(1) == "this" or len(values) not in (1, 2, 3):
		return {"class": name, "unresolved": f"{call.group(1)}({len(values)} arguments)"}
	alias = values[0]
	description = values[1] if len(values) > 1 else ""
	syntax = values[2] if len(values) > 2 else ""
	return {"class": name, "package": package, "alias": alias, "prefix": PREFIXES[package], "description": description, "syntax": syntax}


# ---- ChatUtil and ChatCommand ------------------------------------------------------------------------------------------------------------

def color_white(text: str) -> str:
	"""ChatUtil.color(text, Color.WHITE): DecimalFormat(".##") of 255 / 255f is "1" """
	return f"[color:{text};1 1 1]"


_SYNTAX_WORD = re.compile(r"([^<>\[\]| ]+)")


def parse_syntax_info(alias_with_prefix: str, syntax: str) -> str:
	"""ChatCommand.parseSyntaxInfo"""
	sb = ["Syntax:"]
	if syntax.strip() == "":
		sb.append("\n\tNo syntax info available.")
		return "".join(sb)
	contains_square_brackets = False
	# String.split("\n") drops trailing empty strings
	lines = syntax.split("\n")
	while lines and lines[-1] == "":
		lines.pop()
	for info in lines:
		split = info.split(" - ", 1)
		if len(split) == 2:
			if not contains_square_brackets and "[" in split[0]:
				contains_square_brackets = True
			sb.append("\n\t" + color_white(alias_with_prefix) + " ")
			colored = _SYNTAX_WORD.sub(lambda m: color_white(m.group(1)), split[0]).replace("[[color:f;", "[[color:f​;").strip()
			sb.append(colored + " - " + split[1])
		else:
			sb.append("\n" + info)
	if contains_square_brackets:
		sb.append("\nNote: Parameters enclosed in square brackets are optional.")
	return "".join(sb)


def _utf16(text: str) -> list[int]:
	data = text.encode("utf-16-le")
	return [int.from_bytes(data[i:i + 2], "little") for i in range(0, len(data), 2)]


def _find_split_index(units: list[int], start: int, end: int) -> int:
	"""ChatUtil.findSplitIndex over UTF-16 code units"""
	estimated = 0
	last_new_line = -1
	last_space = -1
	i = start
	while i < end:
		length_to_add = 1
		c = units[i]
		if c == 0x0A:
			last_new_line = i
		elif c == 0x20:
			last_space = i
		elif c == ord("$"):
			if i + 2 < end and (units[i + 1] & 1) == 1:
				i += 2
				length_to_add += 15
		elif c == ord("["):
			if i + 3 < end and chr(units[i + 1]).islower():
				link_end = next((k for k in range(i + 2, min(i + 40, end)) if units[k] == ord("]")), -1)
				if link_end != -1:
					colon = next((k for k in range(i + 2, link_end) if units[k] == ord(":")), -1)
					if colon != -1:
						i += link_end - i
						length_to_add += 30
		estimated += length_to_add
		if estimated >= MESSAGE_SIZE_LIMIT:
			if i == start:
				break
			if last_new_line != -1:
				return last_new_line
			if last_space != -1:
				return last_space
			return i
		i += 1
	return end


def split(message: str) -> list[str]:
	"""ChatUtil.split (UTF-16 code units, as Java counts)"""
	units = _utf16(message)
	if len(units) <= MESSAGE_SIZE_LIMIT // 2:
		return [message]
	parts = []
	start, length = 0, len(units)
	while start < length:
		index = _find_split_index(units, start, length)
		parts.append(_from_units(units[start:index]))
		start = index
		if start < length and units[start] in (0x20, 0x0A):
			start += 1
	return parts


def _from_units(units: list[int]) -> str:
	return b"".join(u.to_bytes(2, "little") for u in units).decode("utf-16-le", errors="surrogatepass")


def help_text(command: dict) -> str:
	"""ChatCommand.run's help arm"""
	alias_with_prefix = command["prefix"] + command["alias"]
	description = command["description"] or "No description available."
	return "Command: " + color_white(alias_with_prefix) + "\n\t" + description + "\n" + parse_syntax_info(alias_with_prefix, command["syntax"])


def l10n_units(l10n_id: int) -> list[int]:
	"""ChatUtil.l10n(id) as UTF-16 code units"""
	value = (l10n_id << 1 | 1) & 0xFFFFFFFF
	return [ord("$"), value & 0xFFFF, (value >> 16) & 0xFFFF]


# ---- configuration ----------------------------------------------------------------------------------------------------------------------

def read_properties(path: Path) -> dict[str, str]:
	"""a .properties file's `key = value` lines (no continuations are used by the files read here)"""
	values = {}
	for line in _read(path).splitlines():
		line = line.strip()
		if not line or line.startswith(("#", "!")):
			continue
		key, sep, value = line.partition("=")
		if sep:
			values[key.strip()] = value.strip()
	return values


def chat_types(java_src: Path) -> dict[str, int]:
	"""model/ChatType.java's constants and ids"""
	return {m.group(1): int(m.group(2)) for m in re.finditer(r"^\s*([A-Z_]+)\((\d+)", _read(java_src / "com/aionemu/gameserver/model/ChatType.java"), re.M)}


# ---- the report -------------------------------------------------------------------------------------------------------------------------

def commands_report(game_server: Path, aliases: list[str] | None, l10n_ids: list[int]) -> dict:
	"""game_server: the Java module directory (game-server/)"""
	handlers = game_server / "data" / "handlers"
	# the alias = level lines; the file's one other key, gameserver.commands.handler_directories, is no command
	levels = {k: int(v) for k, v in read_properties(game_server / "config" / "administration" / "commands.properties").items()
	          if not k.startswith("gameserver.")}
	commands = {}
	unresolved = {}
	for package in PREFIXES:
		for path in sorted((handlers / package).glob("*.java")):
			command = read_command(path, package)
			if "unresolved" in command:
				unresolved[f"{package}/{command['class']}"] = command["unresolved"]
				continue
			alias = command["alias"]
			with_prefix = command["prefix"] + alias
			if aliases and with_prefix not in aliases:
				continue
			command["aliasWithPrefix"] = with_prefix
			command["javaFile"] = f"data/handlers/{package}/{path.name}"
			command["syntaxInfo"] = parse_syntax_info(with_prefix, command["syntax"])
			command["help"] = split(help_text(command))
			if alias in levels:
				command["level"] = levels[alias]
				command["accessMessage"] = f"<You need access level {levels[alias]} or higher to use {with_prefix}>"
			commands[with_prefix] = command  # ChatProcessor.registerCommand keys by the alias with its prefix (admin and console share aliases)
	if aliases:
		missing = [a for a in aliases if a not in commands]
		if missing:
			raise OracleError(f"no readable command for {missing} (unresolved: {unresolved})")
	custom = read_properties(game_server / "config" / "main" / "custom.properties")
	return {
		"commands": commands,
		"unresolved": unresolved,
		"levels": levels,
		"chatTypes": chat_types(game_server / "src"),
		"l10n": {str(i): l10n_units(i) for i in l10n_ids},
		"whisperLevel": int(custom.get("gameserver.chat.whisper.level", "10")),
		"nonDaevaLevelCap": non_daeva_level_cap(game_server / "src"),
	}


def non_daeva_level_cap(java_src: Path) -> int:
	"""PlayerCommonData.setExp: `... ? pxt.getMaxLevel() : N` for a non-Daeva, and the level shown is `maxLevel - 1`"""
	source = _read(java_src / "com/aionemu/gameserver/model/gameobjects/player/PlayerCommonData.java")
	m = re.search(r"int maxLevel = isDaeva .*\? pxt\.getMaxLevel\(\) : (\d+);", source)
	if m is None or "level = Math.min(pxt.getLevelForExp(this.exp), maxLevel - 1);" not in source:
		raise OracleError("PlayerCommonData.setExp's non-Daeva cap was not found")
	return int(m.group(1)) - 1
