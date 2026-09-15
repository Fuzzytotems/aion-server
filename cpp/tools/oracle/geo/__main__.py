"""python -m geo generate|check [--geo-dir DIR] [--world-maps FILE] [--expected FILE] (from cpp/tools/oracle)

	generate   writes expected/geo_expected.json
	check      regenerates and compares with the committed document (exit 1 on drift)
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from . import GeoOracleError
from . import run


def main(argv: list[str]) -> int:
	parser = argparse.ArgumentParser(prog="python -m geo")
	parser.add_argument("command", choices=["generate", "check"])
	parser.add_argument("--geo-dir", type=Path, default=run.DEFAULT_GEO_DIR)
	parser.add_argument("--world-maps", type=Path, default=run.DEFAULT_WORLD_MAPS)
	parser.add_argument("--expected", type=Path, default=run.DEFAULT_EXPECTED)
	args = parser.parse_args(argv)
	try:
		text = run.dumps(run.generate(args.geo_dir, args.world_maps))
	except GeoOracleError as error:
		print(f"geo oracle: {error}", file=sys.stderr)
		return 2
	if args.command == "generate":
		args.expected.parent.mkdir(parents=True, exist_ok=True)
		args.expected.write_text(text, encoding="utf-8", newline="\n")
		print(f"wrote {args.expected}")
		return 0
	current = args.expected.read_text(encoding="utf-8") if args.expected.is_file() else ""
	if current != text:
		print(f"{args.expected} is stale: run python -m geo generate", file=sys.stderr)
		return 1
	return 0


if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
