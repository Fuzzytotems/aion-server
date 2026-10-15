"""M5h legion oracle (m5h/legion.py, m5h-plan.md G-01): the message and question ids by name and the legion enums, read from the Java tree."""

import unittest
from pathlib import Path

from staticdata_oracle import OracleError

from m5h.legion import legion_report

JAVA_SRC = Path(__file__).resolve().parents[4] / "game-server" / "src"


@unittest.skipUnless(JAVA_SRC.is_dir(), "needs the Java tree")
class LegionReportTest(unittest.TestCase):

	def test_ids_enums_and_service_constants(self):
		report = legion_report(JAVA_SRC, ["STR_GUILD_CREATED", "STR_GUILD_NOTICE"], ["STR_GUILD_INVITE_DO_YOU_ACCEPT_INVITATION"])
		self.assertEqual(report["messages"], {"STR_GUILD_CREATED": 1300235, "STR_GUILD_NOTICE": 1400019})
		self.assertEqual(report["questions"], {"STR_GUILD_INVITE_DO_YOU_ACCEPT_INVITATION": 80001})
		self.assertEqual(report["ranks"]["VOLUNTEER"], 4)
		self.assertEqual(report["historyActions"]["DEFENSE"], {"id": 11, "type": "REWARD"})
		self.assertEqual(report["historyActions"]["KINAH_WITHDRAW"], {"id": 18, "type": "WAREHOUSE"})
		self.assertEqual(report["permissionMasks"]["WH_DEPOSIT"], 0x1000)
		self.assertEqual(report["emblemChunkSize"], 7993)
		self.assertEqual(report["announcementLimit"], 256)
		self.assertEqual(report["chatTypes"]["LEGION"], 10)

	def test_an_unknown_name_is_an_error(self):
		with self.assertRaises(OracleError):
			legion_report(JAVA_SRC, ["STR_NO_SUCH_MESSAGE_EVER"], [])


if __name__ == "__main__":
	unittest.main()
