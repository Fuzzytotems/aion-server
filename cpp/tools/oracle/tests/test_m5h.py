"""M5h oracles (m5h-plan.md G-01): legion (m5h/legion.py: the message and question ids by name and the legion enums, read from the Java tree)
and housing (m5h/housing.py: the studio path's npc spots, the Elyos studio, the furniture, PartType and HouseDoorState)."""

import unittest
from pathlib import Path

from staticdata_oracle import OracleError

from m5h.housing import housing_report
from m5h.legion import legion_report

JAVA_SRC = Path(__file__).resolve().parents[4] / "game-server" / "src"
STATIC_DATA = Path(__file__).resolve().parents[4] / "game-server" / "data" / "static_data"


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


@unittest.skipUnless(JAVA_SRC.is_dir() and STATIC_DATA.is_dir(), "needs the Java tree and its static data")
class HousingReportTest(unittest.TestCase):

	def test_the_studio_path(self):
		report = housing_report(STATIC_DATA, JAVA_SRC, ["STR_MSG_HOUSING_INS_OWN_SUCCESS"], [(700010000, 730517), (720010000, 830229)],
		                        [170190034, 171110000])
		self.assertEqual(report["messages"], {"STR_MSG_HOUSING_INS_OWN_SUCCESS": 1401275})
		entrance = report["npcs"]["730517"]
		self.assertEqual((entrance["x"], entrance["y"], entrance["talkDelayMs"], entrance["ai"]), (2583.113, 1963.681, 2000, "studioportal"))
		self.assertEqual(report["npcs"]["830229"]["map"], 720010000)
		studio = report["studio"]
		self.assertEqual((studio["address"], studio["map"], studio["exitMap"], studio["building"], studio["goldPrice"]),
		                 (2001, 720010000, 700010000, 355000, 4000000))
		self.assertEqual((studio["managerNpc"], studio["teleportNpc"]), (810021, 810003))
		self.assertEqual(studio["houseNpcs"]["TELEPORT"], {"x": 360.6813, "y": 294.28326, "z": 222.3526})
		cake = report["items"]["170190034"]
		self.assertEqual(cake["houseObject"], 3190034)
		self.assertEqual((cake["template"]["kind"], cake["template"]["limit"], cake["template"]["rewardId"], cake["template"]["cooldownSeconds"]),
		                 ("use_item", "COOKING", 160010196, 10))
		self.assertEqual(report["items"]["171110000"]["decoration"], 3554000)
		self.assertEqual([(p["name"], p["startLine"], p["rooms"]) for p in report["partTypes"]][6:8], [("INWALL_ANY", 8, 6), ("INFLOOR_ANY", 14, 6)])
		self.assertEqual(sum(p["rooms"] for p in report["partTypes"]), 19)
		self.assertEqual(report["doorStates"], {"OPEN": 1, "CLOSED_EXCEPT_FRIENDS": 2, "CLOSED": 3})
		self.assertEqual(report["scripts"]["padding"], [205] * 8)

	def test_an_npc_without_spawn_on_the_map_is_an_error(self):
		with self.assertRaises(OracleError):
			housing_report(STATIC_DATA, JAVA_SRC, [], [(720010000, 730517)], [])


if __name__ == "__main__":
	unittest.main()
