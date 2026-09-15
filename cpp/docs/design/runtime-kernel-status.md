# Runtime kernel status (prototypes P1-P4)

> Implemented 2026-09-13 by a workflow of 12 agents (headers, 4 implementation areas, services, stress harness, benchmark, 2 reviewers,
> 2 fixers) against [runtime-architecture.md](runtime-architecture.md). Design corrections found during implementation are in §21 there.

## Layout

`cpp/game-server/src/aion/gameserver/runtime/` with static libraries in strict dependency order (no cycles):
`base` ← `sync` ← `lifetime` ← {`fields`, `collections`, `sched`} ← `services`. Ported Java facades keep their Java paths
(`utils/ThreadPoolManager.h`, `utils/idfactory/IDFactory.h`, `services/cron/CronService.h`, `utils/cron/ThreadPoolManagerRunnableRunner.h`).
Tests: `cpp/game-server/tests/runtime/<area>`, stress harness in `tests/runtime/stress`, P4 benchmark in `cpp/game-server/bench`.
Spine step S0a added `base/Unported.h/.cpp` (`AION_UNPORTED`, moved from the handler registry) and `services/RuntimeLifecycle.h/.cpp` (kernel
start/shutdown, runtime-architecture.md §23); the libraries are chunks P4-02a/P4-02b of `game-server/chunks.cmake`.
Build presets: `msvc` and `msvc-asan` (AddressSanitizer; ASan DLLs are copied next to test executables). `AION_CHECKED` is on in Debug and
RelWithDebInfo; PCT yield points and protocol mutation switches exist in checked test builds.

## Gates

| Gate | Result |
|---|---|
| P1 kernel conformance | lifetime 144 (incl. reclaimer liveness tests), sync 49, fields 22, collections 51, base 8 tests; Debug, ASan and Release green; protocol mutation tests fail when a protocol step is removed |
| P2 stress harness | 10-minute runs: 2x ASan Debug, 2x checked RelWithDebInfo; 30-minute checked RelWithDebInfo run of the final harness: PASS (backlog 0, census 0, zombie cuts 0, lockdep 0 cycles, no watchdog dumps, lag p99 156 ms); **First 30-minute ASan Debug run: FAIL (liveness)** — no memory errors, but the Reclaimer collapsed (0.1 scans/s, lag p99 73 s, backlog 7.0M / 1.4 GB, 15.9 GB private) because scans re-walked the whole backlog and the epoch advanced once per scan. **Fixed** with an epoch-bucketed limbo and bounded, resumable scans (design §21); reviewed adversarially (no safety bug; 6 findings fixed with tests). 10-minute ASan run after the fix: PASS, 85 scans/s, lag p99 63 ms, backlog max 6k, 843 MB peak. **30-minute gate reruns after the fix (2026-09-13): both PASS.** ASan Debug: 23.7k tasks/s, 46k objects destroyed/s, 84 scans/s, lag p50 32 / p99 63 / max 92 ms, backlog max 6,081, peak 873 MB, canary 0, census 0, lockdep cycles 0, watchdog dumps 0. Checked RelWithDebInfo: 65k tasks/s, 126k destroyed/s, 46 scans/s, lag p50 41 / p99 144 / max 296 ms, backlog max 30,359, peak 202 MB, canary 0, census 0, lockdep cycles 0, watchdog dumps 0 |
| P3 scheduler/Future/cron | sched 80, services 66 tests; every Java usage pattern on real pools and the DeterministicExecutor |
| P4 movement/known-list benchmark | tick p99 4.3-5.5 ms checked, 3.8 ms Release (target < 50 ms) |
| Review | 2 reviewers: 6 high findings, all fixed; 31 findings fixed in total, 1 rejected, 5 deferred (see open items) |

## Measurements

### lifetime

Release build (unchecked, no PCT), single-threaded and uncontended, from `aion_gs_runtime_lifetime_tests --gtest_also_run_disabled_tests --gtest_filter=*LifetimeCostBench*`:

| Operation | Cost |
|---|---|
| Ref copy + non-last release (with the corrected stamp) | 3.07 ns |
| `ensurePublished` when already published | 1.9 ns |
| Ptr from Ref + dereference | 0.94 ns |
| TaskScope enter/exit | 25 ns (design estimate ~3 ns; see openIssues) |
| makeRef + last release | 55 ns |
| Scan + destruction | 79 ns/object |

Sizes: RefCounted 48 bytes unchecked; Ptr and Ref 8 bytes each.

Test timings:
- PCT, Debug: about 0.35 ms per 4-thread schedule. 5000 schedules × 8 random scenarios took 14.7 s; nightly 100k per scenario would take about 35 s each.
- Lifetime suite in-process: Debug 24 s, ASan 31 s.
- Most of that time is the mutation death tests, about 0.8–1 s each for child startup.
- `ctest -j8` for lifetime + base in Debug: 5 s.

### sync

Disabled test SyncCostsBenchTest.DISABLED_PrimitiveCosts, single thread, uncontended, 5 M iterations each.

| Operation | Release (unchecked) | RelWithDebInfo (checked) |
|---|---|---|
| Monitor lock+unlock | 13.0 ns | 43 ns |
| Reentrant SYNCHRONIZED x2 | 16.8 ns | 52 ns |
| Monitor nested under another monitor (lockdep edge cache hit) | 12.9 ns | 53 ns |
| StampedLock readLock+unlockRead | 13.4 ns | 38 ns |
| Field<float> load | 0.2 ns | 1.9 ns |
| Field<int32_t>++ | 0.2 ns | 3.1 ns |
| AtomicInteger incrementAndGet | 1.5 ns | 2.1 ns |
| Field<Ref> load + dereference | 2.7 ns | 14.2 ns |

- The Release Monitor cost is within the design's ~20 ns estimate (§2.5).
- Test run times: sync tests about 1.5 s per full run and fields tests about 3.5 s, in Debug. With ctest -j8 in the ASan directory, the 65 tests take about 5 s.

### collections

Release build, single thread, 10k-entry maps, Ref<Npc> values (CollectionsMicroBenchTest, AION_COLLECTIONS_BENCH=1):

| Operation | Cost |
|---|---|
| ConcurrentHashMap::get, lock-free | 7.7 ns |
| ConcurrentHashMap::get, AION_CHM_LOCKED_READS | 22.0 ns |
| HashMap::get (collection Monitor) | 36.0 ns |
| ConcurrentHashMap::put, replacing an existing key | 83 ns |
| ConcurrentHashMap::compute, replacing | 91 ns |
| ConcurrentHashMap value iteration | 2.5 ns per element |
| ArrayList::get | 15.5 ns |
| ArrayList snapshot iteration of 300 | 533 ns per loop |
| CopyOnWriteArrayList in-place iteration of 300 | 289 ns per loop |
| ArrayList add + removeAt | 32 ns |

- Put and compute include one node allocation plus retireNode, which goes through the lifetime stub's mutex queue.
- Contended run, 3 lock-free readers and 1 writer on one map: about 78M gets/s per reader and 5.4M puts/s.
- Test run times: full Debug suite about 1.1 s, ASan suite about 2 s, `AION_PCT_SCHEDULES=300` on the PCT tests about 7.5 s under ASan.

### sched

- Scheduler heap, DeterministicExecutorTest.FiftyThousandTimers, 50k timers with random delays:
  - Release: schedule (Future allocation + Pin + heap push) 181 ns/op; cancel 29 ns/op; pop + run + `Reclaimer::reclaimNow` 661 ns/op. All below the design's P3 target of < 1 µs per heap operation.
  - Debug: 1524 / 175 / 7422 ns.
- PCT cancel vs run vs get: 5000 schedules in 3.5 s, no failure. The default run uses 200 schedules (about 140 ms); raise it with AION_PCT_SCHEDULES.
- Suite wall time: Debug about 3.5 s, ASan about 6.4 s, Release about 3.1 s.

### services

Release build, single thread, from `aion_gs_runtime_services_tests --gtest_also_run_disabled_tests --gtest_filter=*ServicesCostsBench*`:

| Operation | Cost |
|---|---|
| IDFactory `nextId` | 22 ns |
| IDFactory `releaseId` into quarantine | 47 ns |
| CleanerQueue `push` | 27 ns |
| CleanerQueue drain | 23 ns per id |
| LeakCensus remove+add events | 75 ns |
| Moving census events into the table during a scan | 92 ns per event |
| CronExpression next fire time, UTC | 6.8 µs |
| CronExpression next fire time, Europe/Berlin | 15.8 µs |

- The cron cost is dominated by MSVC tzdb `get_info`/`to_local`, which go through ICU. Under ASan they cost 0.2-1 ms per call. Cron computes fire times rarely, so this is fine; it is why the conformance test does not test every expression in every zone.

Suite wall times:
- Debug: about 3.2 s (64 tests).
- ASan: about 6 s.
- `ctest -j8` in the ASan dir: 2.9 s.
- Release: 61 passed, 3 PCT skipped.

Slowest tests:
- Cron conformance: 0.66 s Debug, 2.7 s ASan.
- The first logged exception with a symbolized stack: about 1 s.
- The 3 PCT tests at 200-300 schedules: about 0.1 s each in Debug. With `AION_PCT_SCHEDULES=3000` they take 0.9-1.5 s each and stay clean.

### stress

Machine: 32 logical cores. 64 workers, 32 instant and 4 scheduled pool threads, faults at 5‰.

**Checked RelWithDebInfo, final harness, 600 s**
- Throughput: 63.8k tasks/s, 393k kernel ops/s, 48.5k objects created/s, 124k objects destroyed/s during the run (objects, parts and nodes counted by the Reclaimer).
- Futures: 8.0M scheduled, 14.3M bodies run including 6.7M periodic runs, 1.36M serial tasks.
- Other volume: 0.93M resurrections, 36M quiescentPoints, 0.73M C1 stale-borrow detections, 34.5M parts created.
- Reclamation: 36.8 scans/s at a 20 ms period; lag p50 48 ms, p99 148 ms, max 339 ms; backlog max 29.3k objects, avg 9.2k; backlog max 5.2 MB.
- Memory: baseline 3.1 MB private / 0.5 MB heap in use; peak 187.6 MB private / 112.6 MB heap; final 157.2 MB / 74.0 MB. Final heap is flat across run lengths (74.9 MB after 30 s, 74.8 MB and 74.0 MB after 600 s); most of it is the 64 MB delayed-free FIFO.
- RESULT FAIL: backlog counter wrapped to 2^64-3 (openIssues 1), plus one watchdog BACKLOG dump caused by it. All other checks were clean: 0 canary failures, 0 unexpected exceptions, 0 pin violations, 0 lockdep cycles, 0 deadlock or stall dumps, census 0, cleaner 29.08M/29.08M.

**Checked RelWithDebInfo, earlier harness version (no linger op), 600 s:** PASS.
- 75.1k tasks/s, 462k ops/s, 57.2k objects created/s, 146k destroyed/s.
- Lag p50 226 ms, p99 760 ms, max 1309 ms; backlog max 145k / 24.4 MB.
- Memory peak 275.6 MB private / 188.4 MB heap; final 192.6 / 74.8 MB.

**ASan Debug, final harness, 600 s:** PASS.
- 23.7k tasks/s, 146k ops/s, 18.0k objects created/s, 45.8k destroyed/s.
- 5.5 scans/s; lag p50 178 ms, p99 463 ms, max 942 ms; backlog max 28.4k / 5.7 MB.
- Memory: private 62.9 MB baseline, 912 MB peak, 790 MB final (ASan quarantine; heap metric not applicable).
- Teardown drain 564 ms; every check clean.

**ASan Debug, earlier harness version, 600 s:** PASS. 22.7k tasks/s; lag p50 170 ms, p99 500 ms, max 600 ms.

**Teardown drain:** 7 scans and about 50 ms in RelWithDebInfo; 5 scans and about 560 ms under ASan.

**Smoke (8 workers, 4 s):** about 20-25k tasks/s in Debug and RelWithDebInfo, 17k under ASan.

**Self-test** (RelWithDebInfo, 16 workers, 8 s per mutation): 11 of 17 mutations detected. Most are detected within 0.05-1.6 s. SCAN_NO_ADVANCE takes 8.3 s (epoch-stuck check). SCAN_KEEP_WITHOUT_CLEAR and SCOPE_EXIT_NO_UNPUBLISH take about 38 s (end-of-run leak or drain check).

**Test time:** ctest -R "KernelStress|StressReproducer" takes 5.5 s in Debug and 5.9 s under ASan.

### bench

Machine: 32 logical CPUs, Windows 11, MSVC. ForkJoin parallelism 31. Defaults: 80k npcs, 2,000 movers, 200 players, 5 connections, 16 maps × 2048 m, 200 ms period, churn 100/s, SM_MOVE every step, lock-free CHM reads, PartMap regions. Measured window 300 ticks.

**Tick latency (parallel)**

| Run | p50 | p95 | p99 | max |
|---|---|---|---|---|
| Checked, run 1 | 2.65 ms | 3.78 ms | 4.28 ms | 5.22 ms |
| Checked, run 2 | 3.14 ms | 4.41 ms | 5.47 ms | 6.16 ms |
| Checked, run 3 | 3.07 ms | 4.05 ms | 4.54 ms | 5.08 ms |
| Release, run 1 | 2.20 ms | 3.10 ms | 3.83 ms | 4.09 ms |
| Release, run 2 | 2.23 ms | 3.24 ms | 3.82 ms | 4.13 ms |

- Summed mover time per tick p50: 62-71 ms checked, 52 ms Release.
- An earlier checked build of the same model had one outlier run: p99 16.9 ms, max 22.8 ms. Single movers stalled 15-20 ms with no matching lock waits, most likely OS preemption.

**Variants (checked unless noted)**

| Variant | p99 | max | Notes |
|---|---|---|---|
| Locked reads | 3.86 ms | 6.69 ms | allocations double: 30,951 vs 16,272 per tick |
| Fast instanceof | 3.87 ms | — | |
| 200 connections | 3.93 ms | — | CONNECTION_QUEUE waits 222 → 5.5 per tick |
| Java mask rule | 11.6 ms | 21.2 ms | earlier build, OS stalls |
| Synchronized walkers | 42.9 ms | 70.1 ms | summed mover time p99 1,351 ms |
| Synchronized walkers, Release | 35.6 ms | 57.9 ms | |
| Serial movement | 46.3 ms | 49.6 ms | p50 37.3 ms |

- With synchronized walkers and counting hooks on the arrival tick: 294 blocking waits per tick on CONTAINER_SLOT and 271 on CONNECTION_QUEUE.
- Serial runs, uncontended CPU per tick:
  - Release: lock-free 30.1 ms, locked 26.1 ms, flat regions 26.1 ms, Java mask rule 7.1 ms, no players 1.2 ms, fast instanceof 16.2 ms.
  - Checked: lock-free 37.8 ms, locked 40.0 ms, flat 36.1 ms, Java mask rule 12.4 ms, no players 2.4 ms, fast instanceof 31.6 ms.
  - Lock-free vs locked reads is within run-to-run noise at this scale.

**Phase profile (serial, µs per call, Release / checked)**

| Phase | Release | Checked | Calls per tick |
|---|---|---|---|
| Move element | 15.2 | 19.1 | 2,000 |
| World.updatePosition | 0.29 | 0.48 | |
| — getRegion (PartMap) | 0.19 | 0.28 | |
| broadcastPacket(SM_MOVE) | 12.4 | 13.9 | |
| — forEachPlayer (iteration + as<Player> + sends) | 12.3 | 13.8 | |
| — forEachPlayer with fast instanceof | 5.2 | 10.6 | |
| — serialize | 0.33 | 0.53 | 609 |
| — enqueue | 0.16 | 0.22 | 3,010 |
| notifyMoveObservers | 0.16 | 0.21 | |
| KnownList.update | 177 | 444 | 96 |
| — forget pass | 19.5 | 33.8 | |
| — region scan | 70.6 | 132 | |
| — player flag scan (forEachNpc, ~5k npcs) | 206 | 657 | 40 |
| addPair | 1.33 | 2.06 | 1,262 |

**Allocations, whole process, per tick**
- Checked: 16,272 allocations, 2.8 MB.
- Release: 13,835 allocations, 2.1 MB.
- Live heap drift over 60 s:
  - Release: 7.4 MB.
  - Checked: 90 MB, of which the 64 MB delayed-free FIFO is most; with `--delayed-free-mb 0` it is 3.4 MB.

**Refcount traffic and contention (checked, whole process, per tick)**
- Refcount: 7,683 retains and 7,441 releases. 6,228 of those releases (84%) ran the stamp CAS added by the lifetime agent.
- Top kernel operations per tick:
  - ConcurrentHashMap iteration: 538k buckets and 297k nodes, mostly the player flag scans.
  - Field loads: 196k scalar, 68k Ref.
  - Monitor locks: 14.9k.
  - Leaf-mutex locks: 3.45k.
- Blocking waits per tick: CONNECTION_QUEUE 185-435, SCHEDULER 23-48, CONTAINER_SLOT 3-8 (294 in the synchronized-walker arrival tick), Monitor 1.
- Model events per tick: 1,169 pattern-1 pair adds, 554 forgotten pairs, 44 walker re-adds, 1,754 isEmpty fast paths vs 203 delivered observer notifications, 28 region changes, 20.4 spawns and despawns, 872 packets serialized, 3,016 enqueued.

**Reclamation**
- Reclaimer lag at tick end: max 22 ms.
- Backlog: max about 2.8k entries (126 KB).
- Removed Npc to destroyed: p50 48 ms, p99 71 ms, max 82 ms (45 / 64 / 82 ms Release). This is a three-level cascade (map node, then KnownObject, then Npc), one scan per level.
- With synchronized walkers the backlog reaches 39-44k entries and 1.6-1.8 MB, and lag reaches 66-73 ms.
- About 3.2M objects and nodes destroyed per run.

**Memory**

| | Checked | Release |
|---|---|---|
| Per Npc: object graph | 640 B | 568 B |
| Per Npc: world registration | 261 B | 261 B |
| Per known entry (KnownObject + node + tables) | 171 B | 147 B |
| Per active Npc (about 32 entries, incl. 16-stripe arrays and tables) | 6.4 KB | 5.5 KB |
| Live heap after setup | 134 MB | 120 MB |
| sizeof Npc / WorldPosition / KnownObject / RefCounted / Monitor | 152 / 88 / 72 / 56 / 24 | 144 / 80 / 64 / 48 / 24 |

- 10.8k of 80k npcs have a non-empty known list; average 31.9 entries per active npc and 57.6 per player.
- Map instances and regions: 7.3 MB.

**Packets**
- About 1.0M packets and 35.5 MB per run.
- 29-32% of queue inserts are out of arrival order, so the sequence-ordered insert has to scan.
- Max queue depth 2,402 with a 50 ms drain.

**Pair adds and handshake**
- At setup: pattern 1 177,825, pattern 2 397, pattern 3 3.
- In steady state pattern 3 stays at 0, because a relogging player's own spawn update pairs the flags first (Java behaviour).
- The handshake undo never triggered in default runs, even at 3,000 despawns/s (185k despawns, 2.76M pair adds).
- With `--despawn-window-us` it triggered: 8 undos (1000/s, 300 µs window), then 7 checked and 13 Release (400/s, 2 ms window, synchronized walkers). Always with 0 ghosts.

**Build times**
- Model TU compile: about 1 min per config (two instantiations).
- Setup phase (80k npcs, spawn, players): 1.0-1.6 s.

## Implementation choices where the design was silent or had to be interpreted

### headers

- Library layering: locks (sync) sit below lifetime because RefCounted contains a Monitor. Field/Final/Array/Atomic* form a separate target (aion_gs_runtime_fields, owned by the sync agent) above lifetime, because they need Ref and ensurePublished. ThreadContext is a per-thread registry in a new base target, so the lifetime and sync layers can share per-thread state without depending on each other. This avoids the lifetime/sync cycle without merging the targets.
- BlockingRegion lives in the sync target, not next to TaskScope as in the §1.2 code block (it feeds the watchdog and lockdep). It records into ThreadContext only.
- The design puts Future, Pin, TaskArg, PinnedCallback and TimeUnit in aion::gameserver::utils (§7.1). They are declared in aion::gameserver::runtime (runtime/sched, no Java counterpart file) and re-exported into utils with using-declarations in utils/ThreadPoolManager.h, so both spellings work.
- The design's `AllFieldsAre<F, TaskArg>` needs concept template parameters (C++26). It is replaced by `AllFieldsAreTaskArgs<F>`, implemented without Boost.PFR: aggregate initializer counting plus initializers that convert only to TaskArg types.
- `Isolation::JOIN` / `Isolation::PER_ELEMENT` are tag objects in a struct instead of an enum, so the PER_ELEMENT element constraint is checked at compile time. The call syntax is the same as in the design. With PER_ELEMENT only helper threads run elements (the caller waits in a BlockingRegion), because the caller cannot open an outermost scope inside its own task.
- SYNCHRONIZED is the block form only (`SYNCHRONIZED(x) { ... }`), implemented as `if (guard; false) {} else`. A synchronized method wraps its body in `SYNCHRONIZED(*this) { ... }` rather than using a declaration-style first statement.
- `TaskArg` accepts `std::weak_ptr<AionConnection>` through a `TaskArgExtension<T>` customization point, because the kernel cannot name AionConnection. Static data templates are recognized by a `StaticTemplate` marker base or the `IsStaticTemplate<T>` trait.
- Added `Reclaimer::retireNode(unique_ptr<RetiredNode>)` for epoch reclamation of runtime memory without a reference count (ConcurrentHashMap node tables, CopyOnWrite arrays, Field<std::string> boxes). Also added `Reclaimer::drain`, post-scan hooks and `setDestroyObserver`, which LeakCensus and CleanerDrain use to stay below the Reclaimer without upward dependencies.
- `PartSlot`, `PartMap` and `PartList` take the owner in their constructor. `SelfOrRef` takes either the object itself (TargetField) or the part, whose partOwner() is resolved at store time. `PartSlot::get()` returns P* as in the design, `PartMap::get` returns Ptr<P>.
- ConcurrentHashMap.newKeySet() maps to a separate `ConcurrentKeySet<K>` shim. The Java collections that are unused or replaced are not provided: ConcurrentSkipListMap/Set, WeakHashMap, EnumSet fields. The Java-only shims ConcurrentLinkedDeque, CopyOnWriteArraySet and AtomicLongArray were added because the Java sources use them.
- Collection API naming: Java remove(int) becomes removeAt(int) to avoid ambiguity with remove(Object) for int elements. Map reads return Nullable<V> (Ptr for references, std::optional for values). Compute callbacks return ComputeResult<V> and may take (key, old) or (old). Map views (keySet/values/entrySet) are snapshot views for plain shims and in-place View<E> for ConcurrentHashMap.
- For plain HashMap/TreeMap compute, the header says a same-key recursive update throws IllegalStateException("Recursive update"), like ConcurrentHashMap. Java's HashMap would throw ConcurrentModificationException. The collections agent decides and documents this.
- `Field<std::string>::get()` returns a `const std::string&` valid until the task ends, and Java null equals the empty string. `Final<T>` checks single assignment; the publication check needs the owner passed explicitly via `set(value, owner)`.
- `Ptr` is implicitly constructible from T& and `Ref` implicitly from Ptr (explicit from T&/T*), so design samples like `boss = getNpc(...)` and `traps.add(cast<Npc>(...))` compile. RefCounted subclasses with protected constructors need `AION_MAKE_REF_FRIEND`, because makeRef cannot otherwise reach protected constructors of final classes. makeRef uses the global operator new, so over-aligned types are rejected.
- `Future` adds `Schedule.logExceptions` (execute/schedule log exceptions, submit/deferred store them for get) and a backend interface (`create`, `runFromExecutor`, due time and sequence accessors). Periodic tasks keep running after an exception. This matches the Java server in practice, because RunnableWrapper never lets exceptions reach the executor.
- `ExecutorBackend` and `Clock` interfaces were defined (design names only installBackend, DeterministicExecutor and ManualClock). ThreadPoolManager gets a Config struct and a static `configure()`, because ThreadConfig is not ported yet.
- CronService API: jobs are PinnedCallback<void()>. findJobs/findNextFireTimes filter by std::type_info with an exact match only (no withSubTypes) and return vectors. Config fields of Java type CronExpression become `const CronExpression*` through a PropertyTransformer specialization.
- IDFactory API: `releaseId(id, className)` carries the class name for the C16 reuse warning. `lockIds(span)` is public so DAO seeding can happen later. Config, `getQuarantinedCount`, `drainQuarantine`, `recentlyReleased` and `resetForTests` were added.
- Monitor has no wait/notify: the Java game server uses no Object.wait/notify or Condition (checked with grep). Monitor::lock(const LockClass&) carries the dynamic lock class from monitorOf. Shims constructed without AION_LOCK_CLASS fall back to a class named after the shim.
- quiescentPoint() and QuiescentScope take a defaulted std::source_location so C16 can warn once per call site. The call syntax is unchanged.
- Check C2 ("assert + stack") is implemented as a fatal AION_CHECK (log with stacktrace, then abort). The abort dialog and Windows Error Reporting are disabled, so death tests and unattended runs do not hang.
- Build: besides the 7 requested build dirs, 6 matching ASan dirs were configured (build/gs-*-asan). Otherwise all agents would share build/msvc-asan and interfere.

### lifetime

- **Release protocol.** Releases with count > 1 also stamp `retireEpoch = max(retireEpoch, E)` before their CAS, instead of the design's stampless fast path (§2.2, §2.4). The literal protocol is unsafe under count ABA; `ProtocolMutationTest.DesignStamplessFastPathIsUnsafeUnderCountAba` shows the use-after-free. The stamp CAS runs at most once per object per epoch, and Ref copy + release measured 3 ns. Documented in RefCounted.h and reported in openIssues.
- **Lazy-publication re-check loop.** Kept as specified, but analysis and the directed test `PublicationRecheckIsNotASafetyStep` show it is not a safety step of this protocol. The scan compares stamps read after unlinks, and a publication missed by a scan implies a load after that scan's advance. No mutation test can fail for it; this is documented at `Mutation::PUBLISH_NO_RECHECK`.
- **Retire lists.** The design leaves them open. I chose a thread-local vector flushed as a batch to a lock-free incoming stack at outermost scope exit, at quiescentPoint, at 256 entries, and at thread exit. Retires outside any TaskScope, including thread_local destructors, are pushed immediately; retires in the destructor context are flushed at scan end. Unflushed entries are invisible to scans and to Stats::backlog.
- **Allocation header.** In checked builds makeRef allocates a 16-byte header {size, magic} in front of each object. The Reclaimer destroys with an explicit virtual destructor call and frees the allocation itself, which enables the C3 poisoning and delayed-free FIFO and exact backlogBytes. Release builds allocate sizeof(T) only and report backlogBytes per object as sizeof(RefCounted).
- **Scan serialization.** Scans, post-scan hooks and the destroy observer are serialized by a plain std::mutex, not a RECLAIMER RankedMutex. Hooks may take Monitors, which is illegal under a leaf mutex. Stats and config use plain leaf-style std::mutexes that never call out.
- **Lag.** Stats::lag is now minus the recorded time at which E became m, from a 1024-entry ring updated by scans. Epochs advanced by the test hook, or older than the ring, fall back to the oldest known start time. The watchdog lag warning is deduplicated per oldest epoch; the backlog dump is re-armed once the backlog drops below half the threshold.
- **PartMap.** Readers take the map's Monitor as well; it is a reentrant game-level lock, so callers may hold Monitors. This is the simplest option consistent with the header's 'writers are serialized by the Monitor'. `put(key, nullptr)` throws NullPointerException.
- **PartList.** Uses a fixed directory of doubling chunks that are never retired, so readers need no epoch retirement at all.
- **PartSlot OWNER mode.** Replaced parts go into a push-only lock-free stack instead of the stub's Monitor-guarded vector.
- **Removed C2 check.** I dropped the check that no QuiescentScope is open at outermost TaskScope exit. It would fire wrongly for a QuiescentScope opened outside any TaskScope.
- **Pct.h Options.** `maxSteps` now defaults to 0 (automatic) instead of 1000. `explore` calibrates k from the step counts of earlier schedules.
- **Pct.h script mode.** Options gained `keepTrace` and `script` (ScriptStep), so the safety sketch's interleavings can be replayed exactly. Both additions are source compatible.
- **Mutation switches.** `detail/Mutations.h` compiles runtime switches into checked (AION_PCT) builds. Each costs one relaxed atomic load at its switch point.

### sync

- Monitor fairness: the header said 'not fair'. I implemented eventual fairness (a starvation mode after 1 ms of waiting, like Go's sync.Mutex) so that a waiter cannot be starved by a barging thread. Barging is still allowed normally and for tryLock, so it stays close to non-fair Java semantics.
- Lock-order validator: the edge graph and reports are checked-build only as designed, but the held-lock stack in ThreadContext is maintained in release builds too. Design C14 keeps the watchdog in release builds, and the watchdog needs these records to resolve wait-graph edges. Cost: a few atomic stores per non-reentrant acquisition.
- Watchdog wait graph: the design records only the owner thread id copied at wait start. I resolve edges through the held-lock records instead, because the copied owner id goes stale (the lock may be released and re-acquired by someone else while the waiter sleeps). A cycle must also be observed in two consecutive checks before it is reported, to filter snapshots of different threads read at slightly different times.
- Watchdog dumps: portable text dumps have no stack traces for other threads (ThreadContext has no native thread handle). On Windows a minidump (MiniDumpWithThreadInfo, written in-process by the watchdog thread) carries every thread's stack. Other platforms write none; this is documented as a TODO.
- Watchdog::Config::stall changed from seconds to milliseconds, so tests can use short stalls.
- CYCLE reports are deduplicated per new edge rather than strictly per class pair. A new edge that closes a different cycle through already reported classes is reported again. Report occurrence counts are approximate because a thread-local edge cache skips repeated acquisitions.
- StampedLock does not implement writer preference, and conversions such as tryConvertToWriteLock are omitted (unused). This avoids a deadlock in EffectController's nested readLock calls, which Java's queue-based StampedLock can also hit.
- A fair Semaphore only prevents new blocking acquirers from overtaking waiters. Waiters are served in OS wake order rather than strict FIFO. The Java server uses only a non-fair Semaphore(1).
- The lock-order validator's graph mutex is a RankedMutex<STATS> as specified, but the watchdog's check serialization uses a plain std::mutex, because probes and listeners (which may take lower-rank leaf mutexes) run while it is held. Its list and config lock is a STATS leaf mutex that is never held while user code runs.
- LOGGING leaf rank: commons logging uses spdlog's own mutexes, so the rank cannot be enforced. As a rule, kernel code in sync logs only after releasing its leaf mutexes (documented in RankedMutex.h).

### collections

- **ConcurrentLinkedQueue / ConcurrentLinkedDeque are Monitor-guarded, not lock-free** (design §3.3 says lock-free). A Michael-Scott queue with epoch reclamation cannot support Java's interior remove(Object) (PlayerEnterWorldService.enteringWorld, TrapService) without leaking logically deleted nodes or double-retiring on unlink races. The 5 Java fields are low-rate. Java-visible semantics are unchanged; isEmpty/size stay lock-free. Documented in ConcurrentLinkedQueue.h.
- **Plain HashMap/TreeMap compute callbacks:** a same-key write throws IllegalStateException("Recursive update") like ConcurrentHashMap. Other keys may be modified. Java's HashMap would throw ConcurrentModificationException after any structural change. Documented in HashMap.h; needs a DEVIATIONS.md entry.
- **putIfAbsent/computeIfAbsent of a present key inside that key's own compute callback** returns the existing value instead of throwing. This matches Java's CHM fast path; the design sentence 'same-key recursion throws' is applied only to actual updates.
- **ConcurrentHashMap<K,V> has a third defaulted template parameter LockedReads.** It is derived from AION_CHM_LOCKED_READS so both variants coexist without ODR violations. Views became View<E, MapViewKind> with KeyView/ValueView/EntryView aliases, because Key and Value may be the same type.
- **ConcurrentHashMap::snapshot() is lock-free and weakly consistent.** The header said 'consistent-per-stripe'; taking stripe Monitors from readers would add lock edges Java does not have.
- **Resizes copy the stripe's nodes into a new table** and retire the old table as one node (no ForwardingNodes). They run under one process-wide CONTAINER_SLOT leaf mutex (Collections.cpp), not a per-map one, to keep map size small. Key/value copy constructors run under that leaf mutex.
- **Replacing a value links a new immutable node and retires the old one**, so a put or compute that replaces allocates. This keeps readers free of torn Ref reads.
- **The stripe array is allocated lazily on the first write**, so a never-written map costs about 40 B. Stripe Monitors are anonymous but always locked with the map's stripe LockClass.
- **Busy guard** (my choice, not in the design): modifying a collection from inside its own element equals/hashCode/compareTo or comparator throws IllegalStateException instead of corrupting the structure. Java throws CME or corrupts.
- **Plain map/set removeIf/replaceAll evaluate on a snapshot under the Monitor**; they remove only entries still mapped to the identical value. ArrayList removeIf is positional unless the predicate modified the list. CopyOnWriteArrayList removeIf overwrites re-entrant modifications, as Java does.
- **HashMap iteration order is insertion order** (a valid 'unspecified' order); HashMap and LinkedHashMap share the slot-vector store. LinkedList is contiguous, so first-element operations are O(n).
- **TreeMap/TreeSet/PriorityQueue without a comparator on types without natural ordering** throw ClassCastException at runtime (first put or get). JavaEquals.h described a compile error; that would break TreeMap<Ref<X>> with a comparator.
- **iterator()/keySet()/values()/entrySet() are const but write through** to the collection via const_cast, like Java views. Collection fields must therefore not be declared const.

### sched

- ExecutorBackend contract changed: submissions after shutdown are cancelled and dropped instead of throwing RejectedExecutionException. Java's AionRejectedExecutionHandler drops them silently. Throwing would break shutdown steps that still schedule, and silently dropping would leave get() blocked forever, so cancelling is the middle ground. The header comments are updated.
- Added ExecutorBackend::helpWhileWaiting, a virtual with a default, to implement helping get() (design §1.5) without making get() depend on concrete backends. Real worker pools do not help, so unrelated tasks never run nested inside a task.
- DeterministicExecutor helping get() advances the ManualClock to the next due time, or to the deadline, instead of blocking: a blocked thread lets simulated time pass. This makes FixPath's get(5, SECONDS) deterministic.
- Coalescing is checked when a periodic run finishes, using that run's start time. When the run started more than N periods late and at least coalesceMinimumLag late, the next run is set to now + period. A 30-period stall therefore gives exactly one catch-up run, then realigns. coalesceAfterPeriods <= 0 disables coalescing.
- Cancelled-shell purge (design: 'when shells exceed 50%') is checked only once the heap has at least 1024 entries, and again each time it doubles, which keeps the cost amortized O(1) per push.
- SerialExecutor sends each task to the pool on its own; the next one is handed over when the previous body returns (or throws). This replaces the 'turn of up to maxTasksPerTurn tasks' in the header. A turn would have run all its tasks inside one outer scope, contradicting the header's own rule that every task gets its own outermost TaskScope. maxTasksPerTurn is kept but ignored.
- The rejection policy's 'caller priority above normal' uses a thread-local Java priority recorded by kernel thread factories (instant pool threads with USE_PRIORITIES = 7, others 5), not the OS priority. This mirrors Java Thread.getPriority and is portable.
- RunnableStatsManager records are keyed by typeid(Future) with the call site as method name, because the type-erased body has no meaningful class. Exception and slow-task log lines append the call site to the Java text.
- getDelay() returns 0 for tasks that are not SCHEDULED. Java's FutureTask from submit() is not Delayed at all.
- Future get() waiters park on a striped std::mutex plus condition variable, not a RankedMutex. This keeps cancel() noexcept and legal even under leaf mutexes. The waiter is still visible to the watchdog through its BlockingRegion.
- ForkJoinPool: a PER_ELEMENT job on a stopped pool, or a nested call from a helper thread, runs serially on the caller in nested scopes (header-documented serial fallback). Helpers start lazily on the first parallel call.
- ThreadPoolManager::configure() is allowed while an explicitly installed backend is active; it throws only once the default pools were lazily created. This lets tests change the slow-task threshold and coalescing values on a DeterministicExecutor.

### services

- **CronService drivers.** The design names a single Cron thread. I added an EXECUTOR driver (a self-rearming task on the scheduled pool) so that `single_executor` really runs cron on the one executor thread (design §1.6) and a DeterministicExecutor's ManualClock fires jobs exactly. AUTO picks EXECUTOR only when `ThreadPoolManager::Config::singleExecutor` is set.
- **Cron misfires.** Missed fire times always collapse into one run, and the next fire time is computed after now (as the header already specified). Quartz only collapses when a trigger is more than 60 s late; triggers late by less catch up one by one.
- **Cron DST policy.** A nonexistent local time is skipped; an ambiguous local time fires once, at the earlier instant. So sub-hourly crons skip the repeated hour (for example 02:45 CEST is followed by 03:00 CET). This follows the header; it was not verified against Java's GregorianCalendar leniency, because no JDK is available.
- **CronExpression strictness and horizon.** Stricter than Quartz: more than 7 fields, trailing characters after a name, and step 0 are errors. Names with steps are accepted. The year field accepts 1970-2299, and searches stop after 2299 (Quartz: current year + 100). The header's `vector<bool>` year field became a bitset.
- **CronService API.** Beyond the header: a schedule overload with an explicit RunnableRunner (Java ShutdownHook.java:38), `findJobs<T>()`/`findNextFireTimes<T>()` templates, `getNextFireTime` (replaces Java getJobTriggers), `getJobCount`, `runDueJobs`, `resetForTests`, `getDriver`. `getInstance()` throws before init; Java returns null.
- **findNextFireTimes.** Returns one entry per distinct job (PinnedCallback identity, like the Java Map keyed by the Runnable) with the earliest fire time, sorted by time. The header described one entry per JobDetail. It matches the exact callable type only (no withSubTypes).
- **Scheduling a CronExpression reference.** The expression is re-interned through CronExpressions by its text, so JobDetail never points at a caller's temporary. Java kept the object reference.
- **CleanerQueue.** `CleanerAction` and `push` carry an optional class name, so auto-release ids feed IDFactory's C16 reuse warning. The drain is submitted with `ThreadPoolManager::submit`, so its outer TaskScope has kind INSTANT, with a nested CLEANER scope. A drain processes only the ids queued when it starts. Delivery order is push order per pushing thread; objects destroyed in one scan push in the Reclaimer's destruction order.
- **LeakCensus timing.** Event times are taken at push from the backend clock. `LeakCensus::Config` durations became milliseconds. I added `checkInterval`, `stalePinAfter` and `stalePinCheckInterval`, plus `getConfig`/`isInstalled`.
- **Stale periodic pins (C13, §5.4).** A task is warned about when any pinned owner has been removed from the world for more than 10 min, not only when all have. Future exposes `pins(owner)` but not its owner list.
- **Zombie breaker warnings.** Besides one warning per cut edge, it also warns once for objects that are not ZombieBreakable or whose breaker cut nothing. Both are missing breakers under D7.
- **IDFactory cursor.** When nothing is free in [cursor, wrapAt) the cursor wraps to the lowest free id, and ids >= wrapAt are used only if every id below wrapAt is taken. A double release while quarantined triggers the Java "wasn't taken" warning instead of queuing twice. `recentlyReleased` returns "unknown" for releases without a class name. Added `getConfig`, `getCursor`, `logUsedCount`, an `initializer_list` lockIds, and a className parameter on `releaseObjectIds`.
- **Time source.** IDFactory, CronService and LeakCensus read time through `ThreadPoolManager::getInstance().backend().clock()`, the only public accessor. It creates the default pools if no backend is installed. The sched-internal `sched_detail::schedulerClock()` avoids that but is not meant for other areas.

### stress

- **Stress target.** The design's §12.4 harness uses in-process LS+GS and FakeGameClients. P2 v0 is the kernel-level version from §19 P2: synthetic StressObject/StressPart/StressHub classes use every K4 member kind. No bots or DB stalls.
- **Pass criterion 'RSS back to baseline'.** It is checked on HeapSummary bytes in use, not RSS, because the Windows heap does not return committed pages. The slack is 64 MB, plus the 64 MB delayed-free FIFO in checked builds (C3). ASan builds report memory but do not fail on it, because the ASan allocator and quarantine bypass HeapAlloc.
- **Pass criterion 'borrow-free tasks never appear as the pinning scope'.** It is checked by a seqlock-bracketed read of the task's ThreadContext::publishedEpoch. Reclaimer::Stats::oldestPublishedTask is only a warning, because it misattributes (openIssues).
- **Fault site sort-comparator.** A throwing comparator in ArrayList::sort is off by default (AION_STRESS_KNOWN_ISSUES=1 enables it), because it reliably trips a known collections bug and would hide everything else in long runs. Every other fault site is always active.
- **LeakCensus thresholds.** censusAfter, zombieBreakAfter and stalePinAfter are raised beyond the run. Objects removed from the harness's world map are legitimately still referenced by slots and lists, and the census hooks would call ThreadPoolManager::getInstance() after the teardown. The census pass criterion is trackedCount() == 0 and no zombie cuts after the teardown.
- **Duration.** The design's P2 asks for 30 min per configuration. The task asked for at least 10 min each: done twice under ASan and twice in checked RelWithDebInfo (a first pass with an earlier harness version, then the final version).

### bench

- **World layout.** 80k npcs are spread over 16 maps of 2048 m instead of one world. The density equals one 8192 m map (1 npc per 840 m²). Java's per-instance scans (the player flag scan over worldMapNpcs) then see realistic instance sizes of about 5,000 npcs. Movers spawn within 250 m of 20 player clusters so their regions are active.
- **5 fake connections.** 200 player objects share the 5 connections round-robin. Without enough players, regions stay inactive and known lists stay empty. This sharing inflates CONNECTION_QUEUE contention; `--connections 200` shows the one-connection-per-player case.
- **Walkers randomized by default.** Speed ×0.7-1.3, route radius ×0.5-1.5 and the first route step are randomized. With identical walkers started together, all 2,000 arrive in the same tick every ~47 ticks, which dominates p99. That case stays available as `--synchronized-walkers` and is reported as the worst case.
- **SM_MOVE on every move step by default.** This is conservative: about 870 packets per tick against about 250 with the Java rule of 'mask or destination changed' (`--java-mask-rule`).
- **Extra churn beyond the brief.** Player relogs (2 per second, a new Player object) keep pair-add patterns 2 and 3 running. Npc churn replaces objects with new ids; half of it hits movers. A one-time-observer attach rate exercises the observer iterator's `remove()` path.
- **Mechanical mapping choices where the design is silent:**
  - Npc known list and move controller are `PartSlot` parts, set in the constructor (pattern 2).
  - ObserveController is a `final` K4 reference, so `const Ref<ObserveController>`.
  - KnownObject is RefCounted, because Java synchronizes on it.
  - `WorldMapInstance.regions` is a PartMap, pattern 1.
- **Not modelled:** region deactivation (players never leave their clusters). AI, zones, geo and controllers are reduced to counters. Player see/notSee send small SM_NPC_INFO/SM_DELETE-like packets. An IO drain task empties the connection queues every 50 ms.
- **FlagKnownList.update.** Its one-sided `removeIf` is ported as the two-sided removal (design DEVIATION 12).
- **Scheduling.** Ticks, churn, player and flag tasks run through `ThreadPoolManager::scheduleAtFixedRate` with captureless unpinned lambdas and a global model pointer. That is bench-driver code, not a game pattern.
- **Measurement machinery beyond the brief:**
  - Global `operator new` is replaced to count allocations and live bytes.
  - Kernel PCT yield-point hooks count refcount traffic and blocking waits per lock class, in checked builds only, over a separate window of ticks excluded from latency statistics.
  - An opt-in phase profiler.
  - The model is compiled twice so lock-free and stripe-locked reads coexist.
- **Pass criterion interpretation.** 'No KnownList ghosts' means no spawned object knows a despawned one, no despawned object has entries, and no objects leak after teardown. Asymmetric pairs and region ghosts are also reported; they were 0 in every run.
- **Extra build directory.** I configured `build/gs-bench-asan` (`-DAION_ASAN=ON`), in addition to the assigned `build/gs-bench`, to run the prototype under ASan. No other agent uses it.

## Open items after the fixers (to schedule)

### fixer core

- **Resolved in S0a:** Pin retains a pinned part through OwnedPart::retain/release (an owner and its part share a slot, two parts of one owner take two slots; `Pin::part(i)`); tests in `tests/runtime/sched/PinPartTest.cpp`. The outdated workaround comment in `runtime/lifetime/Parts.h` is still there. Was: [sched, Pin.h] PinTarget::classify(const OwnedPart*) retains only the part's owner. After the lifetime fix, Ref<Part> and Field<Ref<Part>> keep a part replaced in a PartSlot<RECLAIMER> or PartMap alive, but Pin(&part) and Pin(Ref<Part>) still do not: the replaced part can be freed while the pinned task runs (the review's `Pin(&storage)` scenario). Fix in sched: pin a part through OwnedPart::retain()/release(), which count the part and retain the owner. For example, store the OwnedPart* and release it through the part, while pins(owner) keeps reporting partOwner(). Until then OwnedPart's header documents: capture a Ref<Part> or pin the owner and re-read the part.
- [integrator, design doc runtime-architecture.md] Record these design corrections. (a) §2.3: a replaced OwnedPart keeps its own reference count and a stamp; retirePart destroys it only when partRefs == 0 and max(retire stamp, last-release stamp) < m. (b) §2.4: 'A Ptr can only exist after publication' now holds by construction: every Ptr made from a Ref, T& or T* publishes (TaskScope-internal borrowStamp), and T& from *ref is valid only while the Ref is held. (c) §2.2/§2.5: makeRef constructs with count 1 (the constructor's reference is adopted). This is in addition to the lifetime agent's earlier release-stamp correction. §2.5 cost table: creating a Ptr from a Ref now costs an out-of-line TLS check (publication once per scope). None of these is visible to Java code, so no DEVIATIONS.md entries are needed.
- [integrator, DEVIATIONS.md] Watchdog behaviour: STALL is measured since the task's last quiescentPoint and reported once per progress interval. SLOW_TASK fires once per run. Kinds long-running, main, startup, shutdown and fork-join get no SLOW_TASK warnings; kinds long-running, main and startup get no STALL dumps (both lists are configurable via Watchdog::Config slowTaskExemptKinds/stallExemptKinds, to be bound to config in the services wiring). The sync agent's earlier DEVIATIONS items still apply.
- [stress owner] StressReproducerTest.DISABLED_ReclaimerBacklogCountersSurviveConcurrentFlushAndScan can be enabled, or left opt-in since it runs about 7 s: the pushBatch fix makes it pass. The harness workaround at StressHarness.cpp:2230 (a reclaimNow barrier because removePostScanHook did not wait) is no longer needed.
- **Resolved (confirmed in S0a):** the hooks use `ThreadPoolManager::installedBackend()` and never create pools. Was: [services] LeakCensus/CleanerQueue hooks still call ThreadPoolManager::getInstance() and can create the default pools if a scan runs after teardown but before uninstall. removePostScanHook now waits for a running hook, so uninstall is a proper barrier, but hooks should still read the clock and backend without creating pools (stress open issue 3, services part).
- [services/stress] Reclaimer::drain() now returns false while a Ref still holds a retired part; such parts stay in Stats::backlog. Teardown code that expects drain() == true must release those Refs first.
- [base, perf] Ptr creation from a Ref now calls detail::borrowStamp() out of line (one TLS lookup plus two compares when already published). If P4 shows it in profiles, an inline thread_local fast path in base/ThreadContext would remove the call; base is frozen except for additive changes.
- [fields owner] SyncCostsBenchTest (tests/runtime/fields) still measures only Monitors with a static class. Add a SYNCHRONIZED(*object) case to confirm the lock-free LockClass::ofType path under P4.
- [sync, deferred] The in-process MiniDumpWriteDump can deadlock on the heap lock and races DbgHelp use by std::stacktrace; a helper-process dump writer is still TODO. Stacks of other threads in text dumps need an additive native-handle field in base ThreadContext. The LOGGING leaf rank cannot be enforced without a commons change.

Deferred or rejected findings:

- **rejected**: [collections open issue] suppress SAME_CLASS_NESTING warnings for stripe classes — The warning is the only lockdep signal for the realistic cross-stripe deadlock the collections agent found in the same map (PlayerContainer rename). It is a warning, not a test failure, so suppressing it would hide a known risk. Individual sites can use LockdepSuppression with a reason.
- **deferred**: [sync open issues] in-process minidump and DbgHelp, LOGGING leaf rank, other-thread stacks — These need a helper process, a commons change or an additive base change (native thread handle in ThreadContext). None of them can be done in the lifetime/sync sources alone; they stay listed in openIssues.

### fixer upper

- P2 gate is only half done: the 30-minute checked RelWithDebInfo run of the final harness passed, but the second required 30-minute run, under ASan Debug (build/gs-stress-asan, AION_STRESS_SECONDS=1800), has not been done. Final heap in the checked run was 74.7 MB against a 0.5 MB baseline, mostly the C3 64 MB delayed-free FIFO. The design/DEVIATIONS note that the P2 memory criterion is heap-in-use rather than RSS is still missing (integrator).
- Design §4.4 is wrong and needs a decision and a DEVIATIONS entry. A compute-family callback that writes another key of the SAME ConcurrentHashMap holds one stripe Monitor while taking a second. Two such callbacks whose stripes cross deadlock at about 1/256 per concurrent pair (PlayerContainer.updateCachedPlayerName, SpawnsData nested compute). Lockdep only warns SAME_CLASS_NESTING; the watchdog dumps the deadlock afterwards (D5). The shim cannot prevent it because the outer stripe is held by a running callback. Options: accept and document; give such maps more stripes via fieldmap.toml; port PlayerContainer (and any concurrent same-map nesting site) with an explicit map-level Monitor; optionally per-stripe-index lock classes for nested same-map acquisitions so tests report a CYCLE. The risk is documented in the ConcurrentHashMap.h header comment.
- Contract changes for DEVIATIONS/design (integrator): (1) ThreadPoolManager::installBackend now retires the previous backend (shutdown, cancel, join) BEFORE publishing the replacement, and keeps replaced backends alive until process exit instead of destroying them (a small bounded leak per test install); it must not be called from a thread of the installed backend. (2) New ExecutorBackend::retire() pure virtual; any future backend must implement it. (3) New kernel accessors that never create pools: ThreadPoolManager::installedBackend(), clock(), submitIfInstalled(), executeIfInstalled(). (4) A periodic Future whose pool reaches its due time while an explicit run() executes it is handed back to the installed backend with its due time unchanged. (5) In checked builds, CHM view and CopyOnWriteArrayList in-place iterators throw IllegalStateException (C1) when advanced or dereferenced in another scope id, e.g. after quiescentPoint(). (6) ArrayList::sort with a throwing comparator leaves the list unchanged (Java may leave it partially sorted). (7) PinnedCallback::target<T>() matches the exact type only (no withSubTypes). (8) LeakCensus stale-pin warnings now require ALL pinned owners to be removed, as the design says; the services agent's deviation note is obsolete.
- Collections memory for per-creature maps (bench): a written ConcurrentHashMap costs 16 stripe Monitors plus tables, about 1.8 KB fixed, now +24 B for the per-map resize mutex. Fewer or lazily allocated stripes for KnownList/AggroList/CreatureController.tasks need a fieldmap.toml or template-parameter decision.
- Checked-build cost: CHM and COW iterator dereference and advance now make one TaskScope::currentScopeId() call (TLS) per element. Re-measure the P4 checked tick if KnownList iteration is hot.
- StressReproducerTest.DISABLED_ReclaimerBacklogCountersSurviveConcurrentFlushAndScan passes now (the lifetime fixer fixed pushBatch) but stays opt-in: about 7 s and probabilistic. Run it in nightly jobs with --gtest_also_run_disabled_tests.
- Not in my areas, unchanged: ExecuteWrapper exception logs have no stack trace (commons Logger::errorCurrentException); dynamic_cast cost of as<>/cast<> (lifetime); DEVIATIONS.md entries listed by the services/sched/collections agents; GameServer startup/shutdown wiring (CronService init, IDFactory seeding, CleanerQueue/LeakCensus install, Watchdog::start, final CleanerQueue::drainNow after ThreadPoolManager::shutdown, ForkJoinPool::setSerial from serialMovement, PacketProcessor/cron on the single executor); gtest_discover_tests writes cmake_test_discovery_*.json into the source tree. **S0a:** the startup/shutdown wiring is `RuntimeLifecycle` (CronService init, IDFactory seeding through `Options::usedIds`, CleanerQueue/LeakCensus install, Reclaimer thread, Watchdog start, ForkJoin `setSerial`, pools, final `drainNow`; cron on the single executor through the EXECUTOR driver). Still for P5-14: the PacketProcessor on the single executor, the RESTART_SCHEDULE cron in the ShutdownHook constructor, `Logging::shutdown`/`quick_exit`, Ctrl+C posting. The discovery JSON is now written to `<build>/test_work/<target>` (resolved).

Deferred or rejected findings:

- **deferred**: Nested writes across two stripes of one map can deadlock; lockdep cannot see it; design §4.4 is wrong — The finding is correct: with 16 stripes, two compute callbacks that write each other's stripes of the same map deadlock at about 1/256 per concurrent pair. There is no mechanical fix inside the shim: the outer stripe is held by a running callback, so neither lock ordering nor try-and-release can be applied. The fix needs a design decision (accept, add stripes via fieldmap, or give PlayerContainer an explicit lock) plus a design/DEVIATIONS edit. I documented the risk, the affected Java sites and the required caller pattern in the ConcurrentHashMap header and reported the design correction in openIssues.
- **deferred**: [bench/collections open issue] CHM memory per per-creature map (16 stripes) — Lazy or fewer stripes would change the design's 16-stripe structure and need a fieldmap.toml or template-parameter decision, and it is a performance item, not a correctness bug. Reported in openIssues.
- **deferred**: [stress/sched/services open issues] DEVIATIONS.md entries, startup/shutdown wiring, single_executor for PacketProcessor/cron, serial_movement wiring, Watchdog start, final CleanerDrain — These are integrator or later-stage items outside the kernel code (a shared document and GameServer wiring). They are still valid and listed in openIssues. **S0a:** resolved by `RuntimeLifecycle` except the PacketProcessor on the single executor (P5-14).
- **deferred**: [stress open issue 6] ExecuteWrapper exceptions logged without stack trace — The formatting is done by commons Logger::errorCurrentException, and cpp/commons must not be modified.

### Spine step S0a (2026-09-14)

Details: [spine-status.md](spine-status.md) and runtime-architecture.md §23.

Resolved:
- `TaskKind::CALLBACK` clashed with the `<windows.h>` `CALLBACK` macro (found in wave 1): renamed to `TaskKind::CALLBACK_`;
  `tests/runtime/services/WindowsHeadersFirstTest.cpp` includes `<windows.h>` before every public kernel header. The skeleton tests'
  `KERNEL_FIRST` workaround is removed.
- Startup/shutdown wiring: `runtime/services/RuntimeLifecycle.h/.cpp` (10 tests; see "fixer upper" above for what stays with P5-14).
- Pin counts pinned parts (see "fixer core" above; `PinPartTest`, 3 tests).
- `DeterministicExecutor` seeds with `Rnd::seedCurrentThreadForTests` instead of assigning `Rnd::generator()`.
- `java.lang.ArithmeticException` is in commons `aion/commons/utils/Exception.h`, re-exported by `runtime/base/Exceptions.h`.
- `AION_UNPORTED` moved to `runtime/base/Unported.h/.cpp`. The first version changed the public namespace to `aion::gameserver::runtime`;
  the fixer added using-declarations of the wave-1 names in `aion::gameserver::handlers` (`UnportedTest.WaveOneHandlerNamesStayAvailable`),
  so the move is no longer an API break.
- `gs.smoke.startup` (starts `aion_game_server` with `RuntimeLifecycle`) is opt-in like the other DB tests: it runs only with
  `AION_TEST_GS_DATABASE_URL` (optional `AION_TEST_GS_DATABASE_USER`/`_PASSWORD`) and is reported as skipped otherwise.

Still open:
- Out of S0a scope, unchanged above: in-process minidump vs DbgHelp, other-thread stacks in text dumps, the LOGGING leaf rank, CHM same-map
  cross-stripe nesting (lint L20, per-stripe lock classes) and per-creature CHM memory, the StressHarness `reclaimNow` workaround with its
  DISABLED reproducer, the SyncCosts/TaskScope fast-path benches, Watchdog `slowTaskExemptKinds`/`stallExemptKinds` config keys
  (`RuntimeConfig::watchdogConfig` belongs to `aion_gs_configs`).
- `RuntimeLifecycle` ran only in Debug, not under msvc-asan or RelWithDebInfo.
- If `start()` fails while the caller holds a STARTUP `TaskScope`, the rollback's final drain cannot free objects that scope published
  (harmless: the process exits).
- `UnportedTest` hard-codes the source lines of its `AION_UNPORTED` sites (20/24); an include added above them must update the expectations.
- The Pin workaround comment in `runtime/lifetime/Parts.h` ("capture a Ref<Part> instead, or pin the owner and re-read the part") is outdated.
  **Resolved:** the comment now states that `Pin(&part)` retains the part and with it the owner.
- `MonitorTest.EventualFairnessAgainstABargingThread` failed once under `ctest --parallel 8` and passed alone: timing-sensitive under load.

### Spine steps S0b and S0c (2026-09-14)

Details: [spine-status.md](spine-status.md) and runtime-architecture.md §24.

Resolved:
- `Pin(PlayerCommonData*)` was ambiguous (C2668) because `PlayerCommonData` is both RefCounted and a static template (it derives
  `CreatureTemplate` through xmlgen). `IsStaticTemplate<T>` is now false for every class that derives `RefCounted`, and `PinTarget` classifies
  templates through one constrained overload, so such a pointer is no `TaskArg` and `Pin` retains the object. The S0b specialization in
  `PlayerCommonData.h` is removed; conventions-game-server.md records the rule.
- Commons `BaseClientPacket<TConnection>` stores the connection's `toString` function in `setConnection()`, so only that call needs the
  complete connection type; `AionClientPacket.h` no longer includes `AionConnection.h`, Asio or `<windows.h>` (one pointer per packet).
- `RefCounted.h`, `BaseClientPacketTest.cpp` and `SchedContractTest.cpp` were rewritten with CRLF by a lane and converted back to LF.

Still open:
- Per-creature `ConcurrentHashMap` memory (`CreatureGameStats.stats` and the other per-creature maps: 16 stripe Monitors each) stays a kernel
  design item (ledger S0B-064/S0B-139); the S0b headers keep Java's CHM layout as `fieldmap.json` prints it.
- `ItemService::DEFAULT_UPDATE_PREDICATE` and similar `static final` RefCounted constants are created at static initialization and never
  released; checked-build behaviour at static initialization was not exercised, and LeakCensus scenario tests that count by type must exempt
  them.

