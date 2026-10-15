"""Streams the merged static data document import by import, without writing it (XmlMerger + JAXB view).

For every import the visitor sees one holder: the root element of the first file (tag and attributes) and then the children of all files
in order. Root elements of later files in a singleRootTag directory are reported through skipped_root() and are not part of the merged
document. Comments and whitespace-only text are not part of the merged document either (XmlMerger.java:289-297); ElementTree drops
comments, and the visitors ignore whitespace-only text.

Visitor protocol (all methods optional):
	begin_import(imp)
	begin_holder(imp, file, tag, attrib)              root of the merged holder (first file)
	skipped_root(imp, file, tag, attrib)              root of a later file of a singleRootTag directory
	begin_file(imp, file)
	start(path, tag, attrib)                          every element below the holder root; path is a tuple of tags from the holder root
	end(path, element)                                every element below the holder root, subtree complete
	holder_child(element)                             depth-1 child complete (subtree available until the call returns)
	end_file(imp, file)
	end_holder(imp)
"""

from __future__ import annotations

import re
import xml.etree.ElementTree as ET
from pathlib import Path

from . import OracleError
from .imports import Import, resolve_imports, DEFAULT_COUNTRY_CODE

_XML_DECL = re.compile(rb'<\?xml[^>]*encoding\s*=\s*["\']([A-Za-z0-9._-]+)["\']')


def _check_encoding(path: Path) -> None:
	with open(path, "rb") as f:
		head = f.read(256)
	if head.startswith(b"\xef\xbb\xbf"):
		raise OracleError(f"{path}: UTF-8 byte order mark (XmlMerger reads through FileReader, which does not skip it)")
	m = _XML_DECL.search(head)
	if m and m.group(1).decode("ascii").upper() not in ("UTF-8", "UTF8"):
		raise OracleError(f"{path}: declared encoding {m.group(1)!r}; FileReader decodes with the platform charset (UTF-8 on Java 18+)")


def _call(visitors, name, *args):
	for v in visitors:
		fn = getattr(v, name, None)
		if fn is not None:
			fn(*args)


def stream_import(imp: Import, visitors) -> None:
	_call(visitors, "begin_import", imp)
	holder_started = False
	for file in imp.files:
		_check_encoding(file.path)
		_call(visitors, "begin_file", imp, file)
		root_tag = None
		root = None
		path: list[str] = []
		try:
			for event, element in ET.iterparse(file.path, events=("start", "end")):
				tag = element.tag
				if event == "start":
					if tag.startswith("{"):
						raise OracleError(f"{file.rel}: namespaced element {tag} is not modelled")
					if root is None:
						root, root_tag = element, tag
						if file.skip_root_start:
							_call(visitors, "skipped_root", imp, file, tag, dict(element.attrib))
						else:
							holder_started = True
							_call(visitors, "begin_holder", imp, file, tag, dict(element.attrib))
						continue
					if imp.single_root_tag and tag == root_tag:
						raise OracleError(f"{file.rel}: nested <{tag}> has the root tag name; XmlMerger.java:307 would drop its end tag")
					path.append(tag)
					_call(visitors, "start", tuple(path), tag, element.attrib)
				else:
					if element is root:
						continue
					_call(visitors, "end", tuple(path), element)
					path.pop()
					if not path:
						_call(visitors, "holder_child", element)
						root.remove(element)
		except ET.ParseError as e:
			raise OracleError(f"{file.rel}: {e}") from e
		except OracleError as e:
			if file.rel in str(e):
				raise
			raise OracleError(f"{file.rel}: {e}") from e
		_call(visitors, "end_file", imp, file)
	if not holder_started:
		raise OracleError(f"import {imp.file_attribute!r}: no holder root")
	_call(visitors, "end_holder", imp)


def stream_static_data(static_data_dir: Path, visitors, country_code: int = DEFAULT_COUNTRY_CODE) -> list[Import]:
	imports = resolve_imports(Path(static_data_dir) / "static_data.xml", country_code)
	for imp in imports:
		stream_import(imp, visitors)
	return imports
