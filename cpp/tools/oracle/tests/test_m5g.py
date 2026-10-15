"""M5g team oracle (m5g/, m5g-plan.md H-01, D8): the experience share of PlayerTeamDistributionService.doReward on hand-made numbers, the
acceptance checks of the level search, and on the real data the gate's npc and levels.

Expected values are derived by hand from PlayerTeamDistributionService.java:35-84 and Rates.java:21-27 and repeated per case.
"""

import unittest

from m5a.javafloat import f32
from m5g.team import acceptable, share
from staticdata_oracle import run as runner


class FakeRules:
	"""TeamRules with a fixed reward and a fixed experience table"""

	def __init__(self, reward_by_highest: dict[int, int], table: list[int]):
		self.reward_by_highest = reward_by_highest
		self.table = table

	def reward(self, highest_level: int) -> int:
		return self.reward_by_highest[highest_level]


# start exp of levels 1..6: level n needs table[n] - table[n-1]
TABLE = [0, 400, 1430, 3815, 9049, 19405]


class M5gShareTest(unittest.TestCase):
	def test_level_split_rounds_the_float_quotient(self):
		rules = FakeRules({4: 598}, TABLE)
		# Math.round(598 * 3 / 10f) = Math.round(179.4f) = 179; * 1.5 = 268.5 -> (long) 268; cap (3815 - 1430) * 0.2f = 477
		a = share(rules, [3, 3, 4], 0, [True, True, True], 1.5)
		self.assertEqual(a["rewardXp"], 179)
		self.assertEqual(a["awarded"], 268)
		self.assertAlmostEqual(a["cap"], f32((3815 - 1430) * f32(0.2)), places=3)
		self.assertFalse(a["capped"])
		# C: Math.round(598 * 4 / 10f) = Math.round(239.2f) = 239; * 1.5 = 358.5 -> 358
		c = share(rules, [3, 3, 4], 2, [True, True, True], 1.5)
		self.assertEqual(c["awarded"], 358)
		# an even split would pay Math.round(598 / 3f) = 199 * 1.5 = 298 to everyone
		self.assertEqual(a["evenSplitAwarded"], 298)

	def test_out_of_range_members_are_not_counted(self):
		rules = FakeRules({3: 598}, TABLE)
		self.assertIsNone(share(rules, [3, 3, 4], 2, [True, True, False], 1.5))
		# highest 3, sum 6: Math.round(598 * 3 / 6f) = 299 * 1.5 = 448 (cap 477)
		self.assertEqual(share(rules, [3, 3, 4], 0, [True, True, False], 1.5)["awarded"], 448)

	def test_ten_levels_below_the_highest_get_nothing(self):
		rules = FakeRules({12: 1000}, TABLE + [0] * 10)
		low = share(rules, [2, 12], 0, [True, True], 1.0)
		self.assertEqual(low["rewardXp"], 0)
		self.assertEqual(low["awarded"], 0)

	def test_the_cap_limits_the_share(self):
		rules = FakeRules({1: 1000}, TABLE)
		# level 1: cap 400 * 0.2f = 80
		capped = share(rules, [1, 1], 0, [True, True], 1.0)
		self.assertTrue(capped["capped"])
		self.assertEqual(capped["awarded"], 80)

	def test_acceptable_names_capped_and_indistinguishable_shares(self):
		member = {"capped": True, "awarded": 80, "soloRateAwarded": 80, "evenSplitAwarded": 80, "level": 1}
		other = dict(member, level=2)
		problems = acceptable([{"members": [member, other]}], [0])
		self.assertIn("kill 1: member 0 is at his cap", problems)
		self.assertIn("kill 1: member 0's share is the same at the solo rate", problems)
		self.assertIn("kill 1: member 0's share is the same as an even split", problems)
		# equal levels: an even split is the level split, not a problem
		self.assertNotIn("kill 1: member 0's share is the same as an even split", acceptable([{"members": [member, member]}], [0]))


@unittest.skipUnless((runner.DEFAULT_STATIC_DATA.parent.parent / "src" / "com" / "aionemu" / "gameserver").is_dir(), "the Java tree is absent")
class M5gRealDataTest(unittest.TestCase):
	def test_the_gates_levels_for_the_juvenile_sparkie(self):
		from m5a.data import StaticData
		from m5g.team import team_report
		report = team_report(StaticData(runner.DEFAULT_STATIC_DATA), runner.DEFAULT_STATIC_DATA.parent.parent / "src", 210663, None, None, 1.5, 1.0,
		                     ["STR_PARTY_INVITED_HIM"], ["STR_PARTY_DO_YOU_ACCEPT_INVITATION"])
		self.assertEqual(report["levels"], [3, 3, 4], "D8's search: A = B = 3, C = 4 (m5g-plan.md's working assumption)")
		self.assertEqual(report["problems"], [])
		first = report["kills"][0]["members"]
		self.assertEqual([m["awarded"] for m in first], [268, 268, 358])
		self.assertEqual(report["kills"][2]["members"][2], None)
		constants = report["constants"]
		self.assertEqual(constants["lootGroupRulesDefault"]["lootRule"], 1)
		self.assertEqual(constants["teamTypes"]["GROUP"], {"type": 0x3F, "subType": 0})
		self.assertEqual(constants["groupEvents"]["UPDATE_EFFECTS"], 65)
		self.assertEqual(constants["messages"]["STR_PARTY_INVITED_HIM"], 1300173)
		self.assertEqual(constants["questions"]["STR_PARTY_DO_YOU_ACCEPT_INVITATION"], 60000)
		self.assertEqual(constants["groupMaxDistance"], 100)


if __name__ == "__main__":
	unittest.main()
