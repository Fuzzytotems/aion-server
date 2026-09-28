"""V2 import resolution: an independent reimplementation of XmlMerger.processImportElement/importFile and XmlUtil.listFiles.

Java references (game-server/src/com/aionemu/gameserver):
- dataholders/loadingutils/XmlMerger.java:208-228 processImportElement: file vs directory, singleRootTag, recursiveImport
- dataholders/loadingutils/XmlMerger.java:233-243 applyCountryOverride: <base>_<region><ext>, only if it is a regular file
- dataholders/loadingutils/XmlMerger.java:275-318 importFile: first file keeps its root start tag, singleRootTag skips all root end tags
- utils/xml/XmlUtil.java:114 listFiles: Files.find, depth-first pre-order, regular files whose path ends with ".xml" (case-insensitive)

Directory order: Files.find returns entries in the order the file system enumerates them. On NTFS that is the ordinal order of the
upper-cased UTF-16 names. The oracle sorts explicitly with that comparator and rejects non-ASCII names (NTFS upcase-table ordering for
them is not modelled). The real-data test checks that os.scandir order matches on the machine that runs it.
"""

from __future__ import annotations

import os
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path

from . import OracleError

COUNTRY_REGION = {1: "usa", 2: "europe", 4: "japan", 5: "china", 6: "taiwan", 7: "russia"}  # XmlMerger.java:89
DEFAULT_COUNTRY_CODE = 99  # GSConfig.java:14 gameserver.country.code default

_IMPORT_ATTRIBUTES = frozenset({"file", "singleRootTag", "recursiveImport"})


@dataclass(frozen=True)
class ImportFile:
	path: Path  # absolute path
	rel: str  # posix path relative to the static_data directory
	skip_root_start: bool  # XmlMerger: skipRootStartElement (every directory file but the first, with singleRootTag)
	skip_root_end: bool  # XmlMerger: skipEndElement (every directory file with singleRootTag)


@dataclass
class Import:
	index: int  # 0-based position among the <import> elements
	file_attribute: str
	rel: str  # resolved path relative to the static_data directory (posix), after the region override
	region_override: bool
	is_directory: bool
	single_root_tag: bool
	recursive: bool
	files: list[ImportFile] = field(default_factory=list)


def java_parse_boolean(value: str) -> bool:
	"""Boolean.parseBoolean: true only for "true", ignoring case."""
	return value.lower() == "true"


def ntfs_sort_key(name: str) -> str:
	for ch in name:
		if ord(ch) > 0x7E or ord(ch) < 0x20:
			raise OracleError(f"file name {name!r} contains a non-ASCII or control character; NTFS upcase ordering is not modelled")
	return name.upper()


def list_xml_files(root: Path, recursive: bool) -> list[Path]:
	"""XmlUtil.listFiles(root, recursive) with NTFS enumeration order."""
	result: list[Path] = []

	def walk(directory: Path) -> None:
		names = os.listdir(directory)
		keys: dict[str, str] = {}
		for name in names:
			key = ntfs_sort_key(name)
			if key in keys:
				raise OracleError(f"{directory}: {keys[key]!r} and {name!r} differ only in case; order is undefined on NTFS")
			keys[key] = name
		for name in sorted(names, key=ntfs_sort_key):
			path = directory / name
			if path.is_symlink():
				raise OracleError(f"{path}: symbolic links are not modelled")
			if path.is_dir():
				if recursive:
					walk(path)
			elif path.is_file():
				if str(path).lower().endswith(".xml"):
					result.append(path)
			else:
				raise OracleError(f"{path}: neither a regular file nor a directory")

	walk(root)
	return result


def apply_country_override(path: Path, country_code: int) -> tuple[Path, bool]:
	"""XmlMerger.applyCountryOverride: <base>_<region><ext> replaces the path only if that is a regular file."""
	region = COUNTRY_REGION.get(country_code)
	if region is None:
		return path, False
	name = path.name
	dot = name.rfind(".")
	base, ext = (name, "") if dot < 0 else (name[:dot], name[dot:])
	override = path.parent / f"{base}_{region}{ext}"
	if override.is_file():
		return override, True
	return path, False


def _rel(path: Path, base: Path) -> str:
	return Path(os.path.relpath(path, base)).as_posix()


def resolve_imports(static_data_xml: Path, country_code: int = DEFAULT_COUNTRY_CODE) -> list[Import]:
	static_data_xml = Path(static_data_xml).resolve()
	base = static_data_xml.parent
	try:
		root = ET.parse(static_data_xml).getroot()
	except ET.ParseError as e:
		raise OracleError(f"{static_data_xml}: {e}") from e
	if root.tag != "static_data":
		raise OracleError(f"{static_data_xml}: root element is <{root.tag}>, expected <static_data>")
	imports: list[Import] = []
	for element in root.iter():
		if element is root:
			continue
		if element.tag != "import":
			raise OracleError(f"{static_data_xml}: unexpected element <{element.tag}> (XmlMerger would copy it into the merged document)")
		if element not in list(root):
			raise OracleError(f"{static_data_xml}: nested <import> is not modelled")
		unknown = set(element.attrib) - _IMPORT_ATTRIBUTES
		if unknown:
			raise OracleError(f"{static_data_xml}: <import> has unsupported attributes {sorted(unknown)} (XmlMerger ignores e.g. skipRoot)")
		if len(element):
			raise OracleError(f"{static_data_xml}: <import> must be empty")
		file_attribute = element.get("file")
		if file_attribute is None:
			raise OracleError(f"{static_data_xml}: Attribute 'file' is missing or empty.")
		path, overridden = apply_country_override(base / file_attribute, country_code)
		if not path.exists():
			raise OracleError(f"Missing file to import: {path}")
		single_root_tag = java_parse_boolean(element.get("singleRootTag", "false"))
		recursive = java_parse_boolean(element.get("recursiveImport", "true"))
		imp = Import(len(imports), file_attribute, _rel(path, base), overridden, path.is_dir(), single_root_tag, recursive)
		if path.is_file():
			imp.single_root_tag = False  # XmlMerger ignores singleRootTag for file imports
			imp.files.append(ImportFile(path, imp.rel, False, False))
		elif path.is_dir():
			if not single_root_tag:
				raise OracleError(f"import {file_attribute!r}: a directory import without singleRootTag=\"true\" produces an unbalanced "
				                  "merged document (XmlMerger.java:226)")
			files = list_xml_files(path, recursive)
			if not files:
				raise OracleError(f"import {file_attribute!r}: directory contains no .xml files (XmlMerger.java:226 would close <static_data>)")
			for i, file in enumerate(files):
				imp.files.append(ImportFile(file, _rel(file, base), i > 0, True))
		else:
			raise OracleError(f"import {file_attribute!r}: {path} is neither a regular file nor a directory")
		imports.append(imp)
	return imports
