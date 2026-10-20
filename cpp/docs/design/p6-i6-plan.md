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
| I6-2a | Theobomos Lab with its AIs (3) | 390 |
| I6-2b | Infinity Shard with its AIs (2; HyperionAI is 250 lines) | 556 |
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
> **Progress:** I6-2a done (`lane-c/i6-theobomos-cp2`): TheobomosLabInstance and the theobomosLab AIs (SilikorofMemoryAI, TriroansSummonAI,
> UnstableTriroanAI). `TheobomosTest.cpp` (7 cases); mutation proof 16 of 16 after one strengthened case (Triroan 4 m from a pillar). Java's
> synchronized TriroansSummonAI.useSkill is a monitor member. A test summon's DummyAI cannot walk (Java's ClassCastException, std::bad_cast here).
> Left to a gate: the summons' walks and arrival, the casts' effects.
> **Progress:** I6-2b done (`lane-c/i6-infinity-cp3`): InfinityShardInstance and the infinityShard AIs IdeResonatorAI and HyperionAI.
> `InfinityShardTest.cpp` (10 cases); mutation proof 23 of 23 after three strengthened cases (the last generator id, the single return home,
> isBoss of another npc). HyperionAI builds its two spawn positions in the constructor from the owner's map id (Java's field
> initializers); IdeResonatorAI's targetCreature(this, null) clears the target. Left to a gate: the charge messages (onStartEffect needs a real
> skill's Effect), the guards' walks, the logout teleport, the casts' effects.
> **Progress:** I6-3 done (`lane-c/i6-vault-cp4`): TheShugoEmperorsVault and the theShugoEmperorsVault AIs (IDSweep_Shugos, IDSweep_Bosses,
> IDSweep_HealTower, IDSweep_In_Npc, RuthlessJabaraki, ShugoMorpher, VaultKeeper, WatchmanHokuruki). `ShugoEmperorsVaultTest.cpp` (14 cases);
> mutation proof 31 of 32 - the survivor is equivalent here (the minute's timer's own `started` check only guards a door opened while the timer
> already runs; the door cancels it otherwise).
> Java's synchronized methods use monitor members. **Java bugs, ported (proposed corrections, not applied):** J-I6-1: VaultKeeper counts up its
> room also for a returning player, so the next new player skips a room (proposed: count up only for a new player); J-I6-2: RuthlessJabaraki's
> phases are 96/80/60/45/40/35 % but its switch reads 95/80/65/50/35/20, so only two of six waves come (proposed: phases 95, 80, 65, 50, 35,
> 20). Left to a gate: the score packet's bytes, the doors, the morph cache, the revive and teleports, the key rewards, the casts.
> **Progress:** I6-4 done (`lane-c/i6-tomb-cp5`): ShugoImperialTombInstance (its 900-line stage method converted statement for statement) and the
> shugoImperialTomb AIs (Attacker, Boss, Ghost, ImperialObelisk, ImperialShrine, Modo, RoyaltyAdmirer, TransformationDevice).
> `ShugoImperialTombTest.cpp` (9 cases); mutation proof 32 of 32. A wave's walkers need their registered AI to walk (a test DummyAI cannot), so the cases count a wave by
> its first npc. **Java bug, ported (proposed correction J-I6-3):** despawnNpcs lists 291512 and 291513 for 219512 and 219513, so a destroyed
> monument leaves those two standing (the test pins it). Left to a gate: the walks and routes, the transformations' effects, the teleports, the
> bosses' casts.
> **Progress:** I6-5a (`lane-c/i6-tiamat-cp6`): TiamatStrongHoldInstance and the four brigade generals (BrigadeGeneralChantraAI,
> BrigadeGeneralLaksyakaAI, BrigadeGeneralTahabataAI, BrigadeGeneralTerathAI). `TiamatTest.cpp` (10 cases). **Java bug, ported (proposed
> correction J-I6-4):** BrigadeGeneralTerathAI.handleBackHome deletes the Aetheric Field but keeps the reference, so the next fight spawns no
> field and leaves door 706 open (the test pins it). Left to a gate: the doors, the zones, the marchers' and drakans' moves, the casts.
> Mutation proof: 29 of 29 mutants killed (two survivors of the first run, a swapped trap id and a missing cancel at death, led to a
> stronger Chantra case that watches both rings and the next ring after her death).
> **Progress:** I6-5b (`lane-c/i6-tiamat-rest-cp7`): the other 21 AIs of ai/instance/tiamatStrongHold (AdjutantAnuhartAI, BladeStormAI,
> CapturedDrakanScientistAI, ChantraRingsAI, DistortedSpaceAI, EarthQuakeAI, ElectrocuteAI, FireStormAI, GravityAI, InvincibleShabokanAI,
> KharunAI, MuraganAI, SinkingSandAI, SlowTimeAI, SuramaTheTraitorAI, TahabataAltarAI, TahabataAltarFinalAI, TiamatEyeAI, TimeAcceleratorAI,
> TraitorKumbandaAI, VershakaStatueAI). `TiamatRestAiTest.cpp` (13 cases); the I6 npc list takes their npcs and gives Kahrun's fighting self
> (800335) and Raksha (219356) a GeneralNpcAI, since they cast. CapturedDrakanScientistAI's `synchronized (qs)` is one class-wide monitor (no
> other code locks a QuestState, so the exclusion is Java's). **Java bugs, ported (proposed corrections):**
> - J-I6-5: TahabataAltarAI picks the debuff for 283116/283118 but checks the bands for 283253/283255, ids no npc template has, so an altar
>   never debuffs; proposed: the bands for 283116/283118.
> - J-I6-6: TiamatEyeAI switches on the 4.0 ids 283913-283916, but the handler spawns the eyes as 283177-283180, so they say nothing (the test
>   pins it); proposed: switch on 283177-283180.
> Left to a gate: the casts and their effects (the rings', quakes', altars' and slowed time's debuffs in their bands, the HP-phase and final
> buffs), the walks, Surama's door.
> Mutation proof: 40 of 41 mutants killed; the survivor is equivalent (DistortedSpaceAI without its deleteOwner at death: with no decay
> NpcController.onDie deletes the npc itself). The first run's survivors (Muragan's watcher walking, the scientist's escape time) led to
> stronger Muragan and scientist cases.
> **Progress:** I6 census at the tip: 435 of 435 named Java bodies ported, 0 open. The census found two I6-3 gaps, fixed on the tip:
> TheShugoEmperorsVault.leaveInstance (the instance exit, left to a gate like the other exits) was missing, and ShugoMorpher's morph() now
> cites Java's handleUseItemFinish so the census sees the rename.
> **Progress (2026-10-19):** J-I6-1 is an applied correction (owner's decision 2026-10-19, both branches): docs/deviations/P6-instances.md.
> **Progress (2026-10-19):** J-I6-2 is an applied correction (owner's decision 2026-10-19, both branches): docs/deviations/P6-instances.md.
