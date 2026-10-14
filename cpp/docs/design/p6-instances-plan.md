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
> **Progress:** P6I-4d: the esoterrace AI KexkraPrototypeAI (I1AiTest, mutation proof 4 of 4). Left of the esoterrace AIs: CaptainMuruganAI,
> DaliaCharlandsAI, GreenfingersAI, WardenSuramaAI (skills and walker routes: they need the skill and walker rows in the fixture).
