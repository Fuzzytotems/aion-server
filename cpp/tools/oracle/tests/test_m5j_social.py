"""M5j social oracle (m5j/social.py, m5j-plan.md §10.4, §18.1): the PvP AP arithmetic on hand-made ranks and configurations, and on the real
Java tree the ids, titles, Daeva seeds and the gate's kill.

Expected values are derived by hand from StatFunctions.calculatePvPApLost / calculatePvpApGained, PvpService.rewardPlayerTeam, Rates and
AbyssRankEnum.getRankForPoints, in Java float arithmetic, and repeated per case.
"""

import unittest
from types import SimpleNamespace

from m5j import social as s
from staticdata_oracle import run as runner

GAME_SERVER = runner.DEFAULT_STATIC_DATA.parent.parent


class FakeRanks(s.AbyssRanks):
	def __init__(self):  # GRADE9..7 of AbyssRankEnum.java and one officer rank (GP only)
		self.ranks = [("G9", 1, 300, 90, 0, 0), ("G8", 2, 345, 103, 1200, 0), ("G7", 3, 396, 118, 4220, 0), ("O1", 10, 1557, 467, 0, 1244)]


def config(gain=(1.0, 2.0), loss=(1.0, 1.0), maxkills=5, cap=False):
	v = lambda value: SimpleNamespace(value=value)
	return {"gameserver.rates.ap.pvp.gain": v(list(gain)), "gameserver.rates.ap.pvp.loss": v(list(loss)), "gameserver.pvp.maxkills": v(maxkills),
	        "gameserver.enable.ap.cap": v(cap)}


class RankTest(unittest.TestCase):
	def test_rank_for_points_ignores_the_gp_ranks_without_gp(self):
		ranks = FakeRanks()
		self.assertEqual(ranks.for_points(0)[1], 1)
		self.assertEqual(ranks.for_points(1199)[1], 1)
		self.assertEqual(ranks.for_points(1200)[1], 2)
		self.assertEqual(ranks.for_points(999999)[1], 3)
		self.assertEqual(ranks.for_points(999999, 1244)[1], 10)


class PvpTest(unittest.TestCase):
	def test_lost_level_penalty(self):
		ranks = FakeRanks()
		self.assertEqual(s.ap_lost(ranks, 0, 10, 10, [1.0], 0), 90)
		self.assertEqual(s.ap_lost(ranks, 0, 10, 13, [1.0], 0), 77)   # Math.round(90 * 0.85f) = round(76.50000214...) = 77
		self.assertEqual(s.ap_lost(ranks, 0, 10, 14, [1.0], 0), 58)   # 90 * 0.65f is the float 58.499996..., Math.round 58
		self.assertEqual(s.ap_lost(ranks, 0, 10, 20, [1.0], 0), 9)
		self.assertEqual(s.ap_lost(ranks, 0, 13, 10, [1.0], 0), 90)   # a lower winner: no penalty
		self.assertEqual(s.ap_lost(ranks, 0, 10, 10, [1.0, 0.5], 1), 45)  # the membership's rate

	def test_gained_level_and_rank_arms(self):
		ranks = FakeRanks()
		self.assertEqual(s.ap_gained_base(ranks, 0, 10, 1, 10), 300)
		self.assertEqual(s.ap_gained_base(ranks, 0, 10, 1, 13), 255)
		self.assertEqual(s.ap_gained_base(ranks, 0, 10, 1, 14), 195)
		self.assertEqual(s.ap_gained_base(ranks, 0, 10, 1, 15), 30)
		self.assertEqual(s.ap_gained_base(ranks, 0, 10, 1, 8), 330)
		self.assertEqual(s.ap_gained_base(ranks, 0, 10, 1, 7), 360)
		self.assertEqual(s.ap_gained_base(ranks, 0, 10, 1, 6), 390)
		self.assertEqual(s.ap_gained_base(ranks, 0, 10, 1, 9), 300)   # -1: no arm
		# winner rank 3 against a rank-1 victim: 300 - Math.round(300 * (2 * 0.05f)) = 300 - 30
		self.assertEqual(s.ap_gained_base(ranks, 0, 10, 3, 10), 270)
		# a rank above 7 takes no penalty
		self.assertEqual(s.ap_gained_base(ranks, 0, 10, 10, 10), 300)

	def test_the_solo_kill(self):
		kill = s.solo_kill(FakeRanks(), config(), 1000, 10, 0, 13, 0)
		self.assertEqual(kill["victim"]["apLost"], 77)
		self.assertEqual(kill["victim"]["apAfter"], 923)
		self.assertEqual(kill["winner"]["apGained"], 255)
		self.assertEqual(kill["winner"]["apAfter"], 255)
		# the loss floors at 0 (AbyssRank.addAp) and the rank follows the AP
		self.assertEqual(s.solo_kill(FakeRanks(), config(), 50, 10, 0, 10, 0)["victim"]["apAfter"], 0)
		self.assertEqual(s.solo_kill(FakeRanks(), config(), 1250, 10, 0, 10, 0)["victim"]["rankAfter"], 1)
		# the membership's gain rate, and the kill limit's 1 AP
		self.assertEqual(s.solo_kill(FakeRanks(), config(), 1000, 10, 0, 10, 1)["winner"]["apGained"], 600)
		self.assertEqual(s.solo_kill(FakeRanks(), config(maxkills=1), 1000, 10, 0, 10, 0)["winner"]["apGained"], 1)

	def test_the_cap_is_refused(self):
		with self.assertRaises(s.OracleError):
			s.solo_kill(FakeRanks(), config(cap=True), 1000, 10, 0, 10, 0)


class RealTreeTest(unittest.TestCase):
	@classmethod
	def setUpClass(cls):
		if not (GAME_SERVER / "config" / "main" / "custom.properties").is_file():
			raise unittest.SkipTest("the Java game-server tree is not beside the static data")
		from m5a.data import StaticData
		cls.report = s.social_report(StaticData(runner.DEFAULT_STATIC_DATA), GAME_SERVER / "src", GAME_SERVER / "config", None, [],
		                             ["STR_YOU_EXCLUDED", "STR_CANT_WHISPER_LEVEL"], ["STR_BUDDYLIST_ADD_BUDDY_REQUEST"], [10, 13], (1000, 10, 0, 13), 0)

	def test_ids(self):
		self.assertEqual(self.report["messages"], {"STR_YOU_EXCLUDED": 1300628, "STR_CANT_WHISPER_LEVEL": 1310004})
		self.assertEqual(self.report["questions"], {"STR_BUDDYLIST_ADD_BUDDY_REQUEST": 1401498})

	def test_config_and_ranks(self):
		self.assertEqual(self.report["config"]["gameserver.chat.whisper.level"]["value"], 10)
		self.assertEqual(self.report["config"]["gameserver.search.player.level"]["value"], 10)
		self.assertEqual(self.report["abyssRanks"][0], {"name": "GRADE9_SOLDIER", "id": 1, "pointsGained": 300, "pointsLost": 90, "requiredAP": 0,
		                                                "requiredGP": 0})
		self.assertEqual(len(self.report["abyssRanks"]), 18)

	def test_titles_and_daeva(self):
		self.assertEqual(self.report["titles"]["ELYOS"][0], {"id": 1, "nameId": 1100900, "race": "ELYOS"})
		self.assertEqual(self.report["daeva"]["ascensionQuests"], {"ELYOS": 1006, "ASMODIANS": 2008})
		self.assertEqual(self.report["daeva"]["byLevel"]["10"], {"exp": 126069, "levelWithoutQuest": 9})

	def test_the_gate_kill(self):
		self.assertEqual(self.report["pvpKill"]["victim"]["apAfter"], 923)
		self.assertEqual(self.report["pvpKill"]["winner"]["apAfter"], 255)


if __name__ == "__main__":
	unittest.main()
