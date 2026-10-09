"""m5h-legion: the constants the M5h legion gate asserts (m5h-plan.md G-01, §10.2), read from the Java sources.

- SM_SYSTEM_MESSAGE and SM_QUESTION_WINDOW ids by name (the factory method's `new SM_SYSTEM_MESSAGE(<id>` and the `static final int` field), as
  m5g-team reads them;
- the legion enums' constructor data: LegionRank's client ids (LegionRank.java), LegionHistoryAction's ids and types (LegionHistoryAction.java)
  and LegionPermissionsMask's bits (LegionPermissionsMask.java);
- the emblem chunk size of LegionService.sendEmblemData (`int maxSize = 7993;`) and the announcement limit of changeAnnouncement
  (`message.length() > 256`);
- ChatType's ids (ChatType.java), for the legion chat of CM_CHAT_MESSAGE_PUBLIC.
"""

from __future__ import annotations

import re
from pathlib import Path

from staticdata_oracle import OracleError

from m5a.creation import enum_constants
from m5a.data import java_int


def _read(source: Path) -> str:
	try:
		return source.read_text(encoding="utf-8")
	except OSError as e:
		raise OracleError(f"{source}: {e}") from e


def message_ids(base: Path, names: list[str]) -> dict:
	system = _read(base / "network" / "aion" / "serverpackets" / "SM_SYSTEM_MESSAGE.java")
	ids = {}
	for name in names:
		found = re.search(rf"public\s+static\s+SM_SYSTEM_MESSAGE\s+{name}\([^)]*\)\s*\{{\s*return\s+new\s+SM_SYSTEM_MESSAGE\((\d+)", system)
		if found is None:
			raise OracleError(f"SM_SYSTEM_MESSAGE.{name}: not found")
		ids[name] = int(found.group(1))
	return ids


def question_ids(base: Path, names: list[str]) -> dict:
	question = _read(base / "network" / "aion" / "serverpackets" / "SM_QUESTION_WINDOW.java")
	ids = {}
	for name in names:
		found = re.search(rf"public\s+static\s+final\s+int\s+{name}\s*=\s*(\d+)\s*;", question)
		if found is None:
			raise OracleError(f"SM_QUESTION_WINDOW.{name}: not found")
		ids[name] = int(found.group(1))
	return ids


def legion_report(java_src: Path, messages: list[str], questions: list[str]) -> dict:
	base = Path(java_src) / "com" / "aionemu" / "gameserver"
	legion_dir = base / "model" / "team" / "legion"
	ranks = {name: java_int(args, f"LegionRank.{name}") for name, args in enum_constants(legion_dir / "LegionRank.java", "LegionRank")}
	actions = {}
	for name, args in enum_constants(legion_dir / "LegionHistoryAction.java", "LegionHistoryAction"):
		action_id, action_type = [a.strip() for a in args.split(",")]
		actions[name] = {"id": java_int(action_id, f"LegionHistoryAction.{name}"), "type": action_type.split(".")[-1]}
	masks = {name: int(args.strip(), 0) for name, args in enum_constants(legion_dir / "LegionPermissionsMask.java", "LegionPermissionsMask")}
	# ChatType(int id) or ChatType(int id, boolean sysMsg): the first argument is the id
	chat_types = {name: int(args.split(",")[0].strip(), 0) for name, args in enum_constants(base / "model" / "ChatType.java", "ChatType")}
	service = _read(base / "services" / "LegionService.java")
	chunk = re.search(r"int\s+maxSize\s*=\s*(\d+)\s*;", service)
	limit = re.search(r"message\.length\(\)\s*>\s*(\d+)", service)
	if chunk is None or limit is None:
		raise OracleError("LegionService.java: the emblem chunk size or the announcement limit was not found")
	return {
		"messages": message_ids(base, messages),
		"questions": question_ids(base, questions),
		"ranks": ranks,
		"historyActions": actions,
		"permissionMasks": masks,
		"chatTypes": chat_types,
		"emblemChunkSize": int(chunk.group(1)),
		"announcementLimit": int(limit.group(1)),
	}
