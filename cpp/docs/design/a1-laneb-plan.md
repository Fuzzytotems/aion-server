# A1, lane B: the siege AIs, the worlds-root AIs, Panesterra and Ahserion's Flight (2026-10-16)

The owner decided on 2026-10-15 that A1 is next. This file is lane B's share of chunk A1 (`handlers/ai/{worlds,siege,...}`), plus
Ahserion's Flight. D6 had deferred the raid to phase 6; it is in scope now. The integrator approved the split and the gates on 2026-10-16.

## 1. The measure

Run on origin/C++ 23e0a205a with `tools/porting/census.py --chunks A1,P5-12b1`. Only open bodies are counted. The siege AIs phase 5 already
ported under the P5-12a and P5-12b1 leases stay as they are.

| Slice | Java files | Open bodies | Java lines |
|---|---|---:|---:|
| siege (12 AIs) | DredgionCommanderAI 8, EmpoweredAgent 21, EnragedAgent 5, FortressGateAI 4, GateRepairAI 7, IncarnateAI 3, MercenaryAI 4, MineAI 2, SiegeCannonAI 5, SiegeTeleporterAI 5, SiegeWeaponAI 11, SpringAI 8 | 83 | 1,212 |
| worlds root (7 AIs) | AgrintAI 7, BlackenedGraveAI 3, DefensiveCannonAI 3, ForgottenGraveAI 4, LegionDominionInvasionRiftOpenAI 3, NamelessGraveAI 3, WorldBlesserAI 3 | 26 | 380 |
| worlds/panesterra (4 AIs) | AdvanceCorridorAI, GovernorCorridorAI, PanesterraEntryCorridorAI, Rank100CorridorAI | 12 | 211 |
| worlds/panesterra/ahserionsflight (15 AIs) | AhserionAI, AhserionTankControllerAI, AhserionMountableTankAI, AhserionConstructDestroyerAI, AdvanceCorridorShield, AhserionRegistrationCorridor, AhserionSkyAssaulterAI, CampDefenseCannonAI, AhserionConstructAI, AhserionAssaultPod, AhserionGate, AhserionAggressiveNpcAI, EreshkigalsVoiceAI, AhserionAssaultCommanderAI, AhserionSorcerer | 78 | 1,026 |
| services (P5-12b1) | `AhserionRaid` 15 bodies (308 lines); `PanesterraService` `startAhserionRaid`, `stopAhserionRaid`, `isAhserionRaidStarted` | 18 | ~330 |
| **all** | | **217** | ~3,160 |

## 2. Checkpoints

Each checkpoint is one branch, stacked on the previous one (`lane-b/a1-<slice>-cpN`). Every checkpoint brings:
- its unit tests in `tests/handlers_ai_world/<Slice>A1AiTest.cpp`, named by slice so the three lanes never edit the same file;
- a mutation proof;
- `gs.smoke.startup`.

The first commit, 5f4440e74, is a standalone change: A1's tests link P5-05's `aion_gs_handlers_ai_core`. Lanes A and C cherry-pick that same commit.

| CP | Branch | What | Bodies | Gate |
|---|---|---|---:|---|
| 1 | lane-b/a1-siege-cp1 | IncarnateAI, MineAI, DredgionCommanderAI, SpringAI, SiegeTeleporterAI, FortressGateAI, MercenaryAI | 34 | gs.scenario.m5i (the siege npcs of 1131 lose their DummyAI) |
| 2 | lane-b/a1-siege-cp2 | EnragedAgent, GateRepairAI, SiegeCannonAI, SiegeWeaponAI | 28 | gs.scenario.m5i |
| 3 | lane-b/a1-siege-cp3 | EmpoweredAgent | 21 | gs.scenario.m5i |
| 4 | lane-b/a1-worlds-cp4 | the seven worlds-root AIs | 26 | the gates whose maps spawn them (to be measured), gs.smoke.startup |
| 5 | lane-b/a1-panesterra-cp5 | the four corridor AIs; AhserionRaid; PanesterraService start/stop/isStarted | 30 | gs.scenario.m5i (C18 enters Belus) |
| 6 | lane-b/a1-ahserion-cp6 | the ahserionsflight AIs other than the five below | ~39 | none: unit tests and gs.smoke.startup |
| 7 | lane-b/a1-ahserion-cp7 | AhserionAI, AhserionTankControllerAI, AhserionMountableTankAI, AhserionConstructDestroyerAI, AdvanceCorridorShield | ~39 | none: unit tests and gs.smoke.startup |

**Ahserion's Flight has no gate coverage.** No scenario gate can run the raid yet. The M5i profiles keep its cron at 2099 (D6), and a raid
needs the Panesterra teams of a whole server. Checkpoints 5 to 7 rest on their unit tests and `gs.smoke.startup`.

## 3. Held

`lane-b/m5i-rest` waits: C5, C6 and C18, and the four gate mutants. Its open item is the geo variant's C5, which must take its points from
the geo shield mesh. It is picked up when a siege checkpoint needs `gs.scenario.m5i_geo` anyway, or after checkpoint 5.

## 4. Progress

> **Progress:** plan written (2026-10-16).
>
> - **cp1** (9e2ccd903): 7 siege AIs. SiegeA1AiTest.cpp has 9 cases; mutants 15 of 15 killed.
> - **cp2** (ba29e81b8): 4 siege AIs, including the first Summon AI, SiegeWeaponAI. The file has 14 cases; mutants 10 of 10 killed.
> - **cp3** (0473b2678): EmpoweredAgent. The file has 17 cases. Mutants: 7 run, 5 killed, 2 equivalent; EmpoweredAgent.ask's ALLOW_RESPAWN
>   and REMOVE_EFFECTS arms repeat SiegeNpcAI's false.
> - **cp4** (102bd3c28): the 7 worlds-root AIs. WorldsA1AiTest.cpp has 4 cases; mutants 8 of 8 killed. Agrint's helpers and chests, the
>   graves' spawns and WorldBlesser's dialog are not unit-tested: rndSpawnInRange needs the test map in the World.
> - **Gates** for cp1 to cp4: gs.smoke.startup passes; gs.scenario.m5i passes 16 of 17. Its C1 fails on the base too (the enter-world wrap,
>   3d620a61c on #338). The m5i spawns now get these AIs instead of a DummyAI.
> - **cp5**: the 4 corridor AIs, AhserionRaid (15 bodies) and PanesterraService start/stop/isStarted. PanesterraA1AiTest.cpp has 4 cases;
>   tests/worldevents/P5-12b1/AhserionRaidTest.cpp has 3; aion_gs_worldevents_tests passes 57 of 57 under ctest. Mutants: 11 run, 9 killed,
>   2 not observable. The corridors' rank checks for the advance and governor corridors are masked by the level-65 check, and the fixture
>   has no level-65 character. The raid's spawns, steps and rewards have no unit or gate coverage. phase5-allowlist.txt drops its 18
>   Ahserion rows (closure: OK).
