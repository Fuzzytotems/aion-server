"""V3 coverage totals over the merged document: elements per tag and attributes per (tag, attribute).

Counted: every holder root of the merged document (the first file's root of each import) and all its descendants. Not counted: the
<static_data> root, the <import> elements, the root elements of later files in singleRootTag directories (listed under skippedRoots with
their attributes, because the merge drops them) and namespaced attributes such as xsi:noNamespaceSchemaLocation (listed under
namespaceAttributes). The C++ loader reports bound + ignored per tag and per (tag, attribute) in the same shape; compare() diffs them.
"""

from __future__ import annotations

from collections import Counter, defaultdict

from . import OracleError

FORMAT = "aion-staticdata-totals"
VERSION = 1


def _attr_name(name: str) -> str:
	if name.startswith("{"):
		uri, local = name[1:].split("}", 1)
		prefix = "xsi" if uri == "http://www.w3.org/2001/XMLSchema-instance" else uri
		return f"{prefix}:{local}"
	return name


class TotalsVisitor:

	def __init__(self):
		self.tags = Counter()
		self.attributes = defaultdict(Counter)
		self.namespace_attributes = Counter()
		self.skipped_roots = []
		self.holders = {}
		self._holder = None

	def _count(self, tag, attrib):
		self.tags[tag] += 1
		h = self._holder
		h["elements"] += 1
		for name in attrib:
			if name[0] == "{":
				self.namespace_attributes[_attr_name(name)] += 1
			else:
				self.attributes[tag][name] += 1
				h["attributes"] += 1

	def begin_import(self, imp):
		self._holder = {"import": imp.file_attribute, "files": len(imp.files), "elements": 0, "attributes": 0}

	def begin_holder(self, imp, file, tag, attrib):
		self._holder["root"] = tag
		self._count(tag, attrib)

	def skipped_root(self, imp, file, tag, attrib):
		self.skipped_roots.append({"file": file.rel, "tag": tag, "attributes": sorted(_attr_name(a) for a in attrib)})

	def start(self, path, tag, attrib):
		self._count(tag, attrib)

	def end_holder(self, imp):
		key = imp.file_attribute
		if key in self.holders:
			raise OracleError(f"import {key!r} listed twice")
		self.holders[key] = self._holder
		self._holder = None

	def result(self):
		by_tag = {}
		for tag in sorted(self.tags):
			by_tag[tag] = {"count": self.tags[tag], "attributes": dict(sorted(self.attributes[tag].items()))}
		return {
			"format": FORMAT,
			"version": VERSION,
			"elements": sum(self.tags.values()),
			"attributes": sum(sum(c.values()) for c in self.attributes.values()),
			"byTag": by_tag,
			"byImport": [self.holders[k] for k in self.holders],
			"namespaceAttributes": dict(sorted(self.namespace_attributes.items())),
			"skippedRoots": self.skipped_roots,
		}


def compare(expected: dict, actual: dict) -> list[str]:
	"""Differences between two totals documents (tag counts and attribute counts); empty when equal."""
	for doc, name in ((expected, "expected"), (actual, "actual")):
		if doc.get("format") != FORMAT or doc.get("version") != VERSION:
			raise OracleError(f"{name}: not a {FORMAT} v{VERSION} document")
	diffs = []
	e_tags, a_tags = expected["byTag"], actual["byTag"]
	for tag in sorted(set(e_tags) | set(a_tags)):
		e, a = e_tags.get(tag), a_tags.get(tag)
		if e is None or a is None:
			diffs.append(f"<{tag}>: {'missing in actual' if a is None else 'not expected'}")
			continue
		if e["count"] != a["count"]:
			diffs.append(f"<{tag}>: {a['count']} elements, expected {e['count']}")
		for attr in sorted(set(e["attributes"]) | set(a["attributes"])):
			ec, ac = e["attributes"].get(attr, 0), a["attributes"].get(attr, 0)
			if ec != ac:
				diffs.append(f"<{tag}> @{attr}: {ac}, expected {ec}")
	return diffs
