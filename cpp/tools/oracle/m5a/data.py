"""Static data access for the M5a oracles: the files of a holder (by its root tag, through the V2 import resolution) and their child elements."""

from __future__ import annotations

import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Iterator

from staticdata_oracle import OracleError
from staticdata_oracle.imports import DEFAULT_COUNTRY_CODE, resolve_imports


def _root_tag(path: Path) -> str:
	for _, element in ET.iterparse(path, events=("start",)):
		return element.tag
	raise OracleError(f"{path}: empty document")


class StaticData:
	"""The imports of one static_data directory, indexed by the holder root tag (a later import of the same tag replaces the earlier one)."""

	def __init__(self, static_data_dir: Path, country_code: int = DEFAULT_COUNTRY_CODE):
		self.dir = Path(static_data_dir)
		xml = self.dir / "static_data.xml"
		if not xml.is_file():
			raise OracleError(f"{xml} does not exist")
		self._files: dict[str, list[Path]] = {}
		for imp in resolve_imports(xml, country_code):
			if not imp.files:
				continue
			self._files[_root_tag(imp.files[0].path)] = [f.path for f in imp.files]

	def files(self, root_tag: str) -> list[Path]:
		files = self._files.get(root_tag)
		if files is None:
			raise OracleError(f"no static data import has the root element <{root_tag}>")
		return files

	def children(self, root_tag: str, child_tag: str | None = None) -> Iterator[ET.Element]:
		"""The direct children of the holder's root elements in merged document order (every file of a directory import in import order)."""
		for path in self.files(root_tag):
			try:
				root = ET.parse(path).getroot()
			except ET.ParseError as e:
				raise OracleError(f"{path}: {e}") from e
			for element in root:
				if child_tag is None or element.tag == child_tag:
					yield element

	def stream(self, root_tag: str, child_tag: str) -> Iterator[ET.Element]:
		"""Like children() for large holders: elements with child_tag directly below the root, cleared after use."""
		for path in self.files(root_tag):
			depth = 0
			try:
				for event, element in ET.iterparse(path, events=("start", "end")):
					if event == "start":
						depth += 1
						continue
					depth -= 1
					if depth == 1 and element.tag == child_tag:
						yield element
						element.clear()
			except ET.ParseError as e:
				raise OracleError(f"{path}: {e}") from e


def java_int(text: str | None, what: str, default: int | None = None) -> int:
	"""A JAXB int attribute: canonical decimal, int range."""
	if text is None:
		if default is None:
			raise OracleError(f"missing required attribute {what}")
		return default
	value = text.strip()
	try:
		number = int(value, 10)
	except ValueError as e:
		raise OracleError(f"{what}={text!r} is not an int") from e
	if not -2147483648 <= number <= 2147483647:
		raise OracleError(f"{what}={text!r} is out of the int range")
	return number


def java_boolean(text: str | None, default: bool = False) -> bool:
	"""A JAXB boolean attribute (xs:boolean lexical forms)."""
	if text is None:
		return default
	value = text.strip()
	if value in ("true", "1"):
		return True
	if value in ("false", "0"):
		return False
	raise OracleError(f"{text!r} is not a boolean")
