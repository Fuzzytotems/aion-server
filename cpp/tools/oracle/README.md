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

## M5a scenario oracles (`m5a/`, `docs/design/m5a-plan.md` F-05)

Expected values for the M5a scenario gate, written from the Java sources named in each module docstring; they read the static data through
the V2 import resolution and, for enum constructor data (ItemGroup, ItemSubType, ItemSlot, PlayerClass), the Java source tree.

```
python oracle.py m5a-spawns --map 210010000 --x 1212.94 --y 1044.85 --z 140.76 [--radius 100] [--game-minutes M | --game-hour H [--game-day D] [--game-month M]] [--weekday DAY]
python oracle.py m5a-border-target --map 210010000 --x 1212.94 --y 1044.85 --z 140.76 --game-hour H
python oracle.py m5a-creation --race ELYOS --class WARRIOR [--java-src game-server/src]
```

- `m5a-spawns`: every regular spawn spot within the radius (PositionUtil.isInRange: float squared 3D distance strictly below the radius) with
  npc id, x/y/z/h, template level, `spawned` (true, false, or null when a pool or an unknown part of the game clock decides), `deterministic`
  (spawned and not a walker) and the flags `pool`, `temporary`, `walker`, `randomWalk`, `handler`, `gatherable`, `flag`, `difficultId`;
  `flagNpcs` lists the FLAG npcs of the whole map (visible map-wide). Temporary spawns are evaluated for the given game time
  (TemporarySpawn.isInSpawnTime; `--game-minutes` is the SM_GAME_TIME value). Each row also carries `staticId`, `spotAi` (the spot's `ai`
  attribute, which overrides the template's - Creature.java:64-66) and `respawnTime` (the group's, in seconds), which `m5b-monster` reads.
- `m5a-border-target`: the first target T (150 to 300 m in 10 m steps, 8 directions from east counter-clockwise, same z) inside the map where
  deterministic npcs appear (within 90 m of T, not within 100 m of the start) and disappear (within 90 m of the start, not within 100 m of T).
- `m5a-creation`: spawn point, starting items with count caps, the equipped flag and the equipment slot mask, the level 1 autolearn skills
  with their levels, and the base max HP/MP of PlayerStatCalculator in float arithmetic.

Tests: `tests/test_m5a.py` (rules on small trees, the game clock, temporary spawn times, the stat formulas, and the two scenario characters on
the real data).

## M5b scenario oracles (`m5b/`, `docs/design/m5b-plan.md` G-01)

```
python oracle.py m5b-monster --map 210010000 --npc-id 210663 [--player-level 1] [--race ELYOS] [--class WARRIOR] [--xp-solo-rate 1.0]
                             [--java-src game-server/src] [--java-handlers game-server/data/handlers] [--game-hour H ...]
```

Everything the M5b gate needs to predict a fight against one npc id on one map, in one JSON document (`aion-m5b-monster`):

- `template`: the NpcTemplate values of the monster - level, `maxHp`, rating, rank (and its ordinal), `srange`/`sangle`/`arange`,
  `attack_speed`, race, tribe, `ai` and the `bound_radius` with its `maxOfFrontAndSide`, each with the JAXB field default of
  `NpcTemplate.java` where the attribute is missing;
- `spots`: **every** regular spawn spot of that id on the map, nearest first, with `staticId`, the spot's `ai` override, `respawnTime`,
  `spawned`, `fixed` (not a pool, not a walker, not randomly walking) and the distance from the race's spawn point. `pinned` is `fixed` for
  all of them, i.e. the `OracleSpot::isPinnedToFixedSpots()` condition the M5a gate's V2 needs, and **`nearestPlainSpot` is the spot the gate
  takes**: the nearest one whose `staticId` is 0 (m5b-plan.md D11 - a static id changes `ask(IS_IMMUNE_TO_ABNORMAL_STATES)` and puts
  `GeoService.spawn/despawnPlaceableObject` on the spawn and death paths);
- `exp`: `ratingMultiplier`, `baseExp`, the map's `expMultiplier`, the `XPRewardEnum` percentage, `experienceReward`, `expNeed`, the
  `Rates.XP_HUNTING` `cap` (`expNeed * 0.2f`) and `awarded` - the number `STR_GET_EXP` carries on the wire;
- `ranges`: `attackRange` (`1 + attackRangeStat / 1000f` plus both bound radii, because `PositionUtil.isInAttackRange` passes
  `centerToCenter = false`), `maxCoveredDistance` (100 ms of the player's movement speed) and `toleranceRange`, the band
  `PlayerController.attackTarget` adds while the target does not hate the player yet;
- `player`: the race, class and spawn point the distances and the weapon stats come from, the main hand weapon's attack range and attack
  speed, the movement speed and the player bound radius.

The values are computed for a **fresh** character, like `m5a-creation`: the starting gear and the unapplied passive skills add no
`ATTACK_RANGE`, `ATTACK_SPEED`, `SPEED` or `BOOST_HUNTING_XP_RATE` modifier. The formula literals and enum tables (the `NpcRating`
multipliers, the rank step, `NpcRank`, `XPRewardEnum`, `GeneralInstanceHandler.getExpMultiplier`, the player bound radius of
`PlayerAccountData`, the run speed of `PlayerClass`, the attack range and attack speed bases of `PlayerGameStats`) are read from the Java
sources, so a change there is a test failure and not a silently wrong expectation. Exit code 2 (`OracleError`) for what the oracle does not
model: an instance map (its reward is multiplied by the instance's `maxPlayers`), a map with a registered `@InstanceID` handler (it may
override `getExpMultiplier`), a template without a rating or rank (Java throws), a map whose `world_type` names no race (pass `--race`).

Tests: `tests/test_m5b.py` (the float formulas alone, the Java literals as they stand today, the whole report on a small static_data tree,
and npc 210663 on Poeta against a level 1 Elyos Warrior).

## M5b-2 scenario oracles (`m5b2/`, `docs/design/m5b2-plan.md` G-01)

```
python oracle.py m5b2-skills --race ELYOS --class MAGE [--level 1] [--skill ID[:LEVEL] ...] [--npc ID ...] [--death-count 1]
                             [--java-src game-server/src]
```

The skills a character casts in the M5b-2 gate and every template constant the gate asserts exactly (plan D8), in one JSON document
(`aion-m5b2-skills`):

- `character.skills`: the autolearn set of `SkillLearnService.learnNewSkills(player, 1, level)` - shared with `m5a-creation` through
  `m5a/creation.py learn_new_skills`, so the class-less `skill_tree.xml` rows (243 *Return*, 245 *Bandage Heal*, 302 *Escape*) are there by
  construction. Only a starting class (`CM_CREATE_CHARACTER.java:92`) and levels 1..9 (a character that is not a daeva is capped at 9); plus
  `passives`, the `equippedItems` of the starting gear and the stat sources the oracle refuses to model (`castingTimeSources`,
  `skillCostSources`);
- `skills`: one entry per (skill id, level) - the autolearn set, every `--skill` (a skill the gate seeds into `player_skills`, default level
  the template's `lvl`), the soul sickness at `--death-count`, and every `--npc`'s npc skills - with `castDuration` (the `writeH` of
  `SM_CASTSPELL`, `Skill.updateCastDurationAndSpeed` for a player), `castSpeed` and `allowAnimationBoost` (its float and its last byte),
  `cooldown` (the `writeD` of `SM_CASTSPELL_RESULT`, 100 ms units, with `cooldown_delta_lv * level`) and `cooldownMillis` (the duration
  `SM_SKILL_COOLDOWN` writes), `mpCost` (the `<mp>` END condition: `MpCondition.getCost`, i.e. the `USED_MP` of `SM_ATTACK_STATUS`) and every
  cost per condition section, `targetSlot` (name, `ordinal` - what `SM_ABNORMAL_STATE`/`SM_ABNORMAL_EFFECT` write per effect - and `id`, the
  slot mask), the `effects` with their tag, **class** (`Effects.java`), `classChain` up to `EffectTemplate`, position, `duration1`,
  `duration2`, `effectiveDuration2` (what the virtual `getDuration2()` answers: `AbstractOverTimeEffect.java:64-67` adds 1,000 ms to every
  damage and heal over time), `randomTime`, `preEffects`, `changes` and raw attributes, and `effectDuration` (`Effect.calculateTemplateDuration`
  when every template succeeds, over `getDuration2()` and not the attribute - 1447 *Erosion*'s `duration2="15000"` lasts 16,000 ms; the
  random part is `effectDurationRandomTime`, not rolled; the sum is a Java `long`, which `Effect.calculateEffectsDuration` clamps to
  `Integer.MAX_VALUE` - the xpboost templates' `duration1="2000000000"` last 2,147,483,647 ms at every level);
- `soulSickness`: the skill id `PlayerController.updateSoulSickness` casts (8291) and the death count it casts it at;
- `npcs`: each `--npc`'s `npc_skills` list (the first list naming the id wins, `NpcSkillData.afterUnmarshal`) with the npc's own
  `castDuration` (`Math.round(duration * cast_speed / 1000f)`);
- `effectClasses`: the leaf effect classes of all reported skills and their closure under `extends` - the per-character form of
  m5b2-plan.md §2.4's inventory.

A value the oracle does not model is `null` with a reason in the entry's `notModelled`, never a guess: a casting time stat function (an
equipped `BOOST_CASTING_TIME*` modifier, an item set, a passive that changes such a stat or is a `BoostSkillCastingTimeEffect`), a
`BoostSkillCostEffect` passive, an `<mp ratio="true">` cost, a CHARGE skill, an `effectDuration` above `Integer.MAX_VALUE` whose
`randomtime` roll decides whether the clamp applies. The literals (`SkillTargetSlot`, the 8291 and its death count cap, the 25 % cast
duration cap, the 170 `Effects.java` bindings, the `extends` chains and the `getDuration2()` overrides with their constant) are read from the
Java sources. Exit code 2 (`OracleError`) for a class that cannot be created, a level outside 1..9, a death count outside 1..10, a skill id
without a template, and for Java sources whose duration getters the oracle does not model (a `getDuration2()` whose body is anything but
`return duration2 [+ N];` - found by its head, so a body with braces of its own is refused too - or any override of `getDuration1()` /
`getRandomTime()`).

Tests: `tests/test_m5b2.py` (the formulas alone, the Java tables as they stand today and on an edited copy, the whole report on a small
static_data tree, and the gate's four ids 2864, 1282, 1328 and 8291 plus 3195, 1838, npc 210133 and the two damage-over-time skills 1447
*Erosion* and npc 210306's 17018 *Bite* on the real data).
