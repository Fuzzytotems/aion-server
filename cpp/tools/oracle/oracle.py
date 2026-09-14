#!/usr/bin/env python3
"""Independent static data oracles (static-data.md section 4). Python 3.12, stdlib only. See README.md.

	oracle.py generate [--static-data DIR] [--country-code N] [--out DIR]   V2+V3+V4 in one pass, writes the expected/ documents
	oracle.py counts [--static-data DIR] [--country-code N] [--json F] [--txt F]
	oracle.py check [--static-data DIR]                                    regenerate and diff against expected/ (exit 1 on drift)
	oracle.py compare-counts --log FILE [--expected F]                     "Loaded N ..." lines of a (C++) log vs expected counts
	oracle.py compare-totals --actual FILE [--expected F]                  V3 totals document of the C++ loader vs expected totals
	oracle.py xsd-check --ir FILE [--xsd-root DIR] [--allowlist F] [--out F]   V1: IR vs the XSDs (exit 1 on unallowed differences)
	oracle.py xsd-inventory [--xsd-root DIR]                                XSD construct counts (sanity check of the XSD reader)
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

	args = parser.parse_args(argv)
	try:
		return args.fn(args)
	except OracleError as e:
		print(f"oracle error: {e}", file=sys.stderr)
		return 2


if __name__ == "__main__":
	sys.exit(main())
