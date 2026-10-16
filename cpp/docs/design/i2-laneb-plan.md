# I2, lane B: the crucibles, Dark Poeta, Illuminary Obelisk, Padmarashka's Cave and the Danuar Sanctuaries (2026-10-16)

The integrator gave lane B the rest of chunk I2 on 2026-10-16. Stonespear Reach, I2's other part, is already on `feat/cpp-p6-stonespear`
(#331/#332). The checkpoints are stacked on that branch tip, d3800009a. The PRs go in after #331. They extend Stonespear's
`tests/handlers_inst_2/I2TestSupport.h` in place, because `InstanceFixture.h` wants one definition of its three hooks per test executable.
The deviations go in `docs/deviations/I2.md`, under a heading per instance.

## 1. The measure

Run on origin/C++ 23e0a205a with origin/feat/cpp-p6-stonespear merged in, using `tools/porting/census.py --chunks I2`. Only open bodies are
counted, and Stonespear's are not open.

| Slice | Java files | Open bodies | Java lines (whole files) |
|---|---|---:|---:|
| crucible base | `instance/crucible/CrucibleInstance` 14, `CrucibleChallengeInstance` 23 | 37 | 775 |
| crucibleChallenge AIs | AdministratorArminosAI 3, ArbiterAI 3, BarrelAI 2, CrucibleRiftAI 5, PoppyOnTheRunAI 4, RecordkeeperAI 3 | 20 | 346 |
| Empyrean Crucible | `EmpyreanCrucibleInstance` | 36 | 1,298 |
| empyreanCrucible AIs (14) | ArminosDrakyAI 5, EmpyreanAdministratorArminosAI 3, EmpyreanArbiterAI 3, EmpyreanRecordKeeperAI 4, KingConsierdAI 10, MagePreceptorAI 10, PriestPreceptorAI 8, QueenAlukinaAI 8, RM1337AI 11, SparkOfDarknessAI 4, SpectralWarriorAI 4, StrangeCreatureAI 4, TakunGojiraAI 2, WarriorPreceptorAI 9 | 85 | 1,162 |
| Dark Poeta | `DarkPoetaInstance` 15; 9 AIs 47 | 62 | 928 |
| Illuminary Obelisk | `IlluminaryObeliskInstance` 19, `InfernalIlluminaryObelisk` 3; 10 AIs 73 | 95 | 1,308 |
| Padmarashka's Cave | `PadmarashkasCaveInstance` 4; 3 AIs 38 | 42 | 584 |
| Danuar Sanctuaries | `DanuarSanctuaryInstance` 6, `SeizedDanuarSanctuaryInstance` 8; 3 AIs 16 | 30 | 468 |
| **all** | 54 files | **407** | 6,869 (5,021 in open bodies) |

Every type these files import is already ported. The only open site is `QuestEngine`'s one `AION_UNPORTED`, and no I2 call reaches it.

## 2. Checkpoints

Each checkpoint is one branch, stacked on the previous one (`lane-b/i2-<slice>-cpN`). Every checkpoint brings:
- its unit tests in `tests/handlers_inst_2/<Instance>Test.cpp` (the instance) and `<Instance>AiTest.cpp` (its AIs);
- a mutation proof;
- the gates `gs.smoke.startup` (the registry scan of the full build) and `gs.scenario.m5f` (instances).

None of the scenario gates enters these maps. A handler only runs when a player enters its instance.

| CP | Branch | What | Bodies |
|---|---|---|---:|
| 1 | lane-b/i2-crucible-cp1 | CrucibleInstance, CrucibleChallengeInstance and the 6 crucibleChallenge AIs | 57 |
| 2 | lane-b/i2-empyrean-cp2 | EmpyreanCrucibleInstance | 36 |
| 3 | lane-b/i2-empyrean-cp3 | the 14 empyreanCrucible AIs | 85 |
| 4 | lane-b/i2-poeta-cp4 | DarkPoetaInstance and its 9 AIs | 62 |
| 5 | lane-b/i2-obelisk-cp5 | IlluminaryObeliskInstance, InfernalIlluminaryObelisk and the 10 AIs | 95 |
| 6 | lane-b/i2-padmarashka-cp6 | Padmarashka's Cave, the two Danuar Sanctuaries and their 6 AIs | 72 |

## 3. Progress

> **Progress:** plan written (2026-10-16).
