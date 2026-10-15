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
- The skills an effect or a skill template launches, followed to a fixpoint over (skill id, level, whether it runs `Skill.endCast`)
  (format version 2; m5b2-plan.md §10, m5e-plan.md §2.4 lesson 2): `<subeffect skill_id>` at level 1 with its `chance`
  (`EffectTemplate.calculateSubEffect`), `provoker` (chance `hittypeprob2`), `delayedskill`, `skilllauncher`, `condskilllauncher` and
  `aura` `skill_id` at the launched template's `lvl`, `carvesignet` `signet_id + level - 1` for every level a carver of the same signet
  stack can leave on the target, and the template's `penalty_skill_id` (kind `penalty`, `Skill.startPenaltySkill`): at the launched
  template's `lvl`, or at level 1 and cast in turn with `penalty_skill_send_msg`, and only from a skill that runs `endCast` - an npc's
  row, the soul sickness, an autolearnt or `--skill` skill that is not PASSIVE, or a penalty skill cast with the message; never a skill
  an effect launches. Each entry lists its `launches` (the effect - null for a penalty -, the launched skill and level, the chance,
  `followed`) and its `launchedBy`; a launched skill is an entry with source `launch`; the character and each `--npc` get
  `launchedSkills` and `effectClasses` with `addedByLaunches`. The launching classes and every modelled shape are read from the Java; any
  other launch of an effect class or a skill template (resurrect / rebirth, pet orders, summons, `addeffect`, two `<subeffect>`s on an
  effect or two effects with one in a skill, a missing or template-less id, a modelled launcher that launches in more ways) is refused
  with exit code 2. Out of scope: the 10 % critical hit proc of a Player (8218 with a polearm, staff or greatsword, 8217 with a bow;
  `SkillEngine.createCriticalProcEffect`), which the equipped weapon decides - the starting weapons launch nothing - and what AI
  scripts, items, chain or charge skills cast.

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

## M5c economy oracle (`m5c/economy.py`, `docs/design/m5c-plan.md` G-01)

```
python oracle.py m5c-economy --npc 798007 --npc 700000 --npc 203336 --npc 203064 --npc 798008 [--map 210010000] [--near X,Y,Z]
                 [--far 10] [--direction DEG] [--recover-exp 1000] [--npc-expands N] [--quest-expands N] [--item-expands N]
                 [--mail 162000002:5:200 --mail 0:0:10 ...] [--item 100000133 --item 110100355 ...] [--manastone 167000226 ...]
                 [--membership M] [--class MAGE] [--race ELYOS] [--level 1]
                 [--daeva GLADIATOR [--daeva-old-level 2]] [--craft-recipe 155001381 --craft-tool 150000009 [--craft-map 110010000]
                 [--craft-distance 3 --craft-distance 7 --craft-distance 12]]
                 [--config DIR] [--profile F | --no-profile] [--set KEY=VALUE ...] [--influence RACE=N ...]
                 [--java-src DIR] [--java-handlers DIR] [--commons-src DIR]
```

The gate constants `m5c-trade` and `m5c-craft` do not answer (stage 0 of G-01), in one JSON document (`aion-m5c-economy`); the Java methods
are named with file:line in the module docstring:

- `talk` per `--npc`: its spots on `--map` (each with the effective AI of the spot and whether it stays where it is), the chosen one (the
  nearest to `--near`, else to the first npc's), the talk distance, both bound radii, `limit` (isInTalkRange's `talk + 1` plus the npc's and the
  player's BoundRadius.getMaxOfFrontAndSide, float), `limitWithoutPlusOne` and `limitCenterToCenter`, and three float-checked spots along
  `--direction`: the **X2 `bandSpot`** in the middle of `[max(talk + radii, talk + 1), talk + 1 + radii)` - admitted by isInTalkRange and refused
  both without its "+ 1" and centre to centre -, a `nearSpot` 2 m away and a `farSpot` (`--far`) outside the range, each with the other
  reported npcs it is in talk range of; `outOfRange` (STR_DIALOG_TOO_FAR_TO_TALK for an is_dialog npc, else STR_WAREHOUSE_TOO_FAR_FROM_NPC;
  null for an npc without talk_info, which onDialogRequest leaves before the range check),
  `startWindow` (GeneralNpcAI -> DialogPage.getStartPageId: 10 for a function npc, 0 without a conversation; PostboxAI: MAIL 18 with the
  mailbox state REGULAR 1 in the last short) and `functions` (REMOVE_ITEM_OPTION's page 20, RECOVERY's and EXTEND_INVENTORY's questions,
  BUY/SELL named for m5c-trade);
- `recovery` for `--recover-exp`: the double factor, the price (249 for 1,000), STR_ASK_RECOVER_EXPERIENCE with its parameter, and what yes
  pays, gives back and says;
- `cube` for each `--npc` with EXTEND_INVENTORY: the cube_expander template, canExpand and the level window against `--npc-expands`,
  `--quest-expands`, `--item-expands` and the config's limits, the raw price (1,000 at Poeta), STR_WAREHOUSE_EXPAND_WARNING and SM_CUBE_UPDATE
  after yes;
- `manastoneRemoval`: getPriceForService(650) per race (917 with sieges off);
- `mail` per `--mail ITEM:COUNT:KINAH[:express]`: the base cost and cost factor, the float item commission (quality rate), the kinah
  commission and what the sender pays per race (251 for five Minor Life Potions and 200 kinah, 23 for 10 kinah alone);
- `items` per `--item`: `breakItem` (effective level, roll range, the stone ids with their probabilities, the count range - Alpha only and 2-5
  for a Plainsman's weapon), `identification` (maxTuneCount after ItemTemplate.afterUnmarshal, whether a new item is unidentified, the SQL
  default of `inventory.tune_count` read from `sql/aion_gs.sql` and whether it loads identified - it does, so a seed that must load
  unidentified writes -1 - and the socket, enchant-bonus and stat-bonus rolls) and `equip` for `--class` and `--race` at `--level`:
  equipItem's checks in its order - isClassSpecific, getRequiredLevel (with the start exp of that level), getMaxLevelRestrict (restrict_max),
  the item's race, checkAvailableEquipSkills (the item group's getRequiredSkills against the autolearn skills learnNewSkills(1, level) teaches
  the class, its starting class's below level 10; `character.learnedSkills` lists them) and the equipment slot -, `refusedBy` naming the first
  that fails and `message` its system message (null for the two that refuse without a packet). C15: a Plainsman's armour piece needs level 4
  (exp 3,820), and a Mage wears only its robe pieces (Tunic 110100355, Leggings 113100293, Shoes 114100311): it knows 103 of the equip skills,
  not the chain, leather or sword ones.

Every modelled member (75 in the game server, GeneralNpcAI.handleDialogStart and PostboxAI in data/handlers, commons' Rnd.get) is fingerprinted
whole as `m5c-trade` does, and the statements the report leans on (the XML defaults, the learn calls at creation, level change and enter
world) must be present verbatim, so an edit in one is exit 2; the tables and literals (DialogAction, DialogPage, PlayerMailboxState, the
question and system message ids, the player bound radius, EnchantmentStone, calculateEffectiveLevel's arms, the removal base price, sendMail's
costs and rates, ItemGroup's required skills, the SQL default) are read. Exit 2 also for: an npc whose DIALOG_START AI is neither GeneralNpcAI
nor PostboxAI, a subdialog_type, a town npc, a moving npc or none on the map, a band no float spot fits, a quality calculateEffectiveLevel
answers 0 for, unknown ids, classes, races or levels, an item group requiring 30001/30002 (the daeva branch of learnNewSkills), sieges without
`--influence`. Not modelled (stated in `assumptions` or as null): a quest handler that answers USE_OBJECT first (the M5d oracle's field), the
player's known list, trading and hide state, a rnd_bonus set's stat bonus draw, skills not learned by autolearn, equipItem's gender, rank and
cube-space checks and everything after its slot check.

Stage 1 (harness-b) added the rest of G-01 (`m5c/sanctum.py` for the C19 blocks; the Java methods with file:line in its docstring and in
`socket_block`'s):

- `items[].socketing` per `--manastone` (C16, X24): CM_MANASTONE arm 2 with targetFusedSlot 1 on the item without stones -
  `new EnchantItemAction().canAct` (a stone id / 1,000,000 of 166 or 167, an item id / 1,000,000 below 120; a refusal sends nothing and keeps
  the stone), socketManastone's slot level `(int) (10 * ceil((itemLevel + 10) / 10d))` against the stone's level, the sockets
  min(m_slots + optional sockets, MAX_BASIC_STONES) of a weapon or an armour and none of any other equip type (Item.getSockets: the
  [Event] Extraction Greatsword 100901051 is refused with `noSocket`) and the float chance Rates.get(MANASTONE_CHANCES) (at `--membership`, * 0.8f from RARE
  on) + (slotLevel - stoneLevel) / 1.75f; `certain` for a chance of 100 or more (D6's 200). The seeded Plainsman's robe pieces take
  167000226 (level 10) with certainty;
- `daeva` for `--daeva CLASS` (C19's seed, X21a; the race is `--race`): the exp getStartExpForLevel(10) = 126,069, the ascension quest
  (1006 Elyos, 2008 Asmodians), the level at load with the quest (10) and without it (9, m5c0-client-session.md F-1), and what the enter
  world's onLevelChange(`--daeva-old-level`, 10) teaches: learnNewSkills(old + 1, 10) over the starting class's rows below 10 and the class's
  own, each learned skill with its level and class (per level in getTemplatesFor's order, the race's rows first), the Daeva swap 30001 -> 30002 (SM_SKILL_REMOVE), the final skill list of the burst's
  SM_SKILL_LIST and the SM_LEARN_RECIPE recipes of the learned crafting and morph skills (the Elyos morph recipes 155000001, 155000002,
  155000005); `needs` names M5d's QuestState restore (F-1) and the disconnected seeding (F-3);
- `craft` for `--craft-recipe ID --craft-tool TEMPLATE` (C19, X17-X20): the recipe summary from m5c-craft (components, product, steps,
  finish times, xp, the skill level after), the profession master standing on `--craft-map` (Hestia 203784) with its talk block,
  COMBINE_SKILL_LEVELUP (46, level 10 and up) and its question STR_CRAFT_ADDSKILL_CONFIRM(ChatUtil.l10n of the skill's nameId, "3500"), the
  recipes learning the skill teaches, the vendors of each component on the map (m5c-trade's kinah for the quantity, the one nearest the
  master chosen: Luelas 203785, 140 for two Salt) and the seed items no vendor sells (one Inina), `exactKinah` (3,640), a `seedSpot` beside
  the master, and the tool's static objects (every spot of the STATIC group, the one nearest the master chosen: Oven static id 103) with the
  spots at each `--craft-distance` along `--direction`, checked against CM_CRAFT's centre-to-centre 10 and checkCraft's 5 plus both bound
  radii (m5c-craft's `craft.station`): 3 m crafts, 7 m answers STR_COMBINE_TOO_FAR_FROM_TOOL, 12 m nothing. Since the review of the gate-2
  lane (2026-09-28) `craft.recipe` also carries every analyze tick's `executionSpeed` and `showBarDelay` (the product bar's, 900 and 1200 at
  the level difference 0) and m5c-craft's SM_CRAFT_UPDATE rows of the start pair, the end and sendCancelCraft (`updates`: speed 0, delay 0),
  and the SM_ACTION_ANIMATION(CRAFT_LEVEL_UP) of the craft's level-up (`skillUpAnimations`, none for level 2) and of the learn
  (`learn.yes.animations`: onLearnSkill's level 1 of a crafting skill; the id is ActionAnimation's, 4).

The C19 blocks need the m5c-craft context over the same data and profile (the command builds it); their own Java members (setExp,
updateDaeva, the experience table, PlayerSkillList.addSkill/removeSkill, RecipeList.addRecipe, SkillLearnTemplate.getSkillLevel, the
COMBINE_SKILL_LEVELUP arm, Profession.getClientName, SpawnEngine.spawnInstance, StaticObjectSpawnManager, PlayerController.see,
SM_GATHERABLE_INFO) are fingerprinted like the rest, and the literals (the quest ids, the level cap, the Daeva swap, the dialog action, the
question id, the learn level, ActionAnimation.CRAFT_LEVEL_UP's id) are read. Exit 2 also for a starting class for `--daeva`, an old level outside 1..9, a recipe of another race
or of the morph skill, no master or no tool spot on the map, a tool group with a pool, an enchantment stone for `--manastone`.

Tests: `tests/test_m5c_sanctum.py` (the level at load and the row filter alone, the C19 literals today and 12 edited copies refused, the
Daeva, the craft and the socketing on a small static_data tree - every learn-list arm, the vendor and tool choices (a vendor that does not
sell, one that only walks, a regular spawn of the oven's template, a master without COMBINE_SKILL_LEVELUP), the ranges, the refusals -, the
Daeva swap without 30001 or with 30002 known and a stored crafting skill on a second skill tree, the CLI, and the gate's Daeva, Sanctum craft
and ovens, robe pieces and an extraction sword on the real data) and `tests/test_m5c_economy.py` (the service price, the recovery price and the talk range with the band alone, the Java tables today and
33 edited copies refused, a comment accepted, the SQL default and two literals followed, the whole report on a small static_data tree -
every start window, the refusals, the cube arms, the mail, extraction, identification, every modelled equip check and the learned skills -
the CLI, and the gate's Poeta npcs and Plainsman's items on the real data, C15's level-4 Mage included).

## M5d quest oracles (`m5d/`, `docs/design/m5d-plan.md` G-01, D9)

```
python oracle.py m5d-quest --quest 1101 [--race R] [--class C] [--level N] [--exp X] [--gender G] [--completed ID[:GROUP] ...]
                           [--inventory ITEM[:COUNT] ...]
python oracle.py m5d-quests --map 210010000 [--race R] [--class C] [--level N] [--gender G] [--completed ID[:GROUP] ...] [--started ID ...]
                            [--inventory ITEM[:COUNT] ...] [--game-hour H ...]
python oracle.py m5d-quests --registration-order [--npc ID]
python oracle.py m5d-quests --census
    (all: [--java-src game-server/src] [--java-handlers data/handlers/quest] [--config config] [--profile FILE | --no-profile]; the last
     two take no character or game-time option: --race, --class, --level, --gender, --completed, --started, --inventory and --game-*/--weekday
     are refused, exit 2)
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

`m5d-quests --registration-order [--npc ID]` (`aion-m5d-registration-order`, `m5d/registry.py`, for T-04's case): the XML-only registry (D9)
as `QuestEngine.init` builds it. `order` is the registration order, the iteration order of `XMLQuests.questsById` (a `new HashMap<>()` filled in
document order); `questOnEnterWorld` and `questOnLevelUp` (per race: a quest without race_permitted is in both, a PC_ALL quest in a PC_ALL list
nobody reads) are the engine's lists; per npc `onTalkEvent` and `onKillEvent` are in registration order and `onQuestStart` is the
`HashSet<>(0)`'s iteration order. Without `--npc` every npc's three lists; with it that npc's lists, each with `...Flags` (whether it is also
the ascending or the document order, and for the start set its insertion order: such a list cannot tell a sorted container, an
insertion-ordered registry filled in document order or an insertion-ordered set from Java's), the set's insertion order and the Java
handlers that start there (not in the C++ registry). `talkLists` and `killLists` count the npcs whose list is neither ascending nor in
document order and name them, `startLists` the npcs whose set is none of the three orders. On the data: `questOnEnterWorld`'s 26 ids are
neither; mires 203057's talk list `[1101, 1102, 1103, 1104]` is ascending, but its start set `[1104, 1102, 1103]` is none of the three (the
insertion order is `[1102, 1103, 1104]`, the order of the C++ runtime::HashSet, which iterates in insertion order); 137 talk lists, 51 kill
lists (of 136 not ascending) and 386 start sets (of 447 not in insertion order) tell the orders apart.

`m5d-quests --census` (`aion-m5d-census`): m5d-plan.md §2.3-§2.5 re-derived. The templates and the handlers by category; where each XML quest
starts, a partition (`start`, first match): minlevel 99, no start npc, a start npc spawned at startup (a regular spawn of an open-world map,
difficulty 0, no handler, an Npc with a template) with a talking ai (general, aggressive), `simple_abyssguard` or another ai, each talking row
split by a Java-handled `<finished>`/`<acquired>` precondition that leaves fewer passing groups than required; spawned only by a service
(siege, instance, base, vortex, ahserion, rift, mercenary); never spawned, split into town spawns (TownService spawns them at startup at the
town's level), house spawns (HousingService spawns every house of an open-world map but the studios at startup, and each house its land's
manager, teleport and sign npcs per house_npcs), timed-event spawns, inert spawns (static, another handler, another difficulty, no template,
no spots, no world map) and `noSpawnData` (no spawn, town, house or event data: Java code spawns the npc or nothing does), whose `namedInJava`
is a text scan of the Java sources for the start npc id as an int literal - a name, not a proof of a spawn: outside data/handlers/quest
(instance and ai handlers, game-server/src), only in data/handlers/quest (phase 6), or nowhere. Then what completing the reachable quests
needs (dialogs and kills, quest loot, crafting, items from elsewhere, quest objects, turn-ins, skill use, PvP kills), E-09's reward bodies
(bonus, AP, GP, cube, warehouse) over all XML quests and over the reachable ones, the CHALLENGE_TASK quests by row and the Poeta and Ishalgen
quests by handler. On the data it gives §2.4's 2,072 / 439 / 270 + 37 / 322 / 498 / 415 / 126 / 5 and 2,511 reachable. It also splits the
498 "never spawned" quests: TownService spawns the givers of 54 at startup, all of them at a town level above 1, the houses the butlers of 2
(18829, 28829), timed events the givers of 240, and 202 are in no spawn data - 24 of those start at an npc an instance or ai handler names
(16991 at 802048, IlluminaryObeliskInstance; 30225 at 216527, BeshmundirInstance; ...), 5 at one only phase-6 quest handlers name, 173 at
one no Java file names.

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
start maps' marker sets and wire orders, and the float rounding of 21040's exp); `tests/test_m5d_registry.py` (the registration order and
every list on eight fixture quests whose HashMap order is neither ascending nor in document order, the flags and list summaries on six more
where they differ, one fixture quest per census rule including the house, inert-spawn and Java-scan rules, the command line and its
refusals, and on the real data the whole order, mires' lists, `questOnEnterWorld`, the list summaries and the census against §2.3-§2.5).

## M5e progression oracle (`m5e/`, `docs/design/m5e-plan.md` G-01)

```
python oracle.py m5e-progression --race ELYOS --class WARRIOR [--level N] [--daeva] [--known-skills FILE]
                                 [--step create enter:1 level:2 quit enter:8 level:9 class:GLADIATOR level:10 ...]
                                 [--skill 769 758 519 ...] [--weapon 100900038 ...] [--weapon-group G] [--dp N] [--robot]
                                 [--chain-after CATEGORY] [--target-kind PC|NPC] [--stigma ITEM ... [--profile FILE | --no-profile] [--config DIR]]
    ([--java-src game-server/src] [--java-handlers data/handlers/quest])
```

`m5e-progression` (`aion-m5e-progression`): one character walked through `--step`s, each step reporting the level, class, class id, Daeva
flag, the base max HP / MP of the class template (m5a's `stats_info_base_max_hp`), the skill list after it and its `events`: every
`PlayerSkillList.addSkill` that added or raised a skill, in Java's order, with `isNew` and the `SM_SKILL_LIST` message id
`SkillLearnService.sendPacket` gives it (`null` while the character is not spawned: the creation and an enter world's offline level change
send none), and every `removeSkill` (`SM_SKILL_REMOVE`; the Daeva's 30001 -> 30002). The steps: `create` (learnNewSkills(1, 1)), `enter:L`
(onLevelChange from the level stored at the last `quit` - `players.old_level` - before the spawn), `level:L` (online), `class:CLASS` /
`action:ID` (the simple class change, `changeClassToSelection` with validate and the Daeva update; `accepted`, the `sendMessage` text of a
refusal), `quit`, `seed:CLASS` (the offline Daeva seed of a gate: the class and the ascension quest, nothing learned), `book:ITEM` (SkillLearnAction.canAct's refusal and learnSkillBook). `isNew` is PlayerSkillList.addSkill's walk:
`SkillTreeData.getSkillsForSkill` from the top of the skill's stack for the player's class at the time of the add, the recursion filtered by
the top template's own class and race, ported exactly (`getHighestSkill`, `createSkillTree`, `getTemplatesForSkill`). `--known-skills` seeds
the list (e.g. from the enter world's `SM_SKILL_LIST`) instead of an assumed history.

Static sections: `experience` (startExp per level and the non-Daeva cap), `classChange` (the race's ascension quest, the selection page of
the starting class, every dialog action with its class, class id and whether it is valid for the starting class - read from
`ClassChangeService` and `DialogAction`), `trainer` (the "A New Skill" quest and the class master, page, quest var and reward group of the
starting class, read from `_1205ANewSkill` / `_2132ANewSkill`), `messages` (sendPacket's ids), `skills` (per `--skill`: activation, tslot,
cooldown, duration, effects with their percentages, durations and launched skills, the costs, every condition and the refusals of the ones a
gate's cast meets - weapon group, DP, chain precategory, robot, target kind - and the `skill_charge.xml` entry of a charge skill),
`launched` (the same for every skill they launch) and `weapons` (item group, `ItemGroup`'s required skills and whether one is known after
the steps, the robot id), and `stigmas` (per `--stigma`: the stone equipped after the steps into an empty regular slot -
StigmaService.notifyEquipAction's kinah, the base of the stone's quality read from the Java through PricesService.getPriceForService with
the profile's prices, m5c's `service_price` and `race_prices`, and addStigmaSkills' temporary skills with their message ids). Exit code 2
for an unknown class or step, an online step of a character not in the world, a skill or item without a template, an item without
`<stigma>` given as `--stigma`, a siege-enabled profile for `--stigma` (the influence is database state), or Java text whose shape the
readers do not recognise.

Not modelled: stats beyond the class template's base (passives, gear), cast speed factors other than 1 in the charge selection
(`charged_skill` takes the factor), the npc and spot side (the gate reads `m5a-spawns` and `m5b-monster` for those).

Tests: `tests/test_m5e.py` (the charge loop and the skill-tree walk on hand-made rows; on the real data the Warrior's path from creation to
level 10 and 15 with every message id of m5e-plan.md §2.1 / §2.3 / X7, the class change tables of both races, the class master, the casts,
the chain follower after its opener, a skill book learned and refused, the weapons, a seeded skill list, a seeded Daeva's enter world, a stigma stone and the command line).

## M5f travel oracle (`m5f/`, `docs/design/m5f-plan.md` G-01)

```
python oracle.py m5f-travel [--npc ID ...] [--race ELYOS|ASMODIANS] [--profile FILE | --no-profile] [--config DIR]
                            [--hotspot ID ... --from X,Y,Z ...] [--obelisk NPCID ...]
                            [--portal NPCID ... --race R [--now-ms EPOCH_MS] [--tz local|UTC|+HH:MM|ZONE]] [--instance-exit WORLD ... --race R]
                            [--instance-spawns WORLD ... --near X,Y,Z --radius R [--difficulty N] [--game-hour H | --game-minutes M] [--weekday D]]
                            [--exp-for-level L] [--census [--cpp-src cpp/game-server]] [--geo-check [--geo-map ID ...] [--geo-dir DIR]]
    ([--java-src game-server/src] [--java-handlers data/handlers/quest])
```

Every selector is repeatable and they combine in one call. The answer (`aion-m5f-travel`) always has the keys `npcs`, `hotspots`,
`obelisks`, `portals`, `instanceExits`, `instanceSpawns` (lists, one entry per selector, `[]` when not asked) and `exp`, `census`,
`geoCheck` (objects, `null` when not asked). `--from` / `--near` are given once (shared) or once per `--hotspot` / `--instance-spawns`.
Floats are the Java float values of the XML decimals; headings are the template's int (`headingByte` = the `(byte)` `sendLoc` writes).

- `npcs[]`: `npc`, `name`, `ai`, `race`, `tribe`, `talkDistance`, `spots` (`map`, `x`, `y`, `z`, `heading`, `staticId` and the group flags,
  every spawn map), `daevaOnly` (DialogService's AIRLINE_SERVICE NO_RIGHT ids, read from DialogService.java:187-197), `teleporter`
  (`teleportId`, `type`) or null, `locations` in npc_teleporter.xml order: `locId`, `type`, `teleportId` (the location's `teleportid`, the
  flight id; 0 if none), `price`, `pricePvp`, `servicePrice` (PricesService.getPriceForService(price, race) with the profile's prices and
  sieges off - m5c's `race_prices` / `service_price`; what checkKinahForTransportation takes), `requiredQuest`, `map`, `x`, `y`, `z`,
  `heading`, `headingByte`, `name`, `hasPosition` (false for a FLIGHT location: the client flies the path). `--race` defaults to the npc's.
- `hotspots[]`: `hotspot`, `map`, `race`, `x`, `y`, `z`, `heading` (null: the player keeps its own, TeleportService.java:249-251),
  `basePrice`, `from`, `distance` (PositionUtil.getDistance: float differences, squares and sum, then Math.sqrt), `price`
  (`max(1, base + (long) (base * distance / 1000d))`), `doublePrice` (the same with a double distance: what a port must not do).
- `obelisks[]`: `npc`, `spots`, `bindPoint` (`id`, `name`, `price` raw - ResurrectAI uses no PricesService - `race`, `tribe`).
- `portals[]`: `npc`, `ai`, `spots`, `talkDelayMs` (talk_info delay in seconds * 1000, ActionItemNpcAI.getTalkDelayInMs), `talkDistance`,
  `talkRange` (+1), `paths` (every portal_use path with `selected` = Portal2Data.getPortalUsePath's choice for `--race`, the portal_path
  fields, the portal_loc `map`, `x`, `y`, `z`, `heading`, `instance` (world_maps), `maxPlayers`, `enterMinLevel` / `enterMaxLevel` as
  PortalService.checkEnterLevel computes them), `dialogPaths`, `cooltime` (the instance_cooltime row of the selected path's world) or null,
  `nowMs`, `timeZone`, `reuseTimeMs` (InstanceCooltimeData.calculateInstanceEntranceCooltime: DAILY/WEEKLY at `ent_cool_time` HHMM in the
  server zone, the next day once now is after it, WEEKLY to the next `typevalue` day; RELATIVE now + minutes in int arithmetic; the
  membership rate taken as 1), `reuseRemainingSeconds` (what SM_INSTANCE_INFO writes), `exit` (InstanceExitData.getInstanceExit) or null.
  The zone: `--tz`, else `gameserver.timezone` of the profile / config/main, empty = the machine's (`local`, through time.localtime: Windows
  has no zoneinfo database, so a zone name works only where Python has one).
- `instanceExits[]`: `world`, `race`, `exit` (`instance`, `map`, `race`, `x`, `y`, `z`, `heading`) or null.
- `instanceSpawns[]`: SpawnEngine.spawnInstance with m5a-spawns' rules, a group of `difficult_id` = `--difficulty` counted as the
  instance's: `spots` within `--radius` of `--near` (nearest first) with `npcId`, `name`, `x`, `y`, `z`, `heading`, `ai` (the spot's
  override or the template's), `spawned` (true / false / null = random or unknown time), `fixed` (spawned, no walker, no random walk),
  `temporary`, `pool`, `walker`, `staticId`, `distance`; `total` over the whole instance.
- `exp`: `level`, `exp` (PlayerExperienceTable.getStartExpForLevel), `maxLevel`.
- `census` (the only part that reads the C++ tree): `w14` per advanced class seeded at level 16 (the m5e progression model's
  learnNewSkills(1, 16)): `passives`, their `effectClasses` and the `unported` ones (`skillengine/effect/<Class>.cpp` with
  `AION_UNPORTED(`, or no source and no header in src or generated/ - a generated data-only class counts as ported), `partial`,
  `unportedBases`, `unportedBySkill`; `haramel`: the npcs of 300200000's spawn map, their npc_skills, the effect classes, and
  `unportedEffectClasses` / `unportedBy` (`npcId`, `skillId`). Launched skills are not followed.
- `geoCheck` (§10.5 G3): every destination of §2.9 (the REGULAR locations of 203194, 203679, 203091, 203581, 203726, 204191, every hotspot
  of the four start maps, Haramel's portal loc and exits) with `geoZ` = GeoService.getZ(x, y, z) (z + 2 .. z - 2, else a z +- 50 probe) and
  `dz`; `outside` lists |dz| > 1 or no surface; `notModelled` the flight locations (no data position) and what the emulation calls
  ambiguous. A point on a terrain cell border (every hotspot at whole coordinates) is probed 1 cm away and marked `nudged`. The whole
  check loads 19 maps (~1.5 min); `--geo-map` narrows it.

Exit code 2 for an npc without a template, a raceless npc's prices without `--race`, a portal or exit without `--race`, an unknown hotspot,
an obelisk without a bind point, a level above the experience table, a zone Python cannot resolve, or Java text of an unknown shape.

Tests: `tests/test_m5f.py` (the price truncations, a hotspot vector where the float and the double distance give different prices, the
cooltime around 09:00 in UTC, a fixed offset and the local zone, WEEKLY and RELATIVE; on the real data m5f-plan.md §2.9's numbers - Daines,
Kustanon, Aero, Urakron, Osmar, Ukin, the obelisks, hotspot 13 from the Elyos spawn, Haramel's portal, cooltime, reuse time, exit and
spawnInstance set, the level-16 exp; the census on a mock C++ tree that reproduces W-14 and W-21, and the geo check on Poeta and Haramel).

## M5j command oracle (`m5j/commands.py`, `docs/design/m5j-plan.md` H-01)

`oracle.py m5j-commands [--alias //kill .help levelup ...] [--l10n ID ...]` reads the chat commands as the Java server builds them: the
access levels of `config/administration/commands.properties` (152 aliases); each command's alias, description and syntax info from its own
constructor's `super(...)` (string literals, text blocks and `+` concatenations of them; a command with a computed part is listed under
`unresolved` with the expression, e.g. `Bookmark_add`'s `ALIAS` and `Easter`'s `ChatUtil.item(...)`), keyed by the alias WITH its prefix
(`//`, `.`, or none for a console command) as ChatProcessor.registerCommand keys it; the parsed syntax info (ChatCommand.parseSyntaxInfo),
the `help` answer as the parts of ChatUtil.split (UTF-16 code units, the l10n and link estimates of findSplitIndex), the access message
(AdminCommand.validateAccess), the ChatType ids, ChatUtil.l10n as UTF-16 code units, the whisper level (custom.properties) and the
non-Daeva level cap (PlayerCommonData.setExp). `gs.scenario.gm`'s X2 compares every stage-0 command's help with it, and X3 takes its access
text from it. Tests: `tests/test_m5j.py`.

## M5j social oracle (`m5j/social.py`, `docs/design/m5j-plan.md` §10.4, §18.1)

`oracle.py m5j-social [--profile FILE | --no-profile] [--set KEY=VALUE ...] [--message NAME ...] [--question NAME ...] [--daeva-level N ...]
[--pvp-kill VICTIM_AP,VICTIM_LEVEL,WINNER_AP,WINNER_LEVEL] [--membership M]` gives what `gs.scenario.m5j` asserts: SM_SYSTEM_MESSAGE and
SM_QUESTION_WINDOW ids by name; the whisper and search levels, the search's faction and GM switches, the PvP kill limit, the AP cap switch and
the PvP AP rates (Config.loadProperties' layering, m5c/trade_config); the first titles of each race (player_titles.xml); AbyssRankEnum; the
Daeva seed of a level (the ascension quests and exp of m5c/sanctum); and one solo PvP kill's AP - StatFunctions.calculatePvPApLost and
calculatePvpApGained in Java float arithmetic, Rates.AP_PVP_LOST / AP_PVP with the membership's rate, AbyssRank.addAp's floor and the rank
after. The arithmetic's Java statements are checked in the source first (a change fails the oracle). Tests: `tests/test_m5j_social.py`.

## Phase-6 golden quest traces (`questtrace/`, `docs/design/phase6-inventory.md` §7.6 item 3)

`questtrace/extract.py` turns a Java quest handler into its expected behaviour, written from Java only: from the handler source,
`AbstractQuestHandler.java`, `QuestService.java`, `QuestState.java`/`QuestVars.java`/`QuestEnv.java` and the DialogAction, DialogPage and
enum tables (`m5d/javasrc.py`). It reuses the Java statement parser of the quest generator (`tools/gen/questgen/jast.py`, over
`tools/gen/javasrc.py`) and never imports the generator itself (`questgen.emit`); a test checks that in a fresh process.

Two consequences of that reuse. The parser is common to the generator and the oracle: a jast mis-parse (precedence, associativity, labels)
would give the C++ and the expected trace the same wrong meaning, so `tests/test_quest_trace.py` pins the precedence and associativity the
oracle relies on through evaluated effects (`10 - 4 - 3` is 3, `true || false && false` is true, ...). And the `tools.oracle` tests now
import `tools/gen`: an edit to `tools/gen/questgen/jast.py` or `tools/gen/javasrc.py` can turn `tools.oracle` red as well as `tools.gen`.

Every hook (the overrides of AbstractQuestHandler's `on*Event` methods and `rideAction`) is executed symbolically; each return leaf is a
case in `expected/quest/<questId>.json` (`format` `aion-quest-trace`, `version` 1):

| Field | What |
|---|---|
| `given` | the inputs the path read, with a value that satisfies every guard on it: `target` (`{kind: npc, npcId}` or `{kind: none}`; a target the guards exclude is the first of the handler's registered npcs, else of `OTHER_NPCS`, the first three templates of `npc_templates.xml`, that no guard names), `questState` (`null`, or `status`, the `vars` slots read, `canRepeat`, the reward group), `dialogAction` (`name`, `id`), `inventory` (item id to count), `otherQuests`, `player` (race, level, class, gender), `item`, `args` (the hook's own int/zone arguments; a zone no guard names is `{anyExcept: [...]}`), `env`. An input the path never read is absent: any value does |
| `assume` | a helper's result a guard branched on (`if (QuestService.startQuest(env))`), by effect index |
| `guards` | the Java text of every condition taken, its outcome and its line, in order |
| `ranges` | an input an ordering guard bounded on both ends, `[lo, hi]`. Both ends satisfy every guard: an end a `!=` guard excludes moves inward (`!(var >= 1 && var < 10) && var != 10` is `[11, 63]`), so `given` holds `lo` and a harness can check `hi` too |
| `rangeExcludes` | the values inside `[lo, hi]` a guard excludes (none in the corpus today) |
| `free` | the QuestVars slots the path reads with no guard on them and uses in no effect argument and not in the return value (`"questState.vars.0"`): any value takes the same path with the same effects, so a harness may set one to what a helper the path calls reads (`checkQuestItems(env, 1, ...)` acts only at var 1). P6-Q, 2026-09-29 |
| `atHigh` | per ranged input, the same path with that input at the high end of its range and the others as given: `input`, `value`, `given`, `effects` and `returns` (or `throws`) evaluated there. `given` holds the low end, so a boundary moved by one (`var < 6` read as `var < 5`) or an expression replaced by its low-end constant (`var + 1` read as 2) fails the high end. P6-Q, 2026-09-29 |
| `effects` | the calls with side effects in order, arguments evaluated under `given`: the AbstractQuestHandler helpers (`sendQuestDialog` with its page, `changeQuestStep`, `giveQuestItem`, `removeQuestItem`, `playQuestMovie`, ...), `qs.setQuestVarById`/`setQuestVar`/`setStatus`/`setRewardGroup`, `QuestService.*`, `PacketSendUtility.sendPacket` with the packet built, `env.setQuestId`. The hook's own `env` and `player` arguments are left out; a varargs `int[]` is spread |
| `task` (on an effect) | the index of the `ThreadPoolManager.schedule` effect whose task made this effect (lane C, 2026-10-05): the task runs after the hook, below |
| `returns` / `throws` | the value (`true`, `"FAILED"`, `{resultOf: k}` for effect k's result, `{fromBoolean: ...}`), or `NullPointerException` when the path dereferences an absent QuestState or target (after the call's arguments are evaluated, JLS 15.12.4, so their effects are in the case) |

A document also holds `register` (the registration trace of `register()`, loops over constant arrays unrolled: the Python-only form of
phase6-questgen-prototype.md §8.2) and `hooks` (each hook with its case count, or the reason it is refused).

The cases are call traces. A helper call is an effect with its arguments, not expanded into what it sends: a recording double of
AbstractQuestHandler/QuestState (the link seam of §7.6 item 3) returns the assumed results and compares the calls; a harness on the real
engine after M5d compares the observable subset (dialog pages, var and status writes, items, movies) and lets the ported helpers run.
Assumed helper results are not checked against the helper's own logic, except the two results `dead_assumption` knows Java cannot return
(below).

Guards are equalities, set membership, ranges and their negations over single inputs (a linear offset such as `var + 1 == 3` is solved),
so a satisfying value is picked directly and an infeasible branch is dropped. Each label of a multi-label `case` is its own case. State
writes are read back (`setQuestVarById` then `getQuestVarById`); after a helper whose Java body may write state (`changeQuestStep`,
`defaultCloseDialog`, `sendQuestEndDialog`, ...), a read of that state refuses the hook, as do a call outside the helper table, a loop that
is not over a constant array, a guard over two inputs and floating-point arithmetic. `QuestService.startQuest` is modelled (false changes
nothing, true sets START on `env.getQuestId()`, which is the handler's quest unless `env.setQuestId` changed it). Java's int arithmetic
(32-bit wrap, `/` and `%` toward zero), casts (JLS 5.1.3), `++`/`--`, compound assignment, switch fall-through, `break`/`continue` in a
for-each over a constant array, `QuestVars.setVar`'s six 6-bit slots and `DialogPage.getRewardPageByIndex` are modelled, each pinned by a
test.

Lane C (phase 6 step 1, 2026-10-05; `docs/design/phase6-transliterator.md` §7). The extractor parses with jast's `closures=True`, so a
lambda or an anonymous `Runnable` refuses the hook that builds it, not the whole file, and a construct the parser refuses (a method
reference) refuses its hook only. Since the review of #79 it parses with `switch_expressions=True` too: a switch expression has the value
of the arm whose label equals the subject, else of `default` (JLS 15.28; one without `default` is refused), and a switch statement with
rule arms (`case A -> ...`) never falls through (JLS 14.11.2). One closure is modelled: the task of `ThreadPoolManager.getInstance().schedule(task,
delay)` (ThreadPoolManager.java) with a constant delay. The call is an effect `ThreadPoolManager.schedule` with the delay; the task runs when
the hook has returned (Java runs it on a pool thread after the delay), in the order of the delays (equal delays in schedule order), on the
locals it captured (Java captures effectively final locals, so their values at the schedule), reading the state the hook left; its effects
follow the hook's, each with `task` naming its schedule effect, and its `return` ends it. A task that would throw (Java's pool logs that
exception after the hook returned), a task scheduling another one and a hook that throws after a schedule are refused. The item-use
handlers around such tasks need `PacketSendUtility.broadcastPacket(player, packet, toSelf)` (an effect with the packet and the flag; `new
SM_ITEM_USAGE_ANIMATION(...)` is a packet like `SM_DIALOG_WINDOW`) and `inventory.decreaseByObjectId(objId, count)` (an `item` effect whose
result is the helper's, which may write the inventory). `AbyssRankEnum` is one of the enum tables, so `registerOnKillRanked(AbyssRankEnum.X,
questId)` is traced. `qs.setQuestVar(v)` with a symbolic v that the path bounds to 0..63 (`int var = qs.getQuestVarById(0); if (var == 2)
qs.setQuestVar(var + 1)`) writes v to slot 0 and 0 to the other five slots (QuestVars.java:52-58); an unbounded one is still refused. The
committed documents are unchanged by all of this (`quest-trace check`); over the 972 files questgen transliterates the extractor raises on
none (33 before: 30 closures, 3 switch expressions) and writes cases for 826 (792 before), 19,727 cases (18,795 before); with switch
expressions, 827 files and 19,799 cases (`_30211`'s dialog hook is traced now, 22 cases, and `_18035`'s and `_28035`'s, 25 each; `_1917`'s is
refused further on, after `sendQuestNoneDialog`).

The owner's corrections of the Java code (lane C, 2026-10-05; docs/design/owner-decisions.md): `OWNER_CORRECTIONS` names a Java line,
its text and the corrected text the trace follows instead, a copy of questgen's table of the same name (the oracle does not import the
generator; a test requires the two to be equal). Such a document has a `corrections` member (line, Java text, traced text, why) and its
`javaSha256` is still the Java file's. The first rows: 11001 and 11008, whose level hook named the quest itself as its pre-quest.

The input model has limits a harness should know. The visible object is an Npc or nothing: `QuestEnv.getTargetId` (QuestEnv.java:94-96)
also returns the template id of a visible object that is not an Npc (a gatherable, a static object), which makes `instanceof Npc` false
with a non-zero target id, and no case has such a target. The assumed results of helpers are not checked against the helpers (but for `dead_assumption`'s two rules).

The first slice is `questtrace.extract.SLICE_TIER_A`: the 20 Poeta and Ishalgen handlers questgen transliterates in tier A with its P6-T
rules (`tools/gen/tests/test_questgen_p6t.py` keeps the two lists equal): 506 cases (515 before `dead_assumption`, below), every hook
traced. The ascension route slice
`SLICE_ROUTE` (P6-Q, 2026-09-29) adds the route's other generated handlers: 1100 and 2100 (their enter-world and level hooks refused:
`WorldMapType`), 1205 and 2132 (every hook refused, registration only) and the 12 dispatches of `ascension/` (17 cases each): 16 documents,
218 cases. `SLICE` is both. The C++ harness `game-server/tests/quest_handlers_golden` drives every document of `expected/quest` through the real
engine with the generated handler (docs/deviations/Q05.md). Its first run found 9 cases no state reproduces: paths that assume a helper
result the helper's Java cannot return. `dead_assumption` drops them since (`giveQuestItem` of a non-zero constant item and count
"-> false", AbstractQuestHandler.java:626-641; `QuestService.collectItemCheck(env, true)` "-> true" without a QuestState,
QuestService.java:557-561), so the first slice has 506 cases. Over all 1,035 handlers the extractor
writes 972 documents (the other 63 contain Java the shared parser refuses: a lambda, an anonymous class, `new ArrayList<>`, a switch
expression) with 18,861 cases (109 dead paths dropped; 422 high ends); 637 of them have every hook traced (16,904 cases; 26 of those with a
`register()` it does not follow). Measured 2026-09-29. The
most common refusals are crafting calls, the follow helpers, teleports and the packed `getQuestVars().getQuestVars()`.

```
python oracle.py quest-trace generate [--out DIR] [--only REL ...]   # writes expected/quest/<id>.json (the slice by default)
python oracle.py quest-trace check [--expected-dir DIR] [--only REL ...]   # exit 1 when a committed trace is missing, stale or extra
```

With `--only`, `check` compares the named handlers only (the other documents in the directory are not reported as extra).

Tests: `tests/test_quest_trace.py` (the input domains, a synthetic handler through every modelled construct and the refused ones, one
small handler per Java rule above with its values worked out by hand, the parser precedence the oracle relies on, the range ends, the
mutation standard - a changed page id, var write or guard in the Java changes the expected case -, hand-derived cases of 1000, 1001, 1005 and
2122, the high ends and free var slots of 1001 and 2001, the route slice and 1913, the dead paths, the committed traces against a
regeneration, `OTHER_NPCS` against `npc_templates.xml`, the command line, and the independence from the generator).
