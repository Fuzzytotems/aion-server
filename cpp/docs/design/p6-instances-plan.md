# Phase 6: the instance handlers (chunks I1-I6)

Lane A, 2026-10-14, branch `lane-a/p6-instances` (stacked on `lane-a/leftovers-1`). The integrator's assignment of 2026-10-14: no lane holds
I1-I6; lane C ports the quest handlers, lane B M5i's siege, vortex and Panesterra instances (none of them is in I1-I6, checked below).

## 1. The measure (census of 2026-10-14 over `lane-a/leftovers-1`, `tools/porting/census.py`)

Every Java body of I1-I6 is open: no instance handler and no instance AI of these chunks has a C++ file yet (the custom-instance AIs of I1's
`ai/instance/custom` are compiled by P5-05's lease, D12). `handlers/instance/` holds only `InstancePrelude.h`.

| Chunk | Java files | Java lines | Open bodies | Open Java lines | Contents |
|---|---:|---:|---:|---:|---|
| I1 | 70 | 6,377 | 434 | 3,510 | the abyss instances (19 files, 1,911 lines), the four Dredgions (808), Adma Stronghold, Draupnir Cave, Fire Temple, Haramel, Lower Udas Temple; AIs abyss, abyssal_splinter, esoterrace, theHexway, unstableSplinterpath, dredgion, azoturanFortress (custom is done) |
| I2 | 59 | 8,025 | 477 | 5,951 | the crucibles (2,073), Illuminary Obelisk (+ Infernal), Stonespear Reach, Dark Poeta, Padmarashka's Cave, the Danuar Sanctuaries; their AIs |
| I3 | 56 | 6,193 | 417 | 4,261 | the PvP instances (1,266) and arenas (1,395), Ophidan Bridge (+ _L), Eternal Bastion, Kromede's Trial, Linkgate Foundry; their AIs |
| I4 | 73 | 6,494 | 471 | 3,981 | Dragon Lord's Refuge (+ Anguished), Drakenspire Depths, Beshmundir, Aturam Sky Fortress; their AIs |
| I5 | 63 | 6,589 | 443 | 4,374 | Danuar Reliquary (+ _L, Infernal), Danuar Mysticarium, Nightmare Circus, Raksang Ruins, Rentus Base (+ Occupied), Sauro Supply Base, Talocs Hollow; their AIs |
| I6 | 65 | 6,943 | 435 | 4,801 | Steel Rake (+ Cabin), Tiamat Stronghold, Shugo Imperial Tomb, The Shugo Emperor's Vault, Infinity Shard, Theobomos Lab; their AIs |
| **all** | **386** | **40,621** | **2,677** | **26,878** | 37 top-level handlers, 6 handler directories, 39 AI directories |

## 2. Dependencies on unported code

Measured by the imports of each instance's Java files against the C++ files that still have `AION_UNPORTED` sites, then the calls read by hand:

- **Stonespear Reach (I2)**: `Legion` (27 sites) and `LegionDominionService` (6) - `player.getLegion()`, `LegionDominionService.onFinishInstance`
  and the dominion checks are M5h's (lane B). **Waits for M5h.**
- `InstanceService` (Ophidan Bridge, the crucibles: `moveToExitPoint`, `destroyInstance`) and `QuestEngine` (the Dredgions'
  `onDredgionReward`, the arenas' `onKillInWorld`, two AIs' `onDialog`): the called functions are ported; the one site of each file is
  `getOrCreateHouseInstance` / `reload`, which no instance handler calls.
- No file of I1-I6 imports `SiegeService`, `VortexService`, `BaseService` or the Panesterra services: nothing here is lane B's.
- The model types the handlers use are ported: DredgionRoom and DredgionScoreWriter (leftovers LO-1/LO-4), the score and reward classes
  (M5j CP3), StageType's data (LO-1), FlyRing.

So everything but Stonespear Reach can be ported now.

## 3. How a handler is ported and tested

- Statement for statement into `handlers/aion/gameserver/handlers/instance/<Java path>.{h,cpp}` (AIs: `handlers/ai/instance/<dir>/`), the
  prelude first, registered with `AION_INSTANCE_HANDLER(Class, mapId)` (Java `@InstanceID`) and `AION_AI(Class, "name")`; the handler file
  rules of `aion_gs_regscan` (no anonymous namespace, no namespace-scope `static`).
- Tests in each chunk's default test directory (`tests/handlers_inst_N`, executable `aion_gs_handlers_inst_N_tests`): the handler on a real
  `WorldMap2DInstance` of its map (the item fixture's world, `tests/cm_ak/ItemPacketTestSupport.h`), Rnd seeded or driven to each branch, the
  spawns and packets asserted; a mutation proof per checkpoint.
- Linking (integrator, 2026-10-14): the instance AIs derive from P5-05's root AIs (`AggressiveNpcAI`, `GeneralNpcAI`, `OneDmgNoActionAI`, ...),
  so `game-server/CMakeLists.txt` links `aion_gs_handlers_ai_core` into the I1-I6 unit test executables (test targets only; the server
  already links every handler library). This replaces D12's per-file LEASE rows for #include'd AIs, which would not scale to ~400 AIs.
- A registration check per chunk: every Java `@InstanceID` of the chunk's ported files has its marker with the same map id.
- Gates: none of the scenario gates enters these maps today; a handler is live only when a player enters its instance. When the first
  checkpoint lands, `gs.smoke.startup` (the registry scan of the full build) and `gs.scenario.m5f` (instances) are run.

## 4. Checkpoints

Smallest self-contained first; each is one commit with its tests and mutation proof.

| CP | Chunk | What | Java lines |
|---|---|---|---:|
| P6I-1 | I1 | Adma Stronghold, Draupnir Cave, Fire Temple, Haramel, Lower Udas Temple (no AI directory); the I1 test directory and fixture | 313 |
| P6I-2 | I1 | AIs azoturanFortress, abyss, dredgion | 212 |
| P6I-3 | I1 | the four Dredgions (DredgionInstance and Baranath, Chantra, Terath) | 808 |
| P6I-4 | I1 | the abyss instances and their AIs (abyssal_splinter, esoterrace, theHexway, unstableSplinterpath) | 4,264 |
| P6I-5 | I3 | Ophidan Bridge (+ _L), Linkgate Foundry, Kromede's Trial and their AIs | ~700 |
| P6I-6 | I5 | the small I5 handlers (Danuar Reliquary _L/Infernal, Mysticarium, Talocs Hollow, Sauro Supply Base) and their AIs | ~1,400 |
| later | I2-I6 | the rest by instance, largest last (crucibles, arenas, PvP instances, Shugo Imperial Tomb, Tiamat, Drakenspire, Dragon Lord's Refuge) | |
| held | I2 | Stonespear Reach, after M5h | 925 |

## 5. Progress

> **Progress:** plan written (2026-10-14, lane A).
> **Progress:** P6I-1 and P6I-3 done (`lane-a/p6-instances`): AdmaStrongholdInstance, DraupnirCaveInstance, FireTempleInstance,
> HaramelInstance, LowerUdasTempleInstance; DredgionInstance with Baranath, Chantra and Terath. Tests in `tests/handlers_inst_1` (no database:
> the fixture builds the instance maps' World, npc rows and the tribe relations; 16 cases); mutation proofs 13 of 13 and 14 of 14. Left to a
> gate: DraupnirCaveInstance.onReviveEvent and the Dredgions' revive and exit (their teleports reach HousingService, which loads from the
> database), Terath's medal rewards. gs.scenario.m5f's T20 now sums every instance handler class (its own commit; P5-SC.md), as Haramel's
> handler is HaramelInstance.
> **Progress:** P6I-2 done: the AIs of azoturanFortress, abyss and dredgion (BetrayerIcaronixAI, FortressInstanceDukeAI, IllusionGateAI,
> ShulackDrudgeAI, SurkanaAI), tested in `tests/handlers_inst_1/I1AiTest.cpp` (6 cases; ShulackDrudgeAI's supplies need the item rows, left
> to a gate); mutation proof 12 of 12. gs.scenario.m5f and gs.smoke.startup green on this code. Census I1: 434 -> 351 open bodies. Next: P6I-4, the abyss instances and their AIs.
> **Progress:** P6I-4a done: AbstractInnerUpperAbyssInstance and the nine siege keeps (Krotan/Kysis/Miren Barracks and Chambers, the three
> legion Barracks), `tests/handlers_inst_1/AbyssKeepTest.cpp` (8 cases), mutation proof 13 of 13; gs.smoke.startup green. **Java bug,
> proposed correction J-P6I4-1 (not applied):** a keep's two boss ids both call spawnChests (e.g. KrotanBarracks `case 233632, 233633`), and
> spawnChests sets chestLocations to null, so the second death throws a NullPointerException (ported faithfully; the test pins it). Proposed:
> return early when chestLocations is null.
> **Progress:** P6I-4b done: AsteriaInstance, RoahInstance, RightWingChamberInstance, LeftWingChamberInstance, SulfurTreeNestInstance
> (`AbyssChamberTest.cpp`, 5 table-driven cases over the five; mutation proof 9 of 9; gs.smoke.startup green). LeftWingChamberInstance's
> exit (handleUseItemFinish with 700455: a teleport, HousingService) is left to a gate.
> **Progress:** P6I-4c done: EsoterraceInstance (`EsoterraceTest.cpp`, 5 cases, mutation proof 8 of 8; the door arms, the revive and the zone
> message are left to a gate). Census I1: 434 -> 263 open bodies. Left of P6I-4: the esoterrace AIs, the splinters, TheHexwayInstance and
> their AIs.
> **Progress:** P6I-4d: the esoterrace AIs KexkraPrototypeAI, WardenSuramaAI and DaliaCharlandsAI (I1AiTest, mutation proofs 4 of 4, 6 of 6
> and 5 of 5; an AggressiveNpcAI without a target goes home on each attack, so the cases give it one). Census I1: 434 -> 248 open bodies.
> Left of the esoterrace AIs: CaptainMuruganAI, GreenfingersAI (repeated skills, walker steps).
> **Progress:** P6I-4e done: CaptainMuruganAI and GreenfingersAI; AbyssalSplinterInstance and UnstableSplinterpathInstance (`SplinterTest.cpp`, 10
> cases); TheHexwayInstance (`HexwayTest.cpp`, 6 cases) and the theHexway AIs CaptainJarkaAI, CaptainLakharaAI, ChestTeleporterAI, AdjutantGalamatAI
> (I1AiTest). The SummonerAI cases publish an ai template with an empty summons list and call handleSpawned first (SummonerAI.handleSpawned reads
> it). Mutation proofs 7 of 7 (Murugan, Greenfingers), 5 of 5 (splinter; the gate case strengthened), 5 of 6 (Hexway; the survivor, the expiry
> task's cancel, is equivalent: the spawn flag is already false). **Java bugs, proposed corrections (not applied):** J-P6I4-2:
> UnstableSplinterpath's returning boss is the other one of the pair; J-P6I4-3: its pieces delete the Abyssal Splinter's ids 281907/281908 (both
> ported faithfully). Census I1: 434 -> 165 open bodies (left: the abyssal_splinter and unstableSplinterpath AIs). The tests of chunks I3 and I5
> share the fixture: `tests/handlers_inst_1/InstanceFixture.h` (each chunk's support header defines its maps, npc ids and AI names).
> **Progress:** P6I-5 done (chunk I3): OphidanBridgeInstance and OphidanBridgeInstance_L, LinkgateFoundryInstance, KromedesTrialInstance; the AIs of
> kromedesTrial (KromedesBuffAI, KromedesCorpseAI, KromedesItemNpcsAI, KromedesMagasAI, KromedesPrisonersAI) and linkgateFoundry
> (LinkgateFoundryBossTeleportAI, LinkgateFoundrySecretTeleportAI). Tests in `tests/handlers_inst_3` on the shared fixture: `I3InstanceTest.cpp` (16
> cases) and `I3AiTest.cpp` (5 cases); mutation proofs 18 of 18 (after two strengthened cases: the random end boss pairs over twelve bridges, the
> Linkgate timer past its byte wrap) and 6 of 6. Left to a gate: the teleports (HousingService), the zone entries, the buffs and item grants. Census
> I3: 417 -> 369 open bodies (left: the PvP instances, the arenas, Eternal Bastion and their AIs).
> **Progress:** P6I-6 done (chunk I5): DanuarMysticariumInstance, SauroSupplyBaseInstance (its static chest list filled by the static block at
> static initialization, ZoneName.cpp's way), TalocsHollowInstance, DanuarReliquaryInstance with DanuarReliquaryInstance_L and
> InfernalDanuarReliquaryInstance; the AIs of sauroBase (6), tallocsHollow (6) and danuarReliquary (7, with Cursed and Enraged Queen Modor). Tests
> in `tests/handlers_inst_5`: `I5InstanceTest.cpp` (15 cases) and `I5AiTest.cpp` (22 cases); mutation proofs 16 of 16 and 21 of 21. Left to a gate:
> the teleports, the item grants, the skill casts and effects, the walkers, the doors (no door npcs here) and the HP phases of Ahuradim and the
> Modors. Census I5: 443 -> 257 open bodies (left: Nightmare Circus, Raksang Ruins, the Rentus Bases and their AIs). Gate note: gs.scenario.m5e_geo
> failed in combat (C17) on the leftovers-1 sweep of 2026-10-14 and on lane C's branch the same day, both under the four-way lock load: a
> load-sensitive combat timing to harden later (not re-run today).
> **Progress:** P6I-4f done (P6I-4 complete): the AIs of abyssal_splinter (9) and unstableSplinterpath (12),
> `tests/handlers_inst_1/SplinterAiTest.cpp` (11 cases, on the Abyssal Splinter map: the AIs spawn at its coordinates; a twin's cast needs an NpcAI
> on the other twin); mutation proof 21 of 21. Java's AIActions.targetCreature(ai, null) (Yamennes or Pazuzu gone) clears the owner's target. Left
> to a gate: the walks to a target, Kaluva's summons percentages, the pieces' effect checks, the casts' effects. Census I1: 434 -> 9 open bodies.
> **Progress:** P6I-7a done (chunk I5): NightmareCircus and the nightmareCircus AIs (11). Tests: I5InstanceTest (4 circus cases: the waves and
> bosses on their timers after movie 983, once; the destroy; Heiramune's death) and I5AiTest (6 cases); mutation proof 19 of 19 after two
> strengthened cases. The walkers' DummyAI in the unit tests cannot walk (Java's ClassCastException, logged by the spawn task after the spawn). Left
> to a gate: the walks, the casts and effects, the crates' loot. Census I5: 443 -> 184 open bodies (left: Raksang Ruins, the Rentus Bases and their
> AIs).
> **Progress:** P6I-7b done (chunk I5): RaksangRuinsInstance and the raksang AIs (9). `tests/handlers_inst_5/RaksangTest.cpp` (14 cases, on the
> Raksang Ruins map: the AIs spawn at its coordinates; an attacked boss must stand near the player); mutation proof 22 of 24 - the two survivors are
> equivalent (a ninth round that spawns nothing; the destroy's cancel of a task that cancels itself once the instance is destroyed). Java's
> IllusionMasterSharikAI.percent stays 100 (no 19903), ported as is. Left to a gate: the vault's teleport, the vault trial's zone entry, the walks
> and casts. Census I5: 443 -> 110 open bodies (left: the Rentus Bases and their AIs).
> **Progress:** P6I-7c done (I5 complete): RentusBaseInstance, OccupiedRentusBaseInstance and the rentusBase AIs (14).
> `tests/handlers_inst_5/RentusTest.cpp` (15 cases); mutation proof 24 of 24 after one strengthened case (Kiriana first). ImprisonedReianAI keeps
> Java's null route id as the empty string (Field<std::string>: a route id is never empty). **Java bug, ported (proposed correction J-P6I7-1):**
> BrigadeGeneralVashartiAI.onEffectEnd switches on Vasharti's own npc id (getNpcId()) instead of the visited npc's (n.getNpcId()), so the Sea of
> Fire effects 283010-283012 are never deleted there; proposed: switch on n.getNpcId(). Left to a gate: the teleport devices, the walks, Vasharti's
> forced walk and Sea of Fire. Census I5: 443 -> 0 open bodies.
