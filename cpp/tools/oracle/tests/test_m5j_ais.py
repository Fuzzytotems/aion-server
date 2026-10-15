"""M5j AIs oracle (m5j/ais.py, m5j-plan.md §10.4 Z12, §18.4 CP4): the template pick and the AI data rule on hand-made data, and on the real
tree the gate's npcs.

Expected values are derived by hand from NpcTemplate's `ai` attribute, BombAI/SummonerAI.handleSpawned and Ai.java's set answer.
"""

import unittest
import xml.etree.ElementTree as ET

from m5j import ais
from staticdata_oracle import OracleError
from staticdata_oracle import run as runner

GAME_SERVER = runner.DEFAULT_STATIC_DATA.parent.parent


class FakeData:
	def __init__(self, **roots: str):
		self.roots = {root: ET.fromstring(text) for root, text in roots.items()}

	def stream(self, root_tag: str, child_tag: str):
		root = self.roots.get(root_tag)
		return iter([] if root is None else root.findall(child_tag))


NPCS = ('<npc_templates><npc_template npc_id="30" ai="bomb"/><npc_template npc_id="20" ai="bomb"/><npc_template npc_id="10" ai="book"/>'
        '<npc_template npc_id="5" ai="book"/><npc_template npc_id="7"/><npc_template npc_id="40" ai="summoner"/></npc_templates>')
AI_DATA = '<ai_templates><ai npcId="30"><bombs><bomb skillId="1"/></bombs></ai><ai npcId="20"/><ai npcId="40"><bombs/></ai></ai_templates>'


class PickTest(unittest.TestCase):
	def setUp(self):
		self.data = FakeData(npc_templates=NPCS, ai_templates=AI_DATA)

	def test_the_lowest_template_of_each_ai(self):
		self.assertEqual(ais.lowest_templates(self.data), {"bomb": [20, 30], "book": [5, 10], "summoner": [40]})

	def test_the_ai_data_rows_by_child(self):
		self.assertEqual(ais.ai_data_rows(self.data), {"bombs": {30, 40}, "summons": set()})


class RealTreeTest(unittest.TestCase):
	def test_the_gate_npcs(self):
		from m5a.data import StaticData
		report = ais.ais_report(StaticData(runner.DEFAULT_STATIC_DATA), GAME_SERVER / "src")
		by_name = {a["name"]: a for a in report["ais"]}
		self.assertEqual(len(report["ais"]), 23)
		# no template uses these two (m5j-plan.md §2: reached only through //ai set)
		self.assertIsNone(by_name["no_interaction"]["npcId"])
		self.assertIsNone(by_name["customcdreset"]["npcId"])
		# bombs.xml's first row is 281327 but the lowest bomb template with a row is 281212 (bombs.xml:8-12)
		self.assertEqual(by_name["bomb"]["npcId"], 281212)
		self.assertEqual(by_name["hidden_teleporter"]["npcId"], 804811)
		self.assertEqual(sum(1 for a in report["ais"] if a["npcId"] is not None), 21)
		self.assertEqual(report["aiSet"]["baseAi"], "general")
		self.assertEqual(report["aiSet"]["answerPrefix"], "Npc now has AI ")
		# auto_group.xml: the Arena of Chaos 1 (mask 21) on 300350000 for levels 46-50
		self.assertEqual(report["autoGroups"]["21"], {"instanceId": 300350000, "minLvl": 46, "maxLvl": 50})

	def test_a_missing_ai_name_is_refused(self):
		with self.assertRaises(OracleError):
			ais.ai_name(GAME_SERVER / "src", "NoSuchAI")


if __name__ == "__main__":
	unittest.main()
