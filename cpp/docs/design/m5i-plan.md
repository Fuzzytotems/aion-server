# M5i work plan (siege and world events)

> **Status:** plan **rev 2**, 2026-09-23, revised after its adversarial review (§14 lists every finding and what changed). Rev 1 was a
> **read-only** analysis over HEAD `c1edb0afb`; rev 2 re-checked every finding over HEAD `760e8ab5c` ("M5b-2 stage 1 part 3: the effect
> classes; passives and post-spawn skills go live"), which touched the two partial allow-lists but **no file of P5-12a or P5-12b**, and over
> the sibling plans as they stand at 23:30 (`m5e`, `m5f`, `m5g`, `m5h` and `m5j` were all revised again after rev 1, between 23:20 and
> 23:30; rev 2 cites their current line numbers). **Nothing was compiled, built or run for this plan.** C++ statements come from reading both trees, from `game-server/chunks.cmake`,
> `tools/porting/chunks.py files|java|owner` and from counting `AION_UNPORTED(` / `AION_PARTIAL(` sites. Data statements come from throw-away
> parses of `data/static_data` and `config/schedule` (ElementTree, so XML comments are skipped as the server skips them); the scripts are in
> the session scratchpad and **must be taken over by the `tools/oracle` commands of G-01/G-02 before any gate asserts a number**. §12
> separates what was **measured** from what was **inferred**.
>
> It follows the shape of [m5b2-plan.md](m5b2-plan.md) and [m5b-plan.md](m5b-plan.md): paths end to end with file:line, work items, lanes, a
> gate whose every assertion says what it proves, what it cannot prove and which mutation it kills, risks, stages, a real-client checklist
> and a measured/inferred split. Inputs: [phase5-roadmap.md](phase5-roadmap.md) row 9, [m5a-plan.md](m5a-plan.md) D1/O-05/O-11,
> [m5a-client-session.md](m5a-client-session.md) F-1, `docs/deviations/P5-12a.md`, `docs/deviations/P5-12b.md`, **every sibling plan**
> ([m5b3](m5b3-plan.md), [m5c](m5c-plan.md), [m5d](m5d-plan.md), [m5e](m5e-plan.md), [m5f](m5f-plan.md), [m5g](m5g-plan.md),
> [m5h](m5h-plan.md), [m5j](m5j-plan.md); §3 says what M5i assumes from each and §3.1 what each hands to M5i),
> `generated/concurrency/cycles.toml`, `tests/scenario/m5{a,b}_partial_allowlist.txt`, `docs/porting/header-requests.md` 5a-pre-10 and sm-b-1,
> `docs/design/capacity-proposals.md` §8.3 (the standing resource rule).
>
> **Five corrections to the roadmap, in order of how much they change the milestone.**
>
> 1. **"P5-12a 105 + P5-12b 197" is 302 sites; the milestone is ~540 bodies.** The two chunks hold **305** sites today (107 + 198, plus one
>    `AION_PARTIAL`), but **20 Java classes of the two chunks have no C++ file at all** (114 bodies, 1,525 Java lines: `FortressSiege`,
>    `ArtifactSiege`, `OutpostSiege`, `AgentSiege`, `SiegeStartRunnable`, every `Base` subclass, `Invasion`, `RiftOpenRunnable`,
>    `WorldRaidRunnable`, …), seven enum companions are missing (~23 bodies), and the milestone cannot run or be gated without **~105 bodies
>    outside the chunks** (the chat-command framework, ten siege/base AI handlers, two server packets, the siege reward bodies, the Legion
>    Dominion share M5h hands over). Five bodies of the chunks are already taken by M5f and M5h. §2.7.
> 2. **These systems do not wait for players, and one of them has no off switch.** With the shipped configuration Java starts 49 artifact
>    sieges, 31 bases, 70 siege locations' npcs and **68 cron jobs** at boot (§2.1). `BaseService` has **no enable key in Java at all**
>    (BaseService.java:30-53); the only thing keeping 31 bases from starting in every C++ server — and in every earlier gate — is the
>    `AION_PARTIAL` in `BaseService.cpp:18`. Closing it is the M5i analogue of M5b-2's D2 and M5d's D3 (D3 below).
> 3. **Nothing here can be gated in real time.** A fortress siege is scheduled weekly and starts 300 s after its preparation, a world raid takes
>    30 minutes to show its boss, a casual base 10-20 minutes to spawn one. The plan drives every event through **the Java admin commands**
>    (`//siege`, `//base`, `//rift`, `//worldraid`) and proves the timelines in-process on `DeterministicExecutor` + `ManualClock` (D2, §2.9,
>    §10). **That the schedules are armed can only be proven in-process** (T-02): at login an armed and an unarmed server send the same bytes
>    in every hour a gate may run in (§2.9).
> 4. **Three things fire on their own today, whatever the profile.** `CM_LEVEL_READY` calls the unported `SiegeService::onEnterSiegeWorld` for
>    every player entering Inggison, Gelkmaros or Reshanta (`CM_LEVEL_READY.cpp:96-99`; M5f's T-05 ports it); `CronJobService` arms a Sunday
>    18:50 job that reaches the unported `PanesterraService::startAhserionRaid` (`CronJobService.cpp:86, 175-177`, `PanesterraService.cpp:38-39`)
>    and a hard-coded Wednesday 09:00 job that reaches the unported `LegionDominionService::startWeeklyCalculation` (`CronJobService.cpp:88,
>    185-187`, `LegionDominionService.cpp:60-61`; M5h's S-08 ports it). §2.1.
> 5. **A GM account cannot enter the world with Java's default admin configuration**, and the gate's control plane is a GM. A staff login
>    reaches `GMService::onPlayerLogin`, whose `execute_commands` arm is `AION_UNPORTED` whenever the list is non-empty (`GMService.cpp:59-68`) —
>    and the default is `//invis, //invul, //enemy none, //see` (`AdminConfig.cpp:28`, `admin.properties:101`); an access level ≥ 9 then reaches
>    the staff `VERSION_INFO` partial (`PlayerEnterWorldService.cpp:524-528`). m5j-plan.md found the first half (its D4, :580). Both M5i
>    profiles empty the list (D2) and stage 0 closes the partial (Z-06). §2.6.

---

## 1. Summary

**What is ported.** The M5a and M5b-1 lanes ported everything world events touch *from the outside*: the location models' zone handlers
(`SiegeLocation`, `FortressLocation.onEnterZone`, `VortexLocation`), `SiegeShield` (the F-1 fix, commit `29a01ece1`), `Influence`, the
service constructors and their disabled branches, `SiegeService::isRespawnAllowed`, `RiftInformer`, `RiftManager`'s spawn-template registry,
`EventService` exact for `disabled_events=*`, `ConquerorAndProtectorService.init`, `LegionDominionService::initLocations`, and **every server
packet the systems send** (`SM_SIEGE_LOCATION_INFO`, `SM_SIEGE_LOCATION_STATE`, `SM_FORTRESS_INFO`, `SM_SHIELD_EFFECT`,
`SM_ABYSS_ARTIFACT_INFO3`, `SM_RIFT_ANNOUNCE`, `SM_AFTER_SIEGE_LOCINFO_475`, `SM_CONQUEROR_PROTECTOR`, `SM_NPC_ASSEMBLER`, `SM_UPGRADE_ARCADE` all
have 0 `AION_UNPORTED`) **except two** (`SM_INFLUENCE_RATIO.cpp:14`, `SM_FORTRESS_STATUS.cpp:14`, which waited for `Influence.h` and can now be
written). The spawn plumbing is ported too: `SpawnsData` keeps siege, base, rift and vortex groups, `VisibleObjectSpawner::spawnSiegeNpc` /
`spawnRiftNpc` / `spawnInvasionNpc` (`VisibleObjectSpawner.cpp:127, 153, 170`), `World::getLocalSiegeNpcs` (`World.cpp:165`),
`SpawnEngine::newSiegeSpawn` and `newSingleTimeSpawn`, `RVController` (0 unported), `ShieldObserver` and `CollisionDieActor::onMoved`.

**What is empty** is the engine of every system: the state machines (`Siege` and its four subclasses, `Base` and its six, `WorldRaid`,
`DimensionalVortex`/`Invasion`, `Event`/`EventBuffHandler`, `AhserionRaid`), the services that start and stop them, and the timers that drive
them. Measured per subsystem (§2.2): siege 107 sites + 59 no-file bodies; bases 38 + 40; Panesterra 43; events 45; rifts 17 + 2; vortex
22 + 11; world raids 20 + 2; conqueror/protector 14.

**The size, honestly:** ~540 bodies over ~10,400 Java lines — ~435 in the two chunks (after the five M5f and M5h take) and ~105 outside; ~20
of them (Ahserion's Flight) are deferred, so **M5i ports ~520**. That is M5b-2's size (~521). It splits into **four stages** (§9): a small
stage 0 that lands the command framework, the staff-login partial and the cron oracle; a stage 1 in **three parts** of up to six lanes that makes
**a fortress besiegeable and an artifact capturable** plus rifts, world raids, events and the conqueror/protector system; a stage 2 in two
parts — five lanes for **bases, Balaur assaults, the agent fight, the rest of the Legion Dominion service** and the gate with the
re-greening, then a fixups part — serial behind the bases lane; and a stage 3 for **the vortex** (it needs M5g's alliances) and the Panesterra
service. **Ahserion's Flight is deferred to phase 6** (D6): it is 15 bodies of `AhserionRaid` on top of 19 phase-6 AI handlers (1,237 Java
lines), level-65 content with no path a gate could drive.

**What rev 2 changed** (§14): the five bodies other plans already take (M5f T-05, M5g K-04, M5h S-06, M5d A-01) became Dep rows marked for
re-verification; the Wednesday cron is M5h's (its S-08, revised after rev 1); M5i takes the rest of the Legion Dominion service M5h hands it
(D14), `CM_SHOW_MAP` (D15), `ApExtractAction`, two enum companions and the staff `VERSION_INFO` partial; the gate places both characters
explicitly, adds a relocation-at-login step, takes the event buff expectation from a seeded fixture and derives every count from the case
history; the soak became a proposal to the user (D17, the standing resource rule).

**The gate** (`gs.scenario.m5i`, + `_geo`) plays the milestone with two characters in Reshanta and Eltnen — an Asmodian player and an Elyos
GM (access level 9) who types the admin commands — and asserts, with decoders written from Java's `writeImpl`: the siege state every client
receives at login, a capture without a siege, a siege start that throws the enemy out of the fortress, the shield that kills an enemy flying
through it, an enemy who logged out before the start relocated at login (and one who logged out after it not), a capture during a siege with
the world buff, the balance buff and the messages, a defended siege, an artifact that restarts its endless siege, the permanent event's pool
buff from a seeded fixture (and the absence of its instance-only buffs), a world raid's first stage, a rift opening and closing, a base
changing hands and being assaulted, a Balaur assault wave, and persistence across a server restart (§10). The timelines the gate cannot wait
for are fourteen in-process `ManualClock` tests (§10.6).

---

## 2. The paths, end to end

### 2.1 What starts on its own at server start — today, and after M5i

Java's startup order (GameServer.java:109-166) initialises every location holder before `SpawnEngine.spawnAll` and starts the systems after
it. The C++ `GameServer::main` mirrors it step for step (`GameServer.cpp:180-186, 199, 214-223, 229, 233, 244`). Each step runs its body
directly (`GameServer.cpp:139-141`), and `main.cpp:654-658` turns an `UnportedException` into "Game server startup stopped at an unported
function" and exit code ERROR — **so an enabled system whose body is unported stops the server at boot, loudly, before any client connects.**

| System | Config key: Java default / shipped `config/main` / user's `config/mygs.properties` | What Java starts at boot | C++ today with the user's profile | C++ today with the shipped config | Safe today? |
|---|---|---|---|---|---|
| **Sieges** | `gameserver.siege.enable` true (SiegeConfig.java:15) / true (siege.properties:8) / **false** (mygs.properties:8) | despawn every siege location's npcs, spawn PEACE npcs of 18 fortresses, 2 outposts, 49 standalone artifacts (1,454 spots at the default races); **start 49 endless artifact sieges immediately**; arm 21 fortress-preparation and 1 agent-fight cron jobs and the hourly status broadcast (SiegeService.java:99-172) | constructor logs "Sieges are disabled in config." (`SiegeService.cpp:56-74`); `initSieges` returns at its guard (`:86-87`) | constructor loads `siege_locations` (ported); `initSieges` reaches `AION_UNPORTED` (`:91`) | **yes** (off, or loud stop) |
| **Balaur assaults** | `gameserver.siege.assault.enable` **false** (SiegeConfig.java:20) / **true** (siege.properties:11) / not set (false) | nothing at boot; every siege start rolls a fortress assault (Rnd < influence × 100 × rate) or schedules an artifact assault in 3-48 h (BalaurAssaultService.java:44-54) | unreachable | unreachable (sieges stop first) | yes |
| **Bases** | **none — Java has no switch** (BaseService.java:30-53) | 31 bases start (11 CASUAL, 12 STAINED, 4 PANESTERRA, 4 PANESTERRA_FACTION_CAMP): flag, merchant and sentinel spawns, then outrider (1-5 min) and boss (0-20 min) tasks, then **an assault cycle forever**: when no player is in the region, a 20 % chance per cycle that another race takes the base (Base.java:64-70, 97-139) | constructor is `AION_PARTIAL` and creates no location (`BaseService.cpp:18`), `initBases` loops over nothing (`:28-41`); the site is an §A row of `m5a_partial_allowlist.txt:20-21` and `m5b_partial_allowlist.txt:33-34` | same | **only because of the partial** |
| **Rifts** | `gameserver.rift.enable` true (CustomConfig.java:182) / true / **false** | 16 cron jobs (8 hourly at `0 0 * ? * *`, 8 weekly with guards) (RiftService.java:43-50, rift_schedule.xml) | locations empty, `initRifts` no-op | `initRifts` reaches `AION_UNPORTED` (`RiftService.cpp:27`) | yes |
| **Dimensional vortex** | `gameserver.vortex.enable` true (CustomConfig.java:187) / true / **false** | PEACE vortex spawns, 2 weekly cron jobs (VortexService.java:33-41) | no-op | `spawn` is `AION_UNPORTED` (`VortexService.cpp:49-50`) — **the first step that stops a shipped-config server** (it runs before `spawnAll`) | yes |
| **World raids** | `gameserver.worldraid.enable` true (EventsConfig.java:27) / true / **false** | 16 schedules, 23 cron jobs, all at hh:30 on listed days of the month (WorldRaidService.java:45-56, world_raid_schedule.xml) | no-op | `initWorldRaids` `AION_UNPORTED` (`WorldRaidService.cpp:37`) | yes |
| **Conqueror/protector** | `gameserver.cp.enable` true / true / **false** | a 10-minute fixed-rate kills-decrease timer (ConquerorAndProtectorService.java:41-63) | no-op | **ported and running** (`ConquerorAndProtectorService.cpp:34-64`); `addVictims` is unported but reached only for a player with victims, i.e. after a PvP kill | yes |
| **Events** | `gameserver.event.service.disabled_events` "" / "" / **`*`** | every active event starts (spawns, inventory drops, buffs) and a **5-minute cron** re-checks them (EventService.java:45-52) | 5-minute cron armed and harmless: the active set stays empty (`EventService.cpp:62-71`) | the permanent event "Beyond Aion Server Buffs" starts → `startOrStopEvents` reaches `AION_UNPORTED` (`EventService.cpp:213`) | yes |
| **Ahserion's Flight** | `gameserver.siege.panesterra.ahserion.time` `0 50 18 ? * SUN`, scheduled **unconditionally** by `CronJobService` (CronJobService.java:61-63) | a cron job | **armed** (`CronJobService.cpp:86, 175-177`); every Sunday 18:50 it calls `PanesterraService::startAhserionRaid` → `AION_UNPORTED` (`PanesterraService.cpp:38-39`) → one ERROR line | same | **no** — loud, survivable, weekly |
| Moltenus (P5-14) | `gameserver.moltenus.time` `0 0 22 ? * SUN` | cron: spawn boss 251045 in Reshanta for 1 h (CronJobService.java:35-59) | armed and **ported** | same | yes |
| **Legion dominion weekly calculation** (P5-11) | **none — the expression is hard-coded**, `0 0 9 ? * WED *` (CronJobService.java:70), so no profile can move it | cron: legion-dominion ranking, occupation, reward mails, `SM_LEGION_DOMINION_LOC_INFO` to every player (LegionDominionService.java:84-148) | **armed** (`CronJobService.cpp:88, 185-187`); every Wednesday 09:00 server time it reaches `LegionDominionService::startWeeklyCalculation` `AION_UNPORTED` (`LegionDominionService.cpp:60-61`) → one ERROR line | same | **no** — loud, survivable, weekly. **M5h's** (S-08, D13: m5h-plan.md:419, :460); after it lands the job is harmless but still broadcasts to every player, so G-04 keeps it out of the gate window |
| Idian depth portals (P5-14) | none | respawns two portal npcs every **3.6-18 s** (`Rnd.get(3600, 18000)` with `TimeUnit.MILLISECONDS`, CronJobService.java:98 — almost certainly meant seconds) | ported, running | same | yes (Java's churn, kept) |
| `SiegeService.onEnterSiegeWorld` | none — reached from `CM_LEVEL_READY` for worlds 210050000, 220070000, 400010000 (CM_LEVEL_READY.java:69-72, Player.java:1268-1273) | two packets | **`AION_UNPORTED` (`SiegeService.cpp:280-281`), reached by `CM_LEVEL_READY.cpp:96-99` with any profile**: entering Inggison, Gelkmaros or Reshanta throws out of the packet | same | **no** — first reachable when M5f makes the maps reachable; **M5f ports it** (T-05, m5f-plan.md:404, D8 at :366) |

**Cron jobs Java arms at boot with the shipped configuration: 68** — 22 siege (21 preparations + 1 agent fight), 1 hourly siege broadcast,
16 rift, 23 world raid, 2 vortex, 1 event (every 5 minutes) and 3 of `CronJobService` (Moltenus, Ahserion, the Wednesday calculation). Other
services arm their own (AbyssRankUpdateService, PeriodicInstanceManager, PlayerLimitService, the housing tasks, …); they run in every earlier
gate already and are outside M5i.

**Answer to "which start on their own today, and is it safe":** with the user's profile nothing of P5-12 runs except the harmless 5-minute
event cron, and with the shipped configuration the server refuses to start at `VortexService.initVortexLocations()` — both safe. The three
unsafe things are independent of any siege key: the Sunday Ahserion job (D6 keeps it), the hard-coded Wednesday 09:00 dominion job (M5h's
S-08 closes it) and the siege-world `CM_LEVEL_READY` hook (M5f's T-05 closes it). **After M5i the picture inverts**: every system starts at
boot and runs forever with no player online, and the bases cannot be switched off (D3).

### 2.2 Status by area (measured at HEAD)

`AION_UNPORTED` / `AION_PARTIAL` counted over the exact file set `chunks.py files` returns; "no file" counts Java classes with no `.cpp`/`.h`
under `src/` or `generated/`; Java lines by `wc -l` over `chunks.py java`.

| Area | Chunk | Sites | Partial | No-file classes (bodies) | Enum companions missing | Java LOC |
|---|---|---|---|---|---|---|
| `SiegeService` 26, `Siege` 14, `SiegeCounter` 5, `SiegeRaceCounter` 9 + 2 in the header, the five location classes 21, `ShieldService` 1 | P5-12a | 78 | 0 | `FortressSiege` (19), `ArtifactSiege` (6), `OutpostSiege` (7), `AgentSiege` (12), `MercenaryLocation` (8), `SiegeStartRunnable` (3), `SiegeException` (4) = **59** | `ArtifactStatus.getValue`, `AssaulterType` (2) | 3,097 |
| Balaur assault: `BalaurAssaultService` 7, `Assault` 5, `FortressAssault` 12, `ArtifactAssault` 5 | P5-12a | 29 | 0 | – | – | 456 |
| **P5-12a total** | | **107** | 0 | **59** | 3 | **3,553** |
| Bases: `Base` 25, `BaseLocation` 3, `BaseService` 9 | P5-12b | 37 | **1** (`BaseService.cpp:18`) | `CasualBase` 5, `SiegeBase` 5, `StainedBase` 9, `SiegeBaseLocation` 1, `StainedBaseLocation` 2, `BaseException` 4, `PanesterraBase` 7, `PanesterraArtifact` 2, `PanesterraFactionCamp` 4, `PanesterraBaseLocation` 1 = **40** | `BaseOccupier` (3) | 882 |
| Panesterra: `PanesterraService` 20, `AhserionRaid` 15, `PanesterraTeam` 8 | P5-12b | 43 | 0 | – | `PanesterraFaction` (2) | 845 |
| Rifts: `RiftService` 7, `RiftManager` 4, `RiftLocation` 6 | P5-12b | 17 | 0 | `RiftOpenRunnable` 2 | **`RiftEnum` (12)** — today a P4-11b stand-in (`ControllerStandIns.h:87-99`) | 894 |
| Vortex: `VortexService` 10, `DimensionalVortex` 7, `VortexLocation` 5 | P5-12b | 22 | 0 | `Invasion` 11 | – | 667 |
| World raids: `WorldRaidService` 3, `WorldRaid` 17 | P5-12b | 20 | 0 | `WorldRaidRunnable` 2 | – | 373 |
| Events: `Event` 15, `EventBuffHandler` 26 + 1 in the header, `EventService` 1, `Headhunter` 2 | P5-12b | 45 | 0 | – | – | 950 |
| Conqueror/protector: service 12, `CPBuff` 2 | P5-12b | 14 | 0 | – | – | 356 |
| **P5-12b total** | | **198** | **1** | **55** | 17 | **4,967** |

The header sites are the three Java generic methods written as templates: `SiegeRaceCounter::addToCounter` and `getOrderedCounterMap`
(`SiegeRaceCounter.h:57-59, 74-76`) and `EventBuffHandler::collectNRandomElements` (`EventBuffHandler.h:103-105`).

**Five of these sites are already claimed by earlier plans under leases** (rev 2, §3): `SiegeService::getSiegeIdByLocId` and
`onEnterSiegeWorld` by M5f (T-05), `SiegeService::cleanLegionId` by M5h (S-06) — so M5i's `SiegeService` share is **23** bodies — and
`ConquerorAndProtectorService::onLeaveLegion` and `resetLegionDominionRank` by M5h (S-06), so M5i's CP share is **10** + `CPBuff` 2.

### 2.3 The siege, end to end

| # | Step | Java | C++ today | Chunk |
|---|---|---|---|---|
| 1 | Boot: `SiegeService` constructor copies the data holder's maps and `SiegeDAO.loadSiegeLocations` sets race, legion, occupy count and faction balance (inserting the missing rows) | SiegeService.java:75-93 | ported (`SiegeService.cpp:56-74`) | P5-12a |
| 2 | Boot: `initSieges` — despawn every location's npcs, spawn PEACE for fortresses, outposts and standalone artifacts, load `config/schedule/siege_schedule.xml`, schedule `SiegeStartRunnable(fortress)` at the preparation cron (the siege time **minus 5 minutes**, minus 10 for Panesterra, `getPreparationCronString`) and `SiegeStartRunnable(agent)` at the siege time (logging "Scheduled siege of fortressID …" at DEBUG, SiegeService.java:132, 140), **start every standalone artifact's siege**, compute each fortress's next state, schedule the hourly broadcast | SiegeService.java:99-172, 697-716 | `AION_UNPORTED` (`:91`) | P5-12a |
| 3 | The hourly job: `updateNextStateUpdateTime`, `updateFortressNextState` (VULNERABLE if the earliest `SiegeStartRunnable` fire time is ≤ the next hh:00, found with `CronService.findNextFireTimes(SiegeStartRunnable.class)`; otherwise INVULNERABLE, which is also the field's default, SiegeLocation.java:33), then per online player `SM_FORTRESS_INFO(…, false)` ×18, `SM_FORTRESS_STATUS`, `SM_FORTRESS_INFO(…, true)` ×18 | SiegeService.java:158-171, 309-334 | unported; `SM_FORTRESS_STATUS.writeImpl` unported (`SM_FORTRESS_STATUS.cpp:14`); the C++ `CronService::findNextFireTimes` filters by the exact callable type (`CronService.h:113-115, 170-173`), so `SiegeStartRunnable` must be a named TaskStruct type | P5-12a, P4-16 |
| 4 | Cron fires: `checkSiegeStart` → agent: `startSiege`; fortress: `startPreparations` → schedule `startSiege` in **300 s**, reset to Balaur when `maxOccupyCount` is reached, Panesterra preparations for ids > 10000 | SiegeService.java:175-196 | unported | P5-12a |
| 5 | `startSiege` (synchronized): refuse a second start, `newSiege` → `FortressSiege` / `OutpostSiege` / `ArtifactSiege` / `AgentSiege`, `siege.startSiege()`, and unless endless schedule `stopSiege` after `siege_duration` s | SiegeService.java:198-218, 436-447; Siege.java:50-70 | unported; **the four subclasses have no C++ file** | P5-12a |
| 6 | `FortressSiege.onSiegeStart`: vulnerable, `SM_SIEGE_LOCATION_STATE` to the world, **`clearLocation`** (enemy players — staff too unless `gameserver.siege.ignore_staff_on_location_clear`, default false, SiegeConfig.java:61-62 — to their bind point, enemy kisks die), despawn, spawn SIEGE npcs, `initSiegeBoss` (exactly one `abyss_type="BOSS"` npc, else `SiegeException`), mercenary zones, balance buffs 8867-8884 or warnings for players inside, a faction-troop assault in 10-30 min **if the boss is level 65**, `BalaurAssaultService.onSiegeStart` | FortressSiege.java:65-96; FortressLocation.java:74-127 | unported; `FortressLocation::clearLocation` unported (`FortressLocation.cpp:115`) — its first blocker `TeleportService::moveToBindLocation` is ported (`TeleportService.cpp:320`), the second is `Kisk.getController().die()`, which is the ported `CreatureController` path | P5-12a |
| 7 | During the siege: the shield npc's AI raises `setUnderShield(true)` and broadcasts `SM_SHIELD_EFFECT` to the map; an enemy entering the fortress zone gets a `ShieldObserver` (sphere shield, `ShieldService.createShieldObserver`) and one entering a geo shield zone a `CollisionDieActor`; crossing an active shield kills (`CollisionDieActor.kill` → `PlayerReviveService.scheduleReviveAtBase(player, 2500, 0)`, which adds a `TaskId.TELEPORT` task at once, so the 500 ms `SM_DIE` never comes: PlayerReviveService.java:250-259, PlayerController.java:355-361) | ShieldNpcAI.java (A1); FortressLocation.java:51-63; SiegeShield.java:39-48; ShieldObserver.java; CollisionDieActor.java | `ShieldNpcAI` has no file (A1, phase 6); `createShieldObserver` unported (`ShieldService.cpp:65-68`); `scheduleReviveAtBase` unported (`PlayerReviveService.cpp:149`, P5-08); `SiegeShield`, `ShieldObserver`, `CollisionDieActor` ported | P5-12a, A1, P5-08 |
| 8 | AP during a siege: `AbyssPointsService.addAp(player, obj, value)` → `SiegeService.onAbyssPointsAdded` → the siege counter. Called from **every npc kill that rewards AP** (NpcController.java:235-240) | AbyssPointsService.java:25-31 | the siege variant is `AION_UNPORTED` (`AbyssPointsService.cpp:11`) and `NpcController.cpp:271-273` calls it — **M5d ports the other two `addAp` overloads but explicitly not this one** (m5d-plan.md:422) | P5-08 |
| 9 | The end, three ways: the boss dies (`AbstractSiegeProtectorAI.handleDied` → damage into the counter, `setBossKilled(true)`, `stopSiege`), the duration elapses, or a GM captures (`captureSiege`: add maxHp+1 race damage, boss killed, stop) | AbstractSiegeProtectorAI.java; SiegeService.java:220-265 | AI has no file (A1); service unported | A1, P5-12a |
| 10 | `FortressSiege.onSiegeFinish`: despawn, remove balance buffs, invulnerable, **`onCapture`** (world buff skill 12147-12158 to every online player of the winner race, new race and legion for fortress and artifact, occupy count 1, winner/loser system messages to every player, `broadcastUpdate`) **or `onDefended`** (occupy count +1 **with no cap**, defence world buff, `broadcastState`), faction balance ±1 by the owner's race, PEACE spawns, **rewards** (`sendRewardsToParticipants`: one `AbyssSiegeLevel.getLevelById` per reward grade, `MailFormatter.sendAbyssRewardMail`, `GloryPointsService.addGp`; legion GP and kinah), outpost states, `SiegeDAO.updateSiegeLocation`, `QuestEngine.onKill` for owners inside | FortressSiege.java:130-320; Siege.java:177-217; SiegeLocation.java:111-113 | unported; `MailFormatter::sendAbyssRewardMail` / `sendCustomAbyssDefeatRewardMail` unported (`MailFormatter.cpp:28, 38`) and **declined by M5c** (m5c-plan.md:219-220); `AbyssSiegeLevel` / `SiegeResult` have no companion (§2.7); `GloryPointsService::addGp` unported (`GloryPointsService.cpp:7`, M5d E-09); `PlayerService::getOrLoadPlayerCommonData` unported (`PlayerService.cpp:323, 327`, M5c M-02); `LegionService::addRewardHistory` unported (`LegionService.cpp:380`, M5h) | P5-12a + deps |
| 11 | Every change is announced: `broadcastUpdate` → `Influence.recalculateInfluence` → `SM_SIEGE_LOCATION_INFO(loc)` + `SM_INFLUENCE_RATIO` to every player; `captureSiege` adds its own after `stopSiege`, so a capture during a siege sends **two** pairs (FortressSiege.java:153, SiegeService.java:261) | SiegeService.java:524-532 | `Influence` ported; `SM_INFLUENCE_RATIO.writeImpl` unported (`SM_INFLUENCE_RATIO.cpp:14`); `SM_SIEGE_LOCATION_INFO` ported but reaches the unported `SiegeLocation::isCanTeleport` and the artifact/outpost/agent `getNextState` overrides (`SM_SIEGE_LOCATION_INFO.cpp:63-64`) | P5-12a, P4-16 |
| 12 | Login: `onPlayerLogin` sends `SM_INFLUENCE_RATIO`, `SM_SIEGE_LOCATION_INFO` (all 70 locations), `SM_AFTER_SIEGE_LOCINFO_475`, `SM_RIFT_ANNOUNCE(silentera of outposts 3111/2111)` (SiegeService.java:585-586); `CM_LEVEL_READY` in a siege world sends `SM_SHIELD_EFFECT` and `SM_ABYSS_ARTIFACT_INFO3`; `validateFortressZone` sends an enemy whose `lastOnline` is before the siege start to the bind point | SiegeService.java:569-605; PlayerEnterWorldService.java:200, 415-428 | enabled branch unported (`SiegeService.cpp:273-277`); `onEnterSiegeWorld` unported — **M5f's T-05**; `validateFortressZone` ported (`PlayerEnterWorldService.cpp:673-690`) | P5-12a |
| 13 | Artifacts: every standalone artifact runs an **endless** siege from boot; its protector's death (or a capture) finishes it, `onCapture` sets the race and legion, and `onSiegeFinish` **starts the next siege at once** (ArtifactSiege.java:42-60); `setInitialDelay` makes a fresh artifact refuse activation for **900 s** (ArtifactLocation.java:32-35); activation is `ArtifactAI` (root P5-05, 269 lines): two question windows, a 10 s channel, the item cost, a 13 s cast, the artifact skill | ArtifactSiege.java; ArtifactAI.java | unported / no file | P5-12a, P5-05 |

### 2.4 The world events, end to end

- **Bases.** `BaseService` builds one location per `base_location` (42: 11 CASUAL, 12 STAINED, 3 SIEGE, 4 PANESTERRA, 4 PANESTERRA_FACTION_CAMP,
  8 PANESTERRA_ARTIFACT) and starts 31 of them (BaseService.java:30-53). `Base.handleStart` spawns the FLAG, MERCHANT and SENTINEL groups of
  the occupier (`SpawnsData.getBaseSpawnsByLocId`, spawned through `VisibleObjectSpawner::spawnNpc`), then schedules outriders and the boss;
  after the boss, the assault cycle reschedules itself forever (Base.java:120-139). A boss kill (`BaseProtectorAI`, A1) or `//base capture`
  calls `BaseService.capture` → `stop` (cancel four tasks, despawn everything including pending respawns) → new occupier → `start` of a
  **new `Base` object**, and for stained bases re-evaluates the enhanced spawns (BaseService.java:112-142). `//base assault <id> <occupier>`
  spawns the ATTACKER group **of the occupier named**, refusing the base's own occupier (BaseCommand.java `assaultBase`). Nothing is
  persisted: bases return to their default occupier at every restart (no base DAO exists).
- **Rifts.** A cron job per `rift_schedule.xml` entry runs `RiftOpenRunnable` → `prepareRiftOpening` (without guards: a 50 % chance to skip
  outside Cygnea/Enshar, 1-4 random locations) → `openRifts` → guards + `RiftManager.spawnRift` (master and slave npcs with an `RVController`
  per instance) → `RiftInformer.sendRiftsInfo(worldId)`, and after `duration × 3540` s `closeAutoCloseableRifts` (RiftService.java:124-202,
  RiftOpenRunnable.java:20-27). Using a rift is `RVController.onDialogRequest` → `SM_QUESTION_WINDOW` → `CM_QUESTION_RESPONSE` → teleport.
  Every entry is at hh:00 (rift_schedule.xml), and no rift is in Reshanta.
- **World raids.** A cron job per schedule runs `WorldRaidRunnable` → `startRaid` → `WorldRaid.onWorldRaidStart`: a 60 s fixed-rate task
  that spawns the flag at minute 0, the vortex at 10, the markers at 25, announces at 29 and at **minute 30** despawns the vortex, spawns a
  random boss with AI `world_raid_aggressive` and schedules its despawn in 1 h; the boss's death stops the raid (WorldRaid.java:60-117). Its
  messages go only to players on the raid's map, and only for the first raid on a map within 30 s (WorldRaid.java:176-179,
  WorldRaidService.java:85-98). **Java bug, to be ported faithfully (D8):** `stopWorldRaid` before minute 30 calls `despawnNpcs(flag, vortex,
  boss)` with nulls and throws a `NullPointerException` at WorldRaid.java:122, and `onWorldRaidFinish` never cancels `preparationTask`
  (104-107), so the orphaned task still spawns a vortex, markers and a boss that no service tracks and nobody despawns.
- **Events.** `EventService.start` computes the active events (not disabled, in their date window), starts the new ones and arms the
  5-minute cron (EventService.java:45-52, 132-163). `Event.start` reloads config properties, spawns event spawns, starts an inventory-drop
  task, activates surveys, creates the `EventBuffHandler`, and replays login + enter-map for every online player (Event.java:78-129). The
  permanent "Beyond Aion Server Buffs" has four buffs (custom_events.xml:3-27): 10408 on 0.1 % of PvE kills, 10549 on 2 % of PvP kills,
  **18141 (Max HP +1500) and 21258 (attack +15 %) restricted to group and alliance instances with a team of at most 70 %**
  (custom_events.xml:15-19) — `isAllowedOnCurrentMap` accepts only instances whose `maxPlayers` is 2-24 and every world map's main instance
  has `maxPlayers` 0 (Buff.java:65-69, WorldMap.java:33-35), and `isAllowedTeamSize` refuses a player with no team (EventBuffHandler.java:246-
  280), **so no player in the open world ever gets them** — and one pool buff of 10821/10823/10824 on one random day per month, which is
  the only buff a solo player on a world map can receive at map entry. Buff state (the pool and the allowed days) is loaded from and stored
  through `EventDAO` (ported); a stored row whose pool is a subset of the buff's skills and whose day count matches is used instead of a new
  roll (EventBuffHandler.java:52-70), which is what lets the gate seed it (§10.2 C1).
- **Conqueror/protector.** Rank and buffs from PvP kills in eight maps; a 10-minute decrease timer; `CM_SHOW_MAP(0)` intruder scan; the
  protector rank in an occupied legion-dominion zone (ConquerorAndProtectorService.java:103-122). With no PvP in any gate the live paths are
  the timer, `onEnterMap` returning early, and zone entry in a dominion zone.
- **Vortex.** Two weekly cron jobs; `Invasion.start` spawns the vortex rift and INVASION npcs, invaders pass through `RVController`, and each
  side is gathered into an **alliance** (`PlayerAllianceService.createAlliance`, Invasion.java:62-89) — M5g territory.
- **Panesterra.** `onEnterPanesterra`, the Panesterra fortress sieges (all four are commented out of `siege_schedule.xml`, :69-82), team
  elimination from faction-camp captures, and Ahserion's Flight (§2.1, D6).
- **Legion Dominion (Stonespear Reach)** — handed to M5i by M5h (m5h-plan.md:381, :408 D2, :521 O-01), **except the Wednesday weekly
  calculation, which M5h ports itself** (S-08, D13). What remains: `join` (from `CM_LEGION` 0x10 → `LegionService.joinLegionDominion`,
  CM_LEGION.java:155, LegionService.java:1148-1160), `onFinishInstance` (only from the phase-6 `StonespearReachInstance`),
  `isInCalculationTime` (Wednesday 08:00-10:59 server time; also a dialog condition, DialogService.java:355-360), `openInvasionRift` (opens a
  rift through `RiftService.openRifts` and schedules `closeRifts` after `duration × 3540` s, LegionDominionService.java:170-192), the ranking
  packet, and `LegionDominionIntruderUpdateTask` — which **nothing in Java ever starts** (its `getInstance()` has no caller outside its own
  file).

### 2.5 Timers M5i starts, with who owns their cycle rows

`cycles.toml` already resolves most of them: the four `Base` task lambdas and `StainedBase`'s as "one-shot task: releases its captures when it
runs or is cancelled" (`cycles.toml:122-126`), the siege locations' creature/player maps and shield observers as java-hooks (185-189), the
vortex location fields (249-254), `AhserionRaid$1`, `Assault@L43`, `FortressAssault@L59`, `DimensionalVortex`, `Invasion` and every `WorldRaid`
field (274-292), and `RVController.passedPlayers`/`slave` (70-71). **Missing rows** (item S-08): `FortressSiege@L87` (the faction-troop task),
`AgentSiege@L52` (the self-rescheduling start chain), `SiegeService@L186` and `@L217` (preparation and end), the hourly broadcast job, the
`EventService@L51` cron (the C++ already pins `this`, `EventService.cpp:68`), `Event@L102` (inventory drop), `CronJobService`'s jobs, the
`ArtifactAI` tasks and the `LegionDominionService@L189` rift close. **One existing row is wrong** (S-08 corrects it): `cycles.toml:286`
`WorldRaid$1#this` = "java-hook: WorldRaid cancels preparationTask (WorldRaid.java:61-62, 92-93)" — an early stop never cancels it
(`onWorldRaidFinish`, WorldRaid.java:104-107, D8); the task is cut only by its own minute-30 branch or at shutdown. **One holder no row can
cut**: with `gameserver.siege.assault.enable=false` a `FortressAssault` started by `//siege assault` stays in
`BalaurAssaultService.fortressAssaults` forever (D4), holding its `Assault.boss` (Assault.java:31-36).

### 2.6 What turning M5i on wakes (lesson 2)

Every entry point the milestone switches on, traced to the first unported or partial body. "M5i" marks bodies this plan ports; "dep" marks
bodies an earlier milestone is assumed to port (§3).

| Entry point | First unported / partial body reached | Owner |
|---|---|---|
| **Boot**, sieges on | `SiegeService::initSieges` (`:91`); then `spawnNpcs`/`deSpawnNpcs`, `newSiege` → the four no-file subclasses, `ArtifactLocation::isStandAlone` | M5i |
| Boot: 1,454 PEACE siege npcs spawn | their AIs: 2,581 siege npc ids use `artifact_protector` (147), `artifact` (133), `siege_cannon` (78), `fortressgate` (72), `simple_abyssguard` (50), `portal` (47), `fortress_protector` (42), `mercenary` (36), `siege_gaterepair` (36), `siege_shieldnpc` (35), `spring` (29), `siege_mine` (18), `portal_dialog` (11) besides `aggressive`/`general`/`noaction`. **`portal` and `portal_dialog` are M5f's** (`PortalAI`, `PortalDialogAI` under an A1 lease, m5f-plan.md:391 I-01) and **`simple_abyssguard` is M5d's** (`AbyssGuardSimpleAI`, m5d-plan.md:506 D5, :590 A-01); the rest have no C++ file; with `gameserver.dev.missing_ai_handlers=warn` each name gets a DummyAI and one WARN (`AIEngine.cpp:103, 162-164`) | M5i ports 7 in stage 1 (D5); `fortressgate` and nine others stay DummyAI; dep M5d, M5f |
| Boot: siege and base npcs are **quest givers** | m5d-plan.md:21-22 and :157 count 310 XML quests whose giver exists only in siege, instance, base, vortex or Ahserion spawns; M5i's spawns make the siege and base ones reachable | dep M5d |
| Boot, bases (no switch) | `BaseService` ctor `AION_PARTIAL` (`:18`), then `start`/`newBase` → the no-file subclasses → `Base::handleStart` → `spawnBySpawnHandler` | M5i |
| Boot: base npcs | AIs `flag` (132 ids, root), `base_protector` (84, A1), `base_flag` (11, A1), `portal_dialog` (6, M5f's) | M5i ports 3 (D5) |
| Boot, rifts / world raids / events | `RiftService::initRifts` (`:27`), `WorldRaidService::initWorldRaids` (`:37`), `EventService::startOrStopEvents` (`:213`) → `Event::start` → `EventBuffHandler` ctor | M5i |
| Event buffs | `SkillEngine::applyEffectDirectly(skillId, …, duration, forceType)` (ported) → **`XPBoostEffect::calculate` unported** (`XPBoostEffect.cpp:8`, P5-04) for 10821/10825; `DRBoostEffect`, `SkillXPBoostEffect`, `APBoostEffect` are data-only on the ported `BufEffect`; `StatupEffect` (M5b-2 subset) for 18141/21258, which are applied only inside group/alliance instances (§2.4) | M5i (1 body, lease) |
| Event start | `ItemService::addItem` (inventory drops, `ItemService.cpp:41-65`), `QuestService::startQuest` (event quests, `QuestService.cpp:219, 223`), `SpawnEngine::spawnEventSpawns` | dep M5b-3, dep M5d |
| **Enter world**, sieges on | `SiegeService::onPlayerLogin` (`:276`) → `SM_INFLUENCE_RATIO` (`:14`), `SiegeLocation::isCanTeleport`, `OutpostLocation::isSilenteraAllowed`, the `getNextState` overrides | M5i |
| **A staff login** (`access_level ≥ 1`; the gate's gm has 9) | `GMService::onPlayerLogin` → the `LOGIN_EXECUTE_COMMANDS` arm, `AION_UNPORTED` whenever the list is non-empty (`GMService.cpp:59-68`, P4-05) — and Java's default and the shipped value are non-empty (`AdminConfig.java:77`, `AdminConfig.cpp:28`, `admin.properties:101`); then, for access ≥ `REVISION_INFO_ON_LOGIN` (default 9, `AdminConfig.cpp:29`, `admin.properties:105`), `AION_PARTIAL("VERSION_INFO for staff logins…")` (`PlayerEnterWorldService.cpp:524-528`, P5-00) | both profiles set `execute_commands` empty (D2; the arm itself is m5j-plan.md:580 D4's, item I-02 at :605); **Z-06 closes the partial** unless M5j's K-02 did |
| `CM_LEVEL_READY` in 210050000/220070000/400010000 | `SiegeService::onEnterSiegeWorld` — **any profile** | **dep M5f** (T-05); fallback Z-03 |
| Enter world in an invasion world with a startable `KillInWorld` / `MonsterHunt` quest | `onEnterWorldEvent` → `searchOpenRift` → `RiftService.getRiftLocations()` (non-empty once rifts are on) → `RiftLocation::getWorldId` (`RiftLocation.cpp:21-22`) (KillInWorld.java:127-146, MonsterHunt.java:253-266) | M5i (R-01) + dep M5d (the templates) |
| An npc teleport (M5f) to a fortress teleloc | `SiegeService::getSiegeIdByLocId` (M5f's T-05) → `SiegeLocation::isCanTeleport` (`SiegeLocation.cpp:54-55`) once sieges are on (TeleportService.java:81-85) | M5i (S-05) |
| Zone entry into a fortress while shielded | `ShieldService::createShieldObserver` (`:67`) — **also for npcs**: an enemy-race npc spawned into a shielded fortress zone (assault waves, faction troops) throws inside `ZoneInstance::onEnter`, which is the "Error during spawn" shape of F-1 | M5i |
| Zone entry, faction balance ≠ 0 | `FortressLocation::checkForBalanceBuff` (ported) → `EffectController::hasAbnormalEffect` (ported) and `applyEffectDirectly(8867..8884)` → `StatupEffect` (M5b-2 subset) | dep M5b-2 |
| Zone entry or exit in a **legion-dominion zone**, `cp.enable=true` | `ConquerorAndProtectorService::onEnterZone` / `onLeaveZone` (PlayerController.java:197, 209) → `isOccupiedLegionDominionZone` (`ConquerorAndProtectorService.cpp:131-132`), and the enter arm itself (`:108-111`) | M5i (E-02) |
| Geo on: every ray test against a SHIELD `DespawnableNode` | ported, but **its behaviour changes**: with sieges off `GeoCallbacks::getSiegeShieldState` finds no location and the node always collides; with sieges on and no shield up it answers 0, no collision (`DespawnableNode.cpp:84-103`) — line of sight, pathing and geo z around the fortresses change the day sieges go on | ported; the geo gates (G-06, G-07) |
| **Npc kill with AP reward** (abyss npcs, siege npcs, guards) | `AbyssPointsService::addAp(Player&, VisibleObject&, int)` (`:11`) inside `NpcController::doReward`'s swallowing `try` — the m5b-client-session S-1 shape | M5i |
| Shield kill, siege-zone death | `PlayerReviveService::scheduleReviveAtBase` (`:149`) | M5i |
| Siege end rewards | `AbyssSiegeLevel.getLevelById` and `SiegeResult.getId` (no companion, §2.7), `MailFormatter` ×2, `GloryPointsService::addGp`, `PlayerService::getOrLoadPlayerCommonData` ×2, `LegionService::addRewardHistory`, `SystemMailService::sendMail` (`SystemMailService.cpp:10`) | M5i (companions, mail ×2) + dep M5c, M5d, M5h |
| Npc death in a siege | `NpcAI.ask(ALLOW_RESPAWN)` → `SiegeService::isRespawnAllowed` (ported, M5b-1 C-05) — now with a real `getSecondsUntilNextFortressState()` | ported |
| Artifact dialog | `CM_SHOW_DIALOG` → `ArtifactAI` → `SM_QUESTION_WINDOW` → `CM_QUESTION_RESPONSE` | dep M5c (packets), M5i (AI) |
| Rift use | `CM_SHOW_DIALOG` → `RVController::onDialogRequest` (ported) → `CM_QUESTION_RESPONSE` → `TeleportService::teleportTo` | dep M5c, M5f |
| Vortex participation | `PlayerAllianceService::createAlliance/addPlayer/removePlayer` | dep M5g |
| Fortress owned by a legion | `LegionService::getLegion` (ported), legion warehouse kinah, legion history, legion emblem in `SM_SIEGE_LOCATION_INFO` | dep M5h |
| Cron (Sunday 18:50) | `PanesterraService::startAhserionRaid` | deferred (D6) |
| Cron (Wednesday 09:00, hard-coded) | `LegionDominionService::startWeeklyCalculation` (`LegionDominionService.cpp:60-61`) | **dep M5h** (S-08, D13) |
| Dialog condition `TARGET_LEGION_DOMINION` (DialogService.java:355-360) | `LegionDominionService::isInCalculationTime` (`LegionDominionService.cpp:68-69`) | M5i (LD-01) + dep M5c (dialogs) |
| `CM_LEGION` 0x10 (CM_LEGION.java:155) | `LegionService::joinLegionDominion` (left unported by m5h-plan.md:408 D2) → `LegionDominionService::join` (`:52-53`) → `LegionDominionLocation::join` | M5i (LD-01) + dep M5h (`CM_LEGION`) |
| **Admin commands** (the gate's control plane) | `CM_CHAT_MESSAGE_PUBLIC` (no file; **M5g's K-04**) → `ChatProcessor::handleChatCommand` (ported; the command check comes before `canChat`, CM_CHAT_MESSAGE_PUBLIC.java:48-52) → `ChatCommand::run` (`ChatCommand.cpp:67`, unported) → `AdminCommand::validateAccess` / `process` (`AdminCommand.cpp:20, 24`) → the command classes (no file, C1, phase 6) | dep M5g; M5i stage 0 (Z-02) + C1 leases |

### 2.7 The bodies no site count reaches (lesson 1)

Java methods compared with C++ declarations for every class of both chunks and for the cross-chunk classes of §2.6 (a throw-away script over
the Java sources and the `src/` + `generated/` headers; false positives — generated `.xml.inc` getters, anonymous-class `run`/`accept`,
exception constructors — removed by hand).

| Kind | Classes | Bodies | Java LOC |
|---|---|---|---|
| **No C++ file**, P5-12a | `FortressSiege` 19, `AgentSiege` 12, `MercenaryLocation` 8, `OutpostSiege` 7, `ArtifactSiege` 6, `SiegeException` 4, `SiegeStartRunnable` 3 | **59** | 957 |
| **No C++ file**, P5-12b | `Invasion` 11, `StainedBase` 9, `PanesterraBase` 7, `CasualBase` 5, `SiegeBase` 5, `BaseException` 4, `PanesterraFactionCamp` 4, `PanesterraArtifact` 2, `StainedBaseLocation` 2, `RiftOpenRunnable` 2, `WorldRaidRunnable` 2, `SiegeBaseLocation` 1, `PanesterraBaseLocation` 1 | **55** | 568 |
| **Enum companions** of the chunks (generated enums whose Java methods have no declaration; precedent `model/siege/SiegeRaceInfo.h`) | `RiftEnum` 12 (+ its P4-11b stand-in to delete), `BaseOccupier` 3, `AssaulterType` 2, `PanesterraFaction` 2 (P5-14 keeps a local mapping, deviations/P5-14.md), `ArtifactStatus` 1 | **~20** | – |
| **Enum companions on the siege-end path, outside the chunks** (rev 2) | `AbyssSiegeLevel` 2 (`getId`, `getLevelById`), `SiegeResult` 1 (`getId`): generated as `generated/aion/gameserver/services/mail/{AbyssSiegeLevel,SiegeResult}.h` with no companion, and no `getLevelById` exists anywhere in `src/` or `generated/` (P5-09). `sendRewardsToParticipants` calls `getLevelById` once per reward grade on **every** fortress or agent siege end, participants or not (Siege.java:186-190) | **3** | 52 |
| Undeclared methods in existing headers of the two chunks | none beyond the above (the abstract hooks of `Siege` and `Base` are declared: `Siege.h` `onSiegeStart`/`onSiegeFinish`/`isEndless`/`onAbyssPointsAdded`) | 0 | – |
| **No C++ file outside the chunks, on the path** | 5 admin commands `SiegeCommand`, `BaseCommand`, `Rift`, `WorldRaid`, `VortexRaid` (C1, 445 lines; `Ahserion`, 42 lines, is deferred with the raid, D6); A1 `SiegeNpcAI`, `AbstractSiegeProtectorAI`, `ArtifactProtectorAI`, `FortressProtectorNpcAI`, `GuardianGeneralAI`, `SiegeRaceProtectorAI`, `ShieldNpcAI`, `BaseProtectorAI`, `FlagBaseNpcAI`, `WorldRaidAI` (383 lines); root P5-05 `ArtifactAI`, `FlagNpcAI`, `RiftProtectorAI`; rev 2: `CM_SHOW_MAP` (3, 44 lines, P5-16), `CM_LEGION_DOMINION_REQUEST_RANKING` (3, 37 lines, P5-15), `LegionDominionIntruderUpdateTask` (3, 75 lines, P5-11) | ~70 | ~1,500 |
| `AION_UNPORTED` / `AION_PARTIAL` outside the chunks, on the path | `ChatCommand` 7, `AdminCommand` 2 (P5-14), `SM_INFLUENCE_RATIO`, `SM_FORTRESS_STATUS` (P4-16), `AbyssPointsService` siege variant, `PlayerReviveService::scheduleReviveAtBase` (P5-08), `MailFormatter` ×2 (P5-09), `XPBoostEffect::calculate` (P5-04); rev 2: `LegionDominionService` 4 + `LegionDominionLocation` 3 + `LegionService::joinLegionDominion` 1 (P5-11; the other five are M5h's S-08), `ApExtractAction` 5 (P5-07, m5c-plan.md:403), the staff `VERSION_INFO` partial (P5-00) | ~30 | ~650 |
| **Taken by earlier plans (rev 2; not counted above)** | `SiegeService::getSiegeIdByLocId`, `onEnterSiegeWorld` (M5f T-05); `SiegeService::cleanLegionId`, `ConquerorAndProtectorService::onLeaveLegion`, `resetLegionDominionRank` (M5h S-06); `CM_CHAT_MESSAGE_PUBLIC` 8 (M5g K-04); `AbyssGuardSimpleAI` ~6 (M5d A-01) | −19 | – |

**Total: 305 sites + 114 no-file + ~20 companions = ~440 in the chunks, of which 5 are M5f's and M5h's → ~435; ~105 outside (~70 no-file
+ ~30 unported + 3 companions); ~540. ~20 are deferred (`AhserionRaid` 15, `startAhserionRaid`/`stopAhserionRaid`, the `Ahserion` command),
so M5i ports ~520.** Against the roadmap's 302 that is 1.79×.

### 2.8 Client packets (lesson 4)

188 Java `CM_*` classes, 42 with a C++ header: **146 have no C++ file** (measured today; the task's 147 predates M5b-2's `CM_CASTSPELL` /
`CM_REMOVE_ALTERED_STATE`). **M5i must add two of them — `CM_SHOW_MAP` and `CM_LEGION_DOMINION_REQUEST_RANKING` — and a third,
`CM_CHAT_MESSAGE_PUBLIC`, only if M5g, M5h and M5j all left it.**

| Packet | Java | Status | Why | Need |
|---|---|---|---|---|
| `CM_CHAT_MESSAGE_PUBLIC` | 155 lines; `//` and `.` go to `ChatProcessor.handleChatCommand` **before** `canChat`, the rest to eight chat-type arms (CM_CHAT_MESSAGE_PUBLIC.java:39-151) | **dep M5g**: K-04 ports it whole with `canChat` (W-01) and `PlayerChatService::logMessage`, `isFlooding` and all five `ChatBanService` bodies (W-03) (m5g-plan.md:489 D14, :551, :559, :561); fallbacks M5h P-03 (m5h-plan.md:469) and M5j K-05 (m5j-plan.md:619). **Z-01 only if all three are missing at branch time**, and then with `canChat`, `logMessage` ×2, `isFlooding` and `ChatBanService` 5 (`canChat` calls `isBanned`/`getBanMinutes`/`banPlayer`, `ChatBanService.cpp:7-25`) | the gate's and the user's only way to start a siege, capture a base, open a rift or start a raid (D2) | **R** (dep) |
| **`CM_SHOW_MAP`** | 44; action 0 = CP intruder scan, action 1 a no-op (CM_SHOW_MAP.java:31-42) | **M5i adds it (E-04, D15)**; settles m5j-plan.md:314 ("M5j unless M5i / M5f take their O") — M5j's S-12 (:647) keeps only the PvP half | `ConquerorAndProtectorService.intruderScan` is in E-02; a real client sends the packet from the map window | **R** |
| **`CM_LEGION_DOMINION_REQUEST_RANKING`** | 37 | **M5i adds it (LD-01, D14)**; m5h-plan.md:346 hands it over | the Stonespear Reach ranking window | **R** |
| `CM_UPGRADE_ARCADE` | 59 | not in M5i | `gameserver.event.arcade.enable` is false by default | O |
| `CM_SHOW_DIALOG`, `CM_DIALOG_SELECT`, `CM_QUESTION_RESPONSE`, `CM_CLOSE_DIALOG` | – | **assumed from M5c stage 0** (m5c-plan.md; m5d-plan.md:629) | artifact activation, rift entry, siege teleporters, mercenaries, the vortex defender invitation | dep |
| `CM_TELEPORT_SELECT`, `CM_MOVE_IN_AIR` | – | assumed from M5f | flying to Reshanta or Inggison with a real client; the gate seeds positions instead | dep |
| `CM_INVITE_TO_GROUP` and the alliance packets | – | assumed from M5g | vortex alliances (stage 3) | dep |
| `CM_LEGION` | – | assumed from M5h | legion-owned fortresses; the 0x10 arm reaches LD-01's `joinLegionDominion` | dep |
| `CM_ABYSS_RANKING_PLAYERS` / `_LEGIONS` | – | **M5j J5** (m5j-plan.md:310, item S-05 at :640); m5h-plan.md:382, :522 O-02 sends `_LEGIONS` to M5i — declined, §3.1 | the Abyss ranking window | O |

**Server packets: none new**; two `writeImpl` bodies (§1). The gate needs independent decoders (G-03).

### 2.9 How a gate drives a siege without waiting for real time

| Mechanism | What it can prove | What it cannot | Verdict |
|---|---|---|---|
| **Admin commands** through `CM_CHAT_MESSAGE_PUBLIC` from a GM account (`//siege start|stop|capture|assault`, `//base start|stop|capture|assault`, `//rift open|close`, `//worldraid start`, later `//vortexraid`) | every state change, spawn, broadcast, reward, persistence and restart, in the real process, in seconds; the same tool the user gets for the real-client session | that the **cron jobs** are armed or fire at the right wall-clock time | **the gate's control plane** (D2) |
| Login-time next-state fields: `SM_SIEGE_LOCATION_INFO` carries each location's `nextState`, `SM_INFLUENCE_RATIO` and `SM_FORTRESS_STATUS` carry `getSecondsUntilNextFortressState()`; the oracle computes both | that `updateNextStateUpdateTime` ran (the seconds), the influence formula, and the artifact/outpost/agent `getNextState` overrides (1 at boot) | **that `initSieges` armed the schedules.** `updateFortressNextState` sets VULNERABLE only when a preparation falls before the next hh:00 (SiegeService.java:309-320), the field defaults to 0 (SiegeLocation.java:33), and every preparation is one evening slot on listed weekdays, so an armed and an unarmed server send `nextState` 0 for all 18 fortresses in every hour but the 22 pre-preparation hours — which G-04's window excludes | used (X1, X2); **arming is T-02's alone** |
| Asserting the 21 + 1 "Scheduled siege of fortressID …" / "Scheduled agent fight …" lines (SiegeService.java:132, 140) | arming in the real process | – | rejected: they are DEBUG lines, and the C++ logging has a root level only (`Logging.h:65-66`; `logback.xml` is not read, `Logging.h:34`), so seeing them means a DEBUG root for the whole run |
| **`DeterministicExecutor` + `ManualClock`** in-process (`CronService` EXECUTOR driver fires "exactly" on `ManualClock::advance`, `CronService.h:97-99, 108-109`; `runtime/sched/Clock.h:34-54`) | the arming (T-02), the timelines: preparation → +300 s start → duration end; the hourly flip; world raid minutes 0/10/25/30 + 1 h; base cycles; rift auto-close; event date boundaries; artifact cooldown | the wiring of `main` (the real cron thread) | used (§10.6, T-01..T-14) |
| A 65-minute soak that crosses at least one hh:00 | the hourly siege broadcast and the hourly rift jobs firing in the real process | the weekly schedules | **offered to the user** (D17: the standing resource rule forbids a soak without asking) |
| A C++-only clock offset or time-scale option | – | would shift DB timestamps, keepalives and every other gate's assumptions; touches the frozen runtime | rejected |
| A C++-only `gameserver.dev.schedule_dir` (the three schedule files are read from fixed relative paths, SiegeSchedules.java:56, RiftSchedule.java:49, WorldRaidSchedules.java:74) | cron wiring in real time — but a fortress still needs the 300 s preparation delay | – | rejected: a deviation that buys one row the soak already covers |
| Choosing `gameserver.timezone` so that "now" is just before a siege | – | offsets span 26 hours, the schedules span a week | rejected |

---

## 3. What this plan assumes the earlier milestones deliver

Each work item in §5 carries a **Dep** column naming the milestone it relies on; **re-verify every row at branch time** — the sibling plans
were still being revised while this one was (m5f at 23:20, m5h at 23:27, m5g at 23:29, m5j at 23:30; rev 2 cites those versions), and the
bodies other plans take (marked **taken**) must be checked in the tree before M5i's lanes start.

| Milestone | Assumed delivered | Used by | If it is missing |
|---|---|---|---|
| **M5b-2** abilities (stage 1 committed through `760e8ab5c`; its gate stage pending) | `SkillEngine::applyEffectDirectly` (all four overloads, incl. duration + `ForceType`), `EffectController::hasAbnormalEffect`/`removeEffect`, the 34 + 4 effect classes incl. `StatupEffect`, `ShieldEffect`, `SnareEffect`, `BufEffect`; `SkillDecoders` for `SM_ABNORMAL_STATE`/`SM_STATS_INFO` | world buffs 12074-12158, balance buffs 8867-8884, outpost buffs 12119/12120, event buffs, artifacts 1135/1143 (12042) and 1213/1223/1233/1243 (12089) | capture/defence throws inside `onSiegeFinish` — the siege half-finishes inside `SiegeService.stopSiege` |
| **M5b-3** loot and items | `ItemService::addItem`, `Inventory::decreaseByItemId`, the item packets, item use | event inventory drops, artifact activation cost (188020000/188020001/188020002), `ApExtractAction` (X-06) | W: event drops, artifact activation, AP extraction |
| **M5c** vendors and economy | `CM_SHOW_DIALOG`, `CM_DIALOG_SELECT`, `CM_QUESTION_RESPONSE`, `CM_CLOSE_DIALOG`, `AIActions.addRequest`, `DialogService` (the `TARGET_LEGION_DOMINION` condition); `PlayerService::getOrLoadPlayerCommonData` (M-02); `SystemMailService::sendMail` | artifact dialogs, rift use, reward mails, artifact/outpost winner names | W: X12 becomes an in-process test; reward mails W |
| **M5d** quest engine | `QuestEngine::onKill` (ported), `QuestService::startQuest`, `AbyssPointsService::addAp` ×2 + `onRankChanged`, `GloryPointsService::addGp` (E-09); **taken: `AbyssGuardSimpleAI`** (`simple_abyssguard`, m5d-plan.md:506 D5, :590 A-01), which 50 siege npc ids use | siege-end quest kill, agent-fight quests 13744/23744, event quests, AP and GP rewards, `ApExtractAction`; the siege guards | W: GP rewards and quest hooks; the siege `addAp` variant still needs `AbyssRank::addAp` (ported); without A-01 the siege guards stay DummyAI (as today) |
| **M5e** training | nothing | – | – |
| **M5f** travel | `TeleportService::teleportTo` (ported), flight paths, `CM_TELEPORT_SELECT`, `CM_MOVE_IN_AIR`, `PortalAI` and `PortalDialogAI` (A1 lease, m5f-plan.md:391 I-01); **taken: `SiegeService::getSiegeIdByLocId` and `onEnterSiegeWorld`** under a P5-12a lease (T-05, m5f-plan.md:404; D8 at :366) | real-client travel to Reshanta/Inggison/Gelkmaros, rift use, X4, every npc teleport, 58 siege portal npcs | **Z-03** ports both bodies in stage 0 (fallback) |
| **M5g** groups | `PlayerAllianceService` create/add/remove, `TemporaryPlayerTeam`, team damage lists, the vortex and rift team arms (m5g-plan.md:609 O-04 leaves them to M5i's V-01); **taken: `CM_CHAT_MESSAGE_PUBLIC` whole** with `canChat`, `logMessage`, `isFlooding` and `ChatBanService` 5 (K-04, D14, W-01, W-03: m5g-plan.md:489, :551, :559, :561) | the gate's control plane (every `//` command), vortex alliances (stage 3), `BaseProtectorAI` team killer, event ENTER_TEAM buffs | M5h P-03 or M5j K-05; else **Z-01** (fallback) |
| **M5h** legion and housing | `LegionService` (history, warehouse kinah, brigade general, `getLegion`), the legion-dominion fields of `Legion`, `LegionDAO::storeLegion`, legion emblems, `CM_LEGION`; **taken: `SiegeService::cleanLegionId`, `ConquerorAndProtectorService::onLeaveLegion`, `resetLegionDominionRank`** (S-06, m5h-plan.md:458; leases :434); **taken: the Legion Dominion weekly calculation** (`startWeeklyCalculation`, `updateLegionOccupation`, `LegionDominionLocation::getLegionRanking`, `getRewards`, `reset`; S-08, D13: m5h-plan.md:419, :460) | legion-owned fortresses, legion GP/kinah rewards, artifact permissions; LD-01; the Wednesday cron | S-01 and E-02 take the three S-06 bodies back; **LD-01 takes the five S-08 bodies** (+ its T-13 cases) |
| **M5j**, only if the user takes m5j-plan.md:577 D1 (its stage 0 right after M5b-2's gate) | K-01 `ChatCommand`/`AdminCommand`, K-02 the staff `VERSION_INFO` partial, K-05 `CM_CHAT_MESSAGE_PUBLIC` (m5j-plan.md:615-619); and, whatever D1 says, **D4 / I-02 — `GMService::onPlayerLogin`'s execute-commands arm "in the next wave"**, which m5j calls a prerequisite of M5i's gate (m5j-plan.md:580, :605, A-I4 at :87) | Z-02, Z-06 | stage 0 as planned. **The gate does not depend on I-02**: with the profiles' empty `execute_commands` (D2) the arm is skipped whether or not it is ported, and the gm stays visible and vulnerable |

### 3.1 What the sibling plans hand to M5i, and what this plan does with it

M5j's revision of 23:30 read rev 1 of this plan and planned **fallback items** for what rev 1 left unowned (m5j-plan.md:360-373, its "deferrals
nobody accepts"): S-13 Legion Dominion (:648), S-09 with `ApExtractAction` (:644), S-12 the PvP half **with `CM_SHOW_MAP`** (:647), S-05 the
ranking packets (:640). Rev 2 takes three of them, so **M5j's S-13, the `ApExtractAction` part of S-09 and the `CM_SHOW_MAP` part of S-12
become void** if this revision is taken (M5j branches after M5i and re-verifies; the integrator records it in both review sections).

| From | What | Decision | M5j's fallback |
|---|---|---|---|
| m5h-plan.md:381, :408 D2, :521 O-01 | Legion Dominion less the weekly calculation: `LegionDominionService` 4 (`join`, `onFinishInstance`, `isInCalculationTime`, `openInvasionRift`), `LegionDominionLocation` 3 (`join`, `store`, `updateRanking`), `LegionDominionIntruderUpdateTask` (3, no file), `LegionService::joinLegionDominion`, `CM_LEGION_DOMINION_REQUEST_RANKING`, `LegionDominionPortalAI` | **taken as LD-01** (D14), 14 bodies, **without `LegionDominionPortalAI`** (A1, 122 lines: the entrance to the phase-6 `StonespearReachInstance`; O-06) | S-13 (:648) void; its `LegionDominionService`/`LegionDominionLocation` "6 + 6" also still counts M5h's five S-08 bodies |
| m5h-plan.md:382, :522 O-02 | `CM_ABYSS_RANKING_LEGIONS`, `AbyssRankingCache` | **declined**: ranking windows are M5j's J5 (m5j-plan.md:310) with `_PLAYERS` (D16) | S-05 (:640) keeps it |
| m5c-plan.md:403 | `ApExtractAction` (5) | **taken as X-06**, under a P5-07 lease, unit-tested only | S-09 (:644) drops it |
| m5g-plan.md:609 O-04 | the vortex and rift team arms (`VortexService` 3, `RVController` removals) | **taken inside V-01** (stage 3) | – |
| m5g-plan.md:610 O-05 | the PvP half of `PvpService::doReward` (team kill counting, AP split), "M5i (m5j-plan.md A-I2)" | **declined**: no M5i case or checklist step has one player kill another (the shield kill takes the "no player damage" arm, `PvpService.cpp:65-67`) (D16) | S-12 (:647) keeps it — m5j's A-I2 (:85) now plans it there |
| m5f-plan.md:204, :345, :478 O-03 | "M5g / M5i": the scored and registered group instances (`InstanceScore`, `PeriodicInstanceManager` 9, `PvPArenaService`), recall, `StaticDoorService` | **declined**: instance machinery, not world events (D16) | m5j-plan.md:78 A-F4: S-07, S-11, X-01, L-03 |
| m5j-plan.md:84 A-I1 | P5-12a/b, `SM_FORTRESS_STATUS`, `SM_INFLUENCE_RATIO`, `scheduleReviveAtBase`, the chat-command control plane, `ArtifactAI`, `FlagNpcAI`, `RiftProtectorAI` | **all taken** (X-02, X-04, Z-02, A-02, A-03), except `CM_CHAT_MESSAGE_PUBLIC`, which is M5g's (K-04) with Z-01 as the fallback | – |
| m5j-plan.md:314 | `CM_SHOW_MAP`: "M5j unless M5i / M5f take their O" | **taken** (E-04, D15) | S-12 drops it |
| m5j-plan.md:86 A-I3 | the four `ConquestOffering*AI`, `NoDmgNoActionAI` | **declined** — Balaurea world content with no P5-12 service behind it (D16) | stage 3 N-01 keeps them |
| m5j-plan.md:556, :578 D2 | the six world-event commands | **five taken** (X-01, R-03, B-02, V-01); `Ahserion` deferred with the raid (D6) | – |
| m5j-plan.md:616 K-02 | the staff `VERSION_INFO` partial | **taken as Z-06** unless M5j's stage 0 ran first (the gm needs it, §2.6) | K-02 finds it done |
| m5j-plan.md:87 A-I4 | "I-02 lands before M5i's gate" (the `GMService` arm) | not needed by M5i (the empty list of D2), harmless if landed | – |

---

## 4. Decisions

| # | Decision | Why |
|---|---|---|
| **D1** | **Split P5-12b into three parts sharing `aion_gs_worldevents`**: **P5-12b1** bases and Panesterra (`model/base/**`, `services/BaseService.*`, `services/panesterra/**`), **P5-12b2** rifts, vortex and world raids (`model/{rift,vortex}/**`, `services/{rift,vortex,worldraid}/**`, `services/{RiftService,VortexService,WorldRaidService}.*`), **P5-12b3** events and conqueror/protector (`model/event/**`, `services/{event,conquerorAndProtectorSystem}/**`). Tests move to `tests/worldevents/P5-12b{1,2,3}` by the manifest's shared-target rule (`chunks.cmake:15-16`); the existing `VortexZoneHandlerTest.cpp` goes to b2 and `WorldEventsM5aTest.cpp` is split by what it tests. P5-12a stays one chunk. **The integrator decides before stage 1** (m5b2-plan.md D1 is the precedent). | P5-12b is seven unrelated subsystems and 271 bodies in one unit of ownership; without the split stage 1 has one lane for rifts + world raids + events (~115 bodies) and stage 2 serialises bases behind it. Sizes after the split: b1 ~126, b2 ~86, b3 ~58. |
| **D2** | **The gate drives every world event with the Java admin commands**, typed by a GM account as `CM_CHAT_MESSAGE_PUBLIC` — **that packet is M5g's** (K-04). The account's `aion_ls.account_data.access_level = 9` reaches the game server with the login server's authentication answer and is set in `AccountService.cpp:44` (`LoginServer.cpp:324-334` only handles a later change). M5i's stage 0 ports the rest of the control plane — `ChatCommand` 7 and `AdminCommand` 2 (Z-02) and the staff `VERSION_INFO` partial (Z-06) — and the five world-event commands of C1 come with their services under a lease (X-01, R-03, B-02, V-01). **Both M5i profiles set `gameserver.administration.login.execute_commands` to an empty value** (an empty value is an empty list, `CollectionTransformer.h:16`): Java's default and the shipped value `//invis, //invul, //enemy none, //see` (AdminConfig.java:77, admin.properties:101) reach the `AION_UNPORTED` arm of `GMService.cpp:59-68` today and, once m5j D4 ports it, would make the gm invisible and invulnerable to the gate's npcs. Timelines are proven in-process (§10.6); that the schedules are armed only by T-02 (§2.9); real-time cron firing only in the soak proposal (D17) and the real-client session. | §2.9. The commands are Java's own (`commands.properties`: `siege`, `rift`, `vortexraid`, `worldraid`, `ahserion` = 9, `base` = 8) and give the user the same control in the real-client session, where waiting for Friday 21:00 is not an option. m5b3-plan.md:746-750 already had to tell the user that GM commands do not work. **Stage 0 is independently useful**; if M5j's stage 0 lands first (m5j D1, the user's call), M5i's stage 0 shrinks to the oracle and the harness. |
| **D3** | **Bases are ported faithfully with no C++ enable key.** The commit that closes `BaseService.cpp:18` is the **last commit of the bases lane**; the integrator deletes the `BaseService.cpp:18` rows from every allow-list that has one (`m5a_partial_allowlist.txt:20-21`, `m5b_partial_allowlist.txt:33-34`, and any later gate's) in the same merge, and it triggers the re-greening of every gate (G-07). | Java has no switch; a C++-only key would make every earlier gate test a server that production never runs. The bases' maps are Eltnen, Heiron, Morheim, Beluslan, Belus, Kaldor and Levinshor; **one earlier gate stands on one of them: M5f's arrives in Morheim** (m5f-plan.md:647 C22, :715 G2), about 1.4 km from bases 2220 and 2221. No earlier gate's packet sequence is expected to change, but every gate's startup, spawn census and leak census do, and a 30-minute stress run will see bases change hands (Base.java:131-135). |
| **D4** | **The gate profile sets `gameserver.siege.assault.enable=false`** (Java's code default, SiegeConfig.java:20) and drives assaults deterministically with `//siege assault <id> 0`. The real-client profile keeps the shipped `true` once stage 2 lands, `false` before. **Faithful consequence, documented not fixed:** with the key false, `Siege.stopSiege` skips `BalaurAssaultService.onSiegeFinish` (Siege.java:79-81), so the `FortressAssault` of a `//siege assault` stays in `fortressAssaults` forever (BalaurAssaultService.java:56-72, 97-104): a second `//siege assault` on that fortress answers "Assault on … was already started" or "must be under siege", and the orphan keeps its `Assault.boss`. A deviations row says "Java does this"; G-08 counts it (X19); T-14 pins it. | The shipped value makes every siege start roll `Rnd.chance() < influence × 100 × rate` (BalaurAssaultService.java:74-95). |
| **D5** | **M5i ports ten phase-6 AI handlers under an A1 lease and three root handlers from P5-05.** Of the 21 files in `handlers/ai/siege`, M5i ports 9 — `SiegeNpcAI`, `AbstractSiegeProtectorAI`, `ArtifactProtectorAI`, `FortressProtectorNpcAI`, `GuardianGeneralAI`, `SiegeRaceProtectorAI`, `ShieldNpcAI` (stage 1), `BaseProtectorAI`, `FlagBaseNpcAI` (stage 2) — plus `WorldRaidAI`; the two agent AIs (`EmpoweredAgent`, `EnragedAgent`) and ten others stay phase 6 as DummyAI under `missing_ai_handlers=warn`: `DredgionCommanderAI`, **`FortressGateAI`** (`fortressgate`, 72 siege npc ids, 73 lines: the pass-by-gate question and the door-repair-stone cleanup), `GateRepairAI`, `IncarnateAI`, `MercenaryAI`, `MineAI`, `SiegeCannonAI`, `SiegeTeleporterAI`, `SiegeWeaponAI`, `SpringAI`; so do the 19 Panesterra/Ahserion AIs. Root: `ArtifactAI`, `FlagNpcAI`, `RiftProtectorAI`. **`AbyssGuardSimpleAI` is M5d's** (A-01) and `PortalAI`/`PortalDialogAI` M5f's (I-01). | Without `ShieldNpcAI` no shield ever rises; without `AbstractSiegeProtectorAI` and its two subclasses no kill ends a siege; without `BaseProtectorAI` no kill takes a base; without `ArtifactAI` no artifact can be used. The root ones are P5-05 handlers the roadmap gave to M5j; they belong with the system they serve. |
| **D6** | **Ahserion's Flight is deferred to phase 6** with its AI chunk: `AhserionRaid` (15), `PanesterraService::startAhserionRaid`/`stopAhserionRaid` and the `Ahserion` command stay unported. Both M5i profiles set `gameserver.siege.panesterra.ahserion.time` and `gameserver.moltenus.time` to a far-future expression (`0 0 0 1 1 ? 2099`, the 7-field form `CronJobService.java:70` already uses) so a gate or a Sunday session is not interrupted. **The Wednesday 09:00 dominion job has no key** (CronJobService.java:70); G-04 keeps it out of the gate window instead. | 19 A1 handlers (1,237 Java lines) are the raid; a raid without them is spawns that do nothing. The weekly ERROR exists today (§2.1) and is not made worse. |
| **D7** | **The vortex is stage 3 and waits for M5g**; both profiles keep `gameserver.vortex.enable=false` until then. | `Invasion.addPlayer` builds alliances (Invasion.java:62-89). |
| **D8** | **Java bugs are ported faithfully and pinned by in-process tests, not fixed:** `WorldRaid.stopWorldRaid` before minute 30 (§2.4); `getPreparationCronString` throwing for a preparation over midnight (SiegeService.java:706-707); **`AbyssSiegeLevel.getLevelById(5)`** — fortresses 10111, 10211, 10311 and 10411 have five `siege_reward` grades (measured: 4 locations with 5, 17 with 4, 137 with none) and `sendRewardsToParticipants` asks for level 5 after paying grades 1-4, which throws `IllegalArgumentException` (AbyssSiegeLevel.java:23-30) into the method's own `catch` (Siege.java:213): one ERROR per call, i.e. two per siege end with a non-Balaur winner (FortressSiege.java:168-169) or with `gameserver.siege.reward.balaur.victory=true` (:172-173); only `//siege start 10111` reaches it, since the four are commented out of the schedule (siege_schedule.xml:69-82); the `Idian` portal milliseconds (P5-14, not M5i's). Each gets a `// java-bug` comment and a deviations row; **the gate never stops a world raid and never besieges a Panesterra fortress**, and the real-client checklist warns about both. | "Faithfulness beats a nicer engine" (m5b2-plan.md D9). The world raid one produces an ERROR (`ChatCommand.java:73-74` logs any command exception) and an orphaned boss, so it is also a real-client warning. |
| **D9** | **Settled: `SiegeService::onEnterSiegeWorld` (and `getSiegeIdByLocId`) are M5f's** (T-05, m5f-plan.md:404, D8 at :366). Z-03 remains only as the fallback if T-05 is missing at branch time. | It is reached by `CM_LEVEL_READY` under any profile (§2.1), and every npc teleport calls `getSiegeIdByLocId` first (TeleportService.java:81). |
| **D10** | **File the header request 5a-pre-10 again**: `SiegeService.locations` as `runtime::LinkedHashMap` (and `getSiegeLocations(int)` returning the Java iteration order instead of `std::map`). If accepted, the gate asserts the order of `SM_SIEGE_LOCATION_INFO` type 0, `SM_SHIELD_EFFECT` and the influence worlds; if rejected, it compares them as sets and the deviation rows of `deviations/P5-12a.md` stay. | The request was rejected only because "D1 keeps sieges off the M5a path" (`header-requests.md:183`); M5i is the milestone that turns them on. |
| **D11** | **M5i owns the siege-only reward bodies outside its chunks**: the siege `addAp` variant (P5-08), `scheduleReviveAtBase` (P5-08), `MailFormatter::sendAbyssRewardMail` and `sendCustomAbyssDefeatRewardMail` and the `AbyssSiegeLevel` / `SiegeResult` companions (P5-09), `XPBoostEffect::calculate` (P5-04). Everything else on the reward path is a Dep (§3). | M5c and M5d explicitly leave these to "their milestones" (m5c-plan.md:219-220, m5d-plan.md:422). |
| **D12** | **The gate's fortress is 1131 (Siel's Western Fortress, Reshanta) and its artifact 1135.** 1131: SIEGE boss 263011/263001/263006 at level 40 (so no faction-troop task, FortressSiege.java:86-88), a shield npc in each race's SIEGE set (263510/263208/263209), a sphere shield of radius 50 (`siege_shields.xml`), no outpost dependency, world buff 12147 (`StatupEffect`), maximum occupy count 2, influence 3, assault data `base_delay` 180 and a TELEPORT wave of npc 276793 (siege_locations.xml). 1135: a standalone artifact with a level-40 `artifact_protector` in each PEACE set, activation skill 12042 (`ShieldEffect`, in the subset), cost item 188020000, cooldown 900 s. **All of these are measured by a throw-away parse and must be re-derived by G-01.** | Every alternative fortress either has a level-65 boss (a random 10-30 min task) or a skill outside the effect subset. |
| **D13** | The gate profile sets **`gameserver.timezone=UTC`** and a **cron-quiet window**: before starting the servers the harness asks the oracle for the next fire time of **every job the profile arms** — the 21 siege preparations and the agent fight, the hourly broadcast and the 8 hourly rift jobs (hh:00), the 8 weekly rift jobs, the 23 world-raid jobs (hh:30 on listed days of the month), the event cron's day rollover (00:00 UTC, the moment `EventBuffHandler.onTimeChanged` re-rolls), and **all three `CronJobService` jobs** (Moltenus and Ahserion at their far-future D6 values, and the hard-coded Wednesday 09:00) — and waits until the planned run, **12 minutes** including C13's first-wave wait of at most `base_delay` = 180 s, fits between two of them with 2 minutes to spare (G-04). | A run that crosses hh:00 gets the hourly broadcast and, in Eltnen, a random rift opening in the middle of an assertion; a run on Saturday 16:55 would start a real siege of 1131; a run on Wednesday 09:00 gets `SM_LEGION_DOMINION_LOC_INFO` (or, before M5h's S-08, an ERROR); a run across midnight re-rolls the event fixture of X13. |
| **D14** | **M5i takes the Legion Dominion share M5h hands it, less the portal AI** (LD-01, 14 bodies, stage 2; §3.1): `LegionDominionService` `join`, `onFinishInstance`, `isInCalculationTime`, `openInvasionRift`; `LegionDominionLocation` `join`, `store`, `updateRanking`; a new `LegionDominionIntruderUpdateTask` file, **not scheduled** (Java never starts it); `LegionService::joinLegionDominion`; `CM_LEGION_DOMINION_REQUEST_RANKING`. **The Wednesday calculation is M5h's** (S-08, D13) — the owner the review asked for; LD-01 takes its five bodies only if S-08 is missing at branch time. Taken by the integrator under the standing instruction. | M5h moved it here (m5h-plan.md:408 D2); M5j's S-13 (m5j-plan.md:648) is only a fallback for rev 1's silence; `openInvasionRift` is rift machinery (R-01) and `isInCalculationTime` is reached from a dialog condition and a phase-6 AI. The portal AI enters the phase-6 `StonespearReachInstance`, so it waits for that instance's chunk (O-06). |
| **D15** | **M5i ports `CM_SHOW_MAP`** (E-04, P5-16). | Its only live arm is `intruderScan`, which E-02 ports; m5j-plan.md:314 leaves it to M5j "unless M5i / M5f take their O", and rev 1's "a PvP milestone" is not on the roadmap. |
| **D16** | **Incoming claims M5i declines** (§3.1): `CM_ABYSS_RANKING_LEGIONS`/`AbyssRankingCache` (M5j J5, S-05), the scored/registered group instances (M5j A-F4's items), the PvP half of `PvpService::doReward` that m5g-plan.md:610 O-05 sends here (M5j S-12), the conquest-offering AIs and `NoDmgNoActionAI` (M5j N-01), `LegionDominionPortalAI` (phase 6). Each named owner's plan already has the item or a fallback for it (m5j-plan.md:360-373). | None of them is reachable from a P5-12 service or from any M5i case; taking them would add ~40 bodies with no gate row. |
| **D17** | **The 65-minute soak (G-09) is a proposal to the user, not a required item.** | The standing resource rule after 2026-09-21: "no stress, soak or ASan run without asking" (capacity-proposals.md:654-656); the roadmap reserves machine load beyond it to the user (phase5-roadmap.md:62-63). m5c D13, m5d D16 and m5g made their runs proposals the same way. Without it, real-time cron firing is proven only by the real-client session's step 13. |

---

## 5. Work items

Effort: **S** < 1 agent-day, **M** 1-2, **L** 2-4, **XL** > 4 — relative sizes, not a calendar (§9 converts with the measured pace).
Need: **R** required, **W** stub-with-warning allowed, **O** optional. **Dep** names the earlier milestone the item relies on (§3).

### Integrator

| Id | What | Deps | Need | Eff |
|---|---|---|---|---|
| I-01 | D1's manifest split of P5-12b and the test directories | – | R | S |
| I-02 | Leases, released at merge: **A1** (the ten files of D5), **C1** (five command files), **P4-16** (`SM_INFLUENCE_RATIO`, `SM_FORTRESS_STATUS`), **P5-08** (`AbyssPointsService.cpp`, `PlayerReviveService.cpp`), **P5-09** (`MailFormatter.cpp` and the two new companion headers in `services/mail`), **P5-04** (`XPBoostEffect.cpp`), **P5-07** (`ApExtractAction.cpp`), **P4-11b** (`ControllerStandIns.{h,cpp}`, `RVController.cpp` for the `RiftEnum` stand-in), **P5-14** (`ChatCommand.cpp`, `AdminCommand.cpp`, `CheckOutput.{h,cpp}`), **P5-00** (`PlayerEnterWorldService.cpp` for Z-06), **P5-11** (`LegionDominionService.cpp`, `LegionDominionLocation.cpp`, the new task file, `LegionService.cpp` for `joinLegionDominion` only), **P5-15** / **P5-16** (the two new client packets) | – | R | S |
| I-03 | The header-request batch of §7, decided before stage 1 | – | R | M |
| I-04 | `game-server/config/m5i.properties.example` in the **Java** tree (beside `m5a`/`m5b` examples): the gate profile of §10.1 and the real-client profile of §11 | – | R | S |

### Stage 0 — the command framework, the staff login and the cron oracle

| Id | What | Java refs | Deps | Dep | Need | Eff |
|---|---|---|---|---|---|---|
| **Z-01** | **Fallback only** — if M5g K-04, M5h P-03 and M5j K-05 are all missing at branch time: `CM_CHAT_MESSAGE_PUBLIC` (P5-15) with all eight chat-type arms, `PlayerRestrictions::canChat` (P5-13 lease), `PlayerChatService::logMessage` ×2 and `isFlooding`, and `ChatBanService` 5 with its GAG runnable as a controller task (P5-08 lease; `canChat` calls `isBanned`/`getBanMinutes`/`banPlayer`, `ChatBanService.cpp:7-25`); byte vectors in `tests/cm_ak` | CM_CHAT_MESSAGE_PUBLIC.java:39-151; PlayerRestrictions.java:254-274; ChatBanService.java:26-75 | – | M5g | R (dep) | M |
| **Z-02** | `ChatCommand` 7 (`run`, `toErrorMessage`, `sendInfo`, `join`, `name`, `worldName`, `info`) and `AdminCommand` 2 (P5-14); tests: access-level gate, `IllegalArgumentException` → info to the player, any other throwable → ERROR log (ChatCommand.java:60-76). Skipped if M5j K-01 landed | ChatCommand.java, AdminCommand.java | – | M5j (if D1) | R | M |
| **Z-03** | **Fallback only** — if M5f T-05 is missing: `SiegeService::onEnterSiegeWorld` and `getSiegeIdByLocId` (P5-12a) | SiegeService.java:590-674 | – | M5f | R (dep) | S |
| **Z-04** | `tools/oracle` **`cron` module**, written from Quartz's documented semantics and the Java callers, **not** from `services/cron/CronExpression.cpp`: seconds/minutes/hours/day-of-month/month/day-of-week/[year], `*`, `?`, lists, ranges, `/` steps, day and month names; anything else → `OracleError` (exit 2). `oracle.py cron-next --expr --tz --after [--count]`. Tests over every expression of `siege_schedule.xml`, `rift_schedule.xml`, `world_raid_schedule.xml`, `CustomConfig`/`SiegeConfig` defaults, `CronJobService`'s hard-coded `0 0 9 ? * WED *` and the preparation shift (incl. the midnight refusal) | SiegeService.java:697-716; CronJobService.java:70 | – | – | R | M |
| **Z-05** | Harness: `GameSession::gmCommand(text)` (builds `CM_CHAT_MESSAGE_PUBLIC` from its Java `readImpl`, collects the reply `SM_MESSAGE`s), `ScenarioDatabase` helpers `setAccessLevel`, `seedPosition(world, x, y, z, h)`, `seedLevel` (exp from `player_experience_table.xml` via the oracle), **`seedEventBuff(eventName, buffIndex, poolIds, days)`** (the `event` table, aion_gs.sql:233-239) | – | – | M5g (to run against a server) | R | M |
| **Z-06** | Close the staff `VERSION_INFO` partial (`PlayerEnterWorldService.cpp:524-528`, P5-00 lease): `PacketSendUtility::sendMessage(player, "Server " + GameServer::versionInfo().getBuildInfo(GSConfig::TIME_ZONE_ID), WHITE)` (`GameServer.cpp:134-135`, `VersionInfo.h:62-66`); delete its allow-list rows if any. Skipped if M5j K-02 landed | PlayerEnterWorldService.java (`VERSION_INFO`) | – | M5j (if D1) | R | S |

### Stage 1 — the siege core (P5-12a)

| Id | What | Java refs | Deps | Dep | Need | Eff |
|---|---|---|---|---|---|---|
| **S-01** | `SiegeService` — the **23** remaining bodies: `initSieges`, `checkSiegeStart`, `startPreparations`, `startSiege`, `stopSiege`, `captureSiege`, `resetSiegeLocation`, `updateFortressNextState`, `collectNextSiegeStartDates`, `getDoorRepairData`, `getRepairStone`, `newSiege`, `updateOutpostSiegeState`, `spawnNpcs`, `deSpawnNpcs`, `broadcastUpdate`, `broadcastStatusAndUpdate`, `broadcast`, the `onPlayerLogin` enabled branch, `onAbyssPointsAdded`, `checkRvrEventPlayer`, `clearRvrEventPlayers`, `getPreparationCronString` (`getSiegeIdByLocId`, `onEnterSiegeWorld` are M5f's, `cleanLegionId` M5h's) | SiegeService.java:99-716 | S-02, S-03 | M5b-2, M5f, M5h | R | L |
| **S-02** | `Siege` (14), new `SiegeException`, new **`SiegeStartRunnable`** as a named K3 TaskStruct (`runtime-architecture.md:1702`) so `findNextFireTimes` can find it by type | Siege.java; SiegeStartRunnable.java | – | – | R | M |
| **S-03** | **New files** `FortressSiege` (19), `ArtifactSiege` (6), `OutpostSiege` (7) with member declarations from `fieldmap.py --class` | FortressSiege.java, ArtifactSiege.java, OutpostSiege.java | S-02 | M5b-2, M5c, M5d, M5h | R | L |
| **S-04** | `SiegeCounter` (5), `SiegeRaceCounter` (9 + the 2 template bodies); the ordered counter map keeps Java's descending-value order, and **ties follow the map's iteration order** — a documented deviation, because Java's `ConcurrentHashMap` order is not the shim's | SiegeCounter.java, SiegeRaceCounter.java | – | – | R | M |
| **S-05** | The locations: `SiegeLocation` 6 (incl. `isCanTeleport`, reached from M5f's teleports once sieges are on), `FortressLocation` 3 (incl. `clearLocation`), `OutpostLocation` 3, `ArtifactLocation` 7 (incl. `getStatus`, reached from M5f's `onEnterSiegeWorld`), `AgentLocation` 2; `ShieldService::createShieldObserver`; companion `ArtifactStatusInfo.h` | model/siege/*.java; ShieldService.java:51-54 | – | – | R | M |
| **S-06** | New `MercenaryLocation` (8): the data half is created by `FortressSiege.initMercenaryZones` whenever a non-Balaur fortress is besieged; `spawn` is reached only from `MercenaryAI` (phase 6) | MercenaryLocation.java | S-03 | – | R (ctor) / W (spawn) | S |
| **S-07** | Tests `tests/siege`: the timeline tests T-01..T-06 and T-14 of §10.6 on `DeterministicExecutor` + `ManualClock`, with a small world fixture holding Reshanta's siege spawns of 1131 and 1135 only; `captureSiege` both branches (two broadcast pairs in the siege branch); `newSiege` dispatch and the `SiegeException` for an unknown id; the double-start and double-stop refusals (Siege.java:50-86) | – | S-01..S-06 | – | R | L |
| **S-08** | `cycles.toml` / `fieldmap.toml` rows for the missing tasks of §2.5, **the corrected `WorldRaid$1#this` row** (`cycles.toml:286`), and the D4 orphan; `// java-race` where Java races (`startSiege` is synchronized, `Siege.startSiege` synchronizes minimally, `activeMercenaryLocs` is concurrent) | – | S-01..S-06 | – | R | M |

### Stage 1 — siege AIs (A1 lease + P5-05)

| Id | What | Deps | Dep | Need | Eff |
|---|---|---|---|---|---|
| **A-01** | A1: `SiegeNpcAI`, `AbstractSiegeProtectorAI`, `ArtifactProtectorAI`, `FortressProtectorNpcAI`, `GuardianGeneralAI`, `SiegeRaceProtectorAI`, `ShieldNpcAI` (222 Java lines); registration markers; tests in the A1 test directory or `tests/handlers_ai_core` under the lease | S-02 | M5b-1 | R | M |
| **A-02** | P5-05 root: `ArtifactAI` (269 lines: two request dialogs, the 10 s channel with an `ItemUseObserver`, `ArtifactUseSkill` with repeats) | S-05 | M5b-2, M5b-3, M5c | R | L |

### Stage 1 — siege cross-chunk (C1, P4-16, P5-08, P5-09, P5-07 leases)

| Id | What | Deps | Dep | Need | Eff |
|---|---|---|---|---|---|
| **X-01** | `SiegeCommand` (C1, 133 lines) | Z-02, S-01 | M5h (legion capture arm) | R | S |
| **X-02** | `SM_INFLUENCE_RATIO::writeImpl`, `SM_FORTRESS_STATUS::writeImpl` + `tests/sm_ak` byte vectors | S-05 | – | R | S |
| **X-03** | `AbyssPointsService::addAp(Player&, VisibleObject&, int)` — the siege variant only (the two others are M5d's E-09) | S-01 | M5d | R | S |
| **X-04** | `PlayerReviveService::scheduleReviveAtBase` — the `TaskId.TELEPORT` task first, then the instance/kisk/bind arms (PlayerReviveService.java:250-259) | – | M5b-1 | R | S |
| **X-05** | `MailFormatter::sendAbyssRewardMail`, `sendCustomAbyssDefeatRewardMail`; **companions `AbyssSiegeLevelInfo.h` (`getId`, `getLevelById` throwing `IllegalArgumentException` for an unknown id, the D8 bug) and `SiegeResultInfo.h` (`getId`)** after the `SiegeRaceInfo.h` precedent (P5-09) | – | M5c | R | M |
| **X-06** | `ApExtractAction` (5, P5-07 lease, m5c-plan.md:403): `canAct`'s refusals and `act`'s cast and AP gain; unit tests in the P5-07 test directory under the lease | – | M5b-3, M5d | R | S |

### Stage 1 — rifts and world raids (P5-12b2)

| Id | What | Deps | Dep | Need | Eff |
|---|---|---|---|---|---|
| **R-01** | `RiftService` 7, `RiftManager` 4, `RiftLocation` 6, new `RiftOpenRunnable`, companion `RiftEnumInfo.h` (12) **replacing** `standins::riftEnumData` (P4-11b lease, signature request) | – | – | R | M |
| **R-02** | `WorldRaidService` 3, `WorldRaid` 17 (the D8 bug kept), new `WorldRaidRunnable` | – | – | R | M |
| **R-03** | `Rift` and `WorldRaid` admin commands (C1; written by the siege-edges lane, which holds the C1 lease in stage 1) | Z-02 | – | R | S |
| **R-04** | Tests T-08, T-09 (§10.6) | R-01, R-02 | – | R | M |

### Stage 1 — events and conqueror/protector (P5-12b3, P5-16)

| Id | What | Deps | Dep | Need | Eff |
|---|---|---|---|---|---|
| **E-01** | `Event` 15, `EventBuffHandler` 26 + the template body, `EventService::startOrStopEvents`, `Headhunter` 2; `XPBoostEffect::calculate` (P5-04 lease) | – | M5b-2, M5b-3, M5d | R | L |
| **E-02** | `ConquerorAndProtectorService` — the **10** remaining bodies (incl. `isOccupiedLegionDominionZone` and the `onEnterZone` arm; `onLeaveLegion` and `resetLegionDominionRank` are M5h's), `CPBuff` 2 (the PvP arms are reachable only from PvP kills: W) | – | M5h | R / W | M |
| **E-03** | Tests T-10, T-12 (§10.6); `EventDAO` round trip of the buff pool and allowed days, including a stored row being preferred to a new roll | E-01, E-02 | – | R | M |
| **E-04** | `CM_SHOW_MAP` (P5-16, D15): `readImpl`, `runImpl` (action 0 → `intruderScan`, 1 no-op, else WARN), the `AION_CLIENT_PACKET` marker; byte vectors in `tests/cm_lz` | E-02 | – | R | S |

### Stage 1 — gate harness (P5-SC, tools/oracle)

| Id | What | Deps | Need | Eff |
|---|---|---|---|---|
| **G-01** | `oracle.py m5i-sieges --at <instant> --tz <zone> [--db-races …]`: the 70 siege locations (18 FORTRESS, 49 standalone ARTIFACT, 2 OUTPOST, 1 AGENT_FIGHT — INDUN 85 and UNDERPASS 3 are ignored by `SiegeLocationData.afterUnmarshal`), their types, worlds, default races (BALAUR; 2111 ELYOS, 3111 ASMODIANS), influence values and the **Influence formula** written from Influence.java, per (location, race, mod) the npc ids and spots with `abyss_type` BOSS and the shield npc, the shield spheres, the artifact activations, the assault data and **the first-wave window of a `FortressAssault`** at a given balance and influence (FortressAssault.java:57-60, 120-133), and **nextState / seconds-until-next-state at the instant** through the Z-04 cron module; plus **`--spots`** choosing S1-S7 of §10.2 with their constraints (inside/outside a location's zone, in view of named spawns, outside the aggro ranges of every PEACE and SIEGE set of every race that will stand there, a sphere-crossing move pair, the count of assault-eligible siege npcs in view of S1) | Z-04 | R | L |
| **G-02** | `oracle.py m5i-events` (event names, the disabled list that leaves exactly "Beyond Aion Server Buffs", **each of its buffs with index, triggers, restrictions and whether it can apply on a world map**, the effect classes), `m5i-rifts` (locations per world, `RiftEnum` master/slave npc ids), `m5i-worldraid` (locations, flag npc 832819 spot), `m5i-bases` (per base and occupier the FLAG/MERCHANT/SENTINEL/BOSS/ATTACKER groups) | Z-04 | R | M |
| **G-03** | Decoders `tests/scenario/decoders/SiegeDecoders.{h,cpp}` from Java `writeImpl` (no `serverpackets/` include, body consumed exactly): `SM_SIEGE_LOCATION_INFO`, `SM_SIEGE_LOCATION_STATE`, `SM_FORTRESS_INFO`, `SM_FORTRESS_STATUS`, `SM_INFLUENCE_RATIO`, `SM_SHIELD_EFFECT`, `SM_ABYSS_ARTIFACT_INFO3`, `SM_RIFT_ANNOUNCE` (all five actions), `SM_AFTER_SIEGE_LOCINFO_475`, `SM_NPC_ASSEMBLER`; `SiegeDecodersTest.cpp` | – | R | M |
| **G-04** | The cron-quiet window (D13) over every job it lists, with the 12-minute run budget; the harness records the window it used | Z-04 | R | S |
| **G-05a** | The stage-1 cases C0-C11 of §10.2 written and run locally against the part-3 build (not yet registered), so stage 2's gate lane starts from a script that runs | G-01..G-04, Z-05 | R | M |

### Stage 2

| Id | What | Deps | Dep | Need | Eff |
|---|---|---|---|---|---|
| **B-01** | Bases (P5-12b1): `Base` 25, `BaseLocation` 3, `BaseService` 9, companion `BaseOccupierInfo.h`, new files `CasualBase`, `SiegeBase`, `StainedBase`, `SiegeBaseLocation`, `StainedBaseLocation`, `BaseException`, `PanesterraBase`, `PanesterraArtifact`, `PanesterraFactionCamp`, `PanesterraBaseLocation`; **closing the constructor partial is the lane's last commit** (D3). `cancelTask(true)` on a task that is running the capture (Base.java:131-135 → `BaseService.capture` → `stop` → `handleStop` cancels the assault task from inside itself) must not deadlock or destroy the running task's captures early | – | M5g (team killer) | R | L |
| **B-02** | `BaseCommand` (C1): `list`, `start`, `stop`, `capture <id> <occupier>`, `assault <id> <occupier>` with its three refusals (BaseCommand.java); tests T-07 and a census case: 100 captures on one base, `Base` created 101 / live 1, every `Npc` of the old bases reclaimed | B-01 | – | R | M |
| **BA-01** | Balaur assault (P5-12a): `BalaurAssaultService` 7, `Assault` 5, `FortressAssault` 12, `ArtifactAssault` 5, companion `AssaulterTypeInfo.h`; tests T-11 and T-14 | S-01 | – | R | M |
| **AG-01** | New `AgentSiege` (12, P5-12a) with its 9 × 60 s start chain; the winner arm's `BaseService.capture(6113, …)` is compiled against the existing declaration and unreachable until the phase-6 agent AIs set a winner (`setWinnerRace` has one caller, EmpoweredAgent.java:208; AgentSiege.java:71), so **it does not wait for B-01**; test T-05 | S-01 | M5d (`startQuest`) | R | M |
| **A-03** | A1: `BaseProtectorAI`, `FlagBaseNpcAI`, `WorldRaidAI`; P5-05: `FlagNpcAI`, `RiftProtectorAI` (5 handlers; `AbyssGuardSimpleAI` is M5d's) | B-01 (registration only; the handlers compile against `Base.h`) | M5g | R | M |
| **LD-01** | Legion Dominion (D14; P5-11 lease + P5-15): `LegionDominionService` `join`, `onFinishInstance`, `isInCalculationTime`, `openInvasionRift`; `LegionDominionLocation` `join`, `store`, `updateRanking`; new `LegionDominionIntruderUpdateTask` (3, not scheduled, a `// java-dead` note); `LegionService::joinLegionDominion`; new `CM_LEGION_DOMINION_REQUEST_RANKING` with `tests/cm_lz` vectors; **+ the five S-08 bodies if M5h's S-08 is missing**; test T-13 | R-01 (`openInvasionRift` → `RiftService::openRifts`, `closeRifts`, `isRiftOpened`, `getDuration`) | M5h (legion model, `CM_LEGION`, S-08), M5c (`SystemMailService`, dialogs) | R | M |
| **P-01** | Panesterra (P5-12b1, stage 3 if M5g is late): `PanesterraService` bodies other than the Ahserion ones, `PanesterraTeam` 8, companion `PanesterraFactionInfo.h` (and P5-14's local mapping replaced) | B-01 | M5g | R / W | M |
| **G-05** | `TEST(M5iScenario, Run)` + `gs.scenario.m5i` in `ScenarioTests.cmake`: output dir `<bin>/scenario/m5i`, schemas `aion_{ls,gs}_test_m5i_<hash>`, the same `RESOURCE_LOCK`, `tests/scenario/m5i_partial_allowlist.txt`, the cases of §10.2 (C12 and C13 switched on when B-02 and BA-01 merge) | G-01..G-04, G-05a, Z-05, all of stage 1, B-01, **B-02**, BA-01 | every row of §3 the cases use | R | L |
| **G-06** | `gs.scenario.m5i_geo` (§10.5) | G-05 | – | R | M |
| **G-07** | **Re-green every gate** after D3's closing commit: `gs.scenario.m5a`, `m5a_geo`, `m5a_stress`, `m5b`, `m5b_geo`, `m5b2`, `m5b2_geo`, `m5b3`, `m5b3_geo`, `m5c`, `m5c_geo`, `m5d`, `m5d_geo`, `m5e`, `m5e_geo`, **`m5f` and `m5f_geo` (Morheim, bases 2220/2221 — D3)**, `m5g`, `m5g_alliance`, `m5g_geo`, `m5h`, `m5h_geo`, `m5h2`, `gs.smoke.startup`(`_geo`) — whichever of them exist at branch time; record startup time and spawn counts before/after | B-01 | – | R | L |
| **G-08** | `CheckOutput` (P5-14 lease): a **service-bounded** live-count rule — `Siege` live = active sieges, `Base` live = active bases, `WorldRaid` live = active raids, `FortressAssault` live = entries of `fortressAssaults` (D4) at the census — and the summary rows | G-05 | – | R | S |
| **F-01** | Fixups the gate and the regate name, in the owning chunk (stage 2 part B) | G-05, G-07 | – | R | – |

### Stage 3

| Id | What | Deps | Dep | Need | Eff |
|---|---|---|---|---|---|
| **V-01** | Vortex (P5-12b2): `VortexService` 10 (with the team arms m5g-plan.md:609 O-04 leaves here), `DimensionalVortex` 7, `VortexLocation` 5, new `Invasion` (11), `VortexRaid` command (C1); test T-11b; gate cases added to `gs.scenario.m5i` with `vortex.enable=true` | R-01 | **M5g** | R | L |
| **G-09** | **Proposal to the user (D17):** the 65-minute soak (`gs.scenario.m5i_soak`): every system enabled, three idle clients in Reshanta, Eltnen and Verteron, crossing ≥ 1 hh:00; ASan; asserts of §10.7. Runs only in a slot the user chooses | G-05 | – | O (user) | M |

### Deferred

| Id | What | Where |
|---|---|---|
| O-01 | Ahserion's Flight: `AhserionRaid` 15, `startAhserionRaid`/`stopAhserionRaid`, 19 A1 handlers (1,237 lines), the `Ahserion` command | phase 6 (D6) |
| O-02 | The ten other siege AIs of D5 (incl. `FortressGateAI`) and the two agent AIs | phase 6 (A1) |
| O-03 | The 13 effect classes that 25 artifact activation skills need beyond the subset (`ProcAtkInstantEffect`, `ProcDPHealInstantEffect`, `DeformEffect`, `MpAttackInstantEffect`, `BindEffect`, `SilenceEffect`, `NoFlyEffect`, `OpenAerialEffect`, `BoostSkillCastingTimeEffect`, `FPHealInstantEffect`, `ProcFPHealInstantEffect`, `FpAttackInstantEffect`, `AbsoluteSnareEffect`) | an effects milestone (m5b2-plan.md O-01; m5e/m5f take some) |
| O-04 | Conqueror/protector PvP arms (W in E-02) | M5j (A-I2's PvP item) |
| O-05 | Event arcade, headhunting, advent calendar (all off by default) | M5j |
| O-06 | `LegionDominionPortalAI`, `LegionDominionInvasionRiftOpenAI`, `StonespearReachInstance` | phase 6 (D14) |
| O-07 | The incoming claims of §3.1 that D16 declines | their named owners |

---

## 6. Lanes

At most six per part; chunks disjoint within a stage; under the resource rule at most two lanes build at once (capacity-proposals.md:654-656).

| Stage / part | Lane | Chunks | Items | Tests |
|---|---|---|---|---|
| **0** | **control-plane** | P5-14, P5-00 (lease) (+ P5-15/P5-13/P5-08 for Z-01, P5-12a for Z-03 — fallbacks only) | Z-02, Z-06 (Z-01, Z-03 if needed) | `tests/misc` (P5-14's), the P5-00 owner's tests under the lease |
| 0 | **cron-oracle** | P5-SC, tools/oracle | Z-04, Z-05 | `tools.oracle`, harness self-tests |
| 1.1 | **siege-core** | **P5-12a** | S-02, S-04, S-05 | `tests/siege` |
| 1.1 | **siege-edges** | C1 (lease), P4-16, P5-08, P5-09, P5-07 (leases) | X-02, X-04, X-05, X-06 | `tests/sm_ak`, `tests/playersvc`, `tests/economy`, the P5-07 tests |
| 1.1 | **rifts-raids** | **P5-12b2**, P4-11b (lease) | R-01 | `tests/worldevents/P5-12b2` |
| 1.1 | **events-cp** | **P5-12b3**, P5-04 (lease), P5-16 | E-02, E-04 | `tests/worldevents/P5-12b3`, `tests/cm_lz` |
| 1.1 | **gate-harness** | P5-SC, tools/oracle | G-01, G-03, G-04 | decoder and oracle tests |
| 1.2 | siege-core | P5-12a | S-03, S-01, S-06 | `tests/siege` |
| 1.2 | **siege-ai** | A1 (lease), P5-05 | A-01 | A1/`handlers_ai_core` tests |
| 1.2 | siege-edges | as 1.1 | X-01, X-03, **R-03** | C1 tests |
| 1.2 | rifts-raids | P5-12b2 | R-02 | `tests/worldevents/P5-12b2` |
| 1.2 | events-cp | P5-12b3, P5-04 | E-01 | `tests/worldevents/P5-12b3` |
| 1.2 | gate-harness | P5-SC, tools/oracle | G-02 | oracle tests |
| 1.3 | siege-core | P5-12a | S-07, S-08 | `tests/siege` |
| 1.3 | siege-ai | A1, P5-05 | A-02 | P5-05 tests |
| 1.3 | rifts-raids | P5-12b2 | R-04 | T-08, T-09 |
| 1.3 | events-cp | P5-12b3 | E-03 | T-10, T-12 |
| 1.3 | gate-harness | P5-SC | G-05a | a local run of C0-C11 |
| **2A** | **bases** | **P5-12b1**, C1 (lease: `BaseCommand`) | B-01, B-02, P-01 (if M5g is in) | `tests/worldevents/P5-12b1` |
| 2A | **assault-agent** | **P5-12a** | BA-01, AG-01 | `tests/siege` |
| 2A | **world-ai** | A1 (lease), P5-05 | A-03 | AI tests |
| 2A | **dominion** | P5-11 (lease), P5-15 | LD-01 | T-13, `tests/cm_lz` |
| 2A | **gate** (gate and regate in one lane) | P5-SC, P5-14 (lease: `CheckOutput`) | G-05, G-06, G-08, then **G-07 after B-01's closing commit** | `gs.scenario.m5i`, `_geo`, every earlier gate |
| **2B** | fixups (a part of its own, not a lane) | the chunks G-05 and G-07 name, one lane per chunk, ≤ 6 | F-01 | owner tests + the gates |
| 3 | **vortex** | P5-12b2, C1 | V-01 | T-11b, gate additions |
| 3 | (soak) | P5-SC | G-09 **only if the user agrees** (D17) | `gs.scenario.m5i_soak` |
| all | integrator | manifest, leases | I-01..I-04 | full verification |

**Notes.** The one lane that holds the C1 lease in stage 1 (siege-edges) writes all three stage-1 commands (`SiegeCommand`, `Rift`,
`WorldRaid`); R-03 is listed under rifts-raids in §5 for traceability only. **Critical path:** stage 1's siege-core (~125 bodies over ~2,500
Java lines; S-03 + S-01 are the dense state machine, in part 1.2); **stage 2 is serial behind the bases lane** — B-01 → B-02 → G-05's C12 →
D3's closing commit → G-07 → part 2B — the other four 2A lanes finish inside that path. **Merge order in stage 1:** header batch (I-03) →
part 1.1 (S-02/S-05 first) → part 1.2 (S-03 → S-01 → A-01 → X-*) → part 1.3; R-*, E-* merge when green within their part. **Stage 0's
concurrency:** the control-plane lane needs P5-14 and P5-00. M5h's stage-1 leases do not include them (m5h-plan.md:434 I-02: A1, Q05, Q09,
P5-12a, P5-12b, P5-13, P4-14, P4-16, P4-17, and P5-00 only "if A-23 is missing"), but its gate stage holds P5-14 for `CheckOutput`
(m5h-plan.md:500 G-07). So the control-plane lane can run beside M5h's stage 1 if A-23 held, **not beside M5h's gate stage**; the
cron-oracle lane holds P5-SC and waits for M5h's gate lane to release it; the Z-03 fallback waits for M5h's P5-12a lease to be released.

---

## 7. Header requests expected

| Request | Kind | For |
|---|---|---|
| New declaration headers for the 20 no-file classes (§2.7), member declarations from `fieldmap.py --class` (`fieldmap.json` already has `FortressSiege`, `MercenaryLocation`, `SiegeStartRunnable`, `RiftOpenRunnable`, `CasualBase`, …), `fwd.h` regenerated with `skeleton.py --fwd`; spine-status.md:510 already lists `ArtifactSiege.h`/`FortressSiege.h` as missing | new files (no request) | S-02, S-03, S-06, R-01, R-02, B-01, BA-01, AG-01, V-01 |
| `taskmanager/tasks/LegionDominionIntruderUpdateTask.h` (`fwd.h:8` declares the name only) | new file | LD-01 |
| Companion headers `ArtifactStatusInfo.h`, `AssaulterTypeInfo.h` (P5-12a), `BaseOccupierInfo.h`, `PanesterraFactionInfo.h` (b1), `RiftEnumInfo.h` (b2), **`AbyssSiegeLevelInfo.h`, `SiegeResultInfo.h` (P5-09)**, after the `SiegeRaceInfo.h` precedent | new files | S-05, BA-01, B-01, P-01, R-01, X-05 |
| `controllers/ControllerStandIns.h`: **delete** `RiftEnumData` and `riftEnumData` (`:87-99`); `RVController.cpp` reads `RiftEnumInfo.h` | signature | R-01 |
| `services/SiegeService.h`: `locations` as `runtime::LinkedHashMap`; `getSiegeLocations(int32_t)` in iteration order (D10; reopens 5a-pre-10) | layout + signature | S-01 |
| `network/aion/clientpackets/CM_SHOW_MAP.h`, `CM_LEGION_DOMINION_REQUEST_RANKING.h` (and `CM_CHAT_MESSAGE_PUBLIC.h` only for the Z-01 fallback) | new files | E-04, LD-01, Z-01 |
| `CheckOutput.h`: the service-bounded rule | additive | G-08 |
| **Manifest (D1)** and the leases of I-02 | build | I-01, I-02 |
| `game-server/config/m5i.properties.example` (Java tree) | none | I-04 |

---

## 8. Risks

Ordered by what is most likely to go wrong, with the evidence for each.

1. **Waking 31 bases in every server is the biggest cross-gate hazard (D3).** `BaseService` has no switch (BaseService.java:30-53); the assault
   cycle captures bases when no player is near (Base.java:131-135), so every capture despawns and respawns npcs, cancels four tasks and
   creates a new `Base` — forever, with nobody online. Every gate's startup, spawn counts, leak census and the nightly stress change the day
   B-01 lands; M5f's gate stands in Morheim, a base map. G-07 exists for this, and **the closing commit must be the lane's last** so every
   earlier commit stays green.
2. **The control plane is other milestones' code now.** The gate cannot run without `CM_CHAT_MESSAGE_PUBLIC` (M5g's K-04), `ChatCommand::run`
   (Z-02, or M5j's K-01) and five C1 command classes under a lease (D2). If any of them is wrong or late, every M5i case fails the same way.
   Stage 0 lands its share first with its own access-level tests; the Dep rows of §3 are checked before stage 0 starts, and the fallbacks
   Z-01/Z-03 are ready if a sibling slipped.
3. **Cron boundaries and wall-clock schedules inside a gate run.** The hourly broadcast and the eight hourly rifts (hh:00), the world raids
   (hh:30 on listed days), the preparations (hh:55, and 20:50 for the agent fight), the hard-coded Wednesday 09:00 dominion job (no key can
   move it, CronJobService.java:70) and the event cron's midnight re-roll fire on the real clock; a run crossing one gets packets and spawns
   no assertion expects, and a run on Saturday 16:55 starts a real siege of the gate's own fortress (siege_schedule.xml: 1131
   `0 0 17 ? * TUE,THU,SAT`). G-04's quiet window must cover **every** job D13 lists, including the ones the gate does not test.
4. **Upstream breadth and double claims.** M5i sits on M5b-2, M5b-3, M5c, M5d, M5f, M5g and M5h (§3). The siege end alone reaches mail, GP,
   legion history, `getOrLoadPlayerCommonData` and quest kills; five bodies of M5i's chunks and the chat packet are other plans' items, and
   the Legion Dominion is split between M5h (the calculation) and M5i (the rest). The sibling plans changed while this one was written
   (m5f, m5g, m5h and m5j between 23:20 and 23:30), and m5j planned fallback items for rev 1's gaps that rev 2 now fills (§3.1). Any slip
   turns a Dep row into a W or a fallback item, and the wave report must say which.
5. **`onSiegeFinish` runs inside `SiegeService.stopSiege`'s monitor and throws mid-way if anything below it is unported.** Java's
   `stopSiege` is `synchronized` (SiegeService.java:220) and `FortressSiege.onSiegeFinish` does ten things in a row (FortressSiege.java:130-188);
   a throw after `setVulnerable(false)` and before `spawnNpcs(PEACE)` leaves a fortress with no npcs, no siege and the old owner in the DB.
   Only two of its calls are guarded (`sendRewardsToParticipants` and the legion ones catch). X9/X10 and `unported_trace.txt` empty are what
   catch it.
6. **Server-lifetime holders of despawned npcs meet the leak census.** The census reports world objects still alive 10 minutes after
   `World.removeObject` (runtime-architecture.md:639-640). `Siege.boss`, `Base.flag`/`assaulter`, `WorldRaid.flag/vortex/boss/markers`,
   `MercenaryLocation.spawnedMercs` and `VortexLocation.spawned` hold `Ref<Npc>`; a finished level-65 `FortressSiege` stays pinned by its
   10-30 minute faction-troop task (FortressSiege.java:87), keeping its despawned boss; and with assaults off **the orphaned `FortressAssault`
   of D4 keeps C13's despawned boss for the rest of the process** (Assault.java:31-36). The gate's first process ends about a minute after
   C13, so the gate does not see it; a long session with assaults off would. Java's GC does not care; the census will. The soak (a proposal,
   D17) is where the first kind shows, and a C++ breaker needs a deviation row.
7. **`ShieldService::createShieldObserver` is reached for npcs too** (FortressLocation.java:55-56 checks the creature's race, not that it is a
   player). An assault wave spawned into a shielded fortress throws inside `ZoneInstance::onEnter` — the F-1 shape (m5a-client-session.md).
   It is one trivial body in S-05; forgetting it is the risk.
8. **The siege AP path runs inside a swallowing `try`.** `NpcController::doReward` calls the siege `addAp` variant (`NpcController.cpp:271-273`)
   for every AP-rewarding kill; unported, it becomes one ERROR per kill in Reshanta — m5b-client-session.md S-1 again. X-03 closes it and
   the "no ERROR" bar keeps it closed.
9. **Tie order in the reward ranking.** `SiegeRaceCounter.getOrderedCounterMap` sorts by value; equal values keep the source map's order, which
   is `ConcurrentHashMap` iteration in Java and the shim's in C++. Top-N reward recipients can differ on ties. Documented (S-04), not fixed.
10. **The gate characters stand among hostile guards.** Every fortress spawns aggressive guards of its owner race, and 1131 changes owner six
    times in the run. The oracle must pick S1-S7 outside the aggro ranges of every set that will stand near them, and the characters are
    seeded to level 40; a guard may still hit a character in the seconds a case needs. m5b2-plan.md risk 13 is the same problem with a kerub.
11. **`SM_SIEGE_LOCATION_INFO` order.** If D10 is rejected, the type-0 packet lists 70 locations in the shim's hash order where Java uses XML
    order; the client probably does not care, and the gate compares sets.
12. **Startup time, memory and run length.** +1,454 PEACE siege spawns, the base spawns and 68 cron jobs; gs.scenario.m5a takes ~32 s today
    (m5b2-plan.md risk 12). The M5i gate boots twice, relogs about ten times and waits up to 180 s for C13's first wave: **budget 12 minutes**
    under the same `RESOURCE_LOCK`, plus G-04's wait for a window.
13. **Java bugs faithfully ported can look like port bugs** (D8): the world-raid stop NPE, the orphaned boss, the Panesterra reward ERROR, the
    Idian portals every few seconds, and D4's orphaned assault. Each needs a deviations row that says "Java does this", or a later reviewer
    will "fix" it.
14. **Every data number here comes from a throw-away parse.** m5b-plan.md and m5b2-plan.md both shipped data counts that were wrong in review
    (commented-out XML, `npc_ids` lists); rev 1 of this plan shipped the claim that every player gets +1500 HP, which the event's own
    restriction contradicts (§2.4). G-01/G-02 take them over before any gate asserts them.
15. **One randomness the gate cannot seed.** C13's first wave places npc 276793 next to each eligible siege npc with a 40 % chance
    (FortressAssault.java:67-73); X17's "at least one in view" needs n ≥ 12 eligible npcs in view of S1 (a miss rate ≤ 0.22 %), which G-01
    must find or the assertion drops to its subset half.

---

## 9. The split: four stages, and why in this order

**The unit is measured wall-clock pace, not agent-days.** M5b-1 went from its reviewed plan to its stage-2 gate in about 24 hours
(`5f65cb14f`, 2026-09-22 02:29 → `23c4e6485`, 2026-09-23 02:32). M5b-2's stage 0, planned at "~2 days", took about 8 hours (→ `6f6756c06`,
10:41), and its stage 1 ran as three parts of about 3-4 hours each (`29009d778` 14:36, `c1edb0afb` 18:49, `760e8ab5c` 22:58) after a
1-hour lease interlude (`2e47bbb64`, `a3f301e7b` 11:50). **One part ≈ 4-8 hours** at the resource rule's two building lanes; M5i is planned in parts from the start.

| Stage | What a player can do at the end | Chunks | Bodies | Lanes / parts | Wall-clock (inferred) |
|---|---|---|---|---|---|
| **0 — the control plane** | a GM can log in with Java's revision config and type `//` commands (with M5g's packet); the cron oracle answers | P5-14, P5-00 (lease), P5-SC, tools/oracle | ~10 + the oracle | 2 lanes, 1 part | ~4-8 h |
| **1 — a fortress can be besieged** | start, capture, defend a fortress; die in an enemy shield; capture an artifact; see the influence bar; get the event's pool buff; open a rift; start a world raid; open the conqueror map scan | P5-12a, P5-12b2, P5-12b3, A1/P5-05, C1, P4-16, P5-08, P5-09, P5-04, P5-07, P5-16, P5-SC | ~292 | 6 lanes, 3 parts | ~12-18 h |
| **2 — bases, assaults, the agent fight, the dominion service; the gate** | bases change hands; a Balaur assault attacks a fortress; the agent fight in Levinshor runs; a legion can join Stonespear Reach; every earlier gate is green again | P5-12b1, P5-12a, A1/P5-05, P5-11/P5-15 (leases), C1, P5-SC, P5-14 | ~183 (incl. P-01) | 5 lanes (2A) + fixups (2B), **serial behind B-01** | ~10-16 h |
| **3 — the vortex** (after M5g) | a Theobomos/Brusthonin invasion | P5-12b2, C1 | ~37 (+ P-01's ~30 if M5g is late) | 1 lane, 1 part | ~4-8 h |

**Total ~520 bodies ported (+ ~20 deferred), eight parts, about 30-50 hours of wall-clock** — the same order as M5b-2 (~521 bodies), whose
stage 0 and stage 1 took ~20.5 hours with its gate stage still to come. Inferred, not measured (§12).

1. **Stage 0 first, because every later lane and the user need the control plane**, and because it is small and touches nothing of P5-12.
2. **Stage 1 is the siege, not the world events, because the siege is what the milestone is named for and the largest single state
   machine**; rifts, world raids and events are independent and fill the remaining lanes. Each part has a green point: 1.1 the models and
   packets unit-tested; 1.2 a start, a capture, a stop and an artifact capture in-process; 1.3 the timelines and a local run of C0-C11.
3. **Bases wait for stage 2 because closing their partial changes every gate (D3)**; stage 2 therefore also holds the gate and the regate.
   The Balaur assault and the agent fight need a working siege underneath them; the dominion service needs R-01.
4. **The vortex waits for M5g** (D7) and Ahserion's Flight for phase 6 (D6); stage 3 is where the vortex lands.

**If stage 1 has to split further, split it at the subsystems, not inside the siege**: land the siege core, AIs and edges with a siege-only
gate, and move rifts, world raids and events to a stage 1b. Splitting `Siege` from its subclasses leaves no green point (a siege with no
subclass cannot be started).

---

## 10. Gate specification (`ctest -L scenario`, `gs.scenario.m5i`)

### 10.1 Processes, databases and profile

As m5b2-plan.md §10.1, except:

| Piece | M5i |
|---|---|
| Schemas / output | `aion_{ls,gs}_test_m5i_<hash>`, `<bin>/scenario/m5i` |
| `RESOURCE_LOCK` | the same `"aion_game_server_log;aion_login_server_log"` |
| Profile | the latest earlier gate profile **plus** `gameserver.siege.enable=true`, `gameserver.siege.assault.enable=false` (D4), `gameserver.rift.enable=true`, `gameserver.worldraid.enable=true`, `gameserver.vortex.enable=false` (true in stage 3), `gameserver.cp.enable=true`, `gameserver.event.service.disabled_events=<the 75 other event names, from G-02>`, `gameserver.timezone=UTC` (D13), `gameserver.siege.panesterra.ahserion.time` and `gameserver.moltenus.time` far future (D6), **`gameserver.administration.login.execute_commands=` (empty, D2)**; `gameserver.geodata.enable=false` (the geo variant: true) |
| Accounts | **A** normal → Asmodian "watcher"; **B** GM, `account_data.access_level=9` → Elyos "gm"; both seeded to **level 40** and to oracle positions (Z-05, §10.2) |
| Fixture | the `event` row of the permanent event's pool buff, seeded before the first boot and rewritten between the two processes (C1, C14) |
| Start | only inside a cron-quiet window (G-04) |
| Allow-list | `tests/scenario/m5i_partial_allowlist.txt`: §A the startup rows still left by earlier milestones (no `BaseService.cpp:18` row: D3 closed it); §B the partials M5i leaves (none planned; the staff `VERSION_INFO` partial is closed by Z-06 or M5j's K-02); §C timing rows |

### 10.2 Cases

Positions are oracle spots (`m5i-sieges --spots`, G-01), each chosen outside the aggro range of every set that stands near it during the
cases that use it (risk 10): **S1** inside 1131's fortress zone and outside its shield sphere, in view of the PEACE sets' spawns near it and
of ≥ 12 assault-eligible siege npcs; **S2** inside 1131's zone in view of the ELYOS SIEGE boss 263001 and shield npc 263208; **S3** outside
1131's zone with the move pair M1 (into the zone, still outside the sphere) and M2 (across the sphere boundary above the centre); **S4** in
Reshanta outside 1131's zone, in view of world-raid location L's flag spot; **S5** next to artifact 1135; **S6** in Eltnen, in view of rift
location R's master spawn; **S7** in Eltnen, in view of base 2120's spawns (S7 = S6 if one spot serves both). A character's bind point is
his race's default one, outside Reshanta.

**State of 1131 through the run** (race / occupy count / faction balance; the expectations of X8-X10, X17 and X18 are computed from this
history, never read back from the port): boot BALAUR/0/0 → C3 ASMODIANS/0/0 → ELYOS/0/0 → C5b's stop (defended by ELYOS: +1 occupy, +1
balance) ELYOS/1/+1 → C6 (captured) ASMODIANS/1/0 → C7 (defended) ASMODIANS/2/−1 → C13 (defended; `onDefended` has no cap,
FortressSiege.java:258-262, SiegeLocation.java:111-113) **ASMODIANS/3/−2**.

| # | Case | Where | Steps |
|---|---|---|---|
| C0 | the oracle answers | – | `m5i-sieges --at <planned start> --tz UTC --spots`, `m5i-events` (the pool buff's index, 3; today's UTC day of month d0 and a day d1 ≠ d0), `m5i-rifts`, `m5i-worldraid`, `m5i-bases`, `cron-next` over every job of D13 |
| C1 | seed and boot | watcher → S1, gm → S2 | seed A and B, both characters at level 40 at their spots; **seed the `event` row** (`event_name`='Beyond Aion Server Buffs', `buff_index`=3, `buff_active_pool_ids`='10821', `buff_allowed_days`='d0'); boot inside a G-04 window |
| C2 | login bursts | S1, S2 | both enter the world |
| C3 | a capture without a siege | S1, S2 | gm: `//siege capture 1131 asmodians`, then `//siege capture 1131 elyos` |
| C4 | a siege start | watcher **online at S1**, gm at S2 | gm: `//siege start 1131` |
| C5 | the shield | watcher: bind point → S3 | watcher: quit; seed S3; enter; move M1, then M2 (after `CM_EMOTION(FLY)` if the oracle's M2 is airborne — `CM_EMOTION`'s FLY arm and `FlyController` have 0 unported bodies) |
| C5b | relocation at login | watcher at his bind point after C5 → S1 while **offline**; gm at S2 | gm: `//siege stop 1131`; watcher: quit (lastOnline t1); seed S1; gm: `//siege start 1131` (start t2 > t1); watcher: enter (X8a); watcher: quit (t3 > t2); seed S1; enter (X8b, the control) |
| C6 | a capture during a siege | watcher at S1 (inside, shielded siege), gm at S2 | gm: `//siege capture 1131 asmodians` |
| C7 | a defended siege | **gm: S2 → S4** (1131 is his enemy's now, and `clearLocation` moves staff too, SiegeConfig.java:61-62) | gm: quit; seed S4; enter; `//siege start 1131`; `//siege stop 1131` |
| C8 | an artifact | anywhere (world-wide messages) | gm: `//siege capture 1135 elyos`, then `//siege capture 1135 asmodians` |
| C9 | the artifact cooldown (**Dep M5c**) | watcher: S1 → S5 | watcher (Asmodian, the owner now): quit, seed S5, enter, talk to the artifact and answer both questions |
| C10 | a world raid's first stage | **precondition asserted: both on 400010000** — gm at S4, watcher at S5 (C9 ran) or at S1 (C9 skipped); no re-seed either way | gm: `//worldraid start <L>`; **never stopped** (D8). If the run reaches C10 + 600 s, the minute-10 `STR_MSG_WORLDRAID_MESSAGE_02` arrives and is ignored |
| C11 | rifts | gm: S4 → S6 | gm: quit, seed S6, enter; `//rift open <R>`, `//rift close <R>` |
| C12 | a base (stage 2) | gm at S7 (quit/seed/enter only if S7 ≠ S6) | gm: `//base capture 2120 elyos`; `//base assault 2120`; `//base assault 2120 elyos`; `//base assault 2120 asmodians` |
| C13 | a Balaur assault (stage 2) | **watcher → S1** (inside 1131, his own fortress); gm in Eltnen | watcher: quit, seed S1, enter; gm: `//siege start 1131`, `//siege assault 1131 0`, wait until the watcher's `STR_ABYSS_WARP_DRAGON` or `base_delay` (180 s) + 5 s, `//siege stop 1131` |
| C14 | persistence | watcher S1, gm S7 | stop file; both servers stop (the first shutdown census); **rewrite the `event` row's `buff_allowed_days` to 'd1'**; restart both servers; both log in |
| C15 | reports and shutdown | – | the M5a Q8 bar, the M5i rows |

### 10.3 Assertions

| # | Case | Assertion | Proves / cannot prove | What a wrong port does |
|---|---|---|---|---|
| **X1** | C2 | `SM_SIEGE_LOCATION_INFO` type 0 lists exactly the oracle's **70** location ids, each with race = the default race, legion 0, vulnerable = 2 for the 49 artifacts (always vulnerable, ArtifactLocation.java:17-21) and 0 otherwise, **canTeleport 1 only for the location of the receiver's race — 3111 for the watcher, 2111 for the gm** (SM_SIEGE_LOCATION_INFO.java:67, SiegeLocation.java:156-160), nextState 1 for artifacts, outposts and the agent (ArtifactLocation.java:24-26, OutpostLocation.java:23-25, AgentLocation.java:15-17) and X2's value for fortresses, occupy count 0; in XML order if D10 was accepted | **Proves:** `SiegeService` holds the right locations, `isCanTeleport`, the three `getNextState` overrides, and `onPlayerLogin`'s enabled branch. **Cannot prove:** the emblem arm (legion 0) | a location map built from all 158 templates; `isCanTeleport` ignoring the race (1 everywhere); `onPlayerLogin` still unported (no packet) |
| **X2** | C2 | `SM_INFLUENCE_RATIO`: `secondsUntilNextFortressState` = the oracle's seconds to the next hh:00 ± 2, the three rates and the per-world influences equal the oracle's Influence model; every fortress's `nextState` in X1 equals the oracle's cron evaluation at the receive instant — **0 for all 18 in any G-04 window** | **Proves:** `updateNextStateUpdateTime` ran, the influence formula, `SM_INFLUENCE_RATIO::writeImpl`. **Cannot prove:** that `initSieges` armed the `SiegeStartRunnable` jobs or that `updateFortressNextState` read them — an unarmed server sends the same bytes in every hour G-04 allows (§2.9); **T-02 is the only guard**. Nor the preparation shift (T-01) or the flip (T-04) | `nextStateUpdateTime` null (seconds 0); rates from the wrong location set |
| **X3** | C2 | `SM_RIFT_ANNOUNCE` action 1 with **both silentera flags 1** — `OutpostLocation.isSilenteraAllowed` is the constant `true` (OutpostLocation.java:34-40) — immediately after `SM_AFTER_SIEGE_LOCINFO_475` in the login burst (SiegeService.java:585-586) | **Proves:** the login-burst order and the constant. **Cannot prove:** anything about the outposts' races (the flags do not depend on them) | the flag computed from the fortress dependencies (0); the two packets missing or swapped |
| **X4** | C2 | `CM_LEVEL_READY` in Reshanta is answered by `SM_SHIELD_EFFECT` listing Reshanta's siege locations, all `0` (no shield npc at peace), and `SM_ABYSS_ARTIFACT_INFO3` listing every artifact location of Reshanta — the standalone ones and the fortresses' own, which the data holder also keys by fortress id (SiegeLocationData.java `afterUnmarshal`) — as `locationId × 10 + 1` with status IDLE; **no ERROR** | **Proves:** M5f's `onEnterSiegeWorld` (Dep T-05) with sieges on, and S-05's `ArtifactLocation::getStatus`. **Cannot prove:** a shielded or active artifact state | the §2.1 throw; `getStatus` unported |
| **X5** | C3 | after each capture both characters get one `SM_SIEGE_LOCATION_INFO` for 1131 with the new race and one `SM_INFLUENCE_RATIO` whose rates moved by 1131's influence value (3); the watcher at S1 gets `SM_DELETE` for exactly the oracle's PEACE npcs of the old race in his view and `SM_NPC_INFO` for the new race's; the `siege_locations` row for 1131 holds the race, occupy count 0, balance 0 | **Proves:** `captureSiege`'s no-siege branch, `deSpawnNpcs`/`spawnNpcs`, `broadcastUpdate`, `Influence.recalculateInfluence`, `SiegeDAO.updateSiegeLocation` in that branch. **Cannot prove:** npcs out of the watcher's view | no `broadcastUpdate`; spawning by the old race |
| **X6** | C4 | both get `SM_SIEGE_LOCATION_STATE(1131, 1)`; the watcher (an enemy of the Elyos owner, online at S1) is **moved to his bind point** (the teleport sequence, then his position equals the oracle's bind spot) and gets no balance message (balance 0); the gm at S2 sees the SIEGE npc set of race ELYOS appear, including exactly one boss (263001) and the shield npc (263208), and then `SM_SHIELD_EFFECT(1131, 1)` | **Proves:** `FortressSiege.onSiegeStart`, `clearLocation`, `initSiegeBoss`, `ShieldNpcAI.handleSpawned`. **Cannot prove:** the kisk arm of `clearLocation`; the faction-troop task (1131's boss is level 40, D12) | `clearLocation` left unported or skipping players; SIEGE spawned for the wrong race; no shield |
| **X7** | C5 | entering at S3: no relocation; M1: no death; M2 while shielded: the watcher gets `SM_EMOTION(DIE)` and `STR_MSG_COMBAT_MY_DEATH` (the damage list holds no player, `PvpService.cpp:65-67`), **no `SM_DIE` within 2 s** (the `TaskId.TELEPORT` task `scheduleReviveAtBase` adds at once suppresses it, PlayerReviveService.java:250-259, PlayerController.java:355-361 — M5b-1's P1 expects `SM_DIE` for a normal death, so X7 does not reuse it), then about 2.5 s after the death the bind-revive teleport to his bind point | **Proves:** `FortressLocation.onEnterZone` → `createShieldObserver` → `ShieldObserver.moved` → `CollisionDieActor.kill` → `scheduleReviveAtBase` (task first, then `bindRevive`). **Cannot prove:** the geo path (X7g), the kisk and instance arms | `createShieldObserver` unported (ERROR, no death); `passedThrough` inverted (death at M1); `scheduleReviveAtBase` without the TELEPORT task (`SM_DIE` at +500 ms) |
| **X8** | C5b | **the stop** (defended by ELYOS): `SM_SIEGE_LOCATION_STATE(1131, 0)`, the gm gets `STR_CASTLE_DEFENCE_WIN_BUFF_ON` and 12147, the DB row ELYOS / occupy 1 / balance +1; **the second start**: the gm, inside at S2, gets `STR_MSG_WEAK_RACE_BUFF_DARK_WARNING` (Elyos, balance > 0, FortressLocation.java:93-101); **X8a**: the watcher, who logged out before the start, enters the world **at his bind point**, not at S1 (`validateFortressZone`, PlayerEnterWorldService.java:415-428 / `.cpp:673-690`); **X8b**: logged out after the start, he enters **at S1** unmoved, and on spawning inside the zone gets 8876 (8875 + balance) and `STR_MSG_WEAK_RACE_BUFF_DARK_GAIN` (FortressLocation.java:102-105) | **Proves:** `validateFortressZone` with a real siege start time, both arms — including the unit of `getLastOnline()->time_since_epoch().count()` against the millisecond start time (`PlayerEnterWorldService.cpp:676-678`); `onDefended` for ELYOS; `checkForBalanceBuff`'s ADD arm with a real balance. **Cannot prove:** the no-bind-point arm (initial spawn location) | `getStartTime` 0 or a lastOnline unit mismatch (X8a not relocated, or X8b relocated); the comparison inverted; the balance buff for the wrong race |
| **X9** | C6 | the watcher (Asmodian, the winner, inside at S1) loses 8876 with `STR_MSG_WEAK_RACE_BUFF_DARK_MIST_OFF` (FortressSiege.java:146), then gets 12147 and `STR_CASTLE_WIN_BUFF_ON`; the gm (Elyos, the old owner) gets `STR_ABYSS_CASTLE_TAKEN`, the watcher `STR_ABYSS_WIN_CASTLE`; each character gets **exactly two** `SM_SIEGE_LOCATION_INFO(1131)` + `SM_INFLUENCE_RATIO` pairs (onSiegeFinish's, FortressSiege.java:153, and captureSiege's, SiegeService.java:261), both with race ASMODIANS, vulnerable 0; both (on 400010000) get `SM_SHIELD_EFFECT(1131, 0)`; the DB row ASMODIANS / occupy 1 / balance 0 (+1 − 1) | **Proves:** `captureSiege`'s siege branch, `onSiegeFinish` → `onCapture`, `applyWorldBuffs`, the balance-buff removal, the loser/winner messages, `adjustFactionBalance`, the shield going down on despawn, the DB update inside `onSiegeFinish` (the only one in this branch, SiegeService.java:239-243). **Cannot prove:** the reward mails (no participants have AP) — T-03 must | `onCapture` not called (no buff); messages to the wrong race; balance adjusted the wrong way; one pair or three |
| **X10** | C7 | the gm is **not** moved (at S4, outside); `SM_SIEGE_LOCATION_STATE(1131, 1)` then, after the stop, `SM_SIEGE_LOCATION_STATE(1131, 0)`; the watcher (owner, inside at S1, balance 0 so no balance message) gets `STR_CASTLE_DEFENCE_WIN_BUFF_ON` and 12147; the DB row ASMODIANS / occupy 2 / balance −1 | **Proves:** `stopSiege` without a boss kill → `onDefended`, `increaseOccupiedCount`, `broadcastState`, the DB update | `onDefended` skipped; a defended siege treated as captured |
| **X11** | C8 | first capture: the gm gets `STR_GUILD_EVENT_WIN_ARTIFACT`, the watcher `STR_GUILD_EVENT_LOSE_ARTIFACT`, both `SM_SIEGE_LOCATION_INFO(1135)` ELYOS; the **second** capture again produces the artifact messages (not the silent no-siege branch) | **Proves:** the artifact's siege is endless and **restarted** in `onSiegeFinish` (ArtifactSiege.java:59), with a boss found each time. **Cannot prove:** a player-damage winner name | `startSiege` not called after finish (the second capture takes the no-siege branch and sends no artifact message) |
| **X12** | C9 (Dep M5c) | the two `SM_QUESTION_WINDOW`s, then `STR_CANNOT_USE_ARTIFACT_OUT_OF_ORDER` and **no** `SM_USE_OBJECT` | **Proves:** `ArtifactAI` and `setInitialDelay`'s 900 s cooldown after a capture. **Cannot prove:** the activation itself (15 minutes) — T-06 | cooldown computed from 0 (activation starts) |
| **X13** | C2, C14 | **X13a** (C2, fixture day d0 = today): after `CM_LEVEL_READY`'s `EventService.onEnterMap` (CM_LEVEL_READY.java:106) each character gets the `[Server Buff]` `SM_SYSTEM_MESSAGE` 1400697 and an `SM_ABNORMAL_STATE` with **10821** (the fixture's pool), **neither 18141 nor 21258** (restricted to group/alliance instances with a team, §2.4), and `SM_STATS_INFO`'s max HP equals the M5b-2 oracle's value (no +1500). **X13b** (C14, fixture day rewritten to d1): after `onEnterMap` no pool skill in the character's effect list and no `[Server Buff]` message; the `event` row still holds pool 10821 and days d1 | **Proves:** `EventService` with a real active set, `Event.start`, `EventBuffHandler`'s stored-data path (EventBuffHandler.java:52-70), `onEnterMap`/`tryBuff`/`endRestrictedEventBuffs`, `isAllowedToday`, `isAllowedOnCurrentMap`, `isAllowedTeamSize`, `XPBoostEffect::calculate`, the force type. **Cannot prove:** the kill procs (0.1 % and 2 %), the ENTER_TEAM trigger (M5g), the monthly re-roll (T-10). The expectation is the fixture's, never a row the port wrote | the map/team restriction ignored (18141/21258 present, +1500 HP); the stored row ignored (a fresh roll: 10821 absent in X13a with probability ≥ 29/30); buffs applied at login instead of at `CM_LEVEL_READY` |
| **X14** | C10 | within 2 s the gm and the watcher (both on 400010000) get `STR_MSG_WORLDRAID_MESSAGE_01` and the gm at S4 `SM_NPC_INFO` of npc 832819 at L's spot | **Proves:** `startRaid`, the 60 s fixed-rate task's minute 0, the map-bounded broadcast. **Cannot prove:** minutes 10-30 and the boss — T-09 | the task scheduled with its first run at 60 s; the message sent world-wide |
| **X15** | C11 | after `//rift open`: `SM_RIFT_ANNOUNCE` action 2 for the master rift with the oracle's entries/levels and `SM_NPC_INFO` of the master and slave npc ids of `RiftEnum`; after `//rift close`: `SM_RIFT_ANNOUNCE` action 4 and `SM_DELETE` of both | **Proves:** `openRifts`, `RiftManager.spawnRift`, the `RiftEnum` companion, `RiftInformer`, `closeRift`. **Cannot prove:** the cron path and the auto-close — T-08 | the stand-in left in place with wrong ids |
| **X16** | C12 (stage 2) | on entering Eltnen the gm sees the FLAG and SENTINEL npcs of occupier BALAUR for base 2120 (default occupier; oracle `m5i-bases`); `//base capture 2120 elyos` → `SM_DELETE` of those and `SM_NPC_INFO` of the ELYOS FLAG/MERCHANT/SENTINEL set; `//base assault 2120` → the `SM_MESSAGE` "Not enough parameters." and nothing spawned; `//base assault 2120 elyos` → "Base cannot be assaulted by the same occupier" and nothing spawned; `//base assault 2120 asmodians` → `SM_NPC_INFO` of the ASMODIANS ATTACKER set (231617) (BaseCommand.java `assaultBase`, `parseBaseId`) | **Proves:** `BaseService` locations, `Base.handleStart/handleStop`, `capture`, the new-object restart, `spawnBySpawnHandler(ATTACKER, occupier)`, `BaseCommand`'s refusals. **Cannot prove:** the timers — T-07 | the old base not stopped (both flags present); the occupier parameter ignored (a spawn on a refused command, or the wrong race's attackers) |
| **X17** | C13 (stage 2) | the watcher (inside at S1) gets `SM_SIEGE_LOCATION_STATE(1131, 1)` and, balance −1 for an Asmodian, `STR_MSG_WEAK_RACE_BUFF_LIGHT_WARNING` (FortressLocation.java:103-107); on `//siege assault 1131 0` both get `SM_NPC_ASSEMBLER` and `STR_ABYSS_CARRIER_SPAWN` at once (BalaurAssaultService.java:122-131); within `base_delay` + 5 s the watcher gets `STR_ABYSS_WARP_DRAGON` (sent only to players inside the location, FortressAssault.java:73, 116-118), every `SM_NPC_INFO` of an ASSAULT npc has an id in the oracle's TELEPORT set of 1131 ({276793}), and at least one arrives when G-01 counts n ≥ 12 eligible npcs in view (risk 15); after `//siege stop 1131` `SM_DELETE` of every assault npc the watcher saw (they are siege spawns of 1131, deleted by `deSpawnNpcs`, SiegeService.java:505-509) and, the stop being a defence, 12147 and `STR_CASTLE_DEFENCE_WIN_BUFF_ON` for the watcher | **Proves:** `BalaurAssaultService.startAssault`, `FortressAssault` (difficulty, `scheduleSpawns`, the teleport wave), the assault npcs being siege spawns. **Cannot prove:** the random assault roll (D4), later waves, the capture branch | waves never spawned; the first-wave delay outside `[minSpawnDelay, base_delay]` |
| **X18** | C14 | after the restart X1 shows 1131 ASMODIANS with **occupy count 3** and the DB row **balance −2** (the history of §10.2; a stage-1-only run without C13 expects 2 and −1); 1135 ASMODIANS; every other row as at C2; base 2120 back to BALAUR (bases are not persisted: the gm at S7 sees its BALAUR FLAG again) | **Proves:** `SiegeDAO` load and update end to end, the uncapped occupy count, and that bases reset like Java's | a missing `updateSiegeLocation` (1131 back to BALAUR); an occupy count capped at `maxOccupyCount` (2) |
| **X19** | C14's shutdown, C15 | `unported_trace.txt` empty; **no ERROR** in either log of either process; lockdep empty; no watchdog dump; `partial_trace.txt` ⊆ the allow-list; the G-08 counts **per process**: at the first shutdown `Siege` 49, `Base` 31, `WorldRaid` 1 (C10's raid), `FortressAssault` 1 (D4's orphan); at the second `Siege` 49, `Base` 31, `WorldRaid` 0, `FortressAssault` 0 | **Proves:** nothing on the scripted path fell outside the port; no object leaked beyond its service. **Cannot prove:** leaks over time — the soak proposal | any unported body; a `Base` leaked per capture; a raid persisted across the restart |

### 10.4 Mutation proof

Every row above must be watched failing with the mutation named and both outputs quoted; the minimum set, including what the gate cannot catch:

| Mutation | Must fail | Must stay green |
|---|---|---|
| `getPreparationCronString`: subtract 0 minutes | **T-01** | the gate (no run falls in a pre-preparation hour, D13) |
| `updateFortressNextState`: `after` → `before` | **T-04** | the gate (same reason) |
| `SiegeStartRunnable` stored as a lambda instead of the named type | **T-02** | **X2** (an unarmed server sends the same bytes, §2.9), X5-X18 |
| `FortressLocation::clearLocation`: skip players | **X6** | X5, X9 |
| `captureSiege`: drop its `broadcastUpdate` | **X5** (the only broadcast of the no-siege branch), **X9** (one pair instead of two) | X6, X10 |
| `SiegeDAO` update skipped in `FortressSiege::onSiegeFinish` | **X9**, **X10**, **X8** (the DB rows), **X18** | X5 (the no-siege branch writes its own row) |
| `FortressSiege::onSiegeFinish`: skip `applyWorldBuffs` | **X8**, **X9**, **X10**, **X17** | X5, X6 |
| `onDefended`: cap the occupy count at `maxOccupyCount` | **X18** (2 instead of 3) | X8-X10 |
| `ArtifactSiege::onSiegeFinish`: skip the restart | **X11** | X5-X10 |
| `ShieldNpcAI::handleSpawned`: do not `setUnderShield(true)` | **X6** (no `SM_SHIELD_EFFECT(…,1)`), **X7** (no death) | X5, X9's race rows |
| `ShieldObserver::moved`: invert `passedThrough` | **X7** | everything else |
| `scheduleReviveAtBase` without the `TaskId.TELEPORT` task | **X7** (`SM_DIE` appears) | the rest |
| `validateFortressZone`: compare `>=` instead of `<` | **X8** (X8a not relocated, X8b relocated) | X6, X7 |
| `EventBuffHandler::isAllowedOnCurrentMap`: always true | **X13a** (18141/21258 appear) | X1-X12 |
| `EventBuffHandler::initBuffData`: ignore the stored row | **X13a** or **X13b** (a fresh roll) | the rest |
| `WorldRaid`: first run at 60 s | **X14** | the rest |
| `RiftService::openRifts`: skip `RiftManager::spawnRift` | **X15** | the rest |
| `BaseService::capture`: forget `setOccupier` | **X16** | stage-1 rows |
| `BaseCommand::assaultBase`: ignore the occupier parameter | **X16** (the refused command spawns) | X16's capture half |
| `Base::handleStop`: do not cancel tasks | **T-07** — **not the gate** | X16 |
| `SiegeRaceCounter` ranking reversed | **S-04's unit test** — nothing in the gate (no AP in any case) | gate green |
| `SiegeShield::onEnterZone`: never attach the `CollisionDieActor` | **X7g** (geo gate) | `gs.scenario.m5i` (sphere path) |

### 10.5 The geo gate (`gs.scenario.m5i_geo`)

Same script, `gameserver.geodata.enable=true`, `LABELS "scenario;realdata;geo"`, the same lock. **Geo changes the shield mechanism, and that is
the geo gate's own row.** With geo on, `ZoneService` registers the geo shields (ShieldService.java:59-65) and `attachShield` removes 1131's
sphere template when a geo shield is attached to it (ShieldService.java:70-88), so the kill of C5 must come from `SiegeShield.onEnterZone`
→ `CollisionDieActor` (SiegeShield.java:39-48, CollisionDieActor.java) instead of `ShieldObserver`. **X7g:** the same crossing kills the
watcher with geo on. The sibling fact that makes it testable: with sieges enabled `SiegeService.getFortress(siegeLocationId)` is no longer
null, so the Java null dereference that `SiegeShield.cpp:33-35` keeps on purpose is no longer reached by a player. **Also new with sieges on:**
a SHIELD `DespawnableNode` stops colliding while its fortress is at peace (`DespawnableNode.cpp:84-103`), so the oracle's spots and the
M1/M2 pair must be computed with that rule in the geo variant. **Verify before writing X7g** that 1131 has a geo shield (m5a-client-session.md
F-1 found six Reshanta npcs inside shield zones, which suggests yes); if it does not, pick the fortress G-01 finds with one, or state that the
geo gate re-runs the script.

### 10.6 The timeline tests (in-process, `DeterministicExecutor` + `ManualClock`)

The answer to "without waiting for real time" for everything the scenario cannot wait for. Each runs the ported bodies on a test world and
advances the clock; fixtures (cron strings, npc ids, delays) come from the oracle's JSON output, never from the port.

| # | Test | What it proves | Mutation it kills |
|---|---|---|---|
| T-01 | `getPreparationCronString` over every siege time of the schedule, the Panesterra −10, a minute underflow and the midnight refusal | the preparation shift | subtract 0; wrong underflow |
| T-02 | `initSieges` arms 21 + 1 `SiegeStartRunnable` jobs and the hourly job; `findNextFireTimes` equals the oracle's dates | **the arming — the only test that can** (§2.9) | a lambda instead of the named type |
| T-03 | (a) 1131 at Tue 16:54:59 UTC → +1 s preparations → +300 s start → +3,600 s stop without a kill → `onDefended`; with AP from two players first → `sendRewardsToParticipants` sends the oracle's mails and GP; `maxOccupyCount` reached → reset to Balaur at preparation. (b) A siege end of 10111 with a non-Balaur winner pays grades 1-4 and logs the `getLevelById(5)` ERROR twice (D8) | the fortress timeline, rewards, the Java bug pinned | 300 s → 0; duration ignored; a "fixed" `getLevelById` |
| T-04 | at 15:59:59 → 16:00:00 the next state of 1131 flips VULNERABLE and every online player gets 2 × 18 `SM_FORTRESS_INFO` + `SM_FORTRESS_STATUS` | the hourly job | `before`/`after` swap |
| T-05 | agent fight 8011: direct start, 9 × 60 s chain, agents at +9 min, stop at 4,200 s; with no winner `onSiegeFinish` returns before `BaseService.capture` (AgentSiege.java:71) | `AgentSiege` | chain count |
| T-06 | 49 artifact sieges started by `initSieges`; capture → restart; activation refused for 900 s and accepted after; the 10 s + 13 s activation with the item removed and skill 12042 applied | artifacts | cooldown from 0 |
| T-07 | a CASUAL base with seeded `Rnd`: outrider, boss, assault cycle; with the region inactive a 20 % capture restarts the base; `stop` cancels all four tasks; 100 captures reclaim every old `Base` | bases | tasks not cancelled |
| T-08 | `RiftOpenRunnable`: the 50 % skip, 1-4 locations, auto-close after `duration × 3540` s | rifts | close delay |
| T-09 | world raid: minutes 0/10/25/29/30, boss despawn after 1 h, boss death stops the raid; **stop before minute 30 throws and leaves the preparation task running (D8)** | world raids, the Java bug pinned | a "fixed" stop |
| T-10 | events: the 5-minute cron starts an event at its start date and stops it after its end; the permanent event's day rollover re-rolls the pool (and on day 1 the allowed days) and stores it | events | no restart of buffs |
| T-11 | Balaur assault waves: the first wave inside `[minSpawnDelay, base_delay]`, the teleport wave on eligible npcs only; (b, stage 3) vortex start and stop after `duration` hours | assault, vortex | a wave delay from 0 |
| T-12 | CP kills-decrease timer; `onEnterZone` in an occupied dominion zone sets LD rank 3 | CP | – |
| T-13 | Legion Dominion (LD-01): `join`, `store`, `updateRanking` with seeded participants; `isInCalculationTime` at Wednesday 07:59 / 08:00 / 10:59 / 11:00 server time; `openInvasionRift` refused inside that window and, outside it, opening the location's rift and closing it after `duration × 3540` s (LegionDominionService.java:165-192) | the dominion share | the calculation window off by an hour |
| T-14 | assault off: a `//siege assault`-style `startAssault`, then `stopSiege` — the `FortressAssault` stays in `fortressAssaults` and a second `startAssault` returns false (D4) | the faithful orphan | a "fixed" cleanup |

### 10.7 The soak (`gs.scenario.m5i_soak`) — a proposal to the user (D17)

65 minutes, every system enabled including the shipped assault setting, three idle clients (Reshanta, Eltnen, Verteron), under ASan, in a
slot the user chooses. Its window avoids the siege preparations, the agent fight, the world-raid starts, Wednesday 09:00 and midnight, and
crosses at least one hh:00. Asserts: each client received the hourly `SM_FORTRESS_STATUS` within 2 s after hh:00; no ERROR; LeakCensus and the
zombie breaker silent (risk 6); IDFactory used-count back to baseline; `Base` created − captures = 31 live.

---

## 11. Real-client checklist (user, after stage 2)

Prerequisites as m5b2-plan.md §11, with `mygs.properties` from the real-client half of `m5i.properties.example` (sieges, rifts, world raids,
events and CP on; assault as shipped; vortex off until stage 3; Ahserion and Moltenus far future) and:

- **`access_level = 9`** on your account in `aion_ls.account_data`;
- **`gameserver.administration.login.execute_commands =`** (empty) — with Java's default the GM login throws (§2.6) until M5j's D4 lands,
  and after it you would log in invisible and invulnerable;
- for step 10, **one Abyss item 188020000** in your character's inventory, inserted through the database with the character logged out (an
  `inventory` row; the gate's `ScenarioDatabase` helper shows the columns, as in m5b3-plan.md §11 step 10) — no ported command can give it.

1. Start the servers: no ERROR at startup, and "Initializing sieges..." and "Initializing bases..." in the log (SiegeService.java:77,
   BaseService.java:31).
2. Log in: **no** Warrior's Courage / Attack Boost in the open world — they are group/alliance-instance buffs (custom_events.xml:15-19). The
   server rolled one day this month for the XP/gathering/crafting energy; if today is that day (or you set `buff_allowed_days` in the `event`
   table to today while the server was stopped), a `[Server Buff]` message and the buff icon appear.
3. Go to Reshanta (fly or teleport after M5f; otherwise set your position in the database while logged out). **No error when the map loads**.
   Open the Abyss map: every fortress and artifact shows its owner, the influence bar is filled.
4. Type `//siege locations`: the list comes back in the chat window.
5. `//siege capture 1131 elyos`: the fortress changes colour on the map; the guards around it change.
6. `//siege start 1131` while standing inside with an Asmodian: the Asmodian is thrown out to his bind point; the shield dome appears.
7. Fly into the dome as the enemy race: you die and revive at your base a moment later, **without** the resurrection dialog.
8. `//siege capture 1131 asmodians`: capture messages for both races, the Siel's buff icon on Asmodians.
9. `//siege start 1131` then `//siege stop 1131`: the defence message and buff.
10. `//siege capture 1135 elyos` twice with different races: the artifact messages both times. Talk to the artifact right away: it refuses;
    **after 15 minutes** it accepts, takes the Abyss item and casts its shield.
11. Log out, restart the server, log in: the fortress and the artifact still belong to whoever took them last.
12. In Eltnen: `//rift list`, then `//rift open <an id it shows>` (or `//rift open 210020000` for every Eltnen location) — a rift appears and
    can be used (question window, teleport); `//base capture 2120 elyos` changes the base's flag and merchants; `//base assault 2120 asmodians`
    brings an Asmodian attacker; `//worldraid start 7` puts a flag and a message on the map — **do not stop it** (D8); its boss comes 30
    minutes later.
13. Leave the server running an hour with nobody online, then look: bases have changed hands on their own, rifts opened on the hour, and the
    log has no ERROR. (This is the only real-time proof of the cron wiring unless you allow the soak, D17.)
14. **Expect, not report:** fortress gates do not open by dialog and mercenaries, cannons, springs and siege teleporters do nothing (their AIs
    are phase 6, D5); `//siege start 10111` (a Panesterra fortress) logs a reward ERROR when it ends — Java does this (D8); `//worldraid
    stop` before minute 30 logs an ERROR and leaves the boss behind (D8); across Wednesday 09:00 one ERROR if M5h's S-08 is not in yet.
15. Send `game-server/log/`, `live_counts.txt`, `partial_trace.txt`, `unported_trace.txt`.

---

## 12. What was measured and what was inferred

**Measured** (grep or a parse over the two trees at HEAD `760e8ab5c`; re-runnable):

- Every `AION_UNPORTED` / `AION_PARTIAL` count of §2.2 per file and per subsystem (107 / 198 + 1), the 20 no-file classes and their method
  lists, the Java line counts (3,553 / 4,967), and the out-of-chunk bodies of §2.6/§2.7, each checked in its own file — including rev 2's
  `LegionDominionService.cpp:52-74`, `LegionDominionLocation.cpp:61-86`, `ConquerorAndProtectorService.cpp:107-177`, `RiftLocation.cpp:17-38`,
  `SiegeLocation.cpp:37-59`, `GMService.cpp:59-68`, `PlayerEnterWorldService.cpp:524-528`, and the absence of any `getLevelById` or
  `AbyssSiegeLevel`/`SiegeResult` companion.
- The startup order in both trees and `main.cpp`'s handling of an `UnportedException` at startup; the config keys, defaults, shipped values
  and the user's `mygs.properties`; which bodies each enabled branch reaches first; the 68 boot cron jobs.
- The three always-on hazards: `CM_LEVEL_READY.cpp:96-99` → `SiegeService.cpp:280-281`; `CronJobService.cpp:86, 175-177` →
  `PanesterraService.cpp:38-39`; `CronJobService.cpp:88, 185-187` → `LegionDominionService.cpp:60-61`.
- The data: 158 siege templates (18 FORTRESS, 49 ARTIFACT, 2 OUTPOST, 1 AGENT_FIGHT, 85 INDUN, 3 UNDERPASS) → 70 service locations and 49
  standalone artifacts; 1,454 PEACE spots at the default races; exactly one BOSS per (location, race, mod) wherever one exists; 1131's and
  1135's bosses, shield npcs, activation data and assault data (`base_delay` 180, TELEPORT 276793); the reward grades (4 locations with 5,
  17 with 4, 137 with none); 42 base locations and the 31 started; base 2120's occupier groups (ELYOS 7, ASMODIANS 7, BALAUR 5; ATTACKER
  231616 / 231617 / 231607); 60 rift locations in 8 worlds; 16 rift, 22 siege, 23 world-raid cron entries (world raids all at hh:30; 5
  locations in Reshanta); 76 distinct event names, one without dates; the permanent event's four buffs and their restrictions; 18141 =
  `<change stat="MAXHP" func="ADD" value="1500"/>`; the effect classes of every world, balance, event buff and artifact skill against
  M5b-2's subset.
- The AI names of every siege, base, rift, mercenary and Ahserion spawn and their Java handler files and sizes; the 21 files of
  `handlers/ai/siege`.
- 146 client packets with no C++ file; the server packets of §1 with 0 `AION_UNPORTED` except the two named; `CM_EMOTION` (FLY arm) and
  `FlyController` with 0 unported bodies.
- The existing cycle rows of §2.5; the chat-command framework's state and the command access levels; `BaseCommand`'s parameters and
  refusals; the C++ logging's single root level; an empty list property parsing to an empty list.
- The Java bugs of D8, read line by line; the D4 orphan, read in `Siege.java:75-86` and `BalaurAssaultService.java:56-104`.
- The sibling plans' claims, at the line numbers §3 and §3.1 cite, as they stood at 23:30.
- The measured pace of §9, from `git log`.

**Inferred, and a lane should confirm before relying on it:**

- **The wall-clock estimates of §9** (30-50 hours), from two measured milestones; and the effort letters.
- That the census treats `Ref<Npc>` holders as risk 6 describes (read from runtime-architecture.md:639-640, not run).
- That 1131 has a geo shield (§10.5) and that the oracle can find S1-S7, the M1/M2 pair and n ≥ 12 (risks 10 and 15).
- That `cancelTask(true)` from inside a running base task is safe in the C++ scheduler (B-01).
- That M5c/M5d/M5f/M5g/M5h deliver what §3 lists, and that the sibling plans do not move again before branch time.
- LD-01's 14 bodies, from m5h-plan.md's own count and the unported sites; `LegionDominionIntruderUpdateTask` being dead in Java (one grep
  over `src` and `data/handlers`).

---

## 13. Open questions this analysis could not settle without building

1. How the final census and `CheckOutput` treat objects held by immortal singletons (`SiegeService.activeSieges`, `BaseService.activeBases`,
   `BalaurAssaultService.fortressAssaults`) — G-08 proposes a rule; the existing rules were not read in full.
2. Whether D10 is accepted; it decides whether X1/X4 assert order.
3. Whether a C++ breaker is wanted for risk 6 (a finished siege pinned by its faction-troop task, an orphaned assault's boss), or Java's
   retention is accepted and the census exempts it.
4. Which of the Dep rows of §3 hold at branch time, and therefore which of the fallbacks (Z-01, Z-03, LD-01's S-08 half) run.
5. Whether `WorldRaid`'s stop bug should be fixed after all: it is an ERROR plus an untracked boss on every early `//worldraid stop` (D8 keeps
   Java's behaviour; a fix would be a deviation the user asks for).
6. Whether the user allows the soak (D17), and in which slot.

---

## 14. Review, 2026-09-23

The adversarial review returned **needs-revision** with 2 high, 11 medium and 10 low findings. Each was re-checked against the source before
it was applied; every one was confirmed, three with a correction. Rev 2 also fixes four defects the review did not name.

| # | Sev. | Finding | Re-check | What changed |
|---|---|---|---|---|
| 1 | high | X2 cannot fail for "schedules not armed" | **Confirmed**, with one nuance: artifacts, outposts and the agent send `nextState` 1 whatever is armed, so it is the 18 fortresses that read 0 in both servers (SiegeLocation.java:33, SiegeService.java:309-320) | X2's Proves column no longer claims the arming; §10.4 lists the lambda mutation as T-02's alone with X2 green; §2.9 says why. The optional assertion of the DEBUG "Scheduled siege …" lines is **not** added: the C++ logging has a root level only (`Logging.h:34, 65-66`), so it would need a DEBUG root for the whole run |
| 2 | high | §3 conflicts with sibling plans (double claims) | **Confirmed** at the current line numbers (m5f T-05 is now :404; m5g K-04 :551; m5h S-06 :458) | Dep rows for M5f T-05, M5g K-04, M5h S-06, M5d A-01; S-01 23 bodies, E-02 10, A-03 5 handlers; Z-01 and Z-03 are fallbacks (Z-01 now also with `ChatBanService` 5, as m5g's D14 has it); stage 0 = Z-02, Z-04, Z-05 (+ Z-06); D9 settled; the stale §12 bullet removed. **Beyond the finding:** §3.1 lists everything the siblings hand to M5i and decides each (D14-D16), including the fallback items m5j's 23:30 revision planned for rev 1's gaps (S-09, S-12, S-13), three of which rev 2 now takes |
| 3 | medium | Nobody owns the Wednesday 09:00 cron | **Confirmed for rev 1; overtaken**: m5h-plan.md was revised after rev 1 and now owns the calculation (S-08, D13: :419, :460). Correction: `startWeeklyCalculation` itself calls no `RiftService` body; `openInvasionRift` does (LegionDominionService.java:170-192) | The owner is recorded (M5h; Dep row; LD-01 takes the five bodies if S-08 is missing). LD-01 (the rest of the dominion share, D14) depends on R-01. D13/G-04 and the soak's rule include all three `CronJobService` jobs |
| 4 | medium | The GM account wakes an `AION_PARTIAL` | **Confirmed, and worse**: `execute_commands` is not "if set" — Java's default and the shipped value are non-empty (AdminConfig.java:77, admin.properties:101), so a staff login throws at `GMService.cpp:65` | §2.6 row "a staff login"; Z-06 closes the partial in stage 0 (unless M5j K-02 did); both profiles set `execute_commands` empty (D2, §10.1, §11) |
| 5 | medium | C12 `//base assault 2120` does nothing | **Confirmed** (BaseCommand.java `parseBaseId(…, 3)`) | C12/X16 use `asmodians`, add the "Not enough parameters." and same-occupier refusals |
| 6 | medium | X8's precondition is never produced | **Confirmed** (lastOnline is set at logout, PlayerLeaveWorldService.java:132) | New C5b: stop, logout, seed, start, login (X8a), and the control login (X8b) |
| 7 | medium | X7 expects `SM_DIE` | **Confirmed** | X7 asserts `SM_EMOTION(DIE)`, `STR_MSG_COMBAT_MY_DEATH`, no `SM_DIE` within 2 s, the revive at ~2.5 s; the TELEPORT-task mutation added |
| 8 | medium | X18 ignores C13 | **Confirmed** | §10.2 carries 1131's history; X18 expects occupy 3 / balance −2 (2 / −1 without C13) and kills the "capped" mutation |
| 9 | medium | Cases never place the characters | **Confirmed** (`ignore_staff_on_location_clear` false, SiegeConfig.java:61-62) | Spots S1-S7, a Where column, the gm moved to S4 before C7, the watcher back at S1 for C13, C10's precondition independent of C9 |
| 10 | medium | X13 reads its expectation from a DB row the port wrote | **Confirmed**; and the review's premise that 18141/21258 apply is itself wrong (see A below) | X13 uses a seeded `event` row (C1) rewritten between processes (C14); asserts after `CM_LEVEL_READY` |
| 11 | medium | Two enum companions missing; the Java bug on four fortresses | **Confirmed** (AbyssSiegeLevel.java:23-30; 4 locations with 5 grades) | X-05 adds `AbyssSiegeLevelInfo.h`, `SiegeResultInfo.h`; D8 row; T-03(b); checklist step 14 |
| 12 | medium | Stage 2 lanes neither disjoint nor parallel | **Confirmed** (AgentSiege.java:71: the capture needs a winner only phase-6 AIs set) | Gate and regate merged into one lane; fixups a part of its own (2B); G-05 depends on B-02; AG-01 no longer on B-01; stage 2 estimated serial behind B-01 |
| 13 | medium | The assault-off profile orphans a `FortressAssault`; the first wave takes ~3 min | **Confirmed**; the wait is bounded by `base_delay` (the delay is drawn from `[minSpawnDelay, base_delay]`, FortressAssault.java:58) | D4 documents the orphan (deviation, X19 count, T-14, risk 6); D13's 12-minute budget includes the 180 s |
| 14 | low | X3 cannot fail on the swap | **Confirmed** | X3 asserts both flags 1 and the order; the swap mutation replaced by a computed flag |
| 15 | low | Two mutation rows contradict X9 | **Confirmed** | X9 asserts exactly two pairs; the `SiegeDAO` row lists X9 (and X8, X10, X18) under must-fail |
| 16 | low | X19 ignores the restart | **Confirmed** | Counts per process |
| 17 | low | Four missing lesson-2 entry points | **Confirmed** | §2.6 rows for the rift search in quests, the dominion zone CP arm, `isCanTeleport` from teleports, the SHIELD `DespawnableNode` change |
| 18 | low | Siege AI lists incomplete | **Confirmed** | D5 names `FortressGateAI` and counts 9 + 2 + 10 of the 21 files |
| 19 | low | `CM_SHOW_MAP`; stage-0 concurrency with M5h | **Confirmed** | D15 (E-04); §6 note qualifies stage 0 against m5h's leases |
| 20 | low | D3's claim about earlier gates' maps | **Confirmed** (m5f-plan.md:647 C22, :715 G2) | D3 reworded; G-07 names every gate |
| 21 | low | The `WorldRaid$1#this` cycles row is wrong | **Confirmed** | §2.5; S-08 corrects it |
| 22 | low | Citation and count corrections | **Confirmed** | `AccountService.cpp:44`; 68 cron jobs; the Ahserion command out of the counts; X1's canTeleport byte; the rifts-raids lane shows its P4-11b lease; §12's two inferences now measured |
| 23 | low | Uncalibrated days | **Confirmed** | §9 converts with the measured pace; stage 1 planned as three parts |
| 24 | low | The activation item | **Confirmed** | §11 prerequisite: insert 188020000 through the database |

**Found while applying the review (not in it):**

- **A. Rev 1's "+1500 HP for every player" was wrong.** The permanent event's 18141/21258 buff is restricted to group and alliance
  instances with a team (custom_events.xml:15-19; EventBuffHandler.java:246-280; every world map's instance has `maxPlayers` 0,
  WorldMap.java:33-35). X13 now asserts their absence; §2.4, risk 10, §11 step 2 and §12 corrected.
- **B. The soak broke the standing resource rule** ("no stress, soak or ASan run without asking", capacity-proposals.md:654-656). G-09 is a
  proposal to the user (D17), which moves one question to the user (§13 item 6).
- **C. The §2.6 AI row was stale**: `portal`/`portal_dialog` are M5f's and `simple_abyssguard` M5d's.
- **D. Checklist step 12 opened a rift with a base id (2120)**; it now uses `//rift list` or Eltnen's world id.
