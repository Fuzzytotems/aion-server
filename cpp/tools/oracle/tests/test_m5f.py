"""M5f travel oracle (m5f/, m5f-plan.md G-01): the arithmetic on hand-made values (PricesService's truncations, the hotspot's float distance,
the instance entrance cooltime across its reset time), then the gate's numbers on the real data - m5f-plan.md §2.9's sanity list, re-derived
here and pinned: Daines, Kustanon, Aero, Urakron, Osmar, Ukin, the obelisks, hotspot 13 from the Elyos spawn, Haramel's portal, cooltime and
exit, its spawnInstance set near the entry, the level-16 start exp; the census on a mock C++ tree and the geo check on two maps.

Everything that reads the Java source tree or the static data is skipped without it, like the M5d/M5e tests.
"""

import contextlib
import io
import json
import tempfile
import time
import unittest
from pathlib import Path

from m5a.data import StaticData
from m5a.javafloat import f32
from m5a.spawns import GameClock
from m5f import travel
from m5f.census import CppEffects, census_report
from staticdata_oracle import OracleError
from staticdata_oracle import run as runner

from geo import run as geo_run

import oracle

JAVA_SRC = runner.TOOL_DIR.parents[2] / "game-server" / "src"
JAVA_QUEST_HANDLERS = runner.TOOL_DIR.parents[2] / "game-server" / "data" / "handlers" / "quest"
CONFIG = runner.TOOL_DIR.parents[2] / "game-server" / "config"
CPP_GAME_SERVER = runner.TOOL_DIR.parents[1] / "game-server"
HAVE_JAVA_TREE = (JAVA_SRC / "com" / "aionemu" / "gameserver").is_dir() and runner.DEFAULT_STATIC_DATA.is_dir()

# m5c §2.10's siege-off factors: global prices 125 %, modifier 100 %, taxes 113 %
PRICES = {"globalPrices": 125, "globalPricesModifier": 100, "taxes": 113}
ELYOS_SPAWN = (1212.9423, 1044.8516, 140.75568)  # player_initial_data.xml's Elyos spawn (m5f-plan.md §2.9)
DAY_MS = 86_400_000


class M5fPriceTest(unittest.TestCase):
	"""PricesService.getPriceForService (PricesService.java:86-90) and BindPointTeleportService.calculateTeleportationPrice (:81-85)"""

	def test_service_price_truncates_after_each_factor(self):
		self.assertEqual(travel.service_price(800, PRICES), 1130, "800 * 1.25 = 1000, * 1.00, * 1.13 = 1130")
		self.assertEqual(travel.service_price(100, PRICES), 141, "125 * 1.13 = 141.25, truncated")
		self.assertEqual(travel.service_price(160, PRICES), 226, "200 * 1.13 = 226")
		self.assertEqual(travel.service_price(1700, PRICES), 2401, "2125 * 1.13 = 2401.25")
		self.assertEqual(travel.service_price(500, PRICES), 706, "625 * 1.13 = 706.25")
		self.assertEqual(travel.service_price(3, PRICES), 3, "(long) 3.75 = 3, then (long) 3.39 = 3: a truncation per factor (4 unrounded)")

	def test_hotspot_price_truncates_the_distance_cost_and_floors_at_one(self):
		self.assertEqual(travel.hotspot_price(44, 451.8074119577943), 63, "44 + (long) 19.879")
		self.assertEqual(travel.hotspot_price(44, 0.0), 44)
		self.assertEqual(travel.hotspot_price(0, 0.0), 1, "Math.max(1, ...)")
		self.assertEqual(travel.hotspot_price(387, 999.9999), 773, "387 + (long) 386.99996")

	def test_hotspot_13_from_the_elyos_spawn(self):
		p = tuple(f32(v) for v in ELYOS_SPAWN)
		d = travel.hotspot_distance(*p, 807.0, 1242.0, 119.0)
		self.assertAlmostEqual(d, 451.807, places=3)
		self.assertEqual(travel.hotspot_price(44, d), 63)
		self.assertEqual(travel.hotspot_price(44, travel.hotspot_distance_double(*p, 807.0, 1242.0, 119.0)), 63,
		                 "the gate's own vector does not tell float from double (m5f-plan.md §10.4: T-08's unit vector must)")

	def test_float_distance_differs_from_a_double_one(self):
		# found by a random search over Poeta's hotspot 13: the float sum of squares (PositionUtil.java:223-230) is 1363.63636098... m away,
		# the double one 1363.63636574... - across the 1000/44 boundary of the distance cost
		p = (1153.5513916015625, 2559.78173828125, 172.45616149902344)
		hotspot = (807.0, 1242.0, 119.0)
		self.assertEqual(travel.hotspot_price(44, travel.hotspot_distance(*p, *hotspot)), 103)
		self.assertEqual(travel.hotspot_price(44, travel.hotspot_distance_double(*p, *hotspot)), 104)
		# and one where the double is the lower price (Verteron's hotspot 19, base 387)
		p = (680.7521362304688, 1533.4130859375, 163.81661987304688)
		self.assertEqual(travel.hotspot_price(387, travel.hotspot_distance(*p, 1643.0, 1500.0, 120.0)), 759)
		self.assertEqual(travel.hotspot_price(387, travel.hotspot_distance_double(*p, 1643.0, 1500.0, 120.0)), 760)


class M5fCooltimeTest(unittest.TestCase):
	"""InstanceCooltimeData.calculateInstanceEntranceCooltime (InstanceCooltimeData.java:70-110)"""

	DAILY = {"type": "DAILY", "typeValue": None, "entCoolTime": 900, "maxCount": 16}
	UTC = travel.Zone("UTC")
	DAY = 1_790_000_000_000 // DAY_MS * DAY_MS  # a UTC midnight (2026-09-21)

	def test_daily_before_at_and_after_the_reset(self):
		nine = self.DAY + 9 * 3_600_000
		self.assertEqual(travel.entrance_cooltime_ms(self.DAILY, nine - 1, self.UTC), nine, "08:59:59.999: today's 09:00")
		self.assertEqual(travel.entrance_cooltime_ms(self.DAILY, nine, self.UTC), nine, "09:00:00.000 is not after 09:00: today's, now")
		self.assertEqual(travel.entrance_cooltime_ms(self.DAILY, nine + 1, self.UTC), nine + DAY_MS, "09:00:00.001: tomorrow's 09:00")
		self.assertEqual(travel.entrance_cooltime_ms(self.DAILY, self.DAY + DAY_MS - 1, self.UTC), nine + DAY_MS)

	def test_a_fixed_offset_moves_the_reset(self):
		plus2 = travel.Zone("+02:00")
		nine_local = self.DAY + 7 * 3_600_000  # 09:00 at +02:00 is 07:00 UTC
		self.assertEqual(travel.entrance_cooltime_ms(self.DAILY, nine_local - 1, plus2), nine_local)
		self.assertEqual(travel.entrance_cooltime_ms(self.DAILY, nine_local + 1, plus2), nine_local + DAY_MS)

	def test_the_local_zone_resets_at_nine_local_time(self):
		local = travel.Zone("local")
		now = int(time.time() * 1000)
		reuse = travel.entrance_cooltime_ms(self.DAILY, now, local)
		t = time.localtime(reuse // 1000)
		self.assertEqual((t.tm_hour, t.tm_min, t.tm_sec, reuse % 1000), (9, 0, 0, 0))
		self.assertTrue(now <= reuse <= now + DAY_MS + 3_600_000)

	def test_weekly_and_relative_and_none(self):
		weekly = {"type": "WEEKLY", "typeValue": "Wed,Sun", "entCoolTime": 900, "maxCount": 1}
		# self.DAY is a Monday: the next 09:00 is Monday's, then +2 days to Wednesday
		self.assertEqual(time.gmtime(self.DAY // 1000).tm_wday, 0)
		self.assertEqual(travel.entrance_cooltime_ms(weekly, self.DAY, self.UTC), self.DAY + 2 * DAY_MS + 9 * 3_600_000)
		self.assertEqual(travel.days_until_reset("Wed,Sun", 7), 0)
		self.assertEqual(travel.days_until_reset("Tue", 5), 4, "(7 - 5) + 2")
		with self.assertRaises(OracleError):
			travel.days_until_reset("Wed,Funday", 1)
		relative = {"type": "RELATIVE", "typeValue": None, "entCoolTime": 30, "maxCount": 1}
		self.assertEqual(travel.entrance_cooltime_ms(relative, 1000, self.UTC), 1000 + 1_800_000)
		self.assertEqual(travel.entrance_cooltime_ms(dict(relative, entCoolTime=0), 1000, self.UTC), 0, "unlimited: not stored")
		self.assertEqual(travel.entrance_cooltime_ms(dict(relative, entCoolTime=40000), 0, self.UTC), 40000 * 60 * 1000 - 2**32,
		                 "minutes * 60 * 1000 is int arithmetic in Java and overflows")
		self.assertEqual(travel.entrance_cooltime_ms(dict(self.DAILY, maxCount=0), 1000, self.UTC), 0)
		self.assertEqual(travel.entrance_cooltime_ms(None, 1000, self.UTC), 0)

	def test_a_zone_name_without_a_database_is_refused_or_resolved(self):
		try:
			zone = travel.Zone("Europe/Berlin")
		except OracleError:
			return  # this machine has no IANA database for zoneinfo: refused, not guessed
		self.assertIsNotNone(zone.tz)


class M5fCensusMockTreeTest(unittest.TestCase):
	"""CppEffects: what counts as ported (m5f-plan.md §12: a generated data-only class is ported)"""

	def test_states(self):
		with tempfile.TemporaryDirectory() as tmp:
			src = Path(tmp) / "src" / "aion" / "gameserver" / "skillengine" / "effect"
			gen = Path(tmp) / "generated" / "aion" / "gameserver" / "skillengine" / "effect"
			src.mkdir(parents=True)
			gen.mkdir(parents=True)
			(src / "AEffect.cpp").write_text("void f() { AION_UNPORTED(); }\nvoid g() { AION_UNPORTED(); }\n", encoding="utf-8")
			(src / "BEffect.cpp").write_text("void f() {}\n", encoding="utf-8")
			(src / "CEffect.cpp").write_text("void f() { AION_PARTIAL(\"x\"); }\n", encoding="utf-8")
			(gen / "DEffect.h").write_text("", encoding="utf-8")
			cpp = CppEffects(Path(tmp))
			self.assertEqual([cpp.state(c) for c in ("AEffect", "BEffect", "CEffect", "DEffect", "EEffect")],
			                 ["unported", "ported", "partial", "ported", "missing"])
			self.assertEqual(cpp.unported_sites("AEffect"), 2)
			self.assertEqual(CppEffects(Path(tmp) / "src").src, src, "--cpp-src may name the src directory too")
			with self.assertRaises(OracleError):
				CppEffects(Path(tmp) / "nowhere")


@unittest.skipUnless(HAVE_JAVA_TREE, "the Java source tree or the static data is not present")
class M5fTravelDataTest(unittest.TestCase):
	"""m5f-plan.md §2.9 on the real data"""

	@classmethod
	def setUpClass(cls):
		cls.td = travel.TravelData(StaticData(runner.DEFAULT_STATIC_DATA), JAVA_SRC)
		cls.daeva_only = travel.daeva_only_npcs(JAVA_SRC)
		cls.prices = {}

	def prices_for(self, race):
		if race not in self.prices:
			self.prices[race] = travel.load_prices(JAVA_SRC, CONFIG, None, False, race)
		return self.prices[race]

	def npc(self, npc_id, race=None):
		return travel.npc_report(self.td, npc_id, race, self.prices_for, self.daeva_only)

	@staticmethod
	def loc(report, loc_id):
		return next(l for l in report["locations"] if l["locId"] == loc_id)

	def test_the_daeva_gate_of_the_dialog(self):
		self.assertEqual(sorted(self.daeva_only), [203194, 203679], "DialogService.java:187-197")

	def test_daines(self):
		r = self.npc(203194)
		self.assertEqual((r["name"], r["ai"], r["race"], r["daevaOnly"], r["teleporter"]), ("daines", "general", "ELYOS", True,
		                                                                                     {"teleportId": 2, "type": "REGULAR"}))
		self.assertEqual([(s["map"], s["x"], s["y"], s["z"]) for s in r["spots"]], [(210010000, f32(804.924), f32(1244.6), f32(118.986))])
		verteron = self.loc(r, 4)
		self.assertEqual((verteron["type"], verteron["price"], verteron["servicePrice"], verteron["requiredQuest"], verteron["map"]),
		                 ("REGULAR", 800, 1130, 0, 210030000))
		self.assertEqual((verteron["x"], verteron["y"], verteron["z"], verteron["heading"]), (f32(1640.76), f32(1500.32), f32(119.70999), 0))
		sanctum = self.loc(r, 2)
		self.assertEqual((sanctum["price"], sanctum["servicePrice"], sanctum["requiredQuest"], sanctum["map"]), (100, 141, 1006, 110010000))

	def test_the_flight_masters(self):
		kustanon = self.npc(203070)
		self.assertFalse(kustanon["daevaOnly"])
		self.assertEqual(kustanon["teleporter"], {"teleportId": 103, "type": "FLIGHT"})
		loc = self.loc(kustanon, 13)
		self.assertEqual((loc["type"], loc["teleportId"], loc["price"], loc["servicePrice"], loc["map"], loc["hasPosition"]),
		                 ("FLIGHT", 5001, 160, 226, 210010000, False))
		aero = self.npc(203083)
		loc = self.loc(aero, 12)
		self.assertEqual((loc["teleportId"], loc["servicePrice"]), (6001, 226))

	def test_the_other_teleporters(self):
		self.assertEqual(self.loc(self.npc(203091), 2)["servicePrice"], 706, "Urakron -> Sanctum 500")
		self.assertEqual(self.loc(self.npc(203091), 2)["requiredQuest"], 0)
		osmar = self.npc(203679)
		self.assertEqual((osmar["race"], osmar["daevaOnly"]), ("ASMODIANS", True))
		altgard = self.loc(osmar, 9)
		self.assertEqual((altgard["servicePrice"], altgard["map"], altgard["heading"]), (1130, 220030000, 60))
		morheim = self.loc(self.npc(203581), 10)
		self.assertEqual((morheim["price"], morheim["servicePrice"], morheim["map"]), (1700, 2401, 220020000))
		self.assertEqual((morheim["x"], morheim["y"], morheim["z"]), (f32(309.53), f32(2271.51), f32(449.41266)))

	def test_a_raceless_npc_needs_a_race(self):
		with self.assertRaises(OracleError):
			self.npc(730318)

	def test_obelisks(self):
		r = travel.obelisk_report(self.td, 700014)
		self.assertEqual((r["ai"], r["bindPoint"]["price"]), ("resurrect", 143))
		self.assertEqual([(s["map"], s["x"]) for s in r["spots"]], [(210010000, f32(423.109))])
		self.assertEqual(travel.obelisk_report(self.td, 700013)["bindPoint"]["price"], 47)
		with self.assertRaises(OracleError):
			travel.obelisk_report(self.td, 203194)

	def test_hotspots(self):
		r = travel.hotspot_report(self.td, 13, ELYOS_SPAWN)
		self.assertEqual((r["map"], r["race"], r["x"], r["y"], r["z"], r["basePrice"], r["price"]), (210010000, "ELYOS", 807.0, 1242.0, 119.0, 44, 63))
		self.assertAlmostEqual(r["distance"], 451.807, places=3)
		self.assertEqual(travel.hotspot_report(self.td, 19, (1643.0, 1500.0, 120.0))["price"], 387, "no distance, no cost")
		with self.assertRaises(OracleError):
			travel.hotspot_report(self.td, 99999, ELYOS_SPAWN)

	def test_haramel_portal(self):
		utc = travel.Zone("UTC")
		now = 1_790_000_000_000  # 2026-09-21 14:13:20 UTC
		r = travel.portal_report(self.td, 730318, "ELYOS", now, utc)
		self.assertEqual((r["ai"], r["talkDelayMs"], r["talkDistance"]), ("portal", 3000, 5))
		self.assertEqual([(s["map"], s["x"], s["y"]) for s in r["spots"]], [(210030000, f32(2539.3267), f32(834.8696))])
		path = r["paths"][0]
		self.assertEqual((path["locId"], path["map"], path["instance"], path["x"], path["y"], path["z"], path["heading"]),
		                 (3002000, 300200000, True, 172.0, 20.0, f32(144.22548), 60))
		self.assertEqual((path["selected"], path["maxPlayers"], path["enterMinLevel"], path["errLevel"]), (True, 1, 16, 27))
		c = r["cooltime"]
		self.assertEqual((c["id"], c["type"], c["entCoolTime"], c["maxCount"], c["maxMemberLight"], c["maxMemberDark"], c["enterMinLevelLight"]),
		                 (46, "DAILY", 900, 16, 1, 1, 16))
		self.assertEqual(r["reuseTimeMs"], now // DAY_MS * DAY_MS + DAY_MS + 9 * 3_600_000, "after 09:00: tomorrow's 09:00")
		self.assertEqual(r["reuseRemainingSeconds"], 67_600)
		self.assertEqual(r["exit"], {"instance": 300200000, "map": 210030000, "race": "ELYOS", "x": f32(2533.8564), "y": f32(835.055),
		                             "z": f32(103.967476), "heading": 59})
		asmo = travel.portal_report(self.td, 730318, "ASMODIANS", now, utc)
		self.assertTrue(asmo["paths"][0]["selected"], "getPortalUsePath returns the last path for the race error")
		self.assertEqual(asmo["exit"]["map"], 220030000)

	def test_instance_exit(self):
		self.assertEqual(travel.instance_exit_report(self.td, 300200000, "ASMODIANS")["exit"]["heading"], 36)
		self.assertIsNone(travel.instance_exit_report(self.td, 210010000, "ELYOS")["exit"])

	def test_haramel_spawn_instance_near_the_entry(self):
		r = travel.instance_spawns_report(self.td, 300200000, 0, (172.0, 20.0, 144.22548), 70.0, GameClock())
		by_npc = {}
		for s in r["spots"]:
			by_npc.setdefault(s["npcId"], []).append(s)
		exit_portal = by_npc[730320][0]
		self.assertEqual((exit_portal["ai"], exit_portal["fixed"], exit_portal["distance"]), ("portal", True, 13.739))
		self.assertEqual(by_npc[216899][0]["distance"], 31.536, "the nearest aggressive npc (§2.9: 31.5 m)")
		self.assertEqual(by_npc[216897][0]["distance"], 61.93, "Drudgelord Kakiti (§2.9: 61.9 m)")
		nearest_aggressive = min(s["distance"] for s in r["spots"] if s["ai"] == "aggressive")
		self.assertEqual(nearest_aggressive, 31.536)
		self.assertEqual((r["total"]["npcSpots"], r["total"]["npcNpcIds"]), (117, 42), "W-11: 117 spots of 42 npc ids")
		self.assertEqual(r["total"]["certain"], r["total"]["spots"], "no pool, no temporary spawn: every spot is certain")

	def test_exp_for_level(self):
		self.assertEqual(travel.exp_report(self.td, 16)["exp"], 844378)
		self.assertEqual(travel.exp_report(self.td, 0)["exp"], 0)
		with self.assertRaises(OracleError):
			travel.exp_report(self.td, 99)

	def test_the_command_line(self):
		out = io.StringIO()
		with contextlib.redirect_stdout(out):
			code = oracle.main(["m5f-travel", "--no-profile", "--npc", "203194", "--hotspot", "13", "--from", ",".join(map(str, ELYOS_SPAWN)),
			                    "--obelisk", "700014", "--portal", "730318", "--race", "ELYOS", "--now-ms", "1790000000000", "--tz", "UTC",
			                    "--instance-exit", "300200000", "--exp-for-level", "16"])
		self.assertEqual(code, 0)
		answer = json.loads(out.getvalue())
		self.assertEqual(answer["format"], "aion-m5f-travel")
		self.assertEqual(self.loc(answer["npcs"][0], 4)["servicePrice"], 1130)
		self.assertEqual(answer["hotspots"][0]["price"], 63)
		self.assertEqual(answer["obelisks"][0]["bindPoint"]["price"], 143)
		self.assertEqual(answer["portals"][0]["cooltime"]["id"], 46)
		self.assertEqual(answer["instanceExits"][0]["exit"]["map"], 210030000)
		self.assertEqual(answer["exp"], {"level": 16, "exp": 844378, "maxLevel": 66})
		self.assertEqual((answer["instanceSpawns"], answer["census"], answer["geoCheck"]), ([], None, None))
		with contextlib.redirect_stderr(io.StringIO()):
			self.assertEqual(oracle.main(["m5f-travel", "--portal", "730318"]), 2, "a portal needs --race")
			self.assertEqual(oracle.main(["m5f-travel", "--hotspot", "13"]), 2, "a hotspot needs --from")
			self.assertEqual(oracle.main(["m5f-travel"]), 2, "no selector")


@unittest.skipUnless(HAVE_JAVA_TREE and (CPP_GAME_SERVER / "src" / "aion").is_dir(), "the Java tree or the C++ tree is not present")
class M5fCensusTest(unittest.TestCase):
	"""W-14 and W-21 on a copy of the C++ tree's effect class names where only the two classes the plan measured unported are unported"""

	def test_census_on_a_mock_tree(self):
		real = CppEffects(CPP_GAME_SERVER)
		names = {p.name.split(".")[0] for p in real.src.iterdir()}
		if real.generated.is_dir():
			names |= {p.name.split(".")[0] for p in real.generated.iterdir()}
		with tempfile.TemporaryDirectory() as tmp:
			src = Path(tmp) / "src" / "aion" / "gameserver" / "skillengine" / "effect"
			src.mkdir(parents=True)
			for name in names:
				(src / f"{name}.h").write_text("", encoding="utf-8")
			for name in ("CondSkillLauncherEffect", "SpellAtkDrainInstantEffect"):
				(src / f"{name}.cpp").write_text("void f() { AION_UNPORTED(); }\n", encoding="utf-8")
			td = travel.TravelData(StaticData(runner.DEFAULT_STATIC_DATA), JAVA_SRC)
			report = census_report(td.data, JAVA_SRC, JAVA_QUEST_HANDLERS, Path(tmp), td.spawn_groups())
		gladiator = report["w14"]["GLADIATOR"]
		self.assertIn(563, gladiator["passives"], "W-14: passive 563")
		self.assertEqual(gladiator["unported"], ["CondSkillLauncherEffect"])
		self.assertEqual(gladiator["unportedBySkill"], {"563": ["CondSkillLauncherEffect"]})
		self.assertEqual(report["w14"]["TEMPLAR"]["unported"], [], "the gate seeds a Templar (D4)")
		haramel = report["haramel"]
		self.assertEqual(haramel["unportedEffectClasses"], ["SpellAtkDrainInstantEffect"])
		self.assertEqual(haramel["unportedBy"]["SpellAtkDrainInstantEffect"], [{"npcId": 216897, "skillId": 19214}], "W-21: Kakiti's 19214")
		self.assertEqual(report["unportedSites"], {"CondSkillLauncherEffect": 1, "SpellAtkDrainInstantEffect": 1})

	def test_census_on_the_real_tree_is_consistent(self):
		td = travel.TravelData(StaticData(runner.DEFAULT_STATIC_DATA), JAVA_SRC)
		report = census_report(td.data, JAVA_SRC, JAVA_QUEST_HANDLERS, CPP_GAME_SERVER, td.spawn_groups())
		real = CppEffects(CPP_GAME_SERVER)
		for cls, row in report["w14"].items():
			self.assertEqual(row["unported"], sorted(c for c in row["effectClasses"] if real.state(c) in ("unported", "missing")), cls)
		self.assertIn("SpellAtkDrainInstantEffect", report["haramel"]["effectClasses"])


@unittest.skipUnless(HAVE_JAVA_TREE and geo_run.DEFAULT_GEO_DIR.is_dir(), "the static data or the geo data is not present")
class M5fGeoCheckTest(unittest.TestCase):
	"""§10.5 G3 on Poeta and Haramel (the full check loads 19 maps, ~1.5 min: the command line's --geo-check)"""

	def test_poeta_and_haramel(self):
		from m5f.geocheck import geo_check
		td = travel.TravelData(StaticData(runner.DEFAULT_STATIC_DATA), JAVA_SRC)
		report = geo_check(td, geo_run.DEFAULT_GEO_DIR, geo_run.DEFAULT_WORLD_MAPS, [210010000, 300200000])
		rows = {c["what"]: c for c in report["checked"]}
		self.assertEqual(rows["portal 730318 loc 3002000"]["geoZ"], f32(144.22548), "Haramel's entry stands on its mesh")
		self.assertEqual(rows["hotspot 13"]["nudged"], [0.01, 0.01], "whole coordinates sit on a terrain cell border")
		self.assertLess(abs(rows["hotspot 13"]["dz"]), 1.0)
		self.assertEqual(report["outside"], [])
		self.assertIn("npc 203070 loc 13", [n["what"] for n in report["notModelled"]], "a flight has no data position")


if __name__ == "__main__":
	unittest.main()
