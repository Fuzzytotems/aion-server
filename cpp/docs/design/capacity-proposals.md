# Capacity testing: the integrator's proposals

> **Status: PROPOSALS, not decisions.** Written 2026-09-23 as a **read-only** analysis over HEAD `c1edb0afb` ("M5b-2 stage 1 part 2") plus
> the M5b-2 stage-1 part-3 working tree (effect classes and the `AttackUtil` effect half being ported by other lanes), and **revised the same day
> after an adversarial review** (§13 lists what the review found and what changed). **Nothing was built, configured or run for this document.**
> Every statement about code comes from reading the two trees; every number about data comes from a throw-away script over the Java data files;
> every number about performance is quoted from an earlier measurement with its source. §12 separates what was measured from what was inferred.
>
> **Why this document exists.** The user deferred capacity testing until M5b-2 (abilities) is in and wants to design it **together**: they have
> stress scenarios of their own, and the integrator brings proposals from where the port is most likely to break (m5b-plan.md, the note before
> §10; phase5-roadmap.md, last paragraph). This is the integrator's side of that conversation. **§4.10 is an empty table for the user's
> scenarios**, §11 lists the questions only the user can answer, and nothing below is scheduled. A capacity run loads the user's working machine
> and therefore starts only in a slot the user chose (§8.4).
>
> **Path convention.** C++ paths without a prefix are relative to `game-server/src/aion/gameserver/`; paths starting with `game-server/`,
> `commons/`, `docs/` or `tools/` are relative to `cpp/`. Java paths are relative to `game-server/src/com/aionemu/gameserver/` (or
> `commons/src/com/aionemu/commons/` where they say `commons`). Data paths are relative to `game-server/data/static_data/`.

---

## 1. Summary

**Nobody has measured the real server under N concurrent players.** What exists is (a) a **churn** run, `gs.scenario.m5a_stress`: 20 fake
clients cycling login, enter world, a four-step walk and logout for 30 minutes, which shakes out lifetime bugs and whose "20" is not a capacity
figure; (b) the kernel prototypes P2 and P4, which measured the runtime against a **synthetic model** of npcs, known lists and packets, not the
ported server; and (c) startup timings. **No Release build of the server has ever been built or run** (§8.1). §2 lists what exists with what each
item can and cannot say.

**Where the port is most likely to break, ranked** (§3 has the evidence):

1. **It degrades silently.** Fixed-rate tasks that fall more than 10 periods and 2 s behind are coalesced without a log line or a counter
   (`runtime/sched/Future.cpp:230-232`, docs/DEVIATIONS.md:165); the packet processor's "Lag detected!" checker never runs at the default 4/4
   threads (`commons/src/aion/commons/network/PacketProcessor.cpp:43-44`, `configs/network/NetworkConfig.cpp:19-20`); the FIFO managers' "added
   faster than they can be executed" warning needs a **growing** number of distinct tasks and stays silent for a steady crowd
   (`taskmanager/AbstractFIFOPeriodicTaskManager.h:107-110`); the only per-task statistics are count/total/min/max, dumped at shutdown
   (`commons/.../RunnableStatsManager.h:11-14`, `ShutdownHook.cpp:150`). A knee can be crossed with nothing in the log. **The first capacity
   work item is therefore instrumentation, not a load test.**
2. **`MovementNotifyTask` is one serial loop with an O(N²) inner cost in a crowd.** Every 500 ms it walks the known list of every creature
   that moved and runs `as<Npc>` on each entry (`taskmanager/tasks/MovementNotifyTask.cpp:40, 54-61`), under one `SYNCHRONIZED(*this)`
   (`AbstractFIFOPeriodicTaskManager.h:85-111`). `Npc` is not `final` (`model/gameobjects/Npc.h:45`; `SiegeNpc`, `SummonedObject` derive from
   it), so each failed check is a `dynamic_cast` (`runtime/lifetime/Ref.h:384-391`), measured at ~220 ns on MSVC (runtime-architecture.md §21).
   With N players in one spot that is N × (N − 1) failed casts per run. And every notification about a moving **player** allocates a
   reference-counted `QuestEnv` that the Reclaimer must destroy (`ai/handler/CreatureEventHandler.cpp:51-55`): ~117,000 per second at 500
   movers beside ~117 npcs (R-2).
3. **Database calls run inline on the four packet processor threads, against a five-connection pool.** A login or reconnect storm puts
   enter-world's DAO calls (`CM_ENTER_WORLD.cpp:15-17` → `services/player/PlayerEnterWorldService.cpp:237`, and the character load in
   `services/player/PlayerService.cpp:180-273` with 31 lines of DAO calls) on the same 4 threads that serve every other client's `CM_MOVE` and
   `CM_ATTACK` (`NetworkConfig.cpp:19-20`; pool size 5 and 5,000 ms timeout at `commons/src/aion/commons/configs/DatabaseConfig.cpp:11-12`).
   And a task that waits on the database with a published epoch **pins the Reclaimer** for the whole wait (runtime-architecture.md §2.6;
   `BlockingRegion` "does not unpublish", §1.2) - Java's collector does not care, refcounting does.
4. **Broadcast is O(N²) in casts, serializations and sends, and the send queue has no bound.** A broadcast walks the known list with an
   `as<Player>` per entry, and `Player` is not `final` either (`model/gameobjects/player/Player.h:79`), so every check is a `dynamic_cast`
   (`Ref.h:384-391`). Then each recipient gets its **own serialization** of the packet (`network/aion/AionConnection.cpp:128`), not one per
   broadcast as runtime-architecture.md §8.3 and `network/aion/AionServerPacket.h:21-22` describe. `SM_MOVE` goes to every sighted player for
   every `CM_MOVE` of type POSITION|MANUAL or IMMEDIATE (`network/aion/clientpackets/CM_MOVE.cpp:135-137`), onto per-connection queues that grow
   without limit (`AionConnection.cpp:144-153`, faithful to `commons` AConnection.java:106-109), drained by **one** IO thread (`GameServer.cpp:279`
   refuses `nio.threads > 1` unless `gameserver.network.nio.threads.unsafe.allow=true`, `NetworkConfig.cpp:18`). A client that stops reading
   makes the server's memory grow for as long as it stays connected.
5. **Abilities multiply all of the above.** Every running effect is a timer on the one scheduled-pool heap (`Effect.cpp:664, 836`,
   `skillengine/effect/AbstractOverTimeEffect.cpp:36-40`; heap and `SCHEDULER` leaf mutex at `runtime/sched/PoolBackends.h:19-21`); every
   damage-over-time tick runs `onAttack`, which fans out `CREATURE_NEEDS_SUPPORT` to every npc in the target's known list
   (`controllers/CreatureController.cpp:284-288`); every effect added rebroadcasts **every effect of its target slot**
   (`controllers/effect/EffectController.cpp:355-358`; `network/aion/serverpackets/SM_ABNORMAL_EFFECT.cpp:22-27`); and every effect pins two
   creatures until it ends (m5b2-plan.md §8 item 2).

**The proposal in one paragraph.** Ten scenarios (§4: CAP-1 .. CAP-9, with CAP-4 split into a grind site and an aggro site) plus room for the
user's; two kinds of measurement - what a fake client can observe (round trips, broadcast delay, cast and tick timing) and a once-per-second server
sampler (§5); a **knee** rule that reports the largest N before any soft bound breaks, plus a **hard correctness bar** that fails a run at any N
and **fails rather than passes when it has nothing to check** (§6); a `CapacityClient` that tracks what it sees and records nothing it does not
need (§7); **Release** for the knee numbers and the checked **RelWithDebInfo** build for the correctness pass, never Debug (§8). The tools are
**stage A: 4 lanes plus a one-time Release build and re-green (CP-00), 11.5-28 agent-days on §9.1's effort scale**; calendar time does not follow
from that scale (§9.2). The runs are **stage B: about 12-18 machine-hours (8-10 without the soak) in 5-6 sessions the user schedules**; stage C is
whatever the runs find. The proposed gate is **`gs.scenario.capacity_smoke`** (N = 4, about 90 s), which proves the measuring pipeline and is
never a capacity number (§9.3); whether it runs in the normal suite is the user's call (Q-11).

---

## 2. What exists today, and what it does not say

### 2.1 Measurements already on record

| Source | What was measured | Why it is not a capacity number |
|---|---|---|
| **P2 kernel stress** (runtime-kernel-status.md "stress"; §21 row "2.4 (scan)") | Synthetic objects on 64 workers: 65k tasks/s, 126k objects destroyed/s, Reclaimer lag p99 144 ms, backlog max 30,359 (checked RelWithDebInfo, 30 min; runtime-kernel-status.md:22); ASan Debug 23.7k tasks/s. The **first** 30-minute ASan run collapsed (lag p99 73 s, 7.0M backlog, 15.9 GB) because one publisher was pinned by a 10 s blocking wait; fixed with bounded scans | No game code at all. It does show the **shape** of the Reclaimer's failure mode under a pinned epoch (§3 R-6), and its destroy rate is a yardstick for R-2's `QuestEnv` load |
| **P4 movement/known-list benchmark** (runtime-kernel-status.md "bench") | 80k npc shells, 2,000 movers, 200 players on 5 connections, 200 ms period: tick p99 3.8 ms Release / 4.3-5.5 ms checked; per-phase µs (KnownList.update 177 µs Release, player flag scan 206 µs at ~5k npcs, `forEachPlayer` 12.3 µs against 5.2 µs with a fast instanceof); 147-171 B per known entry; `CONNECTION_QUEUE` 185-435 and `SCHEDULER` 23-48 blocking waits per tick | A **model**: AI, zones, geo and controllers reduced to counters, 5 shared connections, no database, no effects, and a broadcast that serialized **once** (609 serializations for 3,010 enqueues, runtime-kernel-status.md:203-204) where the port serializes per recipient (R-4). Its useful legacy is the per-operation costs this document reuses as priors |
| **M4 startup** (phase4-status.md:343-353, 395-402) | Startup path 5.8-7.7 s RelWithDebInfo vs 193.9-260.8 s Debug; peak working set 1,668-1,678 MB RelWithDebInfo vs 2,618-5,237 MB Debug | One process, no clients and **no npcs**: M4 predates `spawnAll` (m5a-plan.md:15), while the M5a server spawns 83,872 npcs (m5a-client-session.md:14). It is therefore **not** the memory baseline - CAP-1 at N = 0 is (§8.3). It remains the clearest evidence for §8.2 |
| **`gs.scenario.m5a_stress`** (commit `14ec8e005`; `game-server/tests/scenario/stress/StressRun.h:58-80`) | 30 m 14 s, 20 clients, 13,480 enter-world cycles at a sustained 7.5/s, 33,700 injected DAO failures, every report empty | A churn run. Its rate is set by the harness's own pauses (`moveInterval` 300 ms, `cyclePause` 250 ms, `reentryPause` 1,300 ms at StressRun.h:71-78), peak concurrency is at most 20, and the clients **never fight or cast**: `StressRun.cpp` sends only the login, enter-world, `CM_MOVE` and `CM_QUIT` packets (e.g. :243-299, :343-346). The 30-minute evidence also **predates the lane's own drift fix**: the commit message says the shipped harness "has yet to run a full half hour". m5b-plan.md G-07 (fighting) and m5b2-plan.md G-06 (casting) are the items that extend it; both are open |
| **Real-client sessions** (m5a-client-session.md "Setup", m5b-client-session.md) | One player: idle CPU 1 % of one core, 3.2 GB resident, **in a Debug build**; startup 144-149 s | One client, Debug. Not comparable to anything in §6 |
| **Gate durations** (commit `23c4e6485`; m5b-plan.md §8 item 15) | `gs.scenario.m5a` ~32 s, `m5a_geo` 18-21 s, `m5b` 199 s, `m5b_geo` 215 s | Functional runs; their wall clock is dominated by scripted waits (respawn timers, reentry) |

### 2.2 Instrumentation inventory

| Instrument | Where | What it gives | Gap for capacity |
|---|---|---|---|
| Reclaimer statistics | `runtime/lifetime/Reclaimer.h:115-150, 201` | epoch, backlog objects and bytes, lag, scans, last scan duration, budget-exhausted scans, the oldest publishing task | Read by the startup probe of the check mode (`game-server/src/main.cpp:219-231`) and, during play, by the watchdog probe (`runtime/lifetime/Reclaimer.cpp:1067-1073`), which only logs a lag warning past 10 s or dumps past the backlog thresholds; nothing records a time series |
| Watchdog | runtime-architecture.md §4.3; §2.6 | STALL dump past 60 s, lag warning past 10 s (`Reclaimer.h:83, 103`), BACKLOG dump past 1,000,000 objects or 256 MB (`Reclaimer.h:101-102`), deadlock dumps | Thresholds are for disasters, not for a knee |
| Slow-task warning | `Future::invokeBody` (`runtime/sched/Future.cpp:189-213`) and the packet processor's `ExecuteWrapper` (`AionConnection.cpp:103`; `commons/.../utils/concurrent/ExecuteWrapper.cpp:26`), threshold `gameserver.thread.runtime` = 5,000 ms (`configs/main/ThreadConfig.cpp:10`) | a log line "... - execution time: N ms" per task or packet slower than 5 s | Far above any latency a player notices |
| Pool statistics | `ThreadPoolBackend::getStats` (`runtime/sched/PoolBackends.cpp:512-533`) | pool sizes, counters, queued counts as text lines | Text only; the scheduled "queue" is the timer heap (every pending timer), not a backlog - lateness is what matters and nothing records it |
| Packet processor | `PacketProcessorBase::getWaitingPacketCount` (`commons/src/aion/commons/network/PacketProcessor.h:60`, public) | packets waiting for execution | Only logged by the checker, which does not run at min = max threads (PacketProcessor.cpp:43-44; Java PacketProcessor.java:132-133 is the same) |
| FIFO task managers | `AbstractFIFOPeriodicTaskManager.h:107-110` | "Tasks for X are added faster than they can be executed" after 10 s of growth | Fires only while the number of distinct tasks per run grows (the counter resets otherwise, :107-108; `processedTasks` is a set, :44, bounded by the number of distinct movers), so a steady crowd whose run outlasts its period never triggers it; it does not measure run time against the period |
| Runnable statistics | `commons.runnablestats.enable` (`commons/src/aion/commons/configs/CommonsConfig.cpp:8`), dumped to `log/stats/MethodStats.log` at shutdown (`ShutdownHook.cpp:150`) | per class and method: count, total, min, max. Every pool task feeds it through `Future::invokeBody` (`Future.cpp:209-210`), keyed by where the task was scheduled (`describeTask`, `runtime/sched/detail/SchedRuntime.cpp:37-40`); with the switch on, `AionConnection::sendPacket` also records serialization time per packet class (`AionConnection.cpp:129-130`) | No percentiles, no time series. All periodic managers are scheduled from the same line (`taskmanager/AbstractPeriodicTaskManager.cpp:25`), so they merge into **one** row and cannot be told apart; the serialization rows say how long, not how many serializations one broadcast made |
| Live instance counters | `runtime/lifetime/LiveInstanceCounters.h` ("Checked builds only") | live and created counts per class | **Empty in Release**; written only at startup and after shutdown (`CheckOutput.h:21`) |
| Leak census | runtime-architecture.md §5.4 | objects removed from the world and still alive | Fed only by `World::removeObject`, so it covers `VisibleObject`s alone (m5a-plan.md Q8 note) - effects, skills, tasks, aggro entries and `QuestEnv`s are invisible to it; in Release only memory growth shows their leaks |
| Introspection reports | `runtime/services/Introspection.h` (`debugTasks`, `debugReclaimer`, `debugLocks`, ...) | the text behind `//debug` | **No admin command calls them**: `game-server/handlers/aion/gameserver/handlers/admincommands/` holds only `AdminCommandsPrelude.h` |
| Blocking-wait counting | `runtime/base/YieldPoint.h` (`PctHooks::beforeBlocking`), checked builds | P4 used these hooks to count blocking waits per lock class | Not installed by the server |
| Database pool counters | `ConnectionPool::getTotalConnections` .. `getThreadsAwaitingConnection` (`commons/src/aion/commons/database/ConnectionPool.h:227-245`), `DatabaseFactory::getPool` (DatabaseFactory.h:103-104) | total, active, idle, waiting | Nobody reads them |
| Database wait visibility | runtime-architecture.md §2.6 says "DAO calls run inside `BlockingRegion`" | - | **Not implemented**: no `BlockingRegion` exists under `dao/` or `commons/src/aion/commons/database/` (grep; the only game-server uses are `LoginServer.cpp:135` and `ChatServer.cpp:53`). A thread stuck in a query is invisible to the watchdog's blocking accounting |
| Process memory | `main.cpp:245-251` (`GetProcessMemoryInfo`) | peak working set at startup | Not sampled after startup |
| Thread names | `commons/src/aion/commons/utils/concurrent/ThreadName.cpp:31` (`SetThreadDescription`) | every pool thread is named | Makes per-pool CPU attributable from outside or inside - nothing does it yet |

---

## 3. Where the port is most likely to break under load

The order a unit of load travels: client socket → **IO thread** (one; read, decrypt, `readImpl`) → **packet processor** (4 threads, one packet
per connection at a time) → handler (**known list, broadcast, AI events, effects**) → **scheduled pool** (timers) and **instant pool** (both
`max(4, cores)` threads, i.e. 32 each on the user's machine, `utils/ThreadPoolManager.cpp:70-71`) → **Reclaimer** (one thread, the only place
objects are freed) and **database** (inline, 5 connections). Each risk below names the evidence and what would show it.

**R-1. Silent degradation.** (§1 item 1.) A fixed-rate task that falls behind by more than `gameserver.scheduler.coalesce_after` (10) periods
and 2 s is re-armed at `now + period` (`Future.cpp:220-233`), which skips the missed runs without a trace. For a 200 ms movement manager that is
2 s of lateness before anything is dropped, and then npcs jump. For a 3,000 ms damage-over-time tick the threshold is 30 s, longer than the
effects in scope (15-30 s), so ticks are late rather than lost - but nothing measures late either. *Shows as:* only through CP-02/CP-03/CP-04 (§9).

**R-2. `MovementNotifyTask` in a crowd.** (§1 item 2.) Per run: for each moved creature, `KnownList::stream()` copies the known list into a
vector (`world/knownlist/KnownList.cpp:345-347`), then `as<Npc>` per entry (`MovementNotifyTask.cpp:54-61`) and
`onCreatureEvent(CREATURE_MOVED)` per npc (`:72`). For a moving **player** each such event does two things
(`ai/handler/CreatureEventHandler.cpp:49-56`):

- `checkAggro`, which calls `GeoService::canSee` **only when the npc's tribe is aggressive to the player** (`CreatureEventHandler.cpp:105-107`,
  after `TribeRelationService::isAggressive`). At CAP-4a's kerub cluster no tribe aggros PC (§4.4), so players cause no `canSee` there at all;
  CAP-4b is the site that exercises it.
- a new reference-counted `QuestEnv` for `QuestEngine::onAtDistance` (`CreatureEventHandler.cpp:51-55`; `questEngine/model/QuestEnv.cpp:24-27`,
  a `makeRef` with two `Field<Ref>` members), which the Reclaimer must destroy.

All of it on one thread, every 500 ms. **Inferred** from the ~220 ns failed-cast cost: 500 movers in one spot ≈ 250,000 failed casts ≈ 55 ms per
run; 1,500 movers ≈ 0.5 s, i.e. the whole period. **Inferred** from the data: 500 moving players that each know ~117 npcs (CAP-4a's disc)
retire ~58,500 `QuestEnv`s per run, ~117,000 per second - the order of P2's whole checked stress run, 126k destroyed/s
(runtime-kernel-status.md:22) - which feeds R-6. Beyond saturation, aggro reactions lag; the FIFO warning of §2.2 does **not** fire for a steady
crowd, because it needs the number of distinct movers to grow. `ZoneUpdateService` is another serial FIFO manager at 500 ms
(`world/zone/ZoneUpdateService.cpp:9`), fed by every `CreatureController::onMove` (`controllers/CreatureController.cpp:185-203`), with a zone
check per moved creature instead of a known-list walk. *Shows as:* run time of the manager versus its period (CP-03), aggro latency at the client
in CAP-4b, Reclaimer backlog (S-1) and the `QuestEnv` live count (S-11).

**R-3. Known-list updates on the packet processor.** Each `CM_MOVE` calls `World::updatePosition(..., updateKnownList = true)` (`CM_MOVE.cpp:132`;
the default at `world/World.cpp:190-192`, the update at :258), and `KnownList::update` holds the list's monitor while it re-checks every known
object and scans the neighbour regions (`KnownList.cpp:68-72, 251-287`). For a **player** it first walks **every npc of the map instance** for
flags (`KnownList.cpp:267-271`; P4 measured 206 µs Release per call at ~5,000 npcs; Poeta has 1,029 spawn spots). The work per `CM_MOVE` is
O(known objects + npcs in the instance), on one of 4 threads. *Shows as:* round-trip time of a probe packet that queues behind the moves (§5.1 C-1).

**R-4. O(N²) broadcast - casts, serializations and sends - and the unbounded send queue.** (§1 item 4.)

- **Casts.** A broadcast walks the sender's known list with `as<Player>` per entry (`KnownList::forEachPlayer`, `KnownList.cpp:338-343`;
  `forEachNpc`, :331-336, does the same with `as<Npc>`). `Player` is not `final` (`Player.h:79`), and the fast type check of `Ref.h:384-391`
  applies only to final classes, so every check is a `dynamic_cast`. `broadcastToSightedPlayers` adds a `KnownList::sees` lookup in each
  recipient's own list (`utils/PacketSendUtility.cpp:155-157`). P4 measured `forEachPlayer` at 12.3 µs per call against 5.2 µs with a fast
  instanceof (runtime-kernel-status.md:201-202) and put `instanceof Player` at ~70 % of broadcast cost (runtime-architecture.md §21, last
  paragraph); the type tag P4 recommended is not in the code. This is not specific to `MovementNotifyTask`: every `SM_MOVE`, `SM_ATTACK_STATUS`
  and `SM_ABNORMAL_EFFECT` broadcast, and the support fan-out on every hit (`CreatureController.cpp:287`, `forEachNpc`), casts every entry.
- **Serializations.** `PacketSendUtility::sendPacket` calls `AionConnection::sendPacket` once per recipient (`PacketSendUtility.cpp:68-75,
  83-125`), and that serializes on **every** call (`AionConnection.cpp:128`); `AionServerPacket::serialize` has no cache
  (`network/aion/AionServerPacket.cpp:70-90`) and its only callers are `AionConnection.cpp:128, 159, 172`. runtime-architecture.md §8.3 ("A
  broadcast serializes a SHARED packet once") and `AionServerPacket.h:21-22` describe a design the code does not implement. Each serialization
  also takes a global atomic sequence number (`AionServerPacket.cpp:39, 72`) and copies the bytes into a new heap vector (:88); `SM_PLAYER_INFO`
  is PER_RECIPIENT anyway (runtime-architecture.md §8.3). P4's model serialized 609 times for 3,010 enqueues, so its costs understate this.
- **Sends.** Enqueue is a sorted insert under the connection's `guard` mutex (`AionConnection.cpp:144-153`; in P4's model 29-32 % of inserts
  arrived out of order and scanned), and the IO thread batches up to 32 KB per write (buffer size at `AionConnection.cpp:112`;
  `commons/src/aion/commons/network/AConnection.cpp:219-258`). None of this caps the queue.

With visibility at 95 m (VisibleObject.java:247-249) and no cap on known players, N players in one spot moving with a broadcasting move type
cost **N × (N − 1) casts, serializations, heap allocations and enqueues per move round**, all on the 4 packet-processor threads; and a player's
known list holds N − 1 players: at 147-171 B per known entry (P4) that is ~43 MB of known-list entries at N = 500 and ~171 MB at N = 1,000
before any packet. *Shows as:* serializations per second (S-15), send-queue depth per connection (S-6), IO-thread and packet-processor CPU (S-9),
and the N² term of private bytes versus N (K-8).

**R-5. Effects and the timer heap.** (§1 item 5.) One heap behind one `SCHEDULER` leaf mutex holds every pending timer of the server
(`PoolBackends.h:19-21`); each over-time template schedules a fixed-rate task with an initial delay of `300 + checktime`
(`AbstractOverTimeEffect.cpp:36-40`), each effect with a duration an end task (`Effect.cpp:664-667`), each skill with periodic actions one more
(`Effect.cpp:836-839`). Each `SpellAttackEffect` tick calls `onAttack` (`skillengine/effect/SpellAttackEffect.cpp:51-60`), which adds damage and
hate, fans out `CREATURE_NEEDS_SUPPORT` over the target's known npcs and reduces HP with an `SM_ATTACK_STATUS` broadcast
(`CreatureController.cpp:284-288`). Each `addEffect` that is not passive ends in `broadCastEffects`, which sends `SM_ABNORMAL_EFFECT` with
**every abnormal effect in the added effect's target slot** to every sighted player (`EffectController.cpp:76-137, 355-358`; the slot filter at
`SM_ABNORMAL_EFFECT.cpp:22-27`; all slots only for FULLSLOTS) - so K debuffs on one monster watched by N players cost O(N × K) bytes per change
and O(N × K²) over the life of K overlapping debuffs. *Shows as:* tick-interval jitter and tick counts at the client (§5.1 C-5), scheduled-pool
lateness (CP-02).

**R-6. Database waits pin the Reclaimer.** (§1 item 3.) A task publishes its epoch at its first pointer load and holds it to its end
(runtime-architecture.md §2.4, §2.6); a DAO call made after that - enter world loads the account and character after touching game objects
(`PlayerEnterWorldService.cpp:237-314`) - keeps the epoch published for the query's duration. While it is published, nothing retired after it
can be destroyed, whatever the rest of the server does. With players moving (R-2's `QuestEnv`s alone are ~117k/s at 500 movers), fighting, dying,
respawning and casting elsewhere, the destruction rate is not small. The character load has **31 lines with DAO calls** in
`PlayerService::getPlayer` (`PlayerService.cpp:180-273`), several inside loops; `PlayerEnterWorldService.cpp` adds 9 of its 15 DAO call lines to
entering the world (the other 6, :771-788, belong to `GeneralUpdateTask` and `ItemUpdateTask`); `getPlayer` is called from the enter-world path
itself (`PlayerEnterWorldService.cpp:314`). Under 500 ms of injected latency per statement one enter world can therefore hold the epoch for
20 seconds or more (40 call sites at 500 ms, more where they sit in loops), and the backlog grows for that long - at ~117k retirements per second,
past the watchdog's 1,000,000-object BACKLOG dump (`Reclaimer.h:101`) in under 10 s (inferred). P2's collapse (§2.1) is the precedent for the failure shape; the
bounded scans fixed the spiral, not the growth. *Shows as:* Reclaimer backlog and lag against injected latency (CAP-7).

**R-7. Login, logout and reconnect storms.** Enter world runs on a packet processor thread (above); an abrupt disconnect schedules the logout
10 s later on the scheduled pool (`AionConnection.cpp:269-294` → `services/player/PlayerLeaveWorldService.cpp:77-79`), and a character still
being saved refuses re-entry (`PlayerEnterWorldService.cpp:253`). A reconnect storm therefore lines up N delayed logouts, each with its DAO
stores, against N enter-worlds, all sharing 5 connections with a 5 s borrow timeout. The login server link runs its packets in order on one
serial executor (runtime-architecture.md §1.4, §11). And the login server itself refuses players past `gameserver.network.login.max_players`,
**100 by default** (`NetworkConfig.cpp:16`, sent to the LS at `network/loginserver/serverpackets/SM_GS_AUTH.cpp:24`): a run that does not
raise it measures a configuration key. *Shows as:* time to enter world, pool `waiting`, "request timed out" exceptions.

**R-8. Monsters shared by many attackers.** The npc AI is not ticked: it is driven by events and by one-shot timers on the same scheduled
heap - the next attack (`ai/manager/AttackManager.cpp:104`, `ai/manager/SimpleAttackManager.cpp:81, 95`), the 500 ms aggro notifier
(`ai/handler/AggroEventHandler.cpp:84, 96, 111`), thinking (`ai/handler/ThinkEventHandler.cpp:110`), walking
(`ai/manager/WalkManager.cpp:200, 209`) - so every fighting npc adds at least one timer per attack interval. `DelayedOnAttack` pins attacker
and target until it runs (m5b-plan.md §8 item 2), and from M5b-2 on it can carry an `Effect` that pins two more (m5b2-plan.md §8 item 3); the
aggro list grows by one `AggroInfo` per attacker; every hit fans out support events (`CreatureController.cpp:287`); kills schedule respawns and
decays and retire npcs through the Reclaimer and the id quarantine of 300 s (runtime-architecture.md §6). **A site caps this load**: the
respawn times of CAP-4a's cluster allow at most ~5.9 kills per second whatever N is (§4.4), so R-8 grows with the number of sites, not with the
number of clients at one site. *Shows as:* attack response time, respawn punctuality, `AggroInfo`/`AttackResult`/`Effect` live counts (checked
builds), backlog.

**R-9. What the census has been quietest about.** It sees only `VisibleObject`s (§2.2), and the zombie breaker only objects removed from the
world for 30 minutes. A leak of effects, skills, tasks, aggro entries or `QuestEnv`s in a Release run is visible only as a memory slope - so the
soak (CAP-9) is the only leak detector the Release build has, and it must be read as one.

---

## 4. Proposed scenarios

Every scenario is a **proposal**; the site, the npcs and every expected count come from the Java data through a `tools/oracle` command (CP-11),
never from the port (lesson 3). Numbers below are sizing, measured with a throw-away script over `spawns/Npcs/210010000_Poeta.xml`,
`npcs/npc_templates.xml` and `tribe/tribe_relations.xml`, and **must be re-derived by the oracle** before any assertion uses them. Whether an npc
starts a fight is decided by **tribe**, not by its `ai` name (`TribeRelationService.java:16-64`; `TribeRelationsData.java:42-49`; m5b-plan.md A5a
and the note at :511-521): of Poeta's 72 `ai="aggressive"` ids only 27 have a tribe that is aggressive to PC.

| # | Scenario | Load | Breaks first, if the analysis is right | Buildable after M5b-2 |
|---|---|---|---|---|
| **CAP-1** | Crowd at rest | N characters enter the world at one spot and stand, one probe packet every 5 s | enter-world cost as N grows; memory per player | yes |
| **CAP-2** | Crowd in motion | N characters random-walk inside a disc at the real client's move cadence | R-2, R-3, R-4 | yes |
| **CAP-3** | Region churn and mass return | N characters walk a loop across 128 m region borders; then all cast *Return* together | region activation, known-list forget/find, despawn/spawn broadcast bursts | yes (same-map return only) |
| **CAP-4a** | Grind sites | N characters, spread over as many clusters as the oracle's kill capacity needs, fight monsters, killing, dying and reviving | R-8, then R-2/R-3/R-4 per cluster | yes |
| **CAP-4b** | Aggro site | characters step into the range of npcs whose tribe is aggressive to PC | R-2 with player-driven `canSee`, aggro latency | yes |
| **CAP-5** | Caster pack | M casters keep damage- and heal-over-time effects on many targets | R-5 | yes, with seeded skill rows |
| **CAP-6** | Login, logout and reconnect storms | N characters enter within a window W while M others play; then all drop and reconnect | R-7, R-6 | yes |
| **CAP-7** | Database latency | CAP-2/4a/6 with 10-500 ms added to every statement, and a 30 s stall | R-6, R-7 | yes (harness proxy, TLS off) |
| **CAP-8** | Slow readers | k clients of a crowd stop reading their socket | R-4 (memory) | yes |
| **CAP-9** | Soak | the chosen mix at ~70 % of the knee for 2-4 hours | R-9, drift | yes |

### 4.1 CAP-1 - crowd at rest (the baseline)

- **Site.** The Elyos spawn point `(1212.94, 1044.85)` on Poeta (m5a-plan.md §5.6). Measured: **27** npc spots within 95 m of it, 5 of them on
  walker routes and 1 random-walk, so 21 are deterministic (the expected known-npc count is time-dependent and the oracle's `m5a-spawns` must
  give it per position). Poeta's two `temporary_spawn` blocks, whose npcs depend on the game hour (`210010000_Poeta.xml:1086, 1091`), stand
  ~890 m away and do not touch this site. **None of the 27 is aggressive to PC** (by tribe), so CAP-1 has no aggro.
- **Load.** Ramp N = 0, 1, 10, 25, 50, 100, 200, 400, ... entering at a controlled rate (default 5/s, so CAP-1 does not become CAP-6), each step
  held 3 minutes. Each client sends `CM_PING_REQUEST` every 5 s (§5.1) and nothing else. **N = 0 is the server's memory baseline** with its
  83,872 npcs spawned (§8.3).
- **Measures.** Enter-world latency (`CM_ENTER_WORLD` → `SM_PLAYER_SPAWN`, and to level-ready) per N; private bytes and heap-in-use versus N
  (fitted as K-8 says: the linear term is **memory per player at rest**, the quadratic one the N − 1 known-list entries each); idle CPU per pool;
  probe round trip.
- **Why first.** Every other scenario is read against it; and it is the cheapest check that the harness itself scales (§6.1 V-1).

### 4.2 CAP-2 - crowd in motion

- **Site.** CAP-1's, or a flat disc of radius r (proposal: 20 m) that the oracle picks so that **no npc spot lies within r + 95 m of its centre**
  (+10 m for walkers): a player on the rim knows every npc 95 m beyond it, so an npc-free disc alone would still feed R-2. That variant measures
  R-3 and R-4 without R-2's npc half and may lie far from the spawn point; a second variant puts the disc inside CAP-4a's cluster to get both.
- **Load.** Every client random-walks inside the disc with the **real client's** `CM_MOVE` cadence, step and movement type (CP-13 measures them;
  until then the stress harness's 2 m every 300 ms, StressRun.h:71, is a placeholder, not a fact). Only moves of type POSITION|MANUAL or
  IMMEDIATE broadcast `SM_MOVE` (`CM_MOVE.cpp:135-137`), so the type matters as much as the cadence.
- **Stresses.** `CM_MOVE.cpp:98-140` → `World::updatePosition` → `KnownList::update` (R-3); the `SM_MOVE` broadcast with its per-recipient casts
  and serializations (R-4); the movement notify fan-out and its `QuestEnv`s (R-2) in the npc variant; `PlayerMoveTaskManager` (200 ms,
  `taskmanager/tasks/PlayerMoveTaskManager.cpp:8, 21-28`).
- **Measures.** Probe round trip; **broadcast delay** (client A's `CM_MOVE` send to client B's `SM_MOVE` receipt, both clocks in the harness);
  send-queue depth distribution; serializations per second; IO-thread and packet-processor CPU; `MovementNotifyTask` run time; Reclaimer backlog.
- **What it is for.** It is the scenario most likely to show the classic MMO knee, and the one whose answer decides whether R-2/R-3 or R-4 goes
  first.

### 4.3 CAP-3 - region churn and mass return

- **Load A (walk).** N characters walk a loop that crosses region borders (`gameserver.world.region.size` = 128, `configs/main/WorldConfig.cpp:8`;
  the oracle's `m5a-border-target` already computes a region-move target). Region activation runs on the instant pool
  (`world/MapRegion.cpp:133-146`), deactivation is scheduled (`:148-170`), and npc activation runs `updateKnownlist`
  (`ai/handler/ActivateEventHandler.cpp:14, 22`).
- **Load B (mass return).** All N cast skill **243 *Return*** together: 6,000 ms cast, `cooldown="12000"` = 1,200 s since Java multiplies by 100
  (`skills/skill_templates.xml:3046`; Skill.java:326). The path is `ReturnEffect::applyEffect` → `TeleportService::moveToBindLocation`
  (`services/teleport/TeleportService.cpp:320-341`) → `teleportTo` → `sendLoc` (`:170-189`: despawn with animation `NONE`, then the spawn task
  runs at once) → `spawnOnSameMap` (`:205-224`: respawn, known lists rebuilt for everybody in range). One cast per character per 20 minutes, so
  the harness uses fresh characters per wave or clears the cooldown row.
- **Reachability (lesson 2).** Traced one call deep: the same-map path reaches no `AION_UNPORTED` body (`DuelService::isDueling`,
  `RecallService::cancel`, `World::despawn`, `ConquerorAndProtectorService::onLeaveMap`, `PlayerEffectController::updatePlayerEffectIcons`,
  `PlayerController::startProtectionActiveTask` are ported). **A bind point on another map does**: `SpawnTask::run` then calls
  `InstanceService::onLeaveInstance` (`TeleportService.cpp:116-122`), which is `AION_UNPORTED` (`services/instance/InstanceService.cpp:170`).
  A fresh character has no bind point and returns to its race's spawn location on the same map (`TeleportService.cpp:325-340`); the scenario
  must keep it that way until M5f.
- **Measures.** Burst size and duration of `SM_DELETE`/`SM_PLAYER_INFO`/`SM_NPC_INFO` at the clients; probe round trip during the burst;
  Reclaimer backlog after it (every region move and despawn retires known-list nodes).

### 4.4 CAP-4 - grind sites (4a) and an aggro site (4b)

The first draft had one grind spot and read its npcs as aggressive. **Recounted by tribe, none of them is**, so the grind and the aggro halves
need different sites.

**CAP-4a - grind sites.**

- **The densest cluster.** The densest 95 m disc on Poeta holds **117** spots, centred on `(1004.9, 1125.47)` (a `210664` "digging kerub"
  spot, 223 m from the Elyos spawn point): `210341` ×27, `210134` ×18, `210664` ×13, `210336` ×12, `210079` ×11, `210663` ×8, `210665` ×6,
  `210705` ×5, `700105` ×4, `210133` ×4, `210135` ×3, `210667` ×2 and four single friendly npcs. By tribe: MONSTER 84, D1_HKERUBIM_LF1 14,
  KERUBIM_AD1_LF1 6, KERUBIM_AFARMER_LF1 5, FIELD_OBJECT_LIGHT 4 (the `700105` "kerub grain sacks", ai `quest_use_item`, which has no registered
  handler and runs a `DummyNpcAI`, m5b-plan.md §2.4, D15), FARMER_HKERUBIM_LF1 3 and GENERAL 1. **No tribe here is aggressive to PC**: the
  MONSTER row has no `<aggro>` element (`tribe_relations.xml:2170-2173`), KERUBIM_AD1_LF1 aggros D1_HKERUBIM_LF1 (:1585-1587),
  KERUBIM_AFARMER_LF1 aggros FARMER_HKERUBIM_LF1 (:1588-1591), and `TribeRelationService.java:16-64` has no MONSTER-to-PC arm. Any aggro inside
  the cluster is **npc against npc**. The 109 attackable spots (the MONSTER-based tribes) have 2-282 HP: 199 for `210341`, `210134`, `210664`,
  `210079`, `210663`, `210665`, `210705`; 143 for `210133`; 282 for `210135`; 86 for `210667`; 2 for `210336`.
- **The cluster caps the load.** The 109 attackable spots respawn after 15 s (49 spots), 17 s (11), 20 s (38), 70 s (6) and 150 s (5)
  (`respawn_time`; the timer starts at death), which allows **at most ~5.9 kills per second** whatever N is, and fewer once the time to kill is
  counted. Beyond roughly 100 clients the extra characters stand idle, and the knee of a one-cluster run would be a crowd knee (R-2/R-3/R-4), not
  R-8. **Proposal:** CP-11 computes each cluster's kill capacity (Σ 1/`respawn_time` over its attackable spots) and the harness spreads N over as
  many clusters as that needs, a proposed ≤ 20 clients per kill per second; one variant keeps everybody in the densest cluster on purpose, to read
  the crowd and the fight together.
- **Load.** Characters target the nearest attackable npc (attackable by tribe, from the oracle), attack at their attack speed
  (`GameSession::fightUntil`'s pacing, `game-server/tests/scenario/GameSession.h:183`), and move on. Npcs die in seconds, so the load is
  kill → decay → respawn churn as much as combat. **A death is a teleport:** `CM_REVIVE` → `PlayerReviveService::bindRevive` →
  `TeleportService::moveToBindLocation` (`services/player/PlayerReviveService.cpp:58-91`) puts the character back at the Elyos spawn point, 223 m
  from the densest cluster; the walk back crosses region borders like CAP-3's load A, and the client models it (§7 item 4) and reports it
  separately. Levelling during a long run is safe as long as nobody changes class: the starting classes autolearn nothing above level 9, all of
  it inside M5b-2's 38 effect classes (measured over `skill_tree/skill_tree.xml`).
- **Stresses.** R-8, npc skills (`210133`/`210134` own 16419, m5b2-plan.md §2.4(b)), the npc chase through `MoveTaskManager` on the ForkJoin
  pool (`taskmanager/tasks/MoveTaskManager.cpp:35-54`), npc-against-npc aggro, and R-2 when the crowd is inside one cluster.
- **Measures.** Attack response (`CM_ATTACK` → `SM_ATTACK` naming the attacker); respawn punctuality against the spawn's `respawn_time`;
  kills per second against the oracle's ceiling; checked-build live counts of `AggroInfo`, `AttackResult`, `Effect`, `Npc`.

**CAP-4b - aggro site.**

- **Site.** Poeta has **231** spots of **27** ids whose tribe is aggressive to PC. The densest 95 m disc of them holds **52**, centred on
  `(552.38, 1810.59)` (a `210682` "dukaki lumberjack" spot), **1,011 m** from the Elyos spawn point: `210677` ×23, `210682` ×8, `210143` ×8,
  `210683` ×5, `210701` ×3, `210681` ×2, `210142` ×2, `210678` ×1; tribes BROWNIE 27, BROWNIEFELLER_HZAIF_LF1 10, ZAIF 10,
  ZAIF_ABROWNIEFELLER_LF1 5; levels 6-7, 478-769 HP, `srange` 3-7, respawn 125-130 s; 7 of the 52 are random-walk spots. A level-1 character
  passes `validateAggro` (level difference < 10, `CreatureEventHandler.cpp:127-130`), so these npcs do attack it.
- **Load.** Characters walk into an aggressive npc's range, one at a time per npc, and record the first `SM_ATTACK` from it (C-8). Level-6-7
  npcs are likely to kill level-1 characters (inferred, the fight was not simulated), and a bind revive teleports them 1,011 m away. **Proposal:** instead of the walk back, the harness logs the
  character out and back in with its saved position re-seeded at the site's edge (the `players` row carries `x`, `y`, `z`, `heading` and
  `world_id`, Java tree `game-server/sql/aion_gs.sql`; the same seeding idea as m5b-plan.md D12), so the walk back does not set the sample rate;
  the walk back remains a CAP-3 load if wanted.
- **Stresses.** R-2 with player-driven `GeoService::canSee` (`CreatureEventHandler.cpp:107`) - the only scenario where it runs - and the aggro
  notifier timers of R-8.
- **Measures.** Aggro latency (a client stops inside the range → the npc's first `SM_ATTACK`); `MovementNotifyTask` run time; with geo on, the
  share of `canSee` (S-14 if the user opts in).

### 4.5 CAP-5 - caster pack

- **Skills.** Measured over `skill_templates.xml`: with M5b-2's **38** effect classes (section 2.4's 34 plus `StumbleEffect`, `StaggerEffect`,
  `MPHealEffect` and `ProcVPHealInstantEffect`, m5b2-plan.md D13), **739** templates use only classes of the set and include a periodic one
  (`SpellAttackEffect`, `BleedEffect`, `HealEffect` or `MPHealEffect`, which extends `HealOverTimeEffect`, MPHealEffect.java:15); **196** of them
  are learnable (in `skill_tree.xml`). Counting only the three original periodic classes gives 684 and 184. Useful ones:
  **1447 *Erosion*** (MAGE, level 5; `spellatkinstant value="81"` + `spellatk checktime="3000" value="81" duration2="15000"`, 38 MP, cooldown 3 s,
  `first_target_range="25"`; `skill_templates.xml:21339-21352`), **3742 *Sandblaster*** (SPIRIT_MASTER 25; `target_type="AREA"
  target_maxcount="6"`, 30 s damage over time, **146 MP**, 1,500 ms cast; `:63693`), **3939 *Light of Rejuvenation*** (CLERIC 10;
  `heal checktime="2000" value="30" duration2="30000"`, `first_target="TARGETORME"`, 24 MP, cooldown 5 s; `:67095-67107`). The `AREA` arm is
  ported (`skillengine/properties/TargetRangeProperty.cpp:53`, `MaxCountProperty.cpp:28`, 0 unported in the directory). None is autolearned at
  level 1, so the harness **seeds `player_skills` rows** on level-1 characters of the starting classes, as m5b2-plan.md D3 and C9 do
  (`SkillEngine.getSkillFor` gates only on `isSkillPresent`).
- **Effects end by time only if their target survives.** An *Erosion* runs its 4 ticks (at 3.3, 6.3, 9.3 and 12.3 s) only if the target
  survives the instant hit and all four ticks, about 405 damage at the template values (the damage formula was not run). No attackable target
  within reach of a level-1 character does: the two within *Erosion*'s 25 m of CAP-1's site are `210115` (143 HP) at 21.2 m and 25.2 m, the
  CAP-4a cluster's monsters have 2-282 HP, and death ends every effect (`CreatureController.cpp:209`, `removeAllEffects`). The only Poeta
  candidates with more HP are CAP-4b's level-6-7 npcs (478-769 HP), which aggro and kill level-1 characters. Therefore: **(a)** damage-over-time
  ticks are asserted **up to the target's death** against the oracle's schedule truncated there (H-3); **(b)** **heal-over-time on characters
  supplies the effects that end by time**: nobody attacks a character in CAP-5, and a heal tick that carries a skill id is broadcast even at full
  HP (`model/stats/container/CreatureLifeStats.cpp:172-173`; Java CreatureLifeStats.java:190-191; the tick itself at
  `skillengine/effect/HealOverTimeEffect.cpp:73`).
- **The honest limit.** A level-1 Mage has 405 MP (m5b2-plan.md §2.4): ten *Erosion*s or two *Sandblaster*s, then regeneration sets the rate.
  The scenario reports the cast rate it **achieved** (from `SM_CASTSPELL_RESULT` counts). Seeding **levels or classes** instead takes characters
  outside the range M5b-2 was built for (question Q-6): 3742 belongs to SPIRIT_MASTER at 25 and 3939 to CLERIC at 10 (`skill_tree.xml:3857,
  4038`); a SPIRIT_MASTER levelling 11-30 autolearns 89 skills, 51 of them with classes outside the 38, including the passive
  `SubTypeBoostResistEffect` (skill 133); a CLERIC 11-30 autolearns 80, 23 outside, including the passives `BoostHateEffect` (364) and
  `BoostHealEffect` (367); even the level-10 class change to CLERIC autolearns the passives 363 and 366 (`BoostHealEffect`, `skill_tree.xml:609,
  615`). Learned passives are applied at once (`services/SkillLearnService.cpp:59-60`), so those throw by design (m5b2-plan.md D6).
- **Stresses.** R-5 in full: timer-heap size and lateness, `SM_ABNORMAL_EFFECT` payload growth, support fan-out per tick, effect pinning.
- **Measures.** Per effect at the client: **tick count** against the oracle's schedule (from `duration2`, `checktime` and the 300 ms offset of
  `AbstractOverTimeEffect.cpp:36`, truncated at death) and **tick interval jitter** against `checktime`; cast-bar accuracy (`SM_CASTSPELL` →
  `SM_CASTSPELL_RESULT` against the template duration, the ±10 % of m5b2-plan.md X4); `SM_ABNORMAL_EFFECT` bytes per second per client;
  scheduled-pool lateness.

### 4.6 CAP-6 - login, logout and reconnect storms

- **Load A.** M characters play CAP-2's walk while N more log in within W seconds (W = 60, 10, 1).
- **Load B.** All connected clients drop their sockets at once (no `CM_QUIT`) and reconnect after 2 s - the "network blip" a real server sees.
  Java's own behaviour holds re-entry until the delayed logout saved the character (`AionConnection.cpp:285-288`, `PlayerEnterWorldService.cpp:253`).
- **Profile.** `gameserver.network.login.max_players` raised above N (R-7), `gameserver.character.reentry.time` as the user plays with (the
  scenario profile uses 1, `game-server/tests/scenario/ScenarioServers.cpp:34`).
- **Measures.** Time to enter world (p50/p99 per wave); **in-world probe round trip during the storm** (the packet processor is shared);
  pool `waiting` and borrow timeouts; login server response time; how long until every client is back in the world.

### 4.7 CAP-7 - database latency

- **Mechanism.** A harness-owned TCP proxy between the game server and MariaDB (CP-10) adds a fixed or random delay per round trip and can stall
  for 30 s, and counts statements, bytes and in-flight queries. **It can see statements only in plain text.** The connection's `sslMode`
  defaults to PREFERRED (`commons/src/aion/commons/database/ConnectionProperties.cpp:170`), which uses TLS whenever the server offers it
  (`commons/src/aion/commons/database/Connection.cpp:223-229`), and neither the Java config (`game-server/config/network/database.properties:8`
  in the Java tree) nor the
  scenario harness (`ScenarioServers.cpp:126, 138`) sets it; that the MariaDB 11.8 install in `D:\aion-dev` offers TLS is **inferred** (11.4
  and later generate a certificate on their own, and its `my.ini` has no `ssl` setting). **Proposal:** the capacity profile adds
  `sslMode=DISABLED` to `database.url` and the TSV header records it; if the user prefers TLS on, the proxy only delays and S-13 comes from DAO
  timing inside the server instead. The existing trigger injection (`StressRun.h:14-26`) reaches only `UPDATE`s; a proxy reaches reads too.
- **Load.** CAP-2, CAP-4a and CAP-6 at a fixed N, latency 0 / 10 / 50 / 200 / 500 ms, then one 30 s stall.
- **Measures.** Reclaimer backlog and lag against latency (R-6); probe round trip (the packet processor is blocked by the waits); recovery time
  after the stall.
- **Expected log lines of the 30 s stall** (the per-scenario H-1 allow-list for the ERROR lines; the WARN lines are listed too, so the report
  can tell an expected warning from a new one; the watchdog will **not** stay silent):
  - `Reclamation lag N ms: thread ... holds epoch ...` (WARN, `Reclaimer.cpp:1073`), once per oldest epoch, as soon as a task blocked in a DAO
    call keeps its epoch published for more than 10 s (`Reclaimer.h:83, 103`) - R-6 predicts exactly that;
  - `... - execution time: N ms` (WARN, `ExecuteWrapper.cpp:26` for packets, `AionConnection.cpp:103`; the same text from `Future.cpp:212-213`
    for pool tasks such as the delayed logout) for everything slower than 5,000 ms (`ThreadConfig.cpp:10`);
  - `DatabasePool - Connection is not available, request timed out after 5000ms (total=5, active=5, idle=0, waiting=N)`, an
    `SQLTransientConnectionException` (`commons/src/aion/commons/database/ConnectionPool.h:30-31`, message built at :347; timeout from
    `DatabaseConfig.cpp:12`), which reaches the log through each DAO's own catch block - the allow-list names those messages exactly, as the
    stress run does (`StressRun.h:192-195`);
  - possibly the watchdog's BACKLOG dump (`Reclaimer.h:101-102`), if the retirements of R-2 and R-8 pass 1,000,000 objects or 256 MB during the
    pin;
  - **no** STALL dump, since 30 s is below its 60 s threshold; the watchdog cannot attribute any of it to the database, because no
    `BlockingRegion` wraps DAO calls.

### 4.8 CAP-8 - slow readers

- **Load.** In a CAP-2 crowd of 100-200, k clients (1, 5, 20) keep sending moves and probes but stop reading their socket.
- **Measures.** Server private bytes over time; the stalled connections' queue depth; whether anything ever closes them. The alive checker looks
  only at the time of the last **client** packet (`AionConnection.cpp:83-92`), so a client that still sends is never closed.
- **Guard.** The game server child runs in a job object already (`game-server/tests/scenario/ChildProcess.h:91-95`); this scenario adds a
  process memory limit to it so the test fails instead of the user's desktop.
- **Why it matters.** The behaviour is **Java's** (commons AConnection.java:106-109 is just as unbounded). If it shows unbounded growth, bounding
  the queue is a deviation the user decides on, not a bug fix the integrator makes.

### 4.9 CAP-9 - soak

- **Load.** The mix the user wants to run for real (e.g. CAP-2 + CAP-4a + CAP-5 in proportions from Q-3) at ~70 % of the lowest knee, 2-4 hours,
  Release and then the checked build.
- **Measures.** Memory slope after warm-up; backlog at the end of each minute; the periodic census (leak census every minute, zombie breaker
  every 2, as the stress run sets them, `game-server/tests/scenario/stress/M5aStressTest.cpp:163-166`); id factory cursor; log volume (the M5a
  stress wrote 93 MB in 30 minutes - a four-hour soak must rotate or cap logs).

### 4.10 The user's scenarios

**This table is the user's.** The rows are empty on purpose; the columns are the ones every scenario above answers, so the two sets can be
compared and merged in one conversation.

| # | Scenario (the user's words) | Where (map, spot) | Who (classes, levels, how many) | What they do | What the user expects to see | What would count as broken | Priority |
|---|---|---|---|---|---|---|---|
| U-1 | | | | | | | |
| U-2 | | | | | | | |
| U-3 | | | | | | | |
| U-4 | | | | | | | |

### 4.11 Scenarios that need later milestones

| Scenario | Why not after M5b-2 | Milestone |
|---|---|---|
| Loot storms, inventory churn (item adds, moves, stack splits) - the first **in-play** database writes | `DropService`/`DropRegistrationService`, `ItemService::addItem` unported (m5b-plan.md O-05, O-06) | M5b-3 |
| Vendors, trade, mail, private stores | P5-09, P5-07 | M5c |
| Quest progress on every kill (4,184 XML quests) | P5-06 | M5d |
| Class change, and characters above level 9 of an advanced class | the class-change quest; advanced-class passives outside the 38 (§4.5) | M5d, M5e |
| Teleporters, flight paths, cross-map teleport, instance entry | `TeleportService` 21 unported bodies; `InstanceService::onLeaveInstance` unported (§4.3) | M5f |
| Groups and alliances (per-member packets to every member, shared loot, team damage lists) | P5-10 | M5g |
| Legions | P5-11 | M5h |
| **Siege and world raids** - the real-world worst case: hundreds of players, fortress npcs, siege weapons, Reshanta's 200-notify cap (`MovementNotifyTask.cpp:52`) | P5-12a, P5-12b | M5i |
| PvP crowds (abyss rewards, `PvpService::doReward`) | P5-08 (`PvpService`), abyss maps | M5i or later |
| Chat server traffic | P5-14 leftovers | M5j |

Every later milestone adds fan-out or database traffic to paths CAP-2/4a/6 already measure. **Proposal:** re-run CAP-2 and CAP-4a at the end of
M5b-3, M5d and M5g, and CAP-6/7 at the end of M5b-3 (the first milestone that writes to the database during play) - or at whatever cadence the
user prefers (Q-8).

---

## 5. What to measure and how

### 5.1 Client-observed (no server change needed)

| # | Metric | Definition | Why this one |
|---|---|---|---|
| **C-1** | Probe round trip | `CM_PING_REQUEST` (opcode 103, IN_GAME, `network/aion/ClientPacketInfo.gen.inc:99`) → `SM_PING_RESPONSE`; the client's `/ping` (`CM_PING_REQUEST.cpp`, Java CM_PING_REQUEST.java). Every client, every 5 s | It crosses the IO thread, the packet processor queue **behind the client's own packets**, serialization, the send queue and the IO write - the single best "is the server responsive" number. `CM_PING` cannot be used: it is expected every 180 s and a faster one is flagged as a timer cheat (`CM_PING.h:14`, `CM_PING.cpp:33-39`). No flood filter threshold is configured by default (`gameserver.network.pff.mode = 1` with the example rows commented out, Java `config/network/pff.properties:11-19`) |
| **C-2** | Broadcast delay | client A sends `CM_MOVE` at t0; client B receives the matching `SM_MOVE` at t1; both clocks in one harness process | R-4 end to end |
| **C-3** | Attack response | `CM_ATTACK` → the `SM_ATTACK` whose attacker is the client | R-8; also the input to K-1's jitter bound (§6.3) |
| **C-4** | Cast accuracy | `SM_CASTSPELL` → `SM_CASTSPELL_RESULT` minus the template cast time | scheduled-pool lateness seen by a player |
| **C-5** | Tick count and jitter | per periodic effect, its ticks: `SM_ATTACK_STATUS` for the effected creature carrying the effect's skill id and log id (Java SM_ATTACK_STATUS `writeImpl` writes both after the type; `SpellAttackEffect.cpp:57-58` passes `SPELLATK`, `HealOverTimeEffect.cpp:73` passes `HEAL`); count against the oracle's schedule **truncated at the effected's death** (`SM_EMOTION` DIE), interval against `checktime` | R-5, and R-1's coalescing if it ever reaches an effect. A heal tick with a skill id is sent even at full HP (§4.5), so heal-over-time effects always yield their full count |
| **C-6** | Enter-world time | `CM_ENTER_WORLD` → `SM_PLAYER_SPAWN`, and → the level-ready burst | R-7 |
| **C-7** | Respawn punctuality | npc death (`SM_EMOTION` DIE) → an `SM_NPC_INFO` of the same template at the same spot (a respawn is a new object with a new id), minus the spawn's `respawn_time` | scheduler lateness on a 15-150 s timer |
| **C-8** | Aggro latency | a client stops inside the range of an npc whose **tribe** is aggressive to PC (CAP-4b) → first `SM_ATTACK` from that npc | R-2 |
| **C-9** | Errors | unexpected close, decode errors, `SM_QUIT_RESPONSE` nobody asked for, `SM_SYSTEM_MESSAGE` refusals by id, `SM_ATTACK_RESPONSE` refusals (`STOP_OBSTACLE_IN_THE_WAY` means a wrong z with geo on, §7 item 4) | the hard bar (§6.2) |
| **C-10** | Harness self-lateness | how late each client's own send schedule ran | validity (§6.1 V-1): a late client is a harness bottleneck |

### 5.2 Server-internal

| # | Metric | Source today | To add |
|---|---|---|---|
| **S-1** | Reclaimer backlog, bytes, lag, scans, last scan duration, budget-exhausted scans | `Reclaimer::stats()` (`Reclaimer.h:201`); the watchdog probe reads it but keeps no series (`Reclaimer.cpp:1067-1073`) | sample it (CP-01) |
| **S-2** | Scheduled-pool and instant-pool **lateness** (start − due) | nothing | CP-02: a 100 ms fixed-rate canary on the scheduled pool and a 100 ms `execute` canary on the instant pool, each recording its own lateness; no kernel change |
| **S-3** | Coalesced periods | nothing | CP-04: a counter in `Future::finishRun` (`Future.cpp:230-232`) - additive, header request on P4-02b |
| **S-4** | Periodic manager run time versus period (`MoveTaskManager`, `PlayerMoveTaskManager`, `MovementNotifyTask`, `ZoneUpdateService`, `ExpireTimerTask`) | RunnableStats sees every manager, but merged into one row (§2.2) | CP-03: time `run()` in the `scheduleAtFixedRate` lambda of `AbstractPeriodicTaskManager.cpp:25` into a histogram **per manager** |
| **S-5** | Packets waiting for execution; packet processor thread count | `getWaitingPacketCount` (public) | sample it (CP-01) |
| **S-6** | Send-queue depth per connection (max and p99 over connections) | `getSendMsgQueue` needs `guard`, which is protected (`AionConnection.h:163-164`) | CP-05: a C++-only `sendQueueDepth()` accessor - header request on P4-15 |
| **S-7** | Database pool total/active/idle/waiting | `ConnectionPool.h:227-245` | sample it (CP-01) |
| **S-8** | Private bytes, working set, commit, handle count | `main.cpp:245-251` does it once | sample it (CP-01); in checked builds subtract the 64 MB delayed-free FIFO (runtime-architecture.md §12.5 C3) |
| **S-9** | CPU per pool (IO, packet processor, scheduled, instant, ForkJoin, Reclaimer, watchdog) | thread names exist (`ThreadName.cpp:31`) | CP-01: `GetThreadTimes` over the process's threads grouped by description |
| **S-10** | Blocking waits per lock class (checked builds) | `PctHooks::beforeBlocking` (`YieldPoint.h`), as P4 did | CP-06; note that `activeHooks` is documented as written by the PCT scheduler only, so using it for counting is a C++-only use to record |
| **S-11** | Live counts of chosen classes (checked builds) | `LiveInstanceCounters.h` | sample `liveCountOf` for `Player`, `Npc`, `Effect`, `AggroInfo`, `AttackResult`, `KnownObject`, `QuestEnv` (CP-01) |
| **S-12** | Online players, known-list sizes (mean and max) | `World::forEachPlayer` | CP-01 |
| **S-13** | Statements/s, database latency, in-flight queries | nothing in the server | CP-10's proxy measures it from outside, **with `sslMode=DISABLED`** (§4.7); with TLS on, DAO timing inside the server instead |
| **S-14** | CPU sampling profile and lock contention in Release | nothing | optional: Windows Performance Recorder (`wpr`, built into Windows, needs an elevated prompt) around one step at the knee - **only if the user opts in** (Q-10) |
| **S-15** | Serializations and serialized bytes per second, and enqueues per second | RunnableStats records serialization **time** per packet class, at shutdown (`AionConnection.cpp:129-130`) | CP-15: two relaxed counters in `AionServerPacket::serialize` (`AionServerPacket.cpp:70-90`, P4-15) and one in `AionConnection::enqueue` (:144-153); their ratio shows R-4's per-recipient serialization directly |

### 5.3 Sampling and output

- The server writes **one TSV row per second** to `<check-output>/capacity_metrics.tsv` when started with a new `--capacity-metrics` flag (CP-01,
  `main.cpp` is chunk P5-14; the check-output mode already exists, `main.cpp:13-40`). The first lines record the build configuration,
  `AION_CHECKED`, the git revision, geo on or off, the database `sslMode` and every non-default `-D` key, so a number can never be separated from
  the build and profile that produced it (risk 4).
- **The sampler thread.** It is a dedicated thread, not a pool task, so its own timing does not depend on the pools it measures. It must open a
  `TaskScope` around each sample, because pointer-loading operations assert `TaskScope::active()` in checked builds (runtime-architecture.md
  §2.4, "Unregistered threads"), and it must **close** the scope between samples: a scope held open would publish an epoch and pin the Reclaimer
  (R-6), which is exactly what S-1 measures.
- The harness writes its own per-second rows (C-1 .. C-10) and a per-step summary; the two files share the wall clock.
- Latencies are kept in log-linear histograms (1 µs .. 60 s, 3 % relative error) per metric per second; percentiles p50/p95/p99/p99.9/max come
  from merged histograms, never from averages. No third-party dependency is needed for this.
- A report script (CP-12) turns a run into curves (metric versus N) and a one-page verdict per scenario.

---

## 6. Pass/fail and knee criteria

Three kinds, kept apart on purpose. **Validity** decides whether a step counts at all; the **hard bar** fails a run at any N; the **knee** is a
number, not a pass or a fail. Every row says what it proves, what it cannot, and which defect it catches - the standard of m5b2-plan.md §10.3.
**All thresholds are proposals for the user to change** (Q-4).

### 6.1 Validity (a step that fails these is discarded, not reported)

| # | Rule | Proves | Cannot prove | Catches |
|---|---|---|---|---|
| **V-1** | Harness self-lateness (C-10) p99 < 10 % of the send interval, and harness CPU below its budget (§8.3) | the load was offered as designed | that the machine's other load was zero | a harness that became the bottleneck - the classic false knee when clients and server share 32 cores |
| **V-2** | Each step held ≥ 3 minutes after the last client entered; the first 60 s discarded | steady state | anything about transients (CAP-3/6 measure those separately) | ramp artefacts read as a knee |
| **V-3** | The step at the knee and the one below it repeated once; the knee is reported only if both repeats agree | reproducibility | variance beyond two samples | one-off OS preemption (P4 saw 15-20 ms stalls with no lock waits) |
| **V-4** | Known-list sizes at N = 1 hold at least the oracle's deterministic npc count for the site, and every npc known is a spot of its id within 95 m (+10 m for walkers) (S-12 and the client's own table; the m5a-plan.md V1/V3 pattern) | the site and the data are the ones the scenario claims | behaviour at higher N | a wrong site, a spawn file the server read differently from the oracle |

### 6.2 Hard bar (any N; a failure is a bug report, not a knee)

| # | Rule | Proves | Cannot prove | Catches |
|---|---|---|---|---|
| **H-1** | The M5a Q8 bar: `unported_trace.txt` empty, `partial_trace.txt` within the **capacity** partial allow-list (CP-09 writes it), no ERROR line in either log except the exact messages the scenario's own conditions are expected to cause (listed per scenario - §4.7 lists CAP-7's - as the stress run lists its injected DAO errors, `game-server/tests/scenario/stress/StressRun.h:192-195`), final `census.txt` empty after the drain, no watchdog DEADLOCK or STALL dump; in checked runs lockdep empty | load does not reach an unported body or break the lifetime rules | leaks of non-`VisibleObject`s in Release (R-9) | the dormant code of lesson 2 - a knee run is the first time many paths run concurrently |
| **H-2** | No disconnect the harness did not cause; no client decode error | the protocol stays intact under load | message ordering subtleties the decoders do not check | a send queue that reorders or truncates |
| **H-3** | For every periodic effect a client observes: its tick count equals the oracle's schedule truncated at **min(end by time, the effected's death as `SM_EMOTION` DIE)**, a scheduled tick within ± 100 ms of the truncation point accepted either way. **In the scenarios that cast (CAP-5, and CAP-9 when its mix casts), each step must also contain at least 20 (proposal) effects that ended by time** - heal-over-time on characters supplies them (§4.5) - **or the step fails H-3 instead of passing it** | effects run to their schedule under load, including the ones cut short by death | the damage and heal amounts (random, m5b-plan.md D6) | a lost periodic run (R-1) reaching effects; an end task firing before the last tick; a run with nothing to check (the first draft's H-3 was vacuous in CAP-4 and CAP-5, because no target outlived *Erosion*) |
| **H-4** | After the load stops, backlog returns to within 10 % of CAP-1's N = 0 baseline within 10 s, and the checked-build live counts of `Effect`, `AttackResult`, `Skill` and `QuestEnv` return to **the baseline taken after warm-up at the same N**. Not to 0: once M5b-2 closes `SkillEngine.cpp:137` (`m5b_partial_allowlist.txt`), online players carry permanent passive `Effect`s (learned passives apply at once, `SkillLearnService.cpp:59-60`), and npcs carry post-spawn effects (m5b2-plan.md §2.4(c)). **The strict 0 is asserted only after every client logged out and the runtime shut down**, where `live_counts.txt` is written (`CheckOutput.h:21`; m5b2-plan.md G-06, X13) | nothing is pinned by the load itself | slow leaks - that is CAP-9's job | a `DelayedOnAttack` or effect that never releases (m5b-plan.md §8 item 2, m5b2-plan.md §8 items 2-3) |

### 6.3 The knee (reported, not passed)

The knee of a scenario is **the largest N at which every row below holds**, confirmed per V-3; the report names the row that broke first. That
row is the first bottleneck and the input to stage C.

| # | Soft bound (proposal) | Rationale | Cannot prove |
|---|---|---|---|
| **K-1** | Probe round trip (C-1) p99 ≤ 150 ms and max ≤ 1 s | Java rejects an attack that the server processes less than `attackSpeed − 300` ms after the previous one (PlayerController.java:423-428). A client pacing exactly at its attack speed is therefore refused as soon as one `CM_ATTACK` waits more than 300 ms longer in the server's queues than the one before it. A round-trip p99 of 150 ms bounds the one-way queueing well below that difference | a player's perceived lag, which includes the network the fake client does not have |
| **K-2** | Cast accuracy (C-4) within ±10 %, tick interval (C-5) within ±10 % of `checktime` | the X4 standard of m5b2-plan.md | timing at the client over a real network |
| **K-3** | Every periodic manager's run time (S-4) p99 < 50 % of its period, and 0 coalesced periods (S-3) | above 50 % the next run starts late on any hiccup; a coalesce is a dropped run | why the run is slow (S-10, S-14 answer that) |
| **K-4** | Scheduled-pool and instant-pool lateness (S-2) p99 ≤ 50 ms | every timer in the game (casts, ticks, respawns, AI attacks) inherits it | per-task fairness |
| **K-5** | Packets waiting (S-5) not rising over any 30 s window | a rising queue is a queue that never drains | short bursts, which K-1 sees |
| **K-6** | Reclaimer lag p99 ≤ 1 s and backlog bounded over the step | P2's checked run held 144 ms p99 under far heavier synthetic load; 1 s is generous | leaks (CAP-9) |
| **K-7** | Pool `waiting` < pool size and no borrow timeout | a borrow timeout fails an enter world or a save | database-side slowness (S-13 measures that) |
| **K-8** | Private bytes over the ramp fit **a + bN + cN²** with a residual < 10 %, and at each fixed N the slope after warm-up is 0 within noise | b is the memory per player; c is R-4's crowd term (N − 1 known players per list at 147-171 B, plus queued packets), so a linear fit would be wrong by design; a slope at fixed N is growth, not size | memory per player in a real session's mix |

### 6.4 Regression rule for later milestones (proposal)

If a re-run of CAP-2 or CAP-4a after a later milestone (§4.11) moves its knee **down by more than 20 %**, the milestone's report says so and
names the row of §6.3 that moved. It does not block the roadmap; it goes to the user as a finding.

---

## 7. How the fake client must grow

`GameSession` (`game-server/tests/scenario/GameSession.h`) is a gate client: blocking, one thread per client (`StressRun.cpp:442-447`), and it
**records every server packet forever** (`std::vector<Packet> packets`, `GameSession.h:300`). In a crowd that is O(N² × t) memory in the harness -
at N = 200 moving at 2 steps/s the clients together receive ~80,000 `SM_MOVE`/s. It cannot be reused as is. Proposal: a new
**`CapacityClient`** in chunk P5-SC beside it, reusing `FakeGameClient`'s crypto and framing (`game-server/tests/support/FakeGameClient.h:89-190`)
and the independent decoders (`tests/scenario/decoders/`), with these properties:

1. **Records nothing it does not need.** Per opcode it keeps a count and a byte total; it decodes only `SM_NPC_INFO`, `SM_PLAYER_INFO`,
   `SM_DELETE` (its visible-object table), `SM_MOVE` (for C-2 probes only), `SM_ATTACK`, `SM_ATTACK_STATUS`, `SM_ATTACK_RESPONSE`,
   `SM_CASTSPELL`, `SM_CASTSPELL_RESULT`, `SM_ABNORMAL_EFFECT`/`SM_ABNORMAL_STATE`, `SM_STATS_INFO`, `SM_STATUPDATE_HP/MP`, `SM_EMOTION`,
   `SM_DIE`, `SM_PING_RESPONSE`, `SM_QUIT_RESPONSE`. Decoders stay written from Java's `writeImpl` (m5a-plan.md D9).
2. **Scales past a few hundred.** Either one reader thread per client with bounded buffers (simple; 1 MB reserved stack each, so 1,000 clients
   reserve 1 GB of address space) or one asio `io_context` with asynchronous reads for all clients on a few threads. The second is the
   proposal for N > 300; `TestSocket` already uses asio (`game-server/tests/support/NetworkTestSupport.h:191-207`).
3. **Always reads.** A client that stops reading is a CAP-8 client, never an accident; a separate switch makes it one.
4. **Behaviours** (each a small state machine, composable per scenario):
   - **walk** (random walk in a disc, a waypoint loop, follow a leader), with the real client's cadence, step and move type (CP-13), staying under
     `AntiHackService::canMove` (`CM_MOVE.cpp:123-126`), and **with terrain z from the oracle for every waypoint and every step**. With geo on, a
     player's attack is refused with `STOP_OBSTACLE_IN_THE_WAY` when `canSee` fails (`controllers/PlayerController.cpp:504`; Java
     PlayerController.java:411-414), and aggro uses `canSee` too (`CreatureEventHandler.cpp:107`); a guessed z leaves characters above or below
     the terrain. The M5a geo gate only interpolated z between oracle points (`M5aScenarioTest.cpp:1702`); the oracle already has `get_z`
     (`tools/oracle/geo/probes.py:117`), and CP-11 emits it per walk step;
   - **target** (nearest attackable npc from its own table, attackable by tribe as the oracle marks it; `CM_TARGET_SELECT`);
   - **attack** (at the attack speed from `SM_STATS_INFO`, as `fightUntil` paces, `GameSession.h:175-184`);
   - **cast** (a rotation over seeded skill rows with cooldown bookkeeping at 100 ms units, Skill.java:326, and MP awareness);
   - **die and revive** (`CM_REVIVE`, then the bind teleport and either the walk back or a relog with a re-seeded position, §4.4);
   - **return** (skill 243); **drop and reconnect** (CAP-6B); **probe** (C-1 every 5 s, C-2 on a sampled subset of moves).
5. **Timestamps every send and every receive** on one steady clock, and reports its own lateness (C-10).
6. **Logs in through the login server** exactly as today (`FakeLoginClient`, `ScenarioServers` with `loginserver.accounts.autocreate=true`,
   `ScenarioServers.cpp:131`), so login storms are real storms.
7. **A standalone driver executable** (`aion_gs_capacity`) rather than a gtest case, so a run can be started, stopped and resumed step by step,
   and so the client could later run on a second machine if the user has one (Q-9). A gtest wrapper registers the smoke gate (§9.3).

**Calibration against the real client (lesson 3).** Nothing in this repository says how often a real 4.8 client sends `CM_MOVE`, `CM_MOTION`,
`CM_HEADING_UPDATE` or `CM_PING_REQUEST` while running, fighting or casting, nor with which move types. CP-13 proposes a per-connection,
per-opcode, per-second counter in the check-output mode, run during one ordinary real-client session of the user's (20 minutes: walk, fight, cast,
idle). The counter sits **before** `AionClientPacketFactory::tryCreatePacket` (`AionConnection.cpp:200-205`), reading the opcode from the
decrypted buffer, so the opcodes that have no C++ handler (for which `tryCreatePacket` returns null) are counted too - they are the real-client
packets the census most needs (m5a-client-session.md, m5b-client-session.md list them). The `CapacityClient`'s cadence then comes from that file,
the way the gate's numbers come from the oracle.

---

## 8. Machine, builds and scheduling

### 8.1 Builds (proposal)

| Build | For | How | What it lacks |
|---|---|---|---|
| **Release** (unchecked) | **the knee numbers** | the `Release` configuration of the existing multi-config tree (`CMakePresets.json:6-12`; the generator expression at `game-server/CMakeLists.txt:60` sets `AION_CHECKED=0` for it) - no new build directory | lockdep, C1-C16, live counts, the delayed-free FIFO, PCT yield points (`AION_PCT` defaults to `AION_CHECKED`, `runtime/base/YieldPoint.h:20-21`); keeps Reclaimer assertions, leaf lock ranks and the watchdog (docs/design/README.md "Defaults") |
| **RelWithDebInfo** (checked, the day-to-day build) | **the hard bar at ~70 % of the Release knee** | the existing tree; beware that the build preset **named `msvc-release` is RelWithDebInfo** (`CMakePresets.json:25`), so a number from it is a checked-build number | its numbers are 10-30 % slower by design (runtime-architecture.md §15.1) - never quote them as capacity |
| RelWithDebInfo, unchecked (optional) | profiling with symbols (S-14) | a separate tree with `-DAION_CHECKED_MODE=OFF` (`game-server/CMakeLists.txt:50-58`) | a second tree costs disk and a full build; only if Q-10 says yes |
| Debug, ASan | **never for capacity** | - | see §8.2 |

**The unchecked Release server has never been built or run.** The only game-server executable in any build tree is
`build/msvc/game-server/Debug/aion_game_server.exe` (searched under `build/`); every measured server number is RelWithDebInfo
(phase4-status.md:395-402) or Debug, and the kernel was green in Release only for the P1 tests (runtime-kernel-status.md:21). `AION_CHECKED=0`
changes the compiled code, not only its speed: the checks, the PCT yield points and the live counters are compiled out. **Proposal: stage-A item
CP-00** - build Release once, re-green `gs.scenario.m5a`, `gs.scenario.m5b` and `gs.scenario.m5b2` in it, and run `capacity_smoke` in Release,
before stage B. A Release-only failure found there is a port bug, not a capacity finding.

### 8.2 Why Debug numbers mislead

- **They are not a uniform scale factor, so they reorder the bottlenecks.** The kernel's `Field`, `Ref` and `Monitor` wrappers are inline templates
  whose Release cost is a load or a CAS. Already the checked RelWithDebInfo build pays 3-10 times more for them (`Field<float>` load 0.2 ns vs
  1.9 ns, `Monitor` 13 ns vs 43 ns, `Field<Ref>` load 2.7 ns vs 14.2 ns; runtime-kernel-status.md "sync"), and Debug compiles them without
  inlining on top of the checks (inferred from `/Od`, not measured per primitive), while syscalls, encryption and database round trips cost about
  the same in every build. Code made of wrapper traffic (known lists, broadcasts, AI events) slows far more than IO-bound code, so the first
  bottleneck in Debug need not be the first in Release.
- **Measured spread on the same tree:** after phase-4 wave 3b-2 the startup path takes 193.9 s in Debug (on a loaded machine) against
  5.8-7.7 s in RelWithDebInfo, a factor of 25 to 33, and the peak working set is 2,618 MB against 1,668-1,678 MB; before that wave the pair was
  260.8 s against 10.0-11.0 s and 5,237 MB against 3,570 MB (phase4-status.md:343-353, 395-402). The M5a real-client session ran Debug at
  3.2 GB resident with **one** player (m5a-client-session.md).
- **Memory per player is distorted** by the debug CRT heap's per-allocation headers and fill patterns, and in both checked builds by the 64 MB
  delayed-free FIFO (C3), which is why S-8 subtracts it.
- **ASan** multiplies the Reclaimer's work and quarantines freed memory: P2's 30-minute reruns did 23.7k tasks/s under ASan Debug against 65k in
  checked RelWithDebInfo, with 873 MB peak versus 202 MB (runtime-kernel-status.md:22). ASan runs are for leaks (m5b2-plan.md G-06), not
  capacity.

### 8.3 The machine

The user's desktop: 32 logical CPUs, 64 GB, Windows 11, the machine they work on (runtime-kernel-status.md "bench"; the standing resource rule
recorded after a wave locked it up on 2026-09-21: no stress, soak or ASan run without asking; at most two lanes building at once, each with a
bounded job count). Everything - game server, login server, MariaDB (the portable 11.8 install in `D:\aion-dev`), the clients - shares it, so:

- **CPU partition (proposal):** game server 20 logical CPUs, clients 6, MariaDB and login server 4, 2 left for the desktop, set through the job
  objects the harness already creates (`ChildProcess.h:91-95`) and a job for the driver. A knee measured under an affinity cap is a knee **for
  that cap**, and the report says so. The alternative - no caps, in a slot when the user is away - gives a higher and more honest number (Q-5).
- **Memory budget:** the baseline is **CAP-1 at N = 0** (the server with its 83,872 npcs spawned), not phase4-status.md:351's ~1.7 GB, which was
  measured before `spawnAll` worked; plus N × the per-player figure and the N² term CAP-1 measures; clients bounded by §7 item 1; a job memory
  limit on the server child for CAP-8.
- **Database:** MariaDB's `my.ini` settings (buffer pool, connection limits) decide CAP-6/7 as much as the server does; the run records them and
  the user decides whether to tune them (Q-7). The `my.ini` in `D:\aion-dev\mariadb-data` sets only the port, bind address, character set and
  collation, so every other value is MariaDB's default.
- **Geo:** the user plays with geo on (m5a-client-session.md "Setup"); the scenario profile turns it off
  (`game-server/tests/scenario/ScenarioServers.cpp:33`). Runs with geo on need terrain z for every step (§7 item 4). Proposal: ramps with geo
  **on** once CP-11 emits terrain z, and one geo-off step at the knee to isolate its share; if the z output is not ready, ramps with geo off and
  one geo-on step at the knee. Geo's cost differs by site: at CAP-4a it comes from players' attack checks (`PlayerController.cpp:504`) and
  npc-against-npc aggro, and only CAP-4b has players triggering aggro `canSee`.

### 8.4 Never started unprompted (proposal)

- The capacity driver is registered like the stress nightly: **DISABLED unless the tree was configured with `-DAION_CAPACITY=ON`**, label
  `capacity`, the same `RESOURCE_LOCK` (`game-server/tests/scenario/StressTests.cmake:24-42` is the pattern). Proposed in addition: it refuses to
  start without `AION_CAPACITY_CONFIRM=<today's date>` in the environment, so a stale configuration cannot start it either. Whether the second
  guard is wanted is part of Q-11.
- No lane and no integrator step runs a capacity scenario. The integrator asks, names the scenario, the builds, the expected wall clock and the
  CPU/RAM it takes, and waits for the user's slot.
- The smoke gate (§9.3) is the only piece that could run in the normal suite, and only if the user agrees (Q-11); it is four clients for about
  90 s.

### 8.5 Run plan (proposal)

| Session | Scenarios | Builds | Wall clock (estimate) |
|---|---|---|---|
| 0 | CP-00: the Release build, re-green of `gs.scenario.m5a`, `m5b`, `m5b2`, `capacity_smoke` in Release | Release | one full Release build (not measured for this tree; the largest single cost) plus ~10 minutes of gates |
| 1 | calibration: CAP-1 N ≤ 50, one real-client packet census (CP-13) | Release | ~1 h plus the user's 20-minute session |
| 2 | CAP-1 and CAP-2 ramps | Release, then RelWithDebInfo at 70 % | ~2-3 h |
| 3 | CAP-4a, CAP-4b and CAP-5 | same | ~2-3 h |
| 4 | CAP-6, CAP-7, CAP-3, CAP-8 | same | ~3 h |
| 5 | CAP-9 soak | Release, then RelWithDebInfo | 4-8 h, overnight only if the user wants |

Sessions 1-5 add up to **about 12-18 hours, 8-10 without the soak**, plus session 0. A ramp of ~12 steps at 4 minutes each is ~50 minutes per
scenario per build. Builds run at the user's chosen time with `--parallel 6` or lower.

---

## 9. Work items, lanes, stages and the gate

### 9.1 Work items (proposed)

Effort: **S** 0.5-1 agent-day, **M** 1-2, **L** 2-4 (mixed letters span both). Owner = the chunk from `python tools/porting/chunks.py owner
<path>` (run for each file named): `main.cpp` and `CheckOutput.*` are **P5-14**, `Future.cpp` **P4-02b**, `AionConnection.cpp` and
`AionServerPacket.cpp` **P4-15**, the task managers and the world **P4-10**, `tests/scenario/**` **P5-SC**; `tools/oracle` and `commons/` have no
owner (commons is not modified by this plan).

| Id | What | Owner | Deps | Eff |
|---|---|---|---|---|
| **CP-00** | Build Release once; re-green `gs.scenario.m5a`, `m5b`, `m5b2` in it; later run `capacity_smoke` in Release (§8.1) | integrator, in a slot the user chose | M5b-2 done; CP-09 for the last step | S |
| **CP-01** | `--capacity-metrics` in `main.cpp`: the per-second TSV of §5.3 (S-1, S-5, S-7, S-8, S-9, S-11, S-12 and the header lines) from a dedicated sampler thread that opens and closes a `TaskScope` per sample; off unless the flag is given | P5-14 | CP-03, CP-04, CP-05, CP-15 for the S-4, S-3, S-6, S-15 columns (the other columns need none) | M |
| **CP-02** | Scheduler and instant-pool lateness canaries (S-2), started only in capacity mode | P5-14 | CP-01 | S |
| **CP-03** | Run-time histogram per periodic manager (S-4) in `AbstractPeriodicTaskManager.cpp:25`'s lambda, with a C++-only registry the sampler reads | P4-10 | – | S |
| **CP-04** | Coalesced-period counter in `Future::finishRun` (S-3) | P4-02b | header request | S |
| **CP-05** | `AionConnection::sendQueueDepth()` (S-6) and the sampler's aggregation over `World::forEachPlayer` | P4-15 (+ P5-14 for the sampler) | header request | S |
| **CP-06** | Blocking-wait counter hook for checked builds (S-10), installed only in capacity mode; a `docs/deviations/P5-14.md` row for the non-PCT use of `PctHooks` | P5-14 | – | S |
| **CP-07** | `CapacityClient` (§7 items 1-6) with tests over a fake server socket (`tests/scenario/FakeServerSocket.h` exists) | P5-SC | – | L |
| **CP-08** | The driver `aion_gs_capacity`: scenario definitions CAP-1 .. CAP-9, the ramp, V-1 .. V-4 checks, the per-scenario H-1 allow-lists of expected WARN/ERROR lines (§4.7), CTest registration DISABLED by default (§8.4) | P5-SC | CP-07, CP-11 | M-L |
| **CP-09** | `gs.scenario.capacity_smoke` (§9.3), and the **capacity partial allow-list** its G-6 and every run's H-1 read | P5-SC | CP-01 .. CP-08, CP-11, CP-15 | M |
| **CP-10** | Database proxy: latency, stall, statement/latency/in-flight counters (S-13); the capacity profile's `sslMode=DISABLED` | P5-SC | – | M |
| **CP-11** | `tools/oracle` commands. `capacity-site --map --center --radius`: spots and expected known-npc counts per position (walkers resolved as `m5a-spawns` does), attackable ids and aggressive-to-PC ids **by tribe**, respawn times and each cluster's kill capacity, npc-free discs with r + 95 m clearance, aggro sites, and **terrain z** (`get_z`) for every waypoint and walk step. `capacity-skills --skill-ids`: cast time, cooldown in ms, MP, periodic templates and their tick schedules (`300 + checktime`, `duration2`), including the schedule truncated at a given death time | tools/oracle | – | M-L |
| **CP-12** | `tools/capacity/report.py`: curves, knee per scenario, the row that broke, the K-8 fit, build header | tools | CP-01, CP-08 | S-M |
| **CP-13** | Client packet census in the check-output mode: per connection, per opcode (with the move type for `CM_MOVE`), per second, counted **before** `tryCreatePacket` (`AionConnection.cpp:200-205`) so unhandled opcodes count too | P4-15 for the counter, P5-14 for writing the file | – | S |
| **CP-14** | Run sessions 0-5 of §8.5 and write `docs/design/capacity-results.md` | integrator, **with the user** | all | – |
| **CP-15** | Serialization and enqueue counters (S-15) in `AionServerPacket::serialize` and `AionConnection::enqueue` | P4-15 | – | S |

**Totals on this scale:** eight S (CP-00, 02, 03, 04, 05, 06, 13, 15) 4-8, three M (CP-01, 09, 10) 3-6, two M-L (CP-08, 11) 2-8, one L (CP-07)
2-4, one S-M (CP-12) 0.5-2: **11.5-28 agent-days**. The first draft's "11-15" did not add up (its own items summed to 10.5-24).

### 9.2 Lanes and stages

- **Stage A (tools), 4 lanes** plus the integrator's CP-00, chunks disjoint, each lane followed by an adversarial reviewer whose first job is to
  mutation-test the lane's new assertions:

  | Lane | Chunks | Items | Effort on §9.1's scale |
  |---|---|---|---|
  | **server-metrics** | P5-14 | CP-01, CP-02, CP-06, CP-13 (file side) | 2-4 |
  | **kernel-hooks** | P4-02b, P4-10, P4-15 | CP-03, CP-04, CP-05 (accessor), CP-13 (counter side), CP-15 - small additive bodies behind header requests | 2.5-5 |
  | **capacity-harness** | P5-SC | CP-07, CP-08, CP-09, CP-10 | 5-12 |
  | **oracle-and-report** | tools/oracle, tools/capacity | CP-11, CP-12 | 1.5-6 |

  The long pole is **capacity-harness** (5-12 agent-days on this scale). **Calendar time does not follow from these numbers**: lanes run in
  parallel, and the measured pace of this project is faster than the scale suggests - M5b-1 went from its reviewed plan to its stage-2 gate in
  about 24 hours (`5f65cb14f`, 2026-09-22 02:29, to `23c4e6485`, 2026-09-23 02:32). Under the resource rule only two lanes build at a time:
  oracle-and-report builds nothing; kernel-hooks merges first (its header requests unblock server-metrics); server-metrics and capacity-harness
  then build in turn, each with `--parallel 6` or lower.
- **Stage B (runs):** CP-14, integrator plus user, sessions 0-5 of §8.5. About 12-18 machine-hours (8-10 without the soak) plus session 0.
  **Proposal:** stage B starts only when CP-02 .. CP-04 are in, because without them a run can look healthy while dropping timer runs (R-1);
  whether an exploratory run may go earlier is Q-14.
- **Stage C (fixes):** lanes by owning chunk for whatever stage B finds; unknown size. Two candidates are already visible, both **design fixes,
  not Java deviations**, since Java has neither cost: (1) **serialize a SHARED packet once per broadcast**, as runtime-architecture.md §8.3
  designed and the code does not do (R-4); (2) **the cheap type tag** for `as<Player>`/`as<Npc>` that P4 recommended (runtime-architecture.md §21,
  last paragraph), or making the hot classes eligible for `Ref.h:384-391`'s fast path. Anything that would change Java-visible behaviour (a
  bounded send queue, a different notify loop, a cap on known players) is a **deviation for the user to decide**, not a fix.

**Why after M5b-2 stage 3 and not before:** CAP-4 and CAP-5 are the scenarios the user deferred capacity for, and they need the npc skill rotation
(M5b-2 stage 2) and the casting stress client and `Effect` census rows (M5b-2 stage 3, m5b2-plan.md G-06/G-07); H-4's baselines only make sense
once passives apply. CP-03 .. CP-06, CP-11 and CP-15 have no M5b-2 dependency and could start earlier; the proposal keeps them in stage A so the
user's scenarios can still change what they measure.

### 9.3 The gate: `gs.scenario.capacity_smoke` (proposal)

Four `CapacityClient`s against a Release **or** RelWithDebInfo server for ~60 s of load (~90 s with startup; same `RESOURCE_LOCK`) at CAP-1's
site: a level-1 Priest with a seeded 3939 row, a level-1 Mage with a seeded 1447 row, and two plain characters. It is registered for the Release
and RelWithDebInfo configurations only and skips itself in Debug; its label depends on Q-11 (`scenario` if the user wants it in the normal suite,
otherwise opt-in like the stress nightly). **It never produces a capacity number**; it proves that the pipeline the capacity runs depend on
measures what it claims.

| # | Assertion | Proves | Cannot prove | Mutation it kills |
|---|---|---|---|---|
| **G-1** | `capacity_metrics.tsv` has ≥ 55 rows for 60 s, strictly increasing timestamps, every column parsed, and the header names the build, `AION_CHECKED`, geo and `sslMode` | the sampler runs and is attributable | that any value is right | the sampler thread never started; a header without the build line |
| **G-2** | Each client's visible-object table holds **exactly 3 players**, **at least** the oracle's deterministic npc count within 90 m of its position (each matched by id and position), and **only** npcs that are spots of their id within 95 m (+10 m for walkers) - the m5a-plan.md V1/V3 pattern, because 5 walker spots and 1 random-walk spot lie within 95 m of the site and their npcs move | the client's `SM_NPC_INFO`/`SM_PLAYER_INFO`/`SM_DELETE` tracking, which targeting depends on | correctness at high N; the exact count of moving npcs | the client ignores `SM_DELETE` (players or unknown npcs too many); the client drops `SM_NPC_INFO` (deterministic count too low); a site the server read differently from the oracle |
| **G-3** | Every client gets ≥ 10 probe round trips (C-1), all < 1 s | the probe path and the histogram | anything about load | a wrong opcode (no `SM_PING_RESPONSE`); a histogram bucket off by one decade (checked against the raw samples) |
| **G-4** | Two periodic effects against the oracle's schedule (initial delay `300 + checktime`, `AbstractOverTimeEffect.cpp:36-40`; end at `duration2`, `Effect.cpp:664-667`). **(a)** The Priest casts 3939 on itself: **exactly 14** `SM_ATTACK_STATUS` for its own object with skill id 3939 and log `HEAL`, at 2,300 + 2,000k ms ± 100 ms (k = 0..13) after its `SM_CASTSPELL_RESULT`. Nobody attacks it and a heal tick with a skill id is sent even at full HP (§4.5), so the effect ends by time and the count cannot be vacuous. **(b)** The Mage casts 1447 on the nearest attackable npc (the oracle's candidates at this site are the two `210115` spots at 21.2 m and 25.2 m, 143 HP): the count of `SM_ATTACK_STATUS` with skill id 1447 and log `SPELLATK` for that target equals the scheduled ticks (3,300 + 3,000k ms, k = 0..3) before its `SM_EMOTION` DIE, a tick within ± 100 ms of the death accepted either way; at least one tick is required, and the harness recasts on the next candidate, at most three casts. **A run in which (a) does not reach 14 ticks or no cast of (b) gets a tick FAILS.** | (a) C-5's counter, the 300 ms offset and the end task against the last tick; (b) the damage-over-time path (`SpellAttackEffect` → `onAttack` → `SM_ATTACK_STATUS`) that R-5 is about, under the same counter | behaviour when the scheduler is late; damage and heal values; (b) cannot show a full 4-tick *Erosion*, because no target within reach of a level-1 character outlives it (§4.5) | the client counting ticks of another effect or another target; a server whose initial delay drops the `300 +`; an end task that fires before the last tick (13 in (a)); an effect that outlives its target's death |
| **G-5** | S-2's canaries report ≥ 50 samples each, all < 1 s; S-3 reports 0 coalesces; S-4 reports runs of every manager it lists; S-15 reports serializations > 0 and enqueues > 0 | CP-02 .. CP-04 and CP-15 are wired | lateness under load (K-4 owns the 50 ms bound; a smoke gate that shares the machine with `ctest -j` cannot hold it without flaking) | a canary never scheduled; a canary recording the wrong unit; a manager missing from the registry; a counter never incremented |
| **G-6** | The M5a Q8 bar (H-1) with the capacity partial allow-list | the capacity profile reaches no unported body at N = 4 | anything at N = 400 | a capacity mode that wakes dormant code (lesson 2) |

### 9.4 Header requests expected

| Request | Kind | For |
|---|---|---|
| `runtime/sched/Future.h` or `ThreadPoolManager.h`: a static read of the coalesced-period count | additive | CP-04 |
| `network/aion/AionConnection.h`: `size_t sendQueueDepth() const` (C++ only, locks `guard`) | additive | CP-05 |
| `taskmanager/AbstractPeriodicTaskManager.h`: none if the run-time registry is a new C++-only header; otherwise an accessor | additive or none | CP-03 |
| `network/aion/AionServerPacket.h`: none if the S-15 counters live in a new C++-only header; otherwise a static read | additive or none | CP-15 |
| Optionally `runtime/sched/ExecutorBackend.h`: structured pool statistics instead of parsing `getStats()` text | additive | CP-01 |
| `commons/`: **none** - `getWaitingPacketCount` and the pool counters are already public | – | CP-01 |

### 9.5 Not proposed by the integrator, but open

- **A C++-only `gameserver.dev` switch that makes the server cheaper under test** (fewer notifies, smaller visibility). The integrator does not
  propose it, because it would measure a different server; but one of the user's own scenarios might want exactly that, for example to find the
  ceiling at a smaller visibility radius. It is Q-15, not a decision.
- Admin commands for `//debug` (the handlers are phase 6): the sampler reads the same `Introspection` functions directly.

---

## 10. Risks

1. **Measuring the harness instead of the server.** Clients and server share 32 cores, and in a crowd the clients receive and decrypt as many
   packets as the server's IO thread encrypts. V-1 and a CPU partition are the answer; a knee with V-1 failing is not reported.
2. **A fake client that is not a real client.** Its cadence and move types are invented until CP-13 measures the real ones. A wrong cadence moves
   every knee; a wrong z with geo on turns every attack into a refusal (§7 item 4).
3. **The number is provisional.** M5b-3, M5d and M5g add database writes and fan-out to exactly the paths measured here (§4.11). The report
   must say "as of milestone X".
4. **Build confusion.** The preset named `msvc-release` builds the checked RelWithDebInfo configuration (`CMakePresets.json:25`). The TSV
   header (CP-01) records `AION_CHECKED` so a checked number cannot pass for a Release one.
5. **The first Release build of the server.** It has never been built or run (§8.1), and `AION_CHECKED=0` compiles different code. CP-00 exists
   so that a Release-only failure is found by the existing gates, not in the middle of a capacity session.
6. **Wiring the capacity mode wakes dormant code (lesson 2).** Known now: the cross-map arm of `SpawnTask::run` (`InstanceService::onLeaveInstance`
   unported); `quest_use_item` npcs on a `DummyNpcAI` in CAP-4a's cluster; skills outside the 38-class set, and characters seeded to an advanced
   class or past level 9 of one, whose learned passives throw (§4.5, m5b2-plan.md D6). Not a risk after all: `PlayerReviveService::revive`
   (`services/player/PlayerReviveService.cpp:111-143`) is **ported**, apart from the `isNoResurrectPenalty = false` stand-in at :120-123, so a
   dead character teleported through `TeleportService.cpp:276-277` reaches no unported body there. The traces here go one call deep; each
   scenario's first run at N = 1 under the checked build is the real trace, and H-1 fails it on the first unported hit.
7. **Assertions with nothing to check.** The first draft's tick assertions could never find an effect that ended by time (§4.5). H-3 and G-4 now
   fail a step or run that has nothing to check; every new assertion should be read with the same question.
8. **The machine.** A capacity run is the heaviest thing this project does. §8.4's guards, the job-object limits and the user's slot are the
   mitigation; a run that makes the desktop unusable has also destroyed its own numbers.
9. **Silent degradation hides the knee** (R-1). If CP-02 .. CP-04 slip, a run can look healthy while dropping timer runs. The proposal is that
   stage B waits for them (Q-14).
10. **Thresholds taken as truth.** §6.3's numbers are argued, not measured; the first sessions may show that some are too strict or too loose.
    They are the user's to change (Q-4).

---

## 11. Questions for the user

- **Q-1.** Your scenarios: what do you want to see happen (§4.10)? Which of CAP-1 .. CAP-9 do you not care about?
- **Q-2.** What N would you call a success - the dozens a private server with friends sees, hundreds, or retail-scale thousands? It decides how
  far the ramp goes and whether a second machine for the clients is worth it.
- **Q-3.** For the soak (CAP-9): what mix of standing, walking, fighting and casting resembles how you expect the server to be played?
- **Q-4.** The thresholds of §6 (150 ms probe p99, ±10 % timing, 50 ms lateness, at least 20 effects ending by time per step): keep, tighten or
  loosen?
- **Q-5.** CPU partition with caps while you work, or uncapped runs when you are away?
- **Q-6.** The proposal seeds only **skill rows** on level-1 characters of the starting classes, which stays inside M5b-2's range. Seeding
  **levels or classes** (to cast *Sandblaster* as a real Spiritmaster, or to sustain *Erosion* with more MP) leaves it: an advanced class learns
  passives outside the 38 effect classes at once and throws (a Spiritmaster 11-30 autolearns 51 such skills, a Cleric 23, and even the level-10
  change to Cleric brings two, §4.5). Keep skill rows only, or wait for M5d/M5e before seeding classes?
- **Q-7.** Should the MariaDB `my.ini` be tuned for the runs, or measured as it is (today it sets only port, bind address, character set and
  collation)?
- **Q-8.** Re-run cadence: after every later milestone, only the ones in §4.11, or only when you ask?
- **Q-9.** Is there a second machine the clients could run on?
- **Q-10.** May a run use Windows Performance Recorder (needs an elevated prompt) for a CPU and lock profile at the knee?
- **Q-11.** Should `gs.scenario.capacity_smoke` (~90 s, Release/RelWithDebInfo only) run in the normal suite with the `scenario` label, or stay
  opt-in like the stress nightly? And do you want the second guard on the capacity driver (`AION_CAPACITY_CONFIRM=<today's date>`), or is the
  configure flag enough?
- **Q-12.** Would you spend 20 minutes of ordinary play with the packet census on (CP-13), so the fake client's cadence comes from your client?
- **Q-13.** If CAP-8 shows the unbounded send queue growing, do you want a bounded queue (a deviation from Java), or Java's behaviour kept?
- **Q-14.** Should stage B wait until the lateness and coalescing instruments (CP-02 .. CP-04) are in, or may an exploratory run go earlier,
  knowing it can miss dropped timer runs?
- **Q-15.** Do any of your scenarios want a cheaper test server - a smaller visibility radius, fewer movement notifications - to find a ceiling,
  knowing it measures a different server from the one you play (§9.5)?

---

## 12. What was measured and what was inferred

**Measured** (read-only; grep, reading, or a throw-away Python script over the Java data; re-runnable):

- Every file:line citation above, read in the tree at HEAD `c1edb0afb` plus the working tree of 2026-09-23.
- `nio.threads > 1` refused unless `...unsafe.allow=true` (`GameServer.cpp:279`, `NetworkConfig.cpp:18`); packet processor 4/4 threads and the
  checker only at min ≠ max; send queue unbounded in C++ and in Java; `max_players` default 100; `CM_PING` interval 180 s and the timer-cheat
  check; `CM_PING_REQUEST` answered without a rate check; no flood-filter thresholds in the Java `pff.properties`.
- **No `BlockingRegion` around DAO calls** (grep over `dao/` and `commons/src/aion/commons/database/`), contrary to runtime-architecture.md §2.6.
- `Npc` and `Player` not final; `MovementNotifyTask` casts with `as<Npc>`; `KnownList::stream` copies the list; `forEachPlayer`/`forEachNpc`
  cast every entry; **one serialization per recipient** (`AionConnection.cpp:128`; `AionServerPacket::serialize` has three callers and no cache),
  contrary to runtime-architecture.md §8.3 and `AionServerPacket.h:21-22`; `SM_MOVE` only for POSITION|MANUAL or IMMEDIATE moves;
  `SM_ABNORMAL_EFFECT` filtered by the added effect's slot.
- Every CREATURE_MOVED of a player allocates a `QuestEnv` (`CreatureEventHandler.cpp:51-55`); aggro `canSee` only after tribe aggression
  (:105-107); `validateAggro` is a level-difference check (:127-130).
- The FIFO warning resets unless the distinct-task count grows; the periodic managers feed RunnableStats merged into one row; the watchdog probe
  reads the Reclaimer's stats during play and warns past 10 s of lag.
- Live counts are checked-build only; `Introspection` has no admin command caller; the stress run sends no `CM_ATTACK` or `CM_CASTSPELL`.
- Build trees: the only game-server executable under `build/` is the Debug one.
- DAO call lines: 15 in `PlayerEnterWorldService.cpp` (6 of them in `GeneralUpdateTask`/`ItemUpdateTask`, :771-788; "24" was a grep for `DAO`
  that counted 9 `#include` lines), 31 in `PlayerService::getPlayer` (`PlayerService.cpp:180-273`).
- The database `sslMode` default PREFERRED and the absence of `sslMode` in both URLs; the contents of `D:\aion-dev\mariadb-data\my.ini`.
- Poeta: 1,029 spawn spots (comments excluded, matching m5b2-plan.md §12), 2 `temporary_spawn` blocks ~890 m from the Elyos spawn point; 27 spots
  within 95 m of it (5 walker, 1 random-walk, 21 deterministic), none aggressive to PC; within 25 m, `203049` (GENERAL, 12.0 m) and `210115`
  (143 HP) at 21.2 m and 25.2 m. The densest 95 m disc holds 117 spots around `(1004.9, 1125.47)`, 223 m from the spawn point, with the ids, HP,
  tribes and respawn times of §4.4, none aggressive to PC, 109 attackable, a kill ceiling of 5.9/s. Aggressive to PC by tribe: 231 spots of 27
  ids; the densest disc of them holds 52 around `(552.38, 1810.59)`, 1,011 m from the spawn point, with the ids, levels, HP, tribes and respawn
  times of §4.4.
- Skills: 739 templates whose effects are all in the 38-class set and include a periodic class (with `MPHealEffect`), 196 in `skill_tree.xml`;
  684 and 184 with the three original periodic classes; 681 and 184 for the 34-class set. The templates, costs and cooldowns of 243, 1447, 3742,
  3939; Java's cooldown unit of 100 ms (Skill.java:326). Autolearn: SPIRIT_MASTER 11-30 89 skills (51 outside the 38, passive 133), CLERIC
  11-30 80 (23 outside, passives 364, 367), CLERIC at 10 two passives outside (363, 366); the starting classes autolearn nothing above level 9 and
  nothing outside the 38.
- The heal packet rule: a heal with a skill id sends `SM_ATTACK_STATUS` even when HP does not change (`CreatureLifeStats.cpp:172-173`, Java
  :190-191).
- The lesson-1 check for the classes CAP-2/3/6 add beyond M5b-2's path (`TeleportService`, `MapRegion`, `KnownList`, `PlayerAwareKnownList`,
  `MovementNotifyTask`, `PlayerLeaveWorldService`, `CM_PING_REQUEST`, `CM_MOVE`, `AionConnection`, `ReturnEffect`, `SpellAttackEffect`,
  `AbstractOverTimeEffect`, `RespawnService`, `PlayableMoveController`, `ZoneUpdateService`, `LifeStatsRestoreService`): by method name, every
  Java method has a C++ counterpart except `TeleportService`'s anonymous `acceptRequest` (not on the return path) and `RespawnService.DecayTask`
  (an inner class whose name the check does not match); `TeleportService` carries 21 `AION_UNPORTED` bodies, the others 0. The check is by
  name only, so a missing overload next to a present one would not show. `PlayerReviveService::revive` is ported (:111-143).
- The numbers of §2.1 are **measured by the documents cited there**, not re-measured here.

**Inferred, and a run must confirm before anyone relies on it:**

- **Which bottleneck comes first.** R-2's ~1,500-mover saturation, R-4's known-list memory and the relative order of IO thread, packet
  processor, notify loop, per-recipient serialization and timer heap come from P4's per-operation costs and from reading the code - not from
  running the server.
- R-2's ~117,000 `QuestEnv`s per second at 500 movers: movers × npcs known per mover × 2 runs per second, with ~117 npcs known taken from the
  densest disc; the real count depends on positions.
- The move cadence (2 steps/s) and move types behind every O(N²) estimate; CP-13 exists because they are unknown.
- That database waits pin the Reclaimer for their duration in the paths named (R-6), and that a 30 s stall can reach the BACKLOG dump: it
  follows from the design's publication rule, P2's evidence and R-2's rate; the size of the effect is unmeasured.
- That the MariaDB 11.8 install offers TLS by default (so the proxy would see only ciphertext without `sslMode=DISABLED`).
- That no target within reach of a level-1 character outlives *Erosion*: the ~405 damage is the template values (81 + 4 × 81); the damage formula
  was not run, and it could be lower against some targets.
- That a seeded skill row above the character's level (3939 on a level-1 Priest, 1447 on a level-1 Mage) is castable: D3's reading of
  `SkillEngine.getSkillFor`, not a run.
- That re-seeding the `players` row's position before a relog places the character there (CAP-4b).
- That the same-map *Return* path reaches nothing unported beyond the first call level.
- That `AREA` skills work for a level-1 character: the property code is ported, nothing was cast.
- K-1's link between queueing jitter and Java's 300 ms attack tolerance: sound arithmetic on PlayerController.java:423-428, but the fake client's
  pacing, not a real player's, is what it protects.
- Every effort letter and the stage-A total: from comparing with M5a's and M5b's lanes, not measured.
- The session wall-clock estimates of §8.5, and the cost of a full Release build of this tree.

---

## 13. Review, 2026-09-23

An adversarial review of the first draft returned "needs revision" with 2 high, 11 medium and 7 low findings. Each was re-checked against the
code and the data before anything changed; all were confirmed, two with small numeric corrections and one with a different fix.

| Finding | Severity | Re-check | What changed |
|---|---|---|---|
| G-4 and H-3 could never find an effect that ended by time | high | Confirmed: the only targets within *Erosion*'s 25 m of CAP-1's site are `210115` (143 HP); the CAP-4 cluster has 2-282 HP; death ends effects (`CreatureController.cpp:209`). The reviewer's first fix (a target with more HP) exists on Poeta only among level-6-7 npcs that aggro and kill level-1 characters, 1,011 m away - unsuitable for a 90 s smoke gate. While re-checking, a better vehicle turned up: a heal tick with a skill id is sent even at full HP (`CreatureLifeStats.cpp:172-173`), which also corrects the first draft's C-5 note | G-4 now asserts a self-cast 3939 heal-over-time (exactly 14 ticks, cannot be vacuous) plus *Erosion* ticks up to the target's death; H-3 truncates the schedule at death and fails a step with fewer than 20 effects that ended by time; §4.5 explains why; risk 7 added |
| The CAP-4 cluster has no npc aggressive to PC | high | Confirmed: 0 of 117 by tribe; 231 aggressive-to-PC spots of 27 ids on Poeta, 52 in the densest disc at `(552.38, 1810.59)`, 1,011 m away. The review's pointer to m5b-plan.md D11 for the tribe lesson is really A5a and :511-521 | CAP-4 split into CAP-4a (grind sites) and CAP-4b (aggro site); C-8 moved to 4b; R-2 and §8.3 say where player-driven `canSee` runs; "almost all aggressive" removed |
| Broadcasts serialize once per recipient | medium | Confirmed (`AionConnection.cpp:128`; no cache in `AionServerPacket.cpp:70-90`) | R-4 and §1 item 4 rewritten; S-15 and CP-15 added; "serialize once per broadcast" listed as a stage-C design fix |
| `Player` is not final either | medium | Confirmed (`Player.h:79`; P4's 12.3 vs 5.2 µs) | R-4 and §1 item 4 charge the cast to every broadcast; the type tag listed for stage C |
| Every movement notification allocates a `QuestEnv` | medium | Confirmed (`CreatureEventHandler.cpp:51-55`) | R-2, R-6, R-9 and S-11 updated; the rate estimate marked inferred |
| H-4's "return to 0" while running | medium | Confirmed (passives and post-spawn effects are permanent) | H-4 compares against a warm-up baseline at the same N; strict 0 only after shutdown |
| CAP-7's silent watchdog | medium | Confirmed, and the BACKLOG dump is reachable too | §4.7 lists the expected WARN and ERROR lines; the ERROR ones go into CAP-7's H-1 allow-list |
| Release never built or run | medium | Confirmed (only `build/msvc/game-server/Debug/aion_game_server.exe`) | CP-00 and session 0 added; §8.1 and risk 5 |
| Geo-on runs need terrain z | medium | Confirmed (`PlayerController.cpp:504`; `M5aScenarioTest.cpp:1702`; `probes.py:117`) | CP-11 emits z per step; §7 item 4; §8.3 makes geo-on ramps conditional on it |
| The DB proxy cannot see statements through TLS | medium | Code confirmed; that MariaDB offers TLS remains inferred | `sslMode=DISABLED` in the capacity profile and the TSV header; fallback to in-server DAO timing |
| G-2's exact npc count | medium | Walkers confirmed (5 walker, 1 random-walk within 95 m). The two `temporary_spawn` blocks are ~890 m from the site and do not affect it | G-2 and V-4 use "at least the deterministic count, only spots within 95 m (+10 m)"; §4.1 notes the temporary spawns |
| CAP-4's load stops growing; revive teleports away | medium | Confirmed; the kill ceiling of the 109 attackable spots is 5.9/s (the review said about 5) | CAP-4a spreads N over clusters sized by kill capacity; the revive walk back is modelled |
| Items stated as settled | medium | Confirmed | §8.4 marked proposal; smoke label tied to Q-11; stage-B prerequisite is Q-14; §9.5 became Q-15 |
| DAO line count | low | Confirmed (15 and 31) | R-6, §1 item 3, §12 |
| Instrumentation statements | low | Confirmed | R-2 and §2.2 rows corrected |
| Citation and wording errors | low | Confirmed | `World.cpp:190-192`, `MovementNotifyTask.cpp:72`, `GameServer.cpp:279` + `NetworkConfig.cpp:18`, `CM_MOVE.cpp:135-137`, `SM_ABNORMAL_EFFECT.cpp:22-27`, `revive` ported (:111-143), `AionConnection.cpp:112` |
| Effect-class subset out of date | low | Confirmed (739/196; 684/184) | §4.5 and §12 |
| Effort, hours, dependencies | low | Confirmed (10.5-24; 5-12; 12-18 h) | §9.1 recomputed (11.5-28 with CP-00 and CP-15), calendar claim removed, CP-01 deps, CP-13 moved before `tryCreatePacket`, the sampler's `TaskScope`, the capacity partial allow-list in CP-09 |
| Seeding levels or classes | low | Confirmed, and the level-10 change to Cleric adds two passives outside the set | Q-6 and §4.5 rewritten; §4.11 row added |
| Thresholds and baselines contradicting the analysis | low | Confirmed | K-8 fits a + bN + cN²; CAP-2's clearance r + 95 m; CAP-1 at N = 0 is the memory baseline; the stress-run caveat in §2.1; G-5 no longer asserts the 50 ms bound and the gate skips Debug |

No finding was rejected.
