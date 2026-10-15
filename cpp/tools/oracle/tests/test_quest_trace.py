"""Phase-6 golden quest traces (questtrace/, phase6-inventory.md §7.6 item 3): the path extractor on a synthetic handler (every construct it
models and the ones it refuses), the mutation standard (a changed page id or var in the Java changes the expected case), cases of the real
slice derived by hand from the Java sources, the committed expected/quest documents against a regeneration, the command line, and the
oracle's independence from the quest generator.

Expected values are derived by hand from the handler sources and AbstractQuestHandler/QuestService/QuestState (Java), and repeated per case.
Everything reads the Java tree (DialogAction.java and the enums), so the tests are skipped without it.
"""

import contextlib
import io
import json
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from questtrace import extract
from questtrace.extract import Dom, Unsupported
from staticdata_oracle import run as runner

import oracle

HAVE_JAVA_TREE = (extract.JAVA_SRC / "com" / "aionemu" / "gameserver").is_dir() and extract.QUEST_DIR.is_dir()

SYNTHETIC = """package quest.test;

import static com.aionemu.gameserver.model.DialogAction.*;

import com.aionemu.gameserver.model.gameobjects.Item;
import com.aionemu.gameserver.model.gameobjects.player.Player;
import com.aionemu.gameserver.questEngine.handlers.AbstractQuestHandler;
import com.aionemu.gameserver.questEngine.handlers.HandlerResult;
import com.aionemu.gameserver.questEngine.model.QuestEnv;
import com.aionemu.gameserver.questEngine.model.QuestState;
import com.aionemu.gameserver.questEngine.model.QuestStatus;
import com.aionemu.gameserver.services.QuestService;

public class _99100Trace extends AbstractQuestHandler {

	private static final int ITEM = 182200001;
	private final int[] mobs = { 210001, 210002 };

	public _99100Trace() {
		super(99100);
	}

	@Override
	public void register() {
		qe.registerQuestNpc(203001).addOnQuestStart(questId);
		qe.registerQuestNpc(203001).addOnTalkEvent(questId);
		for (int mob : mobs)
			qe.registerQuestNpc(mob).addOnKillEvent(questId);
		qe.registerOnLevelChanged(questId);
	}

	@Override
	public boolean onDialogEvent(QuestEnv env) {
		Player player = env.getPlayer();
		QuestState qs = player.getQuestStateList().getQuestState(questId);
		int targetId = env.getTargetId();
		if (qs == null || qs.isStartable()) {
			if (targetId == 203001) {
				switch (env.getDialogActionId()) {
					case QUEST_SELECT:
						return sendQuestDialog(env, 1011);
					case QUEST_ACCEPT_1:
					case QUEST_ACCEPT_SIMPLE:
						if (QuestService.startQuest(env))
							return sendQuestDialog(env, 1003);
						return closeDialogWindow(env);
					default:
						return sendQuestStartDialog(env);
				}
			}
			return false;
		}
		int var = qs.getQuestVarById(0);
		if (qs.getStatus() == QuestStatus.START && targetId == 203001) {
			if (var >= 2 && var <= 4 && player.getInventory().getItemCountByItemId(ITEM) >= 3) {
				qs.setQuestVarById(0, var + 10);
				if (qs.getQuestVarById(0) == 13)
					qs.setStatus(QuestStatus.REWARD);
				updateQuestStatus(env);
				return step(env, var * 2);
			}
			return false;
		}
		return qs.getStatus() == QuestStatus.REWARD ? sendQuestEndDialog(env) : false;
	}

	private boolean step(QuestEnv env, int page) {
		return sendQuestDialog(env, page + 1000);
	}

	@Override
	public boolean onKillEvent(QuestEnv env) {
		QuestState qs = env.getPlayer().getQuestStateList().getQuestState(questId);
		if (qs.getStatus() != QuestStatus.START)
			return false;
		return true;
	}

	@Override
	public boolean onAttackEvent(QuestEnv env) {
		QuestState qs = env.getPlayer().getQuestStateList().getQuestState(questId);
		if (qs == null)
			return false;
		changeQuestStep(env, 0, 1);
		return qs.getQuestVarById(0) == 1;
	}

	@Override
	public HandlerResult onItemUseEvent(QuestEnv env, Item item) {
		if (item.getItemId() != ITEM)
			return HandlerResult.UNKNOWN;
		return HandlerResult.fromBoolean(sendQuestDialog(env, 4));
	}

	@Override
	public void onLevelChangedEvent(Player player) {
		int[] quests = { 99098, 99099 };
		defaultOnLevelChangedEvent(player, quests);
	}
}
"""

_TABLES = []


def tables():
	if not _TABLES:
		_TABLES.append(extract.Tables())
	return _TABLES[0]


def trace_text(java_text):
	tmp = Path(tempfile.mkdtemp(prefix="qtrace"))
	try:
		f = tmp / "_99100Trace.java"
		f.write_text(java_text, encoding="utf-8")
		return extract.trace_file(tables(), f, "test/_99100Trace.java")
	finally:
		shutil.rmtree(tmp, ignore_errors=True)


def matching(doc, hook, **given):
	"""the cases of a hook whose `given` holds every key of `given` with that value"""
	return [c for c in doc["cases"] if c["hook"] == hook and all(c["given"].get(k) == v for k, v in given.items())]


def one(doc, hook, test=lambda c: True, **given):
	cs = [c for c in matching(doc, hook, **given) if test(c)]
	if len(cs) != 1:
		raise AssertionError(f"{len(cs)} cases of {hook} match {given}: {[c['id'] for c in cs]}")
	return cs[0]


def calls(case):
	return [(e["call"], e["args"]) for e in case["effects"]]


def movies(case):
	return [e["args"][0] for e in case["effects"] if e["call"] == "playQuestMovie"]


NPC = {"kind": "npc", "npcId": 203001}

HANDLER = """package quest.test;

import static com.aionemu.gameserver.model.DialogAction.*;

import com.aionemu.gameserver.model.DialogPage;
import com.aionemu.gameserver.model.gameobjects.Item;
import com.aionemu.gameserver.model.gameobjects.Npc;
import com.aionemu.gameserver.questEngine.handlers.AbstractQuestHandler;
import com.aionemu.gameserver.questEngine.handlers.HandlerResult;
import com.aionemu.gameserver.questEngine.model.QuestEnv;
import com.aionemu.gameserver.questEngine.model.QuestState;
import com.aionemu.gameserver.questEngine.model.QuestStatus;
import com.aionemu.gameserver.services.QuestService;
import com.aionemu.gameserver.world.WorldMapType;

public class _99100Trace extends AbstractQuestHandler {
%s
	public _99100Trace() {
		super(99100);
	}

	@Override
	public void register() {
		qe.registerQuestNpc(203001).addOnTalkEvent(questId);
	}
%s}
"""
QS = "\t\tQuestState qs = env.getPlayer().getQuestStateList().getQuestState(questId);\n"


def hook(body, sig="boolean onKillEvent(QuestEnv env)"):
	return f"\n\t@Override\n\tpublic {sig} {{\n{body}\t}}\n"


def handler(*members, fields=""):
	"""quest 99100 with these hooks and methods (register(): 203001's talk event)"""
	return HANDLER % (fields, "".join(members))


class DomainTest(unittest.TestCase):
	"""the input domains: equalities, exclusions and ranges, and the value picked from them"""

	def test_ranges_exclusions_and_picks(self):
		d = Dom(lo=0, hi=63).restrict(">=", 2).restrict("<=", 4)
		self.assertEqual((d.lo, d.hi, d.pick()), (2, 4, 2))
		self.assertEqual(d.restrict("!=", 2).pick(), 3)
		self.assertFalse(d.restrict("==", 5).sat())
		self.assertFalse(Dom(lo=0).restrict("<", 0).sat())
		self.assertEqual(Dom().restrict("!=", 0).restrict("!=", 1).pick(), 2)
		self.assertEqual(Dom().restrict("<", -3).pick(), -4)                   # below zero: the largest allowed value
		self.assertEqual(Dom().pick(prefer=(7, 8)), 7)

	def test_allowed_values_keep_their_order(self):
		d = Dom((None, "START", "REWARD"))
		self.assertEqual(d.pick(), None)
		self.assertEqual(d.restrict("!=", None).pick(), "START")
		self.assertEqual(d.restrict("==", "REWARD").pick(), "REWARD")
		self.assertFalse(d.restrict("==", "LOCKED").sat())
		with self.assertRaises(Unsupported):
			d.restrict("<", "START")


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class SyntheticTraceTest(unittest.TestCase):
	@classmethod
	def setUpClass(cls):
		cls.doc = trace_text(SYNTHETIC)

	def test_register_trace_unrolls_the_constant_array(self):
		self.assertEqual(self.doc["register"], [
			{"npc": 203001, "event": "addOnQuestStart", "args": [99100]},
			{"npc": 203001, "event": "addOnTalkEvent", "args": [99100]},
			{"npc": 210001, "event": "addOnKillEvent", "args": [99100]},
			{"npc": 210002, "event": "addOnKillEvent", "args": [99100]},
			{"call": "registerOnLevelChanged", "args": [99100]}])
		self.assertEqual(self.doc["questId"], 99100)

	def test_every_label_of_a_group_is_its_own_case(self):
		c = one(self.doc, "onDialogEvent", target=NPC, questState=None, dialogAction={"name": "QUEST_SELECT", "id": 31})
		self.assertEqual((calls(c), c["returns"]), ([("sendQuestDialog", [1011])], {"resultOf": 0}))
		for name in ("QUEST_ACCEPT_1", "QUEST_ACCEPT_SIMPLE"):
			with self.subTest(label=name):
				action = {"name": name, "id": tables().dialog.actions[name]}
				acc = matching(self.doc, "onDialogEvent", target=NPC, questState=None, dialogAction=action)
				self.assertEqual(len(acc), 2)                                   # QuestService.startQuest true and false

	def test_a_helper_result_the_guard_branches_on_is_an_assumption(self):
		dialog = {"name": "QUEST_ACCEPT_1", "id": tables().dialog.actions["QUEST_ACCEPT_1"]}
		ok = one(self.doc, "onDialogEvent", lambda c: c["assume"] == [{"effect": 0, "returns": True}], target=NPC, questState=None,
		         dialogAction=dialog)
		self.assertEqual(calls(ok), [("QuestService.startQuest", []), ("sendQuestDialog", [1003])])
		self.assertEqual(ok["returns"], {"resultOf": 1})
		self.assertIn("QuestService.startQuest(env) -> true @44", ok["guards"])
		no = one(self.doc, "onDialogEvent", lambda c: c["assume"] == [{"effect": 0, "returns": False}], target=NPC, questState=None,
		         dialogAction=dialog)
		self.assertEqual(calls(no), [("QuestService.startQuest", []), ("closeDialogWindow", [])])

	def test_default_label_picks_an_action_no_label_names(self):
		c = one(self.doc, "onDialogEvent", lambda c: c["effects"] and c["effects"][0]["call"] == "sendQuestStartDialog", target=NPC,
		        questState=None)
		first = next(iter(tables().dialog.actions.items()))                    # DialogAction.java's first constant
		self.assertEqual(c["given"]["dialogAction"], {"name": first[0], "id": first[1]})
		self.assertIn("env.getDialogActionId() not in [QUEST_SELECT, QUEST_ACCEPT_1, QUEST_ACCEPT_SIMPLE] -> true @39", c["guards"])

	def test_is_startable_reads_can_repeat_only_for_a_complete_quest(self):
		# QuestState.isStartable (QuestState.java:117-119) is status == COMPLETE && canRepeat(): canRepeat is not read for another status
		c = one(self.doc, "onDialogEvent", target=NPC, questState={"status": "COMPLETE", "canRepeat": True},
		        dialogAction={"name": "QUEST_SELECT", "id": 31})
		self.assertIn("qs.isStartable() [canRepeat] -> true @37", c["guards"])
		others = [c for c in self.doc["cases"] if (c["given"].get("questState") or {}).get("status") not in (None, "COMPLETE")]
		self.assertTrue(others)
		for s in others:
			self.assertNotIn("canRepeat", s["given"]["questState"], s["id"])
		ended = one(self.doc, "onDialogEvent", target={"kind": "none"}, questState={"status": "COMPLETE", "canRepeat": False, "vars": {"0": 0}})
		self.assertEqual((ended["effects"], ended["returns"]), ([], False))

	def test_target_id_is_zero_without_a_target(self):
		# QuestEnv.getTargetId (QuestEnv.java:94-96): 0 when there is no visible object
		c = one(self.doc, "onDialogEvent", target={"kind": "none"}, questState=None)
		self.assertEqual((c["effects"], c["returns"]), ([], False))
		self.assertEqual(c["guards"], ["qs == null -> true @37"])               # 0 == 203001 is decided without a guard

	def test_a_write_is_read_back_and_arithmetic_is_evaluated(self):
		qs = {"status": "START", "vars": {"0": 3}}
		c = one(self.doc, "onDialogEvent", target=NPC, questState=qs, inventory={"182200001": 3})
		self.assertEqual(calls(c), [("qs.setQuestVarById", [0, 13]), ("qs.setStatus", ["REWARD"]), ("updateQuestStatus", []),
		                            ("sendQuestDialog", [1006])])      # step(env, 3 * 2): page + 1000, inlined
		self.assertEqual(c["returns"], {"resultOf": 3})
		c = one(self.doc, "onDialogEvent", target=NPC, questState={"status": "START", "vars": {"0": 2}}, inventory={"182200001": 3})
		self.assertEqual(calls(c), [("qs.setQuestVarById", [0, 12]), ("updateQuestStatus", []), ("sendQuestDialog", [1004])])
		self.assertEqual(c["ranges"], {"questState.vars.0": [2, 4]})

	def test_inventory_guards_pick_the_boundary(self):
		c = one(self.doc, "onDialogEvent", target=NPC, questState={"status": "START", "vars": {"0": 2}}, inventory={"182200001": 0})
		self.assertEqual((c["effects"], c["returns"]), ([], False))
		self.assertEqual(c["ranges"], {"questState.vars.0": [2, 4], "inventory.182200001": [0, 2]})

	def test_a_null_dereference_is_a_case(self):
		c = one(self.doc, "onKillEvent", questState=None)
		self.assertEqual((c["effects"], c["throws"]), ([], "NullPointerException"))
		self.assertNotIn("returns", c)

	def test_a_read_after_an_unmodelled_helper_refuses_the_hook(self):
		hook = next(h for h in self.doc["hooks"] if h["hook"] == "onAttackEvent")
		self.assertEqual(hook["unsupported"], "reads var/99100/0 after changeQuestStep, which may have written it")
		self.assertFalse(matching(self.doc, "onAttackEvent"))

	def test_handler_result_and_hook_arguments(self):
		c = one(self.doc, "onItemUseEvent", item={"itemId": 182200001})
		self.assertEqual((calls(c), c["returns"]), ([("sendQuestDialog", [4])], {"fromBoolean": {"resultOf": 0}}))
		c = one(self.doc, "onItemUseEvent", item={"itemId": 0})
		self.assertEqual((c["effects"], c["returns"]), ([], "UNKNOWN"))

	def test_varargs_are_flattened_and_env_or_player_arguments_left_out(self):
		c = one(self.doc, "onLevelChangedEvent")
		self.assertEqual((calls(c), c["returns"]), ([("defaultOnLevelChangedEvent", [99098, 99099])], None))

	def test_hooks_are_listed_with_their_case_counts(self):
		counts = {h["hook"]: h.get("cases") for h in self.doc["hooks"]}
		self.assertEqual(set(counts), {"onDialogEvent", "onKillEvent", "onAttackEvent", "onItemUseEvent", "onLevelChangedEvent"})
		self.assertEqual(counts["onKillEvent"], 3)                             # null, START, another status
		self.assertEqual(sum(1 for c in self.doc["cases"] if c["hook"] == "onDialogEvent"), counts["onDialogEvent"])
		self.assertEqual([c["id"] for c in self.doc["cases"] if c["hook"] == "onKillEvent"], ["onKillEvent#1", "onKillEvent#2", "onKillEvent#3"])


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class MutationStandardTest(unittest.TestCase):
	"""the oracle is written from Java: a flipped page id or var in the handler changes the expected case (the port that differs fails it)"""

	def changed_cases(self, old, new):
		self.assertEqual(SYNTHETIC.count(old), 1)
		a, b = trace_text(SYNTHETIC), trace_text(SYNTHETIC.replace(old, new))
		self.assertEqual(len(a["cases"]), len(b["cases"]))
		return [x["id"] for x, y in zip(a["cases"], b["cases"]) if x != y]

	def test_a_page_id(self):
		# QUEST_SELECT at 203001 without a QuestState and with a repeatable COMPLETE one
		self.assertEqual(len(self.changed_cases("sendQuestDialog(env, 1011)", "sendQuestDialog(env, 1012)")), 2)

	def test_a_var_write(self):
		self.assertTrue(self.changed_cases("qs.setQuestVarById(0, var + 10)", "qs.setQuestVarById(0, var + 11)"))

	def test_a_guard(self):
		self.assertTrue(self.changed_cases("var >= 2 && var <= 4", "var >= 2 && var <= 5"))


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class JavaSemanticsTest(unittest.TestCase):
	"""one construct per handler; every expected value is worked out by hand from the Java rule named in the test"""

	def cases(self, doc, name="onKillEvent"):
		return [c for c in doc["cases"] if c["hook"] == name]

	def single(self, doc, name="onKillEvent"):
		(c,) = self.cases(doc, name)
		return c

	def refusal(self, doc, name):
		return next(h for h in doc["hooks"] if h["hook"] == name).get("unsupported")

	def test_a_switch_falls_through_until_a_break(self):
		# JLS 14.11.3: the statements of the matching group and of every later group run until a break
		doc = trace_text(handler(hook(
			"\t\tswitch (env.getDialogActionId()) {\n\t\t\tcase QUEST_SELECT:\n\t\t\t\tplayQuestMovie(env, 1);\n\t\t\tcase SETPRO1:\n"
			"\t\t\t\tplayQuestMovie(env, 2);\n\t\t\t\tbreak;\n\t\t\tcase SETPRO2:\n\t\t\t\tplayQuestMovie(env, 3);\n\t\t}\n\t\treturn false;\n",
			"boolean onDialogEvent(QuestEnv env)")))
		got = {c["given"]["dialogAction"]["name"]: movies(c) for c in self.cases(doc, "onDialogEvent")}
		self.assertEqual(len(got), 4)
		self.assertEqual({k: v for k, v in got.items() if k in ("QUEST_SELECT", "SETPRO1", "SETPRO2")},
		                 {"QUEST_SELECT": [1, 2], "SETPRO1": [2], "SETPRO2": [3]})
		self.assertEqual([v for k, v in got.items() if k not in ("QUEST_SELECT", "SETPRO1", "SETPRO2")], [[]])    # the default

	def test_break_leaves_a_for_each_and_continue_skips_the_rest(self):
		doc = trace_text(handler(hook(
			"\t\tfor (int page : PAGES) {\n\t\t\tif (page == 2)\n\t\t\t\tbreak;\n\t\t\tplayQuestMovie(env, page);\n\t\t}\n"
			"\t\tfor (int page : PAGES) {\n\t\t\tif (page == 2)\n\t\t\t\tcontinue;\n\t\t\tplayQuestMovie(env, page + 10);\n\t\t}\n"
			"\t\treturn true;\n"), fields="\n\tprivate static final int[] PAGES = { 1, 2, 3 };\n"))
		c = self.single(doc)
		self.assertEqual((movies(c), c["returns"]), ([1, 11, 13], True))

	def test_int_arithmetic_wraps_and_divides_toward_zero(self):
		# JLS 15.18.2 (overflow wraps at 32 bits), 15.17.2 (/ rounds toward zero), 15.17.3 (% takes the dividend's sign)
		doc = trace_text(handler(hook(
			"\t\tplayQuestMovie(env, BIG + 2);\n\t\tplayQuestMovie(env, BIG * 2);\n\t\tplayQuestMovie(env, -7 / 2);\n"
			"\t\tplayQuestMovie(env, -7 % 2);\n\t\tplayQuestMovie(env, 7 / -2);\n\t\tplayQuestMovie(env, 7 % -2);\n\t\treturn true;\n"),
			fields="\n\tprivate static final int BIG = 2147483647;\n"))
		self.assertEqual(movies(self.single(doc)), [-2147483647, -2, -3, -1, -3, 1])

	def test_casts_narrow_like_java_and_floating_arithmetic_is_refused(self):
		# JLS 5.1.3: truncation toward zero, saturation at the int range, then the low bits of the narrower type
		doc = trace_text(handler(hook(
			"\t\tplayQuestMovie(env, (int) 2.7);\n\t\tplayQuestMovie(env, (int) -2.7);\n\t\tplayQuestMovie(env, (int) 1e20);\n"
			"\t\tplayQuestMovie(env, (int) 4294967297L);\n\t\tplayQuestMovie(env, (byte) 200);\n\t\tplayQuestMovie(env, (short) -32769);\n"
			"\t\treturn true;\n"),
			hook("\t\tplayQuestMovie(env, (int) (2.5 * 2));\n\t\treturn true;\n", "boolean onAttackEvent(QuestEnv env)"),
			hook("\t\tplayQuestMovie(env, (int) item.getItemId());\n\t\treturn HandlerResult.SUCCESS;\n",
			     "HandlerResult onItemUseEvent(QuestEnv env, Item item)"),
			hook(QS + "\t\treturn (byte) qs.getQuestVarById(0) == 1;\n", "boolean onDieEvent(QuestEnv env)")))
		self.assertEqual(movies(self.single(doc)), [2, -2, 2147483647, 1, -56, 32767])
		self.assertEqual(self.refusal(doc, "onAttackEvent"), "floating-point arithmetic *")
		c = self.single(doc, "onItemUseEvent")                                 # an int input cast to int is itself
		self.assertEqual((movies(c), c["given"]), ([0], {"item": {"itemId": 0}}))
		self.assertEqual(self.refusal(doc, "onDieEvent"), "cast to byte of qs.getQuestVarById(0)")    # an input narrowed: not modelled

	def test_set_quest_var_fills_the_six_6_bit_slots(self):
		# QuestVars.setVar (QuestVars.java:52-58): slot i is bits 6i..6i+5, and the shift is arithmetic (slot 5 of 0x80000000 is 62)
		doc = trace_text(handler(hook(
			QS + "\t\tif (qs == null)\n\t\t\treturn false;\n\t\tqs.setQuestVar(33845571);\n\t\tfor (int slot : SLOTS)\n"
			"\t\t\tplayQuestMovie(env, qs.getQuestVarById(slot));\n\t\tqs.setQuestVar(-2147483648);\n\t\tfor (int slot : SLOTS)\n"
			"\t\t\tplayQuestMovie(env, qs.getQuestVarById(slot));\n\t\treturn true;\n"),
			fields="\n\tprivate static final int[] SLOTS = { 0, 1, 2, 3, 4, 5 };\n"))
		c = next(c for c in self.cases(doc) if c["returns"] is True)
		self.assertEqual(movies(c), [3, 5, 7, 1, 2, 0, 0, 0, 0, 0, 0, 62])     # 33845571 = 3 + 5*64 + 7*64^2 + 1*64^3 + 2*64^4

	def test_state_a_helper_may_write_is_not_read_back(self):
		# QuestService.startQuest keeps the vars of an old QuestState and zeroes a new one's; giveQuestItem changes the inventory
		doc = trace_text(handler(
			hook(QS + "\t\tif (qs == null)\n\t\t\treturn false;\n\t\tqs.setQuestVarById(0, 5);\n\t\tif (QuestService.startQuest(env))\n"
			     "\t\t\treturn sendQuestDialog(env, qs.getQuestVarById(0));\n\t\treturn false;\n", "boolean onDialogEvent(QuestEnv env)"),
			hook("\t\tgiveQuestItem(env, 182200001, 1);\n\t\treturn env.getPlayer().getInventory().getItemCountByItemId(182200001) > 0;\n")))
		self.assertEqual(self.refusal(doc, "onDialogEvent"), "reads var/99100/0 after QuestService.startQuest, which may have written it")
		self.assertEqual(self.refusal(doc, "onKillEvent"), "reads inv/182200001 after giveQuestItem, which may have written it")

	def test_helper_results_java_cannot_return_are_dead_paths(self):
		# P6-Q (2026-09-29): giveQuestItem of a non-zero item and count returns true on both of its paths (AbstractQuestHandler.java:626-641),
		# so its "-> false" branch is dead; an item id 0 can return false. collectItemCheck(env, true) returns false without a QuestState
		# (QuestService.java:557-561)
		doc = trace_text(handler(
			hook("\t\tif (giveQuestItem(env, 182200001, 1))\n\t\t\treturn sendQuestDialog(env, 10);\n\t\treturn false;\n",
			     "boolean onDialogEvent(QuestEnv env)"),
			hook("\t\tif (giveQuestItem(env, 0, 1))\n\t\t\treturn sendQuestDialog(env, 10);\n\t\treturn false;\n"),
			hook(QS + "\t\tif (QuestService.collectItemCheck(env, true))\n\t\t\treturn qs.getStatus() == QuestStatus.START;\n"
			     "\t\treturn false;\n", "boolean onEnterWorldEvent(QuestEnv env)")))
		dialog = self.cases(doc, "onDialogEvent")
		self.assertEqual([(c.get("assume"), c["returns"]) for c in dialog], [([{"effect": 0, "returns": True}], {"resultOf": 1})])
		kill = self.cases(doc, "onKillEvent")
		self.assertEqual(sorted(c["assume"][0]["returns"] for c in kill), [False, True])
		# collectItemCheck -> true with no QuestState (then qs.getStatus() throws) is dead; with one it is kept, and "-> false" never reads qs
		world = [(c.get("assume"), c["given"].get("questState", "absent")) for c in self.cases(doc, "onEnterWorldEvent")]
		self.assertEqual(world, [([{"effect": 0, "returns": True}], {"status": "START"}), ([{"effect": 0, "returns": False}], "absent")])

	def test_a_status_switch_takes_the_enum_labels(self):
		doc = trace_text(handler(hook(
			QS + "\t\tif (qs == null)\n\t\t\treturn false;\n\t\tswitch (qs.getStatus()) {\n\t\t\tcase START:\n"
			"\t\t\t\treturn sendQuestDialog(env, 1011);\n\t\t\tcase REWARD:\n\t\t\t\treturn sendQuestEndDialog(env);\n\t\t}\n\t\treturn false;\n",
			"boolean onDialogEvent(QuestEnv env)")))
		got = {(c["given"]["questState"] or {}).get("status"): (calls(c), c["returns"]) for c in self.cases(doc, "onDialogEvent")}
		self.assertEqual(got, {None: ([], False), "START": ([("sendQuestDialog", [1011])], {"resultOf": 0}),
		                       "REWARD": ([("sendQuestEndDialog", [])], {"resultOf": 0}), "COMPLETE": ([], False)})

	def test_increments_and_compound_assignments(self):
		# JLS 15.14.2/15.15.1: x++ yields the old value, ++x the new one; 15.26.2: x op= y is x = (int) (x op y)
		doc = trace_text(handler(
			hook("\t\tint x = 5;\n\t\tplayQuestMovie(env, ++x);\n\t\tplayQuestMovie(env, x++);\n\t\tplayQuestMovie(env, x);\n"
			     "\t\tplayQuestMovie(env, --x);\n\t\tplayQuestMovie(env, x--);\n\t\tplayQuestMovie(env, x);\n\t\treturn true;\n"),
			hook("\t\tint x = 5;\n\t\tplayQuestMovie(env, x += 3);\n\t\tplayQuestMovie(env, x *= 4);\n\t\tplayQuestMovie(env, x -= 2);\n"
			     "\t\tplayQuestMovie(env, x /= 4);\n\t\tplayQuestMovie(env, x %= 4);\n\t\tx += 10;\n\t\tplayQuestMovie(env, x);\n\t\treturn true;\n",
			     "boolean onAttackEvent(QuestEnv env)")))
		self.assertEqual(movies(self.single(doc)), [6, 6, 7, 6, 6, 5])
		self.assertEqual(movies(self.single(doc, "onAttackEvent")), [8, 32, 30, 7, 3, 13])

	def test_an_inlined_method_leaves_the_callers_locals_alone(self):
		doc = trace_text(handler(
			hook("\t\tint page = 5;\n\t\tint other = 9;\n\t\tbump(env, 7);\n\t\tplayQuestMovie(env, page);\n\t\tplayQuestMovie(env, other);\n"
			     "\t\treturn true;\n"),
			"\n\tprivate void bump(QuestEnv env, int page) {\n\t\tpage = page + 1;\n\t\tplayQuestMovie(env, page);\n\t}\n"))
		self.assertEqual(movies(self.single(doc)), [8, 5, 9])

	def test_handler_result_from_a_constant(self):
		# HandlerResult.fromBoolean (HandlerResult.java:11-17): true SUCCESS, false FAILED, null UNKNOWN
		doc = trace_text(handler(hook(
			"\t\tif (item.getItemId() == 1)\n\t\t\treturn HandlerResult.fromBoolean(true);\n\t\tif (item.getItemId() == 2)\n"
			"\t\t\treturn HandlerResult.fromBoolean(false);\n\t\treturn HandlerResult.fromBoolean(null);\n",
			"HandlerResult onItemUseEvent(QuestEnv env, Item item)")))
		got = {c["given"]["item"]["itemId"]: c["returns"] for c in self.cases(doc, "onItemUseEvent")}
		self.assertEqual(got, {1: "SUCCESS", 2: "FAILED", 0: "UNKNOWN"})

	def test_reward_pages_and_the_reward_group_read_back(self):
		# DialogPage.getRewardPageByIndex (DialogPage.java:85-110): group 0 SELECT_QUEST_REWARD_WINDOW1 (5), 2 WINDOW3 (7), 4 WINDOW5 (45)
		doc = trace_text(handler(hook(
			QS + "\t\tif (qs == null)\n\t\t\treturn false;\n"
			"\t\tsendQuestDialog(env, DialogPage.getRewardPageByIndex(qs.getRewardGroup()).id());\n"
			"\t\tsendQuestDialog(env, DialogPage.getRewardPageByIndex(2).id());\n\t\tqs.setRewardGroup(4);\n"
			"\t\tsendQuestDialog(env, DialogPage.getRewardPageByIndex(qs.getRewardGroup()).id());\n"
			"\t\treturn sendQuestDialog(env, qs.getRewardGroup() + 100);\n", "boolean onDialogEvent(QuestEnv env)")))
		c = next(c for c in self.cases(doc, "onDialogEvent") if c["effects"])
		self.assertEqual(c["given"]["questState"], {"status": "START", "rewardGroup": 0})
		self.assertEqual(calls(c), [("sendQuestDialog", [5]), ("sendQuestDialog", [7]), ("qs.setRewardGroup", [4]), ("sendQuestDialog", [45]),
		                            ("sendQuestDialog", [104])])

	def test_a_null_target_throws_after_the_arguments(self):
		# JLS 15.12.4: the arguments are evaluated before the NullPointerException of a null target
		doc = trace_text(handler(
			hook("\t\tNpc npc = (Npc) env.getVisibleObject();\n\t\tplayQuestMovie(env, 3);\n"
			     "\t\treturn npc.getNpcId() == 203001 || npc.getNpcId() == 200000;\n"),
			hook(QS + "\t\tqs.setQuestVarById(0, page(env));\n\t\treturn true;\n", "boolean onAttackEvent(QuestEnv env)"),
			"\n\tprivate int page(QuestEnv env) {\n\t\tplayQuestMovie(env, 5);\n\t\treturn 3;\n\t}\n"))
		kill = {c["given"]["target"].get("npcId"): (movies(c), c.get("returns", c.get("throws"))) for c in self.cases(doc)}
		# another npc: the first of OTHER_NPCS no guard names (200000 is named, so 200001)
		self.assertEqual(kill, {None: ([3], "NullPointerException"), 203001: ([3], True), 200000: ([3], True), 200001: ([3], False)})
		attack = {c["given"]["questState"] is None: (calls(c), c.get("returns", c.get("throws"))) for c in self.cases(doc, "onAttackEvent")}
		self.assertEqual(attack, {True: ([("playQuestMovie", [5])], "NullPointerException"),
		                          False: ([("playQuestMovie", [5]), ("qs.setQuestVarById", [0, 3])], True)})

	def test_env_quest_id_follows_set_quest_id(self):
		# QuestEnv.getQuestId is the handler's quest until env.setQuestId; QuestService.startQuest (QuestService.java:400-444) starts
		# env.getQuestId(), and sendQuestStartDialog calls it (AbstractQuestHandler.java:364-399)
		doc = trace_text(handler(
			hook("\t\tplayQuestMovie(env, env.getQuestId());\n\t\tenv.setQuestId(99098);\n\t\tplayQuestMovie(env, env.getQuestId());\n"
			     "\t\treturn true;\n"),
			hook("\t\tQuestState own = env.getPlayer().getQuestStateList().getQuestState(questId);\n\t\tenv.setQuestId(99098);\n"
			     "\t\tif (QuestService.startQuest(env)) {\n"
			     "\t\t\tQuestState other = env.getPlayer().getQuestStateList().getQuestState(99098);\n"
			     "\t\t\tsendQuestDialog(env, other.getStatus() == QuestStatus.START ? 1 : 2);\n\t\t\treturn own == null;\n\t\t}\n"
			     "\t\treturn false;\n", "boolean onDialogEvent(QuestEnv env)"),
			hook("\t\tenv.setQuestId(99098);\n\t\tsendQuestStartDialog(env);\n"
			     "\t\treturn env.getPlayer().getQuestStateList().getQuestState(99098).getQuestVarById(0) == 0;\n",
			     "boolean onAttackEvent(QuestEnv env)"),
			hook("\t\tenv.setQuestId(item.getItemId());\n\t\tQuestService.startQuest(env);\n\t\treturn HandlerResult.SUCCESS;\n",
			     "HandlerResult onItemUseEvent(QuestEnv env, Item item)"),
			hook("\t\tsendQuestEndDialog(env);\n\t\tQuestService.startQuest(env);\n\t\treturn true;\n", "boolean onEnterWorldEvent(QuestEnv env)"),
			hook("\t\tenv.setQuestId(99098);\n\t\tQuestState other = env.getPlayer().getQuestStateList().getQuestState(99098);\n"
			     "\t\tif (other == null)\n\t\t\treturn false;\n\t\tsendQuestStartDialog(env);\n\t\treturn other.getQuestId() == 99098;\n",
			     "boolean onLogOutEvent(QuestEnv env)")))
		self.assertEqual(calls(self.single(doc)), [("playQuestMovie", [99100]), ("env.setQuestId", [99098]), ("playQuestMovie", [99098])])
		started = one(doc, "onDialogEvent", lambda c: c["assume"] == [{"effect": 1, "returns": True}])
		self.assertEqual(calls(started), [("env.setQuestId", [99098]), ("QuestService.startQuest", []), ("sendQuestDialog", [1])])
		self.assertEqual((started["given"], started["returns"]), ({"questState": None}, True))     # the handler's own quest is untouched
		self.assertEqual(len(self.cases(doc, "onDialogEvent")), 2)
		self.assertEqual(self.refusal(doc, "onAttackEvent"), "reads status/99098 after sendQuestStartDialog, which may have written it")
		self.assertEqual(self.refusal(doc, "onItemUseEvent"), "QuestService.startQuest when env.getQuestId() is not known")
		# sendQuestEndDialog may set env's quest id (AbstractQuestHandler.java:428,447)
		self.assertEqual(self.refusal(doc, "onEnterWorldEvent"), "QuestService.startQuest when env.getQuestId() is not known")
		# startQuest never removes a QuestState: the one the path saw is still there (no status read, which would be refused)
		logout = one(doc, "onLogOutEvent", lambda c: c["given"]["otherQuests"]["99098"] is not None)
		self.assertEqual((calls(logout), logout["returns"]), ([("env.setQuestId", [99098]), ("sendQuestStartDialog", [])], True))

	def test_world_map_ids_are_the_constructor_argument(self):
		# WorldMapType.java: `POETA(210010000)`, `HOUSING_LC_LEGION(700020000, true)`; getId() returns worldId (:221-223). The map a guard
		# excludes is replaced by the first declared constant it allows (PANDAEMONIUM 120010000, then MARCHUTAN 120020000)
		doc = trace_text(handler(
			hook("\t\tint map = env.getPlayer().getWorldId();\n\t\tif (map == WorldMapType.POETA.getId())\n\t\t\treturn sendQuestDialog(env, 1);\n"
			     "\t\tif (map == WorldMapType.HOUSING_LC_LEGION.getId())\n\t\t\treturn sendQuestDialog(env, 2);\n"
			     "\t\tif (map != WorldMapType.PANDAEMONIUM.getId())\n\t\t\treturn sendQuestDialog(env, 3);\n\t\treturn false;\n"),
			hook("\t\treturn env.getPlayer().getWorldId() == WorldMapType.NO_SUCH_MAP.getId();\n", "boolean onEnterWorldEvent(QuestEnv env)")))
		pages = {c["given"]["player"]["worldId"]: [a[0] for _, a in calls(c)] for c in self.cases(doc)}
		self.assertEqual(pages, {210010000: [1], 700020000: [2], 120020000: [3], 120010000: []})
		self.assertEqual(self.refusal(doc, "onEnterWorldEvent"), "WorldMapType.NO_SUCH_MAP")

	def test_a_constant_plus_an_input_is_solved(self):
		doc = trace_text(handler(hook(
			QS + "\t\tif (qs == null)\n\t\t\treturn false;\n\t\tint var = qs.getQuestVarById(0);\n\t\tif (1 + var == 4) {\n"
			"\t\t\tplayQuestMovie(env, var);\n\t\t\treturn true;\n\t\t}\n\t\treturn false;\n")))
		c = next(c for c in self.cases(doc) if c["returns"] is True)
		self.assertEqual((c["given"]["questState"]["vars"], movies(c)), ({"0": 3}, [3]))

	def test_range_ends_skip_the_values_a_guard_excludes(self):
		# `var >= 2 && var < 6` false, `var == 6` false, `var >= 3 && var != 9 && var != 63`: 7..62 without 9 (6 and 63 are excluded ends)
		doc = trace_text(handler(hook(
			QS + "\t\tif (qs == null)\n\t\t\treturn false;\n\t\tint var = qs.getQuestVarById(0);\n\t\tif (var >= 2 && var < 6)\n"
			"\t\t\treturn sendQuestDialog(env, 1);\n\t\telse if (var == 6)\n\t\t\treturn sendQuestDialog(env, 2);\n"
			"\t\telse if (var >= 3 && var != 9 && var != 63)\n\t\t\treturn sendQuestDialog(env, 3);\n\t\treturn false;\n",
			"boolean onDialogEvent(QuestEnv env)")))
		page = {c["effects"][0]["args"][0]: c for c in self.cases(doc, "onDialogEvent") if c["effects"]}
		self.assertEqual((page[1]["ranges"], page[1]["given"]["questState"]["vars"]), ({"questState.vars.0": [2, 5]}, {"0": 2}))
		self.assertNotIn("rangeExcludes", page[1])
		self.assertEqual(page[3]["ranges"], {"questState.vars.0": [7, 62]})
		self.assertEqual(page[3]["rangeExcludes"], {"questState.vars.0": [9]})
		self.assertEqual(page[3]["given"]["questState"]["vars"], {"0": 7})
		self.assertNotIn("ranges", page[2])

	def test_the_parser_precedence_the_oracle_relies_on(self):
		# the oracle shares questgen's jast (common mode, extract.py docstring): these pin its precedence and associativity (JLS 15.7, 15.17-15.25)
		exprs = ("10 - 4 - 3", "2 + 3 * 4", "(2 + 3) * 4", "100 / 10 / 5", "-2 + 3", "7 - -2", "5 % 3 * 2", "(int) 2.7 * 2",
		         "true || false && false ? 1 : 2", "false && true || true ? 1 : 2", "!true && false ? 1 : 2", "1 + 2 == 3 ? 1 : 2",
		         "1 == 1 ? 2 == 3 ? 4 : 5 : 6")
		doc = trace_text(handler(hook("".join(f"\t\tplayQuestMovie(env, {x});\n" for x in exprs) + "\t\treturn true;\n")))
		self.assertEqual(movies(self.single(doc)), [3, 14, 20, 2, 1, 9, 4, 4, 1, 1, 2, 1, 5])

	def test_other_npcs_are_real_templates(self):
		# the first three npc_template elements of npc_templates.xml
		path = runner.DEFAULT_STATIC_DATA / "npcs" / "npc_templates.xml"
		if not path.is_file():
			self.skipTest("static data not present")
		found = []
		with open(path, encoding="utf-8") as f:
			for line in f:
				m = re.search(r'<npc_template npc_id="(\d+)"', line)
				if m:
					found.append(int(m.group(1)))
					if len(found) == 3:
						break
		self.assertEqual(tuple(found), extract.OTHER_NPCS)


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class Q08SliceTest(unittest.TestCase):
	"""the gelkmaros and enshar slice (SLICE_Q08, phase 6 step 2, lane C, 2026-10-05; docs/deviations/Q08.md)"""

	@classmethod
	def setUpClass(cls):
		cls.docs = {}
		for rel in extract.SLICE_Q08:
			d = extract.trace_file(tables(), extract.QUEST_DIR / rel, rel)
			cls.docs[d["questId"]] = d

	def test_the_q08_slice(self):
		java = sorted(f"{d}/{f.name}" for d in ("gelkmaros", "enshar") for f in (extract.QUEST_DIR / d).glob("*.java"))
		self.assertEqual(len(java), 64)
		# all 64 since the Q08 follow-up (row B39: gelkmaros/_20034RescuetheReians)
		self.assertEqual(sorted(extract.SLICE_Q08), java)
		self.assertEqual(len(self.docs), 64)
		self.assertEqual(set(extract.SLICE_Q08) & set(extract.SLICE_TIER_A + extract.SLICE_ROUTE + extract.SLICE_Q03 + extract.SLICE_Q10), set())
		# every hook refused in eight (the golden harness's ORACLE_REFUSES_EVERY_HOOK); their registration is traced
		empty = sorted(q for q, d in self.docs.items() if not d["cases"])
		self.assertEqual(empty, [21004, 21027, 21033, 21036, 21071, 21105, 21249, 25052])
		for d in self.docs.values():
			self.assertIsInstance(d["register"], list)
		self.assertEqual(extract.check(rels=extract.SLICE_Q08, extra=False), [])

	def test_20500_starts_in_enshar_only(self):
		# _20500EnsharExpedition.java:75-80: onEnterWorldEvent starts the quest in Enshar (WorldMapType.ENSHAR) without one, else false
		d = self.docs[20500]
		world = [c for c in d["cases"] if c["hook"] == "onEnterWorldEvent"]
		started = [c for c in world if c["effects"]]
		self.assertTrue(started)
		for c in started:
			self.assertEqual(c["effects"][0]["call"], "QuestService.startQuest")
			self.assertEqual(c["given"]["player"]["worldId"], extract.Tables().world_maps["ENSHAR"])
		self.assertTrue(all(not c["effects"] and c["returns"] is False for c in world if c not in started))


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class Q01SliceTest(unittest.TestCase):
	"""the reshanta slice (SLICE_Q01, phase 6 step 2, lane C, 2026-10-05; docs/deviations/Q01.md)"""

	@classmethod
	def setUpClass(cls):
		cls.docs = {}
		for rel in extract.SLICE_Q01:
			d = extract.trace_file(tables(), extract.QUEST_DIR / rel, rel)
			cls.docs[d["questId"]] = d

	def test_the_q01_slice(self):
		java = sorted(f"reshanta/{f.name}" for f in (extract.QUEST_DIR / "reshanta").glob("*.java"))
		self.assertEqual(len(java), 82)
		# questgen refuses one (a List<Integer> field): not in the tree, not in the slice
		self.assertEqual(sorted(extract.SLICE_Q01), sorted(set(java) - {"reshanta/_2759TenaciousGuardian.java"}))
		self.assertEqual(len(self.docs), 81)
		self.assertEqual(set(extract.SLICE_Q01) & set(extract.SLICE_TIER_A + extract.SLICE_ROUTE + extract.SLICE_Q03 + extract.SLICE_Q10 +
		                                              extract.SLICE_Q08), set())
		self.assertEqual(sorted(q for q, d in self.docs.items() if not d["cases"]), [2798])
		self.assertEqual(extract.check(rels=extract.SLICE_Q01, extra=False), [])

	def test_kill_ranked_quests_register_their_rank(self):
		# _1702Defeat9thRankAsmodianSoldiers.java:25-31: the lowest rank, and the kill-ranked helper up to var 10 with the reward
		d = self.docs[1702]
		self.assertIn({"call": "registerOnKillRanked", "args": ["GRADE9_SOLDIER", 1702]}, d["register"])
		(c,) = [c for c in d["cases"] if c["hook"] == "onKillRankedEvent"]
		self.assertEqual([(e["call"], e["args"]) for e in c["effects"]], [("defaultOnKillRankedEvent", [0, 10, True])])

	def test_14040_starts_in_reshanta_only(self):
		# _14040OrdersFromReshanta.java:58-63: onEnterWorldEvent starts the quest in Reshanta without one, else nothing
		world = [c for c in self.docs[14040]["cases"] if c["hook"] == "onEnterWorldEvent"]
		started = [c for c in world if c["effects"]]
		self.assertTrue(started)
		for c in started:
			self.assertEqual(c["effects"][0]["call"], "QuestService.startQuest")
			self.assertEqual(c["given"]["player"]["worldId"], tables().world_maps["RESHANTA"])


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class Q02SliceTest(unittest.TestCase):
	"""the inggison slice (SLICE_Q02, phase 6 step 2, lane C, 2026-10-05; docs/deviations/Q02.md)"""

	def test_the_q02_slice(self):
		java = sorted(f"inggison/{f.name}" for f in (extract.QUEST_DIR / "inggison").glob("*.java"))
		self.assertEqual(len(java), 59)
		self.assertEqual(sorted(extract.SLICE_Q02), java)
		self.assertEqual(set(extract.SLICE_Q02) & set(extract.SLICE_TIER_A + extract.SLICE_ROUTE + extract.SLICE_Q03 + extract.SLICE_Q10 +
		                                              extract.SLICE_Q08 + extract.SLICE_Q01), set())
		docs = {}
		for rel in extract.SLICE_Q02:
			d = extract.trace_file(tables(), extract.QUEST_DIR / rel, rel)
			docs[d["questId"]] = d
		# every hook refused in five (the golden harness's ORACLE_REFUSES_EVERY_HOOK); their registration is traced
		self.assertEqual(sorted(q for q, d in docs.items() if not d["cases"]), [11031, 11032, 11033, 11053, 11118])
		self.assertTrue(all(isinstance(d["register"], list) for d in docs.values()))
		self.assertEqual(extract.check(rels=extract.SLICE_Q02, extra=False), [])


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class Q14SliceTest(unittest.TestCase):
	"""the instance directories K-W (SLICE_Q14, phase 6 step 2, lane C, 2026-10-05; docs/deviations/Q14.md)"""

	# the 21 Q14 files questgen refuses (docs/deviations/Q14.md, "Not in the tree")
	REFUSED = {
		"levinshor/_13744AgentinNeed.java", "levinshor/_23744EffectElyosElimination.java", "pangaea/_14220NewZoneNewRules.java",
		"pangaea/_24220WelcometoPanesterra.java", "tiamat_stronghold/_30721OminousDebrisEnergy.java",
		"tiamat_stronghold/_30771ImpendingDebrisEnergy.java", "marchutan_priory/_47000AltgardOrbIt.java", "marchutan_priory/_47003AGlobalProblem.java",
		"marchutan_priory/_47006AmplifiersWithIssues.java", "orichalcum_key/_37100MutantNinjaIninas.java", "orichalcum_key/_37103CamoAndCarnage.java",
		"orichalcum_key/_37106AsmoHunt.java", "orichalcum_key/_37107CoolBlueWater.java", "orichalcum_key/_37110MyYoungApprentice.java",
		"orichalcum_key/_37113AsmoICU.java", "the_circle/_47100WardsAndWardOrbs.java", "the_circle/_47103AGlobeTrottingLesson.java",
		"the_circle/_47106TurningUpTheAmplifiers.java", "the_circle/_47107WardsAndWardOrbs.java", "the_circle/_47110AGlobeTrottingLesson.java",
		"the_circle/_47113TurningUpTheAmplifiers.java"}

	def test_the_q14_slice(self):
		dirs = sorted({rel.split("/")[0] for rel in extract.SLICE_Q14} | {rel.split("/")[0] for rel in self.REFUSED})
		java = sorted(f"{d}/{f.name}" for d in dirs for f in (extract.QUEST_DIR / d).glob("*.java"))
		self.assertEqual(len(java), 95)
		self.assertEqual(sorted(extract.SLICE_Q14), sorted(set(java) - self.REFUSED))
		self.assertEqual(len(extract.SLICE_Q14), 74)
		self.assertEqual(set(extract.SLICE_Q14) & set(extract.SLICE_TIER_A + extract.SLICE_ROUTE + extract.SLICE_Q03 + extract.SLICE_Q10 +
		                                              extract.SLICE_Q08 + extract.SLICE_Q01 + extract.SLICE_Q02), set())
		docs = {}
		for rel in extract.SLICE_Q14:
			d = extract.trace_file(tables(), extract.QUEST_DIR / rel, rel)
			docs[d["questId"]] = d
		# no case in eleven (the golden harness's ORACLE_REFUSES_EVERY_HOOK); their registration is traced
		self.assertEqual(sorted(q for q, d in docs.items() if not d["cases"]),
		                 [3208, 3217, 3219, 3220, 3939, 3940, 4208, 4217, 4219, 4220, 30553])
		self.assertTrue(all(isinstance(d["register"], list) for d in docs.values()))
		self.assertEqual(extract.check(rels=extract.SLICE_Q14, extra=False), [])


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class ScheduledTaskTest(unittest.TestCase):
	"""lane C (phase 6 step 1, phase6-transliterator.md §7): closures (jast's closures=True), the task of ThreadPoolManager.schedule run after the
	hook, the item-use packets and removal around it, a bounded symbolic setQuestVar, AbyssRankEnum, and parser refusals per hook"""

	USE = "HandlerResult onItemUseEvent(final QuestEnv env, Item item)"

	def cases(self, doc, name):
		return [c for c in doc["cases"] if c["hook"] == name]

	def refusal(self, doc, name):
		return next(h for h in doc["hooks"] if h["hook"] == name).get("unsupported")

	def task_hook(self, task, before="", after="\t\treturn HandlerResult.SUCCESS;\n"):
		return hook(
			"\t\tfinal QuestState qs = env.getPlayer().getQuestStateList().getQuestState(questId);\n\t\tif (qs == null)\n"
			"\t\t\treturn HandlerResult.FAILED;\n\t\tfinal int itemObjId = item.getObjectId();\n" + before +
			"\t\tThreadPoolManager.getInstance().schedule(" + task + ", 3000);\n" + after, self.USE)

	def test_an_anonymous_runnable_runs_after_the_hook(self):
		# ThreadPoolManager.schedule (ThreadPoolManager.java): the task runs on a pool thread after its delay, when the hook has returned; it
		# reads the QuestState then (the hook's own write before it is read back) and its effects carry the index of their schedule effect
		doc = trace_text(handler(self.task_hook(
			"new Runnable() {\n\n\t\t\t@Override\n\t\t\tpublic void run() {\n"
			"\t\t\t\tPacketSendUtility.broadcastPacket(env.getPlayer(), new SM_ITEM_USAGE_ANIMATION(env.getPlayer().getObjectId(), itemObjId, "
			"182200001, 0, 1, 0), true);\n\t\t\t\tenv.getPlayer().getInventory().decreaseByObjectId(itemObjId, 1);\n"
			"\t\t\t\tif (qs.getQuestVarById(0) == 2)\n\t\t\t\t\treturn;\n\t\t\t\tqs.setQuestVarById(0, qs.getQuestVarById(0) + 1);\n"
			"\t\t\t\tupdateQuestStatus(env);\n\t\t\t}\n\t\t}",
			before="\t\tplayQuestMovie(env, 7);\n", after="\t\tplayQuestMovie(env, 8);\n\t\treturn HandlerResult.SUCCESS;\n")))
		got = {json.dumps(c["given"].get("questState"), sort_keys=True): ([(e["call"], e["args"], e.get("task")) for e in c["effects"]],
		                                                                c["returns"]) for c in self.cases(doc, "onItemUseEvent")}
		head = [("playQuestMovie", [7], None), ("ThreadPoolManager.schedule", [3000], None), ("playQuestMovie", [8], None),
		        ("PacketSendUtility.broadcastPacket", [{"new": "SM_ITEM_USAGE_ANIMATION", "args": ["$playerObjectId", "$itemObjectId", 182200001, 0,
		                                                                                         1, 0]}, True], 1),
		        ("inventory.decreaseByObjectId", ["$itemObjectId", 1], 1)]
		self.assertEqual(got, {
			"null": ([], "FAILED"),
			json.dumps({"status": "START", "vars": {"0": 2}}, sort_keys=True): (head, "SUCCESS"),
			json.dumps({"status": "START", "vars": {"0": 0}}, sort_keys=True):
				(head + [("qs.setQuestVarById", [0, 1], 1), ("updateQuestStatus", [], 1)], "SUCCESS")})

	def test_tasks_run_in_the_order_of_their_delays_on_the_locals_they_captured(self):
		# two lambdas: the longer delay first in the source, the shorter runs first; each sees the local it captured (effectively final)
		doc = trace_text(handler(hook(
			"\t\tfinal int a = 4;\n\t\tThreadPoolManager.getInstance().schedule(() -> playQuestMovie(env, a), 5000);\n\t\tfinal int b = 5;\n"
			"\t\tThreadPoolManager.getInstance().schedule(() -> {\n\t\t\tplayQuestMovie(env, b);\n\t\t}, 1000);\n\t\treturn true;\n")))
		(c,) = self.cases(doc, "onKillEvent")
		self.assertEqual([(e["call"], e["args"], e.get("task")) for e in c["effects"]],
		                 [("ThreadPoolManager.schedule", [5000], None), ("ThreadPoolManager.schedule", [1000], None), ("playQuestMovie", [5], 1),
		                  ("playQuestMovie", [4], 0)])
		self.assertEqual(c["returns"], True)

	def test_closures_the_oracle_does_not_model_refuse_the_hook_not_the_file(self):
		doc = trace_text(handler(
			hook("\t\tThreadPoolManager.getInstance().schedule(() -> playQuestMovie(env, 1), 1000);\n\t\treturn true;\n"),
			hook("\t\tRunnable r = () -> playQuestMovie(env, 1);\n\t\tr.run();\n\t\treturn true;\n", "boolean onAttackEvent(QuestEnv env)"),
			hook(QS + "\t\tThreadPoolManager.getInstance().schedule(() -> playQuestMovie(env, qs.getQuestVarById(0)), 1000);\n\t\treturn true;\n",
			     "boolean onDieEvent(QuestEnv env)"),
			hook("\t\ttry {\n\t\t\treturn sendQuestDialog(env, 1011);\n\t\t} catch (RuntimeException e) {\n\t\t\treturn false;\n\t\t}\n",
			     "boolean onDialogEvent(QuestEnv env)")))
		self.assertEqual(len(self.cases(doc, "onKillEvent")), 1)
		self.assertEqual(self.refusal(doc, "onAttackEvent"), "call r.run() on a value")
		# the task dereferences a QuestState the hook never checked: Java's pool logs that NullPointerException after the hook returned
		self.assertEqual(self.refusal(doc, "onDieEvent"), "a NullPointerException with a scheduled task")
		self.assertTrue(self.refusal(doc, "onDialogEvent").startswith("try"))      # jast refuses it: the hook, not the file
		self.assertIsInstance(doc["register"], list)

	def test_the_owners_corrections_are_traced_and_equal_to_questgens(self):
		# the owner's decision of 2026-10-05 (docs/deviations/Q02.md): 11001 and 11008 name themselves as their level hook's pre-quest; the
		# document traces the corrected call, says so, and the oracle's table is questgen's (each tool keeps its own copy)
		from questgen import emit
		self.assertEqual(extract.OWNER_CORRECTIONS, emit.OWNER_CORRECTIONS)
		for rel, line in (("inggison/_11001KindMeira.java", 129), ("inggison/_11008LetterOfEncouragement.java", 100)):
			doc = extract.trace_file(tables(), extract.QUEST_DIR / rel, rel)
			self.assertEqual([c["line"] for c in doc["corrections"]], [line])
			level = [c for c in doc["cases"] if c["hook"] == "onLevelChangedEvent"]
			self.assertEqual([c["effects"] for c in level], [[{"call": "defaultOnLevelChangedEvent", "kind": "quest", "args": [], "line": line}]])
		# a file without a correction has no member, and a correction whose Java text is not on its line is refused
		rel = "inggison/_10031ARiskfortheObelisk.java"
		self.assertNotIn("corrections", extract.trace_file(tables(), extract.QUEST_DIR / rel, rel))
		with self.assertRaises(extract.OracleError):
			extract.corrected_source("a\nb\n", {2: ("c", "d", "why")}, "x.java")

	def test_switch_expressions_and_switch_rules(self):
		# the review of #79, item 9: JLS 15.28 (a switch expression's value is its matching arm's, else default's) and JLS 14.11.2 (a rule arm
		# never falls through)
		doc = trace_text(handler(hook(
			"\t\tint page = switch (env.getDialogActionId()) {\n\t\t\tcase QUEST_SELECT -> 1011;\n\t\t\tcase SETPRO1, SETPRO2 -> 1352;\n"
			"\t\t\tdefault -> 0;\n\t\t};\n\t\tswitch (page) {\n\t\t\tcase 1011 -> playQuestMovie(env, 1);\n\t\t\tcase 1352 -> "
			"playQuestMovie(env, 2);\n\t\t\tdefault -> playQuestMovie(env, 3);\n\t\t}\n\t\treturn sendQuestDialog(env, page);\n",
			"boolean onDialogEvent(QuestEnv env)")))
		got = sorted((c["given"]["dialogAction"]["name"], [(e["call"], e["args"]) for e in c["effects"]]) for c in self.cases(doc, "onDialogEvent"))
		self.assertEqual(got, [("QUEST_SELECT", [("playQuestMovie", [1]), ("sendQuestDialog", [1011])]),
		                       ("SETPRO1", [("playQuestMovie", [2]), ("sendQuestDialog", [1352])]),
		                       ("SETPRO2", [("playQuestMovie", [2]), ("sendQuestDialog", [1352])]),
		                       ("USE_OBJECT", [("playQuestMovie", [3]), ("sendQuestDialog", [0])])])
		# _30211's switch rules and its lambda: traced now (its refusal said `expected ':'` before)
		rel = "beshmundir/_30211GroupTheRodandtheOrb.java"
		self.assertTrue(extract.trace_file(tables(), extract.QUEST_DIR / rel, rel)["cases"])

	def test_a_bounded_symbolic_set_quest_var_is_slot_0(self):
		# QuestVars.setVar (QuestVars.java:52-58): a value in 0..63 is slot 0 and the other slots 0; an unbounded one is refused
		doc = trace_text(handler(
			hook(QS + "\t\tif (qs == null)\n\t\t\treturn false;\n\t\tqs.setQuestVarById(1, 9);\n\t\tint var = qs.getQuestVarById(0);\n"
			     "\t\tif (var >= 2 && var < 5) {\n\t\t\tqs.setQuestVar(var + 1);\n\t\t\treturn sendQuestDialog(env, qs.getQuestVarById(0) * 10 + "
			     "qs.getQuestVarById(1));\n\t\t}\n\t\treturn false;\n"),
			hook(QS + "\t\tif (qs == null)\n\t\t\treturn false;\n\t\tqs.setQuestVar(qs.getQuestVarById(0) + 1);\n\t\treturn true;\n",
			     "boolean onDieEvent(QuestEnv env)")))
		c = next(c for c in self.cases(doc, "onKillEvent") if c["effects"])
		self.assertEqual(calls(c), [("qs.setQuestVarById", [1, 9]), ("qs.setQuestVar", [3]), ("sendQuestDialog", [30])])
		self.assertEqual([(h["value"], [e["args"] for e in h["effects"]]) for h in c["atHigh"]], [(4, [[1, 9], [5], [50]])])
		self.assertTrue(self.refusal(doc, "onDieEvent").startswith("a symbolic qs.setQuestVar"))

	def test_kill_ranked_registration_names_the_rank(self):
		text = handler(hook("\t\treturn defaultOnKillRankedEvent(env, 0, 10, true);\n", "boolean onKillRankedEvent(QuestEnv env)"))
		text = text.replace("qe.registerQuestNpc(203001).addOnTalkEvent(questId);",
		                    "qe.registerQuestNpc(203001).addOnTalkEvent(questId);\n\t\tqe.registerOnKillRanked(AbyssRankEnum.STAR1_OFFICER, questId);")
		doc = trace_text(text)
		self.assertEqual(doc["register"][-1], {"call": "registerOnKillRanked", "args": ["STAR1_OFFICER", 99100]})
		(c,) = self.cases(doc, "onKillRankedEvent")
		self.assertEqual((calls(c), c["returns"]), ([("defaultOnKillRankedEvent", [0, 10, True])], {"resultOf": 0}))

	def test_the_closure_handlers_are_traced(self):
		# every handler that schedules a task raised before (the default parser refuses closures): each has a document now (the 29 files of
		# questgen's rule scheduled-closure among them, phase6-transliterator.md §2.2)
		rels = sorted(f.relative_to(extract.QUEST_DIR).as_posix() for f in extract.QUEST_DIR.rglob("*.java")
		              if "ThreadPoolManager.getInstance().schedule(" in f.read_text(encoding="utf-8", errors="replace"))
		self.assertGreaterEqual(len(rels), 29)
		for rel in rels:
			with self.subTest(rel=rel):
				try:
					doc = extract.trace_file(tables(), extract.QUEST_DIR / rel, rel)
				except extract.OracleError as e:
					self.assertNotIn("lambda", str(e))
					self.assertNotIn("anonymous-class", str(e))
					continue
				self.assertIsInstance(doc["hooks"], list)
		doc = extract.trace_file(tables(), extract.QUEST_DIR / "altgard/_2208MauInTenMinutesADay.java", "altgard/_2208MauInTenMinutesADay.java")
		c = next(c for c in self.cases(doc, "onItemUseEvent") if any("task" in e for e in c["effects"]))
		# _2208MauInTenMinutesADay.java:85-97: the start animation, the schedule, then in the task the end animation, the removal, var 1
		self.assertEqual([(e["call"], e.get("task")) for e in c["effects"]],
		                 [("PacketSendUtility.broadcastPacket", None), ("ThreadPoolManager.schedule", None),
		                  ("PacketSendUtility.broadcastPacket", 1), ("inventory.decreaseByObjectId", 1), ("qs.setQuestVarById", 1),
		                  ("updateQuestStatus", 1)])


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class RealSliceTest(unittest.TestCase):
	"""the Poeta and Ishalgen slice; each case below derived by hand from the handler's Java"""

	@classmethod
	def setUpClass(cls):
		cls.docs = {}
		for rel in extract.SLICE_TIER_A:
			d = extract.trace_file(tables(), extract.QUEST_DIR / rel, rel)
			cls.docs[d["questId"]] = d

	def test_every_hook_of_the_slice_is_traced(self):
		self.assertEqual(len(self.docs), 20)
		for qid, d in self.docs.items():
			with self.subTest(quest=qid):
				self.assertIsInstance(d["register"], list)
				self.assertEqual([h for h in d["hooks"] if "unsupported" in h], [])
				self.assertTrue(d["cases"])

	def test_1001_the_item_check_step(self):
		# _1001TheKerubThreat.java:93-108: SETPRO3 at 203071 with var 7 and 3 of 182200001 removes them, sets var 8 and REWARD, closes to page 10
		d = self.docs[1001]
		c = one(d, "onDialogEvent", target={"kind": "npc", "npcId": 203071}, dialogAction={"name": "SETPRO3", "id": 10002},
		        questState={"status": "START", "vars": {"0": 7}}, inventory={"182200001": 3})
		self.assertEqual(calls(c), [("removeQuestItem", [182200001, 3]), ("qs.setQuestVarById", [0, 8]), ("qs.setStatus", ["REWARD"]),
		                            ("updateQuestStatus", []),
		                            ("PacketSendUtility.sendPacket", [{"new": "SM_DIALOG_WINDOW", "args": ["$targetObjectId", 10]}])])
		self.assertIs(c["returns"], True)
		c = one(d, "onDialogEvent", target={"kind": "npc", "npcId": 203071}, dialogAction={"name": "SETPRO3", "id": 10002},
		        questState={"status": "START", "vars": {"0": 7}}, inventory={"182200001": 0})
		self.assertEqual((calls(c), c["returns"]), ([("sendQuestDialog", [1779])], {"resultOf": 0}))
		c = one(d, "onKillEvent", target={"kind": "npc", "npcId": 210670}, questState={"status": "START", "vars": {"0": 1}})
		self.assertEqual(calls(c), [("qs.setQuestVarById", [0, 2]), ("updateQuestStatus", [])])     # :46-50
		self.assertEqual(c["ranges"], {"questState.vars.0": [1, 5]})

	def test_ranged_inputs_are_traced_at_their_high_end(self):
		# P6-Q (route-gen review): the same path with the ranged input at hi, effects evaluated there. _1001TheKerubThreat.java:47-50: a kill
		# at var 5 (the last value `var > 0 && var < 6` lets through) sets var 6; :96-108: 2 of 182200001 (the last count `itemCount >= 3`
		# rejects) gets page 1779
		d = self.docs[1001]
		c = one(d, "onKillEvent", target={"kind": "npc", "npcId": 210670}, questState={"status": "START", "vars": {"0": 1}})
		(h,) = c["atHigh"]
		self.assertEqual((h["input"], h["value"], h["given"]["questState"]), ("questState.vars.0", 5, {"status": "START", "vars": {"0": 5}}))
		self.assertEqual((calls(h), h["returns"]), ([("qs.setQuestVarById", [0, 6]), ("updateQuestStatus", [])], True))
		c = one(d, "onDialogEvent", target={"kind": "npc", "npcId": 203071}, dialogAction={"name": "SETPRO3", "id": 10002},
		        questState={"status": "START", "vars": {"0": 7}}, inventory={"182200001": 0})
		(h,) = c["atHigh"]
		self.assertEqual((h["input"], h["given"]["inventory"]), ("inventory.182200001", {"182200001": 2}))
		self.assertEqual((calls(h), h["returns"]), ([("sendQuestDialog", [1779])], {"resultOf": 0}))
		# the other side of `var < 6` runs to 63, the QuestVars slot's own end (QuestVars.java:22-58)
		c = one(d, "onKillEvent", target={"kind": "npc", "npcId": 210670}, questState={"status": "START", "vars": {"0": 6}})
		self.assertEqual([(h["value"], h["effects"], h["returns"]) for h in c["atHigh"]], [(63, [], False)])
		# a case without a range has no high end
		c = one(d, "onKillEvent", questState=None)
		self.assertNotIn("atHigh", c)

	def test_unguarded_var_reads_are_free(self):
		# P6-Q (route-gen review): _2001ThinkingAhead.java:39 reads var with no guard on the CHECK_USER_HAS_QUEST_ITEM path (:62-63), whose
		# checkQuestItems(env, 1, ...) acts at var 1 only, so a harness may set it; the kill hook guards var (:89-93), so there it is not free
		d = self.docs[2001]
		c = one(d, "onDialogEvent", target={"kind": "npc", "npcId": 203518}, questState={"status": "START", "vars": {"0": 0}},
		        dialogAction={"name": "CHECK_USER_HAS_QUEST_ITEM", "id": 39})
		self.assertEqual((c["free"], calls(c)), (["questState.vars.0"], [("checkQuestItems", [1, 2, False, 1694, 1693])]))
		for c in matching(d, "onKillEvent"):
			self.assertNotIn("free", c)
		# a var an effect uses is not free: `qs.setQuestVarById(1, var1 + 1)` fixes the written value by var1
		doc = trace_text(SYNTHETIC.split("\t@Override\n\tpublic boolean onDialogEvent")[0] + """	@Override
	public boolean onDialogEvent(QuestEnv env) {
		QuestState qs = env.getPlayer().getQuestStateList().getQuestState(questId);
		if (qs == null)
			return false;
		int var = qs.getQuestVarById(0);
		int var1 = qs.getQuestVarById(1);
		qs.setQuestVarById(1, var1 + 1);
		return sendQuestDialog(env, 1011);
	}
}
""")
		c = one(doc, "onDialogEvent", lambda c: c["effects"])
		self.assertEqual(c["free"], ["questState.vars.0"])

	def test_1000_prologue_start_quest(self):
		# _1000Prologue.java:27-35: an Elyos without the quest starts it and plays movie 1; if startQuest fails and the quest is absent,
		# getQuestState(questId).getStatus() throws
		d = self.docs[1000]
		ok = one(d, "onEnterWorldEvent", lambda c: c.get("assume") == [{"effect": 1, "returns": True}])
		self.assertEqual(calls(ok), [("env.setQuestId", [1000]), ("QuestService.startQuest", []), ("playQuestMovie", [1, True])])
		self.assertEqual((ok["given"], ok["returns"]), ({"player": {"race": "ELYOS"}, "questState": None}, True))
		failed = one(d, "onEnterWorldEvent", lambda c: c.get("assume") == [{"effect": 1, "returns": False}])
		self.assertEqual(failed["throws"], "NullPointerException")
		c = one(d, "onEnterWorldEvent", player={"race": "ASMODIANS"})
		self.assertEqual((c["effects"], c["returns"]), ([], False))
		c = one(d, "onMovieEndEvent", args={"movieId": 1}, questState={"status": "START"})
		self.assertEqual(calls(c), [("qs.setStatus", ["REWARD"]), ("QuestService.finishQuest", [])])   # :38-47

	def test_2122_item_use_and_target_zero(self):
		d = self.docs[2122]
		c = one(d, "onItemUseEvent", questState=None)                        # _2122AshesToAshes.java:90-97
		self.assertEqual((calls(c), c["returns"]), ([("sendQuestDialog", [4])], {"fromBoolean": {"resultOf": 0}}))
		c = one(d, "onItemUseEvent", questState={"status": "START"})
		self.assertEqual((c["effects"], c["returns"]), ([], "FAILED"))
		c = one(d, "onDialogEvent", target={"kind": "none"}, questState=None, dialogAction={"name": "QUEST_ACCEPT_1", "id": 1002})
		self.assertEqual(calls(c), [("sendQuestStartDialog", [])])             # :40-43: targetId 0 is the item's dialog

	def test_1005_destroy_helper_and_varargs(self):
		# _1005BarringtheGate.java:127-131 and destroy(-1, env) :159-169
		d = self.docs[1005]
		c = one(d, "onDialogEvent", target={"kind": "npc", "npcId": 700080}, questState={"status": "START", "vars": {"0": 8}})
		self.assertEqual(calls(c), [("playQuestMovie", [21]), ("qs.setStatus", ["REWARD"]), ("updateQuestStatus", [])])
		self.assertIs(c["returns"], False)
		c = one(d, "onQuestCompletedEvent")
		self.assertEqual(calls(c), [("defaultOnQuestCompletedEvent", [1100, 1004, 1003, 1002, 1001])])   # :148-152

	def test_committed_traces_are_current(self):
		self.assertEqual(extract.check(), [], "regenerate with: python oracle.py quest-trace generate")


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class RouteSliceTest(unittest.TestCase):
	"""the ascension route slice (SLICE_ROUTE, P6-Q 2026-09-29); the cases below derived by hand from the handlers' Java"""

	@classmethod
	def setUpClass(cls):
		cls.docs = {}
		for rel in extract.SLICE_ROUTE:
			d = extract.trace_file(tables(), extract.QUEST_DIR / rel, rel)
			cls.docs[d["questId"]] = d

	def test_the_route_slice(self):
		self.assertEqual(sorted(self.docs), [1100, 1205, 1913, 1914, 1915, 1916, 2100, 2132, 2901, 2902, 2903, 2904, 19070, 19071, 29070, 29071])
		self.assertEqual(set(extract.SLICE_TIER_A) & set(extract.SLICE_ROUTE), set())
		for qid in (1205, 2132):                     # every hook refused: `new QuestEnv`, getStartingClass on a value; registration only
			self.assertEqual((self.docs[qid]["cases"], [h for h in self.docs[qid]["hooks"] if "unsupported" not in h]), ([], []))
			self.assertIsInstance(self.docs[qid]["register"], list)
		for qid in (1100, 2100):                     # every hook traced since WorldMapType.X.getId() is a constant (P6-Q prologue)
			self.assertEqual([h for h in self.docs[qid]["hooks"] if "unsupported" in h], [])
			self.assertEqual(sorted(h["hook"] for h in self.docs[qid]["hooks"]), ["onDialogEvent", "onEnterWorldEvent", "onLevelChangedEvent"])

	def test_1100_and_2100_start_in_their_own_map(self):
		# _1100KaliosCall.java:56-67 and _2100OrderoftheCaptain.java:54-65: in Poeta (WorldMapType.POETA, 210010000) / Ishalgen (ISHALGEN,
		# 220010000) a player without the quest starts it at enter world and the level hook runs defaultOnLevelChangedEvent with no pre-quest;
		# anywhere else both do nothing (the other map picked is the first WorldMapType constant, PANDAEMONIUM 120010000)
		for qid, own in ((1100, 210010000), (2100, 220010000)):
			with self.subTest(quest=qid):
				d = self.docs[qid]
				started = one(d, "onEnterWorldEvent", lambda c: c.get("assume") == [{"effect": 0, "returns": True}])
				self.assertEqual((started["given"], calls(started), started["returns"]),
				                 ({"player": {"worldId": own}, "questState": None}, [("QuestService.startQuest", [])], True))
				c = one(d, "onEnterWorldEvent", lambda c: c.get("assume") == [{"effect": 0, "returns": False}])
				self.assertEqual((calls(c), c["returns"]), ([("QuestService.startQuest", [])], False))
				c = one(d, "onEnterWorldEvent", player={"worldId": own}, questState={"status": "START"})
				self.assertEqual((c["effects"], c["returns"]), ([], False))
				c = one(d, "onEnterWorldEvent", player={"worldId": 120010000})
				self.assertEqual((c["effects"], c["returns"]), ([], False))
				c = one(d, "onLevelChangedEvent", player={"worldId": own})
				self.assertEqual(calls(c), [("defaultOnLevelChangedEvent", [])])
				c = one(d, "onLevelChangedEvent", player={"worldId": 120010000})
				self.assertEqual(c["effects"], [])
				self.assertEqual(len(d["cases"]), 13)

	def test_1913_dispatch(self):
		# _1913DispatchtoVerteron.java:21-25 (register) and :28-68 (onDialogEvent)
		d = self.docs[1913]
		self.assertEqual(d["register"], [{"call": "registerOnQuestCompleted", "args": [1913]},
		                                 {"npc": 203726, "event": "addOnTalkEvent", "args": [1913]},
		                                 {"npc": 203097, "event": "addOnTalkEvent", "args": [1913]}])
		c = one(d, "onDialogEvent", target={"kind": "npc", "npcId": 203726}, dialogAction={"name": "QUEST_SELECT", "id": 31},
		        questState={"status": "START", "vars": {"0": 0}})
		self.assertEqual((calls(c), c["returns"]), ([("sendQuestDialog", [1352])], {"resultOf": 0}))
		c = one(d, "onDialogEvent", target={"kind": "npc", "npcId": 203726}, dialogAction={"name": "SETPRO1", "id": 10000},
		        questState={"status": "START", "vars": {"0": 0}})
		self.assertEqual((calls(c), c["returns"]), ([("qs.setQuestVarById", [0, 1]), ("updateQuestStatus", []), ("closeDialogWindow", [])],
		                                            {"resultOf": 2}))
		c = one(d, "onDialogEvent", target={"kind": "npc", "npcId": 203097}, dialogAction={"name": "QUEST_SELECT", "id": 31},
		        questState={"status": "START", "vars": {"0": 1}})
		self.assertEqual(calls(c), [("qs.setStatus", ["REWARD"]), ("updateQuestStatus", []), ("sendQuestDialog", [2375])])
		c = one(d, "onDialogEvent", target={"kind": "npc", "npcId": 203097}, questState={"status": "REWARD", "vars": {"0": 0}})
		self.assertEqual((calls(c), c["returns"]), ([("sendQuestEndDialog", [])], {"resultOf": 0}))
		c = one(d, "onQuestCompletedEvent")
		self.assertEqual(calls(c), [("defaultOnQuestCompletedEvent", [])])


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class Q10SliceTest(unittest.TestCase):
	"""the altgard and pandaemonium slice (SLICE_Q10, P6-Q slice 2, 2026-09-29); the cases below derived by hand from the handlers' Java"""

	@classmethod
	def setUpClass(cls):
		cls.docs = {}
		for rel in extract.SLICE_Q10:
			d = extract.trace_file(tables(), extract.QUEST_DIR / rel, rel)
			cls.docs[d["questId"]] = d

	def test_the_q10_slice(self):
		self.assertEqual(len(self.docs), 69)
		self.assertEqual(set(extract.SLICE_Q10) & (set(extract.SLICE_TIER_A) | set(extract.SLICE_ROUTE)), set())
		self.assertEqual(sorted({rel.split("/")[0] for rel in extract.SLICE_Q10}), ["altgard", "pandaemonium"])
		# the six the slice leaves out: the five hand ports and 4212, held back (docs/deviations/Q10.md)
		for rel in ("altgard/_2208MauInTenMinutesADay.java", "altgard/_2230AFriendlyWager.java", "altgard/_2252ChasingtheLegend.java",
		            "altgard/_24013PoisonInTheWaters.java", "pandaemonium/_2900NoEscapingDestiny.java", "pandaemonium/_4212MissingSidrunerk.java"):
			self.assertNotIn(rel, extract.SLICE)
		# every hook refused: 2213, 2925, 2938, 2952 and the four charms (the golden harness lists them in ORACLE_REFUSES_EVERY_HOOK)
		empty = sorted(q for q, d in self.docs.items() if not d["cases"])
		self.assertEqual(empty, [2213, 2925, 2938, 2952, 4966, 4967, 4968, 4969])
		for qid in empty:
			self.assertIsInstance(self.docs[qid]["register"], list)

	def test_2207_conversing_with_a_skurv(self):
		# _2207ConversingWithaSkurv.java:23-29 (register) and :31-64 (onDialogEvent, the second npc)
		d = self.docs[2207]
		self.assertEqual(d["register"], [{"npc": 203590, "event": "addOnQuestStart", "args": [2207]},
		                                 {"npc": 203590, "event": "addOnTalkEvent", "args": [2207]},
		                                 {"npc": 203591, "event": "addOnTalkEvent", "args": [2207]},
		                                 {"npc": 203557, "event": "addOnTalkEvent", "args": [2207]}])
		c = one(d, "onDialogEvent", target={"kind": "npc", "npcId": 203591}, dialogAction={"name": "SETPRO1", "id": 10000},
		        questState={"status": "START", "vars": {"0": 0}})
		self.assertEqual((calls(c), c["returns"]), ([("defaultCloseDialog", [0, 1])], {"resultOf": 0}))
		c = one(d, "onDialogEvent", target={"kind": "npc", "npcId": 203591}, dialogAction={"name": "SELECT_QUEST_REWARD", "id": 1009},
		        questState={"status": "START", "vars": {"0": 2}})
		self.assertEqual(calls(c), [("qs.setQuestVar", [3]), ("qs.setStatus", ["REWARD"]), ("updateQuestStatus", []), ("sendQuestEndDialog", [])])
		c = one(d, "onDialogEvent", target={"kind": "npc", "npcId": 203591}, questState={"status": "START", "vars": {"0": 1}})
		self.assertEqual((calls(c), c["returns"]), ([], False))

	def test_the_dialog_action_a_path_leaves_free(self):
		# the review of 2026-09-29 (Extractor.dialog_excludes): the else branch of `if (getDialogActionId() == QUEST_SELECT)` excludes only
		# QUEST_SELECT (_2207ConversingWithaSkurv.java:38-41), the one inside the START branch also SETPRO1 (:45-50), a switch's default its
		# cases (_24112NoLaissezFaireForLepharists.java:46-50); a path that names its action has no list
		d = self.docs[2207]
		use = {"name": "USE_OBJECT", "id": -1}
		c = one(d, "onDialogEvent", target={"kind": "npc", "npcId": 203590}, dialogAction=use, questState=None)
		self.assertEqual((calls(c), c["dialogExcludes"]), ([("sendQuestStartDialog", [])], [31]))
		c = one(d, "onDialogEvent", target={"kind": "npc", "npcId": 203591}, dialogAction=use, questState={"status": "START", "vars": {"0": 0}})
		self.assertEqual((calls(c), c["dialogExcludes"]), ([("sendQuestStartDialog", [])], [31, 10000]))
		c = one(d, "onDialogEvent", target={"kind": "npc", "npcId": 203590}, dialogAction={"name": "QUEST_SELECT", "id": 31}, questState=None)
		self.assertNotIn("dialogExcludes", c)
		c = one(d, "onDialogEvent", target={"kind": "npc", "npcId": 203590}, questState={"status": "START"})
		self.assertNotIn("dialogExcludes", c)                # the path reads no dialog action (DIALOG_OVERLAYS cover it)
		c = one(self.docs[24112], "onDialogEvent", target={"kind": "npc", "npcId": 203631}, dialogAction=use, questState=None)
		self.assertEqual((calls(c), c["dialogExcludes"]), ([("sendQuestStartDialog", [])], [31]))

	def test_2213_registers_a_side_drop_and_a_get_item_event(self):
		# _2213PoisonRootPotentFruit.java:22-29: the talk registration of 203604 twice (QuestNpc keeps it once), the drop, the get-item event
		reg = self.docs[2213]["register"]
		self.assertIn({"call": "addHandlerSideQuestDrop", "args": [2213, 700057, 182203208, 1, 100]}, reg)
		self.assertIn({"call": "registerOnGetItem", "args": [182203208, 2213]}, reg)
		self.assertEqual([r for r in reg if r.get("npc") == 203604 and r["event"] == "addOnTalkEvent"],
		                 [{"npc": 203604, "event": "addOnTalkEvent", "args": [2213]}] * 2)


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class CommandLineTest(unittest.TestCase):
	def run_main(self, argv):
		with contextlib.redirect_stdout(io.StringIO()) as out:
			code = oracle.main(argv)
		return code, out.getvalue()

	def test_generate_and_check(self):
		tmp = Path(tempfile.mkdtemp(prefix="qtrace"))
		try:
			rel = "poeta/_1107TheLostAxe.java"
			code, text = self.run_main(["quest-trace", "generate", "--out", str(tmp), "--only", rel])
			self.assertEqual(code, 0, text)
			self.assertEqual(sorted(f.name for f in tmp.iterdir()), ["1107.json"])
			doc = json.loads((tmp / "1107.json").read_text(encoding="utf-8"))
			self.assertEqual((doc["format"], doc["version"], doc["java"]), ("aion-quest-trace", 1, rel))
			self.assertEqual(self.run_main(["quest-trace", "check", "--expected-dir", str(tmp), "--only", rel])[0], 0)
			(tmp / "1107.json").write_text((tmp / "1107.json").read_text(encoding="utf-8").replace('"returns": false', '"returns": true', 1),
			                               encoding="utf-8")
			code, text = self.run_main(["quest-trace", "check", "--expected-dir", str(tmp), "--only", rel])
			self.assertEqual(code, 1)
			self.assertIn("1107.json: stale against poeta/_1107TheLostAxe.java", text)
			(tmp / "1107.json").unlink()
			(tmp / "9.json").write_text("{}", encoding="utf-8")
			code, text = self.run_main(["quest-trace", "check", "--expected-dir", str(tmp), "--only", rel])
			self.assertEqual(code, 1)
			self.assertIn("1107.json: missing", text)
			self.assertNotIn("9.json", text)                                   # --only checks the named handlers only
			self.assertEqual(extract.check(tmp, (rel,)), ["1107.json: missing (python oracle.py quest-trace generate)",
			                                              "9.json: not in the slice"])
		finally:
			shutil.rmtree(tmp, ignore_errors=True)

	def test_check_only_against_the_committed_traces(self):
		code, text = self.run_main(["quest-trace", "check", "--only", "poeta/_1107TheLostAxe.java"])
		self.assertEqual((code, text.strip()), (0, "quest-trace check: 0 problems"))

	def test_the_oracle_never_imports_the_generator(self):
		# phase6-inventory.md §7.6 item 3: written from Java, not from the port or the generator (only questgen's Java parser, jast)
		code = ("import sys; sys.path.insert(0, sys.argv[1]); from questtrace import extract; "
		        "extract.trace_file(extract.Tables(), extract.QUEST_DIR / 'poeta/_1107TheLostAxe.java', 'x'); "
		        "print(sorted(m for m in sys.modules if m.startswith('questgen')))")
		out = subprocess.run([sys.executable, "-c", code, str(runner.TOOL_DIR)], capture_output=True, text=True, check=True).stdout
		self.assertEqual(out.strip(), "['questgen', 'questgen.jast', 'questgen.paths']")


if __name__ == "__main__":
	unittest.main()
