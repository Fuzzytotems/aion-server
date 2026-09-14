# static-data-jaxb

## Summary
Static data is loaded by one JAXB unmarshal of a merged document. XmlMerger (dataholders/loadingutils/XmlMerger.java) resolves the 92 <import> directives in data/static_data/static_data.xml: 80 are single files and 12 are directories, all with singleRootTag="true". It applies a region override (<name>_<region>.xml chosen by GSConfig.SERVER_COUNTRY_CODE; only goodslists has variants, 6 of them) and writes ./cache/static_data.xml with a CRC32 metadata file. JAXBUtil then unmarshals it into StaticData, which has 92 holder fields. XSD validation of the merged file runs on a background thread only when the cache changed, and a failure stops server startup (GameServer.java:177).

Size of the binding model:
- 787 source files import javax.xml.bind. They declare 854 types, of which 750 classes and 87 enums carry JAXB annotations.
- About 2,000 bound properties: 1,428 @XmlAttribute, 819 @XmlElement (173 of them with type=), 31 implicit FIELD-access fields, 3 annotated methods.
- 12 @XmlElements polymorphic lists with 287 element-name-to-subclass mappings. Skill effects alone have 170. There is no xsi:type anywhere in the data.
- Rarely used features: 8 @XmlElementWrapper, 28 @XmlList, 3 @XmlEnumValue, 2 @XmlID with 2 @XmlIDREF, and 7 @XmlJavaTypeAdapter uses of 3 adapters.
- Features never used: @XmlValue, @XmlElementRef, @XmlAnyElement, @XmlMixed, defaultValue=.

The features used are narrow and regular, so data binding can almost all be generated. The hard parts are not the binding:
- 113 afterUnmarshal hooks (about 1,000 lines). They build indexes, validate cross-references to already-loaded holders, change templates, or start async work.
- About 6,000 lines of real game logic inside 245 JAXB classes (skill effects, item actions, conditions, quest XML handlers). Data and behavior live in the same class, so a generator must produce members and parse code for classes that humans otherwise write.
- A DataManager post-load phase that changes templates (ItemData.cleanup, GlobalDropData.processRules, NpcData stat fill-in).
- Runtime mutation: //reload replaces 9 holders, saveSpawn and FixPath change holders and write XML back.

Data volume: about 156 MB in 665 XML files, about 2.1 M elements and 6.6 M attributes. Largest counts: 102,009 item templates, 63,287 npc templates, 131,896 spawn spots in 22,022 spawn groups, 112,765 walker route steps, 13,570 skills, 12,494 recipes, 8,043 quests, 3,978 zones.

Java load time and heap size could not be checked: there is no JDK and no log. The XSDs (96 files, 1,429 xs:attribute, 744 complexTypes) closely mirror the Java classes. But Java field names, class names, defaults and behavior only exist in the annotations and initializers.

Recommendation: keep the plan's generator from the Java annotations, but give it a clear shape:
- It generates member declarations, a per-class XML bind/parse function running on a small hand-written XmlBinder runtime over pugixml, enum name tables, and factories for the polymorphic lists.
- Everything else is hand-ported: afterUnmarshal hooks, holder APIs, adapters, IDREF resolution, the import/merge logic, marshalling, and reload.
- Parse file by file (no merged cache file). Do not add a binary cache until measurements show a need.

## Inventory
## 1. Loading pipeline (Java)
| Step | Where | What it does |
|---|---|---|
| Pre-load JAXB context | GameServer.java:93 `JAXBUtil.preLoadContextAsync(StaticData.class)` | Starts building the reflection model early |
| Merge | XmlMerger.merge() | Resolves `<import file=.. singleRootTag=.. recursiveImport=..>`. Writes ./cache/static_data.xml plus a .properties file with CRC32 per file, and skips the rewrite if nothing changed |
| Region override | XmlMerger.java:89 COUNTRY_REGION, :233 applyCountryOverride | Uses `x_<usa/europe/japan/china/taiwan/russia>.xml` if it exists (codes 1,2,4,5,6,7). Only goodslists/ has variants (6) |
| Directory import | XmlMerger.java:219-226, XmlUtil.java:114 `Files.find` | Recursive `.xml` walk in **unspecified OS order**. With singleRootTag the first file's root encloses all files. Line 226 writes an end tag that only balances when singleRootTag=true; all 12 directory imports set it |
| Unmarshal | XmlDataLoader.java:51 → JAXBUtil.deserialize | One StAX pass. XmlValidationHandler throws on ERROR/FATAL events. **No schema passed** (for speed) |
| Validation | XmlDataLoader.java:50,68 | Async XSD validation of the merged file against static_data.xsd, only when the cache was rewritten. GameServer.java:177 aborts startup on failure |
| Hooks | StaticData.beforeUnmarshal installs StaticDataListener; 113 afterUnmarshal run in document order | NpcData.init runs async (StaticDataListener.java:29); StaticData.waitForAfterUnmarshalTasksToFinish waits for it |
| Post-processing | DataManager.java:229-234 | ITEM_DATA.cleanup() changes item masks; GLOBAL_DROP_DATA.processRules(npcs) expands name rules into npc id lists; TRADE_LIST_DATA.validateBuyLists; SKILL_DATA.validateMotions; DecomposeAction.validateRandomItemIds |
| Publish | DataManager: 91 `public static` holder fields | 621 `DataManager.X` references in 295 files (ITEM_DATA 86, QUEST_DATA 78, SKILL_DATA 58, NPC_DATA 29, SPAWNS_DATA 25, ...) |

static_data.xml has 92 real imports (80 files, 12 dirs: ai, events/timed_events, global_drops/rules, npc_skills, npc_walker, npcs, pet_skills, quest_script_data, skill_tree, spawns, town_spawns, zones). StaticData has 92 @XmlElement holder fields.

## 2. JAXB feature usage (game-server/src, regex scan + structural parse; data/handlers adds 5 files / 11 root+type annotations)
| Feature | Uses | Files | Notes |
|---|---|---|---|
| Files importing javax.xml.bind | 787 | | 44,488 lines total, including logic |
| Type declarations in those files | 854 | | 762 classes, 89 enums, 3 records |
| Classes with any JAXB annotation | 750 | | 25 abstract, 349 `extends`; inheritance depth among JAXB types: 420×0, 221×1, 112×2, 9×3; 66 nested |
| Enums with JAXB annotation | 87 (86 @XmlEnum) | 85 | JAXB maps any enum by `name()`. 114 distinct enum types used in 192 properties |
| @XmlRootElement | 142 | 137 | |
| @XmlType | 670 | 626 | 65 propOrder (only matters when writing XML) |
| @XmlAccessorType | 711 | 665 | FIELD 685, NONE 26; 40 bound classes have no accessor type (PUBLIC_MEMBER default) |
| @XmlElement | 819 | 257 | 173 with `type=` |
| @XmlElements | 12 | 12 | 287 mappings: Effects 170, ItemActions 32, Conditions 24, XMLQuests 16, HousingObjectData 11, QuestOperations 8, ModifiersTemplate/QuestConditions/MailTemplate/ActionModifiers 5 each, Actions 4, PeriodicActions 2 |
| @XmlAttribute | 1428 | 412 | 319 `required=true` (JAXB does not enforce this on unmarshal; the XSD does). Collection-typed attributes are implicit space-separated lists |
| @XmlElementWrapper | 8 | 4 | EventTemplate×3, GlobalRule, HousingLand×2, WorldRaidLocation×2 |
| @XmlList | 28 | 15 | Tribe×6, XMLStartCondition×4, GlobalNpcExclusionData×5 ... |
| @XmlEnumValue | 3 | 1 | ZoneAttributes |
| @XmlID / @XmlIDREF | 2 / 2 | | IDs: ItemTemplate.setXmlUid (ItemTemplate.java:143), NpcTemplate.setXmlUid (NpcTemplate.java:182) share one document-wide ID space. IDREFs: NpcEquipmentList.items → ItemTemplate[], PlayerInitialData.java:90 ItemType.template |
| @XmlJavaTypeAdapter | 7 | 4 | LocalDateTimeAdapter (String↔LocalDateTime.parse; AtreianPassport×2, EventTemplate×2); SpaceSeparatedBytesAdapter ("1 2 3"→byte[]; ItemTemplate×2); NpcEquippedGearAdapter (class-level on NpcEquippedGear: IDREF list of items → lazily built slot→ItemTemplate map + mask) |
| @XmlTransient | 144 | 99 | Mostly index maps in holders |
| @XmlSeeAlso | 17 | 17 | Informational only; polymorphism is always through @XmlElements |
| @XmlValue, @XmlElementRef(s), @XmlAnyElement, @XmlMixed, @XmlAnyAttribute, @XmlSchemaType, package-info @XmlSchema | 0 | | |
| `defaultValue=` | 0 | | Defaults are Java field initializers on 208 bound fields: 162 literals, 41 enum/const, 3 expressions (`DEFAULT_LEVEL_RESTRICTION`, `AbyssRankEnum.X.getId()`×2), 2 `new ArrayList<>()` |
| Annotated methods (property access) | 3 | | ItemTemplate/NpcTemplate `setXmlUid(String)`, ZoneTemplate `getXmlName` + `setXmlName` (interns ZoneName) |
| Implicit FIELD-bound fields (no annotation) | 31 | | Real elements (SkillTemplate properties/startconditions/useconditions/endconditions/effects/actions/motion, EffectTemplate modifiers/change, Parts×8 ...). Some are accidental (AssembledNpcsData Map field, GuideHtmlData `final int CLASS_ALL`, HostileUpEffect.java:32 mutable `tempHate`) |
| afterUnmarshal / beforeUnmarshal | 113 / 1 | | About 1,008 body lines. 77 build maps, 83 null/clear the source list, 12 throw on invalid data, 5 log, 3 read another holder, 2 use `parent`, 1 async |
| beforeMarshal/afterMarshal | 2 pairs | | Spawn, SpawnSpotTemplate (write-back only) |

Property kinds (structural parse, ~2,000): attr:scalar 1188, elem-collection:object 270, elem:object 244, attr:enum 169, attr-collection:scalar 47, elem:scalar 36, @XmlList elem enum/scalar 19, attr @XmlList 9, attr:object 5 (IDREF/adapter), other 13. Scalar field types: int 792, List 354, float 128, boolean 103, String 88, Integer 59 (nullable), byte 20, long 14, Float 12, Boolean 8. 437 distinct object element types. Simple-name collisions: ItemType×2, PetStatsTemplate×2.

## 3. JAXB classes by package (logic = non-accessor method lines, heuristic)
| Package | Classes | Classes with ≥5 logic lines | Logic lines |
|---|---|---|---|
| skillengine/effect | 170 | 125 | 2,710 |
| model/templates/item/actions | 33 | 27 | 1,315 |
| dataholders | 97 | 20 | 605 (+ ~2,845 method lines total) |
| skillengine/condition | 27 | 23 | 356 |
| questEngine xmlQuest conditions/operations/events + models | 44 | 20 | ~384 |
| all others | ~379 | ~30 | ~600 |
| **Total** | **750** | **245** | **~5,977** |

## 4. Data volume (data/static_data, region variants excluded)
665 XML files, 155,962,551 bytes, ~2,108,164 elements, ~6,644,398 attributes (approximate; measured in wave 1: 664 imported files, 2,106,068 elements and 6,661,924 attributes, see static-data.md §4); 96 XSDs (9,530 lines: 1,429 xs:attribute, 744 complexType, 984 xs:element, 149 simpleType, 557 extension, 25 abstract, 157 `default=`, 396 `use="required"`, 2 xs:key, 4 xs:unique).
| Data | Files | MB | Count |
|---|---|---|---|
| items/item_templates.xml | 1 | 57.1 | 102,009 item_template |
| npcs | 1 | 35.8 | 63,287 npc_template |
| goodslists (+6 region copies) | 1 (+6) | 2.8 (18.8) | 19,663 list |
| skills/skill_templates.xml | 1 | 13.0 | 13,570 skill_template |
| spawns | 211 | 9.9 | 229 spawn_map, 22,022 spawn, 131,896 spot |
| npc_walker | 13 | 6.4 | 6,449 walker_template, 112,765 routestep |
| recipe | 1 | 5.2 | 12,494 |
| quest_data | 1 | 4.4 | 8,043 quest |
| zones | 149 | 3.2 | 3,978 zone |
| npc_skills | 25 | 2.7 | |
| global_drops/rules | 86 | 1.9 | 2,196 gd_rule |
| decomposable_items | 1 | 1.6 | 4,125 decomposable |
| quest_script_data | 89 | 0.6 | ~4,040 XMLQuest entries |
| skill_tree | 2 | 0.4 | 4,696 |
| npc_shouts | 1 | 0.4 | 3,789 |
| town_spawns | 2 | 0.4 | 2,450 |
| world_maps.xml | 1 | 0.04 | 161 maps |

## 5. JAXB use outside the startup load
| Site | Kind |
|---|---|
| data/handlers/admincommands/Reload.java:49-104 | Re-deserializes and replaces DataManager.QUEST_DATA, XML_QUESTS(setData), SKILL_DATA, NPC_SKILL_DATA(set), ITEM_DATA(+cleanup), CUSTOM_NPC_DROP, EVENT_DATA(set + restart), UPGRADE_ARCADE_DATA, DECOMPOSABLE_ITEMS_DATA. Each holder root is loaded from its own file(s) |
| SpawnsData.java:205-275 `saveSpawn` (synchronized) | Reads and writes spawns/*/New/*.xml (UnprocessedSpawns), changes SpawnTemplate coords and allSpawnMaps. Called from admin handlers Delete, SpawnNpc, SpawnUpdate×2 |
| WalkerData.java:50-75 addTemplate/saveData | Marshals generated_npc_walker_<id>.xml (handler FixPath.java:126) |
| ZoneData.saveData ← ZoneService.java:191 saveMaterialZones | Only reachable from commented-out GameServer.java:102 |
| DatabaseCleaningService.java:57 | Loads player_experience_table.xml separately |
| configs: InGameShopProperty, RiftSchedule, SiegeSchedules, WorldRaidSchedules | 4 XML config roots in config/ |
| data/handlers: Send (packets xml), Addskill/Combineskill/Deleteskill/Teleport_to_named | Private JAXB classes over consolecommands data xml |

## Key findings
- **The JAXB features in use are a narrow, regular subset. Every bound property is a scalar/enum attribute, a nested object element, a collection of those, a space-separated list, a wrapper, or a polymorphic @XmlElements choice. No xsi:type, @XmlValue, @XmlElementRef, @XmlAnyElement, @XmlMixed or namespaces are used.**
  - Evidence: Counts: @XmlAttribute 1428, @XmlElement 819, @XmlElements 12 (287 mappings), @XmlList 28, @XmlElementWrapper 8, @XmlEnumValue 3, @XmlID/@XmlIDREF 2/2, @XmlJavaTypeAdapter 7; 0 for @XmlValue/@XmlElementRef(s)/@XmlAnyElement/@XmlMixed; `grep xsi:type` over data/static_data/**/*.xml = 0 hits.
  - C++: About 99% of the ~2,000 properties can be generated. Only 12 sites need special handling: 7 adapter uses, 2 IDREFs, 3 annotated methods. One runtime (XmlBinder over pugixml) implementing about 10 binding kinds covers everything.
- **Data and behavior share classes. 245 of the 750 JAXB classes contain real logic (~6,000 lines), mostly skill effects (170 classes, 2,710 logic lines) and item actions (33 classes, 1,315 lines). Java code everywhere calls getters on these classes by their Java names.**
  - Evidence: skillengine/effect/EffectTemplate.java:35 abstract class with 29 bound fields plus apply/calculate logic (574 lines); @XmlElements in Effects.java:31 maps 170 element names to effect classes; questEngine/handlers/models/XMLQuest.java:36 `abstract void register(QuestEngine)`.
  - C++: Generating stand-alone data structs (or XSD-derived types) would create a second type hierarchy parallel to the hand-ported logic classes. The generator should emit code that lives inside the hand-written C++ classes: a member-declaration block plus a `bindXml`/parse definition per class with the Java class and field names, and factories for @XmlElements that construct the hand-written subclasses. Class bodies and logic stay hand-ported.
- **The 113 afterUnmarshal hooks and the DataManager post-load phase contain the non-mechanical semantics: building indexes, cross-holder validation, and changing templates.**
  - Evidence: 77 of 113 build maps and 83 null the source list (e.g. NpcData.java:41, WalkerData.java:33, ZoneData.java:46 builds Area objects and weather zone ids). GlobalDropItem.java:31, ResultedItem.java:36 and ItemRaceEntry.java:50 read staticData.itemData, so they only work because items/ is imported first. NpcData.init (async) overwrites StatsTemplate values. DataManager.java:229-234: ItemData.cleanup changes masks; GlobalDropData.processRules replaces name rules with npc id lists.
  - C++: Give each class an `afterUnmarshal(parent)` hook called by the binder (2 hooks use parent: HouseAddress.java:54, SpawnsData.java:68) and port the bodies by hand (~1,000 lines). Turn the cross-holder checks into an explicit ordered finalize/validate phase so holders can load in parallel. Templates are mutable during load and effectively const afterwards, except the spawn, walker and event paths.
- **The merge step is only a JAXB convenience. What matters semantically is (a) region override per imported file, (b) directory imports whose files' children become one holder instance, and (c) document/file order.**
  - Evidence: XmlMerger.java:89,219,226,233; XmlUtil.java:114 uses Files.find with unspecified order. Order-dependent consumers: SpawnsData.java:90 (custom=true spawns remove earlier spawns of the same npc; spawns/Npcs/Custom/Season_Agrints.xml), ZoneData.java:80 (weather zone ids numbered by encounter order), WalkerData.java:35 duplicate route warning, holders using `put` (last duplicate wins).
  - C++: Skip the merged cache file. Resolve imports, parse each file with pugixml, bind children into one holder per root, then call the holder's afterUnmarshal once. Sort directory listings deterministically. Case-insensitive ordinal sort likely matches Java on NTFS but that is unverified; record it in DEVIATIONS.md. Per-file parsing also keeps the peak DOM at about the largest file (58 MB) instead of 156 MB.
- **Defaults come from Java field initializers, not the XSD, and the two differ. Nullability matters too: Integer/Float/Boolean fields and absent lists stay null in JAXB, and Java code checks for null.**
  - Evidence: 208 initializers on bound fields (162 literal, 41 enum/const, 3 expression, 2 new). XSD has 157 defaults; e.g. ItemRaceEntry `race = Race.PC_ALL` but items/item_groups.xsd:83 has no default. Integer 59, Float 12, Boolean 8 nullable fields; SpawnsData.java:121 `if (mod.getSpawns() == null)`; wrapper present-but-empty gives an empty list, absent gives null (8 wrappers).
  - C++: Generate from the Java annotations, not the XSDs. Map nullable boxed types to std::optional. Keep the Java initializer as the C++ member initializer (the 3 expressions need a manual mapping table). Decide deliberately whether an absent list becomes empty vector or a null flag. Where Java distinguishes null from empty, use std::optional<std::vector<T>> or an explicit `present` flag.
- **Enums are bound by constant name, and several enums are far larger than magic_enum's default range.**
  - Evidence: 114 distinct enum types used in 192 bound properties. Largest Java enums: TribeClass 724 constants, GroupDropType 438, StatEnum 177, EffectType 171, WorldMapType 150. magic_enum is already a dependency (commons/CMakeLists.txt:5), default range [-128,127]. ZoneAttributes uses 3 @XmlEnumValue.
  - C++: Have the generator emit name↔value tables (or X-macros) for the 114 enums instead of relying on magic_enum for XML parsing. Honor @XmlEnumValue. Watch for Windows macro collisions in constant names (commons already has WindowsMacroGuardTest).
- **Static data is modified and reloaded at runtime, and live game objects reference templates directly.**
  - Evidence: Reload.java:49-104 assigns new holder objects to DataManager.QUEST_DATA/SKILL_DATA/ITEM_DATA/CUSTOM_NPC_DROP/UPGRADE_ARCADE_DATA/DECOMPOSABLE_ITEMS_DATA and calls setters on XML_QUESTS/NPC_SKILL_DATA/EVENT_DATA without synchronization. Old ItemTemplates stay referenced by Items, NpcEquippedGear and PlayerInitialData IDREFs, kept alive by the GC. SpawnsData.saveSpawn (synchronized) changes allSpawnMaps and SpawnTemplate coordinates; WalkerData.addTemplate/saveData used by FixPath.java:126.
  - C++: This ties decision B to decision A. Either holders are published as `std::atomic<const Holder*>` or an atomic shared_ptr snapshot, and old holders are never freed (leak-on-reload, the cheapest match for GC semantics), or reload of item/skill templates is dropped. Raw `const ItemTemplate*` in game objects is only safe if templates are never freed. Spawn and walker holders need real locking.
- **XSD validation is part of Java startup semantics, and the XSDs closely mirror the Java classes, probably originally schemagen output.**
  - Evidence: XmlDataLoader.java:50,68 validates asynchronously; GameServer.java:177 aborts on failure. 1,429 xs:attribute vs 1,428 @XmlAttribute; 744 complexType vs 750 classes; XSDs use extension (557), abstract (25), xs:unique (4: spawns.xsd:11, arcadelist.xsd:19), xs:key (2: house_npcs.xsd:17).
  - C++: Options: (1) a strict binder that errors on unknown elements/attributes and missing required=true attributes (319 attrs, 62 elements) as the default runtime check; (2) optional XSD validation with libxml2 (vcpkg) as a startup option or CI test. libxml2 coverage of these XSD constructs is not verified. Enforcing required=true is stricter than JAXB, which ignores it on unmarshal.
- **Java's own verification counts are logged by StaticData.afterUnmarshal, but no Java run or log exists to compare against.**
  - Evidence: StaticData.java:313-405 logs ~92 'Loaded N ...' lines. No log/ or cache/ directory in game-server; game-server/test has no static data tests; PORTING_PLAN.md: 'the user decided against installing a JDK'.
  - C++: Phase 4's 'counts match Java' criterion needs another reference: an independent counting script written per holder's dedupe rules (e.g. WalkerData dedupes route ids, SpawnsData counts maps, TownSpawnsData counts spawns), or a startup log provided by the user.
- **Structurally, a Java-source parser for the generator only needs declarations and annotations. Class names are nearly unique, and inheritance is shallow.**
  - Evidence: Only 2 simple-name collisions (ItemType, PetStatsTemplate); 66 nested classes; max JAXB inheritance depth 3; bound fields have 3 member types (attribute, element, list) plus 3 annotated accessor methods. Some classes bind unannotated base classes (NpcTemplate→CreatureTemplate→VisibleObjectTemplate, PeriodicAction).
  - C++: A Python (or C++) generator reading annotations with a small tokenizer or tree-sitter-java is feasible without a JDK. javalang is Java-8-only and would choke on `_` lambdas and record patterns in method bodies, so skip bodies. Commit the generated code so builds do not need Python.

## Risks
- 1. Data and logic in the same classes (245 classes, ~6,000 logic lines; 170 effect classes). If generated code cannot sit inside hand-ported classes, you get parallel type hierarchies and glue code for every effect, action and condition. Settle the code shape (generated member block + bindXml definition vs generated base struct) before writing the generator.
- 2. Template lifetime under reload (9 reloadable holders) and runtime mutation (saveSpawn, walker FixPath, events restart). Freeing old holders while objects hold raw template pointers causes use-after-free. Must be decided together with decision (A).
- 3. Order-dependent semantics hidden in afterUnmarshal: custom spawn override, weather zone numbering, duplicate-id last-wins, cross-holder validations that assume items were imported first. Java's directory order is OS-defined, so exact parity is unverifiable; sort deterministically and document it.
- 4. Null vs empty vs default: 79 nullable boxed fields, lists left null when absent, 8 wrappers, Java-initializer defaults that differ from the XSD. Silent behavior differences show up far away (drops, stats, spawns), not at load time.
- 5. No Java reference run: 'counts match Java' needs a separately written counting oracle, and JAXB lexical edge cases (whitespace trimming for ints, '1'/'0' booleans, unknown enum constant handling) can't be checked against JAXB. Unknown enum values may give null or a validation event; not verified.
- 6. Very large enums (TribeClass 724, GroupDropType 438) are beyond magic_enum's default range and slow to compile with it. Use generated tables.
- 7. Memory and time: a full 156 MB DOM with pugixml needs an estimated 0.5 GB (node+attribute overhead for 2.1 M elements / 6.6 M attributes). Parse per file and drop each DOM. The object graph (~2 M objects) should get arena/vector-friendly storage. None of this is measured.
- 8. Write-back paths (SpawnsData.saveSpawn with before/afterMarshal on Spawn and SpawnSpotTemplate, WalkerData.saveData) need XML writers matching the XSDs. They are small, but generating writers would double the generator scope; hand-write the 2 needed.
- 9. XSD validation parity: Java refuses to start on XSD errors. libxml2 or Xerces-C would add a dependency, and support for the used constructs (xs:unique/xs:key, abstract types) is not verified.
- 10. Thread safety of holders that change after load (SpawnsData allSpawnMaps is a ConcurrentHashMap of plain HashMaps/ArrayLists; EventData.setEvents; HostileUpEffect.tempHate is a mutable field on a shared template, an existing Java race) must be fixed or explicitly preserved in C++.

## Dependencies
What static data loading needs:
- commons: logging, config (GSConfig.SERVER_COUNTRY_CODE for region override), thread pool (async afterUnmarshal / parallel holder load).
- pugixml, to be added to vcpkg.json.
- Generated enum tables, plus the C++ ports of the ~114 enum types and ~750 template classes. Their class shells must exist before the generated bind code compiles.
- A few logic helpers called from hooks and post-processing: NpcStatCalculation.calculateStat (NpcData.init), Area classes PolyArea/CylinderArea/SphereArea/SemisphereArea (ZoneData), ZoneName (Java String.hashCode-based id), SpawnGroup/SpawnTemplate (SpawnsData), DialogAction (NpcData), ItemSlot (NpcEquippedGear), DecomposeAction static id arrays.
- For phase 4 to finish without all game logic, template classes can be ported data-first: members, getters, bindXml and afterUnmarshal first; logic methods like EffectTemplate.applyEffect and item actions added in phase 5.

What depends on it:
- Nearly the whole game server: 621 `DataManager.X` references in 295 files, including handlers (QUEST_DATA 78 uses, ITEM_DATA 86).
- World/geo init (WORLD_MAPS_DATA), SpawnEngine.spawnAll, QuestEngine (XML_QUESTS registers XMLQuest handlers; QUEST_DATA), AIEngine (AI_DATA, NpcTemplate.ai), skill engine (SKILL_DATA, effects), item/inventory/drop services, siege/base/rift/vortex services, housing.
- It is the first game-server subsystem to port after commons.

Suggested porting order inside this subsystem:
1. XmlBinder runtime and import resolver (per-file, region override, singleRootTag, deterministic order), with unit tests on JAXB semantics.
2. Generator (annotation parser → member blocks, bindXml, @XmlElements factories, enum tables), run on small holders first (world_maps, player_experience_table, tribe_relations).
3. Simple holders (about 60 of 92 are map-building only).
4. items, npcs, skills. This needs effect/condition/action class shells, IDREF resolution for NpcEquippedGear/PlayerInitialData, and the SpaceSeparatedBytes and LocalDateTime adapters.
5. spawns, zones, walkers, quests + quest_script_data, global drops, events (nested SpawnsData).
6. DataManager post-processing and validation, and the count oracle.
7. Reload and write-back paths, once the lifetime model (decision A) is fixed.
8. Config XML roots (in_game_shop, rift/siege/world_raid schedules) and handler-private XML (Send packets, console command data) reuse the same binder.

## Open questions
- How long does the Java server take to load static data, and how much heap does it use? No log or JDK was available. Can the user provide a Java startup log with the 'Loaded N ...' lines and '[Static Data loaded in X seconds]'? It would serve both as a performance baseline and as a count oracle for phase 4.
- Code shape for generated binding: (a) generated member declaration macro/include plus generated `bindXml` definitions inside hand-written classes named like Java, (b) generated `XxxFields` base structs that hand-written classes inherit (awkward with the 349-class Java inheritance chains), or (c) generate once as scaffolding, then maintain by hand with a consistency test. Recommendation: (a) or (c); needs a decision.
- Should //reload for items/skills/quests/npcskills/events/customdrops/arcade/decomposables be kept? If yes, is leak-on-reload (old holders never freed, published via atomic pointer) acceptable? This depends on decision (A).
- Should C++ enforce XSD-level strictness (unknown elements/attributes, required=true) in the binder, run libxml2 XSD validation, or both? Does libxml2 support every construct used (xs:unique in spawns.xsd:11 with 2 fields, xs:key, abstract complex types)? Not verified.
- Which deterministic file order should replace Java's OS-defined Files.find order? It matters for spawns/Npcs/Custom overrides and zone weather ids. Is case-insensitive path ordering on NTFS what the Java server actually saw? Not verified.
- What should happen with an unknown enum constant or malformed number? JAXB RI behavior (null vs ValidationEvent that XmlValidationHandler turns into an exception) was not verified. The C++ port should choose error-on-unknown and document it.
- Are the 31 implicitly FIELD-bound fields intended bindings? E.g. HostileUpEffect.tempHate (a mutable per-template runtime field, a race in Java) and the AssembledNpcsData/CosmeticItemsData Map fields look accidental. The generator needs an explicit allow/deny list for them.
- Should the generator be Python (python 3.12 is present; tree-sitter-java or a custom tokenizer) or C++? And should generated sources be committed (recommended, so CMake builds don't need Python)?
- Is a binary cache ever needed? That depends on measuring the C++ loader. An estimate of parse <2 s plus object construction of a few seconds is unmeasured.
