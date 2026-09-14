"""One pass over the merged static data feeding V2 (counts), V3 (totals) and V4 (census); output documents and comparisons."""

from __future__ import annotations

import json
import re
from pathlib import Path

from . import OracleError
from .census import CensusVisitor, markdown_report
from .counts import CountVisitor, evaluate, evaluate_extras
from .imports import COUNTRY_REGION, DEFAULT_COUNTRY_CODE, resolve_imports
from .merged import stream_import
from .totals import TotalsVisitor

COUNTS_FORMAT = "aion-staticdata-counts"
COUNTS_VERSION = 1

TOOL_DIR = Path(__file__).resolve().parents[1]
DEFAULT_STATIC_DATA = TOOL_DIR.parents[2] / "game-server" / "data" / "static_data"
EXPECTED_DIR = TOOL_DIR / "expected"


def run(static_data_dir: Path, country_code: int = DEFAULT_COUNTRY_CODE, totals: bool = True, census: bool = True,
        region_variants: bool = True) -> dict:
	static_data_dir = Path(static_data_dir)
	imports = resolve_imports(static_data_dir / "static_data.xml", country_code)
	counter = CountVisitor()
	visitors = [counter]
	totals_visitor = TotalsVisitor() if totals else None
	census_visitor = CensusVisitor() if census else None
	visitors += [v for v in (totals_visitor, census_visitor) if v is not None]
	for imp in imports:
		stream_import(imp, visitors)
	lines = evaluate(counter.holders)
	result = {
		"counts": {
			"format": COUNTS_FORMAT,
			"version": COUNTS_VERSION,
			"source": "StaticData.afterUnmarshal (StaticData.java:313-405)",
			"countryCode": country_code,
			"imports": [{"file": i.file_attribute, "resolved": i.rel, "files": len(i.files)} for i in imports],
			"replacedHolders": [{"tag": t, "byImport": f} for t, f in counter.replaced],
			"lines": lines,
			"extras": evaluate_extras(counter.holders),
		}
	}
	if region_variants:
		result["counts"]["regionVariants"] = _region_variants(static_data_dir, imports, counter.holders, lines, country_code)
	if totals_visitor is not None:
		result["totals"] = totals_visitor.result()
	if census_visitor is not None:
		result["census"] = census_visitor.result()
	return result


def _region_variants(static_data_dir, base_imports, base_holders, base_lines, base_code):
	"""For each country code with a region: the imports that resolve differently and the count lines that change."""
	variants = []
	for code in sorted(COUNTRY_REGION):
		if code == base_code:
			continue
		imports = resolve_imports(static_data_dir / "static_data.xml", code)
		changed = [(b, i) for b, i in zip(base_imports, imports) if b.rel != i.rel]
		if not changed:
			continue
		counter = CountVisitor(base_holders)
		for _, imp in changed:
			stream_import(imp, [counter])
		lines = evaluate(counter.holders)
		variants.append({
			"countryCode": code,
			"region": COUNTRY_REGION[code],
			"imports": [{"file": i.file_attribute, "resolved": i.rel} for _, i in changed],
			"changedLines": [{"javaLine": n["javaLine"], "line": n["line"], "values": n["values"]}
			                 for b, n in zip(base_lines, lines) if b["line"] != n["line"]],
		})
	return variants


def counts_text(counts: dict) -> str:
	"""static_data_counts.txt: the Java log messages, one per line (what the C++ StaticData::logCounts must print)."""
	return "".join(line["line"] + "\n" for line in counts["lines"])


def dump_json(doc) -> str:
	return json.dumps(doc, indent="\t", ensure_ascii=False, sort_keys=False) + "\n"


def output_files(result: dict) -> dict[str, str]:
	files = {"static_data_counts.json": dump_json(result["counts"]), "static_data_counts.txt": counts_text(result["counts"])}
	if "totals" in result:
		files["totals.json"] = dump_json(result["totals"])
	if "census" in result:
		files["census.json"] = dump_json(result["census"])
		files["census_report.md"] = markdown_report(result["census"])
	return files


def write_outputs(result: dict, out_dir: Path) -> list[Path]:
	out_dir.mkdir(parents=True, exist_ok=True)
	written = []
	for name, content in output_files(result).items():
		path = out_dir / name
		with open(path, "w", encoding="utf-8", newline="\n") as f:
			f.write(content)
		written.append(path)
	return written


def compare_counts_log(expected_counts: dict, log_text: str) -> list[str]:
	"""Compares the "Loaded ..." messages found in a log (in order) with the expected lines. Returns differences."""
	expected = [line["line"] for line in expected_counts["lines"]]
	actual = []
	for raw in log_text.splitlines():
		idx = raw.find("Loaded ")
		if idx >= 0:
			actual.append(raw[idx:].rstrip())
	# Only lines whose wording matches a StaticData message are relevant (other "Loaded ..." lines may be in a full server log).
	patterns = [re.compile("^" + re.sub(r"\d+", r"\\d+", re.escape(e)) + "$") for e in expected]
	relevant = [a for a in actual if any(p.match(a) for p in patterns)]
	diffs = []
	if len(relevant) != len(expected):
		diffs.append(f"{len(relevant)} static data count lines in the log, expected {len(expected)}")
	for i, (e, a) in enumerate(zip(expected, relevant)):
		if e != a:
			diffs.append(f"line {i + 1}: {a!r}, expected {e!r}")
	return diffs


def load_json(path: Path) -> dict:
	try:
		with open(path, encoding="utf-8") as f:
			return json.load(f)
	except (OSError, ValueError) as e:
		raise OracleError(f"{path}: {e}") from e


def check_static_data_dir(path: Path) -> Path:
	path = Path(path)
	if not (path / "static_data.xml").is_file():
		raise OracleError(f"{path}: static_data.xml not found")
	return path
