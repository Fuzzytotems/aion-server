# xmlgen: static data generator (JAXB replacement)

Python 3.12, standard library only. Design: `cpp/docs/design/static-data.md` §1-2 and its amendments. Reads the Java sources through
`tools/gen/javasrc.py`, applies the JAXB rules the sources use and writes the committed tree `cpp/game-server/generated` (except
`generated/concurrency`, which belongs to `tools/gen/fieldmap.py`).

## Commands

```
python cpp/tools/xmlgen/xmlgen.py generate [--out DIR] [--src DIR]   # write and prune the generated tree (about 3 s)
python cpp/tools/xmlgen/xmlgen.py check [--out DIR]                  # exit 1 when the committed tree is stale
python cpp/tools/xmlgen/xmlgen.py scaffold FQN... [--src DIR]        # hand-written X.h/X.cpp stubs of behaviour classes (never overwrites)
```

FQNs may be written relative to `com.aionemu.gameserver` (`model.templates.walker.WalkerTemplate`). Errors exit with code 2 and a
`file:line` message. Tests: `python -m unittest discover -s tests -t .` from this directory (CTest `tools.xmlgen`); they include the drift
check of the committed tree and V1 (the IR against the XSDs, with the reviewed `v1_allowlist.json`).

## Modules

| Module | Content |
|---|---|
| `jaxb.py` | reachability from the roots, JAXB rules (accessor types, implicit bindings, XML name defaulting, property kinds, hooks, initializers), `Policy` (xmlgen.toml) |
| `cppmodel.py` | data-only vs behaviour classification, C++ names and member types, trivial accessors, polymorphic roots, required checks, factories, includes |
| `emit.py` | enum headers, data structs, member blocks, binder headers/definitions, holder table, include bridge |
| `irjson.py` | `xmlmodel.json`, `staticdata-classes.json`, `xmlgen-report.md` |
| `scaffold.py` | stubs of hand-written behaviour classes |
| `xmlgen.py` | CLI, prune, drift check |

## Generated tree

| File | Content |
|---|---|
| `aion/gameserver/<pkg>/<Enum>.h` | `enum class` in ordinal order (`uint8_t`, `uint16_t` above 256 constants) and `xml::EnumTraits` (names, @XmlEnumValue lookup table). Nested enums: `Outer_Inner.h`, aliased in the outer class |
| `aion/gameserver/<pkg>/<Class>.h` | data-only classes: complete structs (public members, trivial Java accessors, static constants). Nested data classes: `Outer_Inner.h` |
| `aion/gameserver/<pkg>/<Class>.xml.h` | behaviour classes: includes and forward declarations of the member block; the hand-written `<Class>.h` includes it before the class |
| `aion/gameserver/<pkg>/<Class>.xml.inc` | behaviour classes: the first line of the hand-written class body (friend binder, aliases, virtual `javaClassName`, bound members, trivial accessors, hook and annotated setter declarations) |
| `aion/gameserver/<pkg>/<Class>.bind.h` / `.bind.ipp` | `xml::XmlBinding` specializations and definitions of the classes of one Java source file |
| `aion/gameserver/<pkg>/<pkg>.bind.cpp` | the translation unit of a package (includes its `.bind.ipp` files) |
| `aion/gameserver/dataholders/StaticDataHolders.xml.h` | `AION_STATIC_DATA_HOLDERS(X)`: `X(rootTag, HolderClass, javaFieldName)` for the 92 holders |
| `xmlgen-include-root.inc` | include bridge for targets without `generated/` as include directory (the slice test) |
| `xmlmodel.json` | the complete model (format `aion-xmlmodel` v1; its class/property subset is the V1 input documented in `tools/oracle/README.md`) |
| `staticdata-classes.json` | K1 class set for `fieldmap.py`: `{format: "aion-staticdata-classes", version: 1, classes: [{fqn, binaryName, kind: data/behaviour/enum, cppQualifiedName, header, runtimeMutable: [{field, cppType}]}]}` |
| `xmlgen-report.md` | classification, implicit bindings, policy use, null-checked strings, unreachable JAXB types, enums with behaviour, port checklist |

## Porting a behaviour class

1. `python cpp/tools/xmlgen/xmlgen.py scaffold model.templates.walker.WalkerTemplate` creates `src/aion/gameserver/.../WalkerTemplate.h/.cpp`.
2. Keep `#include "....xml.inc"` as the first line of the class body, followed by an access specifier. Nested behaviour classes are declared
   by the outer member block and defined after the outer class (`class Outer::Inner : public Base { #include "Outer_Inner.xml.inc" ... };`).
3. Port the hook (`afterUnmarshal(xml::LoadContext&, const xml::XmlParent&)`), annotated setters (`setXmlUid(std::string_view)`) and the
   methods listed as `// TODO port` (bodies are in the `.cpp` as comments). Trivial Java getters/setters are already generated.
4. Add transient members by hand; members listed in `[runtime_mutable]` are generated as `mutable runtime::Field<T>`.

## Policy (xmlgen.toml)

Every entry needs a reason; `generate` fails on entries that match nothing. Tables: `roots`, `deny_implicit`, `xml_names`, `initializers`,
`adapters`, `runtime_mutable`, `force_behaviour`, `storage_by_pointer`, `optional_strings`, `ignore_public_members`,
`before_unmarshal_allowed`, `external_enums`, `ignore_attributes` (XSD attributes the data uses but Java does not bind),
`unenforced_required` (required flags the data violates), `lenient_enums` (optional enum properties whose unknown data values
become null like JAXB, with a warning).
