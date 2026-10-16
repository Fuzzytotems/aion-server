# Phase 6, chunk I6: the Steel Rake, Tiamat, Shugo and Theobomos instances (lane C)

Lane C, 2026-10-16, branches `lane-c/i6-*` from origin/C++ 23e0a205a. The integrator's assignment of 2026-10-16: lane C ports I6 (lane A has
I3, lane B gets I4 after m5i-rest). The way of porting and testing is p6-instances-plan.md's (§3): statement for statement under
`handlers/aion/gameserver/handlers/instance/` and `handlers/ai/instance/<dir>/`, `AION_INSTANCE_HANDLER` / `AION_AI` markers, tests in
`tests/handlers_inst_6` on the shared fixture `tests/handlers_inst_1/InstanceFixture.h` (I6's support header defines its maps, npc ids and AI
names), a mutation proof per checkpoint, `gs.smoke.startup` and `gs.scenario.m5f` when a checkpoint lands.

## 1. The measure (census of 2026-10-16 on 23e0a205a)

Every Java body of I6 is open: no file of the chunk has a C++ file yet. 65 Java files, 6,943 lines, 435 open bodies (4,801 open Java lines).

| Instance (map) | Handler (lines) | AI directory (files, lines) | Total lines |
|---|---|---|---:|
| Steel Rake (300100000) and its Cabin (300460000) | `rakes/SteelRakeInstance` (28), `rakes/SteelRakeCabineInstance` (30) | `rakes` (12, 882) | 940 |
| Theobomos Lab (310110000) | `TheobomosLabInstance` (94) | `theobomosLab` (3, 296) | 390 |
| Infinity Shard (300800000) | `InfinityShardInstance` (215) | `infinityShard` (2, 341: HyperionAI 250) | 556 |
| The Shugo Emperor's Vault (301400000) | `TheShugoEmperorsVault` (574) | `theShugoEmperorsVault` (8, 547) | 1,121 |
| Shugo Imperial Tomb (300560000) | `ShugoImperialTombInstance` (1,329) | `shugoImperialTomb` (8, 507) | 1,836 |
| Tiamat Stronghold (300510000) | `TiamatStrongHoldInstance` (265) | `tiamatStrongHold` (25, 1,835) | 2,100 |

## 2. Dependencies on unported code

The 68 `com.aionemu.gameserver` imports of the chunk's files, checked against the C++ files with `AION_UNPORTED` sites: only `QuestEngine` (one
site, `reload`, which no handler of I6 calls). The services the handlers call (TeleportService, PlayerReviveService, ItemService, DialogService)
are ported; their teleports reach HousingService (database), so the exits and revives are left to a gate, as in I1-I5. Nothing waits.

## 3. Checkpoints

Smallest self-contained first; each checkpoint is one branch (`lane-c/i6-<instance>-cpN`, stacked) with its tests and mutation proof.

| CP | What | Java lines |
|---|---|---:|
| I6-1 | the I6 test directory and support header; Steel Rake and its Cabin with the 12 rakes AIs | 940 |
| I6-2 | Theobomos Lab and Infinity Shard with their AIs (5) | 946 |
| I6-3 | The Shugo Emperor's Vault with its AIs (8) | 1,121 |
| I6-4 | Shugo Imperial Tomb with its AIs (8) | 1,836 |
| I6-5a | Tiamat Stronghold's handler and its brigade generals (Chantra, Laksyaka, Tahabata, Terath) with their altars and helpers | ~1,100 |
| I6-5b | the rest of the Tiamat AIs (the traps and skill areas, Kumbanda, Shabokan, the scientists, the statues) | ~1,000 |

## 4. Progress

> **Progress:** plan written (2026-10-16, lane C).
> **Progress:** I6-1 done (`lane-c/i6-rakes-cp1`): SteelRakeInstance, SteelRakeCabineInstance and the twelve rakes AIs. Tests in
> `tests/handlers_inst_6` on the shared fixture (`I6TestSupport.h`): `I6InstanceTest.cpp` (3 cases) and `RakesAiTest.cpp` (14); mutation proof
> 25 of 25 after one strengthened case (Mantutu back home). FeedingMantutuAI's custom event passes the device as a `Ref<Npc>`, an empty
> `std::any` for Java's null. Left to a gate: Tamer Anikiki's walk (walker rows), Golden Eye Mantutu's move to a device, the casts' effects, the
> use bars (DialogService) and the fly teleport.
