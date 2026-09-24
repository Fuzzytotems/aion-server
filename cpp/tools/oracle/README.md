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

## M5b-3 drop oracle (`m5b3/drops.py`, `docs/design/m5b3-plan.md` G-01)

```
python oracle.py m5b3-drops --npc 210663 [--map 210010000] [--player-level 1] [--race ELYOS] [--drop-rate R] [--java-src DIR] [--java-handlers DIR]
python oracle.py m5b3-drops --survey --map 210010000 [--player-level 1] [--race ELYOS] [--drop-rate R]
```

What a solo player's kill of one npc id registers (`DropRegistrationService.registerDrop`), in one JSON document (`aion-m5b3-drops`); the
Java methods are named with file:line in the module docstring:

- `ai`, `registerDrop`, `allowDecay`: the effective AI of the npc's spots (the spot's `ai`, else the template's after `NpcTemplate.afterUnmarshal`
  turned a TELEPORTER above level 1 into `siege_teleporter`; `SpawnTemplate.NO_AI` is `DummyAI`) and what its `ask()` chain answers to
  `REWARD_AP_XP_DP_LOOT`, `REWARD_LOOT` and `ALLOW_DECAY` - `registerDrop` (a kill registers the drop) needs the first two, `allowDecay` is
  the third; `registerDropByAi` / `dropTrigger` name the AI classes that call `registerDrop` themselves (`ChestAI`, `QuestItemNpcAI`,
  `NightmareCrateAI`: on use, conditions not modelled); with neither there is no `SM_LOOT_STATUS(LOOT_ENABLE)`;
- `modifiers`: `createDropModifiers` - the chest test, the killer's race, `boostDropRate` (`rate * 100 / 100f`) and `reductionDropRate`
  (`DropRewardEnum` of `npcLevel - playerLevel`, null at 100 %);
- `globalDrops`: the `quest_use_item` AI, the global npc exclusion list that names the npc, a map `drop_type` of NONE, and
  `isAllowedDefaultGlobalDropNpc` (level < 2 outside Poeta and Ishalgen, chests, abyss types) - without it only rules with `gd_npcs` apply;
- `customDrop`: the first `<npc_drop>` of the id, per group its race filter, final chances, entry-count distribution and per-drop pick
  probability (`DropGroup.tryAddDropItems`: the nearest final chance above each roll);
- `questDrops`: `quest_data.xml` and the handler side drops of `data/handlers/quest` for the npc, with the conditions under which one registers
  (quest in START, collecting step, collect item count, ALLIANCE target or MENTE mentor type never solo) - `entries` assumes none does;
- `rules`: every **applicable** global rule in `GlobalDropData` order (`ruleIndex`, `file`, `ordinalInFile`): its restrictions pass **and**
  its candidate set is not empty (`rulesWithoutCandidates` lists the others, `ruleStatistics.blockedBy` the first failing predicate,
  `rulesZoneUndecided` the zone rules that add no entry whatever the zone answers: never fire, or no candidate), with
  `effectiveChance` (the float `calculateEffectiveChance` compares), `fireProbability`, `certain` / `never`, `entriesIfFired`
  (`min(max_drop_rule, candidates)`), `indexes` (exact while every earlier rule and group is deterministic), and the `candidates` (item race
  PC_ALL or the killer's, `min_diff <= npcLevel - itemLevel <= max_diff`, in `gd_items` order) with `weight`, `pickProbability`, `countRange`
  (uniform; `countValues` for kinah: `count *= level * Math.pow(rank * rating, 6)`, truncated), `optionalSocket` and `lootEffectId`;
- `entries`: min, max, `deterministic`, the exact distribution of the entry count, `pNoDrop`, `expected` and `minCertainEffectiveChance`;
  `kinah`, `lootEnable` (the loot effect ids an item of the drop can bring and `pLootEffect`, the exact probability that the id is not 0),
  `gateAssertions` (the exact statements for a gate) and `notModelled`.

Probabilities are exact over the 2^24 values of `Rnd.chance()` = `RandomGenerator.nextFloat(100f)` (JDK: `(nextInt() >>> 8) * 2^-24f * bound`,
corrected to `nextDown(bound)`); `certain` (effective chance >= 100f) and `never` (<= 0) hold for any `nextFloat`. Float values follow Java:
`Float.parseFloat` rounds a decimal once (not through a double), every product is rounded to float. Configuration: `--drop-rate` is the
`gameserver.rates.drop` value of the killer's membership (default `RatesConfig.DROP_RATES` = `1.0, 2.0` read from the source, membership 0,
so 1.0; `config/main/rates.properties` is reported beside it). The oracle assumes `gameserver.event.service.disabled_events = *` (every gate
profile; the shipped value is empty, which keeps the permanent "Beyond Aion Server Buffs" event and every dated event in its period active -
extra drop rules and a random drop-boost buff the oracle does not model), a solo killer with no drop-rate stat, repose, salvation or palace,
and the most damage. Repose is 0 below level 10 (`updateMaxRepose`) and an explicit assumption from level 10 on (`killer.reposeEnergyAssumed`;
it grows offline, +5 boost); salvation points come only from the //energybuff and set_vitalpoint commands. `--player-level` is the level
when `registerDrop` runs, after the kill's XP (`doReward` adds it first). `ruleIndex`, and so which rule an entry index belongs to, assumes
Java lists the `global_drops/rules` files in NTFS order (`gateAssertions` says so when the indexed rules span several files). `--survey`
gives one row per npc id with a spot on the map (refused npcs listed with the reason) and the counts of m5b3-plan.md §2.4.

The Java literals are read from the sources (rank and rating modifiers, the kinah exponent and item id, the chest and quest AI names, the level
exception and its two maps, `DropRewardEnum`, `getLootEffectId`, `SpawnTemplate.NO_AI`, the `DROP_RATES` default, the `GlobalRule` defaults and
the enums, the `NpcTemplate.afterUnmarshal` ai rewrite and the repose level), and 55 statements of the modelled methods in 9 files must be
present verbatim (whitespace-normalized, comments removed), so a changed Java method is a refusal, not a silently stale model. Exit code 2
(`OracleError`) for: a rule whose `gd_zones` decide (`isInsideZone` needs the position and the zone shapes; not when the rule adds no entry
either way), a float product beyond the float range (Java: Infinity), an AI whose `ask()` is not `return switch (question) { case ... -> true|false|super.ask(question); ... }` or
`return true|false|super.ask(question);` for the three questions (448 of the 457 AI names are modelled), an AI overriding
`handleDropRegistered`, spots whose AIs answer differently, a map with an `@InstanceID` handler, an npc without `group_drop` or - where a
dynamic rule or the kinah count needs them - without rank or rating, unknown enum values, unknown `gd_rule` attributes or elements, a candidate
weight <= 0 in a draw, a handler side quest drop the oracle cannot read, and data on which Java fails at startup (a `gd_item` without an item
template, `min_count <= 0`, `max_count < min_count`, a custom `<drop>` `Drop.afterUnmarshal` rejects, also in a repeated `<npc_drop>`, a
`gd_npc_names` rule while a template has no name). A value it cannot compute exactly
(`pickProbability` of a draw beyond 4,000 states, a kinah count that Math.pow's last bit decides) is null with a `notModelled` reason.

Today: 210663 (Poeta, level 2 NORMAL DISCIPLINED BEAST) has 10 applicable rules, no Kinah (BEAST is not in the Kinah rule's races),
P(no drop) 0.4177 at rate 1.0; 210133 has 10 with Kinah 50 % and 5..25 kinah, P(no drop) 0.2147; at rate 10000 both are exactly 10 entries
with indexes 1..10 (the smallest certain chance, "Illusion Godstones (Unique)", is exactly 100.0f there - `gateAssertions` warns; 1000000 has
margin; there `pLootEffect` is 0.4152: each Illusion Godstone rule draws one of 17, four of which carry loot effect 1003). Poeta: 147 npc
ids, 77 with an applicable rule, 42 with Kinah, 0 custom drops, 20 quest drop items, 999 droppable items; Ishalgen 166, 91, 50, 0, 25, 1,077.

Tests: `tests/test_m5b3_drops.py` (the float and lattice arithmetic, `Chance.selectElement` and the custom group draw alone, the AI `ask()`
parser, the Java tables today and on an edited copy, the whole report on a small static_data tree with fixture AI and quest handler classes -
each AI question on its own, an AI registering the drop itself, the siege_teleporter rewrite, the default-exclusion boundaries, zone rules
that add no entry, `max_items` -1, the loot effect probability - and 210663, 210133 and the Poeta survey on the real data).

### The cube-slot budget of a corpse (`m5b3/items.py`, `m5b3-drops --inventory`, m5b3-plan.md G-01, risk 5)

```
python oracle.py m5b3-drops --npc 210133 --drop-rate 1000000 --inventory 182400001:1000 --inventory 169000003:2 ... [--cube-expansions N]
```

With `--npc`, the report carries `cube`: what looting every entry of that corpse does to the looter's cube when it holds the `--inventory`
stacks (ITEM[:COUNT], one per stack; kinah is the storage's own item and takes no slot). DropService.requestDropItem's solo arm calls
ItemService.addItem(player, itemId, count): kinah goes to the kinah item, a stackable item fills the room of every existing stack of its id
(Item.increaseItemCount) and then makes new stacks of at most `max_stack_count` (ItemFactory.calculateCount), a non-stackable item takes one
slot per unit, an item with `<inventory id>` above 0 goes to the special cube, and a LIMIT_ONE item already in the cube is refused and stays
in the corpse (`refusedLimitOne`). Per applicable rule: each candidate's `merge` (`certain` - it fits the room of existing stacks at its
maximum count, `partial`, `never`, `kinah`, `specialCube`, `refusedLimitOne`) and `newSlotsWorst`/`newSlotsBest`; per corpse
`worstCaseNewSlots` (an upper bound: two entries that pick the same new stackable item share a stack), `bestCaseNewSlots` (certain rules at
their minimum), `kinahEntries`, `deterministicMerges` (the entries that take no slot whatever is picked - the Minor Power Shard of "Power
Shards" once a shard stack exists), `limit` (StorageType.CUBE 27 + 9 per expansion) and `slotsFree`. A custom drop group that applies is an
entry too (`customGroup`): its drops that can be picked (`finalChance` above 0), each at `minAmount`..`maxAmount` (DropItem.calculateCount),
the `maxEntries` largest for the worst case and the `minEntries` smallest for the best. Today, for a fresh Elyos Warrior's cube
(9 stacks) at the gate's rate: the first 210663 takes up to 10 slots; 210133 after it up to 8 (kinah takes none, the shard merges); a second
210663 up to 8 (the shard and the Sparkie Carapace Fragment merge) - m5b3-plan.md risk 5's 10 + 8 + 8.

## M5b-3 item oracle (`m5b3/items.py`, `docs/design/m5b3-plan.md` G-01)

```
python oracle.py m5b3-item --item 162000002 [--item 168000116 ...]
```

One document (`aion-m5b3-item`) per call, one entry per item: the template's attributes, `maxStackCount`/`stackable` (the JAXB default 1,
`isStackable` is `> 1`), `mask` with its `maskFlags` (ItemMask.java's constants, read from the source), `extraInventoryId`,
`expireTimeMinutes`, `useLimits` and `cooldown` (`usedelay` ms under the `usedelayid` group - what Player.hasCooldown checks), the `actions`
(tag -> action class of ItemActions.java's @XmlElements) and, for `skilluse` and a `<godstone>`, the skill: `templateCount` (how many
`<skill_template>`s carry the id; SkillData.afterUnmarshal keeps the LAST, SkillData.java:33-39), its attributes and properties, and per effect
the tag, class (Effects.java), `classChain`, attributes and `valueAtLevel` (EffectTemplate.calculateBaseValue: `value + delta * level`).
Today: 162000002 heals 37 at once (`ProcHealInstantEffect`) and 37 per tick for 20 s (`HealEffect`), 30,000 ms under use-delay group 11;
168000116's godstone (probability 1000, breakprob 0) casts 8267, a MAGICAL `ProcAtkInstantEffect` EARTH with delta 100 - **one** template
of 8267 in skill_templates.xml (:80570), not the two m5b3-plan.md §2.6 (b) and risk 11 describe (the WIND template above it is another id).

```
python oracle.py m5b3-item --survey --map 210010000 --map 220010000
```

The survey (`m5b3/survey.py`, `aion-m5b3-item-survey`) re-derives the counts of m5b3-plan.md §2.5-§2.6: `skilluse` - the starter items of
every class (player_initial_data.xml) and the items droppable on the maps (m5b3-drops --survey's `distinctDroppableItems` for the map's
default killer) with a `skilluse` action, their leaf effect classes with `items`, `effects` and `skills` each, and `classChain` (closed under
`extends`); `godstones` - every `<godstone>` item, its proc skill's classes, and the godstones droppable per map; `materials` - every skill of
material_templates.xml and its classes. The last `<skill_template>` of an id is kept. Today: 48 skilluse items (8 starter, 31 droppable on
each map) with 9 leaf classes - StatupEffect is 12 items and 20 effects, §2.5's table counts the effects; 268 godstones with 110 proc skills
and 10 classes (ProcAtkInstantEffect 70 skills, Poison 6, Silence 5, Blind 5, Paralyze 4), the 34 Poeta drops reaching the same ten; 28
material skills with 13 classes.

## M5b-3 material oracle (`m5b3/materials.py`, `docs/design/m5b3-plan.md` G-01, §2.6, §10.5)

```
python oracle.py m5b3-material --map 210010000 [--near 1212.9423,1044.8516,140.75568] [--radius R] [--limit N] [--geo-dir DIR]
```

The skill materials of a map (`aion-m5b3-material`): the material zones GeoWorldLoader.createZone makes for every placed geometry with the
MATERIAL collision intention (named by `geo/loader.py` itself - PlacedGeometry.zone_geometry_name, the string its checked `material_zones`
carry - with its float arithmetic, `|` aliases and town levels), filtered by
ZoneService.createMaterialZoneTemplate (no MaterialTemplate -> no zone; a duplicate zone name keeps the first), each with its `materialId`,
mesh, world bound `center`/`extents`, MaterialZoneTemplate's `area` (CYLINDER for CYLINDER/CONE/H_COLUME names, SEMISPHERE, else SPHERE, radius
the bound's corner distance + 1) and the material's `skills` (id, level, target, frequency, conditions), nearest to `--near` first;
`skillZones`, `skillPlacements`, `skillZonesByMaterial`, `nearestUnconditional` (a zone whose skill has no weather/time condition) and
`terrain`: the histogram of the map's 8-bit materials PNG and the ids with a MaterialTemplate (TerrainZoneCollisionMaterialActor). Today:
Poeta 98 skill zones (material 60 x22, 61 x7, 62 x69; the review's 97 was the miscount) and Ishalgen 48 (11, 1, 36), all skill 8302; the nearest
unconditional one on Poeta is `pr_l_fire_semisphere_01a.cgf` 407 m from the Elyos spawn, the nearest of any a material-62 fire at 108 m; neither
map has a terrain materials file, so no terrain material casts a skill there. Which positions pass the TOUCH check on a zone's mesh is not
modelled (G-04 measures it).

Tests: `tests/test_m5b3_items.py` (the slot arithmetic, the Java tables today and on an edited copy, the item report and the budget on a small
static_data tree - the last skill template wins, valueAtLevel, every `merge` class, top-k picks, a custom drop group's amounts, the refusals -,
the survey on a small tree - the starter and droppable scope, effects against items, a replaced godstone template, material skills -,
MaterialZoneTemplate's three shapes, the zones of a synthetic geo scene - the template filter, a duplicate name, the `_CHILD<n>` suffix, the
distances -, the gate's items, corpses and camp fires and the §2.5-§2.6 survey on the real data, and the CLI).

## M5c trade oracle (`m5c/trade.py`, `m5c/trade_config.py`, `docs/design/m5c-plan.md` G-01)

```
python oracle.py m5c-trade --map 210010000                         # the merchants of a map (220010000: Ishalgen), with their spots
python oracle.py m5c-trade --npc 798007 [--item 162000002] [--count N] [--race ELYOS] [--legion-level N]
python oracle.py m5c-trade --item 162000052 [--count N]              # one item: prices, a vendor's reward, every npc selling or buying it
         [--config game-server/config] [--profile F | --no-profile] [--set KEY=VALUE ...] [--influence RACE=N ...]
         [--account-max-level N] [--membership N] [--java-src game-server/src] [--java-handlers game-server/data/handlers]
```

What a merchant sells and at what price, and what it pays for a sold item, in one JSON document (`aion-m5c-trade`):

- `config`: every key the rules read, with its typed value, raw text and source. It is loaded the way `Config.loadProperties` loads it: the
  `*.properties` of config/administration, main and network, then `--profile` (default config/mygs.properties, which may be missing; a file
  named with `--profile` must exist), then `--set` (read as one line of the profile, so trailing white space stays), else the `@Property
  defaultValue` read from the Config class. **The shipped defaults turn sieges and sell limits ON** (`siege.properties`, `custom.properties`),
  so a gate passes `--set gameserver.siege.enable=false --set gameserver.limits.enable=false` (its profile) or `--profile` with them.
  `gameserver.country.code` (default 99) selects the static data: `XmlMerger.applyCountryOverride` reads `goodslists_<region>.xml` for 1, 2, 4,
  5, 6 and 7, and the oracle reads the same (and refuses data read with another code);
- `prices` per race: the influence (0 with sieges off; `--influence RACE=N` with sieges on, otherwise exit 2), `globalPrices`, `taxes` and
  the `SM_PRICES` bytes - 125, 100, 113 with the defaults;
- `--npc`: the npc's functions (`Npc.canSell/canBuy/canTradeIn/canPurchase`), its `ai`, `talkRange` (`getTalkDistance() + 1`, where the
  client must stand) and `subDialogType`; `buy.dialogPath` and `buy.dialog` - a dialog passes `CM_DIALOG_SELECT`'s gate only when the npc
  supports it or no npc of the data lists it as a function (BUY, SELL and TRADE_SELL_LIST all are listed), otherwise **no packet at all**
  (`"none (CM_DIALOG_SELECT audit: ...)"`, e.g. oz 203081, tula 203082, 798008, 798037); then the npc's AI answers first (an AI class that
  overrides `onDialogSelect` is not modelled: `dialog` None; none of the 2290 trade npcs has one); then `DialogService` - and `buy.smTradeList`
  (the npc type byte is `TradeNpcType.index()`, **1** for NORMAL; the modifier `VENDOR_BUY_MODIFIER * sell_price_rate / 100`; the tabs the
  legion level shows; the limited items of a fresh server; the buy tab is always on past the gate), `buy.goods` - each item once in tab order
  with `unitPrice` (`PricesService.getBuyPrice`), `kinah` (the `TradeList.calculateBuyListPrice` term for `--count`, with `sell_price_rate2`
  for ABYSS_KINAH and 0 for ABYSS/REWARD), `requiredAp`, `requiredItems`, `limited`, `buyable`/`failure` (the first failure no player state
  avoids, with its system message; `CM_BUY_ITEM` does not pass the dialog gate) and `sellBack` (the sale of `--count` of it to the same npc) -
  and `sell` (`dialogs` for SELL and TRADE_SELL_LIST, `reachable`, `smSellItem` only when one of them opens it, e.g. none for 800591 whose
  func_dialogs are [2]; the arm: VENDOR at `VENDOR_SELL_MODIFIER`, PURCHASE at `buy_price_rate`, PURCHASE_AP for an ABYSS purchase template);
  with `--item`, `item.buy` and `item.sell` for that item;
- `--item` alone: the item's prices, `vendorSale` (a vendor without a purchase template), `soldBy` and `purchasedBy`;
- `--map`: every npc with a regular spawn and a trade, trade-in or purchase template or a BUY/SELL/TRADE_IN/TRADE_SELL_LIST function, its
  spots (with each spot's effective AI: a spot's `ai` replaces the template's), `sellers` and `buyers`.

A sale reports `unitReward`, `soldCount`, `kinah` and `repurchasePrice`; with sell limits on, `sellLimit` holds the arithmetic of
`PlayerLimitService.updateSellLimit` for the FIRST sale of an account whose best character has `--account-max-level` and whose membership is
`--membership` (the limit is in-memory state of the day). The oracle fingerprints every Java member whose code it models, WHOLE
(`MODELLED_MEMBERS`: 86 methods, constructors, classes, enum constant bodies and switch arms of the price, buy, sell, limit, dialog, AI and
window code and of the data holders, comments and white space removed), so an edit anywhere inside one - a statement inserted before, after or
between the key statements (`MODELLED_STATEMENTS`, which name what changed) - is exit 2, not a silently wrong price; it reads the literals
(`DialogAction`, `TradeNpcType`, `ItemMask`, `SellLimit`, the `TradeListTemplate` field defaults, the `CM_BUY_ITEM` count cap, the `@Property`
defaults) and follows them. Exit 2 (`OracleError`) also for: sieges without `--influence`, a key a timed event sets, a key two default files
set differently, a config value Java rejects or the reader does not implement (hexadecimal ints, float suffixes), a `--profile` that does not
exist, static data read with another country code, a goods item without a template, an `<acquisition>` without a type in a purchase, a missing
purchase list met before the item (after the apitems switch for an ABYSS purchase template), data the game server cannot start with (a limited
item whose goods list has no `<salestime>` or one of a cron shape not modelled, a cleanup that sets or clears a bit of an item without a
template, no `<trade_in_list_template>` or other list kind, an npc_template `ai` without an AI class), an unknown or nested AI, a missing
config folder, an arithmetic overflow, a count outside 1..20000. Player state is not modelled: `PlayerRestrictions.canTrade`, the kinah, AP,
items and free slots a buy needs (reported as requirements) and what a dialog needs from the player (not trading, in `talkRange`,
`DialogService.isInteractionAllowed`); trade-in prices are listed as not modelled.

Tests: `tests/test_m5c_trade.py` (the arithmetic alone, the properties reader, the Java text reader, the Java tables and members today and on
an edited copy - the review's five insertions refused -, the config loading, the whole report on a small static_data tree with every vendor
and purchase type, the dialog gate, an AI that answers dialogs, the limit boundaries, duplicates, the country variant and the data Java cannot
start with, and the Poeta and Ishalgen merchants on the real data: sellers 203060, 203061, 203063, 203080, 798007 and 203514, 203515, 203526,
203542, 798038; 798007's elixirs at 352, two for 704, ten Minor Life Potions sold for 500, the juice refused; no window at oz, tula, 798008,
798037).

## M5c craft oracle (`m5c/craft.py`, `m5c/craft_java.py`, `m5c/craft_config.py`, `docs/design/m5c-plan.md` §2.6, G-01)

```
python oracle.py m5c-craft --recipe ID [--skill-level N] [--craft-type 0|1] [--skill-xp X]
python oracle.py m5c-craft --skill ID --level N [--map ID] [--character-level N] [--skill-xp X]
python oracle.py m5c-craft --gatherable ID [--skill-level N] [--character-level N] [--skill-xp X]
                 common: [--java-src DIR] [--config DIR] [--profile F | --no-profile] [--set KEY=VALUE ...] [--membership M]
```

One JSON format (`aion-m5c-craft`) for three questions. Every report carries `config` (each key with its Java type, `@Property` default, raw
value, source and typed value), `assumptions`, and `java`: every Java method the numbers come from with its `file:first-last` lines.

- `--recipe`: the recipe (skill, race, `skillpoint` = the minimum level, `dp`, `autolearn`, `max_production_count`, the craft cooldown), every
  `<components_data>` alternative, the product and count, the combo products; `learn` (the autolearn races and level, the `<craftlearn>` recipe
  items with their template price); `craft` at `--skill-level` (default the skillpoint): checkCraft's refusals in order, each followed by
  the cancel pair (update action 4, animation 2; the DP and wrong-target refusals have no system message but still send it), the station
  range (5 + the player's 0.25 + the StaticObject's 0, center to edge; CM_CRAFT's 10 center to center; strict `<`; the station type is never
  checked), the material rule (the first alternative whose FIRST item is a key of the CM_CRAFT map, so a later alternative with the same
  first item is `selectable: false`; none named: nothing checked or consumed; per item `required` = its largest component quantity and
  `consumed` = the sum, of which the craft takes min(held, consumed) - 4 real morph recipes name an item twice), DP, the
  timing (first tick 1000 ms, interval `max(cap, 2500 - 60 * diff)`, cap 1200/1500/1700 by the product's quality, 200 for the morph skill),
  per bar (product, then each combo) the failure and crit-blue thresholds, the step ranges at `multi` 1f and nextDown(2f), execution speed,
  bar delay and the tick bounds, the outcomes (item, count, probability when no bar can fail, finish time bounds) and the packets (setDp's
  SM_DP_INFO / SM_STATS_INFO / SM_STATUPDATE_DP first for a non-starting class, the task's, the cancel and abort pairs with delay 1000 for
  the morph skill; `scope` says what is left out: inventory, recipe-list, quest and exp-bar packets);
  `skillUp` (xp `(int) (0.008 * (sp + 100)^2 + 60)` [+15 % for craft type 1], the skill xp after the rate and boost, `(int) (0.23 *
  (level + 17.2)^2)`, the level after, the recipes autolearnt on a level-up, the player exp, and its `packets`: MAXPOINT_UP /
  DONT_GET_PRODUCTION_EXP, or CRAFT_LEVEL_UP at 100/200/300/400/450/500, SM_SKILL_LIST with 1330064 (1330005 for tapping), the gathering
  and exp messages); `afterCraft` (recipe deleted, cooldown ms as Java's int product).
- `--skill`: xp to the next level and the cap at that level (30001 at 49; 99/199/299/399/449/499/549 for crafting and tapping, 449 free for
  tapping), the master's price at that level (`Profession.getUpgradeCost`; level 0 = learning, 3,500) with the master npc ids, the autolearn
  recipes per race (crafting and morph) and the xp per recipe skillpoint; for a gathering skill the gatherables (with `--map`, those spawned
  there, their spot count and respawn time).
- `--gatherable`: the material roll (`Rnd.nextInt(10000000)` over the materials in descending rate; with rates summing to 10,000,000 the
  first one wins `rate + 1` values and the last `rate - 1`), the exmaterials rule, lvlLimit and skill checks, the gather task (first tick 200..600 ms, interval `max(1200, 2500 - 60 *
  diff)`, purple/blue/normal chances, steps, speeds), the node despawning after `harvestCount` interactions of ANY outcome, and the skill-up
  (`(int) (0.0031 * (sl + 5.3) * (sl + 1592.8) + 60)`).

The Java model (`craft_java.py`) compares each modelled method WHOLE with the body it was written against (comments and layout ignored); a
`$name` placeholder is a literal read from the source and typed (`$d:` double, `$f:` float, else int), so a changed literal type or any other
change is a refusal with the file, line and first difference. Tables are parsed and rebuilt. The classes the tasks are made of (CraftService,
CM_CRAFT, CraftingTask, GatheringTask, AbstractCraftTask, AbstractInteractionTask, GatherableController, the recipe and gatherable templates)
are pinned whole (`CLASS_PINS`): their header and ordered member list, each member a checked template or the SHA-256 of its text, so an
added override or initializer, or a changed method the oracle does not model, is refused too (`JavaFile.describe_pins` prints the pins of a
re-read class). The config (`craft_config.py`) reuses `trade_config.py`'s Properties reader and transformers; the profile defaults to
`<config>/mygs.properties`.

Deterministic, for a gate: product and count (always the base product with `gameserver.rates.crafting.crit_chances = 0`), materials, the
interval, every fixed packet field, the NORMAL/CRIT_BLUE update's execution speed and bar delay, skill xp, level-up and player exp. Random
(reported as bounds or probabilities): the per-tick step and crit-blue roll (so the number of updates and the finish time), failure (unless
`gameserver.craft.fail.chance = 0`, or a level difference of 41+), procs, the gather start delay and purple crits. Refused (exit 2): unknown
ids, a product/combo/component without an item template, a product or combo without a quality, an empty `<components_data>`, a
`<comboproduct>` without itemid, unknown attributes or children on the reported template, craft type 1 without a bonus item, a material roll
that can select nothing, a gatherable material without an item template, a component of quantity <= 0, an autolearn recipe with
`max_production_count`, craft type 1 when the bonus item is a component, a level-up of a skill whose activation is not NONE, an int
attribute that is not ASCII decimal, CAPTCHA on, an instance `--map`, events not disabled (`gameserver.event.service.disabled_events` must be
`*`: events override rate keys and give xp buffs), and every Java body, class or config shape not modelled.

What a new character can do in Poeta and Ishalgen: only gather with 30001 *Collection* at level 1 (craft_skill_tree.xml: 30003 and 40009
come at character level 10, crafting at a master at level 10; the masters stand in the capitals and in Oriel and Pernon, none in Poeta or
Ishalgen) - Young Aria 400601 (Poeta, 71 spots) and Young Azpha 400651
(Ishalgen): 1 Aria/Azpha per gather, 91 skill xp and 91 exp, which makes Collection 1 -> 2; Mela/Raydam Sapling need 10, Impure Iron Ore 15.

Tests: `tests/test_m5c_craft.py` (the arithmetic alone, the Java literals and tables today and on an edited copy - with every edit the
review's method-by-method check let through, now refused by the class pins -, the config layers, the whole report on a small static_data
tree with a distinct rate array per key at membership 1 and a level difference of 24, a new character's gathering in Poeta and Ishalgen,
the repeated-item and shadowed recipes 155101624 and 155101544, and recipe 155001381 *Roast Inina* under the gate profile on the real data:
2 x 160001001, 141 xp, cooking 1 -> 2, 11-36 s).

## M5d quest oracles (`m5d/`, `docs/design/m5d-plan.md` G-01, D9)

```
python oracle.py m5d-quest --quest 1101 [--race R] [--class C] [--level N] [--exp X] [--gender G] [--completed ID[:GROUP] ...]
                           [--inventory ITEM[:COUNT] ...]
python oracle.py m5d-quests --map 210010000 [--race R] [--class C] [--level N] [--gender G] [--completed ID[:GROUP] ...] [--started ID ...]
                            [--inventory ITEM[:COUNT] ...] [--game-hour H ...]
    (both: [--java-src game-server/src] [--java-handlers data/handlers/quest] [--config config] [--profile FILE | --no-profile])
```

The list options take several values after one flag or the flag again: `--completed 1101 1102` is `--completed 1101 --completed 1102`.

`m5d-quest` (`aion-m5d-quest`): one quest from `QuestService`, `QuestTemplate`, `XMLQuests` and its template handler.

- `handler`: `registry` `xml` (an XML template, the only kind the C++ registry has until phase 6 - D9), `java` (a class under
  data/handlers/quest; a Java handler wins over an XML template of the same id, `QuestEngine.addQuestHandler` is putIfAbsent) or `none`; for
  XML the kind, data class, template class, file, every JAXB field with its value (`parameters`), what JAXB drops and what the handler ignores;
- `registration`: what the handler's `register()` adds (onQuestStart/onTalkEvent/onKillEvent npcs, enter world, level, zone, distance, items,
  skills, PvP worlds and zones), `start` and `prerequisites` (race, level window, classes, gender, rank, combine skill, npc faction, inventory
  items, the `<start_conditions>` groups with `requiredConditionCount`, the previous quests, the repeat data);
- `steps`: the happy path as QuestState changes - per step the npcs, the state (`startable`, `START`, `REWARD`), the dialog action (name and
  id), the `SM_DIALOG_WINDOW` page and quest id it answers with, and the effect (status and its `value()`, the six 6-bit vars, ADD/UPDATE,
  items given and taken, finishQuest); `targets` has the kill/skill runs (`killRun`: kill by kill the vars, the status and the
  `SM_QUEST_ACTION` updates, including one kill too many), the talk npcs, the collect items and quest drops;
- `rewards`: every reward group and the extended rewards as `giveReward` pays them with the configured rates (`gameserver.rates.*` of the
  profile - `<config>/mygs.properties` when it exists, `--profile FILE`, none with `--no-profile` - over config/main/rates.properties over the
  `@Property` default, as `Config.loadProperties` layers them; membership 0, no quest-xp stat, no legion bonus: kinah `(long) (gold * rate)`
  and exp `(long) (exp * rate)` in float arithmetic, XP_QUEST uncapped), and `firstCompletion`: what `SELECTED_QUEST_NOREWARD` (23) pays at
  the first completion, in payment order (items, kinah, exp, title, AP, DP, GP, cube; DP does nothing for a starting class and is capped at a
  daeva's max DP, which is not computed);
- `followUp`: for a character of `--race` (default the quest's race), `--class` (WARRIOR), `--level` and `--exp` before the reward (default:
  the quest's minimum level at its start exp; `--exp` alone gives the level), whose quest list is `--completed` plus this quest and whose cube
  holds the new character's items and what `--inventory` states, the window `sendQuestEndDialog` opens after `finishQuest` at each end npc,
  at the level after the reward (`levelsSinceEnterWorld`: setExp per paid exp block). A report_on_levelup quest the character would hold in
  REWARD (started at enter world or at a level change) that ends at this npc answers first: `window` null. Else the first quest of the npc's
  onQuestStart HashSet (Java's iteration order over the XML registration order) that passes `checkStartConditions`, names this quest in a
  `<finished>` and `isAcceptableQuest` gets QUEST_SELECT: its start page, or - when its template leaves QUEST_SELECT unhandled
  (relic_rewards, fountain_rewards) - DialogService's next page `{page: 23, questId: this quest, nextPage: true}` (the finishing action's id:
  23 for SELECTED_QUEST_NOREWARD, 8..22 for SELECTED_QUEST_REWARD1..15), or no packet at all after a relic_rewards quest (`sent: false`).
  Else page 10 when any quest there is startable, else 0. `window` is null with `windowUnknownBecause` when a candidate that could decide it
  has an unknown start check (gender, crafting skills of a level 10+ character, an inventory item a quest of the list may have given or taken).

`m5d-quests` (`aion-m5d-quests`): the npcs `SpawnEngine.spawnAll` puts on the map, the quests their QuestNpc starts (XML registry and the
resolved Java handlers), and per quest `checkStartConditions(player, id, false, 2, ...)` for a character of the race (default: the map's
world type), class, level, gender and quest list: `nearby.xmlOnly` is the `SM_NEARBY_QUESTS` set of the C++ registry, `xmlOnlyGrey` the ids
with bit 17 (`minlevel_permitted` above the level), `xmlOnlyWire` the ids as written, `withJava` the Java server's set. `xmlOnlyWireOrder` and
`withJavaWireOrder` give the written values in Java's order as buckets of the packet's `new HashMap<>()`; the order inside a bucket (the map
instance's ConcurrentHashMap key set order) is not modelled, so the whole order is exact (`exact: true`) only when every bucket holds one id
(Poeta at level 1: 1105, 1108, 1109, 1127, 1112, 1101; Ishalgen: 2101 and 2133 share a bucket). `registry` is the census (8,043 templates,
4,184 XML quests in 89 files by kind, 1,035 Java handlers, 0 in both, 2,824 in neither). `exact` is false when a start npc's spawn depends on
a game time not given, or a quest giver comes from a timed event or a service spawn (siege, base, rift, vortex, mercenary, ahserion, town) -
those are listed under `notModelled`.

The cube of both commands is the new character's (m5a-creation, the items that are not equipped). A quest of the list may have changed it:
a COMPLETE quest paid its reward items (any group, selectable, extended, class lists, a random `<bonus>` item) and work order components and
took its collect items, work items and report_to_many start item; a START quest holds its work items, start item, components, quest drops
and collect items. An `inventory_items` check on such an item has no answer until `--inventory ITEM[:COUNT]` states the cube (`ITEM:0`: not
there) - exit code 2 in `m5d-quests`, a null follow-up window in `m5d-quest`.

Java tables are read from the sources: the JAXB bindings of every quest class (field types and initializers), XMLQuests' 16 tags, the
template class each data class constructs, DialogAction ids, DialogPage ids and `getRewardPageByIndex`, QuestStatus values, the enums, the
`Rates` and `giveReward` shapes, the combine-skill lists, AbyssRankEnum's first rank, the config defaults. What is not modelled is `null` with a
reason in `notModelled`: the xmlQuest operation language (1127), mentor group conditions, the random `<bonus>` item, class-selectable and
extended selectable items chosen by the client, dynamic start triggers (rift/vortex invasion, zones, distance). Exit code 2 (`OracleError`)
for data or code Java itself fails on or the oracle does not model: an XML quest without a quest template, with an empty
`<quest_work_items>`, a kill_spawned `<monster>` or an xml_quest on_kill_event `<monster>` without npc_ids (the template constructors and
`register()` throw at startup), a kill_spawned quest whose kills never reach REWARD, a crafting_rewards level_reward other than 400 or 500
(`canLearn` throws in every dialog), an unknown enum constant, an integer that is not plain decimal, a single-valued element given twice, a
quest var index beyond the sixth, a timed event overriding a quest rate key, a continued or escaped properties line for a used key, a changed
formula shape, an instance map, a nearby set that depends on the gender (`--gender`), on the crafting skills of a level 10+ character or on an
inventory item a quest of the list may have given or taken (`--inventory`), a level outside 1..65 (66 `<exp>`, the level at most getMaxLevel() - 1), a starting
class above level 9 or a daeva class below 10, an `--exp` that is not an exp of `--level`, a quest in `--completed` of its own report.

Tests: `tests/test_m5d.py` (the var arithmetic of the handlers alone, Java's HashMap order and buckets and the Java text readers, the Java
tables today and on an edited copy, the whole report on a small static_data tree with a fixture Java handler and config, one fixture quest per
QuestService rule with every rate apart and a profile, and on the real data 1101 -> 1102 -> 1103 in Poeta, 2101 and 2102 in Ishalgen, both
start maps' marker sets and wire orders, and the float rounding of 21040's exp).
