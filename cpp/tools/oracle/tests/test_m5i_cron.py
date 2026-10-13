"""Tests of the m5i cron module (m5i-plan.md Z-04): known answers derived by hand from Quartz's documented semantics, the shapes the oracle
refuses, SiegeService.getPreparationCronString, and every expression of the shipped schedule files."""
import datetime as dt
import re
import unittest
from pathlib import Path

from staticdata_oracle import OracleError
from m5i.cron import Cron, next_fire_times, preparation_cron, quiet_window, zone

UTC = dt.timezone.utc
CONFIG = Path(__file__).resolve().parents[4] / "game-server" / "config"


def at(*args):
	return dt.datetime(*args, tzinfo=UTC)


def utc(times):
	return [t.astimezone(UTC) for t in times]


class CronTest(unittest.TestCase):

	def test_a_weekly_siege_time(self):
		# 2026-10-13 is a Tuesday
		self.assertEqual(utc(next_fire_times("0 0 17 ? * TUE,THU,SAT", "UTC", at(2026, 10, 13, 10, 0), 3)),
		                 [at(2026, 10, 13, 17), at(2026, 10, 15, 17), at(2026, 10, 17, 17)])

	def test_strictly_after(self):
		self.assertEqual(utc(next_fire_times("20 * * ? * *", "UTC", at(2026, 10, 13, 10, 0, 5))), [at(2026, 10, 13, 10, 0, 20)])
		self.assertEqual(utc(next_fire_times("20 * * ? * *", "UTC", at(2026, 10, 13, 10, 0, 20))), [at(2026, 10, 13, 10, 1, 20)],
		                 "Quartz getTimeAfter: the instant itself does not count")

	def test_steps_lists_and_ranges(self):
		self.assertEqual(utc(next_fire_times("0 5/20 * ? * *", "UTC", at(2026, 10, 13, 10, 0), 3)),
		                 [at(2026, 10, 13, 10, 5), at(2026, 10, 13, 10, 25), at(2026, 10, 13, 10, 45)])
		self.assertEqual(utc(next_fire_times("0 */15 * ? * *", "UTC", at(2026, 10, 13, 10, 1), 2)), [at(2026, 10, 13, 10, 15), at(2026, 10, 13, 10, 30)])
		self.assertEqual(utc(next_fire_times("0 30 14,18,21 ? * *", "UTC", at(2026, 10, 13, 19), 2)), [at(2026, 10, 13, 21, 30), at(2026, 10, 14, 14, 30)])
		self.assertEqual(utc(next_fire_times("0 0 9 ? * MON-FRI", "UTC", at(2026, 10, 16, 10), 1)), [at(2026, 10, 19, 9)], "Friday after 9: Monday")
		self.assertEqual(utc(next_fire_times("0 0 0 1 JAN,JUL ?", "UTC", at(2026, 10, 13), 1)), [at(2027, 1, 1)])
		self.assertEqual(Cron("0 0,30/40 * ? * *").minutes, {0, 30}, "a plain value in a list with a step is itself")

	def test_the_year_field(self):
		self.assertEqual(utc(next_fire_times("0 0 9 ? * WED *", "UTC", at(2026, 10, 13), 1)), [at(2026, 10, 14, 9)], "CronJobService's dominion job")
		self.assertEqual(utc(next_fire_times("0 0 0 1 1 ? 2000,2100,2101", "UTC", at(2026, 10, 13), 2)), [at(2100, 1, 1), at(2101, 1, 1)],
		                 "the scenario profile's FAR_FUTURE_CRON")
		self.assertEqual(next_fire_times("0 0 0 1 1 ? 2000", "UTC", at(2026, 10, 13), 1), [], "a past year never fires")

	def test_a_time_zone_and_its_dst_gap(self):
		try:
			zone("Europe/Berlin")
		except OracleError:
			self.skipTest("no IANA time zone database on this machine (zoneinfo)")
		# Europe/Berlin springs forward on 2026-03-29 at 02:00: 02:30 does not exist that day
		self.assertEqual(utc(next_fire_times("0 30 2 * * ?", "Europe/Berlin", at(2026, 3, 28, 12), 2)),
		                 [at(2026, 3, 30, 0, 30), at(2026, 3, 31, 0, 30)], "02:30 CEST = 00:30 UTC; the 29th has no 02:30")
		self.assertEqual(utc(next_fire_times("0 0 17 ? * TUE", "Europe/Berlin", at(2026, 10, 13, 10), 1)), [at(2026, 10, 13, 15)], "CEST is UTC+2")

	def test_the_cron_quiet_window(self):
		run, spare = dt.timedelta(minutes=12), dt.timedelta(minutes=2)
		hourly = ["0 0 * ? * *", "0 30 * ? * *"]
		self.assertEqual(quiet_window(hourly, "UTC", at(2026, 10, 13, 10, 5), run, spare), at(2026, 10, 13, 10, 5), "10:03-10:19 is free")
		self.assertEqual(quiet_window(hourly, "UTC", at(2026, 10, 13, 10, 20), run, spare), at(2026, 10, 13, 10, 32, 1),
		                 "10:30 falls into 10:18-10:34: the run starts after it and its spare")
		self.assertEqual(quiet_window(hourly, "UTC", at(2026, 10, 13, 10, 59), run, spare), at(2026, 10, 13, 11, 2, 1), "11:00 is in the window")
		with self.assertRaises(OracleError):
			quiet_window(["0 * * ? * *"], "UTC", at(2026, 10, 13, 10), run, spare, dt.timedelta(hours=1))  # every minute: never quiet

	def test_fixed_offsets(self):
		self.assertEqual(utc(next_fire_times("0 0 17 ? * TUE", "+02:00", at(2026, 10, 13, 10), 1)), [at(2026, 10, 13, 15)])
		with self.assertRaises(OracleError):
			zone("Not/AZone")

	def test_refused_shapes(self):
		for expr in ("0 0 0 L * ?", "0 0 0 ? * MON#2", "0 0 0 LW * ?", "0 0 0 * * *", "0 0 0 ? * ?", "0 0 24 ? * *", "0 0 0 ? * MON 1969",
		             "0 0 0 ? *", "0 0 22-2 ? * *", "0 */0 * ? * *"):
			with self.subTest(expr=expr), self.assertRaises(OracleError):
				Cron(expr)

	def test_the_preparation_shift(self):
		self.assertEqual(preparation_cron("0 0 21 ? * FRI", 1011), "0 55 20 ? * FRI", "5 minutes before a fortress siege")
		self.assertEqual(preparation_cron("0 0 21 ? * FRI", 10111), "0 50 20 ? * FRI", "10 minutes before a Panesterra siege")
		self.assertEqual(preparation_cron("0 30 17 ? * SAT", 1131), "0 25 17 ? * SAT")
		with self.assertRaises(OracleError) as refused:
			preparation_cron("0 3 0 ? * MON", 1011)
		self.assertIn("Preparation over midnight not supported.", str(refused.exception))
		with self.assertRaises(OracleError):
			preparation_cron("0 0,30 21 ? * FRI", 1011)  # Byte.parseByte("0,30"): NumberFormatException

	@unittest.skipUnless(CONFIG.is_dir(), "needs the Java tree's config")
	def test_every_shipped_schedule_parses_and_fires(self):
		count = 0
		for path in sorted((CONFIG / "schedule").glob("*.xml")):
			text = re.sub(r"<!--.*?-->", "", path.read_text(encoding="utf-8"), flags=re.S)
			for expr in re.findall(r">\s*([0-9*?/,\-A-Za-z]+(?: [0-9*?/,\-A-Za-z]+){5,6})\s*<", text):
				with self.subTest(file=path.name, expr=expr):
					self.assertTrue(next_fire_times(expr, "UTC", at(2026, 10, 13), 1), expr)
					count += 1
		self.assertGreater(count, 40, "the siege, rift and world raid schedules")
		sieges = re.findall(r'<fortress id="(\d+)">(.*?)</fortress>', (CONFIG / "schedule" / "siege_schedule.xml").read_text(encoding="utf-8"), re.S)
		for fortress_id, body in sieges:
			for siege_time in re.findall(r"<siegeTime>([^<]+)</siegeTime>", body):
				with self.subTest(fortress=fortress_id, siege_time=siege_time):
					Cron(preparation_cron(siege_time.strip(), int(fortress_id)))


if __name__ == "__main__":
	unittest.main()
