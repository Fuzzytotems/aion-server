"""The golden quest oracle's chunk-Q03 slice (P6-Q slice 2, 2026-09-29: questtrace.extract.SLICE_Q03, the 76 verteron and heiron handlers
questgen transliterates) and the two oracle fixes it needed, each case below derived by hand from the Java:

- an item hook whose guards leave the item free takes one of the quest items register() names (the only items QuestEngine.onItemUseEvent
  hands the hook), not 0, which is no item: _1561TheMisersMap removes the used item by its template id;
- a path that assumes removeQuestItem(env, id, count) false while a guard bounds the item's count from below by count is dead Java code
  (decreaseByItemId takes the count and answers true): _1535TheColdColdGround's `count > 4 && removeQuestItem(env, id, 5)`.

Run from cpp/tools/oracle: python -m unittest tests.test_quest_trace_q03
"""
from __future__ import annotations

import unittest

from questtrace import extract

from tests.test_quest_trace import HAVE_JAVA_TREE, calls, handler, hook, one, tables, trace_text

ITEM_HOOK = "HandlerResult onItemUseEvent(QuestEnv env, Item item)"


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class Q03SliceTest(unittest.TestCase):
	@classmethod
	def setUpClass(cls):
		cls.docs = {}
		for rel in extract.SLICE_Q03:
			d = extract.trace_file(tables(), extract.QUEST_DIR / rel, rel)
			cls.docs[d["questId"]] = d

	def test_the_slice_is_the_chunks_transliterated_handlers(self):
		java = sorted(f"{d}/{f.name}" for d in ("verteron", "heiron") for f in (extract.QUEST_DIR / d).glob("*.java"))
		hand = {"heiron/_1643TheStarOfHeiron.java", "heiron/_3200PriceOfGoodwill.java"}   # questgen refuses them: hand ports
		self.assertEqual(len(java), 78)
		self.assertEqual(sorted(extract.SLICE_Q03), sorted(set(java) - hand))
		self.assertEqual(len(self.docs), 76)
		self.assertEqual(set(extract.SLICE_Q03) & set(extract.SLICE_TIER_A + extract.SLICE_ROUTE), set())
		# the integration of slice 2 (p6q-ascension-route.md, "Slice 2"): Q10's altgard/pandaemonium slice follows, then (phase 6 step 2,
		# lane C) Q08's gelkmaros/enshar slice
		self.assertEqual(extract.SLICE, extract.SLICE_TIER_A + extract.SLICE_ROUTE + extract.SLICE_Q03 + extract.SLICE_Q10 + extract.SLICE_Q08 +
		                 extract.SLICE_Q01 + extract.SLICE_Q02 + extract.SLICE_Q14 + extract.SLICE_Q13 + extract.SLICE_Q11 + extract.SLICE_Q05)
		# every hook refused in two of them (the golden harness's ORACLE_REFUSES_EVERY_HOOK): 1640 TeleportService.teleportTo, 1647
		# player.getEquipment and spawnForFiveMinutesInFrontOf
		for qid in (1640, 1647):
			self.assertEqual((self.docs[qid]["cases"], [h for h in self.docs[qid]["hooks"] if "unsupported" not in h]), ([], []))

	def test_14050_orders_from_heiron_fortress(self):
		# _14050OrdersFromHeironFortress.java:39-54: only 204500 answers; START QUEST_SELECT sets REWARD and sends 10002, SELECT_QUEST_REWARD
		# sends page 5 (still START: the reward packet fix answers false), REWARD finishes through sendQuestEndDialog
		d = self.docs[14050]
		npc = {"kind": "npc", "npcId": 204500}
		c = one(d, "onDialogEvent", target=npc, questState={"status": "START"}, dialogAction={"name": "QUEST_SELECT", "id": 31})
		self.assertEqual((calls(c), c["returns"]), ([("qs.setStatus", ["REWARD"]), ("updateQuestStatus", []), ("sendQuestDialog", [10002])],
		                                            {"resultOf": 2}))
		c = one(d, "onDialogEvent", target=npc, questState={"status": "START"}, dialogAction={"name": "SELECT_QUEST_REWARD", "id": 1009})
		self.assertEqual(calls(c), [("sendQuestDialog", [5])])
		c = one(d, "onDialogEvent", target=npc, questState={"status": "REWARD"})
		self.assertEqual(calls(c), [("sendQuestEndDialog", [])])

	def test_1561_the_used_item_is_the_registered_scroll(self):
		# _1561TheMisersMap.java:27-46: register() names quest item 182201728; the hook removes the used item (its template id, no guard reads
		# it) and starts the quest when the quest is absent or repeatable
		d = self.docs[1561]
		self.assertIn({"call": "registerQuestItem", "args": [182201728, 1561]}, d["register"])
		c = one(d, "onItemUseEvent", lambda c: c.get("assume") == [{"effect": 2, "returns": True}], questState=None)
		self.assertEqual(c["given"]["item"], {"itemId": 182201728})
		self.assertEqual(calls(c), [("env.setQuestId", [1561]), ("removeQuestItem", [182201728, 1]), ("QuestService.startQuest", [])])
		self.assertEqual(c["returns"], "SUCCESS")

	def test_1197_a_guarded_item_keeps_the_value_the_guard_leaves(self):
		# _1197KrallBook.java:69-76: `id != 182200558` returns UNKNOWN: that path's item is one no guard names (0), the other path's the book
		d = self.docs[1197]
		self.assertEqual([c["given"]["item"] for c in d["cases"] if c["hook"] == "onItemUseEvent"],
		                 [{"itemId": 0}, {"itemId": 182200558}])

	def test_1535_a_removal_the_guard_guarantees_has_no_false_path(self):
		# _1535TheColdColdGround.java:51-65: `abexSkins = count(182201818) > 4`; SETPRO1 with abexSkins removes 5, which cannot fail, so
		# the only SETPRO1 paths are the removal (REWARD, group 0, page 5) and the missing skins (page 1693)
		d = self.docs[1535]
		setpro1 = [c for c in d["cases"] if c["given"].get("dialogAction", {}).get("name") == "SETPRO1"]
		self.assertEqual(sorted(tuple(e[0] for e in calls(c)) for c in setpro1),
		                 [("removeQuestItem", "qs.setRewardGroup", "qs.setStatus", "updateQuestStatus", "sendQuestDialog"),
		                  ("sendQuestDialog",)])
		self.assertFalse([c for c in d["cases"] if any(a == {"effect": 0, "returns": False} for a in c.get("assume", []))
		                  and calls(c)[0][0] == "removeQuestItem"])

	def test_committed_traces_of_the_slice_are_current(self):
		self.assertEqual(extract.check(rels=extract.SLICE_Q03, extra=False), [])


class OracleFixTest(unittest.TestCase):
	"""the two fixes on synthetic handlers"""

	def test_an_unguarded_item_is_a_registered_quest_item(self):
		java = handler(hook("\t\tremoveQuestItem(env, item.getItemTemplate().getTemplateId(), 1);\n\t\treturn HandlerResult.SUCCESS;\n",
		                    ITEM_HOOK)).replace("qe.registerQuestNpc(203001).addOnTalkEvent(questId);",
		                                        "qe.registerQuestNpc(203001).addOnTalkEvent(questId);\n\t\tqe.registerQuestItem(182200777, questId);")
		d = trace_text(java)
		(c,) = d["cases"]
		self.assertEqual((c["given"]["item"], calls(c)), ({"itemId": 182200777}, [("removeQuestItem", [182200777, 1])]))
		# without a registered quest item the value stays the domain's first (0)
		d = trace_text(handler(hook("\t\tremoveQuestItem(env, item.getItemTemplate().getTemplateId(), 1);\n\t\treturn HandlerResult.SUCCESS;\n",
		                            ITEM_HOOK)))
		self.assertEqual(d["cases"][0]["given"]["item"], {"itemId": 0})

	def test_a_removal_is_dead_only_when_the_count_is_bounded_by_it(self):
		body = ("\t\tif (env.getPlayer().getInventory().getItemCountByItemId(182200001) > %d && removeQuestItem(env, 182200001, 5))\n"
		        "\t\t\treturn sendQuestDialog(env, 5);\n\t\treturn sendQuestDialog(env, 1693);\n")
		sig = "boolean onDialogEvent(QuestEnv env)"
		d = trace_text(handler(hook(body % 4, sig)))                 # count >= 5: the removal cannot fail
		self.assertFalse([c for c in d["cases"] if c.get("assume") == [{"effect": 0, "returns": False}]])
		d = trace_text(handler(hook(body % 3, sig)))                 # count >= 4: 4 of them, the removal fails (and takes the 4)
		c = one(d, "onDialogEvent", lambda c: c.get("assume") == [{"effect": 0, "returns": False}])
		self.assertEqual(calls(c), [("removeQuestItem", [182200001, 5]), ("sendQuestDialog", [1693])])


if __name__ == "__main__":
	unittest.main()
