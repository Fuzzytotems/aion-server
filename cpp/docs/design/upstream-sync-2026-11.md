# Upstream sync 2026-11: triage of beyond-aion 4.8 `51d7a19cb..68eccc7b1`

152 upstream commits (2026-09-13 to 2026-11-22), merged into the Java tree of `4.8` by PR #425. The C++ port follows the Java of `51d7a19cb` until the waves below are ported. Made from `git show --numstat` per commit and `cpp/tools/porting/chunks.py owner` for the C++ chunk that owns each changed Java file.

**Java** is changed Java lines (added + deleted, tests excluded), **Data** changed lines under `game-server/data` other than handlers. **Chunks** are the owning C++ chunks, most-changed first. The wave of a commit is its main subject; several commits also touch other waves' code.

## Summary

| Wave | Scope | Commits | Java lines | Data lines |
|---|---|---|---|---|
| 0 | No port: Java build, tooling, comments, formatting (verify only) | 11 | 6,101 | 3 |
| 1 | Network and server infrastructure (commons, login server, chat server, logging, startup, shutdown) | 22 | 6,558 | 0 |
| 2 | Exploit and validation fixes (the October hardening batch) | 35 | 3,611 | 3,263 |
| 3 | Combat, skills and effects | 23 | 2,671 | 238 |
| 4 | World: zones, movement, falling, flight, channels, weather, duels | 18 | 7,630 | 1,166 |
| 5 | Items, economy and player systems (item actions, decomposables, crafting, PvP EXP, pets, teleport confirmation) | 18 | 6,684 | 241,333 |
| 6 | Content: quests, instances, AI, GM and player commands | 25 | 7,199 | 3,215 |
| | **Total** | **152** | **40,454** | **249,218** |

## Foundation: data formats and database schema

25 commits change an XSD or the SQL schema. Their loader, template and DAO parts go first, before any wave: the C++ servers cannot read the new data or database without them, so the play kit keeps the old data and database until they land. Their behaviour parts stay in their own wave.

| Commit | Subject | XSD files | SQL files | Wave |
|---|---|---|---|---|
| `cc61fb239` | Item actions audit (#189) |  | 2 | 5 |
| `84d3f7559` | Removed custom shield and curing zone templates | 3 |  | 4 |
| `4617c9fd2` | ZoneName & zone templates cleanup | 1 |  | 4 |
| `ea7b1c32a` | Material skill fixes (#203) | 1 |  | 3 |
| `fc87a3f0c` | Implement missing OneTimeBoostSkillAttack parameters (#173) | 1 |  | 3 |
| `267ce6033` | Fixed skill grade being used as the skill level (#209) | 1 | 1 | 3 |
| `222076cb5` | Removed files that were forgotten in 6ee1e7b | 1 |  | 0 |
| `6c6eee658` | Signet effects: Retail Alignment (#197) | 2 |  | 3 |
| `d725054e1` | Implement retail PvP EXP system and acquisition limits. (#192) | 5 | 2 | 5 |
| `e5c133836` | Fixed limited crafting recipes and morphing (#195) |  | 2 | 5 |
| `a519ffc7b` | Clamped the character height and corrected the appearance fields |  | 2 | 4 |
| `46e8d00b9` | Fixed exploits of housing |  | 2 | 2 |
| `4426db07c` | Fixed exploits of pets | 1 |  | 2 |
| `332d54885` | Fixed exploits of windstreams and flight paths | 1 |  | 2 |
| `b55aaf32c` | Fixed chain skills expiring too early (#217) | 1 |  | 3 |
| `15eaf77b6` | Cleanup | 1 |  | 0 |
| `1af948baf` | Fix EmpyreanCrucibleInstance (#5) | 2 |  | 6 |
| `82a5e0cf9` | Aligned falling with retail (#222) | 1 |  | 4 |
| `d254ea172` | Reworked decomposable items (#200) | 1 |  | 5 |
| `822ec1fc2` | Fix the move packets other players get (#236) | 1 |  | 4 |
| `0199b4b03` | Channel change like retail (#235) | 1 |  | 4 |
| `374336b3b` | Port weather to the retail schedule (#233) | 2 |  | 4 |
| `cf4b34d9c` | Apply drains when the target dies before the hit (#242) | 1 |  | 3 |
| `da540feec` | Creature bound fixes (#241) | 1 |  | 5 |
| `f16e03ad3` | Retail teleport confirmation, pet rework and system message parameter order (#248) |  | 2 | 5 |

## Foundation progress

### Static data (lane A, 2026-11-25)

The C++ game server binds and indexes the static data of 68eccc7b1 with bindings generated from the Java of 68eccc7b1. What each commit
needed on the data side, and where its behaviour was kept back (the deviation docs list every interim site with the wave that replaces it):

| Commit | Ported for the static data | Kept back (interim or deferred) |
|---|---|---|
| `20096e5d2` (jakarta) | tools: `gen/javasrc.py`, `gen/fieldmap.py` and `xmlgen/jaxb.py` accept `jakarta.xml.bind` beside `javax.xml.bind` | |
| `222076cb5` | `configs/ingameshop` and `model/templates/ingameshop` removed (xmlgen root, allowlist entry, ConfigRootsRealDataTest case, chunk globs) | |
| `84d3f7559` | ShieldData, CuringObjectsData and their templates removed; ShieldService, FortressLocation (no sphere shield observer), GameServer (no CuringZoneService), CuringObject/CuringZoneService/ShieldObserver deleted | GeoDataConfig.Mode (wave 4): the config keeps GEO_ENABLE, GEO_MATERIALS_ENABLE, GEO_SHIELDS_ENABLE |
| `4617c9fd2` | ZoneTemplate binds `name`, `hasZoneAttribute`; WorldMapTemplate's EnumSet attribute list (xmlgen: `java.util.EnumSet` is a Set, `EnumSet.noneOf` an empty initializer) | ZoneName stays (C++-only `ZoneTemplate::getZoneName`, `ItemUseLimits::getUseAreaZoneName`), WorldMapTemplate's flag accessors stay as C++-only readers of the attribute set (P4-07a) |
| `2b097879f` | ZoneData: zone names, duplicate check, size, `validateZoneName`, `isValidZoneName` | ZoneInfo/Area.create, WorldZoneTemplate and DUMMY (C++-only `isWholeMapZone`), saveData (P4-07a, P4-09) |
| `ea7b1c32a`, `6c6eee658`, `fc87a3f0c`, `cf4b34d9c`, `b55aaf32c`, `267ce6033` | generated effect/condition members (prob/count/dmg/acc deltas, signet carve_prob, chance_delta, SkillLearnTemplate skillLevel); MaterialTemplate.getSkills, SubEffect.getChance(level) | applySubEffect uses only the new chance (P5-03); AbstractOverTimeEffect keeps duration2 + 1000 through a C++-only accessor (P5-02b); chain skill rework |
| `d725054e1` | PvpExpTable, PvpExpModTable, PartyExpModTable, MenteeExpLimitTable (holders, DataManager fields, "Loaded" lines) | PvpExpLimitService (claimed by P5-08, not ported) |
| `d254ea172`, `bad68ed81`, `90a740e2d`, `08f2df617` | DecomposableItemsData (overrides, overrideCount), DecomposableItem/Set/DecomposedItem with their rolls and load checks; //reload binds the folder | DecomposeAction, CM_SELECT_DECOMPOSABLE, the two SHOW packets and .decompose use the new templates on the 51d7a19cb packets and animations (P5-07, P4-16, P5-16, C1) |
| `374336b3b` | weather_maps root, WeatherTable/WeatherEntry/WeatherPhase/WeatherState with the load checks | WeatherService runs no weather (clear sky, //weather set only), DayTime kept by hand (P4-10, P4-11b, P4-17, C1) |
| `332d54885` | FlyPathEntry windstream points and their queries, WindstreamPoint/State | FlightPath boost/progress checks (wave 2) |
| `0199b4b03`, `82a5e0cf9` | WorldMapTemplate `cool_time`, `twin_count` default; death_level gone (the ZoneLevelService death level check with it) | beginner channels (C++-only getBeginnerTwinCount = 0), channel change rework, falling |
| `da540feec` | BoundRadius defaults, DEFAULT/ZERO, NpcTemplate height gone (SM_NPC_INFO sends the cylinder upper) | |
| `4426db07c` | PetData.validatePetIds (pets depend on ItemData) | |
| `f7230c5fa` | ItemData learnable emotions (EmotionLearnAction lost its static set) | ItemData motion types (AnimationAddAction rework) |
| `15eaf77b6`, `784e0764b`, `cabd580f3`, `e5c133836` | data-only classes (QuestKill, PetBuff, Points), ShoutList/ShoutGroup/CollectItems accessors, HouseData/RecipeData additions | |
| `546f96bd2` | StaticData without the after-unmarshal tasks | StaticDataLoader/XmlMerger speed-ups (wave 1) |

Data-only commits (`14d9b8acb`, `cb86e78ea`, `90a740e2d`, `08f2df617`, the npc/item data of `da540feec`, ...) need no code: they load through the
ported bindings.

#### Frozen enums

`xmlgen.toml [hand_written_enums]` keeps ten core enums (not bound by JAXB) whose constants upstream changed at their 51d7a19cb form, as hand-written
copies of the old generated headers, so their companions and switch statements keep compiling and nothing changes ordinals before the wave that
ports the commit: ObserverType, ActionState, CreatureState (cc61fb239), TaskId (a17e66f10, cc61fb239), PetAction (f16e03ad3), CustomPlayerState
(2ee52b1d3, 9b15b6df8), StageType (1af948baf), ZoneType (206f9b5db), ItemPacketService.ItemUpdateType (e5c133836), WorldMapType (374336b3b,
f16e03ad3). Three removed Java enums are kept the same way without an xmlgen entry: DayTime (374336b3b), WeaponType (cc61fb239) and
PlayableMoveController.MovementModifierDirection (moved to the top level by e7a16c553). The wave that ports a commit removes its entries and
the hand-written copies. JAXB-bound enums cannot be frozen: CreatureSeeState/CreatureVisualState got their new ids, AbnormalState
CANCEL_ITEM_USE, PetFunctionType's swapped ordinals (BUFF id 5) and FlyPathType's new names apply now.

#### Registry scan

aion_gs_regscan has `--java-ahead` (cpp/game-server/tools/regscan/java_ahead.txt): Java cross-check errors tolerated while the Java tree is ahead,
each with its commit and wave; an entry that matches no error fails the scan. Seven entries: FollowingNpcAI's constant @AIName (205076446),
@ZoneHandlerArea of _1012SensoryArea and PvPAreaZone (4617c9fd2), CM_POSITION_SELF (2ee52b1d3) and CM_SHOW_BLOCKLIST (e5c133836) removed from the
Java packet table.

#### Not done by the static data step

- **Config classes.** The Java config classes and properties of 68eccc7b1 (SecurityConfig multi-client keys, GeoDataConfig.Mode,
  NetworkConfig, ShutdownConfig, CustomConfig, the removed FallDamageConfig, PeriodicSaveConfig without player pets, ...) are not ported:
  aion_gs_configs_tests fails 7 cases (JavaAnnotationsTest, ConfigLoadTest: unknown and missing keys). The C++ servers start with the new
  config files (unknown keys are ignored with a warning, removed keys keep their C++ defaults; `gameserver.geodata.mode` is ignored, so
  geo data stays on through GEO_ENABLE's default). Each config change goes with its wave.
- **tools/gen.** fieldmap's outputs are regenerated (test_committed_outputs_are_current passes); its design reference points and
  cycles.toml lack the classes and cycle edges of 68eccc7b1 (3 of 7 test_fieldmap_real cases fail: removed reference classes such as
  Polygon2D and ShieldObserver, 18 new unresolved cycle edges). The forward headers (skeleton.py --fwd), the opcode tables (opcodes.py)
  and the system message table (sysmsg.py) still follow 51d7a19cb: regenerating them changes behaviour code of every wave.
- **Registry scan tool tests.** aion_gs_regscan builds the tables with `--java-ahead`; its own tests over the real Java tree
  (RealJavaTreeTest.JavaHandlerKeysAndCounts, aion_gs_regscan.cli.java_tree) still expect the 51d7a19cb counts and no Java-side errors.
- **Concurrency lint.** gs.lint.concurrency compares the C++ members with the regenerated fieldmap.json, which now has the fields of all
  waves (rankLimitExpireTime, MaterialSkillUsage, OneTimeBoostSkillAttack, ...): it fails until the waves port them.
- **Quests.** QuestTemplatesRealDataTest finds 2 npcs whose onTalkEvent registration differs from the Java handlers of 68eccc7b1 (wave 6).
- **Database.** DAO and login slice tests against the test schemas belong to lane B's database half.
- **Gate oracles.** tools/oracle's gate oracles (m5b ... m5j, quest traces) read Java rules and data of 51d7a19cb and refuse the changed
  Java; their tests fail until the waves update them. The static data oracle (V1-V4) passes.

## Proposed order

1. **Foundation** (the 25 XSD/SQL commits' loader, template and DAO parts, plus the data-only commits): the C++ servers load the new
   static data and run on the migrated schema. Gate: the real-data tests (`ConfigRootsRealDataTest` and the loaders' tests) on the new
   data, and `chunks.py check` with the new Java files claimed.
2. **Wave 0** needs no port, only a check per commit. `20096e5d2` (5,300 lines) is verified: only `javax` → `jakarta` imports change.
   `15eaf77b6` (Cleanup) removes the PanesterraTeam method our P5-12b correction fixed; compare before closing it.
3. **Wave 1** (network and servers) and **wave 2** (exploit fixes): independent of the game systems, and wave 2 is the most valuable for
   a server other people connect to.
4. **Waves 3 and 4** (combat, world): in parallel lanes; they share few chunks.
5. **Wave 5** (items, economy, systems), then **wave 6** (content), which builds on all of the above.
6. **Cut over**: back up and migrate the play kit's database with `update.sql`, move the main checkout's data to the new 4.8, install
   a new kit, and record `68eccc7b1` as the Java baseline. The C++ branches take the Java merge (PR #425) with the foundation step.

Each wave's commits are ported as usual: one PR per commit or small group, into both C++ and 4.8, with tests, citing the upstream commit.
Overlaps are compared first; where upstream fixed a bug we corrected, the port takes upstream's fix and the correction's test stays.

## Overlaps with our corrections and C++-dev changes

Compare each before porting: upstream may fix the same bug another way, or change code the owner customised on C++-dev.

| Commit | Subject | Overlap |
|---|---|---|
| `ab48e50a8` | Charge the kinah for item purification first | same bug as our P5-07 correction "a purification takes its kinah" (2026-10-05): compare, keep one |
| `cc61fb239` | Item actions audit (#189) | changes TamperingAction and PolishAction, where our P5-07 correction "TamperingAction cannot act without a target" lives |
| `65eebace9` | Fixed item exploits and aligned enchanting with retail | changes CosmeticItemAction, where our P5-07 corrections live (coupon deletion, preset skin colour) |
| `9b15b6df8` | Retail-like item and skill cooldown GM commands (#225) | changes TamperingAction and PolishAction (our P5-07 correction) |
| `15eaf77b6` | Cleanup | changes PanesterraTeam (our P5-12b correction of removeTeamMember) |
| `53363eea9` | Chat commands fixes and refactoring (#240) | reworks the chat-command framework and //movetoobj (our two C1 corrections; C++-dev //rift all and //damagepercent) |
| `a7092eb97` | Revalidate legion invitations | changes Legion.java (near our C1 //legion kick correction) |
| `cd1cfbc9f` | Balanced Eternal Challenge based on recent logs | Eternal Challenge balance: C++-dev has the owner's DPS report and boss-engage changes (P5-13) |
| `38664c074` | #BA2843 Fixed drop registration for custom instance boss | custom instance boss drops: C++-dev changed CustomInstanceBossAI (P5-13) |
| `ce54b7931` | Fixed CustomInstanceBossAI spawn exception | CustomInstanceBossAI: C++-dev changed it (P5-13) |
| `9089d1a81` | Fixed skill exception from CustomInstanceBossAI | CustomInstanceBossAI: C++-dev changed it (P5-13) |
| `3194c558d` | Fix where players revive after a lethal fall (#247) | revive points: C++-dev changed the custom PvP map respawn (P5-13) |
| `7213fb579` | Fix //access level leaving a GM without a way back (#231) | //access: C++-dev has the C++-only commands (z19) |

## Wave 0: No port: Java build, tooling, comments, formatting (verify only)

11 commits, 6,101 Java lines, 3 data lines.

| Commit | Date | Subject | Java | Data | Chunks | Notes |
|---|---|---|---|---|---|---|
| `2cd926056` | 2026-09-18 | Ensure deterministic file loading order | 4 | 0 | P4-05 |  |
| `222076cb5` | 2026-10-03 | Removed files that were forgotten in 6ee1e7b | 109 | 0 | P4-07b, P4-09 | XSD |
| `20096e5d2` | 2026-10-03 | Updated dependencies and build plugins to their latest versions | 5,302 | 0 | P4-07b, P4-09, P5-04, P4-07a |  |
| `ca2155e6d` | 2026-10-04 | Replaced the remaining LinkedLists with ArrayDeques | 51 | 0 | P4-11a, P4-11b, P5-08 |  |
| `874a64f28` | 2026-10-08 | Added jakarta to import layout config (required since 20096e5) | 0 | 0 |  |  |
| `7181a3864` | 2026-10-13 | ChatUtil.*Name documentation | 18 | 0 | P4-05, P4-12 |  |
| `15eaf77b6` | 2026-10-23 | Cleanup | 547 | 3 | P4-07b, P4-09, P4-07a, P5-06c | XSD, overlap |
| `80548e389` | 2026-11-07 | Removed unused Item field | 18 | 0 | P4-11a, P5-01 |  |
| `f856882b3` | 2026-11-07 | Update syntax highlighting for stack traces in Discord logs | 0 | 0 |  |  |
| `b7bc32976` | 2026-11-07 | Replaced DecimalFormat with a thread-safe formatting method | 17 | 0 | P4-05 |  |
| `2deb96507` | 2026-11-12 | Removed stale or redundant comments in SM_PLAYER_INFO (#244) | 35 | 0 | P4-17 |  |

## Wave 1: Network and server infrastructure (commons, login server, chat server, logging, startup, shutdown)

22 commits, 6,558 Java lines, 0 data lines.

| Commit | Date | Subject | Java | Data | Chunks | Notes |
|---|---|---|---|---|---|---|
| `d22a9af5b` | 2026-09-21 | Shutdown improvements | 187 | 0 | P5-00, P5-14, P4-15, C2 |  |
| `e1a399e62` | 2026-10-03 | Client packets that are larger than the read buffer now disconnect the client | 85 | 0 |  |  |
| `76b2f2bca` | 2026-10-03 | Packet processor keeps a queue per connection and limits its size | 216 | 0 | P4-15, P4-01 |  |
| `d9800d686` | 2026-10-03 | Clients that stop receiving data get disconnected instead of growing their send queue forever | 92 | 0 | P4-01, P4-15 |  |
| `31ad0571f` | 2026-10-03 | Limited the time a client has to authenticate and restored flood protector default config to true (active) | 121 | 0 | P4-15, P4-01 |  |
| `459b35094` | 2026-10-04 | Migrated chat server from Netty 3 to 4.2 | 1,536 | 0 |  |  |
| `01aadf926` | 2026-10-04 | Game server authentication on chat and login server rejects strangers reliably | 49 | 0 |  |  |
| `36bdccb4d` | 2026-10-04 | Log files roll over at 100 MB and keep five old files | 0 | 0 |  |  |
| `7840be30d` | 2026-10-04 | Logging improvements | 1,823 | 0 | P4-09, P4-14, P4-15, P4-02b |  |
| `81e409c5e` | 2026-10-06 | Log too short client packets once per opcode and fixed swapped packet flood filter modes 1 and 2 (#213) | 6 | 0 | P4-15 |  |
| `bce89b2fc` | 2026-10-18 | Moved the login server to Netty and matched the login packets to the 4.8 client (#218) | 815 | 0 | P5-00 |  |
| `a485569df` | 2026-10-18 | Multi-client limit  (#216) | 117 | 0 | P5-08, P4-01, P4-17, P5-00 |  |
| `92da2dd62` | 2026-10-22 | Add invisible continuation mark to split message parts | 9 | 0 | P4-05 |  |
| `55d0af937` | 2026-10-31 | Rate limit network packets that allow spamming DB queries | 1 | 0 | P4-15 |  |
| `9d8eeb2f2` | 2026-10-31 | Stopped client text from forging log lines | 43 | 0 | P4-05, P5-15 |  |
| `4ca531afc` | 2026-10-31 | Stopped leaking failed login and character count state | 20 | 0 |  |  |
| `b0317f967` | 2026-10-31 | Added retail chat channel validations | 55 | 0 |  |  |
| `b028752d0` | 2026-11-10 | Logging fixes & improvements | 247 | 0 | P5-14 |  |
| `8912ec714` | 2026-11-10 | Startup fixes & improvements | 199 | 0 | P5-11, P4-10, P5-14 |  |
| `17093650f` | 2026-11-14 | Fixed chat channel leak (#245) | 109 | 0 |  |  |
| `546f96bd2` | 2026-11-18 | Speed up game server startup (#252) | 824 | 0 | T2, P4-05, P5-14, P4-09 |  |
| `620fc69b0` | 2026-11-18 | Fixed NPE from merged startup changes (546f96b) | 4 | 0 | P4-09 |  |

## Wave 2: Exploit and validation fixes (the October hardening batch)

35 commits, 3,611 Java lines, 3,263 data lines.

| Commit | Date | Subject | Java | Data | Chunks | Notes |
|---|---|---|---|---|---|---|
| `06ab87c2f` | 2026-10-12 | Refuse crafting when the client sends none of the recipe's component sets | 76 | 0 | P5-09c |  |
| `65eebace9` | 2026-10-12 | Fixed item exploits and aligned enchanting with retail | 283 | 0 | P5-16, P5-07, P5-15, P4-12 | overlap |
| `7e501bb9f` | 2026-10-12 | Megaphone fixes | 29 | 0 | P5-07, P5-16 |  |
| `07959a2ef` | 2026-10-13 | Fixed traded and repurchased items losing their attributes | 37 | 0 | P5-07, P4-11a, P5-09b, P5-09c |  |
| `e2b0c35e2` | 2026-10-13 | Fixed NPC shop exploits | 276 | 0 | P5-15, P5-09b, P4-13 |  |
| `f0c1a3695` | 2026-10-13 | Fixed private store purchases transferring before validation | 108 | 0 | P5-09b, P5-15 |  |
| `ea257e5e9` | 2026-10-13 | Fixed exchanges taking items from one side when the other can't pay | 53 | 0 | P5-09b, P4-16 |  |
| `a17e66f10` | 2026-10-15 | Fixed exploits of mail, the broker and storages | 340 | 0 | P5-07, P5-16, P5-05, P4-12 |  |
| `54880a691` | 2026-10-15 | Fixed exploits of looting | 268 | 0 | P5-16, P5-10a, P4-16, P5-15 |  |
| `ba383f739` | 2026-10-15 | Fixed exploits of legion emblems and brigade ranks | 73 | 0 | P5-10f, P5-11, P5-16 |  |
| `46e8d00b9` | 2026-10-15 | Fixed exploits of housing | 496 | 0 | P5-15, P5-11, P4-16, P4-05 | SQL |
| `4426db07c` | 2026-10-15 | Fixed exploits of pets | 96 | 2,249 | P5-16, P5-08, P4-17, P4-07a | XSD |
| `332d54885` | 2026-10-15 | Fixed exploits of windstreams and flight paths | 496 | 1,014 | P4-07b, P5-16, P4-11b, P4-12 | XSD |
| `6dd3aa507` | 2026-10-15 | Fixed client broadcasts reaching the wrong players | 191 | 0 | P5-15, P4-05, P4-16 |  |
| `33aa13cb6` | 2026-10-15 | Fixed static doors and chairs being used from anywhere | 51 | 0 | P5-14, P4-04, P5-15, P4-10 |  |
| `638ed3dd4` | 2026-10-18 | Fixed TradeService.validateBuyItems ignored the legion restriction and refactored trade packets | 232 | 0 | P4-17, P5-09b, P4-07b, P5-08 |  |
| `68d6df38d` | 2026-10-31 | Require learned skills for client casts | 8 | 0 | P5-02a |  |
| `c9c8b771a` | 2026-10-31 | Check permissions for team commands | 57 | 0 | P5-10a |  |
| `8550d9a62` | 2026-10-31 | Fixed private store stacks sold twice | 100 | 0 | P5-09b |  |
| `c6b9cc0e4` | 2026-10-31 | Checked loot team membership for rolls | 2 | 0 | P5-09a |  |
| `9f87bb2a8` | 2026-10-31 | Refused channel changes outside open worlds | 11 | 0 | P5-08 |  |
| `dc1a3a3cd` | 2026-10-31 | Rejected overlong notes and shop messages | 16 | 0 | P4-12, P5-16, P5-09b |  |
| `ed247770b` | 2026-10-31 | Refused skill activation types the client cannot cast | 12 | 0 | P5-02a, P5-15 |  |
| `ab48e50a8` | 2026-10-31 | Charge the kinah for item purification first | 9 | 0 | P5-07 | overlap |
| `567d4af5d` | 2026-10-31 | Check the talk range for NPC services | 115 | 0 | P5-15, P4-12, P5-16 |  |
| `dee487c3e` | 2026-10-31 | Synchronized the exchange confirmation | 4 | 0 | P5-09b |  |
| `4c2004693` | 2026-10-31 | Guarded CM_TUNE against a second identify | 3 | 0 | P5-07 |  |
| `0432aff62` | 2026-10-31 | Check the special cube when splitting items | 7 | 0 | P5-07 |  |
| `97b2a4d33` | 2026-10-31 | Guarded the legion packets against bogus input | 28 | 0 | P5-16 |  |
| `2d34af45a` | 2026-10-31 | Limit the group finder message length | 10 | 0 | P5-15 |  |
| `a39df86df` | 2026-10-31 | Revalidate stale legion invitations | 3 | 0 | P5-11 |  |
| `90dfeb604` | 2026-11-01 | Reject invalid channels without throwing | 7 | 0 | P4-10, P5-08 |  |
| `a7092eb97` | 2026-11-01 | Revalidate legion invitations | 29 | 0 | P5-11, P5-10f | overlap |
| `1a29700f6` | 2026-11-03 | Added trade fail system messages | 19 | 0 | P5-09c, P5-09b |  |
| `e5ea7d94e` | 2026-11-16 | Fixed house script saves and bounded their size (#251) | 66 | 0 | P4-12, P4-05, P5-15 |  |

## Wave 3: Combat, skills and effects

23 commits, 2,671 Java lines, 238 data lines.

| Commit | Date | Subject | Java | Data | Chunks | Notes |
|---|---|---|---|---|---|---|
| `c22ed33b8` | 2026-09-13 | Check effect resist rate first for subeffects (#198) | 6 | 0 | P5-03 |  |
| `626e1c55e` | 2026-09-13 | Fixed off-by-one error in `Effect.setMagicalCriticals()` | 3 | 0 | P5-02b |  |
| `17a718595` | 2026-09-13 | Fix autoattack rate check (#199) | 6 | 0 | P4-11b |  |
| `840a6a8d4` | 2026-09-15 | Fixed HP bugs | 104 | 0 | P5-01, P4-12, P4-10 |  |
| `924d4ed82` | 2026-09-16 | SM_INVINCIBLE_TIME: implement retail packet (#182) | 31 | 0 | P4-16, P4-11b, P4-15 |  |
| `b25a2664c` | 2026-09-17 | Fix animation not ending after multireturn action (#201) | 44 | 0 | P5-07 |  |
| `d2dadc175` | 2026-09-18 | NpcSkillList optimization | 306 | 0 | P5-02b, C1, P5-05, P4-09 |  |
| `18a87505e` | 2026-09-20 | Fixed siege weapon follow task surviving release (#202) | 25 | 0 | P4-11b, P5-05, P5-08 |  |
| `223b56a0d` | 2026-09-22 | Fix skill cast conditions (#180) | 134 | 0 | P5-02a, P4-11a, P5-01, P4-12 |  |
| `b87c6c656` | 2026-09-22 | Fixed delayed skills ignoring dodge and resist (#167) | 10 | 0 | P5-02a, P5-03 |  |
| `558677569` | 2026-09-24 | Fixed skills affecting everyone skipping duel partners (#204) | 10 | 0 | P5-02a |  |
| `ea7b1c32a` | 2026-09-27 | Material skill fixes (#203) | 315 | 180 | P4-11b, P5-01, P4-10, P4-07a | XSD |
| `01a5602f1` | 2026-09-28 | Fixed the tick count of periodic effects (#172) | 77 | 0 | P5-02b, P5-03 |  |
| `fc87a3f0c` | 2026-09-28 | Implement missing OneTimeBoostSkillAttack parameters (#173) | 259 | 19 | P4-11b, P5-04, P5-01, P5-03 | XSD |
| `a31c1dfcb` | 2026-09-29 | Fixed OneTimeBoostSkillAttack ending before all targets of an attack got the bonus (#208) | 7 | 0 | P4-11b |  |
| `267ce6033` | 2026-09-30 | Fixed skill grade being used as the skill level (#209) | 19 | 1 | P5-02b, P5-02a, C2, P4-15 | XSD, SQL |
| `e7a16c553` | 2026-10-04 | Movement modifiers (#171) | 314 | 0 | P4-11b, P5-01 |  |
| `3d86e120b` | 2026-10-04 | Fixed ConcurrentModificationException from `updateArmorMasteryStats()` (introduced by 7010247) | 14 | 0 | P5-01 |  |
| `6c6eee658` | 2026-10-05 | Signet effects: Retail Alignment (#197) | 90 | 24 | P5-03, P5-04, P5-02b, P5-02a | XSD |
| `b55aaf32c` | 2026-10-18 | Fixed chain skills expiring too early (#217) | 238 | 2 | P5-02a, P4-12 | XSD |
| `b0a597458` | 2026-10-19 | Unified invisibility and spawn protection checks (#205) | 222 | 0 | P4-11a, P5-08, P5-02a, P5-01 |  |
| `cf4b34d9c` | 2026-11-11 | Apply drains when the target dies before the hit (#242) | 54 | 12 | P5-04, P5-02b, P5-03 | XSD |
| `38ab3e0ba` | 2026-11-11 | Fix doomed state handling and add option to turn it off (#243) | 383 | 0 | P5-01, P5-02b, P4-11b, P5-13 |  |

## Wave 4: World: zones, movement, falling, flight, channels, weather, duels

18 commits, 7,630 Java lines, 1,166 data lines.

| Commit | Date | Subject | Java | Data | Chunks | Notes |
|---|---|---|---|---|---|---|
| `47ab0f3b7` | 2026-09-13 | Fixed `ChatUtil.getPosition()` | 4 | 0 | P4-05 |  |
| `206f9b5db` | 2026-09-18 | Neutral Zones: Server-Side PvP Restriction (#193) | 208 | 0 | P4-10, P4-11a, P4-12, P5-05 |  |
| `8c231daa1` | 2026-09-19 | ZoneHandler optimization | 74 | 0 | P4-10, P5-12a |  |
| `84d3f7559` | 2026-09-23 | Removed custom shield and curing zone templates | 570 | 91 | P4-07b, P5-14, P4-09, P4-11b | XSD |
| `4617c9fd2` | 2026-09-26 | ZoneName & zone templates cleanup | 1,352 | 5 | P4-10, P4-05, Q14, P4-09 | XSD |
| `2b097879f` | 2026-09-27 | Zone attribute check improvements (#206) | 1,448 | 0 | P4-05, P4-10, P4-07a, P5-07 |  |
| `183723078` | 2026-10-13 | Player height corrections | 629 | 0 | P4-12, P4-10, P4-14, P4-17 |  |
| `a519ffc7b` | 2026-10-13 | Clamped the character height and corrected the appearance fields | 76 | 0 | P5-00, P4-12, P4-17, P4-14 | SQL |
| `30612bc3d` | 2026-10-24 | Fixed players getting stuck at the end of flight paths (#223) | 253 | 82 | Q06, Q09, Q05, P5-06a |  |
| `82a5e0cf9` | 2026-10-26 | Aligned falling with retail (#222) | 190 | 323 | P4-11b, P4-01, P4-10, P5-08 | XSD |
| `9b171d237` | 2026-11-01 | Fixed fall damage from elevators (introduced by 82a5e0c) | 8 | 0 | P5-00 |  |
| `0868abf0f` | 2026-11-03 | Improved invalid MapRegion handling | 56 | 0 | P4-10 |  |
| `822ec1fc2` | 2026-11-04 | Fix the move packets other players get (#236) | 178 | 100 | P4-17, P5-00, P4-11b, P5-16 | XSD |
| `604957f50` | 2026-11-05 | Fix duel timer and teleport handling (#232) | 166 | 0 | P4-16, P5-08, P4-17, P4-05 |  |
| `2ee52b1d3` | 2026-11-07 | End free flight when gliding out of a flight zone (#230) | 558 | 0 | P4-11b, P4-16, P5-16, C2 |  |
| `0199b4b03` | 2026-11-07 | Channel change like retail (#235) | 422 | 204 | P4-10, P4-17, P5-08, P4-16 | XSD |
| `374336b3b` | 2026-11-07 | Port weather to the retail schedule (#233) | 1,360 | 361 | P4-10, P4-07a, C2 | XSD |
| `3194c558d` | 2026-11-14 | Fix where players revive after a lethal fall (#247) | 78 | 0 | P5-08, P4-11b, P5-13, I3 | overlap |

## Wave 5: Items, economy and player systems (item actions, decomposables, crafting, PvP EXP, pets, teleport confirmation)

18 commits, 6,684 Java lines, 241,333 data lines.

| Commit | Date | Subject | Java | Data | Chunks | Notes |
|---|---|---|---|---|---|---|
| `cc61fb239` | 2026-09-13 | Item actions audit (#189) | 1,375 | 0 | P5-07, P4-07a, P4-12, P4-11b | SQL, overlap |
| `eb35dfa5d` | 2026-10-03 | Made the macro limit configurable (#210) | 68 | 0 | P5-00, P5-16, P4-12, P4-14 |  |
| `f7230c5fa` | 2026-10-09 | Motion/emotion learning & emotion expiry fixes (#207) | 476 | 0 | P4-12, P4-17, P5-07, P4-09 |  |
| `d725054e1` | 2026-10-10 | Implement retail PvP EXP system and acquisition limits. (#192) | 559 | 391 | P4-09, P5-01, P5-08 | XSD, SQL |
| `e5c133836` | 2026-10-12 | Fixed limited crafting recipes and morphing (#195) | 391 | 0 | P4-12, P5-09c, P5-16, P4-14 | SQL |
| `784e0764b` | 2026-10-12 | Fixed item templates missing their item kind | 38 | 34 | P4-13 |  |
| `83b9e3322` | 2026-10-17 | Fixed the PvP price of teleports to vulnerable fortresses | 12 | 0 | P5-08 |  |
| `6f5feaceb` | 2026-10-18 | Aligned quitting the game with retail (#215) | 149 | 0 | P5-00, P4-12, P4-17, P5-16 |  |
| `d254ea172` | 2026-10-30 | Reworked decomposable items (#200) | 1,149 | 101,876 | P4-07a, P5-07, P4-17, P5-16 | XSD |
| `9b15b6df8` | 2026-10-30 | Retail-like item and skill cooldown GM commands (#225) | 168 | 0 | C2, P5-07, P4-17, P4-12 | overlap |
| `14d9b8acb` | 2026-11-03 | Restored custom entry scroll bundle that was reset in d254ea1 and added two more scrolls to it | 0 | 13 |  |  |
| `8e97687fd` | 2026-11-07 | Fix identify failing after the first item use (#239) | 2 | 0 | P5-07 |  |
| `da540feec` | 2026-11-11 | Creature bound fixes (#241) | 50 | 136,672 | P4-07b, P4-07a, P4-17, P4-12 | XSD |
| `cb86e78ea` | 2026-11-12 | Restored selectable plumes (see d254ea1) | 0 | 11 |  |  |
| `bad68ed81` | 2026-11-12 | More decomposable fixes and restored fast `.decompose` | 33 | 10 | P4-09, C2, C1 |  |
| `90a740e2d` | 2026-11-14 | Fix decomposable sets that often rolled nothing (#246) | 0 | 2,149 |  |  |
| `08f2df617` | 2026-11-14 | Fix manastone distribution | 0 | 177 |  |  |
| `f16e03ad3` | 2026-11-22 | Retail teleport confirmation, pet rework and system message parameter order (#248) | 2,214 | 0 | P5-08, P4-17, P4-06, P4-12 | SQL |

## Wave 6: Content: quests, instances, AI, GM and player commands

25 commits, 7,199 Java lines, 3,215 data lines.

| Commit | Date | Subject | Java | Data | Chunks | Notes |
|---|---|---|---|---|---|---|
| `ce54b7931` | 2026-09-17 | Fixed CustomInstanceBossAI spawn exception | 4 | 0 | P5-01 | overlap |
| `e4f2f79be` | 2026-09-22 | Simplified `.decompose` and added missing canUseItem check | 65 | 0 | C1 |  |
| `44c8d4387` | 2026-09-30 | Fixed ring orientations for quest 1354 and shortcut in quest handler | 24 | 42 | Q05 |  |
| `f3998f526` | 2026-10-01 | Fixed quest 2209 | 1 | 0 | Q10 |  |
| `a8d182b67` | 2026-10-09 | Fixed #BA3780 | 2 | 0 | P5-08 |  |
| `cabd580f3` | 2026-10-14 | Fixed false log entries about missing quest spawns | 19 | 0 | P4-09, P5-06c, P5-06a |  |
| `616cb3c04` | 2026-10-17 | Fixed quest 23800 | 0 | 14 |  |  |
| `93d5fa15b` | 2026-10-18 | Fixed quest 2002 "Where's Rae?" (#219) | 89 | 2 | Q09 |  |
| `4990b3526` | 2026-10-18 | Added Inggison/Gelkmaros fortress morph recipe merchants to custom spawns | 0 | 36 |  |  |
| `db5ae0453` | 2026-10-24 | Quest fixes | 223 | 4 | Q13, Q01, Q09, Q10 |  |
| `1af948baf` | 2026-10-24 | Fix EmpyreanCrucibleInstance (#5) | 4,611 | 416 | I2, P5-04, P5-13, P5-02a | XSD |
| `c607b2094` | 2026-10-26 | Play the beam teleport when entering the Ascension instances (#226) | 4 | 0 | Q06 |  |
| `96a005a09` | 2026-10-27 | Fix yet another missing npc ID for quest 2002 (#227) | 2 | 0 | Q09 |  |
| `433f42885` | 2026-10-28 | Fixed missing talk events for quest start NPCs (#229) | 6 | 0 | Q09, Q08, Q01 |  |
| `9089d1a81` | 2026-10-30 | Fixed skill exception from CustomInstanceBossAI | 2 | 0 | I1 | overlap |
| `7213fb579` | 2026-10-31 | Fix //access level leaving a GM without a way back (#231) | 45 | 0 | P5-14, C1 | overlap |
| `205076446` | 2026-11-01 | Fix npc follow quests (#237) | 21 | 0 | P5-06a, P4-10, P5-05 |  |
| `cd1cfbc9f` | 2026-11-03 | Balanced Eternal Challenge based on recent logs | 7 | 0 | P5-13 | overlap |
| `53363eea9` | 2026-11-11 | Chat commands fixes and refactoring (#240) | 1,439 | 0 | C1, C2, P5-14, P4-11a | overlap |
| `38664c074` | 2026-11-14 | #BA2843 Fixed drop registration for custom instance boss | 16 | 0 | P5-13 | overlap |
| `934d568a9` | 2026-11-14 | #BA2040 Fixed revive in Illuminary Obelisk | 2 | 0 | I2 |  |
| `5f9f1cba6` | 2026-11-14 | #BA3734 Ported retail skill patterns of siege fortress bosses | 212 | 2,190 | A1 |  |
| `fb5adb72f` | 2026-11-14 | #BA2143 Fixed Kuhara the Volatile (Rentus Base) | 33 | 3 | I5 |  |
| `bfb358517` | 2026-11-14 | #BA371 Fixed Macunbello handling | 316 | 508 | I4 |  |
| `68eccc7b1` | 2026-11-22 | Track the ascension morph of quest instances (#256) | 56 | 0 | Q06, P4-12, P4-16, Q09 |  |
