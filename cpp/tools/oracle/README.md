# Static data oracles (V1-V4)

Independent checks for the JAXB replacement (`cpp/docs/design/static-data.md` section 4). Python 3.12, standard library only. Nothing here
imports or reuses `tools/xmlgen`: import resolution, holder counting rules and the XSD reader were written from the Java sources
(`XmlMerger`, `XmlUtil`, `StaticData`, the 92 holders and their template classes) and the data.

| Check | Module | Output |
|---|---|---|
| V2 import resolution + count oracle | `staticdata_oracle/imports.py`, `merged.py`, `counts.py` | `expected/static_data_counts.json`, `expected/static_data_counts.txt` |
| V3 tag and attribute totals | `staticdata_oracle/totals.py` | `expected/totals.json` |
| V4 lexical census | `staticdata_oracle/census.py` | `expected/census.json`, `expected/census_report.md` |
| V1 IR vs XSD cross-check | `staticdata_oracle/xsdcheck.py` | report JSON (needs the generator's IR) |

The `expected/` files are committed. `tests/test_real_data.py` regenerates them from `game-server/data/static_data` and fails on drift.

## Commands

```
python oracle.py generate [--static-data DIR] [--country-code N] [--out DIR]   # V2+V3+V4 in one pass (about 20 s), writes expected/
python oracle.py check                                                        # exit 1 if expected/ is stale
python oracle.py counts [--country-code N] [--json F] [--txt F]               # only the "Loaded N ..." lines (about 10 s)
python oracle.py compare-counts --log server.log [--expected F]               # C++ log lines vs expected (exit 1 on difference)
python oracle.py compare-totals --actual cpp_totals.json [--expected F]       # C++ loader totals vs expected (exit 1 on difference)
python oracle.py xsd-check --ir xmlmodel.json [--allowlist F] [--out F]       # V1 (exit 1 on unallowed differences)
python oracle.py xsd-inventory                                                # XSD reader sanity counts
```

Exit code 2 means an `OracleError`: data or a construct the oracle does not model, or data on which Java itself would fail at startup.
Tests: `python -m unittest discover -s tests -t .` from this directory (CTest `tools.oracle`).

## V2: import rules implemented

- `static_data.xml` may contain only top-level `<import file=... singleRootTag=... recursiveImport=...>`; other attributes (e.g. the
  documented but unimplemented `skipRoot`) are rejected.
- Region override (`XmlMerger.java:233`): for country codes 1 usa, 2 europe, 4 japan, 5 china, 6 taiwan, 7 russia,
  `<base>_<region><ext>` replaces the path only if it is a regular file (also for directory imports). Default code 99 (GSConfig).
- File import: the root element and its attributes form the holder.
- Directory import: `.xml` files (case-insensitive suffix) depth-first pre-order; entries at each level in ordinal order of the upper-cased
  name (NTFS enumeration). Non-ASCII names and names differing only in case are rejected. `singleRootTag` must be true (otherwise
  XmlMerger writes an unbalanced document); the first file's root is the holder, later roots and their attributes are dropped, all
  children are appended. An element nested in a file with the root's tag name is rejected (XmlMerger.java:307 would drop its end tag).
- A later import of the same holder tag replaces the earlier holder (JAXB field assignment); listed in `replacedHolders`.
- Files with a UTF-8 BOM or a declared encoding other than UTF-8 are rejected (XmlMerger reads through `FileReader`).

## V2: count rules

`counts.py` has one rule per StaticData element, each commented with what the Java `size()` counts. Keys are read strictly: canonical
decimal ints (`-?[0-9]+`, int range; byte range for byte keys), exact enum constant names (ordinals from `enums.py`, verified against the
Java enums by the tests), `true/false/1/0` booleans. Where Java would throw at startup (iterating a JAXB list that is null because no
element exists, missing single elements that are dereferenced, duplicate keys that throw), the oracle raises instead of producing a count.

`static_data_counts.json`:

```json
{
  "format": "aion-staticdata-counts", "version": 1, "countryCode": 99,
  "imports": [{"file": "items/item_templates.xml", "resolved": "items/item_templates.xml", "files": 1}],
  "replacedHolders": [],
  "lines": [{"javaLine": 315, "line": "Loaded 161 maps", "values": [161], "holders": ["world_maps"]}],
  "extras": {"xml_quests": {"description": "XMLQuests distinct quest ids", "value": 4184}},
  "regionVariants": [{"countryCode": 1, "region": "usa", "imports": [...], "changedLines": []}]
}
```

`static_data_counts.txt` holds the 90 messages in Java wording, one per line: what the C++ `StaticData::logCounts()` must print.
Note that Java logs `npcData.size()` while `NpcData.init` may still run asynchronously; the oracle gives the deterministic value.

## V3: totals

`totals.json` counts every holder root of the merged document and all descendants: `byTag[tag].count` and
`byTag[tag].attributes[name]`. Not counted: `<static_data>`, `<import>`, dropped roots of later directory files (`skippedRoots`), and
namespaced attributes such as `xsi:noNamespaceSchemaLocation` (`namespaceAttributes`; `xmlns*` declarations are not attributes).
The C++ loader writes bound + ignored counts in the same shape (`format` `aion-staticdata-totals`, `version` 1, `byTag`) and
`compare-totals` diffs the two. On the C++ side, load with `LoadOptions::collectStats`, write the document with
`ctx.stats().writeTotals(out, "<data>/static_data")` and run `oracle.py compare-totals --actual FILE`. Later-file roots of `singleRootTag`
directory imports appear only under `skippedRoots`, `xsi:*` attributes only under `namespaceAttributes` (`xmlns*` declarations are counted
separately and never per tag), and unknown attributes are left out of `byTag`. `BindStats::write` (tab-separated) is kept for debugging.

## V4: census

Per path (`holder/child/.../element@attribute` or `...element#text`): value count, element count, shape histogram (`empty`, `blank`,
`int`, `long`, `int-noncanonical`, `decimal`, `exponent`, `float-suffix`, `float-special`, `bool`, `enum`, `identifier`, `int-list`,
`enum-list`, `comma-list`, `datetime`, `text`, ...), int range, max length, up to 24 distinct literals, and flags with examples:
`EMPTY`, `BLANK`, `WHITESPACE_EDGE`, `MULTI_SPACE`, `PLUS_SIGN`, `LEADING_ZERO`, `NEGATIVE_ZERO`, `INT_OVERFLOW`, `LONG_OVERFLOW`,
`DECIMAL_EDGE`, `EXPONENT`, `FLOAT_SUFFIX`, `FLOAT_SPECIAL`, `HEX`, `BOOL_NONCANONICAL`, `NON_ASCII`, `CONTROL_CHAR`, `MIXED_SHAPES`,
`BOOL_MIXED_NUMERIC`, `ENUM_CASE_VARIANTS`, `MIXED_CONTENT`. The census is type-agnostic; a flag matters only if the bound Java type makes
it matter, so consumers join paths with the IR. `census_report.md` lists the flagged paths.

## V1: minimal IR format (emitted by `tools/xmlgen`)

The generator's `xmlmodel.json` (`cpp/game-server/generated/xmlmodel.json`) contains much more; V1 reads only these fields and rejects documents where they are missing or have the
wrong type.

```json
{
  "format": "aion-xmlmodel",
  "version": 1,
  "classes": [
    {
      "fqn": "com.aionemu.gameserver.model.templates.item.ItemTemplate",
      "superclass": "com.aionemu.gameserver.model.templates.VisibleObjectTemplate",
      "xmlTypeName": "ItemTemplate",
      "xmlRootElement": null,
      "xmlTransient": false,
      "properties": [
        {"javaName": "mask", "node": "attribute", "xmlName": "mask", "required": false},
        {"javaName": "actions", "node": "element", "xmlName": "actions", "required": false,
         "typeFqn": "com.aionemu.gameserver.model.templates.item.actions.ItemActions"},
        {"javaName": "addresses", "node": "element", "xmlName": "address", "wrapperName": "addresses", "required": true,
         "typeFqn": "com.aionemu.gameserver.model.templates.housing.HouseAddress"},
        {"javaName": "effects", "node": "element", "xmlName": null, "required": false,
         "choices": [{"xmlName": "damage", "typeFqn": "com.aionemu.gameserver.skillengine.effect.DamageEffect"}]}
      ]
    }
  ]
}
```

| Field | Meaning |
|---|---|
| `fqn` | Java binary-style name with dots (nested classes: `Outer.Inner` or `Outer$Inner`, used only as an identifier) |
| `superclass` | FQN or null; properties of superclasses present in `classes` are inherited (bound or unbound, `@XmlTransient` included) |
| `xmlTypeName` | effective `@XmlType` name; `""` or null when anonymous or absent (the class is then matched by root element or reference) |
| `xmlRootElement` | `@XmlRootElement` name or null |
| `xmlTransient` | optional, default false; transient classes are never matched themselves |
| `properties[].node` | `attribute` or `element` (IDREF, adapter and method-setter properties give their XML node kind here) |
| `properties[].xmlName` | effective XML name after JAXB defaulting; null only with `choices` |
| `properties[].required` | `required = true` of the annotation |
| `properties[].typeFqn` | element properties: the bound class (collection element type); omitted/null for simple types and enums |
| `properties[].wrapperName` | `@XmlElementWrapper` name; `xmlName` is then the inner element |
| `properties[].choices` | `@XmlElements`: `[{xmlName, typeFqn}]` in declaration order |

Matching and comparison rules are described at the top of `xsdcheck.py`. Allowlist file: a JSON array of
`{"class": glob, "kind": kind or "*", "name": glob, "reason": text}` with kinds `attributeMissingInXsd`, `attributeMissingInIr`,
`attributeRequiredMismatch`, `elementMissingInXsd`, `elementMissingInIr`, `elementRequiredMismatch`, `unmatchedClass`. Every entry needs a
reason; unused entries are reported under `staleAllowlist`.

The reviewed allowlist is `tools/xmlgen/v1_allowlist.json`. It has explicit per-class/per-name entries only (a wildcard can never go stale),
except the `attributeRequiredMismatch`/`elementRequiredMismatch` wildcards, which stay by design: the binder enforces the Java `required`
flag, never the XSD flag (`static-data.md` §3.4).
