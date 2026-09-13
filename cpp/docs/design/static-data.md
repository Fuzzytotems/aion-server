# Static data (JAXB replacement)

> **Status:** accepted with the amendments at the end of this document (free-threaded runtime D1, reload deferred D3, 4-6 agents per wave D4), 2026-09-13.

## Summary

Design for porting JAXB static data. A Python 3.12 generator (stdlib only) uses its own tokenizer and declaration-level Java parser to read the JAXB annotations. It starts from the StaticData root and the other unmarshal roots and follows types from there, instead of taking every file that imports javax.xml.bind. It writes a JSON intermediate model, which drives code generation and the verification scripts. Generated files are checked in, a `--check` test detects drift, and a scaffold mode creates hand-written stubs.

What gets generated:
- Enums: `enum class` plus name tables.
- Data-only classes: complete headers.
- Classes with behaviour (effects, item actions, ItemTemplate, ...): a member block (`*.xml.inc`) that is `#include`d inside the hand-written class body. One C++ class per Java class; no generated base-class layer.
- For every class: `XmlBinding<T>` specializations over pugixml (attributes, children, required-attribute check, afterUnmarshal hook) and element-name factories for the 12 `@XmlElements` lists.

The runtime loader follows XmlMerger's rules but writes no merged file:
- per-import region override;
- directory imports walked depth-first in NTFS (uppercase-ordinal) order, which I checked against real enumeration for all 12 directories;
- all files of one import parsed in parallel, then bound in order into one holder;
- the holder's afterUnmarshal runs once at the end.
Hand-ported hooks get a LoadContext (earlier holders, parent) and run inline. Post-processing (items cleanup, drop rules, validations) runs on the unpublished StaticData. Holders are then published as immortal const objects through atomic `HolderRef`s. //reload keeps old holders forever instead of freeing them. The spawn, walker and event families are explicitly mutable and locked.

Verification without a JDK has these parts:
- an independent Python count oracle reproducing the ~92 'Loaded N' lines;
- per-tag element and attribute totals: bound plus ignored must equal what is in the files;
- a lexical census proving that the data never reaches JAXB parsing edge cases;
- a full value round-trip dump diff;
- a cross-check of the parsed Java model against the XSDs;
- golden tests for adapters, order and hooks.

The first vertical slice is world_maps, tribe_relations, npc_walker, item_templates and player_initial_data.

# Static data design (decision B)

Checked against the Java sources in this pass (on top of the research maps):
- EffectTemplate/Effects/HostileUpEffect/DamageEffect/BufEffect, ItemTemplate, ItemData, NpcData, StaticData, DataManager, XmlMerger, XmlDataLoader/JAXBUtil/XmlValidationHandler, StaticDataListener, the adapters, SpawnsData/SpawnGroup/SpawnTemplate/Spawn, EventTemplate, WalkerData, static_data.xml.
- The Python scans are in the session scratchpad under `sd/`.

## 0. Decisions at a glance

| Topic | Decision |
|---|---|
| Generator language | Python 3.12, stdlib only (tomllib, json, dataclasses, unittest). Needed only to regenerate; the CMake build never runs it |
| Java parsing | Own tokenizer plus a declaration-level recursive-descent parser. Method bodies are skipped by token-balanced brace matching and kept as text |
| Scope | Everything reachable from the unmarshal roots (not "files importing javax.xml.bind", not `model/templates`) |
| Intermediate form | `xmlmodel.json` (classes, properties, enums, hooks, trivial accessors). It feeds C++ generation, the verification scripts and the reports |
| Code shape | Enums fully generated. Data-only classes fully generated (`generated/` tree). Classes with behaviour are hand-written and `#include "X.xml.inc"` inside the class body. Binders live in generated `.bind.cpp` files |
| Generated code | Checked in. `xmlgen check` runs as a CTest and fails on drift |
| XML runtime | pugixml DOM per file, parsed in place. Per-import parallel parse, then binding in document order |
| Hooks | 113 afterUnmarshal hooks hand-ported as `void afterUnmarshal(xml::LoadContext&, const xml::XmlParent&)`, called post-order exactly like JAXB |
| Validation | No XSD at runtime. A strict binder instead (unknown element = error, as in Java; required attribute = error; unknown attribute = error in strict mode) |
| Lifetime | Templates are `const` and immortal after publish. `HolderRef<T>` is an atomic pointer. Reload leaks the old holder on purpose. Spawn/walker/event data are explicitly mutable and locked |

---

## 1. Generator input

### 1.1 Scope: reachability from roots

Of 792 files importing javax.xml.bind (src plus handlers), a fair number are not bound at all: `utils/xml/*`, files importing only `Unmarshaller`, and so on. Package lists are also wrong in both directions: 360 files are in model/templates, while skillengine, dataholders, questEngine, configs and enums in `model/` are outside it. So the generator starts from explicit roots and follows the type graph:

- Roots in `xmlgen.toml`:
  - `dataholders.StaticData`, which gives the 92 holders;
  - `dataholders.SpawnsData$UnprocessedSpawns` (saveSpawn);
  - `dataholders.PlayerExperienceTable` (DatabaseCleaningService);
  - config roots `configs.ingameshop.InGameShopProperty`, `configs.schedule.{RiftSchedule,SiegeSchedules,WorldRaidSchedules}`;
  - later, the handler-private roots (Send, Addskill, Combineskill, Deleteskill, Teleport_to_named) as a second output tree.
- Edges followed:
  - bound property types (element type, `type=`, `@XmlElements` choices, collection and array element types, IDREF targets, adapter value types such as `NpcEquipmentList`);
  - superclasses, bound or unbound (e.g. `NpcTemplate → CreatureTemplate → VisibleObjectTemplate`);
  - enums used by bound properties;
  - nested classes.
- Outputs: the reachable set, plus a report of JAXB-annotated classes that are not reachable. These are kept for review, since one could be a root I missed.

### 1.2 Why a custom tokenizer and declaration parser

- **javalang** is out: it parses whole files and supports Java 8 only. Bodies here use `_` unnamed variables (`computeIfAbsent(id, _ -> ...)` in SpawnsData.java), switch patterns (`case Reader reader ->` in JAXBUtil), `instanceof Player player` (EffectTemplate.java:516) and records (MotionData, FearEffect, ConfuseEffect). Those files would fail to parse.
- **Regex over lines** is too fragile:
  - `@XmlElements` spans 100 lines (Effects.java:31-130);
  - nested static classes need scope tracking (`SiegeSpawn.SiegeRaceTemplate.SiegeModTemplate`);
  - there are 2 simple-name collisions (ItemType, PetStatsTemplate) that need import resolution;
  - generic types contain commas;
  - initializers contain `new X[] {...}`.
- **tree-sitter-java** would work, but needs native wheels installed. It also parses far more than needed and tolerates errors silently (ERROR nodes). It stays as the fallback front end; the IR JSON is the interface, so switching front ends does not change the rest.
- **Custom parser (chosen).** About 700 lines of Python:
  - The tokenizer knows comments, string, char and text-block literals.
  - The parser covers package, imports (single, wildcard, static), class/enum/record/interface headers, annotations with argument trees (`name=`, `required=`, `type=X.class`, nested annotation arrays), field declarations with their initializer token spans, method signatures, and enum constants with argument spans and `@XmlEnumValue`.
  - Bodies are `{...}` spans kept as raw text.
  - Anything unexpected inside a class body raises an error with file:line. Nothing is silently skipped.
  - The annotation style is regular. A scan found no multi-variable annotated declarations, no constant expressions in annotation arguments, and no `@XmlElementRef`/`PROPERTY` access. Only the `@XmlElements` lists span lines.

### 1.3 What the IR records (the JAXB rules the generator implements)

Per class:
- FQN, C++ namespace and name, outer class, `abstract`, superclass;
- effective `XmlAccessType`. `@XmlAccessorType` is `@Inherited`, so for example `CaseHealEffect` inherits FIELD from EffectTemplate. Default when absent: PUBLIC_MEMBER;
- `@XmlRootElement` / `@XmlType(name)`;
- `@XmlTransient`;
- class-level `@XmlJavaTypeAdapter` (NpcEquippedGear);
- `afterUnmarshal`/`beforeUnmarshal`/`beforeMarshal` presence.

Per property:
- `javaName`, `cppName` (escapes C++ keywords, e.g. `PlayerInitialData.ItemType.template` becomes `template_`), access level;
- `kind`, one of:
  - `attribute`;
  - `element` (single);
  - `elementList`;
  - `choiceList` (`@XmlElements`, with (elementName → FQN) pairs in declaration order);
  - `wrapper` (`@XmlElementWrapper` name plus inner element);
  - `xmlListElement` / `xmlListAttribute` (`@XmlList`);
  - `attributeCollection` (a `List`/`Set`/array on an attribute is space-separated);
  - `idref` (single or array);
  - `adapter` (adapter FQN);
  - `methodSetter` (setXmlUid ×2, ZoneTemplate get/setXmlName);
- XML name. JAXB defaulting applies (field name for `@XmlAttribute` without `name`, implicit FIELD elements use the field name, e.g. `modifiers` and `change` on EffectTemplate);
- `required`;
- `implicit: true` for the 31 unannotated FIELD-bound fields;
- Java type (resolved FQN, boxed or primitive, collection kind List/Set/array) and the mapped C++ type (section 2.3);
- initializer (literal / enum constant / `new ArrayList<>()` / *unmapped expression*, which is an error unless mapped in the config);
- `@XmlID`.

Per method:
- name, access, static or not;
- `trivial: getter(field)` or `setter(field)` when the body is exactly `return [this.]f;` or `[this.]f = p;`;
- raw body text for the scaffold and the port checklist.

Per enum:
- constants in ordinal order, `@XmlEnumValue`, raw constructor arguments;
- whether it has methods.

Classification:
- **enum**;
- **data**: no hooks, no non-trivial methods, not abstract, not a `@XmlElements` base, not config-forced;
- **behaviour**: everything else. The report lists how many land in each bucket; the research estimates about 330 data and about 420 behaviour, including small ones.

### 1.4 `cpp/tools/xmlgen/xmlgen.toml` (hand-maintained policy, small)

```toml
java_src = "../../game-server/src"
roots = ["com.aionemu.gameserver.dataholders.StaticData", "...UnprocessedSpawns", "..."]
[deny_implicit]   # accidental FIELD bindings
fields = ["skillengine.effect.HostileUpEffect.tempHate", "dataholders.AssembledNpcsData.<map>", "..."]
[initializers]    # the 3 expressions JAXB evaluated in Java
"model.templates.item.ItemTemplate.levelRestrictions" = "std::vector<int8_t>(17, 1)"
[adapters]
"...SpaceSeparatedBytesAdapter" = { cpp = "std::vector<int8_t>", parse = "xml::adapters::parseSpaceSeparatedBytes" }
"...LocalDateTimeAdapter"       = { cpp = "std::chrono::local_time<std::chrono::milliseconds>", parse = "xml::adapters::parseLocalDateTime" }
"...NpcEquippedGearAdapter"     = { value_type = "...NpcEquipmentList", cpp = "...NpcEquippedGear", hand_written = true }
[runtime_mutable]  # atomics, forces unique_ptr storage of the owner
fields = ["model.templates.spawns.Spawn.eventTemplate", "model.templates.guide.GuideTemplate.activated"]
[force_behaviour]  # classes that must be hand-written even if they look data-only
[storage_by_pointer]  # element lists that need vector<unique_ptr<T>> (non-movable T)
```

### 1.5 How it runs

- `py cpp/tools/xmlgen/xmlgen.py generate` writes the following. The generator owns the whole `generated/` tree and prunes stale files.
  - `cpp/game-server/generated/aion/gameserver/**`: headers, `.xml.inc`, `.bind.cpp`, enum headers;
  - `cpp/game-server/generated/xmlmodel.json`;
  - `cpp/game-server/generated/xmlgen-report.md`, containing: the implicit bindings, boxed fields that have initializers, the classification, and per class the non-trivial Java methods with no C++ definition yet (the port checklist).
- `xmlgen.py check`: regenerates into a temp directory and diffs. Registered as a CTest only if `find_package(Python3 COMPONENTS Interpreter)` succeeds.
- `xmlgen.py scaffold <JavaFQN>...`: creates the hand-written `src/.../X.h` and `X.cpp` only if they are missing. The header includes the `.inc`; each non-trivial Java method gets a declaration comment and its Java body as a block comment in the .cpp.
- Refusal rule: if `src/` and `generated/` would both provide `aion/gameserver/.../X.h`, generation fails.
- CMake: `aion_add_library(aion_gameserver_staticdata SOURCE_ROOT src ...)` plus a second SOURCE_ROOT `generated`. The existing `GLOB_RECURSE CONFIGURE_DEPENDS` picks the files up.
- Why commit generated files instead of generating at CMake time:
  - the Java reference is frozen, so regeneration happens only when mapping rules change;
  - builds, IntelliSense and code review see real files;
  - a hobby machine does not need Python to build.

---

## 2. Output shape

### 2.1 Why a member include inside the hand-written class

Rejected alternatives:
- **(b) generated `XxxData` base plus hand-written derived class.** Java hierarchies interleave hand-written and generated layers (`EffectTemplate → DamageEffect → SpellAttackEffect`, 349 `extends`). Each level would become a sandwich `EffectTemplateXml ← EffectTemplate ← DamageEffectXml ← DamageEffect ← ...`. That doubles about 750 types, hurts grep-ability by Java name (CONVENTIONS), and makes factories, `getSimpleName` and debuggers show two names.
- **(c) scaffold once, then maintain by hand.** The type-mapping rules (optional vs value, list storage, keyword escapes, atomics) will change during phase 4. Re-applying a rule change by hand to 750 classes is exactly the bug source the generator exists to remove.

Chosen, (a):
- One C++ class per Java class, same name, same namespace.
- The data members, trivial accessors and binder friendship come from a generated `.inc` included at the top of the class body.
- Hand-written code owns everything else: logic, hooks, non-trivial getters, virtuals.
- A data-only class has no hand-written file at all.
- Rules: the `#include` is the first line of the body and must be followed by an explicit access specifier. Generated code always spells types fully qualified: `EffectTemplate` has a member named `change` and there is a namespace `skillengine::change`, so an unqualified `change::Change` would not compile.

### 2.2 Example: a skill effect (behaviour hierarchy)

`cpp/game-server/src/aion/gameserver/skillengine/effect/HostileUpEffect.h` (hand-written):

```cpp
#pragma once
#include "aion/gameserver/skillengine/effect/EffectTemplate.h"

namespace aion::gameserver::skillengine::effect {

/** Adds (temporary) hate to an npc. @author ATracer, Yeats */
class HostileUpEffect : public EffectTemplate {
#include "aion/gameserver/skillengine/effect/HostileUpEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;
	void calculate(model::Effect& effect) const override;
};

} // namespace
```

`cpp/game-server/generated/aion/gameserver/skillengine/effect/HostileUpEffect.xml.inc` (generated):

```cpp
// GENERATED by tools/xmlgen from skillengine/effect/HostileUpEffect.java - do not edit
	friend struct ::aion::gameserver::xml::XmlBinding<::aion::gameserver::skillengine::effect::HostileUpEffect>;
public:
	std::string_view javaClassName() const override { return "HostileUpEffect"; } // choice of Effects.effects
protected:
	int32_t tempDuration = 0; // @XmlAttribute(name = "temp_duration")
	int32_t tempValue = 0;    // @XmlAttribute(name = "temp_value")
	int32_t tempDelta = 0;    // @XmlAttribute(name = "temp_delta")
	// not bound: tempHate (xmlgen.toml deny_implicit) - per-cast state, see DEVIATIONS
```

`EffectTemplate.xml.inc` (excerpt) shows the type rules at work:

```cpp
	friend struct ::aion::gameserver::xml::XmlBinding<::aion::gameserver::skillengine::effect::EffectTemplate>;
public:
	virtual ~EffectTemplate() = default;
	virtual std::string_view javaClassName() const = 0;   // root of an @XmlElements hierarchy
protected:
	std::unique_ptr<::aion::gameserver::skillengine::effect::modifier::ActionModifiers> modifiers; // implicit FIELD <modifiers>
	std::vector<::aion::gameserver::skillengine::change::Change> change;                          // implicit FIELD <change>, absent == empty
	int32_t effectid = 0;
	int32_t duration2 = 0;                                  // required = true
	::aion::gameserver::skillengine::model::HitType hitType = ::aion::gameserver::skillengine::model::HitType::EVERYHIT;
	int32_t hitTypeProb = 100;
	std::unique_ptr<::aion::gameserver::skillengine::model::SubEffect> subEffect;
	std::unique_ptr<::aion::gameserver::skillengine::condition::Conditions> effectConditions;
	std::optional<::aion::gameserver::skillengine::model::HopType> hopType; // enum, no initializer: null when absent
	std::optional<std::vector<int32_t>> preEffects;         // int[] attribute: preeffect="" gives an empty array, not null (validatePreEffects still rolls the chance)
	bool noResist = false;
	...
public:
	int32_t getValue() const { return value; }             // trivial Java accessors, same names
	int32_t getEffectId() const { return effectid; }
	bool isNoResist() const { return noResist; }
	void setNoResist(bool noResist) { this->noResist = noResist; } // Java package-private -> public
```

The hand-written `EffectTemplate.h` includes this block and declares `calculate`, `applyEffect`, `startEffect`, ... as `const` virtuals.
- For phase 4, `applyEffect` gets a non-pure default that throws `UnsupportedOperationException("not ported: " + javaClassName())`. That lets all 170 effect shells (created by `xmlgen scaffold`) be instantiated, so skill_templates.xml loads before phase 5.
- The factory is generated in `skillengine/effect/Effects.bind.cpp`. It is a table sorted by element name: `{"absexppointhealinstant", &make<AbsoluteEXPPointHealInstantEffect>}, ..., {"limitedreduceDamage", ...}`. Names are case-sensitive.
- `Effects::afterUnmarshal` resolves `EffectType` from `javaClassName()`, replacing `getClass().getSimpleName().replace("Effect","")`.

### 2.3 Example: ItemTemplate (data + small logic, NONE access, XmlID method, adapter, hook)

Hand-written `model/templates/item/ItemTemplate.h`:

```cpp
class ItemTemplate : public VisibleObjectTemplate {
#include "aion/gameserver/model/templates/item/ItemTemplate.xml.inc"
public:
	int32_t getTemplateId() const override { return itemId; }
	const std::string& getName() const override { return name; }            // Java: name == null ? "" : name
	int64_t getItemSlot() const;                                               // itemGroup traits
	bool isClassSpecific(PlayerClass playerClass) const;
	const WeaponStats& getWeaponStats() const { return weaponStats ? *weaponStats : EMPTY_WEAPON_STATS; }
	const ItemUseLimits& getUseLimits() const { return useLimits ? *useLimits : EMPTY_USE_LIMITS; }
	void modifyMask(bool apply, int32_t filter);                               // ItemData::cleanup, before publish only
	...
private:
	int32_t itemId = 0;
	void setXmlUid(std::string_view uid);                                      // @XmlID @XmlAttribute(name="id", required=true)
	void afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent& parent);  // maxTuneCount rule
	static const WeaponStats EMPTY_WEAPON_STATS;
	static const ItemUseLimits EMPTY_USE_LIMITS;
};
```

Generated `.inc` (excerpt):

```cpp
	friend struct ::aion::gameserver::xml::XmlBinding<::aion::gameserver::model::templates::item::ItemTemplate>;
private:
	std::unique_ptr<::aion::gameserver::model::templates::stats::ModifiersTemplate> modifiers;
	std::unique_ptr<::aion::gameserver::model::templates::item::actions::ItemActions> actions;
	int32_t mask = 0;
	int32_t maxStackCount = 1;
	::aion::gameserver::model::templates::item::enums::ItemGroup itemGroup = ::aion::gameserver::model::templates::item::enums::ItemGroup::NONE;
	std::optional<::aion::gameserver::model::templates::item::ItemQuality> itemQuality;
	::aion::gameserver::model::Race race = ::aion::gameserver::model::Race::PC_ALL;
	std::string returnAlias;                              // String attr: absent == "" (census proves no present-empty values)
	std::vector<int8_t> levelRestrictions = std::vector<int8_t>(17, 1); // adapter + mapped initializer
	std::optional<std::vector<int8_t>> maxLevelRestrictions;
	std::optional<int32_t> robotId;                       // Integer
	std::unique_ptr<::aion::gameserver::model::templates::item::WeaponStats> weaponStats;
	...
public:
	int32_t getMask() const { return mask; }
	...
```

The generated binder is in `model/templates/item/item.bind.cpp`, one TU per Java package, about 60 TUs. Binder headers are included only by binder TUs, never by game code.

```cpp
bool XmlBinding<ItemTemplate>::attribute(ItemTemplate& o, BindContext& c, std::string_view n, const char* v) {
	switch (xml::nameHash(n)) {
	case "mask"_xh:     if (n == "mask") { o.mask = c.parseInt32(v); return true; } break;
	case "id"_xh:       if (n == "id") { c.seenRequired(0); o.setXmlUid(v); c.registerXmlId(v, o); return true; } break;
	case "restrict"_xh: if (n == "restrict") { o.levelRestrictions = xml::adapters::parseSpaceSeparatedBytes(c, v); return true; } break;
	...
	}
	return XmlBinding<VisibleObjectTemplate>::attribute(o, c, n, v); // unbound base returns false
}
bool XmlBinding<ItemTemplate>::element(ItemTemplate& o, BindContext& c, pugi::xml_node e, std::string_view n) {
	switch (xml::nameHash(n)) {
	case "actions"_xh: if (n == "actions") { c.bindSingle(o.actions, e, o); return true; } break; // repeated element: last wins, as in JAXB
	...
	}
	return XmlBinding<VisibleObjectTemplate>::element(o, c, e, n);
}
void XmlBinding<ItemTemplate>::finish(ItemTemplate& o, BindContext& c, const XmlParent& p) {
	c.checkRequired(o, REQUIRED /* {"id"} */);
	o.afterUnmarshal(c.load(), p); // static call on the concrete type = JAXB's nearest-declared hook (no shadowing exists in the sources)
}
```

### 2.4 Type mapping

| Java / annotation | C++ |
|---|---|
| `int long short byte float double boolean` | `int32_t int64_t int16_t int8_t float double bool` with the Java initializer, or 0 |
| `Integer/Long/Float/Boolean` without initializer | `std::optional<T>` |
| Boxed type with non-null initializer (`Integer respawnTime = 0`) | plain `T`, listed in the report |
| Enum attribute with initializer, or `required=true` | `E` (required is enforced by the binder) |
| Enum attribute otherwise | `std::optional<E>` (`opt == E::X` keeps working) |
| `String` attribute/element | `std::string`, absent == "". Becomes `std::optional<std::string>` when the census finds present-empty values used in a null check (e.g. `mail_templates.xml` has `name=""` 17×, plus `npc_shouts` `client_ai=""`, `panesterra.xml` `start_npc_ids=""`, `world_maps.xml` `flags=""`) or when config says so. The binder warns once per (class, attr) on an empty value into a non-optional string, which keeps the rule honest if data changes |
| Nested object element | `std::unique_ptr<T>` (nullable, stable address) |
| `List<T>` element, T concrete | `std::vector<T>`, reserved to the exact child count and bound in place, so element addresses never change. `vector<unique_ptr<T>>` if configured |
| `@XmlElements List<Base>` | `std::vector<std::unique_ptr<Base>>` plus a factory |
| Plain element list absent vs empty | Cannot differ in XML (a plain list exists only with at least one element), so `vector` with `empty()` replaces `== null` exactly |
| `@XmlElementWrapper` (8), `@XmlList` (28), attribute collections (47) | `std::optional<std::vector<T>>`: absent vs present-empty is observable (e.g. `hasConfigProperties()`) |
| `Set<T>` | `std::unordered_set<T>` (JAXB uses HashSet) |
| `T[]` | `std::vector<T>` (same optional rule) |
| `@XmlIDREF T` / `T[]` | `const T*` / `std::vector<const T*>`, patched after all holders load |
| Adapters | per config: SpaceSeparatedBytes → `vector<int8_t>` (Java `split(" ")` plus `Byte.parseByte`: double spaces are an error); LocalDateTime → `local_time<ms>` (ISO_LOCAL_DATE_TIME forms); NpcEquippedGear hand-written, IDREF list resolved, then initialized eagerly after IDREF patching (Java: lazy plus `synchronized`) |
| `@XmlID`/annotated setters | the binder calls the hand-written `setXmlUid(std::string_view)` / `setXmlName(std::string_view)` |
| `@XmlElement String/int` text content | concatenated PCDATA/CDATA of the element; numbers trimmed like JAXB |
| C++ keywords, Windows macro names | field `template` → `template_`; enum constants go through `WindowsMacroGuard.h` |

Scalar parsing (`xml::parseInt32` etc.) follows JAXB's `DatatypeConverterImpl` where cheap: trim XML whitespace, optional `+`, decimal only, range error.
- Floats use `std::from_chars`. It is correctly rounded like `Float.parseFloat`, but rejects Java-only forms (`1f`, hex, `INF`).
- Booleans accept `true/false/1/0`.
- Unknown enum names are an error. As I recall the JAXB RI source (not runnable here), JAXB would silently produce null there.
- Every place where C++ is stricter than JAXB is proven unreachable by the lexical census (section 4). It is not assumed.

### 2.5 Enums

Enums are always generated (e.g. `generated/.../item/enums/ItemGroup.h`):
- `enum class ItemGroup : uint8_t { NONE, NOWEAPON, SWORD, ... }` in Java ordinal order. Ordinals matter: `levelRestrictions[playerClass.ordinal()]` and packets.
- `template<> struct xml::EnumTraits<ItemGroup>` with a `names` array and `fromXml()` using a sorted table plus binary search, honouring `@XmlEnumValue` (ZoneAttributes).

This replaces magic_enum for these types. TribeClass (724 constants) and GroupDropType (438) are far outside magic_enum's range. commons' `EnumTransformer` should prefer `EnumTraits<E>` when specialized, for config fields of these enums.
- Enum behaviour (constructor data such as `ItemSlot.MAIN_OR_SUB.getSlotIdMask()`, methods) lives in a hand-written companion header (`ItemGroupInfo.h`) as free functions found by ADL: `getItemSubType(group)`.
- The generator lists enums that have methods or arguments so none is forgotten.

### 2.6 Memory layout (estimates, to be measured in the slice)

Real counts (Python iterparse, this pass):
- item_templates: 703,793 elements / 2,454,840 attributes (288k `add` stat modifiers, 102k templates);
- npc_templates: 448,810 / 1,611,572 (160k equipment `item` IDREFs);
- skill_templates: 145,014 / 512,712.

Estimated sizes:
- ItemTemplate is about 450-500 bytes inline (≈35 int/enum fields, 4 `std::string` at 32 B with SSO ≤15 chars, ~12 `unique_ptr`, 2 small vectors). The holder stores `std::vector<ItemTemplate>` contiguously (≈50 MB) plus sub-objects (modifier functions, actions, weapon stats), about 90-120 MB in total.
- NPCs ≈ 40-60 MB. Skills ≈ 20 MB. Spawn spots 132k × ~120 B plus runtime SpawnTemplates ≈ 30-40 MB.
- **All static data: roughly 300-500 MB resident**, unmeasured.

Rules:
- Index maps are `std::unordered_map<int32_t, const T*>` pointing into holder-owned storage. Hooks do not "null the list" (83 Java hooks do); the storage stays.
- No arena or pmr until measurements ask for it: pmr types would leak into every getter signature.
- Strings stay `std::string`. Most repeated values (`ai="aggressive"`, 39k) fit MSVC SSO.

---

## 3. Runtime loader

### 3.1 Import resolution (`dataholders/loadingutils/StaticDataImports`), same rules as XmlMerger

1. Parse `data/static_data/static_data.xml`. For each top-level `<import file=... singleRootTag=... recursiveImport=...>`, in document order:
2. Region override (XmlMerger.java:233): for `GSConfig::SERVER_COUNTRY_CODE` in {1 usa, 2 europe, 4 japan, 5 china, 6 taiwan, 7 russia}, use `<base>_<region><ext>` **only if it is a regular file**. Directory overrides never apply. Today only goodslists has variants.
3. File import: the root element tag must be one of the 92 StaticData element names, otherwise error (JAXB: unexpected element). If the same tag is imported twice, the second one replaces the first (JAXB field assignment); a warning is logged.
4. Directory import:
   - List `.xml` files depth-first pre-order. Directories are descended where they appear.
   - Entries at each level sorted by ordinal comparison of the uppercased UTF-16 name. This is what Java's `Files.find` sees on NTFS. **I verified that `os.scandir` (FindFirstFile) order equals this comparator for all 12 imported directories, 584 files.**
   - With `singleRootTag="true"`, the first file's root tag selects the holder and its root attributes are bound. Later files' root tags and attributes are skipped; their children are appended to the same holder instance.
   - `singleRootTag="false"` on a directory is rejected: XmlMerger.java:226 would write an unbalanced end tag. All 12 use `true`.
5. Ignored as in the merge: comments, whitespace-only text (pugixml default drops ws-only PCDATA), `xmlns*`/`xsi:*` attributes.
6. No `./cache/static_data.xml` and no CRC metadata. Deviation: a cache that exists only for JAXB.

### 3.2 Parse and bind pipeline

- For each import: read the files into buffers, then `pugi::xml_document::load_buffer_inplace` with `parse_default` (escapes, EOL normalization, attribute whitespace normalization as the XML spec and StAX do; CDATA kept). Files of one import are parsed in parallel on a small pool; spawns has 211 files and zones 149.
- Pre-pass: count children per vector property (across all files of the import) so vectors can be reserved.
- Bind in file order, children in document order: `XmlBinding<H>::attributes` (first root), then `element` for each child (post-order: attributes, children with their hooks, then the child's own hook), then `finish` for the holder once. Then the DOMs are freed.
- Peak DOM is about the largest import. For item_templates, with non-compact pugixml (64 B/node, 40 B/attribute): ≈45 MB nodes + 98 MB attributes + 57 MB buffer ≈ 200 MB transient. `PUGIXML_COMPACT` roughly halves that if needed.
- Errors carry `file:line:col` (pugixml offset converted to line/column) plus the element path, and throw `StaticDataException`. Startup aborts, as with JAXB's `XmlValidationHandler` (it throws on ERROR/FATAL).
- Unknown child element: error. JAXB RI reports unexpected children when an event handler is installed, and XmlValidationHandler throws.
- Unknown attribute: JAXB ignores it. C++ counts it, errors in `strict` mode (default for tests/CI) and warns once per (class, attr) otherwise. The XSD validation Java runs at startup would also reject it.
- `xml::LoadContext` gives hooks:
  - `data()` / `holder<H>()`: finished holders of the current load. When loading one holder for reload, it falls back to the published `DataManager` holder, mirroring `staticData != null ? staticData.itemData : DataManager.ITEM_DATA` in GlobalDropItem/ResultedItem/ItemRaceEntry.
  - `runAfterUnmarshalTask(fn)`: Java `registerForAsyncExecutionOrRun`. Runs inline, so `NpcData::init` is deterministic. Java logs `npcData.size()` in StaticData.afterUnmarshal while init may still be running on another thread.
  - `registerXmlId(string, obj)` / `addIdRef(slot, string)`: one document-wide string ID space for items and npcs, like JAXB. It is resolved after all holders; an unresolved or wrong-type target is an error.
  - `options()`: strict, holder filter for tests, `runHooks=false` for the round-trip dump.
- `XmlParent { std::type_index type; void* object; T* as<T>() }` covers the 2 parent-using hooks: `HouseAddress → HousingLand`, and `SpawnsData` checking `parent instanceof EventTemplate`.

### 3.3 Holders, DataManager, post-processing

- A generated `StaticData` struct holds the 92 `std::unique_ptr<H>` (parsed from StaticData.java) and a registry `{rootTag, HolderId, bind functions}`. There is a small hand-written dependency table for hooks that read other holders: `item_groups`, `decomposable_items`, `global_drops/rules` and `timed_events` need `item_templates`. Sequential loading in import order satisfies it; a later parallel mode schedules by it. `holder<H>()` asserts that H is a declared dependency.
- `StaticData::logCounts()`: hand port of StaticData.afterUnmarshal, the same ~92 "Loaded N ..." lines. These lines are what the oracle compares against.
- `DataManager::init()` (port of the DataManager constructor):
  1. load;
  2. patch IDREFs;
  3. post-process on the unpublished `StaticData&`, in Java order: `itemData->cleanup(itemCleanup)`, `globalDropData->processRules(npcs)`, `tradeListData->validateBuyLists(npcs)`, `skillData->validateMotions()`, `DecomposeAction::validateRandomItemIds(data)`. Java reads `DataManager.X` there; C++ passes holders explicitly;
  4. publish;
  5. log `##### [Static Data loaded in X seconds] #####`.
- Publish:

```cpp
template<class H> class HolderRef {            // Java: public static H X
	std::atomic<const H*> ptr{nullptr};
public:
	const H* operator->() const noexcept { return ptr.load(std::memory_order_acquire); }
	const H& operator*() const noexcept { return *operator->(); }
	void publish(std::unique_ptr<H> h);        // the previous holder goes to StaticDataRetirement (kept forever)
};
struct DataManager {
	static inline HolderRef<ItemData> ITEM_DATA; // call sites port as DataManager::ITEM_DATA->getItemTemplate(id)
	static inline MutableHolderRef<SpawnsData> SPAWNS_DATA; // non-const, internally locked, never replaced
	...
};
```

### 3.4 Validation

- No XSD validation in the server: no Xerces/libxml2 dependency, and libxml2 support for `xs:unique`/`xs:key`/abstract types is unverified.
- Java's async XSD check effectively makes XSD errors fatal at startup (GameServer.java:177). The strict binder therefore keeps parity for what matters: unknown elements, unknown attributes, the 319 `required=true` attributes and 62 required elements, malformed numbers, unknown enum constants.
- The 4 `xs:unique` and 2 `xs:key` constraints (spawns.xsd:11, arcadelist.xsd:19, house_npcs.xsd:17) become explicit checks in those holders' hooks.
- XSD-vs-annotation differences (396 `use="required"` vs 319 `required=true`) are reported by V1 (section 4). They are not enforced.

### 3.5 Expected load time and memory (estimates)

- Python ElementTree parses item_templates in 2.3 s; pugixml is typically 5-10× faster.
- Estimate for all 156 MB in Release, single-threaded: parse ≈0.5-1 s, bind and allocation ≈1-1.5 s, hooks and post-processing ≈0.3 s, **≈2-3 s total**. With the per-import parallel parse, ≈1.5-2 s.
- Debug builds (MSVC `_ITERATOR_DEBUG_LEVEL=2`) may be 5-10× slower. Use the RelWithDebInfo preset for running the server; unit tests load only the holders they need (`options.holders`).
- Resident ≈300-500 MB, peak +≈200 MB (DOM). No binary cache unless the slice measures something much worse.

### 3.6 Lifetime, //reload and the mutable families

1. **Immutable templates (≈85 holders).** `const` after publish. Template methods are `const`, so the compiler enforces immutability and finds Java's hidden template mutations. Found so far:
   - `HostileUpEffect.tempHate`, per-cast state written from `calculate` (a Java race). It moves to per-effect state (Effect reserved value or a field). DEVIATIONS entry.
   - `GuideTemplate.activated` → `std::atomic<bool>` via `runtime_mutable`.
   - `Spawn.eventTemplate` → `std::atomic<const EventTemplate*>`.
2. **Reload by replacement:** QUEST_DATA, SKILL_DATA, ITEM_DATA (+cleanup), CUSTOM_NPC_DROP, UPGRADE_ARCADE_DATA, DECOMPOSABLE_ITEMS_DATA. Build, post-process, `publish`. The old holder is **never freed**; that is the Java GC semantics, since live Items, NpcEquippedGear and PlayerInitialData IDREFs keep old templates. Cost: one extra holder's memory per //reload (items ≈100 MB). DEVIATIONS entry.
   - The in-place setters `XML_QUESTS.setData`, `NPC_SKILL_DATA.setNpcSkillTemplates` and `EVENT_DATA.setEvents` become replacements too. That is safer than Java's unsynchronized in-place update and behaves the same for readers.
   - If decision A prefers shared ownership, the upgrade path is `shared_ptr<const H>` plus aliasing `shared_ptr<const ItemTemplate>`; the 621 call sites stay the same.
3. **Explicitly mutable, locked:**
   - **SpawnsData:** the JAXB input (`SpawnMap/Spawn/SpawnSpotTemplate`) is generated and immutable. The runtime indexes (`allSpawnMaps`, base/rift/siege/vortex/ahserion maps) hold **hand-written runtime classes** `SpawnGroup`/`SpawnTemplate`, which are not generated and not const. A `std::recursive_mutex` guards the maps (saveSpawn is `synchronized` and calls helpers), and getters return snapshots.
     - Ownership proposal, for decision A: a SpawnGroup owns its SpawnTemplates (`vector<unique_ptr>` behind its mutex; `addSpawnTemplate` is synchronized in Java). A reference to a SpawnTemplate shares the group's refcount (shared_ptr aliasing constructor, or an intrusive Ref forwarding to the group). That breaks the template↔group cycle without weak pointers. Event stop (`removeEventSpawnObjects`) and per-handler `new SpawnGroup` stay safe while NPCs live.
     - The 47 handler setter calls on shared static SpawnTemplates across instances keep Java's shared semantics; data-race safety depends on decision A's threading model.
   - **WalkerData:** `addTemplate`/`saveData` (FixPath) under a mutex; `walkerlistData` stays insertion-ordered with first-wins.
   - **ZoneName::createOrGet:** a global intern table behind a mutex.
4. **Write-back** (`saveSpawn` with Spawn/SpawnSpotTemplate before/afterMarshal, `WalkerData::saveData`): hand-written pugixml writers, after the load path is green.

---

## 4. Verification (no Java runtime)

Tools in `cpp/tools/staticdata-verify/` (Python, stdlib `xml.etree` only). C++ side: `aion_gameserver_staticdata_tests`, whose full-data tests are labeled `data`, plus `aion_game_server --dump-static-data=<dir>`.

| # | Check | Independent of the generator? | What it catches |
|---|---|---|---|
| V0 | Generator unit tests on Java fixtures: multi-line `@XmlElements`, nested static classes, generics with commas, text blocks, records, `_` lambdas, inherited accessor type, implicit FIELD elements | n/a | parser bugs |
| V1 | IR vs XSDs: for each `@XmlType(name)` matched to an `xs:complexType`, compare attribute and element name sets and required flags; allowlist the known differences (1,428 vs 1,429 attributes) | yes (XSDs) | missed or misnamed annotations, wrong inheritance flattening |
| V2 | **Count oracle** `oracle_counts.py`: applies XmlMerger import rules independently and prints the same ~92 "Loaded N ..." lines from per-holder rules. Examples: item_template distinct `id` (last wins); WalkerData distinct `route_id` (**first** wins, putIfAbsent); SpawnsData = distinct `spawn_map@map_id`; TownSpawnsData = town spawns summed over levels; SkillTreeData = sum of list sizes; XML quests 4,184. The C++ `logCounts()` output must diff-equal | yes | holder semantics, import order and override bugs |
| V3 | **Coverage totals:** the C++ loader counts per (element tag) bound and per (tag, attr) consumed/ignored; Python counts per (tag) and (tag, attr) in the imported files. Equal totals, 2.1 M elements and 6.6 M attributes. Nothing silently dropped | yes | unbound attributes, wrong import set |
| V4 | **Lexical census** `census.py` using `xmlmodel.json` types: every occurring value of int/byte/float/bool/enum/list properties is classified; report must be empty or allowlisted. Flags: whitespace/`+`/leading zeros, byte out of [-128,127] (JAXB narrows; C++ errors), non-`true/false/1/0` booleans, Java-only float forms, unknown enum constants, present-empty String/list attributes (drives the optional rule), double spaces in SpaceSeparatedBytes, IDREF targets whose id string is also used by an npc_template (JAXB's shared ID space) | data is independent; types come from the IR | proves JAXB/C++ lexical parity on the actual data |
| V5 | **Value round-trip:** C++ loads with `runHooks=false` and dumps canonical lines `holder/tag[i]/.../tag[j]@attr=value` for present values (ints decimal, floats as shortest float32 round-trip, enums by name, lists space-joined) via a generated dump visitor. Python emits the same from the XML. Diff must be empty. A generated test also compares default-constructed objects against the IR initializer table | mostly: a swapped attr→member binding shows up as a value mismatch | every one of 6.6 M values lands in the right member |
| V6 | **Golden unit tests** with small fixture XMLs: region override (file only); DFS/uppercase order; singleRootTag root attributes from the first file only; `@XmlElementWrapper` absent vs empty; `@XmlList`; every one of the 287 choice element names constructs the class with the right `javaClassName()` (generated test); post-order hook order; IDREF resolve plus unresolved error; SpaceSeparatedBytes; LocalDateTime forms; NpcEquippedGear slot and mask assignment; ZoneName interning and `String.hashCode` ids; Effects noResist normalization; ItemTemplate maxTuneCount; ItemData manastone maps and cleanup masks; NpcData stat fill-in for 3 npcs (expected values computed by hand from NpcStatCalculation); custom spawn override (Season_Agrints.xml); ZoneData weather zone numbering; GlobalDropData.processRules | yes | hook semantics |
| V7 | **Fingerprints:** ~50 hand-picked templates (item 100000001, a weapon, a stigma, an npc with equipment, skill with sub effects, a siege spawn) with expected values read from XML and Java | yes | end-to-end sanity |

Acceptance criterion for phase 4 static data: V2 diff-equal, V3 totals equal, V4 empty, V5 empty diff, V1 allowlist reviewed, V6/V7 green, strict mode clean. A user-supplied Java startup log would be a bonus check, not a requirement.

---

## 5. Work breakdown

Sizes are focused work sessions. WP2 and WP3 can run in parallel.

| WP | Content | Size | Output |
|---|---|---|---|
| 1 | Prerequisites: `pugixml` in vcpkg.json; `cpp/game-server/CMakeLists.txt` with `aion_gameserver_staticdata` (+ `generated` root) and a test exe; `AION_GAMESERVER_JAVA_DIR` define; docs (PORTING_PLAN line 36 scope, CONVENTIONS "Static data" section, DEVIATIONS entries listed below) | 0.5 | builds |
| 2 | Generator front end: tokenizer, declaration parser, import/nested-type resolver, JAXB rules (inherited accessor type, implicit FIELD, PUBLIC_MEMBER pairs, defaults), reachability from roots, `xmlmodel.json`, report, V0 | 3-4 | IR for all roots |
| 3 | Independent Python: V2 import resolver plus count oracle rules, V3 tag/attribute totals, V4 census, V1 XSD cross-check | 2 | expected-counts file, census report |
| 4 | C++ runtime: converters, `EnumTraits`, `BindContext`/`LoadContext`/`XmlParent`, `ElementFactory`, IDREF patcher, `StaticDataImports`, sequential loader, stats counters, error locations, `HolderRef`, strict mode; unit tests on fixture XML | 2-3 | runtime library |
| 5 | Generator back end: enum headers; data-only headers; `.xml.inc`; per-package `.bind.cpp`; choice factories; dump visitor; `check`; `scaffold` | 3 | generated tree |
| 6 | **Vertical slice** (below) | 2-3 | first holders, measurements |
| 7 | Remaining simple map-building holders (~60): mostly scaffold, then port the hook bodies; batchable in parallel | 3-4 | |
| 8 | Items complete (ItemActions 32 shells, StatFunction/ModifiersTemplate, conditions shells, item_groups/decomposables/purifications/random bonuses) and npcs (NpcData::init with NpcStatCalculation; DialogAction check waits for the DialogAction generator, warn loop stubbed) | 2 | |
| 9 | Skills: SkillTemplate, 170 effect shells with throwing defaults, conditions (24), properties, actions, periodic actions, modifiers, motion; Effects hook | 2-3 | |
| 10 | Spawns (SpawnMap/Spawn/Spot + hand-written SpawnGroup/SpawnTemplate + SpawnsData indexes), zones (Area classes, ZoneName), walkers, quest_data + quest_script_data (16 XMLQuest data classes), global drops, events (nested SpawnsData, wrappers, LocalDateTime), housing (11 choices, HouseAddress parent), town spawns | 3-4 | |
| 11 | DataManager post-processing and publish; full V1-V7 green; timing and memory in docs; decide on parallel holder loading and compact pugixml | 2 | phase-4 static-data criterion met |
| later | //reload (after decision A), saveSpawn/WalkerData writers, config XML roots, handler-private XML | | |

### First vertical slice (WP6)

Holders, chosen to hit every risky mechanism once:
1. **world_maps.xml** (161 maps): plain attributes and elements, enum WorldMapType (150 constants). Includes a present-empty `flags=""`.
2. **tribe/tribe_relations.xml**: `@XmlList` of TribeClass (724 constants), the enum-table stress case.
3. **npc_walker/** (13 files, 6,449 routes, 112,765 steps): directory import, DFS order, singleRootTag, first-wins duplicate route hook, volume per element.
4. **items/item_templates.xml** (57 MB, 102k): the behaviour-class `.inc` shape. It also covers:
   - the unannotated superclass;
   - `@XmlID` setter;
   - SpaceSeparatedBytes adapter with mapped initializer;
   - `unique_ptr` sub-elements;
   - two polymorphic lists (ItemActions 32 shells via scaffold, ModifiersTemplate stat functions with Conditions shells);
   - ItemData hook (manastone maps), `ItemData::cleanup` with items/item_restriction_cleanups.xml;
   - the largest DOM (memory peak) and bind cost (time).
5. **player_initial_data.xml**: `@XmlIDREF` into items and patching after load.

Stretch: `skills/skill_templates.xml`, to prove the 170-choice factory and throwing effect shells.

Slice exit criteria:
- V2 lines for these holders equal; V3 totals equal; V4 empty (or allowlisted with a decision); V5 diff empty;
- golden tests for DFS order, IDREF and adapters;
- strict mode clean;
- Release/RelWithDebInfo/Debug load time and peak/resident memory recorded in `docs/`;
- `xmlgen check` test green;
- the hand-written ItemTemplate/ItemActions shells reviewed as the template for WP7-10.

### DEVIATIONS.md entries this design introduces

- No merged `./cache/static_data.xml` or CRC metadata.
- No XSD validation; a strict binder instead (unknown attributes rejected in strict mode, where JAXB ignores them; unknown enum constants rejected).
- Directory order: DFS with uppercase-ordinal names (matches NTFS; Java on Linux is readdir order).
- `NpcData.init` runs inline.
- Post-processing runs before publish.
- //reload keeps old holders forever; in-place reload setters become replacements.
- `HostileUpEffect.tempHate` is per effect.
- `NpcEquippedGear` is initialized eagerly.
- Generated enums use `EnumTraits` instead of magic_enum.
- Java package-private accessors are public.

## Open questions

- //reload for items/skills/quests/npc skills/events/custom drops/arcade/decomposables: keep it with 'old holder is never freed' (about 100 MB extra per //reload of items), or drop it from the first milestone and add it after decision A?
- Are you OK with a Python 3.12 (stdlib-only) generator and verification tooling in cpp/tools, with about 1,000 generated C++ files (member includes, binders, enum headers) committed under cpp/game-server/generated and checked for drift by a CTest?
- Is the 'generated member block #include'd inside the hand-written class body' shape acceptable to you stylistically? The alternatives are a generated XxxData base class per Java class (about 1,500 types) or scaffold-once-then-hand-maintain.
- Strictness default: unknown XML attributes are an error in strict mode (JAXB ignores them, but Java's startup XSD validation would reject them), and unknown enum constants are always an error. Should the server itself run strict, or only tests/CI with warnings at runtime?
- Can templates become const after load? That makes Java's hidden template mutations compile errors that must be fixed or explicitly marked (HostileUpEffect.tempHate per effect, GuideTemplate.activated and Spawn.eventTemplate as atomics).
- Methods of generated enums: free functions in a companion header found by ADL (getItemSubType(itemGroup)) instead of Java's itemGroup.getItemSubType(). Acceptable naming, or would you prefer a traits struct (ItemGroupInfo::of(g).subType)?
- Phase-4 shells: can effect/item-action/condition virtuals temporarily default to throwing 'not ported' so skills and items load before the phase-5 logic exists?
- Spawn family ownership (feeds decision A): do you agree that a SpawnTemplate reference shares its SpawnGroup's refcount (aliasing), with the group owning its templates, to avoid the template/group cycle?

## Risks

- The custom Java declaration parser could miss an unusual construct. Mitigations: it fails loudly on anything unexpected in class bodies; V1 cross-checks the parsed model against the 96 independent XSDs; tree-sitter-java can replace the front end without changing the IR.
- Include-in-class-body is unusual. Hand-written code must put an explicit access specifier after the include, and clang-format/IntelliSense behaviour should be confirmed in the slice before 400 behaviour classes adopt it.
- Stable addresses depend on binding vector<T> elements in place into exactly reserved storage. A reallocation after a child hook stored a parent pointer would dangle. Needs a debug assertion that capacity never changes after the pre-pass, and the storage_by_pointer override for non-movable types (atomics, mutexes).
- The null/empty/default rules (optional enums and boxed types, String absent == empty, attribute lists as optional) are proven only for the current data by the lexical census. Later data edits could break them. The binder's empty-value warnings reduce but do not remove that risk.
- My JAXB RI recollections (unknown enum gives null, byte narrowing, unexpected child reported as ERROR, unknown attribute ignored) cannot be run without a JDK. The design makes them irrelevant only where the V4 census shows the data never hits them.
- Load time and memory are estimates (2-3 s Release, 300-500 MB resident, about 200 MB transient DOM for item_templates). MSVC Debug builds may be 5-10x slower, which hurts the debugging loop. The slice must measure these before WP7.
- Leak-on-reload grows memory with each //reload; acceptable for a hobby server, not for long uptime with frequent reloads. It must be revisited together with decision A.
- Mutable spawn data (47 handler setter calls on SpawnTemplates shared across instances, event add/remove of SpawnGroups, SpawnGroup.poolUsedTemplates never cleared) remains a data race under free threading. The static-data design only isolates it; the threading and ownership decision A must resolve it.
- About 400 hand-written behaviour headers (scaffolded) will be touched again in phase 5 when methods are ported. Generated member blocks must stay stable, or member-rule changes ripple through all of them.
- Hooks that read other holders or global state (GlobalDropItem/ResultedItem/ItemRaceEntry read items, NpcData.init uses NpcStatCalculation and DialogAction, ZoneData builds Area objects with ZoneName ids) pull non-data code into phase 4. DialogAction depends on the separate SM_SYSTEM_MESSAGE/DialogAction generator.
- Directory order parity is proven only for the current files on NTFS (os.scandir matched the uppercase-ordinal comparator for all 584 files). Unicode or special-character file names added later could differ from NTFS upcase-table ordering.


## Amendments after the user's decisions (2026-09-13)

These amendments take precedence over the text above.

# Required amendments to static-data.md (D1 free-threaded, D2 confirmed, D3 reload deferred)

## 1. Status and decisions
- Status line: "Decisions D1 (free-threaded runtime), D2 (Python stdlib generator, member blocks `#include`d, committed with a drift test) and D3 (//reload deferred until after M5a) apply."
- Remove the open questions these decisions answer: reload, Python generator, member-include shape, const templates, spawn family ownership.
- Keep the enum-methods naming question and the phase-4 throwing-shell question.

## 2. §0 table, row "Lifetime"
"Templates are `const` and immortal after publish. `HolderRef<T>` is an atomic pointer published once by `DataManager::init`. //reload is deferred (D3). A small allow-list of runtime-mutable template fields is generated as `mutable runtime::Field<T>`. The spawn, walker and event families are concurrent (runtime-architecture.md §9)."

## 3. §1.4 `xmlgen.toml`
- `[runtime_mutable]` generates `mutable ::aion::gameserver::runtime::Field<T>`. `Field` is non-movable, so owning element lists keep `storage_by_pointer`. Entries: `Spawn.eventTemplate` (`Field<const EventTemplate*>`), `GuideTemplate.activated` (`Field<bool>`), walker `RouteStep.z` (`Field<float>`, FixPath.java:121-125).
- New output `generated/staticdata-classes.json`: the reachable K1 class set with any runtime-mutable fields. `fieldmap.py` consumes it; K1 classes are exempt from K4 rules and from escape analysis.
- JAXB classes never derive `RefCounted`; the generator does not emit `create()` for them. Runtime-constructed template types (SpawnTemplate family) are the exception below.

## 4. §3.2 Parse and bind pipeline
- Per-import parallel parsing runs on ForkJoin startup workers inside `runtime::TaskScope`. Parsing performs no pointer loads through `Field`/shims, so under lazy epoch publication it never pins reclamation.
- SpawnsData, EventData and TownSpawnsData bind sequentially in import order (determinism, not a thread restriction).
- `LoadContext` and other load-time helpers are K5 confined (inferred or listed in `fieldmap.toml`).
- `LoadContext::runAfterUnmarshalTask` runs inline.

## 5. §3.3 Holders, DataManager
- `HolderRef::publish` once per holder at startup.
- `StaticDataRetirement` and all reload code paths move to "Later (after M5a)"; keep the API shape (atomic pointer, old holders never freed) so reload does not touch the 621 call sites.
- `MutableHolderRef<SpawnsData>` loses its internal `std::recursive_mutex` (§6 below).

## 6. §3.6 rewrite: "Lifetime and the mutable families"
1. **Immutable templates**, with hidden mutations handled: `HostileUpEffect.tempHate` per effect (DEVIATION); `GuideTemplate.activated`, `Spawn.eventTemplate` and walker `RouteStep.z` as `Field<T>`.
2. **Reload deferred (D3).** Java in-place setters used only by reload (`XML_QUESTS.setData`, `NPC_SKILL_DATA.setNpcSkillTemplates`, `EVENT_DATA.setEvents`) are stubbed with `AION_UNPORTED` in the C2 chunk. The "reload by replacement" text moves to "Later".
3. **Spawn family (concurrent):**
   - `SpawnGroup` is RefCounted (`SpawnGroup::create`, protected destructor). It owns its templates in `runtime::PartList<SpawnTemplate>` (append-only, stable addresses) under the group Monitor (`addSpawnTemplate` synchronized, SpawnGroup.java:121). Non-final fields are `Field<T>`.
   - `SpawnTemplate` and its variants are `OwnedPart`s of the group; `Ref<SpawnTemplate>` retains the group. The generator emits constructors and `Field<T>` setters for the ~15 runtime-constructed template types (47 handler setter calls). The cross-instance sharing quirk is kept.
   - `poolUsedTemplates` under `SYNCHRONIZED(*this)` (SpawnGroup.java:163-189), cleared in `destroyInstance` (DEVIATION).
   - SpawnsData indexes are `runtime::ConcurrentHashMap<int32_t, Ref<RcCopyOnWriteArrayList<Ref<SpawnGroup>>>>`. The Java nested `allSpawnMaps.compute` → `mapSpawns.computeIfAbsent` (SpawnsData.java:83-96) ports as written: compute callbacks run under reentrant stripe Monitors (runtime-architecture.md §4.1). Getters return lock-free weakly consistent views or snapshots.
   - Event start/stop on the cron runner (Event.java:88-149, SpawnsData.java:82-99,438-444) adds and removes groups while live Npcs hold `const Ref<SpawnTemplate>`. The event end tasks are `PinnedCallback`s.
   - `saveSpawn` → `SYNCHRONIZED(*this)`.
4. **WalkerData:** `ConcurrentHashMap` shim with first-wins `putIfAbsent`; `addTemplate`/`saveData` (FixPath on the instant pool) under `SYNCHRONIZED(*this)`.
5. **ZoneName::createOrGet:** intern table behind a `runtime::RankedMutex<LockRank::CONTAINER_SLOT>` (no callbacks under it).
6. **NpcEquippedGear:** eagerly initialized after IDREF patching.
7. **Write-back** (saveSpawn, WalkerData::saveData): after the load path is green.

## 7. §5 Work breakdown
- WP4: `HolderRef` has no runtime dependency. Runtime-mutable `Field<T>` depends on the P4-02a headers; a temporary `using Field = std::atomic` alias is acceptable until then.
- WP10: uses the P4-02a collection shims, `OwnedPart`, `PartList` and `create()`.
- Row "later": "//reload (after M5a, D3), saveSpawn/WalkerData writers, config XML roots, handler-private XML".

## 8. DEVIATIONS entries
- Replace "//reload keeps old holders forever; in-place reload setters become replacements" with "//reload of static data is not available until after M5a (D3); when added, old holders are kept forever".
- Add "`SpawnGroup.poolUsedTemplates` is cleared when an instance is destroyed".

## 9. Risks
- Replace the mutable-spawn-data race risk with: "Mutable spawn data is memory-safe through `Field<T>`, Monitors and concurrent shims. Java's cross-instance template mutation and its check-then-act races are kept by design."
- Move the leak-on-reload risk to "Later".
- Memory: runtime SpawnTemplates grow by `Field` and RefCounted headers (≈ +40-60 B each); re-measure in WP6/WP10.

## 10. Open question about decision A
Delete it: spawn family ownership is decided (group-owned templates with forwarding refcount).

