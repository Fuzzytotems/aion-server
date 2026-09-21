#!/usr/bin/env python3
"""Independent static data oracles (static-data.md section 4). Python 3.12, stdlib only. See README.md.

	oracle.py generate [--static-data DIR] [--country-code N] [--out DIR]   V2+V3+V4 in one pass, writes the expected/ documents
	oracle.py counts [--static-data DIR] [--country-code N] [--json F] [--txt F]
	oracle.py check [--static-data DIR]                                    regenerate and diff against expected/ (exit 1 on drift)
	oracle.py compare-counts --log FILE [--expected F]                     "Loaded N ..." lines of a (C++) log vs expected counts
	oracle.py compare-totals --actual FILE [--expected F]                  V3 totals document of the C++ loader vs expected totals
	oracle.py xsd-check --ir FILE [--xsd-root DIR] [--allowlist F] [--out F]   V1: IR vs the XSDs (exit 1 on unallowed differences)
	oracle.py xsd-inventory [--xsd-root DIR]                                XSD construct counts (sanity check of the XSD reader)
	oracle.py m5a-spawns --map ID --x X --y Y --z Z [--radius R] [--game-minutes M | --game-hour H ...] [--weekday DAY]
	oracle.py m5a-border-target --map ID --x X --y Y --z Z [--game-minutes M | --game-hour H ...]
	oracle.py m5a-creation --race ELYOS|ASMODIANS --class CLASS [--java-src DIR]   (m5a/ package, m5a-plan.md F-05)
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from staticdata_oracle import OracleError  # noqa: E402
from staticdata_oracle import run as runner  # noqa: E402
from staticdata_oracle import totals as totals_mod  # noqa: E402
from staticdata_oracle import xsdcheck  # noqa: E402
from staticdata_oracle.imports import DEFAULT_COUNTRY_CODE  # noqa: E402


def _data_dir(args):
	return runner.check_static_data_dir(Path(args.static_data) if args.static_data else runner.DEFAULT_STATIC_DATA)


def cmd_generate(args):
	result = runner.run(_data_dir(args), args.country_code)
	for path in runner.write_outputs(result, Path(args.out)):
		print(f"wrote {path}")
	return 0


def cmd_counts(args):
	result = runner.run(_data_dir(args), args.country_code, totals=False, census=False, region_variants=False)
	text = runner.counts_text(result["counts"])
	if args.json:
		Path(args.json).write_text(runner.dump_json(result["counts"]), encoding="utf-8", newline="\n")
	if args.txt:
		Path(args.txt).write_text(text, encoding="utf-8", newline="\n")
	if not args.json and not args.txt:
		sys.stdout.write(text)
	return 0


def cmd_check(args):
	result = runner.run(_data_dir(args), DEFAULT_COUNTRY_CODE)
	expected_dir = Path(args.expected_dir)
	drift = []
	for name, content in runner.output_files(result).items():
		path = expected_dir / name
		if not path.is_file() or path.read_text(encoding="utf-8") != content:
			drift.append(name)
	for name in drift:
		print(f"drift: {expected_dir / name}")
	return 1 if drift else 0


def cmd_compare_counts(args):
	expected = runner.load_json(Path(args.expected))
	diffs = runner.compare_counts_log(expected, Path(args.log).read_text(encoding="utf-8", errors="replace"))
	for d in diffs:
		print(d)
	print("counts equal" if not diffs else f"{len(diffs)} difference(s)")
	return 1 if diffs else 0


def cmd_compare_totals(args):
	diffs = totals_mod.compare(runner.load_json(Path(args.expected)), runner.load_json(Path(args.actual)))
	for d in diffs:
		print(d)
	print("totals equal" if not diffs else f"{len(diffs)} difference(s)")
	return 1 if diffs else 0


def cmd_xsd_check(args):
	xsd_root = Path(args.xsd_root) if args.xsd_root else runner.DEFAULT_STATIC_DATA
	report = xsdcheck.check(runner.load_json(Path(args.ir)), xsdcheck.load_schemas(xsd_root),
	                        runner.load_json(Path(args.allowlist)) if args.allowlist else None)
	text = runner.dump_json(report)
	if args.out:
		Path(args.out).write_text(text, encoding="utf-8", newline="\n")
	else:
		sys.stdout.write(text)
	return 0 if report["ok"] else 1


def cmd_xsd_inventory(args):
	xsd_root = Path(args.xsd_root) if args.xsd_root else runner.DEFAULT_STATIC_DATA
	sys.stdout.write(runner.dump_json(xsdcheck.load_schemas(xsd_root).inventory()))
	return 0


def _m5a_clock(args):
	from m5a.spawns import GameClock
	if args.game_minutes is not None:
		return GameClock.from_minutes(args.game_minutes, args.weekday)
	return GameClock(args.game_hour, args.game_day, args.game_month, args.weekday)


def cmd_m5a_spawns(args):
	from m5a.data import StaticData
	from m5a.spawns import spots_report
	report = spots_report(StaticData(_data_dir(args)), args.map, (args.x, args.y, args.z), args.radius, _m5a_clock(args))
	sys.stdout.write(runner.dump_json(report))
	return 0


def cmd_m5a_border_target(args):
	from m5a.data import StaticData
	from m5a.spawns import border_target
	sys.stdout.write(runner.dump_json(border_target(StaticData(_data_dir(args)), args.map, (args.x, args.y, args.z), _m5a_clock(args))))
	return 0


def cmd_m5a_creation(args):
	from m5a.creation import creation_report
	from m5a.data import StaticData
	data_dir = _data_dir(args)
	java_src = Path(args.java_src) if args.java_src else data_dir.parent.parent / "src"
	sys.stdout.write(runner.dump_json(creation_report(StaticData(data_dir), java_src, args.race, args.player_class)))
	return 0


def main(argv=None):
	parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
	sub = parser.add_subparsers(dest="command", required=True)

	def data_args(p, country=True):
		p.add_argument("--static-data", help=f"static_data directory (default {runner.DEFAULT_STATIC_DATA})")
		if country:
			p.add_argument("--country-code", type=int, default=DEFAULT_COUNTRY_CODE, help="GSConfig.SERVER_COUNTRY_CODE (default 99)")

	p = sub.add_parser("generate")
	data_args(p)
	p.add_argument("--out", default=str(runner.EXPECTED_DIR))
	p.set_defaults(fn=cmd_generate)

	p = sub.add_parser("counts")
	data_args(p)
	p.add_argument("--json")
	p.add_argument("--txt")
	p.set_defaults(fn=cmd_counts)

	p = sub.add_parser("check")
	data_args(p, country=False)
	p.add_argument("--expected-dir", default=str(runner.EXPECTED_DIR))
	p.set_defaults(fn=cmd_check)

	p = sub.add_parser("compare-counts")
	p.add_argument("--log", required=True)
	p.add_argument("--expected", default=str(runner.EXPECTED_DIR / "static_data_counts.json"))
	p.set_defaults(fn=cmd_compare_counts)

	p = sub.add_parser("compare-totals")
	p.add_argument("--actual", required=True)
	p.add_argument("--expected", default=str(runner.EXPECTED_DIR / "totals.json"))
	p.set_defaults(fn=cmd_compare_totals)

	p = sub.add_parser("xsd-check")
	p.add_argument("--ir", required=True)
	p.add_argument("--xsd-root")
	p.add_argument("--allowlist")
	p.add_argument("--out")
	p.set_defaults(fn=cmd_xsd_check)

	p = sub.add_parser("xsd-inventory")
	p.add_argument("--xsd-root")
	p.set_defaults(fn=cmd_xsd_inventory)

	def clock_args(p):
		p.add_argument("--game-minutes", type=int, help="SM_GAME_TIME minutes since 01.01.0000 (sets hour, day and month)")
		p.add_argument("--game-hour", type=int)
		p.add_argument("--game-day", type=int)
		p.add_argument("--game-month", type=int)
		p.add_argument("--weekday", choices=("MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY", "SUNDAY"))

	p = sub.add_parser("m5a-spawns", help="npc and gatherable spots around a point after spawnAll (m5a-plan.md 5.5)")
	data_args(p, country=False)
	p.add_argument("--map", type=int, required=True)
	p.add_argument("--x", type=float, required=True)
	p.add_argument("--y", type=float, required=True)
	p.add_argument("--z", type=float, required=True)
	p.add_argument("--radius", type=float, default=100.0)
	clock_args(p)
	p.set_defaults(fn=cmd_m5a_spawns)

	p = sub.add_parser("m5a-border-target", help="region move target with appearing and disappearing npcs (m5a-plan.md 5.6)")
	data_args(p, country=False)
	p.add_argument("--map", type=int, required=True)
	p.add_argument("--x", type=float, required=True)
	p.add_argument("--y", type=float, required=True)
	p.add_argument("--z", type=float, required=True)
	clock_args(p)
	p.set_defaults(fn=cmd_m5a_border_target)

	p = sub.add_parser("m5a-creation", help="spawn point, items, skills and base HP/MP of a new character (m5a-plan.md 5.3)")
	data_args(p, country=False)
	p.add_argument("--java-src", help="game-server/src (default: two levels above the static data directory, then src)")
	p.add_argument("--race", required=True, choices=("ELYOS", "ASMODIANS"))
	p.add_argument("--class", dest="player_class", required=True)
	p.set_defaults(fn=cmd_m5a_creation)

	args = parser.parse_args(argv)
	try:
		return args.fn(args)
	except OracleError as e:
		print(f"oracle error: {e}", file=sys.stderr)
		return 2


if __name__ == "__main__":
	sys.exit(main())
